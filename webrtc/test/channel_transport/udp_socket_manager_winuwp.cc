/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/channel_transport/udp_socket_manager_winuwp.h"

#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <winsock2.h>

#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/test/channel_transport/udp_socket_winuwp.h"

namespace webrtc {
namespace test {

UdpSocketManagerWinUWP::UdpSocketManagerWinUWP()
    : UdpSocketManager(),
      _id(-1),
      _critSect(CriticalSectionWrapper::CreateCriticalSection()),
      _numberOfSocketMgr((uint8_t)-1),
      _incSocketMgrNextTime(0),
      _nextSocketMgrToAssign(0),
      _socketMgr()
{
}

bool UdpSocketManagerWinUWP::Init(int32_t id, uint8_t& numOfWorkThreads) {
    CriticalSectionScoped cs(_critSect);
    if ((_id != -1) || (_numOfWorkThreads != 0)) {
        assert(_id != -1);
        assert(_numOfWorkThreads != 0);
        return false;
    }

    _id = id;
    _numberOfSocketMgr = numOfWorkThreads;
    _numOfWorkThreads = numOfWorkThreads;

    if(MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX < _numberOfSocketMgr)
    {
        _numberOfSocketMgr = MAX_NUMBER_OF_SOCKET_MANAGERS_LINUX;
    }
    for(int i = 0;i < _numberOfSocketMgr; i++)
    {
        _socketMgr[i] = new UdpSocketManagerWinUWPImpl();
    }
    return true;
}


UdpSocketManagerWinUWP::~UdpSocketManagerWinUWP()
{
    Stop();
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerWinUWP(%d)::UdpSocketManagerWinUWP()",
                 _numberOfSocketMgr);

    for(int i = 0;i < _numberOfSocketMgr; i++)
    {
        delete _socketMgr[i];
    }
    delete _critSect;
}

bool UdpSocketManagerWinUWP::Start()
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerWinUWP(%d)::Start()",
                 _numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = true;
    for(int i = 0;i < _numberOfSocketMgr && retVal; i++)
    {
        retVal = _socketMgr[i]->Start();
    }
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerWinUWP(%d)::Start() error starting socket managers",
            _numberOfSocketMgr);
    }
    _critSect->Leave();
    return retVal;
}

bool UdpSocketManagerWinUWP::Stop()
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerWinUWP(%d)::Stop()",_numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = true;
    for(int i = 0; i < _numberOfSocketMgr && retVal; i++)
    {
        retVal = _socketMgr[i]->Stop();
    }
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerWinUWP(%d)::Stop() there are still active socket "
            "managers",
            _numberOfSocketMgr);
    }
    _critSect->Leave();
    return retVal;
}

bool UdpSocketManagerWinUWP::AddSocket(UdpSocketWrapper* s)
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerWinUWP(%d)::AddSocket()",_numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = _socketMgr[_nextSocketMgrToAssign]->AddSocket(s);
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerWinUWP(%d)::AddSocket() failed to add socket to\
 manager",
            _numberOfSocketMgr);
    }

    // Distribute sockets on UdpSocketManagerWinUWPImpls in a round-robin
    // fashion.
    if(_incSocketMgrNextTime == 0)
    {
        _incSocketMgrNextTime++;
    } else {
        _incSocketMgrNextTime = 0;
        _nextSocketMgrToAssign++;
        if(_nextSocketMgrToAssign >= _numberOfSocketMgr)
        {
            _nextSocketMgrToAssign = 0;
        }
    }
    _critSect->Leave();
    return retVal;
}

bool UdpSocketManagerWinUWP::RemoveSocket(UdpSocketWrapper* s)
{
    WEBRTC_TRACE(kTraceDebug, kTraceTransport, _id,
                 "UdpSocketManagerWinUWP(%d)::RemoveSocket()",
                 _numberOfSocketMgr);

    _critSect->Enter();
    bool retVal = false;
    for(int i = 0;i < _numberOfSocketMgr && (retVal == false); i++)
    {
        retVal = _socketMgr[i]->RemoveSocket(s);
    }
    if(!retVal)
    {
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            _id,
            "UdpSocketManagerWinUWP(%d)::RemoveSocket() failed to remove socket\
 from manager",
            _numberOfSocketMgr);
    }
    _critSect->Leave();
    return retVal;
}


UdpSocketManagerWinUWPImpl::UdpSocketManagerWinUWPImpl()
{
    _critSectList = CriticalSectionWrapper::CreateCriticalSection();
    _thread.reset(new rtc::PlatformThread(UdpSocketManagerWinUWPImpl::Run, this,
                                          "UdpSocketManagerWinUWPImplThread"));
    FD_ZERO(&_readFds);
    WEBRTC_TRACE(kTraceMemory,  kTraceTransport, -1,
                 "UdpSocketManagerWinUWP created");
}

UdpSocketManagerWinUWPImpl::~UdpSocketManagerWinUWPImpl()
{
    if (_critSectList != NULL)
    {
        UpdateSocketMap();

        _critSectList->Enter();
        for (std::map<SOCKET, UdpSocketWinUWP*>::iterator it =
                 _socketMap.begin();
             it != _socketMap.end();
             ++it) {
          delete it->second;
        }
        _socketMap.clear();
        _critSectList->Leave();

        delete _critSectList;
    }

    WEBRTC_TRACE(kTraceMemory,  kTraceTransport, -1,
                 "UdpSocketManagerWinUWP deleted");
}

