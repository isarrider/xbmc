/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DebugRenderer.h"

#include "DebugProbe.h"
#include "ServiceBroker.h"
#include "cores/VideoPlayer/DVDCodecs/Overlay/DVDOverlayLibass.h"
#include "cores/VideoPlayer/Interface/TimingConstants.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "settings/SubtitlesSettings.h"
#include "utils/StringUtils.h"
#include "utils/log.h"
#include "windowing/GraphicContext.h"
#include "windowing/WinSystem.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

using namespace OVERLAY;

namespace
{
//! Space around the CPU and frame time graphics, for a 1080 pixel high screen
constexpr float GRAPHICS_MARGIN = 8.0f;
} // namespace

CDebugRenderer::CDebugRenderer()
{
}

CDebugRenderer::~CDebugRenderer()
{
  Dispose();
}

void CDebugRenderer::Initialize()
{
  if (m_isInitialized)
    return;

  m_adapter = new CSubtitlesAdapter();

  KODI::DEBUGPROBE::LogOnce();

  m_isInitialized = m_adapter->Initialize();
  if (!m_isInitialized)
  {
    CLog::Log(LOGERROR, "{} - Failed to configure OSD info debug renderer", __FUNCTION__);
    delete m_adapter;
    m_adapter = nullptr;
    return;
  }

  // We create only a single overlay with a fixed PTS for each rendered frame
  m_overlay = m_adapter->CreateOverlay();
  m_overlayRenderer.AddOverlay(m_overlay, 1000000., 0);

  // Kick the dirty-driven walk skip so the toggle takes effect this frame;
  // without this the walk stays idle and the debug overlay never starts
  // rendering until something else dirties the GUI.
  OVERLAY::MarkDirty();
}

void CDebugRenderer::Dispose()
{
  if (!m_isInitialized)
    return;

  m_isInitialized = false;
  m_frameGraph.Reset();
  m_coreUsage.Reset();
  m_systemInfo.Reset();
  m_overlayRenderer.Flush();
  m_overlay.reset();
  OVERLAY::MarkDirty();
  if (m_adapter)
  {
    delete m_adapter;
    m_adapter = nullptr;
  }
}

void CDebugRenderer::SetInfo(DEBUG_INFO_PLAYER& info, const DEBUG_INFO_FRAMES& frames)
{
  if (!m_isInitialized)
    return;

  m_showFrameGraph =
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_videoDebugGraph;
  if (m_showFrameGraph)
    m_frameGraph.SetFrames(frames);

  // FIXME: We currently force ASS_Event's and the current PTS
  // of rendering with fixed values to allow perpetual
  // display of on-screen text. It would be appropriate for Libass
  // provide a way to allow fixed on-screen text display
  // without use all these fixed values.
  m_adapter->FlushSubtitles();

  // Every line under the name <playerdebuginfo><lines> uses for it, in the default order. A line
  // that is switched off or has nothing to show stays empty and is left out, except the player's
  // own lines, which are always shown.
  struct Line
  {
    std::string_view name;
    std::string text;
    bool always;
  };
  std::vector<Line> lines = {
      {"audio", info.audio, true},   {"video", info.video, true},
      {"player", info.player, true}, {"framesizes", {}, false},
      {"cores", {}, false},          {"system", {}, false},
      {"frames", {}, false},         {"threads", {}, false},
      {"vsync", info.vsync, true},   {"geometry", info.contentGeometry, false}};
  const auto set = [&lines](std::string_view name, std::string text)
  { std::ranges::find(lines, name, &Line::name)->text = std::move(text); };

  if (m_showFrameGraph)
    set("framesizes", m_frameGraph.GetLegend());

  const auto& advancedSettings = CServiceBroker::GetSettingsComponent()->GetAdvancedSettings();
  m_coreGraph = static_cast<CDebugCoreUsage::Graph>(advancedSettings->m_videoDebugThreadsGraph);
  if (advancedSettings->m_videoDebugThreadsCores || m_coreGraph != CDebugCoreUsage::Graph::OFF)
    m_coreUsage.Update();
  if (advancedSettings->m_videoDebugThreadsCores)
  {
    const std::string coreUsage = m_coreUsage.GetSummary();
    if (!coreUsage.empty())
      set("cores", "Cores: " + coreUsage);
  }

  m_systemGraph = advancedSettings->m_videoDebugSystemGraph;
  const bool timeFrames = advancedSettings->m_videoDebugFrames || m_systemGraph;
  if (advancedSettings->m_videoDebugSystem || timeFrames)
    m_systemInfo.Update(timeFrames);
  if (advancedSettings->m_videoDebugSystem)
  {
    const std::string systemInfo = m_systemInfo.GetSystemSummary();
    if (!systemInfo.empty())
      set("system", "System: " + systemInfo);
  }
  if (advancedSettings->m_videoDebugFrames)
  {
    const std::string frameInfo = m_systemInfo.GetFrameSummary();
    if (!frameInfo.empty())
      set("frames", "Frames: " + frameInfo);
  }

  if (advancedSettings->m_videoDebugThreads)
  {
    const std::string& threadUsage =
        m_threadUsage.GetSummary(advancedSettings->m_videoDebugThreadsCount);
    if (!threadUsage.empty())
      set("threads", "Threads: " + threadUsage);
  }

  // FIXME: We currently force ASS_Event's and the current PTS
  // of rendering with fixed values to allow perpetual
  // display of on-screen text. It would be appropriate for Libass
  // provide a way to allow fixed on-screen text display
  // without use all these fixed values.
  const auto add = [this](Line& line)
  {
    // Trailing spaces, e.g. after the VSync values, would get a box of their own with a text
    // background
    StringUtils::TrimRight(line.text);
    if (line.always || !line.text.empty())
      m_adapter->AddSubtitle(line.text, 0., 5000000.);
  };
  std::vector<Line*> shown;
  if (advancedSettings->m_videoDebugLines.empty())
  {
    for (Line& line : lines)
      shown.push_back(&line);
  }
  else
  {
    for (const std::string& name : advancedSettings->m_videoDebugLines)
    {
      const auto line = std::ranges::find(lines, name, &Line::name);
      if (line != lines.end())
        shown.push_back(&*line);
    }
  }
  // Libass stacks bottom aligned lines upwards, so add them last to first to keep the order
  if (advancedSettings->m_videoDebugTextPosition >= 2)
    std::ranges::reverse(shown);
  for (Line* line : shown)
    add(*line);
}

