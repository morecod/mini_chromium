// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winbase\power_monitor\power_monitor.h"

#include <utility>

#include "winbase\power_monitor\power_monitor_source.h"
///#include "winbase\trace_event\trace_event.h"

namespace winbase {

static PowerMonitor* g_power_monitor = nullptr;

PowerMonitor::PowerMonitor(std::unique_ptr<PowerMonitorSource> source)
    : observers_(new ObserverListThreadSafe<PowerObserver>()),
      source_(std::move(source)) {
  WINBASE_DCHECK(!g_power_monitor);
  g_power_monitor = this;
}

PowerMonitor::~PowerMonitor() {
  source_->Shutdown();
  WINBASE_DCHECK_EQ(this, g_power_monitor);
  g_power_monitor = nullptr;
}

// static
PowerMonitor* PowerMonitor::Get() {
  return g_power_monitor;
}

void PowerMonitor::AddObserver(PowerObserver* obs) {
  observers_->AddObserver(obs);
}

void PowerMonitor::RemoveObserver(PowerObserver* obs) {
  observers_->RemoveObserver(obs);
}

PowerMonitorSource* PowerMonitor::Source() {
  return source_.get();
}

bool PowerMonitor::IsOnBatteryPower() {
  return source_->IsOnBatteryPower();
}

void PowerMonitor::NotifyPowerStateChange(bool battery_in_use) {
  WINBASE_DVLOG(1) << "PowerStateChange: " << (battery_in_use ? "On" : "Off")
                   << " battery";
  observers_->Notify(WINBASE_FROM_HERE, &PowerObserver::OnPowerStateChange,
                     battery_in_use);
}

void PowerMonitor::NotifySuspend() {
  ///TRACE_EVENT_INSTANT0("base", "PowerMonitor::NotifySuspend",
  ///                     TRACE_EVENT_SCOPE_GLOBAL);
  WINBASE_DVLOG(1) << "Power Suspending";
  observers_->Notify(WINBASE_FROM_HERE, &PowerObserver::OnSuspend);
}

void PowerMonitor::NotifyResume() {
  ///TRACE_EVENT_INSTANT0("base", "PowerMonitor::NotifyResume",
  ///                     TRACE_EVENT_SCOPE_GLOBAL);
  WINBASE_DVLOG(1) << "Power Resuming";
  observers_->Notify(WINBASE_FROM_HERE, &PowerObserver::OnResume);
}

}  // namespace winbase