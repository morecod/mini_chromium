// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/base/file_stream_context.h"

#include <utility>

#include "crbase/debug/alias.h"
#include "crbase/files/file_path.h"
///#include "crbase/tracing/location.h"
///#include "crbase/profiler/scoped_tracker.h"
#include "crbase/threading/task_runner.h"
#include "crbase/threading/task_runner_util.h"
#include "crbase/threading/thread_restrictions.h"
#include "crbase/values.h"
#include "crnet/base/net_errors.h"

namespace crnet {

namespace {

void CallInt64ToInt(CompletionOnceCallback callback, int64_t result) {
  std::move(callback).Run(static_cast<int>(result));
}

}  // namespace

FileStream::Context::IOResult::IOResult()
    : result(OK),
      os_error(0) {
}

FileStream::Context::IOResult::IOResult(
    int64_t result,
    cr_logging::SystemErrorCode os_error)
        : result(result), os_error(os_error) {
}

// static
FileStream::Context::IOResult FileStream::Context::IOResult::FromOSError(
    cr_logging::SystemErrorCode os_error) {
  return IOResult(MapSystemError(os_error), os_error);
}

// ---------------------------------------------------------------------

FileStream::Context::OpenResult::OpenResult() {
}

FileStream::Context::OpenResult::OpenResult(cr::File file,
                                            IOResult error_code)
    : file(std::move(file)), error_code(error_code) {}

FileStream::Context::OpenResult::OpenResult(OpenResult&& other)
    : file(std::move(other.file)), error_code(other.error_code) {}

FileStream::Context::OpenResult& FileStream::Context::OpenResult::operator=(
    OpenResult&& other) {
  file = std::move(other.file);
  error_code = other.error_code;
  return *this;
}

// ---------------------------------------------------------------------

void FileStream::Context::Orphan() {
  CR_DCHECK(!orphaned_);

  orphaned_ = true;

  if (!async_in_progress_) {
    CloseAndDelete();
  } else if (file_.IsValid()) {
#if defined(MINI_CHROMIUM_OS_WIN)
    CancelIo(file_.GetPlatformFile());
#endif
  }
}

void FileStream::Context::Open(const cr::FilePath& path,
                               int open_flags,
                               CompletionOnceCallback callback) {
  CheckNoAsyncInProgress();

  bool posted = cr::PostTaskAndReplyWithResult(
      task_runner_.get(),
      CR_FROM_HERE,
      cr::BindOnce(
          &Context::OpenFileImpl, cr::Unretained(this), 
          path, open_flags),
      cr::BindOnce(
          &Context::OnOpenCompleted, cr::Unretained(this), 
          std::move(callback)));
  CR_DCHECK(posted);

  last_operation_ = OPEN;
  async_in_progress_ = true;
}

void FileStream::Context::Close(CompletionOnceCallback callback) {
  CheckNoAsyncInProgress();
  bool posted = cr::PostTaskAndReplyWithResult(
      task_runner_.get(),
      CR_FROM_HERE,
      cr::BindOnce(&Context::CloseFileImpl, cr::Unretained(this)),
      cr::BindOnce(&Context::OnAsyncCompleted,
                       cr::Unretained(this),
                       IntToInt64(std::move(callback))));
  CR_DCHECK(posted);

  last_operation_ = CLOSE;
  async_in_progress_ = true;
}

void FileStream::Context::Seek(int64_t offset,
                               Int64CompletionOnceCallback callback) {
  CheckNoAsyncInProgress();

  bool posted = cr::PostTaskAndReplyWithResult(
      task_runner_.get(), CR_FROM_HERE,
      cr::BindOnce(&Context::SeekFileImpl, cr::Unretained(this), 
                       offset),
      cr::BindOnce(&Context::OnAsyncCompleted, cr::Unretained(this),
                       std::move(callback)));
  CR_DCHECK(posted);

  last_operation_ = SEEK;
  async_in_progress_ = true;
}

void FileStream::Context::Flush(CompletionOnceCallback callback) {
  CheckNoAsyncInProgress();

  bool posted = cr::PostTaskAndReplyWithResult(
      task_runner_.get(),
      CR_FROM_HERE,
      cr::BindOnce(&Context::FlushFileImpl, cr::Unretained(this)),
      cr::BindOnce(&Context::OnAsyncCompleted,
                 cr::Unretained(this),
                 IntToInt64(std::move(callback))));
  CR_DCHECK(posted);

  last_operation_ = FLUSH;
  async_in_progress_ = true;
}

bool FileStream::Context::IsOpen() const {
  return file_.IsValid();
}

void FileStream::Context::CheckNoAsyncInProgress() const {
  if (!async_in_progress_)
    return;
  LastOperation state = last_operation_;
  cr::debug::Alias(&state);
  // TODO(xunjieli): Once https://crbug.com/487732 is fixed, use
  // DCHECK(!async_in_progress_) directly at call places.
  CR_CHECK(!async_in_progress_);
}

FileStream::Context::OpenResult FileStream::Context::OpenFileImpl(
    const cr::FilePath& path, int open_flags) {
#if defined(MINI_CHROMIUM_OS_POSIX)
  // Always use blocking IO.
  open_flags &= ~base::File::FLAG_ASYNC;
#endif
  cr::File file;

    // FileStream::Context actually closes the file asynchronously,
    // independently from FileStream's destructor. It can cause problems for
    // users wanting to delete the file right after FileStream deletion. Thus
    // we are always adding SHARE_DELETE flag to accommodate such use case.
    // TODO(rvargas): This sounds like a bug, as deleting the file would
    // presumably happen on the wrong thread. There should be an async delete.
    open_flags |= cr::File::FLAG_SHARE_DELETE;
    file.Initialize(path, open_flags);
  if (!file.IsValid())
    return OpenResult(cr::File(),
                      IOResult::FromOSError(
                            cr_logging::GetLastSystemErrorCode()));

  return OpenResult(std::move(file), IOResult(OK, 0));
}

FileStream::Context::IOResult FileStream::Context::CloseFileImpl() {
  file_.Close();
  return IOResult(OK, 0);
}

FileStream::Context::IOResult FileStream::Context::FlushFileImpl() {
  if (file_.Flush())
    return IOResult(OK, 0);

  return IOResult::FromOSError(cr_logging::GetLastSystemErrorCode());
}

void FileStream::Context::OnOpenCompleted(CompletionOnceCallback callback,
                                          OpenResult open_result) {
  file_ = std::move(open_result.file);
  if (file_.IsValid() && !orphaned_)
    OnFileOpened();

  OnAsyncCompleted(IntToInt64(std::move(callback)), open_result.error_code);
}

void FileStream::Context::CloseAndDelete() {
  // TODO(ananta)
  // Replace this CHECK with a DCHECK once we figure out the root cause of
  // http://crbug.com/455066
  CheckNoAsyncInProgress();

  if (file_.IsValid()) {
    bool posted = task_runner_.get()->PostTask(
        CR_FROM_HERE, 
        cr::BindOnce(
            cr::IgnoreResult(&Context::CloseFileImpl), 
            cr::Owned(this)));
    CR_DCHECK(posted);
  } else {
    delete this;
  }
}

Int64CompletionOnceCallback FileStream::Context::IntToInt64(
    CompletionOnceCallback callback) {
  return cr::BindOnce(&CallInt64ToInt, std::move(callback));
}

void FileStream::Context::OnAsyncCompleted(
    Int64CompletionOnceCallback callback,
    const IOResult& result) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  ///tracked_objects::ScopedTracker tracking_profile(
  ///    FROM_HERE_WITH_EXPLICIT_FUNCTION(
  ///        "477117 FileStream::Context::OnAsyncCompleted"));
  // Reset this before Run() as Run() may issue a new async operation. Also it
  // should be reset before Close() because it shouldn't run if any async
  // operation is in progress.
  async_in_progress_ = false;
  last_operation_ = NONE;
  if (orphaned_)
    CloseAndDelete();
  else
    std::move(callback).Run(result.result);
}

}  // namespace crnet
