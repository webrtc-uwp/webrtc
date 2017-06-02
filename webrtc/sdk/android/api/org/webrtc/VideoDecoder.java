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

public interface VideoDecoder {
  public class Settings {
    public final int numberOfCores;

    public Settings(int numberOfCores) {
      this.numberOfCores = numberOfCores;
    }
  }

  public class DecodeInfo {
    public final boolean missingFrames;
    public final long renderTimeMs;

    public DecodeInfo(boolean missingFrames, long renderTimeMs) {
      this.missingFrames = missingFrames;
      this.renderTimeMs = renderTimeMs;
    }
  }

  public interface Callback {
    void onDecodedFrame(VideoFrame frame, Integer decodeTimeMs, Integer qp);
  }

  void initDecode(Settings settings, Callback decodeCallback);
  void release();
  void decode(EncodedImage frame, DecodeInfo info);
  boolean getPrefersLateDecoding();
  String getImplementationName();
}
