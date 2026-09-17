// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/fml/thread.h"

#include <memory>
#include <string>
#include <utility>

#include "flutter/fml/build_config.h"

#if defined(FML_OS_VITA)
#include <psp2/kernel/cpu.h>
#include <psp2/kernel/threadmgr.h>
#endif
#include "flutter/fml/message_loop.h"
#include "flutter/fml/synchronization/waitable_event.h"

#if defined(FML_OS_WIN)
#include <windows.h>
#elif defined(OS_FUCHSIA)
#include <lib/zx/thread.h>
#else
#include <pthread.h>
#endif

extern "C" void flutter_vita_log_affinity(const char* name,
                                         int before,
                                         int after);

namespace fml {

typedef std::function<void()> ThreadFunction;

class ThreadHandle {
 public:
  explicit ThreadHandle(ThreadFunction&& function);
  ~ThreadHandle();

  void Join();

 private:
#if defined(FML_OS_WIN)
  HANDLE thread_;
#else
  pthread_t thread_;
#endif
};

#if defined(FML_OS_WIN)
ThreadHandle::ThreadHandle(ThreadFunction&& function) {
  thread_ = (HANDLE*)_beginthreadex(
      nullptr, Thread::GetDefaultStackSize(),
      [](void* arg) -> unsigned {
        std::unique_ptr<ThreadFunction> function(
            reinterpret_cast<ThreadFunction*>(arg));
        (*function)();
        return 0;
      },
      new ThreadFunction(std::move(function)), 0, nullptr);
  FML_CHECK(thread_ != nullptr);
}

void ThreadHandle::Join() {
  WaitForSingleObjectEx(thread_, INFINITE, FALSE);
}

ThreadHandle::~ThreadHandle() {
  CloseHandle(thread_);
}
#else
ThreadHandle::ThreadHandle(ThreadFunction&& function) {
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  int result = pthread_attr_setstacksize(&attr, Thread::GetDefaultStackSize());
  FML_CHECK(result == 0);
  result = pthread_create(
      &thread_, &attr,
      [](void* arg) -> void* {
        std::unique_ptr<ThreadFunction> function(
            reinterpret_cast<ThreadFunction*>(arg));
        (*function)();
        return nullptr;
      },
      new ThreadFunction(std::move(function)));
  FML_CHECK(result == 0);
  result = pthread_attr_destroy(&attr);
  FML_CHECK(result == 0);
}

void ThreadHandle::Join() {
  pthread_join(thread_, nullptr);
}

ThreadHandle::~ThreadHandle() {}
#endif

#if defined(FML_OS_WIN)
// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD kVCThreadNameException = 0x406D1388;
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;      // Must be 0x1000.
  LPCSTR szName;     // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;     // Reserved for future use, must be zero.
} THREADNAME_INFO;
#endif

void SetThreadName(const std::string& name) {
  if (name == "") {
    return;
  }
#if defined(FML_OS_MACOSX)
  pthread_setname_np(name.c_str());
#elif defined(FML_OS_LINUX) || defined(FML_OS_ANDROID)
  // Linux thread names are limited to 16 characters including the terminating
  // null.
  constexpr std::string::size_type kLinuxMaxThreadNameLen = 15;
  pthread_setname_np(pthread_self(),
                     name.substr(0, kLinuxMaxThreadNameLen).c_str());
#elif defined(FML_OS_WIN)
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name.c_str();
  info.dwThreadID = GetCurrentThreadId();
  info.dwFlags = 0;
  __try {
    RaiseException(kVCThreadNameException, 0, sizeof(info) / sizeof(DWORD),
                   reinterpret_cast<DWORD_PTR*>(&info));
  } __except (EXCEPTION_CONTINUE_EXECUTION) {
  }
#elif defined(OS_FUCHSIA)
  zx::thread::self()->set_property(ZX_PROP_NAME, name.c_str(), name.size());
#elif defined(FML_OS_VITA)
  // The Vita has no pthread_setname_np, so this hook reports what it can
  // instead: which cores the engine's threads are allowed to run on.
  //
  // Nothing sets that. fml::RequestAffinity is a no-op outside Android, and
  // this console's three user cores are identical, so Flutter's
  // performance/efficiency distinction has nothing to select between.
  //
  // Pinning was tried here on 2026-08-24 and did not work. The hypothesis was
  // contention: route_push runs 14x faster under GL and its UI-thread build
  // time falls from 48.9 ms to 1.2 ms, which should not depend on the
  // renderer unless the renderer decides how much CPU the UI thread gets. So
  // ui, raster and io were pinned to USER_0, USER_1 and USER_2. Every scene in
  // the benchmark came back within noise of unpinned, and two came back 12 and
  // 17 percent worse. The threads were not fighting over a core.
  //
  // What remains is bus contention rather than scheduling: the software
  // rasteriser writes a 2 MB framebuffer every frame and Dart's own work
  // slows down alongside it. That would be invisible to affinity, which is
  // consistent with this result. Untested.
  //
  // The logging stays because it is the measurement that settled it, and
  // because a `before` of 0 is worth knowing: no engine thread on this port
  // has ever had an affinity mask set.
  {
    const SceUID self = sceKernelGetThreadId();
    const int mask = sceKernelGetThreadCpuAffinityMask(self);
    flutter_vita_log_affinity(name.c_str(), mask, mask);
  }
#else
  FML_DLOG(INFO) << "Could not set the thread name to '" << name
                 << "' on this platform.";
#endif
}

void Thread::SetCurrentThreadName(const Thread::ThreadConfig& config) {
  SetThreadName(config.name);
}

Thread::Thread(const std::string& name)
    : Thread(Thread::SetCurrentThreadName, ThreadConfig(name)) {}

Thread::Thread(const ThreadConfigSetter& setter, const ThreadConfig& config)
    : joined_(false) {
  fml::AutoResetWaitableEvent latch;
  fml::RefPtr<fml::TaskRunner> runner;

  thread_ = std::make_unique<ThreadHandle>(
      [&latch, &runner, setter, config]() -> void {
        setter(config);
        fml::MessageLoop::EnsureInitializedForCurrentThread();
        auto& loop = MessageLoop::GetCurrent();
        runner = loop.GetTaskRunner();
        latch.Signal();
        loop.Run();
      });
  latch.Wait();
  task_runner_ = runner;
}

Thread::~Thread() {
  Join();
}

fml::RefPtr<fml::TaskRunner> Thread::GetTaskRunner() const {
  return task_runner_;
}

void Thread::Join() {
  if (joined_) {
    return;
  }
  joined_ = true;
  task_runner_->PostTask([]() { MessageLoop::GetCurrent().Terminate(); });
  thread_->Join();
}

size_t Thread::GetDefaultStackSize() {
  return 1024 * 1024 * 2;
}

}  // namespace fml
