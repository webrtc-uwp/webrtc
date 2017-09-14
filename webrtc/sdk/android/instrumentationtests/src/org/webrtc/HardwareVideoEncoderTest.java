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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.annotation.TargetApi;
import android.graphics.Matrix;
import android.opengl.GLES11Ext;
import android.support.test.filters.SmallTest;
import java.nio.ByteBuffer;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;

@TargetApi(16)
@RunWith(BaseJUnit4ClassRunner.class)
public class HardwareVideoEncoderTest {
  final static String TAG = "HardwareVideoEncoderTest";

  private static final boolean ENABLE_INTEL_VP8_ENCODER = true;
  private static final boolean ENABLE_H264_HIGH_PROFILE = true;
  private static final VideoEncoder.Settings SETTINGS =
      new VideoEncoder.Settings(1 /* core */, 640 /* width */, 480 /* height */, 300 /* kbps */,
          30 /* fps */, true /* automaticResizeOn */);
  private static final int ENCODE_TIMEOUT_MS = 1000;
  private static final int NUM_TEST_FRAMES = 10;
  private static final int NUM_ENCODE_TRIES = 100;
  private static final int ENCODE_RETRY_SLEEP_MS = 1;

  // # Mock classes
  /**
   * Mock encoder callback that allows easy verification of the general properties of the encoded
   * frame such as width and height.
   */
  private static class MockEncoderCallback implements VideoEncoder.Callback {
    private BlockingQueue<EncodedImage> frameQueue = new LinkedBlockingQueue<>();

    public void onEncodedFrame(EncodedImage frame, VideoEncoder.CodecSpecificInfo info) {
      frameQueue.offer(frame);
    }

    public EncodedImage poll() {
      try {
        return frameQueue.poll(ENCODE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
      } catch (InterruptedException e) {
        fail("Timeout waiting from frame to be encoded.");
        throw new RuntimeException(e);
      }
    }

    public void assertFrameEncoded(VideoFrame frame) {
      final VideoFrame.Buffer buffer = frame.getBuffer();
      final EncodedImage image = poll();
      assertTrue(image.buffer.capacity() > 0);
      assertEquals(image.encodedWidth, buffer.getWidth());
      assertEquals(image.encodedHeight, buffer.getHeight());
      assertEquals(image.captureTimeNs, frame.getTimestampNs());
      assertEquals(image.rotation, frame.getRotation());
    }
  }

  /** A common base class for the texture and I420 buffer that implements reference counting. */
  private static abstract class MockBufferBase implements VideoFrame.Buffer {
    protected final int width;
    protected final int height;
    private final Runnable releaseCallback;
    private final Object refCountLock = new Object();
    private int refCount = 1;

    public MockBufferBase(int width, int height, Runnable releaseCallback) {
      this.width = width;
      this.height = height;
      this.releaseCallback = releaseCallback;
    }

    @Override
    public int getWidth() {
      return width;
    }

    @Override
    public int getHeight() {
      return height;
    }

    @Override
    public void retain() {
      synchronized (refCountLock) {
        assertTrue("Texture buffer retained after being destroyed.", refCount > 0);
        ++refCount;
      }
    }

    @Override
    public void release() {
      synchronized (refCountLock) {
        assertTrue("Texture buffer released too many times.", --refCount >= 0);
        if (refCount == 0) {
          releaseCallback.run();
        }
      }
    }
  }

  private static class MockTextureBuffer
      extends MockBufferBase implements VideoFrame.TextureBuffer {
    private final int textureId;

    public MockTextureBuffer(int textureId, int width, int height, Runnable releaseCallback) {
      super(width, height, releaseCallback);
      this.textureId = textureId;
    }

    @Override
    public VideoFrame.TextureBuffer.Type getType() {
      return VideoFrame.TextureBuffer.Type.OES;
    }

    @Override
    public int getTextureId() {
      return textureId;
    }

    @Override
    public Matrix getTransformMatrix() {
      return new Matrix();
    }

    @Override
    public VideoFrame.I420Buffer toI420() {
      return I420BufferImpl.allocate(width, height);
    }

    @Override
    public VideoFrame.Buffer cropAndScale(
        int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight) {
      retain();
      return new MockTextureBuffer(textureId, scaleWidth, scaleHeight, this ::release);
    }
  }

  private static class MockI420Buffer extends MockBufferBase implements VideoFrame.I420Buffer {
    private final I420BufferImpl realBuffer;

    public MockI420Buffer(int width, int height, Runnable releaseCallback) {
      super(width, height, releaseCallback);
      // We never release this but it is not a problem in practice because the release is a no-op.
      realBuffer = I420BufferImpl.allocate(width, height);
    }

