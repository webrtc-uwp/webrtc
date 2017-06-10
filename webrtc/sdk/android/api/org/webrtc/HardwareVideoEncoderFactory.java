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

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecList;
import android.os.Build;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Factory for android hardware video encoders. */
public class HardwareVideoEncoderFactory implements VideoEncoderFactory {
  private static final String TAG = "HardwareVideoEncoderFactory";

  private static final String VP8_MIME_TYPE = "video/x-vnd.on2.vp8";
  private static final String VP9_MIME_TYPE = "video/x-vnd.on2.vp9";
  private static final String H264_MIME_TYPE = "video/avc";
  private static final List<String> SUPPORTED_MIME_TYPES =
      Arrays.asList(VP8_MIME_TYPE, VP9_MIME_TYPE, H264_MIME_TYPE);

  // Type of bitrate adjustment for video encoder.
  public enum BitrateAdjustmentType {
    // No adjustment - video encoder has no known bitrate problem.
    NO_ADJUSTMENT,
    // Framerate based bitrate adjustment is required - HW encoder does not use frame
    // timestamps to calculate frame bitrate budget and instead is relying on initial
    // fps configuration assuming that all frames are coming at fixed initial frame rate.
    FRAMERATE_ADJUSTMENT,
    // Dynamic bitrate adjustment is required - HW encoder used frame timestamps, but actual
    // bitrate deviates too much from the target value.
    DYNAMIC_ADJUSTMENT
  }

  // Forced key frame interval - used to reduce color distortions on Qualcomm platform.
  private static final int QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_L_MS = 15000;
  private static final int QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_M_MS = 20000;
  private static final int QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_N_MS = 15000;

  /** Description of the requirements for a valid video codec. */
  private static class VideoCodecRequirements {
    public final String codecPrefix;
    // Minimum Android SDK required for this codec to be used.
    public final int minSdk;
    // Flag if encoder implementation does not use frame timestamps to calculate frame bitrate
    // budget and instead is relying on initial fps configuration assuming that all frames are
    // coming at fixed initial frame rate. Bitrate adjustment is required for this case.
    public final BitrateAdjustmentType bitrateAdjustmentType;

    VideoCodecRequirements(
        String codecPrefix, int minSdk, BitrateAdjustmentType bitrateAdjustmentType) {
      this.codecPrefix = codecPrefix;
      this.minSdk = minSdk;
      this.bitrateAdjustmentType = bitrateAdjustmentType;
    }
  }

  // List of supported HW VP8 encoders.
  private static final VideoCodecRequirements qcomVp8HwProperties = new VideoCodecRequirements(
      "OMX.qcom.", Build.VERSION_CODES.KITKAT, BitrateAdjustmentType.NO_ADJUSTMENT);
  private static final VideoCodecRequirements exynosVp8HwProperties = new VideoCodecRequirements(
      "OMX.Exynos.", Build.VERSION_CODES.M, BitrateAdjustmentType.DYNAMIC_ADJUSTMENT);
  private static final VideoCodecRequirements intelVp8HwProperties = new VideoCodecRequirements(
      "OMX.Intel.", Build.VERSION_CODES.LOLLIPOP, BitrateAdjustmentType.NO_ADJUSTMENT);
  private static List<VideoCodecRequirements> vp8HwList() {
    final List<VideoCodecRequirements> supported_codecs = new ArrayList<VideoCodecRequirements>();
    supported_codecs.add(qcomVp8HwProperties);
    supported_codecs.add(exynosVp8HwProperties);
    if (PeerConnectionFactory.fieldTrialsFindFullName("WebRTC-IntelVP8").equals("Enabled")) {
      supported_codecs.add(intelVp8HwProperties);
    }
    return supported_codecs;
  }

  // List of supported HW VP9 encoders.
  private static final VideoCodecRequirements qcomVp9HwProperties = new VideoCodecRequirements(
      "OMX.qcom.", Build.VERSION_CODES.N, BitrateAdjustmentType.NO_ADJUSTMENT);
  private static final VideoCodecRequirements exynosVp9HwProperties = new VideoCodecRequirements(
      "OMX.Exynos.", Build.VERSION_CODES.N, BitrateAdjustmentType.FRAMERATE_ADJUSTMENT);
  private static final List<VideoCodecRequirements> vp9HwList =
      Arrays.asList(qcomVp9HwProperties, exynosVp9HwProperties);

