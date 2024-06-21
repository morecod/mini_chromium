// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_SRC_CRBASE_TRACING_TRACKED_TIME_H_
#define MINI_CHROMIUM_SRC_CRBASE_TRACING_TRACKED_TIME_H_

#include <stdint.h>

#include "crbase/base_export.h"
#include "crbase/time/time.h"

namespace crbase {
namespace tracked_objects {

//------------------------------------------------------------------------------

// TimeTicks maintains a wasteful 64 bits of data (we need less than 32), and on
// windows, a 64 bit timer is expensive to even obtain. We use a simple
// millisecond counter for most of our time values, as well as millisecond units
// of duration between those values.  This means we can only handle durations
// up to 49 days (range), or 24 days (non-negative time durations).
// We only define enough methods to service the needs of the tracking classes,
// and our interfaces are modeled after what TimeTicks and TimeDelta use (so we
// can swap them into place if we want to use the "real" classes).

class CRBASE_EXPORT Duration {  // Similar to crbase::TimeDelta.
 public:
  Duration();

  Duration& operator+=(const Duration& other);
  Duration operator+(const Duration& other) const;

  bool operator==(const Duration& other) const;
  bool operator!=(const Duration& other) const;
  bool operator>(const Duration& other) const;

  static Duration FromMilliseconds(int ms);

  int32_t InMilliseconds() const;

 private:
  friend class TrackedTime;
  explicit Duration(int32_t duration);

  // Internal time is stored directly in milliseconds.
  int32_t ms_;
};

class CRBASE_EXPORT TrackedTime {  // Similar to base::TimeTicks.
 public:
  TrackedTime();
  explicit TrackedTime(const crbase::TimeTicks& time);

  static TrackedTime Now();
  Duration operator-(const TrackedTime& other) const;
  TrackedTime operator+(const Duration& other) const;
  bool is_null() const;

  static TrackedTime FromMilliseconds(int32_t ms) { return TrackedTime(ms); }

 private:
  friend class Duration;
  explicit TrackedTime(int32_t ms);

  // Internal duration is stored directly in milliseconds.
  uint32_t ms_;
};

}  // namespace tracked_objects
}  // namespace crbase

#endif  // MINI_CHROMIUM_SRC_CRBASE_TRACING_TRACKED_TIME_H_