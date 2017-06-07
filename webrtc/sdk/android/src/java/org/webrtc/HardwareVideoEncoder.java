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
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
import android.os.Bundle;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Deque;
import java.util.Set;
import java.util.concurrent.LinkedBlockingDeque;

/** Android hardware video encoder. */
@TargetApi(19)
@SuppressWarnings("deprecation") // Cannot support API level 19 without using deprecated methods.
class HardwareVideoEncoder implements VideoEncoder {
  private static final String TAG = "HardwareVideoEncoder";

  // Bitrate modes - should be in sync with OMX_VIDEO_CONTROLRATETYPE defined
  // in OMX_Video.h
  private static final int VIDEO_ControlRateConstant = 2;
  // Key associated with the bitrate control mode value (above). Not present as a MediaFormat
  // constant until API level 21.
  private static final String KEY_BITRATE_MODE = "bitrate-mode";

  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;

  private static final int MAX_VIDEO_FRAMERATE = 30;

  // See MAX_ENCODER_Q_SIZE in androidmediaencoder_jni.cc.
  private static final int MAX_ENCODER_Q_SIZE = 2;

  private static final int MEDIA_CODEC_RELEASE_TIMEOUT_MS = 5000;

  // TODO(mellem):  Maybe move mime types to the factory or a common location.
  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  private static final String VP9_MIME_TYPE = "video/x-vnd.on2.vp9";
  private static final String H264_MIME_TYPE = "video/avc";
  private static final Set<String> SUPPORTED_MIME_TYPES =
      new HashSet<>(Arrays.asList(VP8_MIME_TYPE, VP9_MIME_TYPE, H264_MIME_TYPE));

  private final String codecName;
  private final String mimeType;
  private final int colorFormat;
  // Base interval for generating key frames.
  private final int keyFrameIntervalSec;
  // Interval at which to force a key frame. Used to reduce color distortions caused by some
  // Qualcomm video encoders.
  private final long forcedKeyFrameMs;
  // Presentation timestamp of the last requested (or forced) key frame.
  private long lastKeyFrameMs;

  private final BitrateAdjuster bitrateAdjuster;
  private int adjustedBitrate;

  // A queue of EncodedImage.Builders that correspond to frames in the codec.  These builders are
  // pre-populated with all the information that can't be sent through MediaCodec.
  private final Deque<EncodedImage.Builder> outputBuilders;

  // Thread that delivers encoded frames to the user callback.
  private Thread outputThread;

  // Whether the encoder is running.  Volatile so that the output thread can watch this value and
  // exit when the encoder stops.
  private volatile boolean running = false;
  // Any exception thrown during shutdown.  The output thread releases the MediaCodec and uses this
  // value to send exceptions thrown during release back to the encoder thread.
  private volatile Exception shutdownException = null;

  private MediaCodec codec;
  private Callback callback;

  private int width;
  private int height;

  // Contents of the last observed config frame output by the MediaCodec. Used by H.264.
  private ByteBuffer configBuffer = null;

  /**
   * Creates a new HardwareVideoEncoder with the given codecName, mimeType, colorFormat, key frame
   * intervals, and bitrateAdjuster.
   *
   * @param codecName the hardware codec implementation to use
   * @param mimeType MIME type of the codec's output; must be one of "video/x-vnd.on2.vp8",
   *     "video/x-vnd.on2.vp9", or "video/avc"
   * @param colorFormat color format used by the input buffer
   * @param keyFrameIntervalSec interval in seconds between key frames; used to initialize the codec
   * @param forceKeyFrameIntervalMs interval at which to force a key frame if one is not requested;
   *     used to reduce distortion caused by some codec implementations
   * @param bitrateAdjuster algorithm used to correct codec implementations that do not produce the
   *     desired bitrates
   */
  public HardwareVideoEncoder(String codecName, String mimeType, int colorFormat,
      int keyFrameIntervalSec, int forceKeyFrameIntervalMs, BitrateAdjuster bitrateAdjuster) {
    if (!SUPPORTED_MIME_TYPES.contains(mimeType)) {
      throw new IllegalArgumentException("Unsupported MIME type: " + mimeType);
    }
    if (!isColorFormatSupported(colorFormat)) {
      throw new IllegalArgumentException("Unsupported color format: " + colorFormat);
    }
    this.codecName = codecName;
    this.mimeType = mimeType;
    this.colorFormat = colorFormat;
    this.keyFrameIntervalSec = keyFrameIntervalSec;
    this.forcedKeyFrameMs = forceKeyFrameIntervalMs;
    this.bitrateAdjuster = bitrateAdjuster;
    this.outputBuilders = new LinkedBlockingDeque<>();
  }

