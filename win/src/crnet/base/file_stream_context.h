// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines FileStream::Context class.
// The general design of FileStream is as follows: file_stream.h defines
// FileStream class which basically is just an "wrapper" not containing any
// specific implementation details. It re-routes all its method calls to
// the instance of FileStream::Context (FileStream holds a scoped_ptr to
// FileStream::Context instance). Context was extracted into a different class
// to be able to do and finish all async operations even when FileStream
// instance is deleted. So FileStream's destructor can schedule file
// closing to be done by Context in WorkerPool (or the TaskRunner passed to
// constructor) and then just return (releasing Context pointer from
// scoped_ptr) without waiting for actual closing to complete.
// Implementation of FileStream::Context is divided in two parts: some methods
// and members are platform-independent and some depend on the platform. This
// header file contains the complete definition of Context class including all
// platform-dependent parts (because of that it has a lot of #if-#else
// branching). Implementations of all platform-independent methods are
// located in file_stream_context.cc, and all platform-dependent methods are
// in file_stream_context_{win,posix}.cc. This separation provides better
// readability of Context's code. And we tried to make as much Context code
// platform-independent as possible. So file_stream_context_{win,posix}.cc are
// much smaller than file_stream_context.cc now.

#ifndef MINI_CHROMIUM_SRC_CRNET_BASE_FILE_STREAM_CONTEXT_H_
#define MINI_CHROMIUM_SRC_CRNET_BASE_FILE_STREAM_CONTEXT_H_

#include <stdint.h>

#include "crbase/files/file_path.h"
#include "crbase/files/file.h"
#include "crbase/macros.h"
#include "crbase/memory/weak_ptr.h"
#include "crbase/message_loop/message_loop.h"
#include "crbase/threading/single_thread_task_runner.h"
#include "crbase/threading/task_runner.h"
#include "crnet/base/completion_once_callback.h"
#include "crnet/base/file_stream.h"

#if defined(MINI_CHROMIUM_OS_POSIX)
#include <errno.h>
#endif

namespace crnet {

class IOBuffer;

#if defined(MINI_CHROMIUM_OS_WIN)
class FileStream::Context : public cr::MessageLoopForIO::IOHandler {
#elif defined(MINI_CHROMIUM_OS_POSIX)
class FileStream::Context {
#endif
 public:
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  ////////////////////////////////////////////////////////////////////////////
  // Platform-dependent methods implemented in
  // file_stream_context_{win,posix}.cc.
  ////////////////////////////////////////////////////////////////////////////

  explicit Context(
      const cr::scoped_refptr<cr::TaskRunner>& task_runner);
  Context(
      cr::File file, 
      const cr::scoped_refptr<cr::TaskRunner>& task_runner);

#if defined(MINI_CHROMIUM_OS_WIN)
  ~Context() override;
#elif defined(MINI_CHROMIUM_OS_POSIX)
  ~Context();
#endif

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback);

  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback);

  bool async_in_progress() const { return async_in_progress_; }

  ////////////////////////////////////////////////////////////////////////////
  // Platform-independent methods implemented in file_stream_context.cc.
  ////////////////////////////////////////////////////////////////////////////

  // Destroys the context. It can be deleted in the method or deletion can be
  // deferred if some asynchronous operation is now in progress or if file is
  // not closed yet.
  void Orphan();

  void Open(const cr::FilePath& path,
            int open_flags,
            CompletionOnceCallback callback);

  void Close(CompletionOnceCallback callback);

  // Seeks |offset| bytes from the start of the file.
  void Seek(int64_t offset, Int64CompletionOnceCallback callback);

  void Flush(CompletionOnceCallback callback);

  bool IsOpen() const;

 private:
  struct IOResult {
    IOResult();
    IOResult(int64_t result, cr_logging::SystemErrorCode os_error);
    static IOResult FromOSError(cr_logging::SystemErrorCode os_error);

    int64_t result;
    cr_logging::SystemErrorCode os_error;  // Set only when result < 0.
  };

  struct OpenResult {
   public:
    OpenResult(const OpenResult&) = delete;
    OpenResult& operator=(const OpenResult&) = delete;

    OpenResult();
    OpenResult(cr::File file, IOResult error_code);
    OpenResult(OpenResult&& other);
    OpenResult& operator=(OpenResult&& other);

    cr::File file;
    IOResult error_code;

   private:
    ///DISALLOW_COPY_AND_ASSIGN(OpenResult);
  };

  // TODO(xunjieli): Remove after crbug.com/487732 is fixed.
  enum LastOperation {
    // FileStream has a pending Open().
    OPEN,
    // FileStream has a pending Write().
    WRITE,
    // FileStream has a pending Read().
    READ,
    // FileStream has a pending Seek().
    SEEK,
    // FileStream has a pending Flush().
    FLUSH,
    // FileStream has a pending Close().
    CLOSE,
    // FileStream doesn't have any pending operation.
    NONE
  };

  // TODO(xunjieli): Remove after crbug.com/487732 is fixed.
  void CheckNoAsyncInProgress() const;

