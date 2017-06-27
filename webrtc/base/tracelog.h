#ifndef WEBRTC_BASE_TRACELOG_H_
#define WEBRTC_BASE_TRACELOG_H_

#include <string>
#include <sstream>
#include <vector>
#include <memory>

#include "webrtc/base/sigslot.h"
#include "webrtc/base/criticalsection.h"

namespace rtc {
class AsyncSocket;
class Thread;
class PlatformThread;

// Aggregates traces. Allows to save traces in local file
// and send them to remote host. To start aggregating traces
// call StartTracing method. Before saving the data locally or
// remotely make sure you have called StopTracing method.
class TraceLog : public sigslot::has_slots<sigslot::multi_threaded_local> {
 public:
  TraceLog();
  virtual ~TraceLog();

  void Add(char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned long long id,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    unsigned char flags);

  void StartTracing();
  void StopTracing();
  bool IsTracing();
  //enable internal storage for trace
  void EnableTraceInternalStorage();

  bool Save(const std::string& file_name);
  bool Save(const std::string& addr, int port);

#ifdef WINUWP
  //get the size of current trace data in the memory
  int64 CurrentTraceMemUsage();
#endif /* WINUWP */

 private:
  static bool processMessages(void* args);

 private:
  void OnCloseEvent(AsyncSocket* socket, int err);
  void OnWriteEvent(AsyncSocket* socket);
  //Returns true if error is critical
  bool HandleWriteError(AsyncSocket* socket);

  // Save traces stored in-memory to traces storage file
  void SaveTraceChunk();

  // Remove traces storage file
  void CleanTracesStorage();

  // Loads first chunk of data from the traces storage file
  // Returns false when loading of the first chunk fails
  bool LoadFirstTraceChunk();

  // Tries to load next chunk of data from the traces storage file
  // Returns false when traces file not accessible or no more data to load
  // from file.
  bool LoadNextTraceChunk();

 private:
  bool is_tracing_;
  unsigned int offset_;

  // Enable to store traces on persistent storage in case in-memory traces
  // exceed in-memory limit, configurable using traces_memory_limit_.
  bool traces_storage_enabled_;

  // Maximum size, in bytes, of stored traces in processes volatile memory.
  unsigned int traces_memory_limit_;

  // Traces persistent storage. The file is created if in-memory traces exceed
  // traces_memory_limit_ bytes, and removed when the tracing is stopped.
  std::string traces_storage_file_;

  // Size, in bytes, of the current chunk to be sent to tracing server.
  unsigned int send_chunk_size_;

  // Offset of the next chunk in the traces storage file
  unsigned int send_chunk_offset_;

  // Maximum size, in bytes, of the chunk to be loaded and sent to tracing
  // server. Increasing send_max_chunk_size_ can result in temporary higher memory
  // usage.
  unsigned int send_max_chunk_size_;

  // Size, in bytes, of the traces stored on persistent storage.
  unsigned int stored_traces_size_;

  // If not zero, indicates the number of bytes sent to traces server in
  // current transfer session.
  // If zero, no transfer is in progress.
  unsigned int sent_bytes_;

  // Current chunk to be sent, loaded from persistent storage
  std::vector<char> send_chunk_buffer_;

  //Maximum size of the block passed to socket at once, in bytes.
  //0 means max block size is not limited.
  unsigned int send_max_block_size_; 
  std::ostringstream oss_;
  CriticalSection critical_section_;
  Thread* thread_;
  std::unique_ptr<rtc::PlatformThread> tw_;
};

}  // namespace rtc


#endif  //  WEBRTC_BASE_TRACELOG_H_
