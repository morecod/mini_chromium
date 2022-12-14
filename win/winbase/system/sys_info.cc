// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winbase\system\sys_info.h"

#include <algorithm>

///#include "winbase\base_switches.h"
///#include "winbase\command_line.h"
#include "winbase\lazy_instance.h"
#include "winbase\system\sys_info_internal.h"
#include "winbase\time\time.h"

namespace winbase {
namespace {
static const int kLowMemoryDeviceThresholdMB = 512;
}

// static
int64_t SysInfo::AmountOfPhysicalMemory() {
  ///if (winbase::CommandLine::ForCurrentProcess()->HasSwitch(
  ///        switches::kEnableLowEndDeviceMode)) {
  ///  return kLowMemoryDeviceThresholdMB * 1024 * 1024;
  ///}

  return AmountOfPhysicalMemoryImpl();
}

// static
int64_t SysInfo::AmountOfAvailablePhysicalMemory() {
  ///if (winbase::CommandLine::ForCurrentProcess()->HasSwitch(
  ///        switches::kEnableLowEndDeviceMode)) {
  ///  // Estimate the available memory by subtracting our memory used estimate
  ///  // from the fake |kLowMemoryDeviceThresholdMB| limit.
  ///  size_t memory_used =
  ///      AmountOfPhysicalMemoryImpl() - AmountOfAvailablePhysicalMemoryImpl();
  ///  size_t memory_limit = kLowMemoryDeviceThresholdMB * 1024 * 1024;
  ///  // std::min ensures no underflow, as |memory_used| can be > |memory_limit|.
  ///  return memory_limit - std::min(memory_used, memory_limit);
  ///}

  return AmountOfAvailablePhysicalMemoryImpl();
}

bool SysInfo::IsLowEndDevice() {
  ///if (winbase::CommandLine::ForCurrentProcess()->HasSwitch(
  ///        switches::kEnableLowEndDeviceMode)) {
  ///  return true;
  ///}

  return IsLowEndDeviceImpl();
}

bool DetectLowEndDevice() {
  ///CommandLine* command_line = CommandLine::ForCurrentProcess();
  ///if (command_line->HasSwitch(switches::kEnableLowEndDeviceMode))
  ///  return true;
  ///if (command_line->HasSwitch(switches::kDisableLowEndDeviceMode))
  ///  return false;

  int ram_size_mb = SysInfo::AmountOfPhysicalMemoryMB();
  return (ram_size_mb > 0 && ram_size_mb <= kLowMemoryDeviceThresholdMB);
}

static LazyInstance<
  internal::LazySysInfoValue<bool, DetectLowEndDevice> >::Leaky
  g_lazy_low_end_device = WINBASE_LAZY_INSTANCE_INITIALIZER;

// static
bool SysInfo::IsLowEndDeviceImpl() {
  return g_lazy_low_end_device.Get().value();
}

std::string SysInfo::HardwareModelName() {
  return std::string();
}

// static
winbase::TimeDelta SysInfo::Uptime() {
  // This code relies on an implementation detail of TimeTicks::Now() - that
  // its return value happens to coincide with the system uptime value in
  // microseconds, on Win/Mac/iOS/Linux/ChromeOS and Android.
  int64_t uptime_in_microseconds = TimeTicks::Now().ToInternalValue();
  return winbase::TimeDelta::FromMicroseconds(uptime_in_microseconds);
}

}  // namespace winbase