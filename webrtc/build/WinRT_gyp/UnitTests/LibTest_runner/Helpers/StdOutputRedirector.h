
// Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_HELPERS_STDOUTPUTREDIRECTOR_H_
#define WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_HELPERS_STDOUTPUTREDIRECTOR_H_

#include <string>

namespace LibTest_runner {
//=============================================================================
//         class: CStdOutputRedirector
//   Description: Class provides redirection of standard output device to
//                std::string or std::wstring
//      Argument: wideString - if true std::wstring is used, std::string
//                otherwise
// History:
// 2015/03/06 TP: created
//=============================================================================

template<bool wideString>
class CStdOutputRedirector {
 private:
  //! buffer for string version
  template<bool wideString> struct BufferT {
    enum IsWideChar { result = false };
    typedef std::string OutputType_t;
    OutputType_t& buffer;
    BufferT(OutputType_t& output)
      : buffer(output) {}
  };

  // buffer for wstring version -> requires conversion
  template<> struct BufferT < true > {
    enum IsWideChar { result = true };
    typedef std::wstring OutputType_t;
    OutputType_t& output;
    std::string buffer;
    BufferT(OutputType_t& output)
      : output(output) {
      buffer.resize(output.size());
    }

    ~BufferT() {
      // convert output to wchar_t
      int nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0,
            const_cast<char*>(buffer.c_str()), -1, NULL, 0);
      if (nRequiredSize >= 0) {
        output.resize(nRequiredSize - 1);  // without ending NULL
        nRequiredSize = ::MultiByteToWideChar(CP_ACP, 0,
          const_cast<char*>(buffer.c_str()), nRequiredSize - 1,
          const_cast<wchar_t*>(output.c_str()), nRequiredSize);
      }
    }
  };
  // buffer type
  typedef typename BufferT<wideString>  Buffer_t;

 public:
  enum { IsWideChar = Buffer_t::result };
  // output type
  typedef typename Buffer_t::OutputType_t OutputType_t;

 private:
  // hide ctor
  CStdOutputRedirector(const CStdOutputRedirector&);
  // buffer for storing std output
  Buffer_t m_buffer;

 public:
  CStdOutputRedirector(OutputType_t& output)
    : m_buffer(output) {
    setvbuf(stdout, const_cast<char*>(m_buffer.buffer.c_str()), _IOFBF,
                                      m_buffer.buffer.size());
  }

  ~CStdOutputRedirector() {
    // reset std output buffer
    setvbuf(stdout, NULL, _IONBF, 0);
  }
};
}  // namespace LibTest_runner

#endif  // WEBRTC_BUILD_WINRT_GYP_UNITTESTS_LIBTEST_RUNNER_HELPERS_STDOUTPUTREDIRECTOR_H_