void CDebugRenderer::SetInfo(DEBUG_INFO_VIDEO& video, DEBUG_INFO_RENDER& render)
{
  if (!m_isInitialized)
    return;

  m_showFrameGraph = false;
  m_coreGraph = CDebugCoreUsage::Graph::OFF;
  m_systemGraph = false;

  // FIXME: We currently force ASS_Event's and the current PTS
  // of rendering with fixed values to allow perpetual
  // display of on-screen text. It would be appropriate for Libass
  // provide a way to allow fixed on-screen text display
  // without use all these fixed values.
  m_adapter->FlushSubtitles();
  m_adapter->AddSubtitle(video.videoSource, 0., 5000000.);
  m_adapter->AddSubtitle(video.metaPrim, 0., 5000000.);
  m_adapter->AddSubtitle(video.metaLight, 0., 5000000.);
  m_adapter->AddSubtitle(video.shader, 0., 5000000.);
  m_adapter->AddSubtitle(video.render, 0., 5000000.);
  m_adapter->AddSubtitle(render.renderFlags, 0., 5000000.);
  m_adapter->AddSubtitle(render.videoOutput, 0., 5000000.);
}

void CDebugRenderer::RenderGraphics(const CRect& view)
{
  const auto& advancedSettings = CServiceBroker::GetSettingsComponent()->GetAdvancedSettings();
  const float size = static_cast<float>(advancedSettings->m_videoDebugGraphicsSize) / 100;
  // Corners as in the settings: bit 0 is the right side, bit 1 the bottom. If the text is in the
  // same corner, the graphics move to the other side.
  int corner = advancedSettings->m_videoDebugGraphicsPosition;
  if (corner == advancedSettings->m_videoDebugTextPosition)
    corner ^= 1;
  const bool right = (corner & 1) != 0;
  const bool bottom = (corner & 2) != 0;

  CSize cores;
  float scale = 0.0f;
  if (m_coreGraph != CDebugCoreUsage::Graph::OFF)
  {
    cores = m_coreUsage.Prepare(view, m_coreGraph, size);
    scale = m_coreUsage.GetScale();
  }
  const float baseScale = std::max(view.Height() / 1080.0f, 0.5f) * size;
  // The frame times line up with the CPU graphic above them and share its margins
  const float margin = GRAPHICS_MARGIN * (scale > 0.0f ? scale : baseScale);
  CSize frames;
  if (m_systemGraph)
  {
    const float used = cores.Height() > 0.0f ? cores.Height() + margin : 0.0f;
    frames = m_systemInfo.Prepare(view, size, scale, view.Height() - used - 2 * margin);
  }
  if (cores.Width() <= 0.0f && frames.Width() <= 0.0f)
    return;

  const float gap = cores.Height() > 0.0f && frames.Height() > 0.0f ? margin : 0.0f;
  float y = bottom ? view.y2 - margin - (cores.Height() + gap + frames.Height()) : view.y1 + margin;
  const auto x = [&](float width) { return right ? view.x2 - margin - width : view.x1 + margin; };
  if (cores.Width() > 0.0f)
  {
    m_coreUsage.RenderAt({x(cores.Width()), y});
    y += cores.Height() + gap;
  }
  if (frames.Width() > 0.0f)
    m_systemInfo.RenderAt({x(frames.Width()), y});
}

