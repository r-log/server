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

#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include "vec3d.h"
#include "modelheaders.h"
#include "vmapexport.h"

class WMOInstance;
class MPQFile;
struct WMODoodadData;

Vec3D fixCoordSystem(Vec3D v);

namespace Doodad
{
    /// Spawn the interior doodads of one placed WMO into the dirfile.
    /// Each doodad becomes one M2 spawn record (mangosthree dirfile
    /// format: mapID + tileX + tileY + flags + adtId + id + pos + rot
    /// + scale + name). No bound is written; TileAssembler computes
    /// the M2 bounding box at assemble time.
    ///
    /// For PR2 (no MOD_PARENT_SPAWN write) the spawn flags are always
    /// MOD_M2; PR3 will extend the signature to thread originalMapId
    /// through and gate MOD_PARENT_SPAWN when mapID != originalMapId.
    void ExtractSet(WMODoodadData const& doodadData,
                    WMOInstance const& wmo,
                    bool isGlobalWmo,
                    uint32 mapID,
                    uint32 tileX,
                    uint32 tileY,
                    FILE* pDirfile);
}

/**
 * @brief
 *
 */
class Model
{
    public:
        ModelHeader header;
        uint32 offsBB_vertices, offsBB_indices;
        Vec3D* BB_vertices, *vertices;
        uint16* BB_indices, *indices;
        size_t nIndices;

        /**
         * @brief
         *
         * @param failedPaths
         * @return bool
         */
        bool open(StringSet& failedPaths);
        /**
         * @brief
         *
         * @param outfilename
         * @return bool
         */
        bool ConvertToVMAPModel(const char* outfilename);

        bool ok;

        /**
         * @brief
         *
         * @param filename
         */
        Model(std::string& filename);
        /**
         * @brief
         *
         */
        ~Model() {_unload();}

    private:
        /**
         * @brief
         *
         */
        void _unload()
        {
            delete[] vertices;
            delete[] indices;
            vertices = NULL;
            indices = NULL;
        }
        std::string filename;
        char outfilename;
};

/**
 * @brief
 *
 */
class ModelInstance
{
    public:
        Model* model;

        uint32 id;
        Vec3D pos, rot;
        unsigned int d1, Scale;
        float w, sc;

        /**
         * @brief
         *
         */
        ModelInstance() {}
        /**
         * @brief
         *
         * @param f
         * @param ModelInstName
         * @param mapID
         * @param tileX
         * @param tileY
         * @param pDirfile
         */
        ModelInstance(MPQFile& f, const char* ModelInstName, uint32 mapID, uint32 tileX, uint32 tileY, FILE* pDirfile);

};

#endif
