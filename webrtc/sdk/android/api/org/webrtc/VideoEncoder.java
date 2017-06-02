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

public interface VideoEncoder {
  public class Settings {
    public final int numberOfCores;

    public Settings(int numberOfCores) {
      this.numberOfCores = numberOfCores;
    }
  }

  public class EncodeInfo {
    public final EncodedImage.FrameType[] frameTypes;

    public EncodeInfo(EncodedImage.FrameType[] frameTypes) {
      this.frameTypes = frameTypes;
    }
  }

  // TODO(sakal): Add values to these classes as necessary.
  public class CodecSpecificInfo {}

  public class CodecSpecificInfoVP8 extends CodecSpecificInfo {}

  public class CodecSpecificInfoVP9 extends CodecSpecificInfo {}

  public class CodecSpecificInfoH264 extends CodecSpecificInfo {}

  public class ScalingSettings {
    public final boolean on;
    public final int low;
    public final int high;

    ScalingSettings(boolean on, int low, int high) {
      this.on = on;
      this.low = low;
      this.high = high;
    }
  }

  public interface Callback {
    void onEncodedFrame(EncodedImage frame, CodecSpecificInfo info, RtpFragmentationHeader header);
  }

  void initEncode(Settings settings, Callback encodeCallback);
  void release();
  void encode(VideoFrame frame, EncodeInfo info);
  void setChannelParameters(int packetLoss, long roundTripTimeMs);
  void setRateAllocation(BitrateAllocation allocation, long framerate);
  ScalingSettings getScalingSettings();
  void setPeriodicKeyFrame(boolean enable);
  boolean getSupportsNativeHandle();
  String getImplementationName();
}
