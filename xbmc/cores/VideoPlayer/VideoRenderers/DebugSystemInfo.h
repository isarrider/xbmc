/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "utils/Geometry.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CTexture;

/*!
 * \brief CPU clocks, memory, frame times, GPU and thermal state for the player debug overlay,
 * as a text line and optionally as a history graphic of the frame times.
 *
 * Only what the device lets Kodi read is shown: GPU time needs GL_EXT_disjoint_timer_query,
 * the GPU clock a readable Mali clock file and the thermal state Android 11.
 */
class CDebugSystemInfo
{
public:
  CDebugSystemInfo();
  ~CDebugSystemInfo();

  //! Take a sample if the last one is a second old. Call while the overlay is shown.
  void Update();
  //! e.g. "CPU 1.33 / 2.37 GHz  RAM Kodi 350 MB, free 5.1 GB  Frame 3.2, max 7.9 of 16.7 ms"
  std::string GetSummary() const;
  //! Draw the frame time history at the right edge, below top, at most at maxScale (0 for no
  //! limit) to line up with a graphic above it. Returns its bottom.
  float Render(const CRect& view, float top, float maxScale);
  //! Free the samples, pixels and texture and stop timing frames, e.g. when the overlay is closed
  void Reset();

private:
  struct Sample
  {
    float averageMs;
    float maxMs;
    float gpuMs; //!< negative if not available
    float busyPercent; //!< share of the second the render thread spent on frames
    float gpuPercent; //!< share of the second the GPU spent on Kodi's frames, negative if unknown
  };

  void ReadClusters();
  void Draw(float scale);

  std::chrono::steady_clock::time_point m_lastSample;
  std::vector<std::vector<int>> m_clusters; //!< cores that share a clock
  bool m_clustersRead{false};
  std::vector<int> m_clockKHz; //!< per cluster
  uint64_t m_kodiBytes{0};
  uint64_t m_availableBytes{0};
  int m_gpuMHz{-1};
  int m_thermalStatus{-1};
  float m_thermalHeadroom{-1.0f};
  float m_budgetMs{0.0f};
  Sample m_current{0.0f, 0.0f, -1.0f, 0.0f, -1.0f};
  //! Oldest first, at most one minute
  std::vector<Sample> m_history;
  bool m_changed{false};

  struct Thermal;
  std::unique_ptr<Thermal> m_thermal;

  unsigned int m_width{0};
  unsigned int m_height{0};
  std::vector<uint32_t> m_pixels;
  std::unique_ptr<CTexture> m_texture;
};
