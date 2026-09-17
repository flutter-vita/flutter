// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/fml/paths.h"

#include <fcntl.h>

#include "flutter/fml/file.h"

namespace fml {
namespace paths {

std::pair<bool, std::string> GetExecutablePath() {
  // There is no /proc/self/exe and no equivalent: an application cannot ask
  // sceIo where its own eboot.bin lives. The path is knowable, but only from
  // the title id, which the engine does not have.
  //
  // Nothing on the software render path needs it. It is used for locating
  // engine artifacts next to the binary on desktop, and this port links its
  // snapshot in rather than loading it from disk (ADR-0005 option A).
  return {false, ""};
}

fml::UniqueFD GetCachesDirectory() {
  // Unlike QNX above, this one is real. The engine uses the caches directory
  // for the persistent shader cache and for anything else it wants to keep
  // between runs, and the Vita has an obvious home for it: an application may
  // always write to ux0:data.
  //
  // Note this is where the FileMapping decision in mapping_posix.cc bites --
  // writable mappings are refused, so the shader cache will not persist even
  // though the directory exists. Returning a real directory is still right:
  // the failure then lands on the one operation that cannot work, rather than
  // on every use of the path.
  return fml::UniqueFD(::open("ux0:data/flutter-vita", O_RDONLY));
}

}  // namespace paths
}  // namespace fml
