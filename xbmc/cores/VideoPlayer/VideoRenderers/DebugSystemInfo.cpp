/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DebugSystemInfo.h"

#include "DebugFrameTime.h"
#include "ServiceBroker.h"
#include "guilib/GUITexture.h"
#include "guilib/Texture.h"
#include "guilib/TextureFormats.h"
#include "utils/StringUtils.h"
#include "windowing/GraphicContext.h"
#include "windowing/WinSystem.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string_view>

#if defined(TARGET_POSIX)
#include <unistd.h>
#endif
#if defined(TARGET_ANDROID)
#include <dlfcn.h>
#endif

namespace
{
constexpr auto SAMPLE_INTERVAL = std::chrono::seconds(1);
// Longer gaps mean the overlay was hidden, so the history would have a hole
constexpr auto MAX_SAMPLE_GAP = std::chrono::seconds(3);
constexpr size_t HISTORY_SAMPLES = 60;

constexpr uint32_t COLOR_BACKDROP = 0xAA000000;
constexpr uint32_t COLOR_TRACK = 0x26FFFFFF;
constexpr uint32_t COLOR_TEXT = 0xFFE6E6E6;
constexpr uint32_t COLOR_LOW = 0xFF3DDC84;
constexpr uint32_t COLOR_MEDIUM = 0xFFFFB547;
constexpr uint32_t COLOR_HIGH = 0xFFE5484D;
constexpr uint32_t COLOR_LINE = 0xE6FFFFFF;
constexpr uint32_t COLOR_GPU = 0xFF3F8CFF;
constexpr uint32_t COLOR_MAX = 0x30FFFFFF;

// ASS override colours (&HBBGGRR&) of the same green, amber and red
constexpr std::string_view ASS_LOW = "{\\c&H84DC3D&}";
constexpr std::string_view ASS_MEDIUM = "{\\c&H47B5FF&}";
constexpr std::string_view ASS_HIGH = "{\\c&H4D48E5&}";
constexpr std::string_view ASS_RESET = "{\\c}";

// Sizes for a 1080 pixel high screen
constexpr float MARGIN = 8.0f;
constexpr float PADDING = 12.0f;
// Laid out like the CPU cores history above it, two rows high
constexpr float PLOT_WIDTH = 300.0f;
constexpr float PLOT_HEIGHT = 96.0f;
constexpr float TOTAL_WIDTH = 30.0f;

//! 5x7 pixel digits and '.', one row per entry, bit 4 is the left column
constexpr std::string_view GLYPH_CHARS = "0123456789.";
constexpr int GLYPH_WIDTH = 5;
constexpr int GLYPH_HEIGHT = 7;
constexpr std::array<std::array<uint8_t, GLYPH_HEIGHT>, 11> GLYPHS = {{
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C},
}};
constexpr float FONT = 2.0f;
constexpr float LABEL_WIDTH = 3 * (GLYPH_WIDTH + 1) * FONT;

uint32_t Blend(uint32_t dst, uint32_t src)
{
  const uint32_t a = src >> 24;
  if (a == 0xFF)
    return src;

  const uint32_t da = dst >> 24;
  const uint32_t outA = a + da * (255 - a) / 255;
  if (outA == 0)
    return 0;

  uint32_t out = outA << 24;
  for (int shift = 0; shift < 24; shift += 8)
  {
    const uint32_t s = (src >> shift) & 0xFF;
    const uint32_t d = (dst >> shift) & 0xFF;
    out |= ((s * a + d * da * (255 - a) / 255) / outA) << shift;
  }
  return out;
}

//! Share of the frame budget: up to 60% is fine, up to 90% tight, above that frames drop
uint32_t BudgetColor(float ms, float budgetMs)
{
  if (ms > budgetMs * 0.9f)
    return COLOR_HIGH;
  return ms > budgetMs * 0.6f ? COLOR_MEDIUM : COLOR_LOW;
}

std::string_view BudgetAss(float ms, float budgetMs)
{
  const uint32_t color = BudgetColor(ms, budgetMs);
  return color == COLOR_HIGH ? ASS_HIGH : color == COLOR_MEDIUM ? ASS_MEDIUM : ASS_LOW;
}

std::string ReadLine(const std::string& path)
{
  std::ifstream file(path);
  std::string line;
  std::getline(file, line);
  return line;
}

