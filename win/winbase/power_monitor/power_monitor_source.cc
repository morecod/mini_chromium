// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "winbase\power_monitor\power_monitor_source.h"

#include "winbase\power_monitor\power_monitor.h"

namespace winbase {

PowerMonitorSource::PowerMonitorSource() = default;
PowerMonitorSource::~PowerMonitorSource() = default;

bool PowerMonitorSource::IsOnBatteryPower() {
  AutoLock auto_lock(battery_lock_);
  return on_battery_power_;
}

void PowerMonitorSource::ProcessPowerEvent(PowerEvent event_id) {
  PowerMonitor* monitor = PowerMonitor::Get();
  if (!monitor)
    return;

  PowerMonitorSource* source = monitor->Source();

  // Suppress duplicate notifications.  Some platforms may
  // send multiple notifications of the same event.
  switch (event_id) {
    case POWER_STATE_EVENT:
      {
        bool new_on_battery_power = source->IsOnBatteryPowerImpl();
        bool changed = false;

        {
          AutoLock auto_lock(source->battery_lock_);
          if (source->on_battery_power_ != new_on_battery_power) {
              changed = true;
              source->on_battery_power_ = new_on_battery_power;
          }
        }

        if (changed)
          monitor->NotifyPowerStateChange(new_on_battery_power);
      }
      break;
    case RESUME_EVENT:
      if (source->suspended_) {
        source->suspended_ = false;
        monitor->NotifyResume();
      }
      break;
    case SUSPEND_EVENT:
      if (!source->suspended_) {
        source->suspended_ = true;
        monitor->NotifySuspend();
      }
      break;
  }
}

void PowerMonitorSource::SetInitialOnBatteryPowerState(bool on_battery_power) {
  // Must only be called before a monitor exists, otherwise the caller should
  // have just used a normal ProcessPowerEvent(POWER_STATE_EVENT) call.
  WINBASE_DCHECK(!PowerMonitor::Get());
  on_battery_power_ = on_battery_power;
}

}  // namespace winbase