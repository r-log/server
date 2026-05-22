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

// vmap-harness: regression sampler for vmap / mmap / maps spatial queries.
//
// Reads a CSV of (sample_id, map_id, x, y, z, los_x, los_y, los_z, description)
// rows, probes the same spatial queries the live server uses via VMapManager,
// and emits a deterministic CSV that downstream stages diff against a baseline.
//
// Design rationale: STAGE0_PLAN.md.
// Query set mirrors TC's `.gps` GM command (cs_misc.cpp:230-310) so the
// regression diffs are interpretable in TC-vocabulary terms.

#include "VMapManager2.h"
#include "IVMapManager.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Grid math constants — mirror src/game/WorldHandlers/GridDefines.h
// to avoid pulling in framework headers from the harness.
namespace
{
    constexpr double SIZE_OF_GRIDS      = 533.33333;
    constexpr int    CENTER_GRID_ID     = 32;
    constexpr double CENTER_GRID_OFFSET = SIZE_OF_GRIDS / 2.0;

    struct Sample
    {
        std::string sample_id;
        unsigned int map_id   = 0;
        float        x        = 0.0f;
        float        y        = 0.0f;
        float        z        = 0.0f;
        bool         has_los  = false;
        float        los_x    = 0.0f;
        float        los_y    = 0.0f;
        float        los_z    = 0.0f;
        std::string  description;
    };

    // Compute the vmap tile grid coordinate from a world coord.
    // Matches Map::EnsureGridLoaded's formula at GridMap.cpp:1544-1545
    // exactly so loadMap()'s (x, y) line up with the tile-file naming
    // (`mapId_TILEY_TILEX.vmtile`).
    int worldToGrid(float worldCoord)
    {
        return int(CENTER_GRID_ID - worldCoord / SIZE_OF_GRIDS);
    }

    // VMapManager2 calls this function pointer via the IsVMAPDisabledForPtr
    // member to ask "is vmap globally disabled for this map+feature?"
    // The pointer is nullptr after default construction (VMapManager2.cpp:62)
    // and the manager calls it unguarded — calling a null pointer segfaults.
    // The harness wants every query to run, so we return false (= not disabled)
    // for everything.
    bool IsVMAPDisabledDummy(uint32 /*entry*/, uint8 /*flags*/)
    {
        return false;
    }

    void printUsage(const char* prog)
    {
        std::cerr
            << "usage: " << prog << " --data-dir <path> --samples <file.csv> --out <file.csv> [--map <id>]\n"
            << "\n"
            << "  --data-dir   Root containing vmaps/, maps/, mmaps/ (typically server_install/)\n"
            << "  --samples    Input CSV of probe points (see STAGE0_PLAN.md)\n"
            << "  --out        Output CSV; one row per input row\n"
            << "  --map <id>   Optional: restrict to a single map id\n";
    }

    // Parse a CSV row into a Sample. Returns false on malformed input.
    // Field order: sample_id, map_id, x, y, z, los_x, los_y, los_z, description
    bool parseRow(const std::string& line, Sample& out)
    {
        std::vector<std::string> fields;
        std::string field;
        std::istringstream ss(line);
        while (std::getline(ss, field, ','))
        {
            fields.push_back(field);
        }
        if (fields.size() < 5)
        {
            return false;
        }

        out.sample_id = fields[0];
        out.map_id    = static_cast<unsigned int>(std::atol(fields[1].c_str()));
        out.x         = static_cast<float>(std::atof(fields[2].c_str()));
        out.y         = static_cast<float>(std::atof(fields[3].c_str()));
        out.z         = static_cast<float>(std::atof(fields[4].c_str()));

        if (fields.size() >= 8 && !fields[5].empty() && !fields[6].empty() && !fields[7].empty())
        {
            out.has_los = true;
            out.los_x   = static_cast<float>(std::atof(fields[5].c_str()));
            out.los_y   = static_cast<float>(std::atof(fields[6].c_str()));
            out.los_z   = static_cast<float>(std::atof(fields[7].c_str()));
        }

        if (fields.size() >= 9)
        {
            out.description = fields[8];
        }
        return true;
    }
}

