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
 * Represents bitrate allocated for an encoder to produce frames. Bitrate can be divided between
 * spatial and temporal layers.
 */
public class BitrateAllocation {
  // First index is the spatial layer and second the temporal layer.
  public final long[][] bitratesBbs;

  /**
   * Initializes the allocation with a two dimensional array of bitrates. The first index of the
   * array is the spatial layer and the second index in the temporal layer.
   */
  public BitrateAllocation(long[][] bitratesBbs) {
    this.bitratesBbs = bitratesBbs;
  }

  /**
   * Gets the total bitrate allocated for all layers.
   */
  public long getSum() {
    long sum = 0;
    for (long[] spatialLayer : bitratesBbs) {
      for (long bitrate : spatialLayer) {
        sum += bitrate;
      }
    }
    return sum;
  }
}
