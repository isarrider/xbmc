/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DebugProbe.h"

#include "ServiceBroker.h"
#include "rendering/RenderSystem.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#if defined(TARGET_POSIX)
#include <dirent.h>
#include <dlfcn.h>
#endif

namespace
{
constexpr const char* PREFIX = "DebugProbe:";

void Probe(const std::string& path)
{
  std::ifstream file(path);
  if (!file)
  {
    // ENOENT means the device doesn't have it, EACCES that Android blocks it
    CLog::Log(LOGINFO, "{} {} -> not readable ({})", PREFIX, path, std::strerror(errno));
    return;
  }
  std::string line;
  std::getline(file, line);
  if (!file && !file.eof())
  {
    CLog::Log(LOGINFO, "{} {} -> opened, read failed ({})", PREFIX, path, std::strerror(errno));
    return;
  }
  StringUtils::Trim(line);
  if (line.size() > 120)
    line = line.substr(0, 120) + "...";
  CLog::Log(LOGINFO, "{} {} -> \"{}\"", PREFIX, path, line);
}

#if defined(TARGET_POSIX)
//! Names in dir that contain one of the filters (all names if empty), sorted
std::vector<std::string> List(const std::string& dir, const std::vector<std::string>& filters)
{
  std::vector<std::string> names;
  DIR* d = opendir(dir.c_str());
  if (!d)
  {
    CLog::Log(LOGINFO, "{} {}/ -> not listable ({})", PREFIX, dir, std::strerror(errno));
    return names;
  }
  while (const dirent* entry = readdir(d))
  {
    const std::string name = entry->d_name;
    if (name == "." || name == "..")
      continue;
    if (filters.empty() || std::any_of(filters.begin(), filters.end(), [&](const auto& f)
                                       { return StringUtils::Contains(name, f); }))
      names.push_back(name);
  }
  closedir(d);
  std::sort(names.begin(), names.end());
  CLog::Log(LOGINFO, "{} {}/ -> {}", PREFIX, dir,
            names.empty() ? "(none)" : StringUtils::Join(names, " "));
  return names;
}

void ProbeAndroidThermal()
{
  void* lib = dlopen("libandroid.so", RTLD_NOW);
  if (!lib)
  {
    CLog::Log(LOGINFO, "{} libandroid.so -> not loadable", PREFIX);
    return;
  }
  using Acquire = void* (*)();
  using Status = int (*)(void*);
  using Headroom = float (*)(void*, int);
  using Release = void (*)(void*);
  const auto acquire = reinterpret_cast<Acquire>(dlsym(lib, "AThermal_acquireManager"));
  const auto status = reinterpret_cast<Status>(dlsym(lib, "AThermal_getCurrentThermalStatus"));
  const auto headroom = reinterpret_cast<Headroom>(dlsym(lib, "AThermal_getThermalHeadroom"));
  const auto release = reinterpret_cast<Release>(dlsym(lib, "AThermal_releaseManager"));
  if (!acquire || !status || !release)
  {
    CLog::Log(LOGINFO, "{} AThermal -> not available (needs Android 11)", PREFIX);
    dlclose(lib);
    return;
  }
  if (void* manager = acquire())
  {
    CLog::Log(LOGINFO, "{} AThermal status -> {} (0 none ... 6 shutdown)", PREFIX, status(manager));
    if (headroom)
      CLog::Log(LOGINFO, "{} AThermal headroom -> {:.2f} (1.0 = throttling)", PREFIX,
                headroom(manager, 0));
    else
      CLog::Log(LOGINFO, "{} AThermal headroom -> not available (needs Android 12)", PREFIX);
    release(manager);
  }
  dlclose(lib);
}
#endif
} // namespace

void KODI::DEBUGPROBE::LogOnce()
{
  static std::once_flag once;
  std::call_once(
      once,
      []
      {
        CLog::Log(LOGINFO, "{} start", PREFIX);

        // GPU time per frame needs a timer query extension
        if (const auto renderSystem = CServiceBroker::GetRenderSystem())
        {
          CLog::Log(LOGINFO, "{} renderer -> {} / {}", PREFIX, renderSystem->GetRenderRenderer(),
                    renderSystem->GetRenderVersionString());
          for (const char* ext :
               {"GL_EXT_disjoint_timer_query", "GL_EXT_disjoint_timer_query_webgl2",
                "GL_ARB_timer_query", "GL_EXT_timer_query"})
            CLog::Log(LOGINFO, "{} {} -> {}", PREFIX, ext,
                      renderSystem->IsExtSupported(ext) ? "yes" : "no");
        }

#if defined(TARGET_POSIX)
        // GPU load and clock: Qualcomm Adreno, Arm Mali, MediaTek, generic devfreq
        for (const char* path :
             {"/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage", "/sys/class/kgsl/kgsl-3d0/gpubusy",
              "/sys/class/kgsl/kgsl-3d0/gpuclk", "/sys/class/misc/mali0/device/utilization",
              "/sys/class/misc/mali0/device/utilisation",
              "/sys/class/misc/mali0/device/gpu_utilization",
              "/sys/class/misc/mali0/device/cur_freq", "/sys/class/misc/mali0/device/clock",
              "/sys/kernel/gpu/gpu_busy", "/sys/kernel/gpu/gpu_clock", "/sys/kernel/gpu/gpu_model",
              "/sys/kernel/ged/hal/gpu_utilization", "/sys/kernel/ged/hal/current_freqency",
              "/proc/mali/utilization", "/proc/gpufreq/gpufreq_var_dump",
              "/sys/class/drm/card0/device/gpu_busy_percent"})
          Probe(path);
        for (const auto& name : List("/sys/devices/platform", {"mali", "gpu"}))
          for (const char* file : {"utilisation", "utilization", "cur_freq", "gpu_busy"})
            Probe("/sys/devices/platform/" + name + "/" + file);
        for (const auto& name : List("/sys/class/devfreq", {}))
          for (const char* file : {"cur_freq", "load", "governor"})
            Probe("/sys/class/devfreq/" + name + "/" + file);

        // CPU clock per core
        for (const auto& name : List("/sys/devices/system/cpu", {"cpu"}))
        {
          if (name.size() < 4 || !StringUtils::IsNaturalNumber(name.substr(3)))
            continue;
          for (const char* file : {"scaling_cur_freq", "cpuinfo_cur_freq", "scaling_max_freq"})
            Probe("/sys/devices/system/cpu/" + name + "/cpufreq/" + file);
        }

        // Temperatures and throttling
        int zones = 0;
        for (const auto& name : List("/sys/class/thermal", {"thermal_zone"}))
        {
          if (++zones > 40)
            break;
          Probe("/sys/class/thermal/" + name + "/type");
          Probe("/sys/class/thermal/" + name + "/temp");
        }
#if defined(TARGET_ANDROID)
        ProbeAndroidThermal();
#endif

        // Memory
        Probe("/proc/self/statm");
        Probe("/proc/meminfo");
#endif

        CLog::Log(LOGINFO, "{} end", PREFIX);
      });
}
