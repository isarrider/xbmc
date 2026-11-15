/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

/*!
 * \brief CPU usage of Kodi's busiest threads for the player debug overlay.
 *
 * Threads with the same name, e.g. a decoder and its FFmpeg worker threads,
 * are combined. Only available where /proc/self/task exists (Linux, Android).
 */
class CDebugThreadUsage
{
public:
  //! e.g. "VideoPlayerVide x13 182%, ActiveAE 9%", empty if not available
  const std::string& GetSummary(int maxEntries);

private:
  void Update(size_t maxEntries);

  std::chrono::steady_clock::time_point m_lastRead;
  std::unordered_map<int, uint64_t> m_threadTicks;
  std::string m_summary;
};
