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

#define _CRT_SECURE_NO_WARNINGS

#include <cassert>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "mpqfile.h"
#include "model.h"
#include "wmo.h"
#include "vmapexport.h"
#include "adtfile.h"   // GetPlainName / fixnamen helpers

#include <G3D/Matrix3.h>
#include <G3D/Quat.h>
#include <G3D/Vector3.h>
#include <G3D/g3dmath.h>

extern HANDLE WorldMpq;

Model::Model(std::string& filename) : filename(filename), vertices(0), indices(0)
{
}

bool Model::open(StringSet& failedPaths)
{
    MPQFile f(WorldMpq, filename.c_str());

    ok = !f.isEof();

    if (!ok)
    {
        f.close();
        failedPaths.insert(filename);
        return false;
    }

    _unload();

    memcpy(&header, f.getBuffer(), sizeof(ModelHeader));
    if (header.nBoundingTriangles > 0)
    {
        f.seek(0);
        f.seekRelative(header.ofsBoundingVertices);
        vertices = new Vec3D[header.nBoundingVertices];
        f.read(vertices, header.nBoundingVertices * 12);

        for (uint32 i = 0; i < header.nBoundingVertices; i++)
        {
            vertices[i] = fixCoordSystem(vertices[i]);
        }
        f.seek(0);
        f.seekRelative(header.ofsBoundingTriangles);

        indices = new uint16[header.nBoundingTriangles];
        f.read(indices, header.nBoundingTriangles * 2);

        f.close();
    }
    else
    {
        //printf("not included %s\n", filename.c_str());
        f.close();
        return false;
    }
    return true;
}