std::string FormatBytes(uint64_t bytes)
{
  const double mb = static_cast<double>(bytes) / (1024 * 1024);
  return mb < 1024 ? StringUtils::Format("{:.0f} MB", mb)
                   : StringUtils::Format("{:.1f} GB", mb / 1024);
}
} // namespace

struct CDebugSystemInfo::Thermal
{
#if defined(TARGET_ANDROID)
  using Acquire = void* (*)();
  using Status = int (*)(void*);
  using Headroom = float (*)(void*, int);
  using Release = void (*)(void*);

  void* lib = nullptr;
  void* manager = nullptr;
  Status status = nullptr;
  Headroom headroom = nullptr;
  Release release = nullptr;

  // The thermal API needs Android 11 (headroom Android 12) and is not in the NDK Kodi targets
  Thermal()
  {
    lib = dlopen("libandroid.so", RTLD_NOW);
    if (!lib)
      return;
    const auto acquire = reinterpret_cast<Acquire>(dlsym(lib, "AThermal_acquireManager"));
    status = reinterpret_cast<Status>(dlsym(lib, "AThermal_getCurrentThermalStatus"));
    headroom = reinterpret_cast<Headroom>(dlsym(lib, "AThermal_getThermalHeadroom"));
    release = reinterpret_cast<Release>(dlsym(lib, "AThermal_releaseManager"));
    if (acquire && status && release)
      manager = acquire();
  }
  ~Thermal()
  {
    if (manager)
      release(manager);
    if (lib)
      dlclose(lib);
  }
#endif
};

CDebugSystemInfo::CDebugSystemInfo() = default;

CDebugSystemInfo::~CDebugSystemInfo()
{
  Reset();
}

void CDebugSystemInfo::ReadClusters()
{
  m_clustersRead = true;
#if defined(TARGET_POSIX)
  // Cores with the same cpufreq policy run at the same clock
  std::map<std::string, std::vector<int>> policies;
  for (int core = 0; core < 64; ++core)
  {
    const std::string path = StringUtils::Format("/sys/devices/system/cpu/cpu{}/cpufreq", core);
    if (ReadLine(path + "/scaling_cur_freq").empty())
    {
      if (core > 0 &&
          access(StringUtils::Format("/sys/devices/system/cpu/cpu{}", core).c_str(), F_OK) != 0)
        break;
      continue;
    }
    char resolved[PATH_MAX];
    const std::string policy =
        realpath(path.c_str(), resolved) ? std::string(resolved) : std::to_string(core);
    policies[policy].push_back(core);
  }
  for (auto& [policy, cores] : policies)
    m_clusters.push_back(std::move(cores));
  std::sort(m_clusters.begin(), m_clusters.end());
#endif
}

void CDebugSystemInfo::Update()
{
  KODI::DEBUG_FRAME_TIME::SetEnabled(true);

  const auto now = std::chrono::steady_clock::now();
  if (now - m_lastSample < SAMPLE_INTERVAL)
    return;
  if (now - m_lastSample > MAX_SAMPLE_GAP)
  {
    m_history.clear();
    // Drop the frames timed before the overlay was hidden
    KODI::DEBUG_FRAME_TIME::Take();
    m_lastSample = now;
    return;
  }
  m_lastSample = now;

  const auto frames = KODI::DEBUG_FRAME_TIME::Take();
  const float fps = CServiceBroker::GetWinSystem()->GetGfxContext().GetFPS();
  m_budgetMs = fps > 0.0f ? 1000.0f / fps : 0.0f;
  m_current = {frames.averageMs, frames.maxMs, frames.gpuMs};
  if (frames.frames > 0)
  {
    m_history.push_back(m_current);
    if (m_history.size() > HISTORY_SAMPLES)
      m_history.erase(m_history.begin());
    m_changed = true;
  }

#if defined(TARGET_POSIX)
  if (!m_clustersRead)
    ReadClusters();
  m_clockKHz.clear();
  for (const auto& cores : m_clusters)
  {
    m_clockKHz.push_back(std::atoi(
        ReadLine(StringUtils::Format("/sys/devices/system/cpu/cpu{}/cpufreq/scaling_cur_freq",
                                     cores.front()))
            .c_str()));
  }

  const std::string statm = ReadLine("/proc/self/statm");
  const auto fields = StringUtils::Split(statm, ' ');
  m_kodiBytes = fields.size() > 1 ? std::strtoull(fields[1].c_str(), nullptr, 10) *
                                        static_cast<uint64_t>(sysconf(_SC_PAGESIZE))
                                  : 0;
  m_availableBytes = 0;
  std::ifstream meminfo("/proc/meminfo");
  for (std::string line; std::getline(meminfo, line);)
  {
    if (StringUtils::StartsWith(line, "MemAvailable:"))
    {
      m_availableBytes = std::strtoull(line.c_str() + 13, nullptr, 10) * 1024;
      break;
    }
  }

  // Mali reports kHz
  const std::string gpu = ReadLine("/sys/class/misc/mali0/device/cur_freq");
  m_gpuMHz = gpu.empty() ? -1 : std::atoi(gpu.c_str()) / 1000;
#endif

#if defined(TARGET_ANDROID)
  if (!m_thermal)
    m_thermal = std::make_unique<Thermal>();
  m_thermalStatus = m_thermal->manager ? m_thermal->status(m_thermal->manager) : -1;
  m_thermalHeadroom = m_thermal->manager && m_thermal->headroom
                          ? m_thermal->headroom(m_thermal->manager, 0)
                          : -1.0f;
#endif
}

