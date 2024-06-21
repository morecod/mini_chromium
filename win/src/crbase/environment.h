// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_ENVIRONMENT_H_
#define MINI_CHROMIUM_SRC_CRBASE_ENVIRONMENT_H_

#include <map>
#include <string>
#include <memory>

#include "crbase/base_export.h"
#include "crbase/strings/string16.h"

namespace crbase {

class CRBASE_EXPORT Environment {
 public:
  virtual ~Environment();

  // Static factory method that returns the implementation that provide the
  // appropriate platform-specific instance.
  static Environment* Create();

  // Gets an environment variable's value and stores it in |result|.
  // Returns false if the key is unset.
  virtual bool GetVar(const char* variable_name, std::string* result) = 0;

  // Syntactic sugar for GetVar(variable_name, NULL);
  virtual bool HasVar(const char* variable_name);

  // Returns true on success, otherwise returns false.
  virtual bool SetVar(const char* variable_name,
                      const std::string& new_value) = 0;

  // Returns true on success, otherwise returns false.
  virtual bool UnSetVar(const char* variable_name) = 0;
};


typedef string16 NativeEnvironmentString;
typedef std::map<NativeEnvironmentString, NativeEnvironmentString>
    EnvironmentMap;

// Returns a modified environment vector constructed from the given environment
// and the list of changes given in |changes|. Each key in the environment is
// matched against the first element of the pairs. In the event of a match, the
// value is replaced by the second of the pair, unless the second is empty, in
// which case the key-value is removed.
//
// This Windows version takes and returns a Windows-style environment block
// which is a concatenated list of null-terminated 16-bit strings. The end is
// marked by a double-null terminator. The size of the returned string will
// include the terminators.
CRBASE_EXPORT string16 AlterEnvironment(const wchar_t* env,
                                        const EnvironmentMap& changes);


}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_ENVIRONMENT_H_