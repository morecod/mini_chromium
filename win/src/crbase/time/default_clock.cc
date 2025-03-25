// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/time/default_clock.h"

namespace crbase {

DefaultClock::~DefaultClock() {}

Time DefaultClock::Now() const {
  return Time::Now();
}

}  // namespace crbase