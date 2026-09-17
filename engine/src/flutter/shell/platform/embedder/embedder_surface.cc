// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/embedder/embedder_surface.h"

// sk_sp<GrDirectContext>'s destructor needs the complete type, and the header
// only forward-declares it. Every other embedder target gets the definition
// transitively through a GPU backend header; this port has none enabled
// (skia_enable_ganesh = false), so it has to be named here.
//
// The pointer is always null on this platform -- CreateResourceContext below is
// never overridden, because the GL, Vulkan and Metal surfaces that override it
// are all excluded from a software-only build.
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

namespace flutter {

EmbedderSurface::EmbedderSurface() = default;

EmbedderSurface::~EmbedderSurface() = default;

std::shared_ptr<impeller::Context> EmbedderSurface::CreateImpellerContext()
    const {
  return nullptr;
}

sk_sp<GrDirectContext> EmbedderSurface::CreateResourceContext() const {
  return nullptr;
}

}  // namespace flutter
