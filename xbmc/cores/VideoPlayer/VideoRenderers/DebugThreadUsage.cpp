/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DebugThreadUsage.h"

#include "utils/StringUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

#if defined(TARGET_LINUX) || defined(TARGET_ANDROID)
#include <dirent.h>
#include <unistd.h>
#endif

namespace
{
constexpr auto UPDATE_INTERVAL = std::chrono::seconds(1);

struct ThreadGroup
{
  std::string name;
  uint64_t ticks{0};
  int count{0};
};
} // namespace

const std::string& CDebugThreadUsage::GetSummary(int maxEntries)
{
  if (std::chrono::steady_clock::now() - m_lastRead >= UPDATE_INTERVAL)
    Update(static_cast<size_t>(maxEntries));

  return m_summary;
}

void CDebugThreadUsage::Update(size_t maxEntries)
{
#if defined(TARGET_LINUX) || defined(TARGET_ANDROID)
  DIR* dir = opendir("/proc/self/task");
  if (!dir)
    return;

  const auto now = std::chrono::steady_clock::now();
  const bool haveBaseline = m_lastRead != std::chrono::steady_clock::time_point{};
  const double elapsed = std::chrono::duration<double>(now - m_lastRead).count();

  std::unordered_map<int, uint64_t> threadTicks;
  std::vector<ThreadGroup> groups;

  while (const dirent* entry = readdir(dir))
  {
    if (entry->d_name[0] == '.')
      continue;

    std::ifstream file(std::string("/proc/self/task/") + entry->d_name + "/stat");
    std::string line;
    if (!std::getline(file, line))
      continue;

    // The thread name (field 2) is in parentheses and may itself contain them
    const auto open = line.find('(');
    const auto close = line.rfind(')');
    if (open == std::string::npos || close == std::string::npos || close < open)
      continue;

    std::istringstream ss(line.substr(close + 1));
    std::string field;
    uint64_t ticks = 0;
    for (int i = 3; i <= 15 && ss >> field; i++)
    {
      if (i == 14 || i == 15) // utime, stime
        ticks += std::strtoull(field.c_str(), nullptr, 10);
    }

    const int tid = std::atoi(entry->d_name);
    threadTicks[tid] = ticks;

    // A thread without a previous sample started within the interval
    const auto prev = m_threadTicks.find(tid);
    const uint64_t delta =
        (prev != m_threadTicks.end() && prev->second <= ticks) ? ticks - prev->second : ticks;

    const std::string name = line.substr(open + 1, close - open - 1);
    auto group = std::find_if(groups.begin(), groups.end(),
                              [&name](const ThreadGroup& g) { return g.name == name; });
    if (group == groups.end())
    {
      groups.push_back({name, 0, 0});
      group = groups.end() - 1;
    }
    group->ticks += delta;
    group->count++;
  }
  closedir(dir);

  m_threadTicks = std::move(threadTicks);
  m_lastRead = now;

  // The first read only establishes a baseline
  const double elapsedTicks = elapsed * static_cast<double>(sysconf(_SC_CLK_TCK));
  if (!haveBaseline || elapsedTicks <= 0)
    return;

  std::sort(groups.begin(), groups.end(),
            [](const ThreadGroup& a, const ThreadGroup& b) { return a.ticks > b.ticks; });

  std::vector<std::string> entries;
  for (const auto& group : groups)
  {
    const long percent = std::lround(static_cast<double>(group.ticks) * 100.0 / elapsedTicks);
    if (entries.size() >= maxEntries || percent < 1)
      break;

    if (group.count > 1)
      entries.push_back(StringUtils::Format("{} x{} {}%", group.name, group.count, percent));
    else
      entries.push_back(StringUtils::Format("{} {}%", group.name, percent));
  }
  m_summary = StringUtils::Join(entries, ", ");
#endif
}
