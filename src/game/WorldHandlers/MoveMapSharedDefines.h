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

#ifndef MANGOS_H_MOVE_MAP_SHARED_DEFINES
#define MANGOS_H_MOVE_MAP_SHARED_DEFINES

#include "../recastnavigation/Detour/Include/DetourNavMesh.h"
#include "Platform/Define.h"

#define MMAP_MAGIC 0x4d4d4150   // 'MMAP'
// MMAP_VERSION lineage on this fork — see MANGOS/STAGE6_MMAP_VERSION_PLAN.md:
//   v3  = pre-existing mangosthree state (frozen 2013-ish)
//   v6  = WMO liquid nav-type mask (TC 15a1e26)
//   v7  = walkable slope angle 60° -> 55° (TC f53708f)
//   v10 = walkable climb 4 -> 6 cells, ~1.6 yd (TC e3bab35, #24539)
// v4/v5/v8/v9 and the v11/v12 intermediates remain parked in Phase B/C:
//   v8  = tools merge sweep (no concrete bug)
//   v9  = Recast library bump (src/ change skipped 2026-05-24, see plan)
//   v11 = multi-slope handling (would require porting NAV_AREA_GROUND_STEEP)
//   v12 = steep-vs-ground swap (depends on v11)
//   v13 = mmtile tileX/tileY coordinate-swap fix (port of upstream ea1927454)
//         + build ADT floor under liquid: drop the buggy
//           "minLLevel > maxTLevel -> useTerrain=false" block, aligning with
//           mangoszero/mangostwo which already removed it
//   v14 = 4-digit map id in all tile filenames (.map/.mmtile/.mmap/.vmtree/
//         .vmtile) for MoP map ids > 999; paired with VMAP_MAGIC "VMAP_4.3"
#define MMAP_VERSION 14

struct MmapTileHeader
{
    uint32 mmapMagic;
    uint32 dtVersion;
    uint32 mmapVersion;
    uint32 size;
    char usesLiquids;
    char padding[3];

    MmapTileHeader() : mmapMagic(MMAP_MAGIC), dtVersion(DT_NAVMESH_VERSION),
        mmapVersion(MMAP_VERSION), size(0), usesLiquids(true), padding() {}
};

// Mirrors TC's pattern — every byte of the 20-byte on-disk header is
// explicitly accounted for. The previous `bool usesLiquids : 1` bitfield
// left the upper 3 bytes of that storage word compiler-uninitialized,
// so older .mmtile files have non-deterministic bytes at offsets 17-19.
// The server only reads byte 16 (usesLiquids) so this is harmless at
// runtime, but it made the file format non-portable across compilers
// and blocked future format extensions from re-using the padding.
static_assert(sizeof(MmapTileHeader) == 20,
              "MmapTileHeader size must be 20; adjust the padding field");
static_assert(sizeof(MmapTileHeader) == (sizeof(MmapTileHeader::mmapMagic) +
                                         sizeof(MmapTileHeader::dtVersion) +
                                         sizeof(MmapTileHeader::mmapVersion) +
                                         sizeof(MmapTileHeader::size) +
                                         sizeof(MmapTileHeader::usesLiquids) +
                                         sizeof(MmapTileHeader::padding)),
              "MmapTileHeader has uninitialized padding fields");

enum NavTerrain
{
    NAV_EMPTY   = 0x00,
    NAV_GROUND  = 0x01,
    NAV_MAGMA   = 0x02,
    NAV_SLIME   = 0x04,
    NAV_WATER   = 0x08,
    NAV_UNUSED1 = 0x10,
    NAV_UNUSED2 = 0x20,
    NAV_UNUSED3 = 0x40,
    NAV_UNUSED4 = 0x80
    // we only have 8 bits
};

#endif  // _MOVE_MAP_SHARED_DEFINES_H
