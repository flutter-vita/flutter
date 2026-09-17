// Thread-safe statics for the Vita, because libstdc++'s do not work here.
//
// A function-local static with a dynamic initialiser --
//
//     static auto* bus = new SkMessageBus<...>();
//
// -- compiles to a __cxa_guard_acquire / __cxa_guard_release pair around the
// construction, and those come from libstdc++. On this statically linked
// VitaSDK build, calling one faults. The probe in spikes/g2-ganesh shows a
// plain `new` succeeding and the identical allocation behind a function-local
// static killing the process, with no Skia and no GL anywhere near it.
//
// That is why ADR-0014's criterion 2 spent five bisection layers ending inside
// SkMessageBus::Get(): the GPU had nothing to do with it. It is also a
// candidate explanation for the intermittent hard fault open since the corpus
// work, because *every* dynamically initialised function-local static in the
// engine goes through these.
//
// The spike defined a three-line version that assumes single-threaded
// initialisation. That is true of a spike whose whole life is on the main
// thread and false of the engine, which has a platform thread, a UI thread, a
// raster thread, an IO thread and Dart's workers, any of which can be first to
// touch a given static. This is the real one.
//
// ## The ABI
//
// ARM EABI (IHI0041, §3.2.3) rather than Itanium: the guard is a **32-bit**
// word, not 64, and bit 0 of its first byte means "initialisation complete".
// The compiler is explicitly permitted to test that bit inline without calling
// anything, which is why __cxa_guard_release must publish with a release
// barrier -- on a multi-core Cortex-A9 another core can otherwise observe the
// flag set before the object it guards is visible.
//
// The remaining bytes are the implementation's. Byte 1 is used here for
// "someone is constructing", the same byte libstdc++ uses for its pending bit,
// so the layout is unsurprising to anything that looks.
//
// ## Why one global lock
//
// Per-guard locks would need somewhere to live, and the guard word has no room
// for a pointer. A single mutex serialises all static initialisation in the
// process, which sounds worse than it is: these run once each, they are short,
// and the contended case is two threads racing to first-touch the same static
// -- which has to serialise anyway.
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

extern "C" {

namespace {

pthread_mutex_t g_guard_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_guard_cond = PTHREAD_COND_INITIALIZER;

constexpr uint8_t kDone = 1;      // byte 0, bit 0 -- defined by the ABI
constexpr uint8_t kPending = 1;   // byte 1 -- ours

// Who is currently constructing what, so that a static whose constructor
// reaches the same static can be reported instead of deadlocking.
//
// Recursive initialisation is a program bug and libstdc++ answers it by
// throwing __gnu_cxx::recursive_init_error. This build has no exceptions, and
// the alternative -- waiting on a condition variable that the waiting thread
// is itself responsible for signalling -- is a silent hang on a console with
// no debugger, which is the worst outcome available. Sixteen entries is well
// past any real nesting depth; overflowing it only costs the diagnostic.
struct InProgress {
  void* guard;
  pthread_t owner;
};
constexpr int kMaxInProgress = 16;
InProgress g_in_progress[kMaxInProgress];

bool OwnedByCurrentThread(void* guard) {
  for (const InProgress& e : g_in_progress) {
    if (e.guard == guard) return pthread_equal(e.owner, pthread_self()) != 0;
  }
  return false;
}

void NoteInProgress(void* guard) {
  for (InProgress& e : g_in_progress) {
    if (e.guard == nullptr) {
      e.guard = guard;
      e.owner = pthread_self();
      return;
    }
  }
}

void ClearInProgress(void* guard) {
  for (InProgress& e : g_in_progress) {
    if (e.guard == guard) {
      e.guard = nullptr;
      return;
    }
  }
}

}  // namespace

// Returns non-zero if the caller must run the initialiser, zero if it is
// already done. Blocks while another thread is running it.
int __cxa_guard_acquire(int* guard) {
  uint8_t* bytes = reinterpret_cast<uint8_t*>(guard);

  pthread_mutex_lock(&g_guard_mutex);
  for (;;) {
    if (bytes[0] & kDone) {
      pthread_mutex_unlock(&g_guard_mutex);
      return 0;
    }
    if (!(bytes[1] & kPending)) {
      bytes[1] |= kPending;
      NoteInProgress(guard);
      pthread_mutex_unlock(&g_guard_mutex);
      return 1;
    }
    if (OwnedByCurrentThread(guard)) {
      // Deliberately not a wait. See the note on InProgress above.
      pthread_mutex_unlock(&g_guard_mutex);
      fprintf(stderr,
              "flutter-vita: recursive initialisation of the static at %p\n",
              static_cast<void*>(guard));
      __builtin_trap();
    }
    pthread_cond_wait(&g_guard_cond, &g_guard_mutex);
  }
}

void __cxa_guard_release(int* guard) {
  uint8_t* bytes = reinterpret_cast<uint8_t*>(guard);

  // Publish the object before the flag that says it exists. The compiler tests
  // bit 0 inline and without a lock, so this barrier is the only thing
  // standing between a second core and a fully constructed pointer to
  // uninitialised memory.
  __sync_synchronize();

  pthread_mutex_lock(&g_guard_mutex);
  bytes[0] |= kDone;
  bytes[1] &= static_cast<uint8_t>(~kPending);
  ClearInProgress(guard);
  pthread_cond_broadcast(&g_guard_cond);
  pthread_mutex_unlock(&g_guard_mutex);
}

// The initialiser did not complete. Leave the done bit clear so the next
// caller tries again, and wake anyone waiting so they can be the one to try.
void __cxa_guard_abort(int* guard) {
  uint8_t* bytes = reinterpret_cast<uint8_t*>(guard);

  pthread_mutex_lock(&g_guard_mutex);
  bytes[1] &= static_cast<uint8_t>(~kPending);
  ClearInProgress(guard);
  pthread_cond_broadcast(&g_guard_cond);
  pthread_mutex_unlock(&g_guard_mutex);
}

}  // extern "C"
