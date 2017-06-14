/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.annotation.TargetApi;
import android.graphics.Matrix;
import android.media.MediaCodec;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaFormat;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Queue;
import java.util.Set;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** Android hardware video decoder. */
@TargetApi(16)
@SuppressWarnings("deprecation") // Cannot support API 16 without using deprecated methods.
class HardwareVideoDecoder implements VideoDecoder {
  private static final String TAG = "HardwareVideoDecoder";

  // TODO(magjed): Use MediaFormat constants when part of the public API.
  private static final String FORMAT_KEY_STRIDE = "stride";
  private static final String FORMAT_KEY_SLICE_HEIGHT = "slice-height";
  private static final String FORMAT_KEY_CROP_LEFT = "crop-left";
  private static final String FORMAT_KEY_CROP_RIGHT = "crop-right";
  private static final String FORMAT_KEY_CROP_TOP = "crop-top";
  private static final String FORMAT_KEY_CROP_BOTTOM = "crop-bottom";

  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int COLOR_QCOM_FORMATYVU420PackedSemiPlanar32m4ka = 0x7FA30C01;
  private static final int COLOR_QCOM_FORMATYVU420PackedSemiPlanar16m4ka = 0x7FA30C02;
  private static final int COLOR_QCOM_FORMATYVU420PackedSemiPlanar64x32Tile2m8ka = 0x7FA30C03;
  private static final int COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;
  // Allowable color formats supported by codec - in order of preference.
  private static final Set<Integer> SUPPORTED_COLOR_FORMATS = new HashSet<>(Arrays.asList(
      CodecCapabilities.COLOR_FormatYUV420Planar, CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
      CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
      COLOR_QCOM_FORMATYVU420PackedSemiPlanar32m4ka, COLOR_QCOM_FORMATYVU420PackedSemiPlanar16m4ka,
      COLOR_QCOM_FORMATYVU420PackedSemiPlanar64x32Tile2m8ka,
      COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m));

  private static final int MEDIA_CODEC_RELEASE_TIMEOUT_MS = 5000;

  private static final int DEQUEUE_OUTPUT_BUFFER_TIMEOUT_US = 100000;

  private final String codecName;
  private final VideoCodecType codecType;
  private int colorFormat;
  private Thread outputThread;

  private volatile boolean running = false;
  private volatile Exception shutdownException = null;

  private int width;
  private int height;
  private int stride;
  private int sliceHeight;
  private boolean hasDecodedFirstFrame;
  private boolean keyFrameRequired;

  private Callback callback;

  private MediaCodec codec = null;

  HardwareVideoDecoder(String codecName, VideoCodecType codecType, int colorFormat) {
    if (!SUPPORTED_COLOR_FORMATS.contains(colorFormat)) {
      throw new IllegalArgumentException("Unsupported color format: " + colorFormat);
    }
    this.codecName = codecName;
    this.codecType = codecType;
    this.colorFormat = colorFormat;
  }

  @Override
  public VideoCodecStatus initDecode(Settings settings, Callback callback) {
    return initDecodeInternal(settings.width, settings.height, callback);
  }

  private VideoCodecStatus initDecodeInternal(int width, int height, Callback callback) {
    this.width = width;
    this.height = height;
    stride = width;
    sliceHeight = height;
    this.callback = callback;

    codec = createCodecByName(codecName);
    if (codec == null) {
      Logging.e(TAG, "Cannot create media decoder");
      return VideoCodecStatus.ERROR;
    }
    try {
      MediaFormat format = MediaFormat.createVideoFormat(codecType.mimeType(), width, height);
      format.setInteger(MediaFormat.KEY_COLOR_FORMAT, colorFormat);
      codec.configure(format, null, null, 0);
      codec.start();
    } catch (IllegalStateException e) {
      Logging.e(TAG, "initDecode failed", e);
      release();
      return VideoCodecStatus.ERROR;
    }

    hasDecodedFirstFrame = false;
    keyFrameRequired = true;
    running = true;
    outputThread = createOutputThread();
    outputThread.start();

    return VideoCodecStatus.OK;
  }