std::string CDebugSystemInfo::GetSummary() const
{
  if (m_lastSample == std::chrono::steady_clock::time_point{})
    return {};

  std::string summary;
  if (!m_clockKHz.empty())
  {
    std::vector<std::string> clocks;
    for (const int kHz : m_clockKHz)
      clocks.push_back(StringUtils::Format("{:.2f}", kHz / 1e6));
    summary += "CPU " + StringUtils::Join(clocks, " / ") + " GHz  ";
  }
  if (m_kodiBytes > 0)
  {
    summary += "RAM Kodi " + FormatBytes(m_kodiBytes);
    if (m_availableBytes > 0)
      summary += ", free " + FormatBytes(m_availableBytes);
    summary += "  ";
  }
  if (m_budgetMs > 0.0f && m_current.maxMs > 0.0f)
  {
    summary += StringUtils::Format("Frame {}{:.1f}{}, max {}{:.1f}{} of {:.1f} ms  ",
                                   BudgetAss(m_current.averageMs, m_budgetMs), m_current.averageMs,
                                   ASS_RESET, BudgetAss(m_current.maxMs, m_budgetMs),
                                   m_current.maxMs, ASS_RESET, m_budgetMs);
  }
  if (m_current.gpuMs >= 0.0f || m_gpuMHz > 0)
  {
    summary += "GPU";
    if (m_current.gpuMs >= 0.0f)
      summary += StringUtils::Format(" {}{:.1f} ms{}", BudgetAss(m_current.gpuMs, m_budgetMs),
                                     m_current.gpuMs, ASS_RESET);
    if (m_gpuMHz > 0)
      summary += StringUtils::Format(" @ {} MHz", m_gpuMHz);
    summary += "  ";
  }
  if (m_thermalStatus >= 0)
  {
    static constexpr std::array<std::string_view, 7> names = {
        "ok", "light", "moderate", "severe", "critical", "emergency", "shutdown"};
    const std::string_view name =
        m_thermalStatus < static_cast<int>(names.size()) ? names[m_thermalStatus] : "?";
    const std::string_view color = m_thermalStatus == 0   ? ASS_LOW
                                   : m_thermalStatus <= 2 ? ASS_MEDIUM
                                                          : ASS_HIGH;
    summary += StringUtils::Format("Thermal {}{}", color, name);
    if (m_thermalHeadroom >= 0.0f && !std::isnan(m_thermalHeadroom))
      summary += StringUtils::Format(" {:.2f}", m_thermalHeadroom);
    summary += ASS_RESET;
  }
  StringUtils::TrimRight(summary);
  return summary;
}

