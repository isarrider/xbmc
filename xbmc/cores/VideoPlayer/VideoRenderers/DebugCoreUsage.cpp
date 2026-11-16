/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DebugCoreUsage.h"

#include "ServiceBroker.h"
#include "guilib/GUITexture.h"
#include "guilib/Texture.h"
#include "guilib/TextureFormats.h"
#include "utils/CPUInfo.h"
#include "utils/StringUtils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

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
constexpr std::array<uint32_t, 4> CORE_COLORS = {0xFF3DDC84, 0xFF3F8CFF, 0xFFFFB547, 0xFFFF6AD5};

// Sizes for a 1080 pixel high screen
constexpr float MARGIN = 8.0f;
constexpr float PADDING = 12.0f;
constexpr float BAR_WIDTH = 44.0f;
constexpr float BAR_GAP = 14.0f;
constexpr float BAR_HEIGHT = 150.0f;
constexpr float ROW_HEIGHT = 44.0f;
constexpr float ROW_GAP = 8.0f;
constexpr float PLOT_WIDTH = 300.0f;

//! 5x7 pixel digits, '#' and '%', one row per entry, bit 4 is the left column
constexpr std::string_view GLYPH_CHARS = "0123456789#%";
constexpr int GLYPH_WIDTH = 5;
constexpr int GLYPH_HEIGHT = 7;
constexpr std::array<std::array<uint8_t, GLYPH_HEIGHT>, 12> GLYPHS = {{
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
    {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A},
    {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03},
}};
//! Size of one glyph pixel
constexpr float FONT = 2.0f;
constexpr float TEXT_HEIGHT = GLYPH_HEIGHT * FONT;
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

uint32_t UsageColor(float percent)
{
  if (percent >= 90.0f)
    return COLOR_HIGH;
  return percent >= 70.0f ? COLOR_MEDIUM : COLOR_LOW;
}
} // namespace

CDebugCoreUsage::CDebugCoreUsage() = default;

CDebugCoreUsage::~CDebugCoreUsage() = default;

void CDebugCoreUsage::Update()
{
  const auto now = std::chrono::steady_clock::now();
  if (now - m_lastSample < SAMPLE_INTERVAL)
    return;

  const auto cpuInfo = CServiceBroker::GetCPUInfo();
  if (!cpuInfo || !cpuInfo->SupportsCPUUsage())
    return;

  if (now - m_lastSample > MAX_SAMPLE_GAP)
    m_history.clear();
  m_lastSample = now;

  m_total = cpuInfo->GetUsedPercentage();
  const int cores = cpuInfo->GetCPUCount();
  m_history.resize(static_cast<size_t>(std::max(cores, 0)));
  for (int core = 0; core < cores; ++core)
  {
    auto& samples = m_history[static_cast<size_t>(core)];
    samples.push_back(static_cast<float>(cpuInfo->GetCoreInfo(core).m_usagePercent));
    if (samples.size() > HISTORY_SAMPLES)
      samples.erase(samples.begin());
  }
  m_changed = true;
}

std::string CDebugCoreUsage::GetSummary() const
{
  if (m_history.empty())
    return {};

  std::string summary;
  for (size_t core = 0; core < m_history.size(); ++core)
  {
    summary +=
        StringUtils::Format("#{} {:.0f}%  ", core, static_cast<double>(m_history[core].back()));
  }
  return summary + StringUtils::Format("(total {}%)", m_total);
}

void CDebugCoreUsage::Render(const CRect& view, Graph graph)
{
  if (graph == Graph::OFF || m_history.empty() || view.IsEmpty())
    return;

  const float scale = std::max(view.Height() / 1080.0f, 0.5f);
  const float cores = static_cast<float>(m_history.size());
  const float width = graph == Graph::BARS ? 2 * PADDING + cores * BAR_WIDTH + (cores - 1) * BAR_GAP
                                           : 3 * PADDING + LABEL_WIDTH + PLOT_WIDTH;
  const float height = graph == Graph::BARS
                           ? 2 * PADDING + 2 * (TEXT_HEIGHT + 6) + BAR_HEIGHT
                           : 2 * PADDING + cores * ROW_HEIGHT + (cores - 1) * ROW_GAP;
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
    return;

  if (m_changed || graph != m_drawnGraph)
  {
    Draw(graph, scale);
    m_texture->Update(w, h, w * 4, XB_FMT_A8R8G8B8,
                      reinterpret_cast<const unsigned char*>(m_pixels.data()), false);
    m_changed = false;
    m_drawnGraph = graph;
  }

  const float x2 = view.x2 - MARGIN * scale;
  const float y1 = view.y1 + MARGIN * scale;
  const CRect rect(x2 - static_cast<float>(w), y1, x2, y1 + static_cast<float>(h));
  // The texture may be padded, e.g. to a power of two
  const CRect texCoords(0.0f, 0.0f,
                        static_cast<float>(w) / static_cast<float>(m_texture->GetTextureWidth()),
                        static_cast<float>(h) / static_cast<float>(m_texture->GetTextureHeight()));
  CGUITexture::DrawQuad(rect, 0xFFFFFFFF, m_texture.get(), &texCoords, -1.0f);
}

