/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DebugFrameTime.h"

#include <algorithm>
#include <array>
#include <chrono>

#if defined(TARGET_ANDROID)
#include "ServiceBroker.h"
#include "rendering/RenderSystem.h"

#include "system_gl.h"

#include <EGL/egl.h>
#endif

using namespace KODI::DEBUG_FRAME_TIME;

namespace
{
bool g_enabled = false;
bool g_inFrame = false;
std::chrono::steady_clock::time_point g_frameStart;
Stats g_stats;
double g_sumMs = 0.0;

#if defined(TARGET_ANDROID)
// GL_EXT_disjoint_timer_query, looked up at runtime as not all drivers have it
constexpr GLenum TIME_ELAPSED = 0x88BF;
constexpr GLenum QUERY_RESULT = 0x8866;
constexpr GLenum QUERY_RESULT_AVAILABLE = 0x8867;
constexpr GLenum GPU_DISJOINT = 0x8FBB;

using GenQueries = void(GL_APIENTRYP)(GLsizei, GLuint*);
using DeleteQueries = void(GL_APIENTRYP)(GLsizei, const GLuint*);
using BeginQuery = void(GL_APIENTRYP)(GLenum, GLuint);
using EndQuery = void(GL_APIENTRYP)(GLenum);
using GetQueryObjectuiv = void(GL_APIENTRYP)(GLuint, GLenum, GLuint*);

struct GpuTimer
{
  bool checked = false;
  GenQueries genQueries = nullptr;
  DeleteQueries deleteQueries = nullptr;
  BeginQuery beginQuery = nullptr;
  EndQuery endQuery = nullptr;
  GetQueryObjectuiv getQueryObjectuiv = nullptr;

  // The GPU finishes a frame a few frames later, so keep several queries in flight
  std::array<GLuint, 4> queries{};
  std::array<bool, 4> pending{};
  size_t current = 0;
  bool created = false;
  bool running = false;
  double sumMs = 0.0;
  int count = 0;

  bool Available()
  {
    if (!checked)
    {
      checked = true;
      const auto renderSystem = CServiceBroker::GetRenderSystem();
      if (renderSystem && renderSystem->IsExtSupported("GL_EXT_disjoint_timer_query"))
      {
        genQueries = reinterpret_cast<GenQueries>(eglGetProcAddress("glGenQueriesEXT"));
        deleteQueries = reinterpret_cast<DeleteQueries>(eglGetProcAddress("glDeleteQueriesEXT"));
        beginQuery = reinterpret_cast<BeginQuery>(eglGetProcAddress("glBeginQueryEXT"));
        endQuery = reinterpret_cast<EndQuery>(eglGetProcAddress("glEndQueryEXT"));
        getQueryObjectuiv =
            reinterpret_cast<GetQueryObjectuiv>(eglGetProcAddress("glGetQueryObjectuivEXT"));
      }
    }
    return genQueries && deleteQueries && beginQuery && endQuery && getQueryObjectuiv;
  }

  void Collect()
  {
    GLint disjoint = 0;
    glGetIntegerv(GPU_DISJOINT, &disjoint);
    for (size_t i = 0; i < queries.size(); ++i)
    {
      if (!pending[i])
        continue;
      GLuint available = 0;
      getQueryObjectuiv(queries[i], QUERY_RESULT_AVAILABLE, &available);
      if (!available)
        continue;
      GLuint ns = 0;
      getQueryObjectuiv(queries[i], QUERY_RESULT, &ns);
      pending[i] = false;
      // A disjoint event, e.g. a clock change, makes the results in flight meaningless
      if (!disjoint)
      {
        sumMs += ns / 1e6;
        ++count;
      }
    }
  }

  void Begin()
  {
    if (!Available())
      return;
    if (!created)
    {
      genQueries(static_cast<GLsizei>(queries.size()), queries.data());
      created = true;
    }
    Collect();
    // Skip this frame if the GPU is still busy with the frame that used this query
    if (pending[current])
      return;
    beginQuery(TIME_ELAPSED, queries[current]);
    running = true;
  }

  void End()
  {
    if (!running)
      return;
    endQuery(TIME_ELAPSED);
    running = false;
    pending[current] = true;
    current = (current + 1) % queries.size();
  }

  void Free()
  {
    if (created)
      deleteQueries(static_cast<GLsizei>(queries.size()), queries.data());
    created = false;
    running = false;
    pending.fill(false);
    sumMs = 0.0;
    count = 0;
  }
};
GpuTimer g_gpu;
bool g_freeGpu = false;
#endif
} // namespace

void KODI::DEBUG_FRAME_TIME::SetEnabled(bool enabled)
{
#if defined(TARGET_ANDROID)
  // Freed in the next BeginFrame, as this may not be called with a current GL context
  if (g_enabled && !enabled)
    g_freeGpu = true;
#endif
  g_enabled = enabled;
}

void KODI::DEBUG_FRAME_TIME::BeginFrame()
{
#if defined(TARGET_ANDROID)
  if (g_freeGpu)
  {
    g_gpu.Free();
    g_freeGpu = false;
  }
#endif
  if (!g_enabled)
    return;

  g_inFrame = true;
  g_frameStart = std::chrono::steady_clock::now();
#if defined(TARGET_ANDROID)
  g_gpu.Begin();
#endif
}

void KODI::DEBUG_FRAME_TIME::EndFrame()
{
  if (!g_inFrame)
    return;
  g_inFrame = false;

#if defined(TARGET_ANDROID)
  g_gpu.End();
#endif
  const float ms =
      std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - g_frameStart)
          .count();
  ++g_stats.frames;
  g_sumMs += static_cast<double>(ms);
  g_stats.maxMs = std::max(g_stats.maxMs, ms);
}

Stats KODI::DEBUG_FRAME_TIME::Take()
{
  Stats stats = g_stats;
  if (stats.frames > 0)
    stats.averageMs = static_cast<float>(g_sumMs / stats.frames);
#if defined(TARGET_ANDROID)
  if (g_gpu.count > 0)
    stats.gpuMs = static_cast<float>(g_gpu.sumMs / g_gpu.count);
  g_gpu.sumMs = 0.0;
  g_gpu.count = 0;
#endif
  g_stats = {};
  g_sumMs = 0.0;
  return stats;
}
