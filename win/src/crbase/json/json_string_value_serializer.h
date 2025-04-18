// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_JSON_JSON_STRING_VALUE_SERIALIZER_H_
#define MINI_CHROMIUM_SRC_CRBASE_JSON_JSON_STRING_VALUE_SERIALIZER_H_

#include <string>

#include "crbase/base_export.h"
#include "crbase/files/file_path.h"
#include "crbase/macros.h"
#include "crbase/strings/string_piece.h"
#include "crbase/values.h"

namespace cr {

class CRBASE_EXPORT JSONStringValueSerializer 
    : public ValueSerializer {
 public:
  JSONStringValueSerializer(const JSONStringValueSerializer&) = delete;
  JSONStringValueSerializer& operator=(const JSONStringValueSerializer&) = delete;

  // |json_string| is the string that will be the destination of the
  // serialization.  The caller of the constructor retains ownership of the
  // string. |json_string| must not be null.
  explicit JSONStringValueSerializer(std::string* json_string);

  ~JSONStringValueSerializer() override;

  // Attempt to serialize the data structure represented by Value into
  // JSON.  If the return value is true, the result will have been written
  // into the string passed into the constructor.
  bool Serialize(const Value& root) override;

  // Equivalent to Serialize(root) except binary values are omitted from the
  // output.
  bool SerializeAndOmitBinaryValues(const Value& root);

  void set_pretty_print(bool new_value) { pretty_print_ = new_value; }
  bool pretty_print() { return pretty_print_; }

 private:
  bool SerializeInternal(const Value& root, bool omit_binary_values);

  // Owned by the caller of the constructor.
  std::string* json_string_;
  bool pretty_print_;  // If true, serialization will span multiple lines.
};

class CRBASE_EXPORT JSONStringValueDeserializer 
    : public ValueDeserializer {
 public:
  JSONStringValueDeserializer(
      const JSONStringValueDeserializer&) = delete;
  JSONStringValueDeserializer& operator=(
      const JSONStringValueDeserializer&) = delete;

  // This retains a reference to the contents of |json_string|, so the data
  // must outlive the JSONStringValueDeserializer.
  explicit JSONStringValueDeserializer(const StringPiece& json_string);

  ~JSONStringValueDeserializer() override;

  // Attempt to deserialize the data structure encoded in the string passed
  // in to the constructor into a structure of Value objects.  If the return
  // value is null, and if |error_code| is non-null, |error_code| will
  // contain an integer error code (a JsonParseError in this case).
  // If |error_message| is non-null, it will be filled in with a formatted
  // error message including the location of the error if appropriate.
  // The caller takes ownership of the returned value.
  std::unique_ptr<Value> Deserialize(int *error_code,
                                     std::string *error_message) override;

  void set_allow_trailing_comma(bool new_value) {
    allow_trailing_comma_ = new_value;
  }

 private:
  // Data is owned by the caller of the constructor.
  StringPiece json_string_;
  
  // If true, deserialization will allow trailing commas.
  bool allow_trailing_comma_;
};

}  // namespace cr

#endif  // MINI_CHROMIUM_SRC_CRBASE_JSON_JSON_STRING_VALUE_SERIALIZER_H_