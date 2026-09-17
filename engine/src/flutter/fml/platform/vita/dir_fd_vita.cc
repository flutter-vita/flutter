// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/fml/platform/vita/dir_fd_vita.h"

#include <fcntl.h>
#include <psp2/io/dirent.h>

#include <mutex>
#include <vector>

namespace fml {
namespace {

// Tokens start well above anything newlib will ever hand out, so a token used
// by mistake as a real descriptor fails loudly instead of acting on an
// unrelated open file. newlib's table here is a few dozen entries.
constexpr int kDirFdBase = 0x40000000;

struct Registry {
  std::mutex mutex;
  // Index is the token minus kDirFdBase. An empty string marks a free slot,
  // and freed slots are reused, so a program that opens and closes bundles in
  // a loop does not grow this without bound.
  std::vector<std::string> paths;
};

Registry& GetRegistry() {
  static Registry* registry = new Registry();
  return *registry;
}

// Trailing separators would produce "app0:/dir//file". Harmless on most
// filesystems and not worth relying on here.
std::string StripTrailingSlash(const std::string& path) {
  if (path.size() > 1 && path.back() == '/') {
    return path.substr(0, path.size() - 1);
  }
  return path;
}

bool DirectoryExists(const std::string& path) {
  // sceIoDopen is the only call on this platform that will open a directory,
  // which is precisely why fml's `open(..., O_DIRECTORY)` route cannot work.
  const SceUID handle = sceIoDopen(path.c_str());
  if (handle < 0) {
    return false;
  }
  sceIoDclose(handle);
  return true;
}

}  // namespace

bool IsVitaDirFd(int fd) {
  return fd >= kDirFdBase;
}

int VitaOpenDirFd(const std::string& path) {
  if (!DirectoryExists(path)) {
    return -1;
  }

  const std::string normalised = StripTrailingSlash(path);

  Registry& registry = GetRegistry();
  std::scoped_lock lock(registry.mutex);
  for (size_t i = 0; i < registry.paths.size(); i++) {
    if (registry.paths[i].empty()) {
      registry.paths[i] = normalised;
      return kDirFdBase + static_cast<int>(i);
    }
  }
  registry.paths.push_back(normalised);
  return kDirFdBase + static_cast<int>(registry.paths.size() - 1);
}

void VitaCloseDirFd(int fd) {
  if (!IsVitaDirFd(fd)) {
    return;
  }
  const size_t index = static_cast<size_t>(fd - kDirFdBase);

  Registry& registry = GetRegistry();
  std::scoped_lock lock(registry.mutex);
  if (index < registry.paths.size()) {
    registry.paths[index].clear();
  }
}

std::string VitaResolveAt(int dirfd, const char* path) {
  if (path == nullptr) {
    return {};
  }

  // An absolute path ignores the directory, exactly as openat does. On this
  // platform "absolute" means a device prefix -- `app0:/x`, `ux0:/x` -- rather
  // than a leading slash, so the test is for the colon.
  const std::string relative(path);
  if (relative.find(':') != std::string::npos) {
    return relative;
  }

  if (dirfd == AT_FDCWD) {
    return relative;
  }

  if (!IsVitaDirFd(dirfd)) {
    // A real file descriptor used as a directory. Nothing sensible to do:
    // returning the bare relative path would resolve it against the process
    // cwd, which is the silent wrong answer this whole file exists to avoid.
    return {};
  }

  const size_t index = static_cast<size_t>(dirfd - kDirFdBase);
  Registry& registry = GetRegistry();
  std::scoped_lock lock(registry.mutex);
  if (index >= registry.paths.size() || registry.paths[index].empty()) {
    return {};
  }
  return registry.paths[index] + "/" + relative;
}

}  // namespace fml
