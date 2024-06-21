// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/sys_info.h"

#include "crbase/strings/string_number_conversions.h"
#include "crbase/strings/string_util.h"
#include "crbase/time/time.h"

namespace crbase {

static const int kLowMemoryDeviceThresholdMB = 1024;

bool DetectLowEndDevice() {
  int ram_size_mb = SysInfo::AmountOfPhysicalMemoryMB();
  return (ram_size_mb > 0 && ram_size_mb < kLowMemoryDeviceThresholdMB);
}

// static
bool SysInfo::IsLowEndDevice() {
  return DetectLowEndDevice();
}

std::string SysInfo::HardwareModelName() {
  return std::string();
}

// static
TimeDelta SysInfo::Uptime() {
  // This code relies on an implementation detail of TimeTicks::Now() - that
  // its return value happens to coincide with the system uptime value in
  // microseconds, on Win/Mac/iOS/Linux/ChromeOS and Android.
  int64_t uptime_in_microseconds = TimeTicks::Now().ToInternalValue();
  return TimeDelta::FromMicroseconds(uptime_in_microseconds);
}

}  // namespace crbase