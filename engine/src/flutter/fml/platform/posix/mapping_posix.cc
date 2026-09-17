// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/fml/mapping.h"

#include <fcntl.h>
#if !defined(__vita__)
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <unistd.h>

#include <cstdlib>
#include <type_traits>

#include "flutter/fml/build_config.h"
#include "flutter/fml/eintr_wrapper.h"
#include "flutter/fml/unique_fd.h"

namespace fml {

#if !defined(FML_OS_VITA)
static int ToPosixProtectionFlags(
    std::initializer_list<FileMapping::Protection> protection_flags) {
  int flags = 0;
  for (auto protection : protection_flags) {
    switch (protection) {
      case FileMapping::Protection::kRead:
        flags |= PROT_READ;
        break;
      case FileMapping::Protection::kWrite:
        flags |= PROT_WRITE;
        break;
      case FileMapping::Protection::kExecute:
        // Do not combine write flags with execute flags.
        return PROT_READ | PROT_EXEC;
    }
  }
  return flags;
}
#endif  // !defined(FML_OS_VITA)

static bool IsWritable(
    std::initializer_list<FileMapping::Protection> protection_flags) {
  for (auto protection : protection_flags) {
    if (protection == FileMapping::Protection::kWrite) {
      return true;
    }
  }
  return false;
}

Mapping::Mapping() = default;

Mapping::~Mapping() = default;

#if defined(FML_OS_VITA)

// The Vita has no mmap. newlib's file IO sits on sceIo, which offers no mapping
// call at any level, so a mapping here is a heap copy of the file.
//
// Read-only mappings are faithful: callers only ever read through GetMapping(),
// and the copy is freed with the FileMapping exactly as a real mapping would be
// unmapped. What differs is that nothing is lazily paged and nothing is shared
// between two mappings of the same file -- both cost memory on a device whose
// largest contiguous allocation measured 188 MiB.
//
// **Writable mappings are refused**, rather than emulated. MAP_SHARED writes
// go back to the file; a heap copy's do not, and there is no honest way to know
// when to write it back. Emulating one would mean the persistent shader cache
// appearing to work and silently discarding everything. Callers check
// IsValid(), so refusing here degrades a feature instead of corrupting it.

FileMapping::FileMapping(const fml::UniqueFD& handle,
                         std::initializer_list<Protection> protection) {
  if (!handle.is_valid()) {
    return;
  }

  struct stat stat_buffer = {};

  if (::fstat(handle.get(), &stat_buffer) != 0) {
    return;
  }

  if (stat_buffer.st_size == 0) {
    valid_ = true;
    return;
  }

  if (IsWritable(protection)) {
    return;
  }

  const size_t size = static_cast<size_t>(stat_buffer.st_size);
  auto* buffer = static_cast<uint8_t*>(std::malloc(size));
  if (buffer == nullptr) {
    return;
  }

  // mmap does not consume the descriptor's file position and neither may this;
  // the caller still owns the fd and may read from it afterwards.
  const off_t saved = ::lseek(handle.get(), 0, SEEK_CUR);
  if (saved < 0 || ::lseek(handle.get(), 0, SEEK_SET) < 0) {
    std::free(buffer);
    return;
  }

  size_t got = 0;
  while (got < size) {
    const ssize_t n =
        FML_HANDLE_EINTR(::read(handle.get(), buffer + got, size - got));
    if (n <= 0) {
      break;
    }
    got += static_cast<size_t>(n);
  }
  ::lseek(handle.get(), saved, SEEK_SET);

  if (got != size) {
    std::free(buffer);
    return;
  }

  mapping_ = buffer;
  size_ = size;
  valid_ = true;
}

FileMapping::~FileMapping() {
  std::free(mapping_);
}

#else

FileMapping::FileMapping(const fml::UniqueFD& handle,
                         std::initializer_list<Protection> protection) {
  if (!handle.is_valid()) {
    return;
  }

  struct stat stat_buffer = {};

  if (::fstat(handle.get(), &stat_buffer) != 0) {
    return;
  }

  if (stat_buffer.st_size == 0) {
    valid_ = true;
    return;
  }

  const auto is_writable = IsWritable(protection);

  auto* mapping =
      ::mmap(nullptr, stat_buffer.st_size, ToPosixProtectionFlags(protection),
             is_writable ? MAP_SHARED : MAP_PRIVATE, handle.get(), 0);

  if (mapping == MAP_FAILED) {
    return;
  }

  mapping_ = static_cast<uint8_t*>(mapping);
  size_ = stat_buffer.st_size;
  valid_ = true;
  if (is_writable) {
    mutable_mapping_ = mapping_;
  }
}

FileMapping::~FileMapping() {
  if (mapping_ != nullptr) {
    ::munmap(mapping_, size_);
  }
}

#endif  // defined(FML_OS_VITA)

size_t FileMapping::GetSize() const {
  return size_;
}

const uint8_t* FileMapping::GetMapping() const {
  return mapping_;
}

bool FileMapping::IsDontNeedSafe() const {
  return mutable_mapping_ == nullptr;
}

bool FileMapping::IsValid() const {
  return valid_;
}

}  // namespace fml
