/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_H_MAP_MAGIC_DEFINES
#define MANGOS_H_MAP_MAGIC_DEFINES

// Single source of truth for the Cata *.map file format magic.
//
// Intentionally dependency-free so it can be included by:
//   - mangosd (via GridMapDefines.h -> Platform/Define.h -> ACE)
//   - mmap-extractor (via TerrainBuilder.h -> GridMapDefines.h)
//   - map-extractor (directly — does NOT link to ACE)
//   - shared/ExtractorCommon.cpp (directly — same reason)
//
// Previously duplicated across four files, with the drift hazard the
// vmap magic centralisation already addressed in memory file
// feedback_vmap_magic_drift_hazard.md. A future bump now touches
// exactly this one constant.
//
// Format suffix bumps when the .map writer's output changes shape or
// fixes an on-disk semantic bug. Current value c1.5 was bumped from
// c1.4 when the water-minHeight defaulting fix landed (TC v13 -> v14
// port, commit a6a0c9219 on vmap-redesign).
static char const MAP_VERSION_MAGIC[] = "c1.5";

#endif // MANGOS_H_MAP_MAGIC_DEFINES
