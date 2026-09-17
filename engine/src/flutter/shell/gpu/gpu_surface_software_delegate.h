// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_GPU_GPU_SURFACE_SOFTWARE_DELEGATE_H_
#define FLUTTER_SHELL_GPU_GPU_SURFACE_SOFTWARE_DELEGATE_H_

#include <optional>

#include "flutter/display_list/geometry/dl_geometry_types.h"
#include "flutter/flow/embedded_views.h"
#include "flutter/fml/macros.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace flutter {

//------------------------------------------------------------------------------
/// @brief      Interface implemented by all platform surfaces that can present
///             a software backing store to the "screen". The GPU surface
///             abstraction (which abstracts the client rendering API) uses this
///             delegation pattern to tell the platform surface (which abstracts
///             how backing stores fulfilled by the selected client rendering
///             API end up on the "screen" on a particular platform) when the
///             rasterizer needs to allocate and present the software backing
///             store.
///
/// @see        |IOSSurfaceSoftware|, |AndroidSurfaceSoftware|,
///             |EmbedderSurfaceSoftware|.
///
class GPUSurfaceSoftwareDelegate {
 public:
  ~GPUSurfaceSoftwareDelegate();

  //----------------------------------------------------------------------------
  /// @brief      Called when the GPU surface needs a new buffer to render a new
  ///             frame into.
  ///
  /// @param[in]  size  The size of the frame.
  ///
  /// @return     A raster surface returned by the platform.
  ///
  virtual sk_sp<SkSurface> AcquireBackingStore(const DlISize& size) = 0;

  //----------------------------------------------------------------------------
  /// @brief      Called by the platform when a frame has been rendered into the
  ///             backing store and the platform must display it on-screen.
  ///
  /// @param[in]  backing_store  The software backing store to present.
  ///
  /// @return     Returns if the platform could present the backing store onto
  ///             the screen.
  ///
  virtual bool PresentBackingStore(sk_sp<SkSurface> backing_store) = 0;

  //----------------------------------------------------------------------------
  /// @brief      Whether the backing store handed out by |AcquireBackingStore|
  ///             still holds the previous frame when the next one starts.
  ///
  ///             This is the whole precondition for partial repaint. When it
  ///             holds, the rasterizer may clip drawing to the region that
  ///             actually changed and leave the rest of the buffer alone --
  ///             which on a CPU rasterizer is the difference between painting
  ///             a whole screen and painting a cursor. When it does not hold,
  ///             the untouched region is stale garbage and every frame must be
  ///             painted whole.
  ///
  ///             Defaults to false so that a delegate which recycles or
  ///             double-buffers its backing stores keeps the old behaviour
  ///             without having to say anything.
  ///
  virtual bool BackingStoreRetainsPreviousFrame() const { return false; }

  //----------------------------------------------------------------------------
  /// @brief      The region of the backing store that differs from the frame
  ///             already on screen, reported just before |PresentBackingStore|.
  ///
  ///             Only meaningful when |BackingStoreRetainsPreviousFrame| is
  ///             true; otherwise it is nullopt and the whole store is new.
  ///             A delegate that copies the store somewhere else can use this
  ///             to copy less of it.
  ///
  /// @param[in]  damage  The changed region, or nullopt for "all of it".
  ///
  virtual void OnFrameDamage(const std::optional<DlIRect>& damage) {}

  //----------------------------------------------------------------------------
  /// @brief      How long the raster pass took, in microseconds: AcquireFrame
  ///             to the end of the encode callback, which is every pixel this
  ///             frame will contain.
  ///
  ///             Separate from the frame's total, and from the present. Those
  ///             three add up to something an embedder can act on -- a slow
  ///             raster pass argues for a GPU backend, a slow remainder argues
  ///             for the UI thread, and the damage rectangle alone
  ///             distinguishes neither.
  ///
  ///             Called on the raster thread, immediately before OnFrameDamage.
  ///
  virtual void OnRasterTiming(uint64_t raster_us) {}
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_GPU_GPU_SURFACE_SOFTWARE_DELEGATE_H_
