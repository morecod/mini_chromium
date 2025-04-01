// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crnet/base/file_stream.h"

#include <utility>

#include "crbase/logging.h"
#include "crnet/base/file_stream_context.h"
#include "crnet/base/net_errors.h"

namespace crnet {

FileStream::FileStream(const 
    crbase::scoped_refptr<crbase::TaskRunner>& task_runner)
        : context_(new Context(task_runner)) {
}

FileStream::FileStream(
    crbase::File file,
    const crbase::scoped_refptr<crbase::TaskRunner>& task_runner)
        : context_(new Context(std::move(file), task_runner)) {}

FileStream::~FileStream() {
  context_.release()->Orphan();
}

int FileStream::Open(const crbase::FilePath& path, int open_flags,
                     CompletionOnceCallback callback) {
  if (IsOpen()) {
    CR_DLOG(FATAL) << "File is already open!";
    return ERR_UNEXPECTED;
  }

  CR_DCHECK(open_flags & crbase::File::FLAG_ASYNC);
  context_->Open(path, open_flags, std::move(callback));
  return ERR_IO_PENDING;
}

int FileStream::Close(CompletionOnceCallback callback) {
  context_->Close(std::move(callback));
  return ERR_IO_PENDING;
}

bool FileStream::IsOpen() const {
  return context_->IsOpen();
}

int FileStream::Seek(int64_t offset, 
                     Int64CompletionOnceCallback callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->Seek(offset, std::move(callback));
  return ERR_IO_PENDING;
}

int FileStream::Read(IOBuffer* buf,
                     int buf_len,
                     CompletionOnceCallback callback) {
  // TODO(rvargas): Remove ScopedTracker below once crbug.com/475751 is fixed.
  ///tracked_objects::ScopedTracker tracking_profile(
  ///    FROM_HERE_WITH_EXPLICIT_FUNCTION("475751 FileStream::Read"));

  if (!IsOpen())
    return ERR_UNEXPECTED;

  // read(..., 0) will return 0, which indicates end-of-file.
  CR_DCHECK_GT(buf_len, 0);

  return context_->Read(buf, buf_len, std::move(callback));
}

int FileStream::Write(IOBuffer* buf,
                      int buf_len,
                      CompletionOnceCallback callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  CR_DCHECK_GE(buf_len, 0);
  return context_->Write(buf, buf_len, std::move(callback));
}

int FileStream::Flush(CompletionOnceCallback callback) {
  if (!IsOpen())
    return ERR_UNEXPECTED;

  context_->Flush(std::move(callback));
  return ERR_IO_PENDING;
}

}  // namespace crnet
