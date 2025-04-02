// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_BASE_PATHS_H_
#define MINI_CHROMIUM_SRC_CRBASE_BASE_PATHS_H_

// This file declares path keys for the crbase module.  These can be used with
// the PathService to access various special directories and files.

#include "crbase/build_config.h"

#if defined(MINI_CHROMIUM_OS_WIN)
#include "crbase/base_paths_win.h"
#elif defined(MINI_CHROMIUM_OS_POSIX)
#include "crbase/base_paths_posix.h"
#endif

namespace cr {

enum BasePathKey {
  PATH_START = 0,

  DIR_CURRENT,       // Current directory.
  DIR_EXE,           // Directory containing FILE_EXE.
  DIR_MODULE,        // Directory containing FILE_MODULE.
  DIR_TEMP,          // Temporary directory.
  DIR_HOME,          // User's root home directory. On Windows this will look
                     // like "C:\Users\you" (or on XP
                     // "C:\Document and Settings\you") which isn't necessarily
                     // a great place to put files.
  FILE_EXE,          // Path and filename of the current executable.
  FILE_MODULE,       // Path and filename of the module containing the code for
                     // the PathService (which could differ from FILE_EXE if the
                     // PathService were compiled into a shared object, for
                     // example).
  DIR_USER_DESKTOP,  // The current user's Desktop.
  PATH_END
};

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_BASE_PATHS_H_