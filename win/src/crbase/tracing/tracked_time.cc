// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/tracing/tracked_time.h"

#include <mmsystem.h>  // Declare timeGetTime()... after including build_config.

namespace cr {
namespace tracked_objects {

Duration::Duration() : ms_(0) {}
Duration::Duration(int32_t duration) : ms_(duration) {}

Duration& Duration::operator+=(const Duration& other) {
  ms_ += other.ms_;
  return *this;
}

Duration Duration::operator+(const Duration& other) const {
  return Duration(ms_ + other.ms_);
}

bool Duration::operator==(const Duration& other) const {
  return ms_ == other.ms_;
}

bool Duration::operator!=(const Duration& other) const {
  return ms_ != other.ms_;
}

bool Duration::operator>(const Duration& other) const {
  return ms_ > other.ms_;
}

// static
Duration Duration::FromMilliseconds(int ms) { return Duration(ms); }

int32_t Duration::InMilliseconds() const {
  return ms_;
}

//------------------------------------------------------------------------------

TrackedTime::TrackedTime() : ms_(0) {}
TrackedTime::TrackedTime(int32_t ms) : ms_(ms) {}
TrackedTime::TrackedTime(const cr::TimeTicks& time)
    : ms_(static_cast<int32_t>(
          (time - cr::TimeTicks()).InMilliseconds())) {}

// static
TrackedTime TrackedTime::Now() {
  return TrackedTime(cr::TimeTicks::Now());
}

Duration TrackedTime::operator-(const TrackedTime& other) const {
  return Duration(ms_ - other.ms_);
}

TrackedTime TrackedTime::operator+(const Duration& other) const {
  return TrackedTime(ms_ + other.ms_);
}

bool TrackedTime::is_null() const { return ms_ == 0; }

}  // namespace tracked_objects
}  // namespace cr
