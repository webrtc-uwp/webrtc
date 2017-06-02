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

public class RtpFragmentationHeader {
  public final long[] fragmentationOffset;
  public final long[] fragmentationLength;
  public final int[] fragmentationTimeDiff;
  public final byte[] fragmentationPlType;

  public RtpFragmentationHeader(long[] fragmentationOffset, long[] fragmentationLength,
      int[] fragmentationTimeDiff, byte[] fragmentationPlType) {
    this.fragmentationOffset = fragmentationOffset;
    this.fragmentationLength = fragmentationLength;
    this.fragmentationTimeDiff = fragmentationTimeDiff;
    this.fragmentationPlType = fragmentationPlType;
  }
}
