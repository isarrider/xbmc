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
 * Called at startup and when the Player Debug Info is first opened, whichever
 * comes first. Needs the render system to be initialized.
 */
void LogOnce();
} // namespace KODI::DEBUGPROBE
