// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/gpu/gpu_surface_software.h"

#include <memory>

#include "flow/surface_frame.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/time/time_point.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace flutter {

GPUSurfaceSoftware::GPUSurfaceSoftware(GPUSurfaceSoftwareDelegate* delegate,
                                       bool render_to_surface)
    : delegate_(delegate),
      render_to_surface_(render_to_surface),
      weak_factory_(this) {}

GPUSurfaceSoftware::~GPUSurfaceSoftware() = default;

// |Surface|
bool GPUSurfaceSoftware::IsValid() {
  return delegate_ != nullptr;
}

// |Surface|
std::unique_ptr<SurfaceFrame> GPUSurfaceSoftware::AcquireFrame(
    const DlISize& logical_size) {
  SurfaceFrame::FramebufferInfo framebuffer_info;
  framebuffer_info.supports_readback = true;

  // TODO(38466): Refactor GPU surface APIs take into account the fact that an
  // external view embedder may want to render to the root surface.
  if (!render_to_surface_) {
    return std::make_unique<SurfaceFrame>(
        nullptr, framebuffer_info,
        [](const SurfaceFrame& surface_frame, DlCanvas* canvas) {
          return true;
        },
        [](const SurfaceFrame& surface_frame) { return true; }, logical_size);
  }

  if (!IsValid()) {
    return nullptr;
  }

  // Partial repaint, when the delegate can promise the buffer survives.
  //
  // `existing_damage` is the area of the buffer we are about to draw into that
  // lags behind what is on screen -- the thing an embedder has to track per
  // back buffer when it is double or triple buffered. A delegate that hands
  // back the same surface every frame has no such lag, so the honest answer is
  // an *empty* rect, not nullopt: nullopt means "unknown", and the rasterizer
  // reads it as "repaint everything".
  if (delegate_ != nullptr && delegate_->BackingStoreRetainsPreviousFrame()) {
    framebuffer_info.supports_partial_repaint = true;
    framebuffer_info.existing_damage = DlIRect();
  }

  // The raster pass starts here, and the timestamp is captured *into the encode
  // callback below* rather than parked in a static.
  //
  // The first version used a file static, on the reasoning that the raster
  // thread runs one frame at a time so nothing could race. That is true and
  // still wrong: AcquireFrame is not guaranteed to be followed by an encode.
  // When a frame is acquired and then dropped, the next frame's encode reads
  // the *previous* frame's start time, and the pass appears to have taken as
  // long as the gap between them. It showed up immediately -- a raster max of
  // 3,023,406 us on a 55 fps window, alongside the two idle gaps that produced
  // it, and a mean of 442% of a frame.
  const fml::TimePoint raster_start = fml::TimePoint::Now();

  const auto size = DlISize(logical_size.width, logical_size.height);

  sk_sp<SkSurface> backing_store = delegate_->AcquireBackingStore(size);

  if (backing_store == nullptr) {
    return nullptr;
  }

  if (size.width != backing_store->width() ||
      size.height != backing_store->height()) {
    return nullptr;
  }

  // If the surface has been scaled, we need to apply the inverse scaling to the
  // underlying canvas so that coordinates are mapped to the same spot
  // irrespective of surface scaling.
  SkCanvas* canvas = backing_store->getCanvas();
  canvas->resetMatrix();

  SurfaceFrame::EncodeCallback encode_callback =
      [self = weak_factory_.GetWeakPtr(), raster_start](
          const SurfaceFrame& surface_frame, DlCanvas* canvas) -> bool {
    // If the surface itself went away, there is nothing more to do.
    if (!self || !self->IsValid() || canvas == nullptr) {
      return false;
    }

    canvas->Flush();

    // After Flush, not before: Flush is where a deferred display list is
    // actually executed, so timing that returns before it measures the
    // bookkeeping and not the painting.
    self->delegate_->OnRasterTiming(static_cast<uint64_t>(
        (fml::TimePoint::Now() - raster_start).ToMicroseconds()));
    return true;
  };
  SurfaceFrame::SubmitCallback submit_callback =
      [self = weak_factory_.GetWeakPtr()](const SurfaceFrame& surface_frame) {
        // If the surface itself went away, there is nothing more to do.
        if (!self || !self->IsValid()) {
          return false;
        }
        // Reported before the present rather than passed through it, so that a
        // delegate which does not care needs no signature change.
        self->delegate_->OnFrameDamage(surface_frame.submit_info().frame_damage);
        return self->delegate_->PresentBackingStore(
            surface_frame.SkiaSurface());
      };

  return std::make_unique<SurfaceFrame>(backing_store, framebuffer_info,
                                        encode_callback, submit_callback,
                                        logical_size);
}

// |Surface|
DlMatrix GPUSurfaceSoftware::GetRootTransformation() const {
  // This backend does not currently support root surface transformations. Just
  // return identity.
  return DlMatrix();
}

// |Surface|
GrDirectContext* GPUSurfaceSoftware::GetContext() {
  // There is no GrContext associated with a software surface.
  return nullptr;
}

}  // namespace flutter
