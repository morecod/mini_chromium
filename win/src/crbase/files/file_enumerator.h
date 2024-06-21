// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_FILES_FILE_ENUMERATOR_H_
#define MINI_CHROMIUM_SRC_CRBASE_FILES_FILE_ENUMERATOR_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#include <stack>
#include <vector>

#include "crbase/base_export.h"
#include "crbase/files/file_path.h"
#include "crbase/macros.h"
#include "crbase/time/time.h"


namespace crbase {

// A class for enumerating the files in a provided path. The order of the
// results is not guaranteed.
//
// This is blocking. Do not use on critical threads.
//
// Example:
//
//   crbase::FileEnumerator enum(my_dir, false, crbase::FileEnumerator::FILES,
//                               CR_FILE_PATH_LITERAL("*.txt"));
//   for (crbase::FilePath name = enum.Next(); !name.empty();
  //      name = enum.Next())
//     ...
class CRBASE_EXPORT FileEnumerator {
 public:
  // Note: copy & assign supported.
  class CRBASE_EXPORT FileInfo {
   public:
    FileInfo();
    ~FileInfo();

    bool IsDirectory() const;

    // The name of the file. This will not include any path information. This
    // is in constrast to the value returned by FileEnumerator.Next() which
    // includes the |root_path| passed into the FileEnumerator constructor.
    FilePath GetName() const;

    int64_t GetSize() const;
    Time GetLastModifiedTime() const;

    // Note that the cAlternateFileName (used to hold the "short" 8.3 name)
    // of the WIN32_FIND_DATA will be empty. Since we don't use short file
    // names, we tell Windows to omit it which speeds up the query slightly.
    const WIN32_FIND_DATAW& find_data() const { return find_data_; }

   private:
    friend class FileEnumerator;

    WIN32_FIND_DATAW find_data_;
  };

  enum FileType {
    FILES                 = 1 << 0,
    DIRECTORIES           = 1 << 1,
    INCLUDE_DOT_DOT       = 1 << 2,
  };

  FileEnumerator(const FileEnumerator&) = delete;
  FileEnumerator& operator=(const FileEnumerator&) = delete;

  // |root_path| is the starting directory to search for. It may or may not end
  // in a slash.
  //
  // If |recursive| is true, this will enumerate all matches in any
  // subdirectories matched as well. It does a breadth-first search, so all
  // files in one directory will be returned before any files in a
  // subdirectory.
  //
  // |file_type|, a bit mask of FileType, specifies whether the enumerator
  // should match files, directories, or both.
  //
  // |pattern| is an optional pattern for which files to match. This
  // works like shell globbing. For example, "*.txt" or "Foo???.doc".
  // However, be careful in specifying patterns that aren't cross platform
  // since the underlying code uses OS-specific matching routines.  In general,
  // Windows matching is less featureful than others, so test there first.
  // If unspecified, this will match all files.
  // NOTE: the pattern only matches the contents of root_path, not files in
  // recursive subdirectories.
  // TODO(erikkay): Fix the pattern matching to work at all levels.
  FileEnumerator(const FilePath& root_path,
                 bool recursive,
                 int file_type);
  FileEnumerator(const FilePath& root_path,
                 bool recursive,
                 int file_type,
                 const FilePath::StringType& pattern);
  ~FileEnumerator();

  // Returns the next file or an empty string if there are no more results.
  //
  // The returned path will incorporate the |root_path| passed in the
  // constructor: "<root_path>/file_name.txt". If the |root_path| is absolute,
  // then so will be the result of Next().
  FilePath Next();

  // Write the file info into |info|.
  FileInfo GetInfo() const;

 private:
  // Returns true if the given path should be skipped in enumeration.
  bool ShouldSkip(const FilePath& path);

  // True when find_data_ is valid.
  bool has_find_data_;
  WIN32_FIND_DATAW find_data_;
  HANDLE find_handle_;

  FilePath root_path_;
  bool recursive_;
  int file_type_;
  FilePath::StringType pattern_;  // Empty when we want to find everything.

  // A stack that keeps track of which subdirectories we still need to
  // enumerate in the breadth-first search.
  std::stack<FilePath> pending_paths_;
};

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_FILES_FILE_ENUMERATOR_H_
