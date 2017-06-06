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
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.Queue;

/** Android hardware video encoder. */
@TargetApi(19)
@SuppressWarnings("deprecation") // Cannot support API 19 without using deprecated methods.
class HardwareVideoEncoder implements VideoEncoder {
  private static final String TAG = "HardwareVideoEncoder";

  // Bitrate modes - should be in sync with OMX_VIDEO_CONTROLRATETYPE defined
  // in OMX_Video.h
  private static final int VIDEO_ControlRateConstant = 2;
  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;

  private static final int MAX_VIDEO_FRAMERATE = 30;

  // See MAX_ENCODER_Q_SIZE in androidmediaencoder_jni.cc.
  private static final int MAX_INPUT_QUEUE_SIZE = 2;

  private final MediaCodec codec;
  private final int colorFormat;
  private final RateAdjuster rateAdjuster;
  private final Thread encoderThread;
  private final Queue<VideoFrame> inputFrames;

  private volatile boolean running = false;

  private Callback callback;

  // Interval at which to force a key frame, and how long it's been since the last forced key frame.
  private long forcedKeyFrameMs;
  private long lastKeyFrameMs;

  private int width;
  private int height;

  private int outputRotation;

  public HardwareVideoEncoder(String name, int colorFormat, RateAdjuster rateAdjuster) {
    codec = createCodecByName(name);
    this.colorFormat = colorFormat;
    this.rateAdjuster = rateAdjuster;
    encoderThread = new Thread() {
      @Override
      public void run() {
        pumpVideoCodec();
      }
    };
    this.inputFrames = new ConcurrentLinkedQueue<>();
  }

  @Override
  public void initEncode(Settings settings, Callback callback) {
    width = settings.width;
    height = settings.height;
    rateAdjuster.setRates(settings.startBitrate * 1000, settings.maxFramerate);

    this.callback = callback;

    lastKeyFrameMs = -1;

    try {
      // TODO(mellem):  Pick mime type based on codec type.
      MediaFormat format = MediaFormat.createVideoFormat("" /* mime */, width, height);
      format.setInteger(MediaFormat.KEY_BIT_RATE, rateAdjuster.getBitrateBps());
      format.setInteger("bitrate-mode", VIDEO_ControlRateConstant);
      format.setInteger(MediaFormat.KEY_COLOR_FORMAT, colorFormat);
      format.setInteger(MediaFormat.KEY_FRAME_RATE, rateAdjuster.getFramerate());
      // TODO(mellem):  Pick key frame interval based on codec type.
      format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 100 /* keyFrameIntervalSec */);
      codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
      codec.start();
    } catch (IllegalStateException e) {
      release();
    }

