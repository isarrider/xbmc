/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

/*!
 * \brief Time Kodi's render thread and, where the GPU driver supports timer queries, the GPU
 * spend on each frame, for the player debug overlay.
 *
 * All functions must be called from the render thread.
 */
namespace KODI::DEBUG_FRAME_TIME
{
struct Stats
{
  int frames{0};
  float averageMs{0.0f}; //!< render thread time per frame
  float maxMs{0.0f};
  float gpuMs{-1.0f}; //!< GPU time per frame, negative if not available
};

//! Measure from the next frame on, or stop and free the GPU queries
void SetEnabled(bool enabled);
//! Call right after the render system began the frame
void BeginFrame();
//! Call right before the render system ends the frame, before it is presented
void EndFrame();
//! The frames since the last call
Stats Take();
} // namespace KODI::DEBUG_FRAME_TIME
