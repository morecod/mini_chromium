// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/base_paths.h"

#include "crbase/files/file_path.h"
#include "crbase/files/file_util.h"
#include "crbase/path_service.h"

namespace cr {

bool PathProvider(int key, FilePath* result) {
  // NOTE: DIR_CURRENT is a special case in PathService::Get

  switch (key) {
    case DIR_EXE:
      PathService::Get(FILE_EXE, result);
      *result = result->DirName();
      return true;
    case DIR_MODULE:
      PathService::Get(FILE_MODULE, result);
      *result = result->DirName();
      return true;
    case DIR_TEMP:
      if (!GetTempDir(result))
        return false;
      return true;
    case DIR_HOME:
      *result = GetHomeDir();
      return true;
    default:
      return false;
  }
}

}  // namespace cr
