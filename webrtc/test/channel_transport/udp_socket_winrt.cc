/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/channel_transport/udp_socket_winrt.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <WS2TCPIP.h>
#include <winsock2.h>
#include <io.h>

#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/test/channel_transport/udp_socket_manager_wrapper.h"
#include "webrtc/test/channel_transport/udp_socket_wrapper.h"

#if defined(WINRT)
#  ifdef InitializeCriticalSection
#    undef InitializeCriticalSection
#  endif
#define InitializeCriticalSection(a) InitializeCriticalSectionEx(a, 0, 0)
#endif

namespace webrtc {
namespace test {
UdpSocketWinRT::UdpSocketWinRT(const int32_t id, UdpSocketManager* mgr,
                               bool ipV6Enable) : _id(id)
{
    WEBRTC_TRACE(kTraceMemory, kTraceTransport, id,
                 "UdpSocketWinRT::UdpSocketWinRT()");

    _wantsIncoming = false;
    _mgr = mgr;

    _obj = NULL;
    _incomingCb = NULL;
    _readyForDeletionCond = new webrtc::ConditionVariableEventWin;
    _closeBlockingCompletedCond =
        new webrtc::ConditionVariableEventWin;
    InitializeCriticalSection(&_cs);
    _readyForDeletion = false;
    _closeBlockingActive = false;
    _closeBlockingCompleted= false;
    if(ipV6Enable)
    {
        _socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    }
    else {
        _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }

    // Set socket to nonblocking mode.
    u_long enable_non_blocking = 1;
    if(ioctlsocket (_socket, FIONBIO, &enable_non_blocking) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceTransport, id,
                     "Failed to make socket nonblocking");
    }
    // Enable close on fork for file descriptor so that it will not block until
    // forked process terminates.
    /*if(fcntl(_socket, F_SETFD, FD_CLOEXEC) == -1)
    {
        WEBRTC_TRACE(kTraceWarning, kTraceTransport, id,
                     "Failed to set FD_CLOEXEC for socket");
    }*/
}

UdpSocketWinRT::~UdpSocketWinRT()
{
    if(_socket != INVALID_SOCKET)
    {
        closesocket(_socket);
        _socket = INVALID_SOCKET;
    }
    if(_readyForDeletionCond)
    {
        delete _readyForDeletionCond;
    }

    if(_closeBlockingCompletedCond)
    {
        delete _closeBlockingCompletedCond;
    }

    DeleteCriticalSection(&_cs);
}

bool UdpSocketWinRT::SetCallback(CallbackObj obj, IncomingSocketCallback cb)
{
    _obj = obj;
    _incomingCb = cb;

    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketWinRT(%p)::SetCallback", this);

    if (_mgr->AddSocket(this))
      {
        WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                     "UdpSocketWinRT(%p)::SetCallback socket added to manager",
                     this);
        return true;   // socket is now ready for action
      }

    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketWinRT(%p)::SetCallback error adding me to mgr",
                 this);
    return false;
}

bool UdpSocketWinRT::SetSockopt(int32_t level, int32_t optname,
                                const int8_t* optval, int32_t optlen)
{
   if(0 == setsockopt(_socket, level, optname, (const char*)optval, optlen ))
   {
       return true;
   }

   WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                "UdpSocketWinRT::SetSockopt(), error:%d", errno);
   return false;
}

int32_t UdpSocketWinRT::SetTOS(int32_t serviceType)
{
    if (SetSockopt(IPPROTO_IP, IP_TOS ,(int8_t*)&serviceType ,4) != 0)
    {
        return -1;
    }
    return 0;
}

bool UdpSocketWinRT::Bind(const SocketAddress& name)
{
    int size = sizeof(sockaddr);
    if (0 == bind(_socket, reinterpret_cast<const sockaddr*>(&name),size))
    {
        return true;
    }
    WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                 "UdpSocketWinRT::Bind() error: %d", errno);
    return false;
}