  // List of supported HW H.264 encoders.
  private static final VideoCodecRequirements qcomH264HwProperties = new VideoCodecRequirements(
      "OMX.qcom.", Build.VERSION_CODES.KITKAT, BitrateAdjustmentType.NO_ADJUSTMENT);
  private static final VideoCodecRequirements exynosH264HwProperties = new VideoCodecRequirements(
      "OMX.Exynos.", Build.VERSION_CODES.LOLLIPOP, BitrateAdjustmentType.FRAMERATE_ADJUSTMENT);
  private static final List<VideoCodecRequirements> h264HwList =
      Arrays.asList(qcomH264HwProperties, exynosH264HwProperties);

  // List of devices with poor H.264 encoder quality.
  // HW H.264 encoder on below devices has poor bitrate control - actual
  // bitrates deviates a lot from the target value.
  private static final String[] H264_HW_EXCEPTION_MODELS =
      new String[] {"SAMSUNG-SGH-I337", "Nexus 7", "Nexus 4"};

  private static List<VideoCodecRequirements> allSupportedCodecs() {
    List<VideoCodecRequirements> allCodecs = new ArrayList<>();
    allCodecs.addAll(vp8HwList());
    allCodecs.addAll(vp9HwList);
    allCodecs.addAll(h264HwList);
    return allCodecs;
  }

  // NV12 color format supported by QCOM codec, but not declared in MediaCodec -
  // see /hardware/qcom/media/mm-core/inc/OMX_QCOMExtns.h
  private static final int COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m = 0x7FA30C04;

  // Supported color formats, in order of preference.
  private static final int[] SUPPORTED_COLOR_FORMATS = {
      MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Planar,
      MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420SemiPlanar,
      MediaCodecInfo.CodecCapabilities.COLOR_QCOM_FormatYUV420SemiPlanar,
      COLOR_QCOM_FORMATYUV420PackedSemiPlanar32m};

  /** Properties for a video codec. */
  private static class VideoCodecProperties {
    public final String name;
    public final String mimeType;
    public final int colorFormat;
    public final VideoCodecRequirements requirements;

    VideoCodecProperties(
        String name, String mimeType, int colorFormat, VideoCodecRequirements requirements) {
      this.name = name;
      this.mimeType = mimeType;
      this.colorFormat = colorFormat;
      this.requirements = requirements;
    }
  }

  private final Map<String, VideoCodecProperties> supportedCodecs = new HashMap<>();

  @Override
  public VideoEncoder createEncoder(VideoCodecInfo info) {
    loadSupportedCodecs();
    VideoCodecProperties properties = supportedCodecs.get(info.name);

    int keyFrameIntervalSec = 0;
    int forcedKeyFrameMs = 0;
    switch (properties.mimeType) {
      case VP8_MIME_TYPE:
        keyFrameIntervalSec = 100;
        if (properties.name.startsWith(qcomVp8HwProperties.codecPrefix)) {
          if (Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP
              || Build.VERSION.SDK_INT == Build.VERSION_CODES.LOLLIPOP_MR1) {
            forcedKeyFrameMs = QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_L_MS;
          } else if (Build.VERSION.SDK_INT == Build.VERSION_CODES.M) {
            forcedKeyFrameMs = QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_M_MS;
          } else if (Build.VERSION.SDK_INT > Build.VERSION_CODES.M) {
            forcedKeyFrameMs = QCOM_VP8_KEY_FRAME_INTERVAL_ANDROID_N_MS;
          }
        }
        break;
      case VP9_MIME_TYPE:
        keyFrameIntervalSec = 100;
        break;
      case H264_MIME_TYPE:
        keyFrameIntervalSec = 20;
        break;
    }

    BitrateAdjuster adjuster = new BaseBitrateAdjuster();
    switch (properties.requirements.bitrateAdjustmentType) {
      case NO_ADJUSTMENT:
        // BaseBitrateAdjuster does not adjust the bitrate.  Nothing to do.
        break;
      case FRAMERATE_ADJUSTMENT:
        adjuster = new FramerateBitrateAdjuster();
        break;
      case DYNAMIC_ADJUSTMENT:
        adjuster = new DynamicBitrateAdjuster();
        break;
    }

    return new HardwareVideoEncoder(properties.name, properties.mimeType, properties.colorFormat,
        keyFrameIntervalSec, forcedKeyFrameMs, adjuster);
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    loadSupportedCodecs();
    VideoCodecInfo[] supportedCodecInfos = new VideoCodecInfo[supportedCodecs.size()];
    int i = 0;
    for (String name : supportedCodecs.keySet()) {
      // TODO(mellem):  What actually goes in the VideoCodecInfo?
      supportedCodecInfos[i] = new VideoCodecInfo(-1, name, new HashMap<String, String>());
      ++i;
    }
    return supportedCodecInfos;
  }

