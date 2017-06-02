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

import java.nio.ByteBuffer;

/**
 * An encoded frame from a video stream. Used an input for decoders and as an output for encoders.
 */
public class EncodedImage {
  public enum FrameType {
    EmptyFrame,
    VideoFrameKey,
    VideoFrameDelta,
  }

  public final ByteBuffer buffer;
  public int encodedWidth;
  public int encodedHeight;
  public long timeStampMs;
  public long captureTimeMs;
  public FrameType frameType;
  public int rotation;
  public boolean completeFrame;
  public Integer qp;

  public EncodedImage(ByteBuffer buffer) {
    this.buffer = buffer;
  }
}
