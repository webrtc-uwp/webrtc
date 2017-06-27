#include <sstream>
#include <fstream>
#include <cstdio>
#include <iterator>
#include <algorithm>
#include <assert.h>

#include "webrtc/base/tracelog.h"
#include "webrtc/base/asyncsocket.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/physicalsocketserver.h"

namespace rtc {

TraceLog::TraceLog() : is_tracing_(false), offset_(0),
  traces_storage_enabled_(false), traces_memory_limit_(0), send_chunk_size_(0),
  send_chunk_offset_(0), send_max_chunk_size_(0), stored_traces_size_(0),
  sent_bytes_(0), send_max_block_size_(0), tw_() {
  PhysicalSocketServer* pss = new PhysicalSocketServer();
  thread_ = new Thread(pss);
}

void TraceLog::EnableTraceInternalStorage() {
  if (traces_storage_enabled_)
    return;  // already enabled.

#if WINUWP
  Platform::String^ path_w =
    Windows::Storage::ApplicationData::Current->LocalFolder->Path;
  int len8 = WideCharToMultiByte(CP_UTF8, 0, path_w->Data(), path_w->Length(),
    NULL, 0, NULL, NULL);
  std::string path(len8, ' ');
  if (WideCharToMultiByte(CP_UTF8, 0, path_w->Data(), path_w->Length(),
    (LPSTR)path.c_str(), len8, NULL, NULL) == 0) {
    LOG(LS_WARNING) << "Failed to initialize traces storage errCode="
      << GetLastError();
  } else {
    traces_storage_file_ = path + "\\" + "_webrtc_traces.log";
    send_max_chunk_size_ = 1024 * 1024;  // 1mb
    traces_memory_limit_ = 1024 * 1024;  // 1mb
    traces_storage_enabled_ = true;
  }
#endif // WINUWP
}

#ifdef WINUWP
int64 TraceLog::CurrentTraceMemUsage(){
  return this->oss_.tellp();
}
#endif // WINUWP

TraceLog::~TraceLog() {
  if (tw_) {
    tw_->Stop();
    thread_->Stop();
  }
}


void TraceLog::Add(char phase,
  const unsigned char* category_group_enabled,
  const char* name,
  unsigned long long id,
  int num_args,
  const char** arg_names,
  const unsigned char* arg_types,
  const unsigned long long* arg_values,
  unsigned char flags) {
  if (!IsTracing())
    return;

  std::ostringstream t;
  t << "{"
    << "\"name\": \"" << name << "\", "
    << "\"cat\": \"" << category_group_enabled << "\", "
    << "\"ph\": \"" << phase << "\", "
    << "\"ts\": " << rtc::TimeMicros() << ", "
    << "\"pid\": " << 0 << ", "
    << "\"tid\": " << CurrentThreadId() << ", "
    << "\"args\": {";

  webrtc::trace_event_internal::TraceValueUnion tvu;

  for (int i = 0; i < num_args; ++i) {
    t << "\"" << arg_names[i] << "\": ";
    tvu.as_uint = arg_values[i];

    switch (arg_types[i]) {
    case TRACE_VALUE_TYPE_BOOL:
      t << tvu.as_bool;
      break;
    case TRACE_VALUE_TYPE_UINT:
      t << tvu.as_uint;
      break;
    case TRACE_VALUE_TYPE_INT:
      t << tvu.as_int;
      break;
    case TRACE_VALUE_TYPE_DOUBLE:
      t << tvu.as_double;
      break;
    case TRACE_VALUE_TYPE_POINTER:
      t << tvu.as_pointer;
      break;
    case TRACE_VALUE_TYPE_STRING:
    case TRACE_VALUE_TYPE_COPY_STRING:
      t << "\"" << tvu.as_string << "\"";
      break;
    default:
      break;
    }

    if (i < num_args - 1) {
      t << ", ";
    }
  }

  t << "}" << "},";

  CritScope lock(&critical_section_);
  oss_ << t.str();
  if (traces_storage_enabled_ && oss_.tellp() > traces_memory_limit_) {
    SaveTraceChunk();
  }
}

void TraceLog::StartTracing() {
	CritScope lock(&critical_section_);
  if (!is_tracing_) {
    oss_.clear();
    oss_.str("");
    oss_ << "{ \"traceEvents\": [";
    is_tracing_ = true;

    if (traces_storage_enabled_) {
      CleanTracesStorage();
    }
  }
}

void TraceLog::StopTracing() {
   CritScope lock(&critical_section_);
  if (is_tracing_) {
    long pos = oss_.tellp();
    oss_.seekp(pos - 1);
    oss_ << "]}";
    is_tracing_ = false;
  }
}

bool TraceLog::IsTracing() {
  CritScope lock(&critical_section_);
  return is_tracing_;
}

bool TraceLog::Save(const std::string& file_name) {
  bool result = true;
  std::ofstream file;
  file.open(file_name.c_str());
  if (file.is_open()) {
    if (traces_storage_enabled_) {
      // Save stored traces first
      if (LoadFirstTraceChunk()) {
        while (!send_chunk_buffer_.empty()) {
          copy(send_chunk_buffer_.begin(), send_chunk_buffer_.end(),
            std::ostream_iterator<unsigned char>(file));
          if (!LoadNextTraceChunk()) {
            CleanTracesStorage();
            break;
          }
        }
      } else {
        LOG(LS_ERROR) << "Failed load first chunk from traces storage";
        CleanTracesStorage();
      }
    }

    file << oss_.str();
    file.close();
    if (file.bad())
      result = false;
  } else {
    if (traces_storage_enabled_) {
      CleanTracesStorage();
    }
    result = false;
  }
  return result;
}

bool TraceLog::Save(const std::string& addr, int port) {
  if (sent_bytes_) {
    // Sending the data still is in progress.
    return false;
  }

  if (tw_ == NULL) {
    tw_.reset(new PlatformThread(&TraceLog::processMessages, thread_, "TraceLog"));
    tw_->Start();

    LOG(LS_INFO) << "New TraceLog thread created.";
  }
  if (traces_storage_enabled_) {
    if (!LoadFirstTraceChunk()) {
      LOG(LS_ERROR) << "Failed load first chunk from traces storage";
    }
  }

  AsyncSocket* sock =
    thread_->socketserver()->CreateAsyncSocket(AF_INET, SOCK_STREAM);
  sock->SignalWriteEvent.connect(this, &TraceLog::OnWriteEvent);
  sock->SignalCloseEvent.connect(this, &TraceLog::OnCloseEvent);

  SocketAddress server_addr(addr, port);
  sock->Connect(server_addr);

  // Send wake up signal to update the event list to wait
  thread_->socketserver()->WakeUp();
  return true;
}

void TraceLog::OnCloseEvent(AsyncSocket* socket, int err) {
  if (!socket)
    return;

  SocketAddress addr = socket->GetRemoteAddress();
  LOG(LS_ERROR) << "The connection was closed. "
    << "IP: " << addr.HostAsURIString() << ", "
    << "Port: " << addr.port() << ", "
    << "Error: " << err;

  offset_ = 0;
  send_chunk_offset_ = 0;

  thread_->Dispose(socket);
}

void TraceLog::OnWriteEvent(AsyncSocket* socket) {
  if (!socket)
    return;

  if (traces_storage_enabled_) {
    // Send stored traces first
    while (!send_chunk_buffer_.empty()) {
      while (offset_ < send_chunk_size_) {
        int sent_size = socket->Send(
          (const void*)(send_chunk_buffer_.data() + offset_),
          send_max_block_size_ ? std::min(send_max_block_size_,
                                          send_chunk_size_ - offset_)
                               : send_chunk_size_ - offset_);
        if (sent_size == -1) {
          if (!IsBlockingError(socket->GetError())) {
            if (!HandleWriteError(socket)) {
              continue;
            }
            offset_ = 0;
            send_max_block_size_ = 0;
            socket->Close();
          }
          return;
        } else {
          offset_ += (unsigned int)sent_size;
          sent_bytes_ += sent_size;
        }
      }
      if (!LoadNextTraceChunk()) {
        CleanTracesStorage();
        break;
      }
    }
    offset_ = 0;
  }

  const std::string& tmp = oss_.str();
  size_t tmp_size = tmp.size();
  const char* data = tmp.c_str();

  int sent_size = 0;
  while (offset_ < tmp_size) {
    sent_size = socket->Send((const void*) (data + offset_),
      send_max_block_size_ ? std::min<size_t>(send_max_block_size_,
                                              tmp_size - offset_)
                           : tmp_size - offset_);
    if (sent_size == -1) {
      if (!IsBlockingError(socket->GetError())) {
        if (!HandleWriteError(socket)) {
          continue;
        }
        offset_ = 0;
        send_max_block_size_ = 0;
        socket->Close();
      }
      break;
    } else {
      offset_ += sent_size;
      sent_bytes_ += sent_size;
    }
  }

  if (tmp_size == offset_) {
    offset_ = 0;
    sent_bytes_ = 0;
    send_max_block_size_ = 0;
    socket->Close();
  }
}

bool TraceLog::HandleWriteError(AsyncSocket* socket) {
#if defined(WINAPI_FAMILY)
  if (socket->GetError() == ENOBUFS) {
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms740668(v=vs.85).aspx#WSAENOBUFS
    // No buffer space available.
    // An operation on a socket could not be performed because the system
    // lacked sufficient buffer space or because a queue was full.
    // Try passing smaller blocks to socket next time.
    if (send_max_block_size_ == 0) {
      send_max_block_size_ = (unsigned int)oss_.tellp() - offset_;
    }
    send_max_block_size_ /= 2;
    if (send_max_block_size_) {
      LOG(LS_INFO) << "Reduced block size to " << send_max_block_size_;
      return false;
    }
  }
#endif
  return true;
}

void TraceLog::SaveTraceChunk() {
  assert(traces_storage_enabled_);
  std::ofstream file;
  file.open(traces_storage_file_, std::ios_base::app);
  if (file.is_open()) {
    file << oss_.str();
    file.flush();
    file.close();
    LOG(LS_INFO) << "Saved trace chunk having " << oss_.str().size()
                 <<"b to storage";
    oss_.str("");
  }
}

void TraceLog::CleanTracesStorage() {
  assert(traces_storage_enabled_);
  std::remove(traces_storage_file_.c_str());
}

bool TraceLog::LoadFirstTraceChunk() {
  assert(traces_storage_enabled_);
  bool result = false;
  send_chunk_offset_ = 0;
  send_chunk_buffer_.reserve(send_max_chunk_size_);
  std::ifstream in(traces_storage_file_,
    std::ifstream::ate | std::ifstream::binary);
  if (in.is_open()) {
    stored_traces_size_ = in.tellg();
    in.close();
    result = LoadNextTraceChunk();
  } else {
    result = false;
  }
  return result;
}

bool TraceLog::LoadNextTraceChunk() {
  assert(traces_storage_enabled_);
  bool result = false;
  std::ifstream input;
  input.open(traces_storage_file_);
  if (input.is_open()) {
    input.seekg(send_chunk_offset_);
    if (send_chunk_offset_ < stored_traces_size_) {
      int chunkSize = send_chunk_size_ = std::min(
        send_max_chunk_size_,
        stored_traces_size_ - send_chunk_offset_);
      send_chunk_buffer_.resize(chunkSize);
      input.read(send_chunk_buffer_.data(), chunkSize);
      input.close();
      offset_ = 0;
      send_chunk_offset_ += chunkSize;
      result = true;
      LOG(LS_INFO) << "Loaded trace chunk having " << chunkSize
        << "b from storage";
    } else {
      send_chunk_buffer_.clear();
    }
  } else {
    send_chunk_buffer_.clear();
  }
  return result;
}

bool TraceLog::processMessages(void* args) {
  Thread* t = reinterpret_cast<Thread*>(args);
  if (t)
    t->Run();
  return true;
}

}  //  namespace rtc
