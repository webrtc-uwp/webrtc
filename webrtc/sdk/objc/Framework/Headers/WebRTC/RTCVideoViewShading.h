/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#import <WebRTC/RTCVideoFrame.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * RTCVideoViewShading provides a way for apps to customize the OpenGL(ES) shaders used in
 * rendering for the RTCEAGLVideoView/RTCNSGLVideoView.
 */
RTC_EXPORT
@protocol RTCVideoViewShading <NSObject>

/** Callback for I420 frames. Each plane is given as a texture. */
- (void)applyShadingForFrameWithWidth:(int)width
                               height:(int)height
                             rotation:(RTCVideoRotation)rotation
                               yPlane:(GLuint)yPlane
                               uPlane:(GLuint)uPlane
                               vPlane:(GLuint)vPlane;

/** Callback for NV12 frames. Each plane is given as a texture. */
- (void)applyShadingForFrameWithWidth:(int)width
                               height:(int)height
                             rotation:(RTCVideoRotation)rotation
                               yPlane:(GLuint)yPlane
                              uvPlane:(GLuint)uvPlane;

@end

/** Default RTCVideoViewShading that will be used in RTCNSGLVideoView and
 *  RTCEAGLVideoView if no external shader is specified. This shader will render
 *  the video in a rectangle without any color or geometric transformations.
 */
RTC_EXPORT
@interface RTCDefaultShader : NSObject<RTCVideoViewShading>

@end

NS_ASSUME_NONNULL_END