  @Override
  public void initEncode(Settings settings, Callback callback) {
    Logging.d(TAG,
        "initEncode: " + settings.width + " x " + settings.height + ". @ " + settings.startBitrate
            + "kbps. Fps: " + settings.maxFramerate);
    this.width = settings.width;
    this.height = settings.height;
    bitrateAdjuster.setTargets(settings.startBitrate * 1000, settings.maxFramerate);
    adjustedBitrate = bitrateAdjuster.getAdjustedBitrateBps();

    this.callback = callback;

    lastKeyFrameMs = -1;

    codec = createCodecByName(codecName);
    if (codec == null) {
      Logging.e(TAG, "Cannot create media encoder");
      return;
    }
    try {
      MediaFormat format = MediaFormat.createVideoFormat(mimeType, width, height);
      format.setInteger(MediaFormat.KEY_BIT_RATE, adjustedBitrate);
      format.setInteger(KEY_BITRATE_MODE, VIDEO_ControlRateConstant);
      format.setInteger(MediaFormat.KEY_COLOR_FORMAT, colorFormat);
      format.setInteger(MediaFormat.KEY_FRAME_RATE, bitrateAdjuster.getAdjustedFramerate());
      format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, keyFrameIntervalSec);
      Logging.d(TAG, "  Format: " + format);
      codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
      codec.start();
    } catch (IllegalStateException e) {
      Logging.e(TAG, "initEncode failed", e);
      release();
      return;
    }

