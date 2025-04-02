// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/json/json_value_converter.h"

namespace cr {
namespace internal {

bool BasicValueConverter<int>::Convert(
    const cr::Value& value, int* field) const {
  return value.GetAsInteger(field);
}

bool BasicValueConverter<std::string>::Convert(
    const cr::Value& value, std::string* field) const {
  return value.GetAsString(field);
}

bool BasicValueConverter<string16>::Convert(
    const cr::Value& value, string16* field) const {
  return value.GetAsString(field);
}

bool BasicValueConverter<double>::Convert(
    const cr::Value& value, double* field) const {
  return value.GetAsDouble(field);
}

bool BasicValueConverter<bool>::Convert(
    const cr::Value& value, bool* field) const {
  return value.GetAsBoolean(field);
}

}  // namespace internal
}  // namespace cr
