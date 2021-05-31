#ifndef DETOURINPUTGEOMETRY_H
#define DETOURINPUTGEOMETRY_H

/**
  * This code is mostly taken from recastnavigation's demo project. Just slightly adjusted to fit within godotdetour.
  * Most thanks go to Mikko Mononen and maintainers for this.
  */
//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include <MeshInstance.hpp>
#include <Godot.hpp>
#include <File.hpp>
#include "chunkytrimesh.h"

using namespace godot;
class MeshDataAccumulator;

static const int MAX_CONVEXVOL_PTS = 12;
struct ConvexVolume
{
    float verts[MAX_CONVEXVOL_PTS*3];
    float hmin, hmax;
    int nverts;
    int area;

    // 2D Rectangle around the area, not precise but "good enough" to check if it hits tiles
    float front;
    float back;
    float left;
    float right;

    bool isNew = false;
};

class DetourInputGeometry
{
public:
    static const int MAX_OFFMESH_CONNECTIONS = 256;
    static const int MAX_VOLUMES = 256;

private:
    rcChunkyTriMesh* m_chunkyMesh;
    MeshDataAccumulator* m_mesh;
    float m_meshBMin[3], m_meshBMax[3];

    /// @name Off-Mesh connections.
    ///@{
    float           m_offMeshConVerts[MAX_OFFMESH_CONNECTIONS*3*2];
    float           m_offMeshConRads[MAX_OFFMESH_CONNECTIONS];
    unsigned char   m_offMeshConDirs[MAX_OFFMESH_CONNECTIONS];
    unsigned char   m_offMeshConAreas[MAX_OFFMESH_CONNECTIONS];
    unsigned short  m_offMeshConFlags[MAX_OFFMESH_CONNECTIONS];
    unsigned int    m_offMeshConId[MAX_OFFMESH_CONNECTIONS];
    bool            m_offMeshConNew[MAX_OFFMESH_CONNECTIONS];
    int m_offMeshConCount;
    ///@}

    /// @name Convex Volumes.
    ///@{
    ConvexVolume m_volumes[MAX_VOLUMES];
    int m_volumeCount;
    ///@}

public:

    DetourInputGeometry();
    ~DetourInputGeometry();

    bool loadMesh(class rcContext* ctx, godot::MeshInstance* inputMesh);
    void clearData();

    /**
     * @brief Save the input geometry data to the file.
     */
    bool save(Ref<File> targetFile);

    /**
     * @brief Load the input geometry data from the byte array.
     */
    bool load(Ref<File> sourceFile);

    /// Method to return static mesh data.
    const MeshDataAccumulator* getMesh() const { return m_mesh; }
    const float* getMeshBoundsMin() const { return m_meshBMin; }
    const float* getMeshBoundsMax() const { return m_meshBMax; }
    const float* getNavMeshBoundsMin() const { return m_meshBMin; }
    const float* getNavMeshBoundsMax() const { return m_meshBMax; }
    const rcChunkyTriMesh* getChunkyMesh() const { return m_chunkyMesh; }
    bool raycastMesh(float* src, float* dst, float& tmin);

    /// @name Off-Mesh connections.
    ///@{
    int getOffMeshConnectionCount() const { return m_offMeshConCount; }
    const float* getOffMeshConnectionVerts() const { return m_offMeshConVerts; }
    const float* getOffMeshConnectionRads() const { return m_offMeshConRads; }
    const unsigned char* getOffMeshConnectionDirs() const { return m_offMeshConDirs; }
    const unsigned char* getOffMeshConnectionAreas() const { return m_offMeshConAreas; }
    const unsigned short* getOffMeshConnectionFlags() const { return m_offMeshConFlags; }
    const unsigned int* getOffMeshConnectionId() const { return m_offMeshConId; }
    bool* getOffMeshConnectionNew() { return m_offMeshConNew; }
    void addOffMeshConnection(const float* spos, const float* epos, const float rad,
                              unsigned char bidir, unsigned char area, unsigned short flags);
    void deleteOffMeshConnection(int i);
    void drawOffMeshConnections(struct duDebugDraw* dd, bool hilight = false);
    ///@}

    /// @name Box Volumes.
    ///@{
    int getConvexVolumeCount() const { return m_volumeCount; }
    ConvexVolume* getConvexVolumes() { return m_volumes; }
    void addConvexVolume(const float* verts, const int nverts,
                         const float minh, const float maxh, unsigned char area);
    void deleteConvexVolume(int i);
    void drawConvexVolumes(struct duDebugDraw* dd, bool hilight = false);
    ///@}

private:
    // Explicitly disabled copy constructor and copy assignment operator.
    DetourInputGeometry(const DetourInputGeometry&);
    DetourInputGeometry& operator=(const DetourInputGeometry&);
};

#endif // DETOURINPUTGEOMETRY_H
