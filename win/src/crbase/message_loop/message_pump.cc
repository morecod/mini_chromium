// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crbase/message_loop/message_pump.h"

namespace crbase {

MessagePump::MessagePump() {
}

MessagePump::~MessagePump() {
}

void MessagePump::SetTimerSlack(TimerSlack) {
}

}  // namespace crbase