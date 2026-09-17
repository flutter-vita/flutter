// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FML_PLATFORM_VITA_DIR_FD_VITA_H_
#define FLUTTER_FML_PLATFORM_VITA_DIR_FD_VITA_H_

#include <string>

// Directory descriptors for a platform that has none.
//
// fml addresses files as (directory fd, relative path) and calls the POSIX
// *at* family to resolve them -- `openat`, `mkdirat`, `unlinkat`, `faccessat`,
// `renameat`. The Vita cannot express that. sceIo has no notion of a
// descriptor that names a directory: `open()` fails on one outright, and
// directories are walked with a separate sceIoDopen handle that no *at* call
// accepts. `vita_posix_shims.cc` therefore rejects any dirfd other than
// AT_FDCWD, deliberately, on the grounds that resolving against the process
// cwd would silently produce the wrong path.
//
// That is right for `dart:io` and fatal for assets. `DirectoryAssetBundle`
// opens `app0:/flutter_assets` and then asks for every asset relative to it,
// so with no dirfd support **no asset loads at all** -- the symptom was a
// counter app rendering in Roboto with a tofu box where its icon should be,
// and a log line saying FontManifest.json could not be found while the file
// sat on the device, intact, at the path the bundle was looking in.
//
// The registry gives back the missing concept: a directory fd is a token this
// layer hands out, and every *at* call resolves it to a path prefix before
// falling through to the ordinary sceIo-backed call. Tokens come from a
// private high range so they can never be confused with a real descriptor, and
// they are released through fml::UniqueFD's own closer, which is the single
// point every directory fd passes through on its way out.
//
// **File** descriptors are untouched. Once a path is resolved, opening it
// returns a real fd from newlib, which is what mapping and reading need.

namespace fml {

// True if `fd` is one of ours rather than a descriptor from the C library.
bool IsVitaDirFd(int fd);

// Take a directory path and return a token for it, or -1 if the directory does
// not exist. The path is stored normalised without a trailing separator.
int VitaOpenDirFd(const std::string& path);

// Release a token. Safe to call with anything -- a real fd, an already-freed
// token, or -1 -- so the UniqueFD closer can call it unconditionally.
void VitaCloseDirFd(int fd);

// Resolve (dirfd, path) to an absolute path.
//
// Returns `path` unchanged when it is already absolute or when `dirfd` is
// AT_FDCWD, which keeps the AT_FDCWD callers behaving exactly as before.
// Returns an empty string when `dirfd` is a token this registry does not know,
// so callers fail rather than reaching for a path built from garbage.
std::string VitaResolveAt(int dirfd, const char* path);

}  // namespace fml

#endif  // FLUTTER_FML_PLATFORM_VITA_DIR_FD_VITA_H_
