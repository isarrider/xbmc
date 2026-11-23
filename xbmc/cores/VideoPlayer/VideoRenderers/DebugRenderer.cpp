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
#include "utils/log.h"
#include "windowing/GraphicContext.h"
#include "windowing/WinSystem.h"

#include <algorithm>
#include <string_view>
#include <vector>

using namespace OVERLAY;

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
    if (line.always || !line.text.empty())
      m_adapter->AddSubtitle(line.text, 0., 5000000.);
  };
  const auto& order = advancedSettings->m_videoDebugLines;
  if (order.empty())
  {
    for (Line& line : lines)
      add(line);
  }
  else
  {
    for (const std::string& name : order)
    {
      const auto line = std::ranges::find(lines, name, &Line::name);
      if (line != lines.end())
        add(*line);
    }
  }
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

void CDebugRenderer::Render(CRect& src, CRect& dst, CRect& view)
{
  if (!m_isInitialized)
    return;

  if (CServiceBroker::GetWinSystem()->GetGfxContext().IsFullScreenVideo())
  {
    if (m_showFrameGraph)
      m_frameGraph.Render(view);
    float top = view.y1;
    float scale = 0.0f;
    if (m_coreGraph != CDebugCoreUsage::Graph::OFF)
    {
      top = m_coreUsage.Render(view, m_coreGraph);
      scale = m_coreUsage.GetScale();
    }
    if (m_systemGraph)
      m_systemInfo.Render(view, top, scale);
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
  m_debugOverlayStyle = std::make_shared<KODI::SUBTITLES::STYLE::style>();
  m_debugOverlayStyle->fontName = KODI::SUBTITLES::FONT_DEFAULT_FAMILYNAME;
  m_debugOverlayStyle->fontSize = 20.0;
  m_debugOverlayStyle->marginVertical = 12;
}
