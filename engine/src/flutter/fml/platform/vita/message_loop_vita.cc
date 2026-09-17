// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/fml/platform/vita/message_loop_vita.h"

#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr.h>

#include "flutter/fml/logging.h"

namespace fml {

// The one bit of the event flag this loop uses. An event flag carries 32, and
// only "something happened" is needed.
static constexpr unsigned int kWakeBit = 1u;

MessageLoopVita::MessageLoopVita() {
  event_flag_ = sceKernelCreateEventFlag("flutter_loop", 0, 0, nullptr);
  FML_CHECK(event_flag_ >= 0)
      << "sceKernelCreateEventFlag failed: " << event_flag_;
}

MessageLoopVita::~MessageLoopVita() {
  if (event_flag_ >= 0) {
    sceKernelDeleteEventFlag(event_flag_);
    event_flag_ = -1;
  }
}

// |fml::MessageLoopImpl|
void MessageLoopVita::Run() {
  running_ = true;

  while (running_) {
    // Clear before reading the wake time, not after. If WakeUp lands in the
    // window between the two, the bit it sets survives into the wait below and
    // returns immediately -- which is the correct outcome. Clearing afterwards
    // would swallow that wakeup and the loop would sleep through it.
    sceKernelClearEventFlag(event_flag_, ~kWakeBit);

    const int64_t wake_us = wake_time_us_.load(std::memory_order_acquire);
    const int64_t now_us = TimePoint::Now().ToEpochDelta().ToMicroseconds();

    if (wake_us <= now_us) {
      // Reset before running, so that a WakeUp issued *by* one of these tasks
      // is not overwritten when they finish.
      wake_time_us_.store(kNoWakeUp, std::memory_order_release);
      RunExpiredTasksNow();
      continue;
    }

    SceUInt timeout_us_storage = 0;
    SceUInt* timeout = nullptr;  // nullptr means wait indefinitely.
    if (wake_us != kNoWakeUp) {
      const int64_t delta = wake_us - now_us;
      // SceUInt is 32-bit, so a deadline more than ~71 minutes out would
      // overflow. Cap it and wake early instead: the loop re-reads the deadline
      // and sleeps again, so a spurious wakeup costs one cheap iteration and
      // an overflowed timeout would cost a missed one.
      constexpr int64_t kMaxTimeoutUs = 0x7FFFFFFF;
      timeout_us_storage =
          static_cast<SceUInt>(delta > kMaxTimeoutUs ? kMaxTimeoutUs : delta);
      timeout = &timeout_us_storage;
    }

    unsigned int out_bits = 0;
    const int result =
        sceKernelWaitEventFlag(event_flag_, kWakeBit, SCE_EVENT_WAITOR,
                               &out_bits, timeout);

    // A timeout is the ordinary case -- it means the deadline arrived -- so it
    // is not an error. Anything else that is not success is, and unlike the
    // Linux loop this says so rather than silently dropping out of the loop:
    // a message loop that stops running is not a condition to discover later.
    if (result < 0 &&
        static_cast<unsigned int>(result) != SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
      FML_LOG(ERROR) << "sceKernelWaitEventFlag failed: " << result;
      running_ = false;
    }
  }
}

// |fml::MessageLoopImpl|
void MessageLoopVita::Terminate() {
  running_ = false;
  WakeUp(TimePoint::Now());
}

// |fml::MessageLoopImpl|
void MessageLoopVita::WakeUp(fml::TimePoint time_point) {
  wake_time_us_.store(time_point.ToEpochDelta().ToMicroseconds(),
                      std::memory_order_release);
  sceKernelSetEventFlag(event_flag_, kWakeBit);
}

}  // namespace fml