    @Override
    public ByteBuffer getDataY() {
      return realBuffer.getDataY();
    }

    @Override
    public ByteBuffer getDataU() {
      return realBuffer.getDataU();
    }

    @Override
    public ByteBuffer getDataV() {
      return realBuffer.getDataV();
    }

    @Override
    public int getStrideY() {
      return realBuffer.getStrideY();
    }

    @Override
    public int getStrideU() {
      return realBuffer.getStrideU();
    }

    @Override
    public int getStrideV() {
      return realBuffer.getStrideV();
    }

    @Override
    public VideoFrame.I420Buffer toI420() {
      retain();
      return this;
    }

    @Override
    public VideoFrame.Buffer cropAndScale(
        int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight) {
      return realBuffer.cropAndScale(cropX, cropY, cropWidth, cropHeight, scaleWidth, scaleHeight);
    }
  }

  // # Test fields
  private Object referencedFramesLock = new Object();
  private int referencedFrames = 0;

  private Runnable releaseFrameCallback = new Runnable() {
    public void run() {
      synchronized (referencedFramesLock) {
        --referencedFrames;
      }
    }
  };

  private EglBase14 eglBase;
  private long lastTimestampNs;

  // # Helper methods
  private VideoEncoderFactory createEncoderFactory(EglBase.Context eglContext) {
    return new HardwareVideoEncoderFactory(
        eglContext, ENABLE_INTEL_VP8_ENCODER, ENABLE_H264_HIGH_PROFILE);
  }

  private VideoEncoder createEncoder(EglBase.Context eglContext) {
    VideoEncoderFactory factory = createEncoderFactory(eglContext);
    VideoCodecInfo[] supportedCodecs = factory.getSupportedCodecs();
    return factory.createEncoder(supportedCodecs[0]);
  }

  private VideoEncoder createEncoder() {
    return createEncoder(eglBase.getEglBaseContext());
  }

  private VideoFrame generateI420Frame(int width, int height) {
    lastTimestampNs += TimeUnit.SECONDS.toNanos(1) / SETTINGS.maxFramerate;
    VideoFrame.I420Buffer buffer = I420BufferImpl.allocate(width, height);
    VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, lastTimestampNs);
    return frame;
  }