    running = true;
    outputThread = createOutputThread();
    outputThread.start();
  }

  @Override
  public void release() {
    try {
      // The outputThread actually stops and releases the codec once running is false.
      running = false;
      if (!ThreadUtils.joinUninterruptibly(outputThread, MEDIA_CODEC_RELEASE_TIMEOUT_MS)) {
        Logging.e(TAG, "Media encoder release timeout");
        throw new RuntimeException("Media encoder release timeout.");
      }
      if (shutdownException != null) {
        throw new RuntimeException(shutdownException);
      }
    } finally {
      codec = null;
      outputThread = null;
    }
  }

  @Override
  public void encode(VideoFrame videoFrame, EncodeInfo encodeInfo) {
    // TODO(mellem):  Reconfigure if the resolution changed.
    // No timeout.  Don't block for an input buffer, drop frames if the encoder falls behind.
    int index;
    try {
      index = codec.dequeueInputBuffer(0 /* timeout */);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "dequeueInputBuffer failed", e);
      return;
    }
    if (index == -1) {
      // Encoder is falling behind.  No input buffers available.  Drop the frame.
      return;
    }

    if (outputBuilders.size() > MAX_ENCODER_Q_SIZE) {
      // Too many frames in the encoder.  Drop this frame.
      return;
    }

    // TODO(mellem):  Add support for input surfaces and textures.
    ByteBuffer buffer = codec.getInputBuffers()[index];
    VideoFrame.I420Buffer i420 = videoFrame.getBuffer().toI420();
    toColorFormat(i420, colorFormat, buffer);

    boolean requestedKeyFrame = false;
    for (EncodedImage.FrameType frameType : encodeInfo.frameTypes) {
      if (frameType == EncodedImage.FrameType.VideoFrameKey) {
        requestedKeyFrame = true;
      }
    }

    long presentationTimestampUs = videoFrame.getTimestampNs() / 1000;
    if (requestedKeyFrame || shouldForceKeyFrame(presentationTimestampUs)) {
      requestKeyFrame(presentationTimestampUs);
    }

    // Number of bytes in the video buffer. Y channel is sampled at one byte per pixel; U and V are
    // subsampled at one byte per four pixels.
    int bufferSize = videoFrame.getBuffer().getHeight() * videoFrame.getBuffer().getWidth() * 3 / 2;
    long timestampMs = videoFrame.getTimestampNs() / 1_000_000;
    EncodedImage.Builder builder = EncodedImage.builder()
                                       .setTimeStampMs(timestampMs)
                                       .setCaptureTimeMs(timestampMs)
                                       .setCompleteFrame(true)
                                       .setEncodedWidth(videoFrame.getWidth())
                                       .setEncodedHeight(videoFrame.getHeight())
                                       .setRotation(videoFrame.getRotation());
    outputBuilders.offer(builder);
    try {
      codec.queueInputBuffer(
          index, 0 /* offset */, bufferSize, presentationTimestampUs, 0 /* flags */);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "queueInputBuffer failed", e);
      // Keep the output builders in sync with buffers in the codec.
      outputBuilders.pollLast();
    }
  }

  @Override
  public void setChannelParameters(short packetLoss, long roundTripTimeMs) {
    // No op.
  }

  @Override
  public void setRateAllocation(BitrateAllocation bitrateAllocation, int framerate) {
    if (framerate > MAX_VIDEO_FRAMERATE) {
      framerate = MAX_VIDEO_FRAMERATE;
    }
    bitrateAdjuster.setTargets(bitrateAllocation.getSum(), framerate);
    updateBitrate();
  }

  @Override
  public ScalingSettings getScalingSettings() {
    // TODO(mellem):  Implement scaling settings.
    return null;
  }

  @Override
  public String getImplementationName() {
    return "HardwareVideoEncoder: " + codecName;
  }

  private boolean shouldForceKeyFrame(long presentationTimestampUs) {
    return forcedKeyFrameMs > 0
        && presentationTimestampUs > (lastKeyFrameMs + forcedKeyFrameMs) * 1000;
  }

  private void requestKeyFrame(long presentationTimestampUs) {
    long presentationTimestampMs = (presentationTimestampUs + 500) / 1000;

    // Ideally MediaCodec would honor BUFFER_FLAG_SYNC_FRAME so we could
    // indicate this in queueInputBuffer() below and guarantee _this_ frame
    // be encoded as a key frame, but sadly that flag is ignored.  Instead,
    // we request a key frame "soon".
    try {
      Bundle b = new Bundle();
      b.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
      codec.setParameters(b);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "maybeForceKeyFrame failed", e);
      return;
    }
    lastKeyFrameMs = presentationTimestampMs;
  }

  private Thread createOutputThread() {
    return new Thread() {
      @Override
      public void run() {
        while (running) {
          deliverEncodedImage();
        }
        releaseCodecOnOutputThread();
      }
    };
  }

  private void deliverEncodedImage() {
    try {
      MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
      int index = codec.dequeueOutputBuffer(info, 0);
      if (index < 0) {
        return;
      }

      ByteBuffer codecOutputBuffer = codec.getOutputBuffers()[index];
      codecOutputBuffer.position(info.offset);
      codecOutputBuffer.limit(info.offset + info.size);

      if ((info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
        Logging.d(TAG, "Config frame generated. Offset: " + info.offset + ". Size: " + info.size);
        configBuffer = ByteBuffer.allocateDirect(info.size);
        configBuffer.put(codecOutputBuffer);
      } else {
        bitrateAdjuster.reportEncodedFrame(info.size);
        if (adjustedBitrate != bitrateAdjuster.getAdjustedBitrateBps()) {
          updateBitrate();
        }

        ByteBuffer frameBuffer;
        boolean isKeyFrame = (info.flags & MediaCodec.BUFFER_FLAG_SYNC_FRAME) != 0;
        if (isKeyFrame && mimeType.equals(H264_MIME_TYPE)) {
          Logging.d(TAG,
              "Prepending config frame of size " + configBuffer.capacity()
                  + " to output buffer with offset " + info.offset + ", size " + info.size);
          // For H.264 key frame prepend SPS and PPS NALs at the start.
          frameBuffer = ByteBuffer.allocateDirect(info.size + configBuffer.capacity());
          configBuffer.rewind();
          frameBuffer.put(configBuffer);
        } else {
          frameBuffer = ByteBuffer.allocateDirect(info.size);
        }
        frameBuffer.put(codecOutputBuffer);
        frameBuffer.rewind();

        EncodedImage.FrameType frameType = EncodedImage.FrameType.VideoFrameDelta;
        if (isKeyFrame) {
          Logging.d(TAG, "Sync frame generated");
          frameType = EncodedImage.FrameType.VideoFrameKey;
        }
        EncodedImage.Builder builder = outputBuilders.poll();
        builder.setBuffer(frameBuffer).setFrameType(frameType);
        // TODO(mellem):  Set codec-specific info.
        callback.onEncodedFrame(builder.createEncodedImage(), new CodecSpecificInfo());
      }
      codec.releaseOutputBuffer(index, false);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "deliverOutput failed", e);
    }
  }

  private void releaseCodecOnOutputThread() {
    Logging.d(TAG, "Releasing MediaCodec on output thread");
    try {
      codec.stop();
    } catch (Exception e) {
      Logging.e(TAG, "Media encoder stop failed", e);
    }
    try {
      codec.release();
    } catch (Exception e) {
      Logging.e(TAG, "Media encoder release failed", e);
      // Propagate exceptions caught during release back to the main thread.
      shutdownException = e;
    }
    Logging.d(TAG, "Output thread done");
  }

  private void updateBitrate() {
    adjustedBitrate = bitrateAdjuster.getAdjustedBitrateBps();
    try {
      Bundle params = new Bundle();
      params.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, adjustedBitrate);
      codec.setParameters(params);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "updateBitrate failed", e);
    }
  }

  private boolean isColorFormatSupported(int colorFormat) {
    return toColorFormat(null, colorFormat, null);
  }

  private boolean toColorFormat(VideoFrame.I420Buffer i420, int colorFormat, ByteBuffer out) {
    switch (colorFormat) {
      case MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar:
        if (out != null && i420 != null) {
          fillBufferNv12(out, i420.getDataY(), i420.getDataU(), i420.getDataV());
        }
        return true;
      case MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar:
      case MediaCodecInfo.CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar:
      case COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m:
        if (out != null && i420 != null) {
          fillBufferI420(out, i420.getDataY(), i420.getDataU(), i420.getDataV());
        }
        return true;
      default:
        return false;
    }
  }

  private void fillBufferI420(ByteBuffer buffer, ByteBuffer y, ByteBuffer u, ByteBuffer v) {
    buffer.put(y);
    buffer.put(u);
    buffer.put(v);
  }

  private void fillBufferNv12(ByteBuffer buffer, ByteBuffer y, ByteBuffer u, ByteBuffer v) {
    buffer.put(y);

    // Interleave the bytes from the U and V portions, starting with U.
    int i = 0;
    while (u.hasRemaining() && v.hasRemaining()) {
      buffer.put(u.get());
      buffer.put(v.get());
    }
  }

  private static MediaCodec createCodecByName(String codecName) {
    try {
      return MediaCodec.createByCodecName(codecName);
    } catch (IOException | IllegalArgumentException e) {
      Logging.e(TAG, "createCodecByName failed", e);
      return null;
    }
  }
}
