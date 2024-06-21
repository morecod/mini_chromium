// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_TIME_DEFAULT_CLOCK_H_
#define MINI_CHROMIUM_SRC_CRBASE_TIME_DEFAULT_CLOCK_H_

#include "crbase/base_export.h"
#include "crbase/time/clock.h"

namespace crbase {

// DefaultClock is a Clock implementation that uses Time::Now().
class CRBASE_EXPORT DefaultClock : public Clock {
 public:
  ~DefaultClock() override;

  // Simply returns Time::Now().
  Time Now() override;
};

}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_TIME_DEFAULT_CLOCK_H_