#ifndef NAVIGATIONMESHHELPERS_H
#define NAVIGATIONMESHHELPERS_H

#include <DetourCommon.h>
#include <DetourTileCache.h>
#include <DetourTileCacheBuilder.h>
#include "fastlz.h"

class DetourInputGeometry;
struct rcHeightfield;
struct rcHeightfieldLayerSet;
struct rcCompactHeightfield;

// Helper struct for allocating
struct LinearAllocator : public dtTileCacheAlloc
{
    unsigned char* buffer;
    size_t capacity;
    size_t top;
    size_t high;

    LinearAllocator(const size_t cap);

    ~LinearAllocator();

    void resize(const size_t cap);
    virtual void reset();
    virtual void* alloc(const size_t size);
    virtual void free(void* /*ptr*/);
};

// Helper struct for compression
struct FastLZCompressor : public dtTileCacheCompressor
{
    virtual int maxCompressedSize(const int bufferSize);
    virtual dtStatus compress(const unsigned char* buffer, const int bufferSize,
                              unsigned char* compressed, const int /*maxCompressedSize*/, int* compressedSize);
    virtual dtStatus decompress(const unsigned char* compressed, const int compressedSize,
                                unsigned char* buffer, const int maxBufferSize, int* bufferSize);
};

// Flags to determine the kind of areas and which abilities they support
// TODO: Implement different/dynamic areas
enum PolyAreaType
{
    POLY_AREA_INVALID = -1,
    POLY_AREA_GROUND,
    POLY_AREA_ROAD,
    POLY_AREA_WATER,
    POLY_AREA_DOOR,
    POLY_AREA_GRASS,
    POLY_AREA_JUMP,
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
                         unsigned char* polyAreas, unsigned short* polyFlags);
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
    RasterizationContext(int layerCount);

    ~RasterizationContext();

    rcHeightfield* solid;
    unsigned char* triareas;
    rcHeightfieldLayerSet* lset;
    rcCompactHeightfield* chf;
    TileCacheData* tiles;
    int ntiles;
    int numLayers;
};

int calcLayerBufferSize(const int gridWidth, const int gridHeight);
#endif // NAVIGATIONMESHHELPERS_H
