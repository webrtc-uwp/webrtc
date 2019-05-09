#ifndef REMOTING_API_H
#define REMOTING_API_H

#include <cstdint>

#if defined(WEBRTC_WIN)
#include <d3d11.h>
#endif

// TODO: this is an outward-facing header (should be the only one), meaning it
// should not  depend on WEBRTC_* macros being defined, so we need to set
// dllexport manually based on platform-specific defines.  this is a "nice to
// have", I guess most people will use the unity integration anyway, in which
// case it's  all run-time dynamic linking without any kind of C compiler
// involved.

#if defined(WEBRTC_WIN)
#define REMOTING_API __declspec(dllexport)
#elif defined(WEBRTC_ANDROID)
#define REMOTING_API __attribute__((visibility("default")))
#endif


// Forward declarations
// --------------------------------------------------------------------------------
struct ErrorInfo;
struct XrStereoPose;
struct RawFrame;


// Type aliases
// --------------------------------------------------------------------------------

// A value of 0 denotes failure. The failure callback is always called before 0 is returned.
#define REMOTING_HANDLE uint32_t


// Structs
// --------------------------------------------------------------------------------

struct Matrix4x4 {
    // Format: _RowCol
    float _11;
    float _12;
    float _13;
    float _14;
    float _21;
    float _22;
    float _23;
    float _24;
    float _31;
    float _32;
    float _33;
    float _34;
    float _41;
    float _42;
    float _43;
    float _44;
};

struct Vector3 {
    float x;
    float y;
    float z;
};

struct XrStereoPose {

    // View matrices
    Matrix4x4 viewLeft;
    Matrix4x4 viewRight;

    // Projection Matrices
    Matrix4x4 projLeft;
    Matrix4x4 projRight;

    // Utility data
    Vector3 cameraPosition;
    Vector3 cameraOrientation;
    // Vector3 focusPoint;
};

// Unused for now
// struct XrMonoPose {
//     Matrix viewProjection;
// };


// TODO: Provide conversion from Error type (in error.h)
struct ErrorInfo {
    int code;
    const char * const message;
};

struct RawFrameDesc {
    uint32_t width;
    uint32_t height;

    int stride_y;
    int stride_u;
    int stride_v;
    int stride_a;
};

struct RawFrame {
    RawFrameDesc desc;

    uint8_t* _data_y = nullptr;
    uint8_t* _data_u = nullptr;
    uint8_t* _data_v = nullptr;
    uint8_t* _data_a = nullptr;
};


// Cross-platform Graphics API structs
// --------------------------------------------------------------------------------

struct GraphicsApiConfig {
#if defined(WEBRTC_WIN)
    ID3D11Device* d3d_device;
    ID3D11DeviceContext* d3d_context;
    D3D11_TEXTURE2D_DESC* render_target_desc;
#endif
};

struct GraphicsApiFrame {
#if defined(WEBRTC_WIN)
    ID3D11Texture2D* d3d_frame;
#endif
};


// Config structs
// --------------------------------------------------------------------------------

struct ServerConfig {

  bool useSoftwareEncoder = false;
  GraphicsApiConfig graphicsApiConfig;

  // TODO: std::vector<std::string> ice_servers;
};

struct ClientConfig {

  bool useSoftwareDecoder = false;

  // TODO: std::vector<std::string> ice_servers;
};

// Callbacks
// --------------------------------------------------------------------------------
typedef void (*FailureCallback)(ErrorInfo errorCode);

// Markus:
// TODO: Think about what we do about these nested namespaces. Exportet things should probaly be top-level...
typedef void (*FrameCallback)(RawFrame frame, void* userData);
typedef void (*PoseCallback)(XrStereoPose* pose, void* userData); // Poses are pretty big, so we pass a pointer instead of copying

// Markus:
// TODO: Since we don't have namespaces in C, consider renaming all these to stuff like HoloRemotingInit... etc.

// API
// --------------------------------------------------------------------------------
extern "C" {


    // General
    // --------------------------------------------------------------------------------

    // Call this before anything else
    REMOTING_API void Init(FailureCallback onFailure);

    // REMOTING_API ServerConfig ParseServerConfigJson(const char * const path);

    // REMOTING_API ClientConfig ParseClientConfigJson(const char * const path);


    // Connection Lifecycle (blocking)
    // These APIs are blocking so clients can use async/await patterns that are
    // available in their higher level languages instead of relying on weird C callbacks
    // --------------------------------------------------------------------------------

    REMOTING_API REMOTING_HANDLE ConnectToServerTcp(const char * const ip, uint16_t port, ClientConfig clientConfig);
    REMOTING_API REMOTING_HANDLE ListenForClientTcp(uint16_t port, ServerConfig serverConfig);

    REMOTING_API void CloseConnection(REMOTING_HANDLE hConn);

    // Server API
    // --------------------------------------------------------------------------------

    // callback will be called immediately (on same thread) or not at all
    // return value indicates whether callback was executed (true) or not (false)
    // callback arguments are only guaranteed to be valid inside the callback
    // servers should avoid rendering a scene if false is returned
    REMOTING_API bool TryRunWithNextPose(REMOTING_HANDLE hConn, PoseCallback poseCallback, void* userData = nullptr);

    REMOTING_API void PushFrameAsync(REMOTING_HANDLE hConn, GraphicsApiFrame frame);


    // Client API
    // --------------------------------------------------------------------------------

    // callback will be called immediately (on same thread) or not at all
    // return value indicates whether callback was executed (true) or not (false)
    // callback arguments are only guaranteed to be valid inside the callback
    // clients should call this API inside a fairly tight loop
    REMOTING_API bool TryRunWithNextFrame(REMOTING_HANDLE hConn, FrameCallback frameCallback, void* userData = nullptr);

    REMOTING_API void PushPoseAsync(REMOTING_HANDLE hConn, XrStereoPose* pose);
}

#endif  // REMOTING_API_H