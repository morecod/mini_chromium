// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_VALUE_CONVERSIONS_H_
#define MINI_CHROMIUM_SRC_CRBASE_VALUE_CONVERSIONS_H_

// This file contains methods to convert things to a |Value| and back.

#include "crbase/base_export.h"

namespace cr {

class FilePath;
class TimeDelta;
class StringValue;
class Value;

// The caller takes ownership of the returned value.
CRBASE_EXPORT StringValue* CreateFilePathValue(const FilePath& in_value);
CRBASE_EXPORT bool GetValueAsFilePath(const Value& value, FilePath* file_path);

CRBASE_EXPORT StringValue* CreateTimeDeltaValue(const TimeDelta& time);
CRBASE_EXPORT bool GetValueAsTimeDelta(const Value& value, TimeDelta* time);

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_VALUE_CONVERSIONS_H_