int32_t UdpSocketWinRT::SendTo(const int8_t* buf, size_t len,
                               const SocketAddress& to)
{
    int size = sizeof(sockaddr);

    // since it's winsock2, which expects int, disable warning for this line
#pragma warning (disable:4267)
    int retVal = sendto(_socket, (const char*)buf, len, 0,
                        reinterpret_cast<const sockaddr*>(&to), size);
#pragma warning (default:4267)
    if(retVal == SOCKET_ERROR)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, _id,
                     "UdpSocketWinRT::SendTo() error: %d", errno);
    }

    return retVal;
}

SOCKET UdpSocketWinRT::GetFd() { return _socket; }

bool UdpSocketWinRT::ValidHandle()
{
    return _socket != INVALID_SOCKET;
}

bool UdpSocketWinRT::SetQos(int32_t /*serviceType*/,
                            int32_t /*tokenRate*/,
                            int32_t /*bucketSize*/,
                            int32_t /*peekBandwith*/,
                            int32_t /*minPolicedSize*/,
                            int32_t /*maxSduSize*/,
                            const SocketAddress& /*stRemName*/,
                            int32_t /*overrideDSCP*/) {
  return false;
}

void UdpSocketWinRT::HasIncoming()
{
    // replace 2048 with a mcro define and figure out
    // where 2048 comes from
    int8_t buf[2048];
    int retval;
    SocketAddress from;
#if defined(WEBRTC_MAC)
    sockaddr sockaddrfrom;
    memset(&from, 0, sizeof(from));
    memset(&sockaddrfrom, 0, sizeof(sockaddrfrom));
    socklen_t fromlen = sizeof(sockaddrfrom);
#else
    memset(&from, 0, sizeof(from));
    socklen_t fromlen = sizeof(from);
#endif

#if defined(WEBRTC_MAC)
        retval = recvfrom(_socket,buf, sizeof(buf), 0,
                          reinterpret_cast<sockaddr*>(&sockaddrfrom), &fromlen);
        memcpy(&from, &sockaddrfrom, fromlen);
        from._sockaddr_storage.sin_family = sockaddrfrom.sa_family;
#else
    retval = recvfrom(_socket, reinterpret_cast<char*>(buf), sizeof(buf), 0,
                          reinterpret_cast<sockaddr*>(&from), &fromlen);
#endif

    switch(retval)
    {
    case 0:
        // The peer has performed an orderly shutdown.
        break;
    case SOCKET_ERROR:
        break;
    default:
        if (_wantsIncoming && _incomingCb)
        {
          _incomingCb(_obj, buf, retval, &from);
        }
        break;
    }
}

bool UdpSocketWinRT::WantsIncoming() { return _wantsIncoming; }

void UdpSocketWinRT::CloseBlocking()
{
    EnterCriticalSection(&_cs);
    _closeBlockingActive = true;
    if(!CleanUp())
    {
        _closeBlockingActive = false;
        LeaveCriticalSection(&_cs);
        return;
    }

    while(!_readyForDeletion)
    {
        _readyForDeletionCond->SleepCS(&_cs);
    }
    _closeBlockingCompleted = true;
    _closeBlockingCompletedCond->Wake();
    LeaveCriticalSection(&_cs);
}

void UdpSocketWinRT::ReadyForDeletion()
{
    EnterCriticalSection(&_cs);
    if(!_closeBlockingActive)
    {
        LeaveCriticalSection(&_cs);
        return;
    }
    closesocket(_socket);
    _socket = INVALID_SOCKET;
    _readyForDeletion = true;
    _readyForDeletionCond->Wake();
    while(!_closeBlockingCompleted)
    {
        _closeBlockingCompletedCond->SleepCS(&_cs);
    }
    LeaveCriticalSection(&_cs);
}

bool UdpSocketWinRT::CleanUp()
{
    _wantsIncoming = false;

    if (_socket == INVALID_SOCKET)
    {
        return false;
    }

    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "calling UdpSocketManager::RemoveSocket()...");
    _mgr->RemoveSocket(this);
    // After this, the socket should may be or will be as deleted. Return
    // immediately.
    return true;
}

}  // namespace test
}  // namespace webrtc