void CDebugCoreUsage::Reset()
{
  m_lastSample = {};
  m_history = {};
  m_pixels = std::vector<uint32_t>();
  m_texture.reset();
  m_width = 0;
  m_height = 0;
}

void CDebugCoreUsage::Draw(Graph graph, float scale)
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

  const int padding = px(PADDING);
  const int textHeight = GLYPH_HEIGHT * font;

  if (graph == Graph::BARS)
  {
    const int barWidth = px(BAR_WIDTH);
    const int barTop = padding + textHeight + px(6.0f);
    const int barBottom = barTop + px(BAR_HEIGHT);
    for (size_t core = 0; core < m_history.size(); ++core)
    {
      const float usage = std::clamp(m_history[core].back(), 0.0f, 100.0f);
      const int x = padding + static_cast<int>(core) * px(BAR_WIDTH + BAR_GAP);
      const int top = barBottom - static_cast<int>(std::lround((barBottom - barTop) * usage / 100));
      fill(x, barTop, x + barWidth, barBottom, COLOR_TRACK);
      fill(x, top, x + barWidth, barBottom, UsageColor(usage));

      const std::string value = StringUtils::Format("{:.0f}%", static_cast<double>(usage));
      text(x + (barWidth - textWidth(value)) / 2, std::max(top - textHeight - font, padding),
           value);
      const std::string label = StringUtils::Format("#{}", core);
      text(x + (barWidth - textWidth(label)) / 2, barBottom + px(6.0f), label);
    }
    return;
  }

  const int plotX = padding + px(LABEL_WIDTH) + padding;
  const int plotWidth = w - plotX - padding;
  for (size_t core = 0; core < m_history.size(); ++core)
  {
    const auto& samples = m_history[core];
    const uint32_t color = CORE_COLORS[core % CORE_COLORS.size()];
    const int top = padding + static_cast<int>(core) * px(ROW_HEIGHT + ROW_GAP);
    const int bottom = top + px(ROW_HEIGHT);

    fill(plotX, top, plotX + plotWidth, bottom, COLOR_TRACK);
    text(padding, (top + bottom - textHeight) / 2, StringUtils::Format("#{}", core));

    // The newest sample is at the right edge
    const auto toY = [&](float usage)
    {
      return bottom -
             static_cast<int>(std::lround((bottom - top) * std::clamp(usage, 0.0f, 100.0f) / 100));
    };
    int prevY = -1;
    for (int x = 0; x < plotWidth; ++x)
    {
      const float position = static_cast<float>(x) * (HISTORY_SAMPLES - 1) /
                                 static_cast<float>(std::max(plotWidth - 1, 1)) -
                             static_cast<float>(HISTORY_SAMPLES - samples.size());
      if (position < 0.0f)
        continue;
      const auto index = std::min(static_cast<size_t>(position), samples.size() - 1);
      const float fraction = position - static_cast<float>(index);
      const float usage = index + 1 < samples.size()
                              ? samples[index] + (samples[index + 1] - samples[index]) * fraction
                              : samples[index];
      const int y = toY(usage);
      fill(plotX + x, y, plotX + x + 1, bottom, (color & 0x00FFFFFF) | 0x5A000000);
      const int fromY = prevY < 0 ? y : prevY;
      fill(plotX + x, std::min(fromY, y), plotX + x + 1, std::max(fromY, y) + px(2.0f), color);
      prevY = y;
    }

    const std::string value = StringUtils::Format("{:.0f}%", static_cast<double>(samples.back()));
    text(plotX + plotWidth - textWidth(value) - font, top + font, value);
  }
}