bool UdpSocketManagerWinUWPImpl::Start()
{
    if (!_thread)
    {
        return false;
    }

    WEBRTC_TRACE(kTraceStateInfo,  kTraceTransport, -1,
                 "Start UdpSocketManagerWinUWP");
    _thread->Start();
    _thread->SetPriority(rtc::kRealtimePriority);
    return true;
}

bool UdpSocketManagerWinUWPImpl::Stop()
{
    if (_thread == NULL)
    {
        return true;
    }

    WEBRTC_TRACE(kTraceStateInfo,  kTraceTransport, -1,
                 "Stop UdpSocketManagerWinUWP");
    _thread->Stop();
    return true;
}

bool UdpSocketManagerWinUWPImpl::Process()
{
    bool doSelect = false;
    // Timeout = 1 second.
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;

    FD_ZERO(&_readFds);

    UpdateSocketMap();

    int nFd = static_cast<int>(_socketMap.size());
    for (std::map<SOCKET, UdpSocketWinUWP*>::iterator it = _socketMap.begin();
         it != _socketMap.end();
         ++it) {
      doSelect = true;
      FD_SET(it->first, &_readFds);
    }



    int num = 0;
    if (doSelect)
    {
        num = select(nFd, &_readFds, NULL, NULL, &timeout);

        if (num == SOCKET_ERROR)
        {
            // Timeout = 10 ms.
            SleepMs(10);
            return true;
        }
    }else
    {
        // Timeout = 10 ms.
        SleepMs(10);
        return true;
    }

    for (std::map<SOCKET, UdpSocketWinUWP*>::iterator it = _socketMap.begin();
         it != _socketMap.end();
         ++it) {
      if (FD_ISSET(it->first, &_readFds)) {
        it->second->HasIncoming();
        --num;
      }
    }

    return true;
}

bool UdpSocketManagerWinUWPImpl::Run(void* obj)
{
    UdpSocketManagerWinUWPImpl* mgr =
        static_cast<UdpSocketManagerWinUWPImpl*>(obj);
    return mgr->Process();
}

bool UdpSocketManagerWinUWPImpl::AddSocket(UdpSocketWrapper* s)
{

    UdpSocketWinUWP* sl = static_cast<UdpSocketWinUWP*>(s);
    if (sl->GetFd() == INVALID_SOCKET || _socketMap.size() >= FD_SETSIZE)
    {
        return false;
    }
    _critSectList->Enter();
    _addList.push_back(s);
    _critSectList->Leave();
    return true;
}

bool UdpSocketManagerWinUWPImpl::RemoveSocket(UdpSocketWrapper* s)
{
    // Put in remove list if this is the correct UdpSocketManagerWinUWPImpl.
    _critSectList->Enter();

    // If the socket is in the add list it's safe to remove and delete it.
    for (SocketList::iterator iter = _addList.begin();
         iter != _addList.end(); ++iter) {
        UdpSocketWinUWP* addSocket = static_cast<UdpSocketWinUWP*>(*iter);
        unsigned int addFD = addSocket->GetFd();
        unsigned int removeFD = static_cast<UdpSocketWinUWP*>(s)->GetFd();
        if(removeFD == addFD)
        {
            _removeList.push_back(removeFD);
            _critSectList->Leave();
            return true;
        }
    }

    // Checking the socket map is safe since all Erase and Insert calls to this
    // map are also protected by _critSectList.
    if (_socketMap.find(static_cast<UdpSocketWinUWP*>(s)->GetFd()) !=
        _socketMap.end()) {
      _removeList.push_back(static_cast<UdpSocketWinUWP*>(s)->GetFd());
      _critSectList->Leave();
      return true;
    }
    _critSectList->Leave();
    return false;
}

// declaration of 'iter' hides previous local declaration
#pragma warning (disable : 4456)
void UdpSocketManagerWinUWPImpl::UpdateSocketMap()
{
    // Remove items in remove list.
    _critSectList->Enter();
    for (FdList::iterator iter = _removeList.begin();
         iter != _removeList.end(); ++iter) {
        UdpSocketWinUWP* deleteSocket = NULL;
        SOCKET removeFD = *iter;

        // If the socket is in the add list it hasn't been added to the socket
        // map yet. Just remove the socket from the add list.
        for (SocketList::iterator iter = _addList.begin();
             iter != _addList.end(); ++iter) {
            UdpSocketWinUWP* addSocket = static_cast<UdpSocketWinUWP*>(*iter);
            SOCKET addFD = addSocket->GetFd();
            if(removeFD == addFD)
            {
                deleteSocket = addSocket;
                _addList.erase(iter);
                break;
            }
        }

        // Find and remove socket from _socketMap.
        std::map<SOCKET, UdpSocketWinUWP*>::iterator it =
            _socketMap.find(removeFD);
        if(it != _socketMap.end())
        {
          deleteSocket = it->second;
          _socketMap.erase(it);
        }
        if(deleteSocket)
        {
            deleteSocket->ReadyForDeletion();
            delete deleteSocket;
        }
    }
    _removeList.clear();

    // Add sockets from add list.
    for (SocketList::iterator iter = _addList.begin();
         iter != _addList.end(); ++iter) {
        UdpSocketWinUWP* s = static_cast<UdpSocketWinUWP*>(*iter);
        if(s) {
          _socketMap[s->GetFd()] = s;
        }
    }
    _addList.clear();
    _critSectList->Leave();
}

}  // namespace test
}  // namespace webrtc
