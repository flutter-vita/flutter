// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file adds defines about the platform we're currently building on.
//  Operating System:
//    FML_OS_WIN / FML_OS_MACOSX / FML_OS_LINUX / FML_OS_POSIX (MACOSX or LINUX)
//    / FML_OS_NACL (NACL_SFI or NACL_NONSFI) / FML_OS_NACL_SFI /
//    FML_OS_NACL_NONSFI
//  Compiler:
//    COMPILER_MSVC / COMPILER_GCC
//  Processor:
//    FML_ARCH_CPU_X86 / FML_ARCH_CPU_X86_64 / FML_ARCH_CPU_X86_FAMILY (X86 or
//    X86_64) FML_ARCH_CPU_32_BITS / FML_ARCH_CPU_64_BITS

#ifndef FLUTTER_FML_BUILD_CONFIG_H_
#define FLUTTER_FML_BUILD_CONFIG_H_

#if defined(__Fuchsia__)
#define OS_FUCHSIA 1
#elif defined(ANDROID)
#define FML_OS_ANDROID 1
#elif defined(__APPLE__)
// only include TargetConditions after testing ANDROID as some android builds
// on mac don't have this header available and it's not needed unless the target
// is really mac/ios.
#include <TargetConditionals.h>
#define FML_OS_MACOSX 1
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define FML_OS_IOS 1
#endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#if defined(TARGET_IPHONE_SIMULATOR) && TARGET_IPHONE_SIMULATOR
#define FML_OS_IOS_SIMULATOR 1
#endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#elif defined(__linux__)
#define FML_OS_LINUX 1
// include a system header to pull in features.h for glibc/uclibc macros.
#include <unistd.h>
#if defined(__GLIBC__) && !defined(__UCLIBC__)
// we really are using glibc, not uClibc pretending to be glibc
#define LIBC_GLIBC 1
#endif
#elif defined(_WIN32)
#define FML_OS_WIN 1
#elif defined(__FreeBSD__)
#define FML_OS_FREEBSD 1
#elif defined(__OpenBSD__)
#define FML_OS_OPENBSD 1
#elif defined(__sun)
#define FML_OS_SOLARIS 1
#elif defined(__QNXNTO__)
#define FML_OS_QNX 1
#elif defined(__EMSCRIPTEN__)
#define FML_OS_EMSCRIPTEN
#elif defined(__vita__)
// PlayStation Vita (VitaSDK, newlib). Note this must come after the __linux__
// arm above rather than being folded into it: VitaSDK's GCC defines neither
// __linux__ nor __unix__, only __vita__.
#define FML_OS_VITA 1
#else
#error Please add support for your platform in flutter/fml/build_config.h
#endif

// For access to standard BSD features, use FML_OS_BSD instead of a
// more specific macro.
#if defined(FML_OS_FREEBSD) || defined(FML_OS_OPENBSD)
#define FML_OS_BSD 1
#endif

// For access to standard POSIXish features, use FML_OS_POSIX instead of a
// more specific macro.
// The Vita is in this list for the same reason its sources come from
// fml/platform/posix: newlib gives it open/read/close, pthreads and
// pthread_once, which is all FML_OS_POSIX gates. It is emphatically not a claim
// that the platform is POSIX -- there is no mmap, no dlopen and no fork, and
// each of those is handled where it is used.
#if defined(FML_OS_MACOSX) || defined(FML_OS_LINUX) ||    \
    defined(FML_OS_FREEBSD) || defined(FML_OS_OPENBSD) || \
    defined(FML_OS_SOLARIS) || defined(FML_OS_ANDROID) || \
    defined(FML_OS_NACL) || defined(FML_OS_QNX) ||        \
    defined(FML_OS_VITA)
#define FML_OS_POSIX 1
#endif

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#define FML_ARCH_CPU_X86_FAMILY 1
#define FML_ARCH_CPU_X86_64 1
#define FML_ARCH_CPU_64_BITS 1
#define FML_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(_M_IX86) || defined(__i386__)
#define FML_ARCH_CPU_X86_FAMILY 1
#define FML_ARCH_CPU_X86 1
#define FML_ARCH_CPU_32_BITS 1
#define FML_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__ARMEL__)
#define FML_ARCH_CPU_ARM_FAMILY 1
#define FML_ARCH_CPU_ARMEL 1
#define FML_ARCH_CPU_32_BITS 1
#define FML_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__aarch64__)
#define FML_ARCH_CPU_ARM_FAMILY 1
#define FML_ARCH_CPU_ARM64 1
#define FML_ARCH_CPU_64_BITS 1
#define FML_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__riscv) && __SIZEOF_POINTER__ == 4
#define FML_ARCH_CPU_RISCV_FAMILY 1
#define FML_ARCH_CPU_RISCV32 1
#define FML_ARCH_CPU_32_BITS 1
#define FML_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__riscv) && __SIZEOF_POINTER__ == 8
#define FML_ARCH_CPU_RISCV_FAMILY 1
#define FML_ARCH_CPU_RISCV64 1
#define FML_ARCH_CPU_64_BITS 1
#define FML_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__pnacl__)
#define FML_ARCH_CPU_32_BITS 1
#define FML_ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__EMSCRIPTEN__)
#define FML_ARGH_CPU_32_BITS 1
#define FML_ARCH_CPU_LITTLE_ENDIAN 1
#else
#error Please add support for your architecture in flutter/fml/build_config.h
#endif

#endif  // FLUTTER_FML_BUILD_CONFIG_H_
