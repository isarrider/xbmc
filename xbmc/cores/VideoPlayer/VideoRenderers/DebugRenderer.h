/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "DebugCoreUsage.h"
#include "DebugFrameGraph.h"
#include "DebugInfo.h"
#include "DebugSystemInfo.h"
#include "DebugThreadUsage.h"
#include "OverlayRenderer.h"
#include "cores/VideoPlayer/DVDCodecs/Overlay/DVDOverlay.h"
#include "cores/VideoPlayer/DVDSubtitles/SubtitlesAdapter.h"

#include <atomic>
#include <memory>
#include <string>

class CDebugRenderer
{
public:
  CDebugRenderer();
  virtual ~CDebugRenderer();
  void Initialize();
  void Dispose();
  void SetInfo(DEBUG_INFO_PLAYER& info, const DEBUG_INFO_FRAMES& frames);
  bool IsFrameGraphUpdateDue() { return m_frameGraph.IsUpdateDue(); }
  void SetInfo(DEBUG_INFO_VIDEO& video, DEBUG_INFO_RENDER& render);
  void Render(CRect& src, CRect& dst, CRect& view);
  void Flush();

protected:
  class CRenderer : public OVERLAY::CRenderer
  {
  public:
    CRenderer();
    void Render(int idx, float depth = 1.0f) override;
    void CreateSubtitlesStyle();

  private:
    // Implementation of Observer
    void Notify(const Observable& obs, const ObservableMessage msg) override{};

    std::shared_ptr<struct KODI::SUBTITLES::STYLE::style> m_debugOverlayStyle;
  };

  CRenderer m_overlayRenderer;
  CDebugFrameGraph m_frameGraph;
  CDebugThreadUsage m_threadUsage;
  CDebugCoreUsage m_coreUsage;
  CDebugCoreUsage::Graph m_coreGraph{CDebugCoreUsage::Graph::OFF};
  CDebugSystemInfo m_systemInfo;
  bool m_systemGraph{false};
  bool m_showFrameGraph{false};

private:
  //! The CPU and frame time graphics, stacked in the configured corner
  void RenderGraphics(const CRect& view);

  CSubtitlesAdapter* m_adapter{nullptr};
  std::atomic_bool m_isInitialized{false};
  std::shared_ptr<CDVDOverlay> m_overlay;
};
