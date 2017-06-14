/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.LinkedList;
import java.util.IdentityHashMap;

/** Java version of VideoTrackInterface. */
public class VideoTrack extends MediaStreamTrack {
  private final LinkedList<VideoRenderer> renderers = new LinkedList<VideoRenderer>();
  private final IdentityHashMap<VideoSink, Long> sinks = new IdentityHashMap<VideoSink, Long>();

  /**
   * Package protected since VideoTracks are only created from the PeerConnectionFactory.
   */
  VideoTrack(long nativeTrack) {
    super(nativeTrack);
  }

  public void addSink(VideoSink sink) {
    final long nativeSink = nativeWrapSink(sink);
    sinks.put(sink, nativeSink);
    nativeAddSink(nativeTrack, nativeSink);
  }

  public void removeSink(VideoSink sink) {
    final long nativeSink = sinks.remove(sink);
    if (nativeSink != 0) {
      nativeRemoveSink(nativeTrack, nativeSink);
    }
  }

  public void addRenderer(VideoRenderer renderer) {
    renderers.add(renderer);
    nativeAddRenderer(nativeTrack, renderer.nativeVideoRenderer);
  }

  public void removeRenderer(VideoRenderer renderer) {
    if (!renderers.remove(renderer)) {
      return;
    }
    nativeRemoveRenderer(nativeTrack, renderer.nativeVideoRenderer);
    renderer.dispose();
  }

  public void dispose() {
    while (!renderers.isEmpty()) {
      removeRenderer(renderers.getFirst());
    }
    for (long nativeSink : sinks.values()) {
      nativeRemoveSink(nativeTrack, nativeSink);
    }
    sinks.clear();
    super.dispose();
  }

  private static native void free(long nativeTrack);

  private static native void nativeAddRenderer(long nativeTrack, long nativeRenderer);
  private static native void nativeRemoveRenderer(long nativeTrack, long nativeRenderer);
  private static native long nativeWrapSink(VideoSink sink);
  private static native void nativeAddSink(long nativeTrack, long nativeSink);
  private static native void nativeRemoveSink(long nativeTrack, long nativeSink);
}
