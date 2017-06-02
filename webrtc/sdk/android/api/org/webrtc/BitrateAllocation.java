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

public class BitrateAllocation {
  public final long[][] bitrates;

  public BitrateAllocation(long[][] bitrates) {
    this.bitrates = bitrates;
  }

  public long getSum() {
    long sum = 0;
    for (long[] spatialLayer : bitrates) {
      for (long bitrate : spatialLayer) {
        sum += bitrate;
      }
    }
    return sum;
  }
}
