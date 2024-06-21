// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_TIME_DEFAULT_TICK_CLOCK_H_
#define MINI_CHROMIUM_SRC_CRBASE_TIME_DEFAULT_TICK_CLOCK_H_

#include "crbase/base_export.h"
#include "crbase/time/tick_clock.h"

namespace crbase {

// DefaultClock is a Clock implementation that uses TimeTicks::Now().
class CRBASE_EXPORT DefaultTickClock : public TickClock {
 public:
  ~DefaultTickClock() override;

  // Simply returns TimeTicks::Now().
  TimeTicks NowTicks() override;
};

}  // namespace crbase

#endif  //MINI_CHROMIUM_SRC_CRBASE_TIME_DEFAULT_TICK_CLOCK_H_