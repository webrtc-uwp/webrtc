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

/** RateAdjuster that does not adjust bitrates. */
class NoOpRateAdjuster implements RateAdjuster {
  private int bitrate = 0;
  private int fps = 0;

  @Override
  public boolean reportEncodedFrame(int size) {
    return false;
  }

  @Override
  public void setRates(int targetBitrateBps, int targetFps) {
    bitrateBps = targetBitrateBps;
    fps = targetFps;
  }

  @Override
  public int getBitrateBps() {
    return bitrateBps;
  }

  @Override
  public int getFramerate() {
    return fps;
  }
}
