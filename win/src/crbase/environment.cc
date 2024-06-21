// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/environment.h"

#include <stddef.h>
#include <windows.h>

#include <vector>
#include <memory>

#include "crbase/strings/string_piece.h"
#include "crbase/strings/string_util.h"
#include "crbase/strings/utf_string_conversions.h"


namespace crbase {

namespace {

class EnvironmentImpl : public Environment {
 public:
  bool GetVar(const char* variable_name, std::string* result) override {
    if (GetVarImpl(variable_name, result))
      return true;

    // Some commonly used variable names are uppercase while others
    // are lowercase, which is inconsistent. Let's try to be helpful
    // and look for a variable name with the reverse case.
    // I.e. HTTP_PROXY may be http_proxy for some users/systems.
    char first_char = variable_name[0];
    std::string alternate_case_var;
    if (first_char >= 'a' && first_char <= 'z')
      alternate_case_var = ToUpperASCII(variable_name);
    else if (first_char >= 'A' && first_char <= 'Z')
      alternate_case_var = ToLowerASCII(variable_name);
    else
      return false;
    return GetVarImpl(alternate_case_var.c_str(), result);
  }

  bool SetVar(const char* variable_name,
              const std::string& new_value) override {
    return SetVarImpl(variable_name, new_value);
  }

  bool UnSetVar(const char* variable_name) override {
    return UnSetVarImpl(variable_name);
  }

 private:
  bool GetVarImpl(const char* variable_name, std::string* result) {
    DWORD value_length = ::GetEnvironmentVariableW(
        UTF8ToWide(variable_name).c_str(), NULL, 0);
    if (value_length == 0)
      return false;
    if (result) {
      std::unique_ptr<wchar_t[]> value(new wchar_t[value_length]);
      ::GetEnvironmentVariableW(UTF8ToWide(variable_name).c_str(), value.get(),
                                value_length);
      *result = WideToUTF8(value.get());
    }
    return true;
  }

  bool SetVarImpl(const char* variable_name, const std::string& new_value) {
    // On success, a nonzero value is returned.
    return !!SetEnvironmentVariableW(UTF8ToWide(variable_name).c_str(),
                                     UTF8ToWide(new_value).c_str());
  }

  bool UnSetVarImpl(const char* variable_name) {
    // On success, a nonzero value is returned.
    return !!SetEnvironmentVariableW(UTF8ToWide(variable_name).c_str(), NULL);
  }
};

// Parses a null-terminated input string of an environment block. The key is
// placed into the given string, and the total length of the line, including
// the terminating null, is returned.
size_t ParseEnvLine(const NativeEnvironmentString::value_type* input,
                    NativeEnvironmentString* key) {
  // Skip to the equals or end of the string, this is the key.
  size_t cur = 0;
  while (input[cur] && input[cur] != '=')
    cur++;
  *key = NativeEnvironmentString(&input[0], cur);

  // Now just skip to the end of the string.
  while (input[cur])
    cur++;
  return cur + 1;
}

}  // namespace

Environment::~Environment() {}

// static
Environment* Environment::Create() {
  return new EnvironmentImpl();
}

bool Environment::HasVar(const char* variable_name) {
  return GetVar(variable_name, NULL);
}

string16 AlterEnvironment(const wchar_t* env,
                          const EnvironmentMap& changes) {
  string16 result;

  // First copy all unmodified values to the output.
  size_t cur_env = 0;
  string16 key;
  while (env[cur_env]) {
    const wchar_t* line = &env[cur_env];
    size_t line_length = ParseEnvLine(line, &key);

    // Keep only values not specified in the change vector.
    EnvironmentMap::const_iterator found_change = changes.find(key);
    if (found_change == changes.end())
      result.append(line, line_length);

    cur_env += line_length;
  }

  // Now append all modified and new values.
  for (EnvironmentMap::const_iterator i = changes.begin();
       i != changes.end(); ++i) {
    if (!i->second.empty()) {
      result.append(i->first);
      result.push_back('=');
      result.append(i->second);
      result.push_back(0);
    }
  }

  // An additional null marks the end of the list. We always need a double-null
  // in case nothing was added above.
  if (result.empty())
    result.push_back(0);
  result.push_back(0);
  return result;
}

}  // namespace crbase