void CDebugRenderer::Render(CRect& src, CRect& dst, CRect& view)
{
  if (!m_isInitialized)
    return;

  if (CServiceBroker::GetWinSystem()->GetGfxContext().IsFullScreenVideo())
  {
    if (m_showFrameGraph)
      m_frameGraph.Render(view);
    RenderGraphics(view);
  }

  m_overlayRenderer.SetVideoRect(src, dst, view);
  m_overlayRenderer.Render(0);

  // Sustain the dirty-driven Render skip while the debug OSD is enabled.
  // Unlike video subtitles (which call AddOverlay per video frame and so
  // mark dirty per frame), the debug OSD only calls AddOverlay once at
  // Initialize, so without a per-frame nudge here the GUI walk would not
  // re-fire after the first frame and the debug OSD would freeze.
  MarkDirty();
}

void CDebugRenderer::Flush()
{
  if (!m_isInitialized)
    return;

  m_adapter->FlushSubtitles();
}

CDebugRenderer::CRenderer::CRenderer() : OVERLAY::CRenderer()
{
}

void CDebugRenderer::CRenderer::Render(int idx, float depth)
{
  if (!m_debugOverlayStyle)
    CreateSubtitlesStyle();

  SubtitleResolution resolution;
  UpdateSubtitlePosition(resolution);

  for (const SElement& e : m_buffers[idx])
  {
    if (!e.overlay_dvd)
      continue;

    const auto& ovAss = static_cast<const CDVDOverlayLibass&>(*e.overlay_dvd);
    if (!ovAss.GetLibassHandler())
      continue;

    const KODI::SUBTITLES::STYLE::renderOpts rOpts{
        GetRenderOptions(ovAss, *m_debugOverlayStyle, resolution)};

    int changes = 0;
    ASS_Image* images =
        ovAss.GetLibassHandler()->RenderImage(e.pts, rOpts, m_debugOverlayStyle, &changes);

    if (!images)
      continue;

    std::shared_ptr<COverlay> o = COverlay::Create(images, rOpts.frameWidth, rOpts.frameHeight);

    if (o)
      OVERLAY::CRenderer::Render(o.get());
  }
  ReleaseUnused();
}

void CDebugRenderer::CRenderer::CreateSubtitlesStyle()
{
  using namespace KODI::SUBTITLES::STYLE;
  const auto& advancedSettings = CServiceBroker::GetSettingsComponent()->GetAdvancedSettings();

  m_debugOverlayStyle = std::make_shared<style>();
  m_debugOverlayStyle->fontName = KODI::SUBTITLES::FONT_DEFAULT_FAMILYNAME;
  m_debugOverlayStyle->fontSize = 20.0 * advancedSettings->m_videoDebugTextSize / 100;
  m_debugOverlayStyle->marginVertical = 12;
  if (advancedSettings->m_videoDebugTextBackdrop > 0)
  {
    // One box behind the whole line. BOX draws a box per run of text, which overlap where the
    // colour changes, and casts a shadow.
    m_debugOverlayStyle->borderStyle = BorderType::SQUARE_BOX;
    m_debugOverlayStyle->backgroundOpacity = advancedSettings->m_videoDebugTextBackdrop;
  }
  static constexpr std::array<FontAlign, 4> alignments = {
      FontAlign::TOP_LEFT, FontAlign::TOP_RIGHT, FontAlign::SUB_LEFT, FontAlign::SUB_RIGHT};
  m_debugOverlayStyle->alignment =
      alignments[std::clamp(advancedSettings->m_videoDebugTextPosition, 0, 3)];
}