int main(int argc, char* argv[])
{
    std::string dataDir;
    std::string samplesPath;
    std::string outPath;
    int         restrictMap = -1;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--data-dir" && i + 1 < argc)
        {
            dataDir = argv[++i];
        }
        else if (arg == "--samples" && i + 1 < argc)
        {
            samplesPath = argv[++i];
        }
        else if (arg == "--out" && i + 1 < argc)
        {
            outPath = argv[++i];
        }
        else if (arg == "--map" && i + 1 < argc)
        {
            restrictMap = std::atoi(argv[++i]);
        }
        else if (arg == "--help" || arg == "-h")
        {
            printUsage(argv[0]);
            return 0;
        }
        else
        {
            std::cerr << "unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (dataDir.empty() || samplesPath.empty() || outPath.empty())
    {
        printUsage(argv[0]);
        return 1;
    }

    // Ensure dataDir ends with a separator so vmaps/maps subpaths concatenate cleanly.
    if (dataDir.back() != '/' && dataDir.back() != '\\')
    {
        dataDir.push_back('/');
    }
    std::string vmapBasePath = dataDir + "vmaps";

    std::ifstream in(samplesPath.c_str());
    if (!in)
    {
        std::cerr << "could not open samples file: " << samplesPath << "\n";
        return 1;
    }

    std::ofstream out(outPath.c_str());
    if (!out)
    {
        std::cerr << "could not open output file for write: " << outPath << "\n";
        return 1;
    }

    out << "sample_id,result,height_vmap,area_id,area_flags,los_blocked,liquid_level,liquid_type,notes\n";

    // Direct instantiation: VMapFactory lives outside vmap2 lib so we
    // construct the manager ourselves. Matches how mmap-extractor does it
    // (TerrainBuilder.cpp builds a VMapManager2 per tile-build).
    // IsVMAPDisabledForPtr defaults to nullptr in VMapManager2's ctor but is
    // called unguarded by getHeight/getAreaInfo/GetLiquidLevel — wire the
    // dummy so it returns false (= "vmap not disabled, run the query").
    VMAP::VMapManager2 vmgrImpl;
    vmgrImpl.IsVMAPDisabledForPtr = &IsVMAPDisabledDummy;
    VMAP::IVMapManager* vmgr = &vmgrImpl;

    std::string line;
    std::getline(in, line);  // discard header row
    int  totalRows   = 0;
    int  errorRows   = 0;

    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        Sample s;
        if (!parseRow(line, s))
        {
            std::cerr << "skipping malformed row: " << line << "\n";
            continue;
        }
        if (restrictMap >= 0 && s.map_id != static_cast<unsigned int>(restrictMap))
        {
            continue;
        }
        ++totalRows;

        // Load the vmap tile containing this sample. VMapManager caches loaded
        // tiles so repeated samples in the same tile are O(1) after first load.
        int gx = worldToGrid(s.x);
        int gy = worldToGrid(s.y);

        VMAP::VMAPLoadResult lr = vmgr->loadMap(vmapBasePath.c_str(), s.map_id, gx, gy);
        if (lr == VMAP::VMAP_LOAD_RESULT_ERROR)
        {
            char buf[256];
            snprintf(buf, sizeof(buf), "vmap tile load failed (mapId=%u gx=%d gy=%d)", s.map_id, gx, gy);
            out << s.sample_id << ",ERR,,,,,,," << buf << "\n";
            ++errorRows;
            continue;
        }

        // For LoS pairs, load the second endpoint's tile too.
        if (s.has_los)
        {
            int gx2 = worldToGrid(s.los_x);
            int gy2 = worldToGrid(s.los_y);
            if (gx2 != gx || gy2 != gy)
            {
                vmgr->loadMap(vmapBasePath.c_str(), s.map_id, gx2, gy2);
            }
        }

        // Run the four spatial queries. Float formatting uses %.3f
        // (millimeter precision in yards) per STAGE0_PLAN §14.
        float    height   = vmgr->getHeight(s.map_id, s.x, s.y, s.z, 1000.0f);
        uint32   flags    = 0;
        int32    adtId    = 0;
        int32    rootId   = 0;
        int32    groupId  = 0;
        float    zForArea = s.z;
        bool gotArea = vmgr->getAreaInfo(s.map_id, s.x, s.y, zForArea, flags, adtId, rootId, groupId);

        float    liqLevel = 0.0f;
        float    liqFloor = 0.0f;
        uint32   liqType  = 0;
        bool gotLiquid = vmgr->GetLiquidLevel(s.map_id, s.x, s.y, s.z, 0xFF, liqLevel, liqFloor, liqType);

        bool losBlocked = false;
        bool ranLos     = false;
        if (s.has_los)
        {
            ranLos = true;
            losBlocked = !vmgr->isInLineOfSight(s.map_id, s.x, s.y, s.z, s.los_x, s.los_y, s.los_z);
        }

        // Emit a deterministic row. Empty fields stay empty (not 0)
        // so the diff tool can distinguish "query absent" from "query returned 0".
        char heightBuf[32];
        snprintf(heightBuf, sizeof(heightBuf), "%.3f", height);

        char areaIdBuf[32]   = "";
        char areaFlagsBuf[32] = "";
        if (gotArea)
        {
            snprintf(areaIdBuf, sizeof(areaIdBuf), "%d", rootId);
            snprintf(areaFlagsBuf, sizeof(areaFlagsBuf), "%u", flags);
        }

        char liqLevelBuf[32] = "";
        char liqTypeBuf[32]  = "";
        if (gotLiquid)
        {
            snprintf(liqLevelBuf, sizeof(liqLevelBuf), "%.3f", liqLevel);
            snprintf(liqTypeBuf, sizeof(liqTypeBuf), "%u", liqType);
        }

        const char* losStr = ranLos ? (losBlocked ? "TRUE" : "FALSE") : "";

        out << s.sample_id << ","
            << "OK" << ","
            << heightBuf << ","
            << areaIdBuf << ","
            << areaFlagsBuf << ","
            << losStr << ","
            << liqLevelBuf << ","
            << liqTypeBuf << ","
            << "\n";
    }

    in.close();
    out.close();

    std::cerr
        << "vmap-harness: " << totalRows << " probes, " << errorRows << " errors\n";

    return errorRows == 0 ? 0 : 1;
}