  @Override
  public VideoCodecStatus decode(EncodedImage frame, DecodeInfo info) {
    if (codec == null || callback == null) {
      return VideoCodecStatus.UNINITIALIZED;
    }

    if (frame.buffer == null) {
      Logging.e(TAG, "decode() - no input data");
      return VideoCodecStatus.ERR_PARAMETER;
    }

    int size = frame.buffer.remaining();
    if (size == 0) {
      Logging.e(TAG, "decode() - no input data");
      return VideoCodecStatus.ERROR;
    }

    // Check if the resolution changed and reset the codec if necessary.
    if (frame.encodedWidth * frame.encodedHeight > 0
        && (frame.encodedWidth != width || frame.encodedHeight != height)) {
      VideoCodecStatus status = resetCodec(frame.encodedWidth, frame.encodedHeight);
      if (status != VideoCodecStatus.OK) {
        return VideoCodecStatus.FALLBACK_SOFTWARE;
      }
    }

    if (!keyFrameRequired) {
      // Need to process a key frame first.
      if (frame.frameType != EncodedImage.FrameType.VideoFrameKey) {
        Logging.e(TAG, "decode() - key frame required first");
        return VideoCodecStatus.ERROR;
      }
      if (!frame.completeFrame) {
        Logging.e(TAG, "decode() - complete frame required first");
        return VideoCodecStatus.ERROR;
      }
    }

    // TODO(mellem):  Support textures.
    int index;
    try {
      index = codec.dequeueInputBuffer(0 /* timeout */);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "dequeueInputBuffer failed", e);
      return VideoCodecStatus.FALLBACK_SOFTWARE;
    }
    if (index < 0) {
      // Decoder is falling behind.  No input buffers available.
      // The decoder can't simply drop frames, it might lose a key frame.
      Logging.e(TAG, "decode() - no HW buffers available; decoder falling behind");
      return VideoCodecStatus.FALLBACK_SOFTWARE;
    }

    ByteBuffer buffer;
    try {
      buffer = codec.getInputBuffers()[index];
    } catch (IllegalStateException e) {
      Logging.e(TAG, "getInputBuffers failed", e);
      return VideoCodecStatus.FALLBACK_SOFTWARE;
    }

    if (buffer.capacity() < size) {
      Logging.e(TAG, "decode() - HW buffer too small");
      return VideoCodecStatus.FALLBACK_SOFTWARE;
    }
    buffer.put(frame.buffer);