float CDebugSystemInfo::Render(const CRect& view, float top, float maxScale)
{
  if (m_history.empty() || m_budgetMs <= 0.0f || view.IsEmpty())
    return top;

  const float width = 4 * PADDING + LABEL_WIDTH + PLOT_WIDTH + TOTAL_WIDTH;
  const float height = 2 * PADDING + PLOT_HEIGHT;
  const float scale =
      std::min({maxScale > 0.0f ? maxScale : std::max(view.Height() / 1080.0f, 0.5f),
                (view.Width() - 2 * MARGIN) / width, (view.y2 - top - 2 * MARGIN) / height});
  if (scale <= 0.0f)
    return top;
  const auto w = static_cast<unsigned int>(std::lround(width * scale));
  const auto h = static_cast<unsigned int>(std::lround(height * scale));

  if (w != m_width || h != m_height || !m_texture)
  {
    m_width = w;
    m_height = h;
    m_pixels.assign(static_cast<size_t>(w) * h, 0);
    m_texture = CTexture::CreateTexture(w, h, XB_FMT_A8R8G8B8);
    m_changed = true;
  }
  if (!m_texture)
    return top;

  if (m_changed)
  {
    Draw(scale);
    m_texture->Update(w, h, w * 4, XB_FMT_A8R8G8B8,
                      reinterpret_cast<const unsigned char*>(m_pixels.data()), false);
    m_changed = false;
  }

  const float x2 = view.x2 - MARGIN * scale;
  const float y1 = top + MARGIN * scale;
  const CRect rect(x2 - static_cast<float>(w), y1, x2, y1 + static_cast<float>(h));
  // The texture may be padded, e.g. to a power of two
  const CRect texCoords(0.0f, 0.0f,
                        static_cast<float>(w) / static_cast<float>(m_texture->GetTextureWidth()),
                        static_cast<float>(h) / static_cast<float>(m_texture->GetTextureHeight()));
  CGUITexture::DrawQuad(rect, 0xFFFFFFFF, m_texture.get(), &texCoords, -1.0f);
  return rect.y2;
}

void CDebugSystemInfo::Reset()
{
  KODI::DEBUG_FRAME_TIME::SetEnabled(false);
  m_lastSample = {};
  m_history = {};
  m_current = {0.0f, 0.0f, -1.0f};
  m_clockKHz.clear();
  m_thermal.reset();
  m_pixels = std::vector<uint32_t>();
  m_texture.reset();
  m_width = 0;
  m_height = 0;
}

