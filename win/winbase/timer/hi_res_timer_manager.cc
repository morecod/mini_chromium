// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winbase\timer\hi_res_timer_manager.h"

#include <algorithm>

#include "winbase\atomic\atomicops.h"
///#include "winbase\metrics\histogram_macros.h"
#include "winbase\power_monitor\power_monitor.h"
///#include "winbase\task_scheduler\post_task.h"
#include "winbase\time\time.h"

namespace winbase {

namespace {

constexpr TimeDelta kUsageSampleInterval = TimeDelta::FromMinutes(10);

void ReportHighResolutionTimerUsage() {
  ///UMA_HISTOGRAM_PERCENTAGE("Windows.HighResolutionTimerUsage",
  ///                         Time::GetHighResolutionTimerUsage());
  // Reset usage for the next interval.
  Time::ResetHighResolutionTimerUsage();
}

}  // namespace

HighResolutionTimerManager::HighResolutionTimerManager()
    : hi_res_clock_available_(false) {
  PowerMonitor* power_monitor = PowerMonitor::Get();
  WINBASE_DCHECK(power_monitor != NULL);
  power_monitor->AddObserver(this);
  UseHiResClock(!power_monitor->IsOnBatteryPower());

  // Start polling the high resolution timer usage.
  Time::ResetHighResolutionTimerUsage();
  timer_.Start(WINBASE_FROM_HERE, kUsageSampleInterval,
               Bind(&ReportHighResolutionTimerUsage));
}

HighResolutionTimerManager::~HighResolutionTimerManager() {
  PowerMonitor::Get()->RemoveObserver(this);
  UseHiResClock(false);
}

void HighResolutionTimerManager::OnPowerStateChange(bool on_battery_power) {
  UseHiResClock(!on_battery_power);
}

void HighResolutionTimerManager::OnSuspend() {
  // Stop polling the usage to avoid including the standby time.
  timer_.Stop();
}

void HighResolutionTimerManager::OnResume() {
  // Resume polling the usage.
  Time::ResetHighResolutionTimerUsage();
  timer_.Reset();
}

void HighResolutionTimerManager::UseHiResClock(bool use) {
  if (use == hi_res_clock_available_)
    return;
  hi_res_clock_available_ = use;
  Time::EnableHighResolutionTimer(use);
}

}  // namespace winbase