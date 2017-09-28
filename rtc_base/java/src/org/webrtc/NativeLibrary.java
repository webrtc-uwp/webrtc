/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

public class NativeLibrary {
  private static String TAG = "NativeLibrary";

  public interface Loader {
    /**
     * Loads a native library with the given name.
     *
     * @return True on success
     */
    boolean load(String name);
  }

  static class DefaultLoader implements Loader {
    @Override
    public boolean load(String name) {
      Logging.d(TAG, "Loading library: " + name);
      try {
        System.loadLibrary(name);
      } catch (UnsatisfiedLinkError e) {
        Logging.e(TAG, "Failed to load native library: " + name, e);
        return false;
      }
      return true;
    }
  }

  private static Object lock = new Object();
  private static boolean libraryLoaded = false;

  /**
   * Loads the native library. Clients should call PeerConnectionFactory.initialize. It will call
   * this method for them.
   */
  static void initialize(Loader loader) {
    synchronized (lock) {
      if (libraryLoaded) {
        Logging.d(TAG, "Native library has already been loaded.");
        return;
      }
      Logging.d(TAG, "Loading native library.");
      libraryLoaded = loader.load("jingle_peerconnection_so");
    }
  }

  /** Returns true if the library has been loaded successfully. */
  static boolean isLoaded() {
    synchronized (lock) {
      return libraryLoaded;
    }
  }
}
