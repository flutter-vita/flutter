// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_SOFTWARE_FRAME_DAMAGE_H_
#define FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_SOFTWARE_FRAME_DAMAGE_H_

#include <cstdint>
#include <functional>

namespace flutter {

//------------------------------------------------------------------------------
/// @brief      Where the software surface reports frame damage, for an embedder
///             that can copy less than a whole screen.
///
///             This is a global rather than a member of
///             FlutterSoftwareRendererConfig, and the reason is build cost, not
///             taste. That struct lives in embedder.h, which is included --
///             directly or otherwise -- by essentially every object in the
///             engine; adding a field to it invalidates around two thousand of
///             them and silently changes a struct size all of them must agree
///             on. This fork has exactly one software embedder, so a hook it
///             installs at startup buys the same thing for two objects.
///
///             The declaration lives in a header of its own, with no Skia in
///             it, because the embedder that installs the hook is built without
///             Skia's include directories -- it only ever sees embedder.h.
///
///             Called on the raster thread immediately before the present
///             callback, with the region that differs from the frame already on
///             screen. `full` means the damage is unknown and all of it
///             changed; the four coordinates are then meaningless.
///
using SoftwareFrameDamageObserver = std::function<
    void(int32_t left, int32_t top, int32_t right, int32_t bottom, bool full)>;

void SetSoftwareFrameDamageObserver(SoftwareFrameDamageObserver observer);

//------------------------------------------------------------------------------
/// @brief      How long Skia spent turning the layer tree into pixels, as
///             distinct from how long the frame took.
///
///             The frame-to-frame gap already measured in the embedder answers
///             "how fast is this app", and the blit timer answers "how much of
///             that is the present memcpy". Neither separates the two things
///             that make up the rest: the UI thread building and laying out,
///             and the raster thread painting. Those want opposite fixes -- a
///             GPU backend does nothing for the first and everything for the
///             second -- so the number that decides between them has to be
///             measured, not inferred from the damage rectangle.
///
///             Inferring it from damage is exactly what failed. localsend's
///             Receive screen runs at 4.7 fps having damaged 13% of the
///             screen, while flutter_manga_reader holds 48 fps at 89%: seven
///             times the area for ten times the frame rate. `dmg%` does not
///             predict cost, because it says how much area changed and nothing
///             about what is inside it.
///
///             Timed from `AcquireFrame` to the end of the encode callback,
///             which is where `DlCanvas::Flush` returns and every pixel the
///             frame will contain has been written. Called on the raster
///             thread, immediately before the damage observer above.
///
using SoftwareRasterTimingObserver = std::function<void(uint64_t raster_us)>;

void SetSoftwareRasterTimingObserver(SoftwareRasterTimingObserver observer);

}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_EMBEDDER_EMBEDDER_SOFTWARE_FRAME_DAMAGE_H_
