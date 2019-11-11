#include "navigationmeshhelpers.h"
#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <cstring>
#include "detourinputgeometry.h"

LinearAllocator::LinearAllocator(const size_t cap)
    : buffer(0)
    , capacity(0)
    , top(0)
    , high(0)
{
    resize(cap);
}

LinearAllocator::~LinearAllocator()
{
    dtFree(buffer);
}

void
LinearAllocator::resize(const size_t cap)
{
    if (buffer) dtFree(buffer);
    buffer = (unsigned char*)dtAlloc(cap, DT_ALLOC_PERM);
    capacity = cap;
}


void
LinearAllocator::reset()
{
    high = dtMax(high, top);
    top = 0;
}

void*
LinearAllocator::alloc(const size_t size)
{
    if (!buffer)
        return 0;
    if (top+size > capacity)
        return 0;
    unsigned char* mem = &buffer[top];
    top += size;
    return mem;
}

void
LinearAllocator::free(void* /*ptr*/)
{
    // Empty
}

int
FastLZCompressor::maxCompressedSize(const int bufferSize)
{
    return (int)(bufferSize* 1.05f);
}

dtStatus
FastLZCompressor::compress(const unsigned char* buffer, const int bufferSize,
                          unsigned char* compressed, const int /*maxCompressedSize*/, int* compressedSize)
{
    *compressedSize = fastlz_compress((const void *const)buffer, bufferSize, compressed);
    return DT_SUCCESS;
}

dtStatus
FastLZCompressor::decompress(const unsigned char* compressed, const int compressedSize,
                            unsigned char* buffer, const int maxBufferSize, int* bufferSize)
{
    *bufferSize = fastlz_decompress(compressed, compressedSize, buffer, maxBufferSize);
    return *bufferSize < 0 ? DT_FAILURE : DT_SUCCESS;
}

void
MeshProcess::process(struct dtNavMeshCreateParams* params,
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

RasterizationContext::RasterizationContext(int layerCount) :
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

RasterizationContext::~RasterizationContext()
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

int calcLayerBufferSize(const int gridWidth, const int gridHeight)
{
    const int headerSize = dtAlign4(sizeof(dtTileCacheLayerHeader));
    const int gridSize = gridWidth * gridHeight;
    return headerSize + gridSize*4;
}
