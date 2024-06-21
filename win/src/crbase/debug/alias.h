// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MINI_CHROMIUM_SRC_CRBASE_DEBUG_ALIAS_H_
#define MINI_CHROMIUM_SRC_CRBASE_DEBUG_ALIAS_H_

#include "crbase/base_export.h"

namespace crbase {
namespace debug {

// Make the optimizer think that var is aliased. This is to prevent it from
// optimizing out variables that that would not otherwise be live at the point
// of a potential crash.
void CRBASE_EXPORT Alias(const void* var);

}  // namespace debug
}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_DEBUG_ALIAS_H_