/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.codec;

import android.graphics.Matrix;
import java.nio.ByteBuffer;
import org.webrtc.EncodedImage;
import org.webrtc.I420BufferImpl;
import org.webrtc.Logging;
import org.webrtc.VideoDecoder;
import org.webrtc.VideoFrame;

public class DummyVideoDecoder implements VideoDecoder {
  private static final String TAG = "DummyVideoDecoder";

  private Callback decodeCallback;

  /**
   * Initializes the decoding process with specified settings. Will be called on the decoding thread
   * before any decode calls.
   */
  @Override
  public void initDecode(Settings settings, Callback decodeCallback) {
    this.decodeCallback = decodeCallback;
  }

  /**
   * Called when the decoder is no longer needed. Any more calls to decode will not be made.
   */
  @Override
  public void release() {}

  /**
   * Request the decoder to decode a frame.
   */
  @Override
  public void decode(EncodedImage frame, DecodeInfo info) {
    decodeCallback.onDecodedFrame(new VideoFrame(new I420BufferImpl(640, 480), frame.rotation,
                                      frame.captureTimeMs * 1000 * 1000, new Matrix()),
        null, null);
  }

  /**
   * The decoder should return true if it prefers late decoding. That is, it can not decode
   * infinite number of frames before the decoded frame is consumed.
   */
  @Override
  public boolean getPrefersLateDecoding() {
    return true;
  }

  /** Should return a descriptive name for the implementation. */
  @Override
  public String getImplementationName() {
    return "DummyVideoDecoder";
  }
}
