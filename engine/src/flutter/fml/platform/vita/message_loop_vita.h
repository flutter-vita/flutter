// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FML_PLATFORM_VITA_MESSAGE_LOOP_VITA_H_
#define FLUTTER_FML_PLATFORM_VITA_MESSAGE_LOOP_VITA_H_

#include <psp2/types.h>

#include <atomic>

#include "flutter/fml/macros.h"
#include "flutter/fml/message_loop_impl.h"

namespace fml {

// The Vita's message loop, on a sceKernel event flag.
//
// MessageLoopLinux is built on epoll plus timerfd. Neither exists here, and
// unlike the dart:io event handler this loop waits on no descriptors at all --
// it only ever needs "sleep until a time, or until someone wakes me". A
// sceKernel event flag is exactly that primitive, and it is the one the M1
// timer thread already used, so its behaviour on this device is known rather
// than assumed.
//
// Until this class existed, MessageLoopImpl::Create() fell through to
// `#else return nullptr`, which links cleanly and then fails an FML_CHECK on
// the first thread that asks for a loop.
class MessageLoopVita : public MessageLoopImpl {
 private:
  SceUID event_flag_ = -1;

  // Microseconds on fml::TimePoint's epoch. Read by the loop thread, written by
  // any thread calling WakeUp, hence atomic. kNoWakeUp means "nothing pending":
  // wait indefinitely.
  static constexpr int64_t kNoWakeUp = INT64_MAX;
  std::atomic<int64_t> wake_time_us_{kNoWakeUp};

  std::atomic<bool> running_{false};

  MessageLoopVita();

  ~MessageLoopVita() override;

  // |fml::MessageLoopImpl|
  void Run() override;

  // |fml::MessageLoopImpl|
  void Terminate() override;

  // |fml::MessageLoopImpl|
  void WakeUp(fml::TimePoint time_point) override;

  FML_FRIEND_MAKE_REF_COUNTED(MessageLoopVita);
  FML_FRIEND_REF_COUNTED_THREAD_SAFE(MessageLoopVita);
  FML_DISALLOW_COPY_AND_ASSIGN(MessageLoopVita);
};

}  // namespace fml

#endif  // FLUTTER_FML_PLATFORM_VITA_MESSAGE_LOOP_VITA_H_