  private VideoFrame generateTextureFrame(int width, int height) {
    lastTimestampNs += TimeUnit.SECONDS.toNanos(1) / SETTINGS.maxFramerate;
    final int textureId = GlUtil.generateTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES);
    synchronized (referencedFramesLock) {
      ++referencedFrames;
    }
    VideoFrame.TextureBuffer buffer =
        new MockTextureBuffer(textureId, width, height, releaseFrameCallback);
    VideoFrame frame = new VideoFrame(buffer, 0 /* rotation */, lastTimestampNs);
    return frame;
  }

  private void testEncodeFrame(
      VideoEncoder encoder, VideoFrame frame, VideoEncoder.EncodeInfo info) {
    int numTries = 0;

    // It takes a while for the encoder to become ready so try until it accepts the frame.
    while (true) {
      ++numTries;

      final VideoCodecStatus returnValue = encoder.encode(frame, info);
      switch (returnValue) {
        case OK:
          return; // Success
        case NO_OUTPUT:
          if (numTries < NUM_ENCODE_TRIES) {
            try {
              Thread.sleep(ENCODE_RETRY_SLEEP_MS); // Try again.
            } catch (InterruptedException e) {
              throw new RuntimeException(e);
            }
            break;
          } else {
            fail("encoder.encode keeps returning NO_OUTPUT");
          }
        default:
          fail("encoder.encode returned: " + returnValue); // Error
      }
    }
  }

  // # Tests
  @Before
  public void setUp() {
    eglBase = new EglBase14(null, EglBase.CONFIG_PLAIN);
    eglBase.createDummyPbufferSurface();
    eglBase.makeCurrent();
    lastTimestampNs = System.nanoTime();
  }

  @After
  public void tearDown() {
    eglBase.release();
    synchronized (referencedFramesLock) {
      assertEquals(0, referencedFrames);
    }
  }

  @Test
  @SmallTest
  public void testInitialize() {
    VideoEncoder encoder = createEncoder();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, null));
    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  @Test
  @SmallTest
  public void testInitializeWithoutEglContext() {
    VideoEncoder encoder = createEncoder(null /* eglContext */);
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, null));
    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  @Test
  // TODO(sakal): Fix the implementation.
  @Ignore("Does not work yet.")
  @SmallTest
  public void testEncodeI420Buffers() {
    VideoEncoder encoder = createEncoder();
    MockEncoderCallback callback = new MockEncoderCallback();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, callback));

    for (int i = 0; i < NUM_TEST_FRAMES; i++) {
      VideoFrame frame = generateI420Frame(SETTINGS.width, SETTINGS.height);
      VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
          new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameDelta});
      testEncodeFrame(encoder, frame, info);

      callback.assertFrameEncoded(frame);
      frame.release();
    }

    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  @Test
  @SmallTest
  public void testEncodeTextureBuffers() {
    VideoEncoder encoder = createEncoder();
    MockEncoderCallback callback = new MockEncoderCallback();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, callback));

    for (int i = 0; i < NUM_TEST_FRAMES; i++) {
      VideoFrame frame = generateTextureFrame(SETTINGS.width, SETTINGS.height);
      VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
          new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameDelta});
      testEncodeFrame(encoder, frame, info);

      callback.assertFrameEncoded(frame);
      frame.release();
    }

    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  @Test
  @SmallTest
  public void testEncodeI420BuffersWithoutEglContext() {
    VideoEncoder encoder = createEncoder(null /* eglContext */);
    MockEncoderCallback callback = new MockEncoderCallback();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, callback));

    for (int i = 0; i < NUM_TEST_FRAMES; i++) {
      VideoFrame frame = generateI420Frame(SETTINGS.width, SETTINGS.height);
      VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
          new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameDelta});
      testEncodeFrame(encoder, frame, info);

      callback.assertFrameEncoded(frame);
      frame.release();
    }

    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  @Test
  // TODO(sakal): Fix the implementation.
  @Ignore("Does not work yet.")
  @SmallTest
  public void testEncodeAltenatingBuffers() {
    VideoEncoder encoder = createEncoder();
    MockEncoderCallback callback = new MockEncoderCallback();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, callback));

    for (int i = 0; i < NUM_TEST_FRAMES; i++) {
      VideoFrame frame;
      VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
          new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameDelta});

      frame = generateTextureFrame(SETTINGS.width, SETTINGS.height);
      testEncodeFrame(encoder, frame, info);
      callback.assertFrameEncoded(frame);
      frame.release();

      frame = generateI420Frame(SETTINGS.width, SETTINGS.height);
      testEncodeFrame(encoder, frame, info);
      callback.assertFrameEncoded(frame);
      frame.release();
    }

    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  @Test
  // TODO(sakal): Fix the implementation.
  @Ignore("Does not work yet.")
  @SmallTest
  public void testEncodeI420BuffersOfDifferentSizes() {
    VideoEncoder encoder = createEncoder();
    MockEncoderCallback callback = new MockEncoderCallback();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, callback));

    VideoFrame frame;
    VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
        new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameDelta});

    frame = generateI420Frame(SETTINGS.width / 2, SETTINGS.height / 2);
    testEncodeFrame(encoder, frame, info);
    callback.assertFrameEncoded(frame);
    frame.release();

    frame = generateI420Frame(SETTINGS.width, SETTINGS.height);
    testEncodeFrame(encoder, frame, info);
    callback.assertFrameEncoded(frame);
    frame.release();

    frame = generateI420Frame(SETTINGS.width / 4, SETTINGS.height / 4);
    testEncodeFrame(encoder, frame, info);
    callback.assertFrameEncoded(frame);
    frame.release();

    assertEquals(VideoCodecStatus.OK, encoder.release());
  }

  @Test
  // TODO(sakal): Fix the implementation.
  @Ignore("Does not work yet.")
  @SmallTest
  public void testEncodeTextureBuffersOfDifferentSizes() {
    VideoEncoder encoder = createEncoder();
    MockEncoderCallback callback = new MockEncoderCallback();
    assertEquals(VideoCodecStatus.OK, encoder.initEncode(SETTINGS, callback));

    VideoFrame frame;
    VideoEncoder.EncodeInfo info = new VideoEncoder.EncodeInfo(
        new EncodedImage.FrameType[] {EncodedImage.FrameType.VideoFrameDelta});

    frame = generateTextureFrame(SETTINGS.width / 2, SETTINGS.height / 2);
    testEncodeFrame(encoder, frame, info);
    callback.assertFrameEncoded(frame);
    frame.release();

    frame = generateTextureFrame(SETTINGS.width, SETTINGS.height);
    testEncodeFrame(encoder, frame, info);
    callback.assertFrameEncoded(frame);
    frame.release();

    frame = generateTextureFrame(SETTINGS.width / 4, SETTINGS.height / 4);
    testEncodeFrame(encoder, frame, info);
    callback.assertFrameEncoded(frame);
    frame.release();

    assertEquals(VideoCodecStatus.OK, encoder.release());
  }
}
