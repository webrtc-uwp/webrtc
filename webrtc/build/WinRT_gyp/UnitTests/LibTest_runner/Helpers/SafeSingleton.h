
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

//===========================================================================
//! \file SafeSingleton.h
//!
//! file contains implementation of CSafeSingleton
//!
//! 2014/02/13: created
//============================================================================

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_HELPERS_SAFESINGLETON_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_HELPERS_SAFESINGLETON_H_

#include <assert.h>

#include "webrtc/base/criticalsection.h"

namespace LibTest_runner {

//! declares the function prototype for callback of atexit CRT function
typedef void (*PFnAtExit)();


//=============================================================================
//! \brief This is a creation policy class of CSafeSingletonT.
//! It allocates instance by new expression and calls default ctor().
//!
//=============================================================================
template <class T>
struct CStonCreateUsingNewT {
  //! creates new instance
  static T* Create() {
    return new T();
  }

  //! creates new instance
  static void Destroy(T* p) {
    delete p;
  }
};

//=============================================================================
//! \brief This is a lifetime policy of class for CSafeSingletonT.
//! It registers CSafeSingletonT::DectroySingleton function by atexit(), so it
//! is called when program exits.
//=============================================================================
template <class T>
struct CStonDefaultLifetimeT {
  //! Schedules destruction on exit
  static void ScheduleDestruction(T*, PFnAtExit pFun) {
    std::atexit(pFun);
  }

  //! dead reference handler
  static void OnDeadReference() throw() {
  }
};

//=============================================================================
//! \brief Singleton class
//!  This class must be used instead static variables inside
//!  (static) member functions. It ensures thread safe and only
//!  one initialization of the class instance wrapped by singleton.
//!  The construction/destruction and lifetime can be controled by policy.
//!  Here are provided base policies, but user can create custom policy
//!  to create (provide storage) and initialize wrapped instance.
//!
//! @param T - type which is wraped by singleton
//! @param CreationPolicy - this class defines creation policy. See
//!               CStonCreateUsingNewT for sample which
//!               interface it should have.
//! @param LifetimePolicy - this class defines lifetime policy. It defines
//!               the end of the lifetime of the singleton.
//=============================================================================
template <class T,
class CreationPolicy = CStonCreateUsingNewT<T>,
class LifetimePolicy = CStonDefaultLifetimeT<T> >
class CSafeSingletonT {
 public:
  CSafeSingletonT() {}

  //===========================================================================
  //! \brief Returns reference to instance of the encapsulated singleton.
  //! In case singleton is not created it creates singleton.
  //! @return reference to single instance
  //===========================================================================
  static T& Instance() {
    if (!InternalInstance()) {
      CreateSingleton();
    }
    return *InternalInstance();
  }

 protected:
  //! Internal instance
  static T*& InternalInstance() {
    static T*     s_pInstance;
    return s_pInstance;
  }

  //! indicates that instance destroyed
  static bool& Destroyed() {
    static bool stDestroyed = false;
    return stDestroyed;
  }
  

  static rtc::CriticalSection& SingletonLock() {
    static rtc::CriticalSection singleTonMutex;
    return singleTonMutex;
  }



  //===========================================================================
  //! \brief creates singleton through create policy.
  //! It registers DestroySingleton into lifetime policy. Creation is guarded
  //!                  by mutex so it is thread safe.
  //! TODO: Mutex not implemented so not thread safe !!!!!!
  //! @return
  //===========================================================================
  static void CreateSingleton() {

    rtc::CritScope cs(&SingletonLock());

    if (!InternalInstance()) {  // use double-check pattern
      if (Destroyed()) {
        LifetimePolicy::OnDeadReference();
        Destroyed() = false;
      }
      InternalInstance() = CreationPolicy::Create();
      LifetimePolicy::ScheduleDestruction(InternalInstance(),
                                            &DestroySingleton);
    }
  }

  //===========================================================================
  //! \brief This static function is registered into lifetime policy.
  //! It could be called when lifetime of the singleton is finished.
  //!
  //! @return
  //===========================================================================
  static void DestroySingleton() {

    rtc::CritScope cs(&SingletonLock());
    assert(!Destroyed());
    CreationPolicy::Destroy(InternalInstance());
    InternalInstance() = NULL;
    Destroyed() = true;
  }
};
}  // namespace LibTest_runner

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_HELPERS_SAFESINGLETON_H_