void CDebugSystemInfo::Draw(float scale)
{
  const int w = static_cast<int>(m_width);
  const int h = static_cast<int>(m_height);
  const auto px = [scale](float value) { return static_cast<int>(std::lround(value * scale)); };
  const int font = std::max(1, px(FONT));

  std::fill(m_pixels.begin(), m_pixels.end(), COLOR_BACKDROP);

  const auto fill = [&](int x1, int y1, int x2, int y2, uint32_t color)
  {
    for (int y = std::max(y1, 0); y < std::min(y2, h); ++y)
      for (int x = std::max(x1, 0); x < std::min(x2, w); ++x)
        m_pixels[static_cast<size_t>(y) * w + x] =
            Blend(m_pixels[static_cast<size_t>(y) * w + x], color);
  };
  const auto textWidth = [&](const std::string& text)
  { return static_cast<int>(text.size()) * (GLYPH_WIDTH + 1) * font - font; };
  const auto text = [&](int x, int y, const std::string& str)
  {
    for (const char c : str)
    {
      const auto glyph = GLYPH_CHARS.find(c);
      if (glyph != std::string_view::npos)
      {
        for (int row = 0; row < GLYPH_HEIGHT; ++row)
          for (int col = 0; col < GLYPH_WIDTH; ++col)
            if (GLYPHS[glyph][row] & (0x10 >> col))
              fill(x + col * font, y + row * font, x + (col + 1) * font, y + (row + 1) * font,
                   COLOR_TEXT);
      }
      x += (GLYPH_WIDTH + 1) * font;
    }
  };
  const auto dashedLine = [&](int x1, int x2, int y)
  {
    const int dash = std::max(1, px(6.0f));
    const int gap = std::max(1, px(4.0f));
    const int thickness = std::max(1, px(1.5f));
    for (int x = x1; x < x2; x += dash + gap)
      fill(x, y, std::min(x + dash, x2), y + thickness, COLOR_LINE);
  };

  const auto outline = [&](int x1, int y1, int x2, int y2)
  {
    const int t = std::max(1, px(2.0f));
    fill(x1, y1, x2, y1 + t, COLOR_TEXT);
    fill(x1, y2 - t, x2, y2, COLOR_TEXT);
    fill(x1, y1, x1 + t, y2, COLOR_TEXT);
    fill(x2 - t, y1, x2, y2, COLOR_TEXT);
  };

  const int padding = px(PADDING);
  const int textHeight = GLYPH_HEIGHT * font;
  const int plotX = padding + px(LABEL_WIDTH) + padding;
  const int totalWidth = px(TOTAL_WIDTH);
  const int plotWidth = w - plotX - padding - totalWidth - padding;
  const int top = padding;
  const int bottom = h - padding;
  fill(plotX, top, plotX + plotWidth, bottom, COLOR_TRACK);

  // The scale ends at twice the frame budget, so the budget is in the middle
  const float rangeMs = 2 * m_budgetMs;
  const auto toY = [&](float ms, int rangeTop)
  {
    return bottom - static_cast<int>(
                        std::lround((bottom - rangeTop) * std::clamp(ms / rangeMs, 0.0f, 1.0f)));
  };
  const std::string budget = StringUtils::Format("{:.0f}", m_budgetMs);
  text(padding + px(LABEL_WIDTH) - textWidth(budget), toY(m_budgetMs, top) - textHeight / 2,
       budget);

  // Newest sample at the right edge, interpolated between seconds like the CPU history
  const auto sampleX = [&](size_t index)
  {
    return static_cast<int>(
        std::lround(static_cast<float>(index + HISTORY_SAMPLES - m_history.size()) *
                    (plotWidth - 1) / static_cast<float>(HISTORY_SAMPLES - 1)));
  };
  const auto at = [&](float position, float Sample::*value)
  {
    const auto index = std::min(static_cast<size_t>(position), m_history.size() - 1);
    const float fraction = position - static_cast<float>(index);
    const float from = m_history[index].*value;
    return index + 1 < m_history.size() ? from + (m_history[index + 1].*value - from) * fraction
                                        : from;
  };
  int prevY = -1;
  int prevGpuY = -1;
  for (int x = 0; x < plotWidth; ++x)
  {
    const float position = static_cast<float>(x) * (HISTORY_SAMPLES - 1) /
                               static_cast<float>(std::max(plotWidth - 1, 1)) -
                           static_cast<float>(HISTORY_SAMPLES - m_history.size());
    if (position < 0.0f)
      continue;
    const float average = at(position, &Sample::averageMs);
    const int y = toY(average, top);
    const uint32_t color = BudgetColor(average, m_budgetMs);
    // The slowest frame of each second as a light band above the average
    fill(plotX + x, toY(at(position, &Sample::maxMs), top), plotX + x + 1, y, COLOR_MAX);
    fill(plotX + x, y, plotX + x + 1, bottom, (COLOR_LOW & 0x00FFFFFF) | 0x5A000000);
    const int fromY = prevY < 0 ? y : prevY;
    fill(plotX + x, std::min(fromY, y), plotX + x + 1, std::max(fromY, y) + px(2.0f), color);
    prevY = y;

    const float gpu = at(position, &Sample::gpuMs);
    if (gpu >= 0.0f)
    {
      const int gpuY = toY(gpu, top);
      const int fromGpuY = prevGpuY < 0 ? gpuY : prevGpuY;
      fill(plotX + x, std::min(fromGpuY, gpuY), plotX + x + 1, std::max(fromGpuY, gpuY) + px(2.0f),
           COLOR_GPU);
      prevGpuY = gpuY;
    }
    else
      prevGpuY = -1;
  }

  dashedLine(plotX, plotX + plotWidth, toY(m_budgetMs, top));

  // A red dot where the slowest frame of a second missed the budget, i.e. a frame was dropped
  const int dot = std::max(2, px(3.0f));
  for (size_t i = 0; i < m_history.size(); ++i)
  {
    if (m_history[i].maxMs <= m_budgetMs)
      continue;
    const int x = plotX + sampleX(i);
    const int y = toY(m_history[i].maxMs, top);
    fill(x - dot, y - dot, x + dot, y + dot, COLOR_HIGH);
  }

  // The last second's average in a bar like the CPU total
  const int x = plotX + plotWidth + padding;
  const int barTop = padding + textHeight + font;
  const float current = m_history.back().averageMs;
  const int currentTop = toY(current, barTop);
  fill(x, barTop, x + totalWidth, bottom, COLOR_TRACK);
  fill(x, currentTop, x + totalWidth, bottom, BudgetColor(current, m_budgetMs));
  outline(x, currentTop, x + totalWidth, bottom);
  const std::string value = StringUtils::Format("{:.0f}", current);
  text(x + (totalWidth - textWidth(value)) / 2,
       std::max(currentTop - textHeight - font, padding / 2), value);
}