bool Model::ConvertToVMAPModel(const char* outfilename)
{
    int N[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    FILE* output = fopen(outfilename, "wb");
    if (!output)
    {
        printf("Can't create the output file '%s'\n", outfilename);
        return false;
    }

    fwrite(VMAP::RAW_VMAP_MAGIC, 8, 1, output);
    uint32 nVertices = 0;
    nVertices = header.nBoundingVertices;
    fwrite(&nVertices, sizeof(int), 1, output);
    uint32 nofgroups = 1;
    fwrite(&nofgroups, sizeof(uint32), 1, output);
    fwrite(N, 4 * 3, 1, output); // rootwmoid, flags, groupid
    fwrite(N, sizeof(float), 3 * 2, output); //bbox, only needed for WMO currently
    fwrite(N, 4, 1, output); // liquidflags
    fwrite("GRP ", 4, 1, output);
    uint32 branches = 1;
    int wsize;
    wsize = sizeof(branches) + sizeof(uint32) * branches;
    fwrite(&wsize, sizeof(int), 1, output);
    fwrite(&branches, sizeof(branches), 1, output);
    uint32 nIndexes = 0;
    nIndexes = header.nBoundingTriangles;
    fwrite(&nIndexes, sizeof(uint32), 1, output);
    fwrite("INDX", 4, 1, output);
    wsize = sizeof(uint32) + sizeof(unsigned short) * nIndexes;
    fwrite(&wsize, sizeof(int), 1, output);
    fwrite(&nIndexes, sizeof(uint32), 1, output);
    if (nIndexes > 0)
    {
        fwrite(indices, sizeof(unsigned short), nIndexes, output);
    }
    fwrite("VERT", 4, 1, output);
    wsize = sizeof(int) + sizeof(float) * 3 * nVertices;
    fwrite(&wsize, sizeof(int), 1, output);
    fwrite(&nVertices, sizeof(int), 1, output);
    if (nVertices > 0)
    {
        for (uint32 vpos = 0; vpos < nVertices; ++vpos)
        {
            std::swap(vertices[vpos].y, vertices[vpos].z);
        }
        fwrite(vertices, sizeof(float) * 3, nVertices, output);
    }

    fclose(output);

    return true;
}


Vec3D fixCoordSystem(Vec3D v)
{
    return Vec3D(v.x, v.z, -v.y);
}

Vec3D fixCoordSystem2(Vec3D v)
{
    return Vec3D(v.x, v.z, v.y);
}

ModelInstance::ModelInstance(MPQFile& f, const char* ModelInstName, uint32 mapID, uint32 tileX, uint32 tileY, FILE* pDirfile)
{
    float ff[3];
    f.read(&id, 4);
    f.read(ff, 12);
    pos = fixCoords(Vec3D(ff[0], ff[1], ff[2]));
    f.read(ff, 12);
    rot = Vec3D(ff[0], ff[1], ff[2]);
    f.read(&Scale, 4);
    // Scale factor - divide by 1024. blizzard devs must be on crack, why not just use a float?
    sc = Scale / 1024.0f;

    char tempname[512];
    sprintf(tempname, "%s/%s", szWorkDirWmo, ModelInstName);
    FILE* input;
    input = fopen(tempname, "r+b");

    if (!input)
    {
        //printf("ModelInstance::ModelInstance couldn't open %s\n", tempname);
        return;
    }

    fseek(input, 8, SEEK_SET); // get the correct no of vertices
    int nVertices;
    fread(&nVertices, sizeof(int), 1, input);
    fclose(input);

    if (nVertices == 0)
    {
        return;
    }

    uint16 adtId = 0;// not used for models
    uint32 flags = MOD_M2;
    if (tileX == 65 && tileY == 65)
    {
        flags |= MOD_WORLDSPAWN;
    }
    //write mapID, tileX, tileY, Flags, ID, Pos, Rot, Scale, name
    fwrite(&mapID, sizeof(uint32), 1, pDirfile);
    fwrite(&tileX, sizeof(uint32), 1, pDirfile);
    fwrite(&tileY, sizeof(uint32), 1, pDirfile);
    fwrite(&flags, sizeof(uint32), 1, pDirfile);
    fwrite(&adtId, sizeof(uint16), 1, pDirfile);
    fwrite(&id, sizeof(uint32), 1, pDirfile);
    fwrite(&pos, sizeof(float), 3, pDirfile);
    fwrite(&rot, sizeof(float), 3, pDirfile);
    fwrite(&sc, sizeof(float), 1, pDirfile);
    uint32 nlen = strlen(ModelInstName);
    fwrite(&nlen, sizeof(uint32), 1, pDirfile);
    fwrite(ModelInstName, sizeof(char), nlen, pDirfile);

    /* int realx1 = (int) ((float) pos.x / 533.333333f);
    int realy1 = (int) ((float) pos.z / 533.333333f);
    int realx2 = (int) ((float) pos.x / 533.333333f);
    int realy2 = (int) ((float) pos.z / 533.333333f);

    fprintf(pDirfile,"%s/%s %f,%f,%f_%f,%f,%f %f %d %d %d,%d %d\n",
        MapName,
        ModelInstName,
        (float) pos.x, (float) pos.y, (float) pos.z,
        (float) rot.x, (float) rot.y, (float) rot.z,
        sc,
        nVertices,
        realx1, realy1,
        realx2, realy2
        ); */
}

namespace
{
    // Globally unique spawn id allocator for WMO interior doodads. Avoids
    // clobbering ADT/WDT-placed M2 IDs (which come from MODF.UniqueId,
    // typically well below 2^31). Starts at 0xC0000000 so the high two
    // bits act as a "this is a WMO-doodad spawn" marker.
    std::atomic<uint32> g_doodadSpawnId{ 0xC0000000u };
}

namespace Doodad
{
    void ExtractSet(WMODoodadData const& doodadData,
                    WMOInstance const& wmo,
                    bool /*isGlobalWmo*/,
                    uint32 mapID,
                    uint32 originalMapId,
                    uint32 tileX,
                    uint32 tileY,
                    FILE* pDirfile)
    {
        if (doodadData.Sets.empty() || doodadData.Paths.empty() || doodadData.Spawns.empty())
        {
            return;
        }

        // WMO placement origin in world space (mangosthree's WMOInstance
        // ctor already applied fixCoords and the 0,0 -> 32*tile centering
        // for global WMOs, so wmo.pos is the world coord to add to).
        G3D::Vector3 wmoPosition(wmo.pos.x, wmo.pos.y, wmo.pos.z);

        // MODF.Rotation is stored as raw Euler degrees (pitch/yaw/roll)
        // before fixCoords. The convention mirrors TC's reading at
        // vmap4_extractor/model.cpp:217: fromEulerAnglesZYX(rotY, rotX, rotZ).
        G3D::Matrix3 wmoRotation = G3D::Matrix3::fromEulerAnglesZYX(
            G3D::toRadians(wmo.rot.y),
            G3D::toRadians(wmo.rot.x),
            G3D::toRadians(wmo.rot.z));

        // TC's extractSingleSet lambda — iterate the WMODoodadData's
        // References set (= doodads actually referenced by any WMO group,
        // filtered for valid model extraction) and keep only those whose
        // doodad index falls in the requested MODS range.
        auto extractSingleSet = [&](WMO::MODS const& doodadSetData)
        {
            for (uint16 doodadIndex : doodadData.References)
            {
                if (doodadIndex < doodadSetData.StartIndex ||
                    doodadIndex >= doodadSetData.StartIndex + doodadSetData.Count)
                {
                    continue;
                }
                if (doodadIndex >= doodadData.Spawns.size())
                {
                    continue;
                }
                WMO::MODD const& doodad = doodadData.Spawns[doodadIndex];

            // Resolve model name from the MODN string blob.
            if (doodad.NameIndex >= doodadData.Paths.size())
            {
                continue;
            }
            char const* pathStart = doodadData.Paths.data() + doodad.NameIndex;
            char ModelInstName[1024];
            const char* plain = GetPlainName(pathStart);
            std::snprintf(ModelInstName, sizeof(ModelInstName), "%s", plain);
            uint32 nlen = static_cast<uint32>(std::strlen(ModelInstName));
            if (nlen < 4)
            {
                continue;
            }

            // .mdx/.mdl -> .m2 rewrite. WMORoot::open already applied
            // fixnamen to the Paths blob (mirroring adtfile.cpp:162 for
            // MMDX entries), so the extension is guaranteed lowercase by
            // the time we get here and the strcmp matches first try.
            char* ext = &ModelInstName[nlen - 4];
            if (!std::strcmp(ext, ".mdx") || !std::strcmp(ext, ".mdl"))
            {
                ModelInstName[nlen - 2] = '2';
                ModelInstName[nlen - 1] = '\0';
                nlen = static_cast<uint32>(std::strlen(ModelInstName));
            }

            // Skip if the extracted .m2 isn't on disk (failed earlier or
            // model is not LoS-relevant per ExtractSingleModel's filter).
            char tempname[1100];
            std::snprintf(tempname, sizeof(tempname), "%s/%s", szWorkDirWmo, ModelInstName);
            FILE* input = std::fopen(tempname, "r+b");
            if (!input)
            {
                continue;
            }
            std::fseek(input, 8, SEEK_SET);
            int nVertices = 0;
            std::size_t count = std::fread(&nVertices, sizeof(int), 1, input);
            std::fclose(input);
            if (count != 1 || nVertices == 0)
            {
                continue;
            }

            // World position = wmo.pos + R(wmo.rot) * doodad.localPos
            G3D::Vector3 localPos(doodad.Position[0], doodad.Position[1], doodad.Position[2]);
            G3D::Vector3 worldPos = wmoPosition + (wmoRotation * localPos);

            // World rotation = compose doodad quat with WMO rotation matrix,
            // then convert back to Euler. Order mirrors TC's model.cpp:268-274.
            G3D::Quat doodadQuat(doodad.RotationX, doodad.RotationY, doodad.RotationZ, doodad.RotationW);
            G3D::Matrix3 worldRot = doodadQuat.toRotationMatrix() * wmoRotation;
            float ex = 0.0f, ey = 0.0f, ez = 0.0f;
            worldRot.toEulerAnglesXYZ(ez, ex, ey);

            Vec3D rotation;
            rotation.x = G3D::toDegrees(ex);
            rotation.y = G3D::toDegrees(ey);
            rotation.z = G3D::toDegrees(ez);

            uint16 adtId = 0;            // not used for models
            uint32 flags = MOD_M2;
            if (mapID != originalMapId)
            {
                // PR3: spawn inherited from a parent WDT's ADT data.
                // TileAssembler will route this into ParentTileEntries.
                flags |= MOD_PARENT_SPAWN;
            }
            uint32 uniqueId = g_doodadSpawnId.fetch_add(1, std::memory_order_relaxed);

            // Write a spawn record matching ModelInstance's layout
            // (mapID, tileX, tileY, flags, adtId, id, pos, rot, sc, nlen, name).
            Vec3D outPos(worldPos.x, worldPos.y, worldPos.z);
            float scale = doodad.Scale;
            std::fwrite(&mapID, sizeof(uint32), 1, pDirfile);
            std::fwrite(&tileX, sizeof(uint32), 1, pDirfile);
            std::fwrite(&tileY, sizeof(uint32), 1, pDirfile);
            std::fwrite(&flags, sizeof(uint32), 1, pDirfile);
            std::fwrite(&adtId, sizeof(uint16), 1, pDirfile);
            std::fwrite(&uniqueId, sizeof(uint32), 1, pDirfile);
            std::fwrite(&outPos, sizeof(float), 3, pDirfile);
            std::fwrite(&rotation, sizeof(float), 3, pDirfile);
            std::fwrite(&scale, sizeof(float), 1, pDirfile);
            std::fwrite(&nlen, sizeof(uint32), 1, pDirfile);
            std::fwrite(ModelInstName, sizeof(char), nlen, pDirfile);
            }   // end of for (doodadIndex : References)
        };      // end of extractSingleSet lambda

        // TC model.cpp:320-323: always emit set 0 (default) and, if the
        // placement asked for a different set, that one too.
        extractSingleSet(doodadData.Sets[0]);
        std::size_t setIndex = static_cast<std::size_t>(wmo.doodadset);
        if (setIndex != 0 && setIndex < doodadData.Sets.size())
        {
            extractSingleSet(doodadData.Sets[setIndex]);
        }
    }
}