    try {
      codec.queueInputBuffer(
          index, 0 /* offset */, size, frame.captureTimeMs * 1000, 0 /* flags */);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "queueInputBuffer failed", e);
      return VideoCodecStatus.FALLBACK_SOFTWARE;
    }
    if (keyFrameRequired) {
      keyFrameRequired = false;
    }
    return VideoCodecStatus.OK;
  }

  @Override
  public boolean getPrefersLateDecoding() {
    return true;
  }

  @Override
  public String getImplementationName() {
    return "HardwareVideoDecoder: " + codecName;
  }

  @Override
  public VideoCodecStatus release() {
    try {
      // The outputThread actually stops and releases the codec once running is false.
      running = false;
      if (!ThreadUtils.joinUninterruptibly(outputThread, MEDIA_CODEC_RELEASE_TIMEOUT_MS)) {
        // Log an exception to capture the stack trace and turn it into a TIMEOUT error.
        Logging.e(TAG, "Media encoder release timeout", new RuntimeException());
        return VideoCodecStatus.TIMEOUT;
      }
      if (shutdownException != null) {
        // Log the exception and turn it into an error.  Wrap the exception in a new exception to
        // capture both the output thread's stack trace and this thread's stack trace.
        Logging.e(TAG, "Media encoder release error", new RuntimeException(shutdownException));
        return VideoCodecStatus.ERROR;
      }
    } finally {
      codec = null;
      outputThread = null;
      callback = null;
    }
    return VideoCodecStatus.OK;
  }

  private VideoCodecStatus resetCodec(int newWidth, int newHeight) {
    VideoCodecStatus status = release();
    if (status != VideoCodecStatus.OK) {
      return status;
    }
    return initDecodeInternal(newWidth, newHeight, callback);
  }

  private Thread createOutputThread() {
    return new Thread() {
      @Override
      public void run() {
        while (running) {
          deliverDecodedFrame();
        }
        releaseCodecOnOutputThread();
      }
    };
  }

  private void deliverDecodedFrame() {
    try {
      MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
      int result = codec.dequeueOutputBuffer(info, DEQUEUE_OUTPUT_BUFFER_TIMEOUT_US);
      if (result == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
        reformat(codec.getOutputFormat());
        return;
      }

      if (result < 0) {
        return;
      }

      hasDecodedFirstFrame = true;

      if (info.size < width * height * 3 / 2) {
        Logging.e(TAG, "Insufficient output buffer size: " + info.size);
        return;
      }

      if (info.size < stride * height * 3 / 2 && sliceHeight == height && stride > width) {
        // Some codecs (Exynos) report an incorrect stride.  Correct it here.
        stride = info.size * 2 / (height * 3);
      }

      ByteBuffer buffer = codec.getOutputBuffers()[result];
      buffer.position(info.offset);
      buffer.limit(info.size);

      VideoFrame.I420Buffer frameBuffer = new I420BufferImpl(width, height);

      if (colorFormat == CodecCapabilities.COLOR_FormatYUV420Planar) {
        int uvStride = stride / 2;
        int chromaWidth = (width + 1) / 2;
        // Note that hardware truncates instead of rounding.  WebRTC expects rounding, so the last
        // row will be duplicated if the sliceHeight is odd.
        int chromaHeight = (sliceHeight % 2 == 0) ? (height + 1) / 2 : height / 2;

        int yPos = info.offset;
        int uPos = yPos + stride * sliceHeight;
        int vPos = uPos + uvStride * sliceHeight / 2;

        copyPlane(buffer, yPos, stride, frameBuffer.getDataY(), 0, frameBuffer.getStrideY(), width,
            height);
        copyPlane(buffer, uPos, uvStride, frameBuffer.getDataU(), 0, frameBuffer.getStrideU(),
            chromaWidth, chromaHeight);
        copyPlane(buffer, vPos, uvStride, frameBuffer.getDataV(), 0, frameBuffer.getStrideV(),
            chromaWidth, chromaHeight);

        // If the sliceHeight is odd, duplicate the last rows of chroma.
        if (sliceHeight % 2 != 0) {
          int strideU = frameBuffer.getStrideU();
          uPos = chromaHeight * strideU;
          copyRow(
              frameBuffer.getDataU(), uPos - strideU, frameBuffer.getDataU(), uPos, chromaWidth);
          int strideV = frameBuffer.getStrideV();
          vPos = chromaHeight * strideV;
          copyRow(
              frameBuffer.getDataV(), vPos - strideV, frameBuffer.getDataV(), vPos, chromaWidth);
        }
      } else {
        // All other supported color formats are NV12.
        int yPos = info.offset;
        int uvPos = yPos + stride * sliceHeight;
        int chromaHeight = (height + 1) / 2;

        copyPlane(buffer, yPos, stride, frameBuffer.getDataY(), 0, frameBuffer.getStrideY(), width,
            height);

        // Split U and V rows.
        int dstUPos = 0;
        int dstVPos = 0;
        for (int i = 0; i < chromaHeight; ++i) {
          for (int j = 0; j < width - 1; j += 2) {
            frameBuffer.getDataU().put(dstUPos + j, buffer.get(uvPos + j));
            frameBuffer.getDataV().put(dstVPos + j, buffer.get(uvPos + j + 1));
          }
          dstUPos += frameBuffer.getStrideU();
          dstVPos += frameBuffer.getStrideV();
          uvPos += stride;
        }
      }

      long presentationTimeNs = info.presentationTimeUs * 1000;
      VideoFrame frame =
          new VideoFrame(frameBuffer, 0 /* rotation */, presentationTimeNs, new Matrix());

      // TODO(mellem):  Add decodeTimeMs and qp.
      callback.onDecodedFrame(frame, null, null);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "deliverDecodedFrame failed", e);
    }
  }

  private void reformat(MediaFormat format) {
    Logging.d(TAG, "Decoder format changed: " + format.toString());
    final int newWidth;
    final int newHeight;
    if (format.containsKey(FORMAT_KEY_CROP_LEFT) && format.containsKey(FORMAT_KEY_CROP_RIGHT)
        && format.containsKey(FORMAT_KEY_CROP_BOTTOM) && format.containsKey(FORMAT_KEY_CROP_TOP)) {
      newWidth =
          1 + format.getInteger(FORMAT_KEY_CROP_RIGHT) - format.getInteger(FORMAT_KEY_CROP_LEFT);
      newHeight =
          1 + format.getInteger(FORMAT_KEY_CROP_BOTTOM) - format.getInteger(FORMAT_KEY_CROP_TOP);
    } else {
      newWidth = format.getInteger(MediaFormat.KEY_WIDTH);
      newHeight = format.getInteger(MediaFormat.KEY_HEIGHT);
    }
    if (hasDecodedFirstFrame && (width != newWidth || height != newHeight)) {
      running = false;
      shutdownException = new RuntimeException("Unexpected size change. Configured " + width + "*"
          + height + ". New " + newWidth + "*" + newHeight);
      return;
    }
    width = newWidth;
    height = newHeight;

    if (format.containsKey(MediaFormat.KEY_COLOR_FORMAT)) {
      colorFormat = format.getInteger(MediaFormat.KEY_COLOR_FORMAT);
      Logging.d(TAG, "Color: 0x" + Integer.toHexString(colorFormat));
      if (!SUPPORTED_COLOR_FORMATS.contains(colorFormat)) {
        running = false;
        shutdownException = new IllegalStateException("Unsupported color format: " + colorFormat);
        return;
      }
    }

    if (format.containsKey(FORMAT_KEY_STRIDE)) {
      stride = format.getInteger(FORMAT_KEY_STRIDE);
    }
    if (format.containsKey(FORMAT_KEY_SLICE_HEIGHT)) {
      sliceHeight = format.getInteger(FORMAT_KEY_SLICE_HEIGHT);
    }
    Logging.d(TAG, "Frame stride and slice height: " + stride + " x " + sliceHeight);
    stride = Math.max(width, stride);
    sliceHeight = Math.max(height, sliceHeight);
  }

  private void releaseCodecOnOutputThread() {
    Logging.d(TAG, "Releasing MediaCodec on output thread");
    try {
      codec.stop();
    } catch (Exception e) {
      Logging.e(TAG, "Media decoder stop failed", e);
      // Propagate exceptions caught during release back to the main thread.
      shutdownException = e;
    }
    try {
      codec.release();
    } catch (Exception e) {
      Logging.e(TAG, "Media decoder release failed", e);
      // Propagate exceptions caught during release back to the main thread.
      shutdownException = e;
    }
    Logging.d(TAG, "Release on output thread done");
  }

  private void copyPlane(ByteBuffer src, int srcPos, int srcStride, ByteBuffer dst, int dstPos,
      int dstStride, int width, int height) {
    for (int i = 0; i < height; ++i) {
      copyRow(src, srcPos, dst, dstPos, width);
      srcPos += srcStride;
      dstPos += dstStride;
    }
  }

  private void copyRow(ByteBuffer src, int srcPos, ByteBuffer dst, int dstPos, int width) {
    for (int i = 0; i < width; ++i) {
      dst.put(dstPos + i, src.get(srcPos + i));
    }
  }

  private static MediaCodec createCodecByName(String name) {
    try {
      return MediaCodec.createByCodecName(name);
    } catch (IOException | IllegalArgumentException e) {
      Logging.e(TAG, "createCodecByName failed", e);
      return null;
    }
  }
}