  ////////////////////////////////////////////////////////////////////////////
  // Platform-independent methods implemented in file_stream_context.cc.
  ////////////////////////////////////////////////////////////////////////////

  OpenResult OpenFileImpl(const cr::FilePath& path, int open_flags);

  IOResult CloseFileImpl();

  IOResult FlushFileImpl();

  void OnOpenCompleted(CompletionOnceCallback callback,
                       OpenResult open_result);

  void CloseAndDelete();

  Int64CompletionOnceCallback IntToInt64(
      CompletionOnceCallback callback);

  // Called when Open() or Seek() completes. |result| contains the result or a
  // network error code.
  void OnAsyncCompleted(Int64CompletionOnceCallback callback,
                        const IOResult& result);

  ////////////////////////////////////////////////////////////////////////////
  // Platform-dependent methods implemented in
  // file_stream_context_{win,posix}.cc.
  ////////////////////////////////////////////////////////////////////////////

  // Adjusts the position from where the data is read.
  IOResult SeekFileImpl(int64_t offset);

  void OnFileOpened();

#if defined(MINI_CHROMIUM_OS_WIN)
  void IOCompletionIsPending(CompletionOnceCallback callback, 
                             IOBuffer* buf);

  // Implementation of MessageLoopForIO::IOHandler.
  void OnIOCompleted(cr::MessageLoopForIO::IOContext* context,
                     DWORD bytes_read,
                     DWORD error) override;

  // Invokes the user callback.
  void InvokeUserCallback();

  // Deletes an orphaned context.
  void DeleteOrphanedContext();

  // The ReadFile call on Windows can execute synchonously at times.
  // http://support.microsoft.com/kb/156932. This ends up blocking the calling
  // thread which is undesirable. To avoid this we execute the ReadFile call
  // on a worker thread.
  // The |context| parameter is a pointer to the current Context instance. It
  // is safe to pass this as is to the pool as the Context instance should
  // remain valid until the pending Read operation completes.
  // The |file| parameter is the handle to the file being read.
  // The |buf| parameter is the buffer where we want the ReadFile to read the
  // data into.
  // The |buf_len| parameter contains the number of bytes to be read.
  // The |overlapped| parameter is a pointer to the OVERLAPPED structure being
  // used.
  // The |origin_thread_task_runner| is a task runner instance used to post
  // tasks back to the originating thread.
  static void ReadAsync(
      FileStream::Context* context,
      HANDLE file,
      cr::scoped_refptr<IOBuffer> buf,
      int buf_len,
      OVERLAPPED* overlapped,
      cr::scoped_refptr<cr::SingleThreadTaskRunner> 
          origin_thread_task_runner);

  // This callback executes on the main calling thread. It informs the caller
  // about the result of the ReadFile call.
  // The |read_file_ret| parameter contains the return value of the ReadFile
  // call.
  // The |bytes_read| contains the number of bytes read from the file, if
  // ReadFile succeeds.
  // The |os_error| parameter contains the value of the last error returned by
  // the ReadFile API.
  void ReadAsyncResult(BOOL read_file_ret, DWORD bytes_read, DWORD os_error);

#elif defined(MINI_CHROMIUM_OS_POSIX)
  // ReadFileImpl() is a simple wrapper around read() that handles EINTR
  // signals and calls RecordAndMapError() to map errno to net error codes.
  IOResult ReadFileImpl(cr::scoped_refptr<IOBuffer> buf, int buf_len);

  // WriteFileImpl() is a simple wrapper around write() that handles EINTR
  // signals and calls MapSystemError() to map errno to net error codes.
  // It tries to write to completion.
  IOResult WriteFileImpl(cr::scoped_refptr<IOBuffer> buf, int buf_len);
#endif

  cr::File file_;
  bool async_in_progress_;

  // TODO(xunjieli): Remove after crbug.com/487732 is fixed.
  LastOperation last_operation_;

  bool orphaned_;
  cr::scoped_refptr<cr::TaskRunner> task_runner_;

#if defined(MINI_CHROMIUM_OS_WIN)
  cr::MessageLoopForIO::IOContext io_context_;
  CompletionOnceCallback callback_;
  cr::scoped_refptr<IOBuffer> in_flight_buf_;
  // This flag is set to true when we receive a Read request which is queued to
  // the thread pool.
  bool async_read_initiated_;
  // This flag is set to true when we receive a notification ReadAsyncResult()
  // on the calling thread which indicates that the asynchronous Read
  // operation is complete.
  bool async_read_completed_;
  // This flag is set to true when we receive an IO completion notification for
  // an asynchonously initiated Read operaton. OnIOComplete().
  bool io_complete_for_read_received_;
  // Tracks the result of the IO completion operation. Set in OnIOComplete.
  int result_;
#endif

  ///DISALLOW_COPY_AND_ASSIGN(Context);
};

}  // namespace crnet

#endif  // MINI_CHROMIUM_SRC_CRNET_BASE_FILE_STREAM_CONTEXT_H_