    running = true;
    encoderThread.start();
  }

  @Override
  public void release() {
    running = false;
    ThreadUtils.joinUninterruptibly(encoderThread);
  }

  @Override
  public void encode(VideoFrame videoFrame, EncodeInfo encodeInfo) {
    // No timeout.  Don't block for an input buffer, drop frames if the encoder falls behind.
    int index = codec.dequeueInputBuffer(0 /* timeout */);
    if (index == -1) {
      // Encoder is falling behind.  No input buffers available.  Drop the frame.
      return;
    }

    if (inputFrames.size() > MAX_INPUT_QUEUE_SIZE) {
      // Too many frames in the encoder.  Drop this frame.
      return;
    }

    VideoFrame.I420Buffer i420 = videoFrame.getBuffer().toI420();
    // Number of bytes in the video buffer. Y channel is sampled at one byte per pixel; U and V are
    // subsampled at one byte per four pixels.
    int bufferSize = videoFrame.getBuffer().getHeight() * videoFrame.getBuffer().getWidth() * 3 / 2;

    ByteBuffer buffer = codec.getInputBuffers()[index];
    switch (colorFormat) {
      case MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar:
        fillBufferNv12(buffer, i420.getDataY(), i420.getDataU(), i420.getDataV());
        break;
      case MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar:
      case MediaCodecInfo.CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar:
      case COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m:
        fillBufferI420(buffer, i420.getDataY(), i420.getDataU(), i420.getDataV());
        break;
      default:
        // Can't encode other color formats.  Fall back to software encoding.
        break;
    }

    boolean requestedKeyFrame = false;
    for (EncodedImage.FrameType frameType : encodeInfo.frameTypes) {
      if (frameType == EncodedImage.FrameType.VideoFrameKey) {
        requestedKeyFrame = true;
      }
    }

    long presentationTimestampUs = videoFrame.getTimestampNs() / 1000;
    setKeyFrame(requestedKeyFrame, presentationTimestampUs);

    inputFrames.offer(videoFrame);
    codec.queueInputBuffer(index /* inputBuffer */, 0, bufferSize, presentationTimestampUs, 0);
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
    rateAdjuster.setRates(bitrateAllocation.getSum(), framerate);
    updateBitrate();
  }

  @Override
  public ScalingSettings getScalingSettings() {
    // TODO(mellem):  Implement this.
    return null;
  }

  @Override
  public String getImplementationName() {
    return codec.getName();
  }

  private void setKeyFrame(boolean requestedKeyFrame, long presentationTimestampUs) {
    long presentationTimestampMs = (presentationTimestampUs + 500) / 1000;
    if (lastKeyFrameMs < 0) {
      lastKeyFrameMs = presentationTimestampMs;
    }
    boolean forcedKeyFrame = false;
    if (!requestedKeyFrame && forcedKeyFrameMs > 0
        && presentationTimestampMs > lastKeyFrameMs + forcedKeyFrameMs) {
      forcedKeyFrame = true;
    }
    if (requestedKeyFrame || forcedKeyFrame) {
      // Ideally MediaCodec would honor BUFFER_FLAG_SYNC_FRAME so we could
      // indicate this in queueInputBuffer() below and guarantee _this_ frame
      // be encoded as a key frame, but sadly that flag is ignored.  Instead,
      // we request a key frame "soon".
      Bundle b = new Bundle();
      b.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
      codec.setParameters(b);
      lastKeyFrameMs = presentationTimestampMs;
    }
  }

  private void pumpVideoCodec() {
    while (running) {
      MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
      int index = codec.dequeueOutputBuffer(info, 0);
      if (index >= 0) {
        // TODO(mellem):  Handle H264 config and key frames.
        ByteBuffer buffer = codec.getOutputBuffers()[index];
        buffer.position(info.offset);
        buffer.limit(info.offset + info.size);

        if (rateAdjuster.reportEncodedFrame(info.size)) {
          updateBitrate();
        }

        ByteBuffer frameBuffer = ByteBuffer.allocate(info.size);
        frameBuffer.put(buffer);

        VideoFrame inputFrame = inputFrames.poll();
        if (inputFrame != null) {
          outputRotation = inputFrame.getRotation();
        }

        EncodedImage.FrameType frameType = EncodedImage.FrameType.VideoFrameDelta;
        if ((info.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0) {
          frameType = EncodedImage.FrameType.VideoFrameKey;
        }
        long presentationTimeMs = info.presentationTimeUs / 1000;
        EncodedImage image = EncodedImage.builder()
                                 .setBuffer(frameBuffer)
                                 .setFrameType(frameType)
                                 .setTimeStampMs(presentationTimeMs)
                                 .setCaptureTimeMs(presentationTimeMs)
                                 .setCompleteFrame(true)
                                 .setEncodedWidth(width)
                                 .setEncodedHeight(height)
                                 .setRotation(outputRotation)
                                 .createEncodedImage();
        // TODO(mellem):  Is any codec-specific info necessary?
        callback.onEncodedFrame(image, new CodecSpecificInfo());

        codec.releaseOutputBuffer(index, false);
      }
    }

    // TODO(mellem):  Punt this to another thread?  Signal and time out?
    codec.stop();
    codec.release();
  }

  private void updateBitrate() {
    try {
      Bundle params = new Bundle();
      params.putInt(MediaCodec.PARAMETER_KEY_VIDEO_BITRATE, rateAdjuster.getBitrateBps());
      codec.setParameters(params);
    } catch (IllegalStateException e) {
      Logging.e(TAG, "updateBitrate failed", e);
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

  private static MediaCodec createCodecByName(String name) {
    try {
      return MediaCodec.createByCodecName(name);
    } catch (IOException | IllegalArgumentException e) {
      Logging.e(TAG, "createCodecByName failed", e);
      return null;
    }
  }
}
