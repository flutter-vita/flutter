// Copyright (c) 2026, the Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The M2 gate: does the Flutter engine link for arm-vita-eabi with no
// unresolved symbols?
//
// This exists because the obvious target cannot answer that question. The
// engine's own `flutter_engine_library` is a `shared_library`, and on this
// platform that fails twice over: VitaSDK's crt0.o and crtbegin.o are not PIC
// (correctly -- homebrew is statically linked), and a shared link *tolerates*
// undefined symbols by default, so even succeeding would prove nothing.
//
// An executable is the honest test. The linker must resolve every symbol
// reachable from the entry points below, which is what "the engine links"
// means.
//
// It is deliberately not a working embedder. It never runs -- there is no
// eboot.bin here and nothing calls into it. Its whole job is to name enough of
// the public API that the object closure gets pulled in and checked. Phase 3's
// real embedder replaces it.

#include <cstdio>

#include "flutter/shell/platform/embedder/embedder.h"

int main(int argc, char** argv) {
  // Referencing the struct sizes forces the headers to be consistent with the
  // objects, and referencing the functions forces the objects to link.
  FlutterRendererConfig config = {};
  config.type = kSoftware;

  FlutterProjectArgs args = {};
  args.struct_size = sizeof(FlutterProjectArgs);

  FlutterEngine engine = nullptr;

  // The four calls a real embedder cannot do without. Between them they reach
  // the shell, the Dart runtime, dart:io, Skia's software path, txt, and fml.
  volatile FlutterEngineResult result = kSuccess;
  result = FlutterEngineRun(FLUTTER_ENGINE_VERSION, &config, &args, nullptr,
                            &engine);
  result = FlutterEngineSendWindowMetricsEvent(engine, nullptr);
  result = FlutterEngineSendPointerEvent(engine, nullptr, 0);
  result = FlutterEngineShutdown(engine);

  std::printf("link check: %d\n", static_cast<int>(result));
  return 0;
}