  private String getSupportedMimeType(MediaCodecInfo codecInfo) {
    for (String mimeType : codecInfo.getSupportedTypes()) {
      if (SUPPORTED_MIME_TYPES.contains(mimeType)) {
        // Check if device is in H.264 exception list.
        if (mimeType.equals(H264_MIME_TYPE)) {
          List<String> exceptionModels = Arrays.asList(H264_HW_EXCEPTION_MODELS);
          if (exceptionModels.contains(Build.MODEL)) {
            Logging.w(TAG, "Model: " + Build.MODEL + " has black listed H.264 encoder.");
            continue; // H.264 encoder is not supported on this model, skip it.
          }
        }
        return mimeType;
      }
    }
    return null;
  }

  private VideoCodecRequirements getMatchingRequirements(String codecName) {
    for (VideoCodecRequirements requirements : allSupportedCodecs()) {
      if (codecName.startsWith(requirements.codecPrefix)) {
        if (Build.VERSION.SDK_INT >= requirements.minSdk) {
          return requirements;
        }
        Logging.w(
            TAG, "Codec " + codecName + " is disabled due to SDK version " + Build.VERSION.SDK_INT);
      }
    }
    return null;
  }

  private int getSupportedColorFormat(CodecCapabilities capabilities) {
    for (int supportedColorFormat : SUPPORTED_COLOR_FORMATS) {
      for (int codecColorFormat : capabilities.colorFormats) {
        if (codecColorFormat == supportedColorFormat) {
          return codecColorFormat;
        }
      }
    }
    return -1;
  }

  private void loadSupportedCodecs() {
    if (!supportedCodecs.isEmpty()) {
      // Already loaded.
      return;
    }

    // MediaCodec.setParameters is missing for JB and below, so bitrate
    // cannot be adjusted dynamically.
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
      return;
    }

    for (int i = 0; i < MediaCodecList.getCodecCount(); ++i) {
      MediaCodecInfo info = null;
      try {
        info = MediaCodecList.getCodecInfoAt(i);
      } catch (IllegalArgumentException e) {
        Logging.e(TAG, "Cannot retrieve encoder codec info", e);
      }

      // Check if the codec is an encoder.
      if (info == null || !info.isEncoder()) {
        continue;
      }

      // Check if the codec can handle any supported MIME types.
      String mime = getSupportedMimeType(info);
      if (mime == null) {
        continue;
      }

      String name = info.getName();
      Logging.v(TAG, "Found candidate encoder " + name);

      // Check if the HW encoder meets requirements.
      VideoCodecRequirements requirements = getMatchingRequirements(name);
      if (requirements == null) {
        continue;
      }

      // Check if HW codec supports known color format.
      CodecCapabilities capabilities;
      try {
        capabilities = info.getCapabilitiesForType(mime);
      } catch (IllegalArgumentException e) {
        Logging.e(TAG, "Cannot retrieve encoder capabilities", e);
        continue;
      }

      int colorFormat = getSupportedColorFormat(capabilities);
      if (colorFormat < 0) {
        continue;
      }

      // Found supported HW encoder.
      Logging.d(TAG,
          "Found target encoder for mime " + mime + " : " + name + ". Color: 0x"
              + Integer.toHexString(colorFormat));
      VideoCodecProperties properties =
          new VideoCodecProperties(name, mime, colorFormat, requirements);
      supportedCodecs.put(name, properties);
    }
  }
}
