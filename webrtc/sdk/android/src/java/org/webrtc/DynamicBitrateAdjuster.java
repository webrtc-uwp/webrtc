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

/**
 * RateAdjuster that tracks the bandwidth produced by an encoder and dynamically adjusts the
 * bitrate.
 */
class DynamicBitrateAdjuster implements RateAdjuster {
  private static final double BITRATE_CORRECTION_SEC = 3.0;
  // Maximum bitrate correction scale - no more than 4 times.
  private static final double BITRATE_CORRECTION_MAX_SCALE = 4;
  // Amount of correction steps to reach correction maximum scale.
  private static final int BITRATE_CORRECTION_STEPS = 20;
  private static final int MAX_FPS = 30;

  private int targetBitrateBps = 0;
  private int fps = 0;

  // How far the codec has deviated above (or below) the target bitrate (tracked in bytes).
  private double deviationBytes = 0;
  // Adjust the bitrate if the codec deviates from the target bitrate by more than this amount.
  // This is held at the target bitrate (but in bytes).  If the encoder deviates from the target
  // by more than one second's worth of data, adjust the bitrate.
  private double deviationThresholdBytes = 0;
  private double timeSinceLastAdjustmentMs = 0;
  private int bitrateAdjustmentScaleExp = 0;

  @Override
  public boolean reportEncodedFrame(int size) {
    if (fps == 0) {
      return false;
    }

    // Accumulate the difference between actial and expected frame sizes.
    double expectedBytesPerFrame = targetBitrateBps / (8.0 * fps);
    deviationBytes += (size - expectedBytesPerFrame);
    timeSinceLastAdjustmentMs += 1000.0 / fps;

    // Cap the deviation, i.e., don't let it grow beyond some level to avoid using too old data for
    // bitrate adjustment.  This also prevents taking more than 3 "steps" in a given 3-second cycle.
    double deviationCap = BITRATE_CORRECTION_SEC * deviationThresholdBytes;
    deviationBytes = Math.min(deviationBytes, deviationCap);
    deviationBytes = Math.max(deviationBytes, -deviationCap);

    // Do bitrate adjustment every 3 seconds if actual encoder bitrate deviates too much
    // from the target value.
    if (timeSinceLastAdjustmentMs > 1000 * BITRATE_CORRECTION_SEC) {
      boolean bitrateAdjustmentScaleChanged = false;
      if (deviationBytes > deviationThresholdBytes) {
        // Encoder generates too high bitrate - need to reduce the scale.
        int bitrateAdjustmentInc = (int) (deviationBytes / deviationThresholdBytes + 0.5);
        bitrateAdjustmentScaleExp -= bitrateAdjustmentInc;
        deviationBytes = deviationThresholdBytes;
        bitrateAdjustmentScaleChanged = true;
      } else if (deviationBytes < -deviationThresholdBytes) {
        // Encoder generates too low bitrate - need to increase the scale.
        int bitrateAdjustmentInc = (int) (-deviationBytes / deviationThresholdBytes + 0.5);
        bitrateAdjustmentScaleExp += bitrateAdjustmentInc;
        deviationBytes = -deviationThresholdBytes;
        bitrateAdjustmentScaleChanged = true;
      }
      if (bitrateAdjustmentScaleChanged) {
        // Clamp the exponent between +/-1:  bitrateAdjustmentScaleExp is the numerator,
        // BITRATE_ADJUSTMENT_STEPS is the denominator.
        bitrateAdjustmentScaleExp = Math.min(bitrateAdjustmentScaleExp, BITRATE_CORRECTION_STEPS);
        bitrateAdjustmentScaleExp = Math.max(bitrateAdjustmentScaleExp, -BITRATE_CORRECTION_STEPS);
      }
      timeSinceLastAdjustmentMs = 0;
      return bitrateAdjustmentScaleChanged;
    }
    return false;
  }

  @Override
  public void setRates(int targetBitrateBps, int targetFps) {
    deviationThresholdBytes = targetBitrateBps / 8.0;
    if (this.targetBitrateBps > 0 && targetBitrateBps < this.targetBitrateBps) {
      // Rescale the accumulator level if the accumulator max decreases
      deviationBytes = deviationBytes * targetBitrateBps / this.targetBitrateBps;
    }
    this.targetBitrateBps = targetBitrateBps;
    this.fps = targetFps;
  }

  @Override
  public int getBitrateBps() {
    return (int) (targetBitrateBps
        * Math.pow(BITRATE_CORRECTION_MAX_SCALE,
              (double) bitrateAdjustmentScaleExp / BITRATE_CORRECTION_STEPS));
  }

  @Override
  public int getFramerate() {
    return fps;
  }
}
