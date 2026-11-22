/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

namespace KODI::DEBUGPROBE
{
/*!
 * \brief Log once which GPU, CPU clock, thermal and memory sources this device lets
 * Kodi read, to decide which of them the Player Debug Info could show.
 *
 * Call from the render thread, as it also queries the render system.
 */
void LogOnce();
} // namespace KODI::DEBUGPROBE
