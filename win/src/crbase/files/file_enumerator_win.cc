// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/files/file_enumerator.h"

#include <stdint.h>
#include <string.h>

#include "crbase/win/windows_version.h"

namespace cr {

// FileEnumerator::FileInfo ----------------------------------------------------

FileEnumerator::FileInfo::FileInfo() {
  memset(&find_data_, 0, sizeof(find_data_));
}

bool FileEnumerator::FileInfo::IsDirectory() const {
  return (find_data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

FilePath FileEnumerator::FileInfo::GetName() const {
  return FilePath(find_data_.cFileName);
}

int64_t FileEnumerator::FileInfo::GetSize() const {
  ULARGE_INTEGER size;
  size.HighPart = find_data_.nFileSizeHigh;
  size.LowPart = find_data_.nFileSizeLow;
  CR_DCHECK_LE(size.QuadPart,
               static_cast<ULONGLONG>(std::numeric_limits<int64_t>::max()));
  return static_cast<int64_t>(size.QuadPart);
}

cr::Time FileEnumerator::FileInfo::GetLastModifiedTime() const {
  return cr::Time::FromFileTime(find_data_.ftLastWriteTime);
}

// FileEnumerator --------------------------------------------------------------

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               int file_type)
    : has_find_data_(false),
      find_handle_(INVALID_HANDLE_VALUE),
      recursive_(recursive),
      file_type_(file_type) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  CR_DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
  memset(&find_data_, 0, sizeof(find_data_));
  pending_paths_.push(root_path);
}

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               int file_type,
                               const FilePath::StringType& pattern)
    : has_find_data_(false),
      find_handle_(INVALID_HANDLE_VALUE),
      recursive_(recursive),
      file_type_(file_type),
      pattern_(pattern) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  CR_DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
  memset(&find_data_, 0, sizeof(find_data_));
  pending_paths_.push(root_path);
}

FileEnumerator::~FileEnumerator() {
  if (find_handle_ != INVALID_HANDLE_VALUE)
    FindClose(find_handle_);
}

FileEnumerator::FileInfo FileEnumerator::GetInfo() const {
  if (!has_find_data_) {
    return FileInfo();
  }
  FileInfo ret;
  memcpy(&ret.find_data_, &find_data_, sizeof(find_data_));
  return ret;
}

FilePath FileEnumerator::Next() {
  while (has_find_data_ || !pending_paths_.empty()) {
    if (!has_find_data_) {
      // The last find FindFirstFile operation is done, prepare a new one.
      root_path_ = pending_paths_.top();
      pending_paths_.pop();

      // Start a new find operation.
      FilePath src = root_path_;

      if (pattern_.empty())
        src = src.Append(L"*");  // No pattern = match everything.
      else
        src = src.Append(pattern_);

      if (cr::win::GetVersion() >= cr::win::Version::WIN7) {
        // Use a "large fetch" on newer Windows which should speed up large
        // enumerations (we seldom abort in the middle).
        find_handle_ = ::FindFirstFileExW(src.value().c_str(),
                                          FindExInfoBasic,  // Omit short name.
                                          &find_data_,
                                          FindExSearchNameMatch,
                                          NULL,
                                          FIND_FIRST_EX_LARGE_FETCH);
      } else {
        find_handle_ = ::FindFirstFileW(src.value().c_str(), &find_data_);
      }
      has_find_data_ = true;
    } else {
      // Search for the next file/directory.
      if (!::FindNextFileW(find_handle_, &find_data_)) {
        ::FindClose(find_handle_);
        find_handle_ = INVALID_HANDLE_VALUE;
      }
    }

    if (INVALID_HANDLE_VALUE == find_handle_) {
      has_find_data_ = false;

      // This is reached when we have finished a directory and are advancing to
      // the next one in the queue. We applied the pattern (if any) to the files
      // in the root search directory, but for those directories which were
      // matched, we want to enumerate all files inside them. This will happen
      // when the handle is empty.
      pattern_ = FilePath::StringType();

      continue;
    }

    FilePath cur_file(find_data_.cFileName);
    if (ShouldSkip(cur_file))
      continue;

    // Construct the absolute filename.
    cur_file = root_path_.Append(find_data_.cFileName);

    if (find_data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      if (recursive_) {
        // If |cur_file| is a directory, and we are doing recursive searching,
        // add it to pending_paths_ so we scan it after we finish scanning this
        // directory. However, don't do recursion through reparse points or we
        // may end up with an infinite cycle.
        DWORD attributes = GetFileAttributesW(cur_file.value().c_str());
        if (!(attributes & FILE_ATTRIBUTE_REPARSE_POINT))
          pending_paths_.push(cur_file);
      }
      if (file_type_ & FileEnumerator::DIRECTORIES)
        return cur_file;
    } else if (file_type_ & FileEnumerator::FILES) {
      return cur_file;
    }
  }

  return FilePath();
}

}  // namespace cr
