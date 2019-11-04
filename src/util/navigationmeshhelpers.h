#ifndef NAVIGATIONMESHHELPERS_H
#define NAVIGATIONMESHHELPERS_H

#include <DetourCommon.h>
#include <DetourTileCache.h>
#include <DetourTileCacheBuilder.h>
#include "fastlz.h"

// Helper struct for allocating
struct LinearAllocator : public dtTileCacheAlloc
{
    unsigned char* buffer;
    size_t capacity;
    size_t top;
    size_t high;

    LinearAllocator(const size_t cap) : buffer(0), capacity(0), top(0), high(0)
    {
        resize(cap);
    }

    ~LinearAllocator()
    {
        dtFree(buffer);
    }

    void resize(const size_t cap)
    {
        if (buffer) dtFree(buffer);
        buffer = (unsigned char*)dtAlloc(cap, DT_ALLOC_PERM);
        capacity = cap;
    }

    virtual void reset()
    {
        high = dtMax(high, top);
        top = 0;
    }

    virtual void* alloc(const size_t size)
    {
        if (!buffer)
            return 0;
        if (top+size > capacity)
            return 0;
        unsigned char* mem = &buffer[top];
        top += size;
        return mem;
    }

    virtual void free(void* /*ptr*/)
    {
        // Empty
    }
};

// Helper struct for compression
struct FastLZCompressor : public dtTileCacheCompressor
{
    virtual int maxCompressedSize(const int bufferSize)
    {
        return (int)(bufferSize* 1.05f);
    }

    virtual dtStatus compress(const unsigned char* buffer, const int bufferSize,
                              unsigned char* compressed, const int /*maxCompressedSize*/, int* compressedSize)
    {
        *compressedSize = fastlz_compress((const void *const)buffer, bufferSize, compressed);
        return DT_SUCCESS;
    }

    virtual dtStatus decompress(const unsigned char* compressed, const int compressedSize,
                                unsigned char* buffer, const int maxBufferSize, int* bufferSize)
    {
        *bufferSize = fastlz_decompress(compressed, compressedSize, buffer, maxBufferSize);
        return *bufferSize < 0 ? DT_FAILURE : DT_SUCCESS;
    }
};

// Flags to determine the kind of areas and which abilities they support
// TODO: Implement different areas
enum PolyAreaType
{
    POLY_AREA_INVALID = -1,
    POLY_AREA_GROUND,
    POLY_AREA_GRASS,
    POLY_AREA_ROAD,
    POLY_AREA_WATER,
    POLY_AREA_DOOR,
    NUM_POLY_AREAS
};
enum SamplePolyFlags
{
    POLY_FLAGS_WALK		= 0x01,		// Ability to walk (ground, grass, road)
    POLY_FLAGS_SWIM		= 0x02,		// Ability to swim (water).
    POLY_FLAGS_DOOR		= 0x04,		// Ability to move through doors.
    POLY_FLAGS_JUMP		= 0x08,		// Ability to jump.
    POLY_FLAGS_DISABLED	= 0x10,		// Disabled polygon
    POLY_FLAGS_ALL		= 0xffff	// All abilities.
};

// Helper class for setting area flags and offMesh connections
struct MeshProcess : public dtTileCacheMeshProcess
{
    DetourInputGeometry* _geom;

    inline MeshProcess() : _geom(0)
    {
    }

    inline void init(DetourInputGeometry* geom)
    {
        _geom = geom;
    }

    virtual void process(struct dtNavMeshCreateParams* params,
                         unsigned char* polyAreas, unsigned short* polyFlags)
    {
        // Update poly flags from areas.
        for (int i = 0; i < params->polyCount; ++i)
        {
            if (polyAreas[i] == DT_TILECACHE_WALKABLE_AREA)
            {
                polyAreas[i] = POLY_AREA_GROUND;
            }

            if (polyAreas[i] == POLY_AREA_GROUND ||
                polyAreas[i] == POLY_AREA_GRASS ||
                polyAreas[i] == POLY_AREA_ROAD)
            {
                polyFlags[i] = POLY_FLAGS_WALK;
            }
            else if (polyAreas[i] == POLY_AREA_WATER)
            {
                polyFlags[i] = POLY_FLAGS_SWIM;
            }
            else if (polyAreas[i] == POLY_AREA_DOOR)
            {
                polyFlags[i] = POLY_FLAGS_WALK | POLY_FLAGS_DOOR;
            }
        }

        // Pass in off-mesh connections.
        if (_geom)
        {
            params->offMeshConVerts = _geom->getOffMeshConnectionVerts();
            params->offMeshConRad = _geom->getOffMeshConnectionRads();
            params->offMeshConDir = _geom->getOffMeshConnectionDirs();
            params->offMeshConAreas = _geom->getOffMeshConnectionAreas();
            params->offMeshConFlags = _geom->getOffMeshConnectionFlags();
            params->offMeshConUserID = _geom->getOffMeshConnectionId();
            params->offMeshConCount = _geom->getOffMeshConnectionCount();
        }
    }
};

// Helper struct to store cache data
struct TileCacheData
{
    unsigned char* data;
    int dataSize;
};

// Helper struct for rasterization
struct RasterizationContext
{
    RasterizationContext(int layerCount) :
        solid(0),
        triareas(0),
        lset(0),
        chf(0),
        ntiles(0),
        numLayers(layerCount)
    {
        tiles = new TileCacheData[numLayers];
        memset(tiles, 0, sizeof(TileCacheData) * numLayers);
    }

    ~RasterizationContext()
    {
        rcFreeHeightField(solid);
        delete [] triareas;
        rcFreeHeightfieldLayerSet(lset);
        rcFreeCompactHeightfield(chf);
        for (int i = 0; i < numLayers; ++i)
        {
            dtFree(tiles[i].data);
            tiles[i].data = 0;
        }
        delete [] tiles;
    }

    rcHeightfield* solid;
    unsigned char* triareas;
    rcHeightfieldLayerSet* lset;
    rcCompactHeightfield* chf;
    TileCacheData* tiles;
    int ntiles;
    int numLayers;
};

static int calcLayerBufferSize(const int gridWidth, const int gridHeight)
{
    const int headerSize = dtAlign4(sizeof(dtTileCacheLayerHeader));
    const int gridSize = gridWidth * gridHeight;
    return headerSize + gridSize*4;
}

#endif // NAVIGATIONMESHHELPERS_H
