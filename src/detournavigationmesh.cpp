#include "detournavigationmesh.h"
#include <DetourTileCache.h>
#include <SurfaceTool.hpp>
#include <File.hpp>
#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourNavMeshBuilder.h>
#include <DetourDebugDraw.h>
#include <DetourCrowd.h>
#include <climits>
#include "util/detourinputgeometry.h"
#include "util/recastcontext.h"
#include "util/navigationmeshhelpers.h"
#include "util/meshdataaccumulator.h"
#include "util/godotdetourdebugdraw.h"
#include "detourobstacle.h"

using namespace godot;

#define NAVMESH_SAVE_VERSION 1

void
DetourNavigationMeshParameters::_register_methods()
{
    register_property<DetourNavigationMeshParameters, Vector2>("cellSize", &DetourNavigationMeshParameters::cellSize, Vector2(0.0f, 0.0f));
    register_property<DetourNavigationMeshParameters, int>("maxNumAgents", &DetourNavigationMeshParameters::maxNumAgents, 256);
    register_property<DetourNavigationMeshParameters, float>("maxAgentSlope", &DetourNavigationMeshParameters::maxAgentSlope, 0.0f);
    register_property<DetourNavigationMeshParameters, float>("maxAgentHeight", &DetourNavigationMeshParameters::maxAgentHeight, 0.0f);
    register_property<DetourNavigationMeshParameters, float>("maxAgentClimb", &DetourNavigationMeshParameters::maxAgentClimb, 0.0f);
    register_property<DetourNavigationMeshParameters, float>("maxAgentRadius", &DetourNavigationMeshParameters::maxAgentRadius, 0.0f);
    register_property<DetourNavigationMeshParameters, float>("maxEdgeLength", &DetourNavigationMeshParameters::maxEdgeLength, 0.0f);
    register_property<DetourNavigationMeshParameters, float>("maxSimplificationError", &DetourNavigationMeshParameters::maxSimplificationError, 0.0f);
    register_property<DetourNavigationMeshParameters, int>("minNumCellsPerIsland", &DetourNavigationMeshParameters::minNumCellsPerIsland, 0);
    register_property<DetourNavigationMeshParameters, int>("minCellSpanCount", &DetourNavigationMeshParameters::minCellSpanCount, 0);
    register_property<DetourNavigationMeshParameters, int>("maxVertsPerPoly", &DetourNavigationMeshParameters::maxVertsPerPoly, 0);
    register_property<DetourNavigationMeshParameters, int>("tileSize", &DetourNavigationMeshParameters::tileSize, 0);
    register_property<DetourNavigationMeshParameters, int>("layersPerTile", &DetourNavigationMeshParameters::layersPerTile, 0);
    register_property<DetourNavigationMeshParameters, float>("detailSampleDistance", &DetourNavigationMeshParameters::detailSampleDistance, 0.0f);
    register_property<DetourNavigationMeshParameters, float>("detailSampleMaxError", &DetourNavigationMeshParameters::detailSampleMaxError, 0.0f);
}

void
DetourNavigationMesh::_register_methods()
{
    // TODO: Do we really need to expose this class to GDScript?
}

DetourNavigationMesh::DetourNavigationMesh()
    : _recastContext(nullptr)
    , _rcConfig(nullptr)
    , _tileCache(nullptr)
    , _navMesh(nullptr)
    , _navQuery(nullptr)
    , _crowd(nullptr)
    , _allocator(nullptr)
    , _compressor(nullptr)
    , _meshProcess(nullptr)
    , _inputGeom(nullptr)
    , _maxAgentSlope(0.0f)
    , _maxAgentClimb(0.0f)
    , _maxAgentHeight(0.0f)
    , _maxAgentRadius(0.0f)
    , _maxObstacles(0)
    // TODO: is this enough? Demo has only 32, seems very little for tall vs flat levels?
    , _maxLayers(32)
    , _navQueryMaxNodes(2048)
    , _tileSize(0)
    , _layersPerTile(4)
    , _navMeshIndex(0)
{
    _rcConfig = new rcConfig();
    _navQuery = dtAllocNavMeshQuery();
    _crowd = dtAllocCrowd();
    _allocator = new LinearAllocator(_maxLayers * 1000);
    _compressor = new FastLZCompressor();
    _meshProcess = new MeshProcess();
}

DetourNavigationMesh::~DetourNavigationMesh()
{
    dtFreeCrowd(_crowd);
    dtFreeNavMeshQuery(_navQuery);
    dtFreeNavMesh(_navMesh);
    dtFreeTileCache(_tileCache);
    delete _allocator;
    delete _compressor;
    delete _meshProcess;
    delete _rcConfig;
}

bool
DetourNavigationMesh::initialize(DetourInputGeometry* inputGeom, Ref<DetourNavigationMeshParameters> params, int maxObstacles, RecastContext* recastContext, int index)
{
    Godot::print("DTNavMeshInitialize: Initializing navigation mesh");

    _navMeshIndex = index;
    _recastContext = recastContext;

    DetourNavigationMeshParameters* para = *params;
    _inputGeom = inputGeom;
    _meshProcess->init(_inputGeom);
    _maxAgentSlope = para->maxAgentSlope;
    _maxAgentClimb = para->maxAgentClimb;
    _maxAgentHeight = para->maxAgentHeight;
    _maxAgentRadius = para->maxAgentRadius;
    _maxAgents = para->maxNumAgents;
    _maxObstacles = maxObstacles;
    _cellSize = para->cellSize;
    _tileSize = para->tileSize;
    _layersPerTile = para->layersPerTile;

    // Init cache
    const float* bmin = _inputGeom->getNavMeshBoundsMin();
    const float* bmax = _inputGeom->getNavMeshBoundsMax();
    int gw = 0, gh = 0;
    rcCalcGridSize(bmin, bmax, _cellSize.x, &gw, &gh);
    const int ts = _tileSize;
    const int tw = (gw + ts-1) / ts;
    const int th = (gh + ts-1) / ts;

    Godot::print(String("DTNavMeshInitialize: tile sizes {0} {1} {2}").format(Array::make(ts, tw, th)));
    Godot::print(String("DTNavMeshInitialize: grid sizes {0} {1}").format(Array::make(gw, gh)));
    Godot::print(String("DTNavMeshInitialize: bounds {0} {1}").format(Array::make(Vector3(bmin[0], bmin[1], bmin[2]), Vector3(bmax[0], bmax[1], bmax[2]))));

    // Generation parameters
    rcConfig& cfg = *_rcConfig;
    memset(_rcConfig, 0, sizeof(cfg));
    cfg.cs = _cellSize.x;
    cfg.ch = _cellSize.y;
    cfg.walkableSlopeAngle = _maxAgentSlope;
    cfg.walkableHeight = (int)ceilf(_maxAgentHeight / cfg.ch);
    cfg.walkableClimb = (int)floorf(_maxAgentClimb / cfg.ch);
    cfg.walkableRadius = (int)ceilf(_maxAgentRadius / cfg.cs);
    cfg.maxEdgeLen = (int)(para->maxEdgeLength / para->cellSize.x);
    cfg.maxSimplificationError = para->maxSimplificationError;
    cfg.minRegionArea = para->minNumCellsPerIsland;
    cfg.mergeRegionArea = para->minCellSpanCount;
    cfg.maxVertsPerPoly = para->maxVertsPerPoly;
    cfg.tileSize = _tileSize;
    cfg.borderSize = cfg.walkableRadius + 3; // Reserve enough padding.
    cfg.width = cfg.tileSize + cfg.borderSize * 2;
    cfg.height = cfg.tileSize + cfg.borderSize * 2;
    cfg.detailSampleDist = para->detailSampleDistance < 0.9f ? 0 : _cellSize.x * para->detailSampleDistance;
    cfg.detailSampleMaxError = _cellSize.y * para->detailSampleMaxError;
    rcVcopy(cfg.bmin, bmin);
    rcVcopy(cfg.bmax, bmax);

    // Tile cache params.
    dtTileCacheParams tcparams;
    memset(&tcparams, 0, sizeof(tcparams));
    rcVcopy(tcparams.orig, bmin);
    tcparams.cs = _cellSize.x;
    tcparams.ch = _cellSize.y;
    tcparams.width = _tileSize;
    tcparams.height = _tileSize;
    tcparams.walkableHeight = _maxAgentHeight;
    tcparams.walkableRadius = _maxAgentRadius;
    tcparams.walkableClimb = _maxAgentClimb;
    tcparams.maxSimplificationError = para->maxSimplificationError;
    tcparams.maxTiles = tw * th * _layersPerTile;
    tcparams.maxObstacles = _maxObstacles;
    Godot::print("DTNavMeshInitialize: Assigned parameters...");

    dtFreeTileCache(_tileCache);
    _tileCache = dtAllocTileCache();
    if (!_tileCache)
    {
        ERR_PRINT("DTNavMeshInitialize: Could not allocate tile cache.");
        return false;
    }
    dtStatus status = _tileCache->init(&tcparams, _allocator, _compressor, _meshProcess);
    if (dtStatusFailed(status))
    {
        ERR_PRINT("DTNavMeshInitialize: Could not init tile cache.");
        return false;
    }
    Godot::print("DTNavMeshInitialize: Initialized tile cache...");

    dtFreeNavMesh(_navMesh);
    _navMesh = dtAllocNavMesh();
    if (!_navMesh)
    {
        ERR_PRINT("DTNavMeshInitialize: Could not allocate navmesh.");
        return false;
    }

    // Max tiles and max polys affect how the tile IDs are caculated.
    // There are 22 bits available for identifying a tile and a polygon.
    int tileBits = rcMin((int)dtIlog2(dtNextPow2(tw*th*_layersPerTile)), 14);
    if (tileBits > 14) tileBits = 14;
    int polyBits = 22 - tileBits;
    int maxTiles = 1 << tileBits;
    int maxPolysPerTile = 1 << polyBits;

    dtNavMeshParams nmParams;
    memset(&nmParams, 0, sizeof(nmParams));
    rcVcopy(nmParams.orig, bmin);
    nmParams.tileWidth = _tileSize * _cellSize.x;
    nmParams.tileHeight = _tileSize * _cellSize.x;
    nmParams.maxTiles = maxTiles;
    nmParams.maxPolys = maxPolysPerTile;

    status = _navMesh->init(&nmParams);
    if (dtStatusFailed(status))
    {
        ERR_PRINT("DTNavMeshInitialize: Could not init Detour navmesh.");
        return false;
    }
    Godot::print("DTNavMeshInitialize: Initialized Detour navmesh...");

    status = _navQuery->init(_navMesh, _navQueryMaxNodes);
    if (dtStatusFailed(status))
    {
        ERR_PRINT("DTNavMeshInitialize: Could not init Detour navmesh query");
        return false;
    }
    Godot::print("DTNavMeshInitialize: Initialized Detour navmesh query...");

    // Preprocess tiles
    _recastContext->resetTimers();

    int cacheLayerCount = 0;
    int cacheCompressedSize = 0;
    int cacheRawSize = 0;
    for (int y = 0; y < th; ++y)
    {
        for (int x = 0; x < tw; ++x)
        {
            TileCacheData tiles[_maxLayers];
            memset(tiles, 0, sizeof(tiles));
            int ntiles = rasterizeTileLayers(x, y, cfg, tiles, _maxLayers);

            for (int i = 0; i < ntiles; ++i)
            {
                TileCacheData* tile = &tiles[i];
                status = _tileCache->addTile(tile->data, tile->dataSize, DT_COMPRESSEDTILE_FREE_DATA, 0);
                if (dtStatusFailed(status))
                {
                    ERR_PRINT(String("DTNavMeshInitialize: Unable to add tile: {0}").format(Array::make(status)));
                    dtFree(tile->data);
                    tile->data = 0;
                    continue;
                }

                cacheLayerCount++;
                cacheCompressedSize += tile->dataSize;
                cacheRawSize += calcLayerBufferSize(tcparams.width, tcparams.height);
            }
        }
    }
    Godot::print("DTNavMeshInitialize: Processed input mesh..");

    // Build initial meshes
    _recastContext->startTimer(RC_TIMER_TOTAL);
    for (int y = 0; y < th; ++y)
    {
        for (int x = 0; x < tw; ++x)
        {
            status = _tileCache->buildNavMeshTilesAt(x, y, _navMesh);
            if (dtStatusFailed(status))
            {
                ERR_PRINT(String("DTNavMeshInitialize: Could not build nav mesh tiles at {0} {1}").format(Array::make(x, y)));
                return false;
            }
        }
    }
    _recastContext->stopTimer(RC_TIMER_TOTAL);

    // Statistics
    int cacheBuildTimeMs = _recastContext->getAccumulatedTime(RC_TIMER_TOTAL)/1000.0f;
    size_t cacheBuildMemUsage = static_cast<unsigned int>(_allocator->high);
    const dtNavMesh* nav = _navMesh;
    int navmeshMemUsage = 0;
    for (int i = 0; i < nav->getMaxTiles(); ++i)
    {
        const dtMeshTile* tile = nav->getTile(i);
        if (tile->header)
            navmeshMemUsage += tile->dataSize;
    }
    Godot::print(String("DTNavMeshInitialize: navmesh memory usage: {0} bytes").format(Array::make(navmeshMemUsage)));

    // Initialize the crowd
    if (!initializeCrowd())
    {
        ERR_PRINT("DTNavMeshInitialize: Unable to initialize crowd.");
        return false;
    }

    return true;
}

bool
DetourNavigationMesh::save(Ref<File> targetFile)
{
    // Sanity check
    if (_tileCache == nullptr)
    {
        ERR_PRINT("DTNavMeshSave: Cannot save, no tile cache present.");
        return false;
    }

    // Store version
    targetFile->store_16(NAVMESH_SAVE_VERSION);

    // Properties
    targetFile->store_32(_navMeshIndex);
    targetFile->store_var(_cellSize, true);
    targetFile->store_32(_tileSize);
    targetFile->store_32(_maxAgents);
    targetFile->store_32(_maxObstacles);
    targetFile->store_32(_maxLayers);
    targetFile->store_32(_layersPerTile);
    targetFile->store_32(_navQueryMaxNodes);
    targetFile->store_float(_maxAgentSlope);
    targetFile->store_float(_maxAgentHeight);
    targetFile->store_float(_maxAgentClimb);
    targetFile->store_float(_maxAgentRadius);

    // rcConfig
    {
        targetFile->store_float(_rcConfig->bmin[0]);
        targetFile->store_float(_rcConfig->bmin[1]);
        targetFile->store_float(_rcConfig->bmin[2]);
        targetFile->store_float(_rcConfig->bmax[0]);
        targetFile->store_float(_rcConfig->bmax[1]);
        targetFile->store_float(_rcConfig->bmax[2]);
        targetFile->store_32(_rcConfig->borderSize);
        targetFile->store_float(_rcConfig->ch);
        targetFile->store_float(_rcConfig->cs);
        targetFile->store_float(_rcConfig->detailSampleDist);
        targetFile->store_float(_rcConfig->detailSampleMaxError);
        targetFile->store_32(_rcConfig->height);
        targetFile->store_32(_rcConfig->maxEdgeLen);
        targetFile->store_float(_rcConfig->maxSimplificationError);
        targetFile->store_32(_rcConfig->maxVertsPerPoly);
        targetFile->store_32(_rcConfig->mergeRegionArea);
        targetFile->store_32(_rcConfig->minRegionArea);
        targetFile->store_32(_rcConfig->tileSize);
        targetFile->store_32(_rcConfig->walkableClimb);
        targetFile->store_32(_rcConfig->walkableHeight);
        targetFile->store_32(_rcConfig->walkableRadius);
        targetFile->store_float(_rcConfig->walkableSlopeAngle);
        targetFile->store_32(_rcConfig->width);
    }

    // Tiles
    targetFile->store_32(_tileCache->getTileCount());
    for (int i = 0; i < _tileCache->getTileCount(); ++i)
    {
        const dtCompressedTile* tile = _tileCache->getTile(i);
        if (!tile || !tile->header || !tile->dataSize)
        {
            targetFile->store_32(0);
            continue;
        }

        targetFile->store_32(tile->dataSize);
        PoolByteArray array;
        array.resize(tile->dataSize);
        PoolByteArray::Write writer = array.write();
        memcpy(writer.ptr(), tile->data, tile->dataSize);
        targetFile->store_buffer(array);
    }

    return true;
}

bool
DetourNavigationMesh::load(DetourInputGeometry* inputGeom, RecastContext* recastContext, Ref<File> sourceFile)
{
    _inputGeom = inputGeom;
    _recastContext = recastContext;

    int version = sourceFile->get_16();

    if (version == NAVMESH_SAVE_VERSION)
    {
        // Properties
        _navMeshIndex = sourceFile->get_32();
        _cellSize = sourceFile->get_var(true);
        _tileSize = sourceFile->get_32();
        _maxAgents = sourceFile->get_32();
        _maxObstacles = sourceFile->get_32();
        _maxLayers = sourceFile->get_32();
        _layersPerTile = sourceFile->get_32();
        _navQueryMaxNodes = sourceFile->get_32();
        _maxAgentSlope = sourceFile->get_float();
        _maxAgentHeight = sourceFile->get_float();
        _maxAgentClimb = sourceFile->get_float();
        _maxAgentRadius = sourceFile->get_float();

        // rcConfig
        {
            _rcConfig->bmin[0] = sourceFile->get_float();
            _rcConfig->bmin[1] = sourceFile->get_float();
            _rcConfig->bmin[2] = sourceFile->get_float();
            _rcConfig->bmax[0] = sourceFile->get_float();
            _rcConfig->bmax[1] = sourceFile->get_float();
            _rcConfig->bmax[2] = sourceFile->get_float();
            _rcConfig->borderSize = sourceFile->get_32();
            _rcConfig->ch = sourceFile->get_float();
            _rcConfig->cs = sourceFile->get_float();
            _rcConfig->detailSampleDist = sourceFile->get_float();
            _rcConfig->detailSampleMaxError = sourceFile->get_float();
            _rcConfig->height = sourceFile->get_32();
            _rcConfig->maxEdgeLen = sourceFile->get_32();
            _rcConfig->maxSimplificationError = sourceFile->get_float();
            _rcConfig->maxVertsPerPoly = sourceFile->get_32();
            _rcConfig->mergeRegionArea = sourceFile->get_32();
            _rcConfig->minRegionArea = sourceFile->get_32();
            _rcConfig->tileSize = sourceFile->get_32();
            _rcConfig->walkableClimb = sourceFile->get_32();
            _rcConfig->walkableHeight = sourceFile->get_32();
            _rcConfig->walkableRadius = sourceFile->get_32();
            _rcConfig->walkableSlopeAngle = sourceFile->get_float();
            _rcConfig->width = sourceFile->get_32();
        }

        // NavMesh
        dtFreeNavMesh(_navMesh);
        _navMesh = dtAllocNavMesh();
        if (!_navMesh)
        {
            ERR_PRINT("DTNavMeshLoad: Could not allocate navmesh.");
            return false;
        }

        // Init cache
        const float* bmin = _inputGeom->getNavMeshBoundsMin();
        const float* bmax = _inputGeom->getNavMeshBoundsMax();
        int gw = 0, gh = 0;
        rcCalcGridSize(bmin, bmax, _cellSize.x, &gw, &gh);
        const int ts = _tileSize;
        const int tw = (gw + ts-1) / ts;
        const int th = (gh + ts-1) / ts;

        // Tile cache params.
        dtTileCacheParams tcparams;
        memset(&tcparams, 0, sizeof(tcparams));
        rcVcopy(tcparams.orig, bmin);
        tcparams.cs = _cellSize.x;
        tcparams.ch = _cellSize.y;
        tcparams.width = _tileSize;
        tcparams.height = _tileSize;
        tcparams.walkableHeight = _maxAgentHeight;
        tcparams.walkableRadius = _maxAgentRadius;
        tcparams.walkableClimb = _maxAgentClimb;
        tcparams.maxSimplificationError = _rcConfig->maxSimplificationError;
        tcparams.maxTiles = tw * th * _layersPerTile;
        tcparams.maxObstacles = _maxObstacles;

        dtFreeTileCache(_tileCache);
        _tileCache = dtAllocTileCache();
        if (!_tileCache)
        {
            ERR_PRINT("DTNavMeshLoad: Could not allocate tile cache.");
            return false;
        }
        dtStatus status = _tileCache->init(&tcparams, _allocator, _compressor, _meshProcess);
        if (dtStatusFailed(status))
        {
            ERR_PRINT("DTNavMeshLoad: Could not init tile cache.");
            return false;
        }

        // Max tiles and max polys affect how the tile IDs are caculated.
        // There are 22 bits available for identifying a tile and a polygon.
        int tileBits = rcMin((int)dtIlog2(dtNextPow2(tw * th * _layersPerTile)), 14);
        if (tileBits > 14) tileBits = 14;
        int polyBits = 22 - tileBits;
        int maxTiles = 1 << tileBits;
        int maxPolysPerTile = 1 << polyBits;

        // Init NavMesh
        dtNavMeshParams nmParams;
        memset(&nmParams, 0, sizeof(nmParams));
        rcVcopy(nmParams.orig, bmin);
        nmParams.tileWidth = _tileSize * _cellSize.x;
        nmParams.tileHeight = _tileSize * _cellSize.x;
        nmParams.maxTiles = maxTiles;
        nmParams.maxPolys = maxPolysPerTile;

        status = _navMesh->init(&nmParams);
        if (dtStatusFailed(status))
        {
            ERR_PRINT("DTNavMeshLoad: Could not init Detour navmesh.");
            return false;
        }

        // Init NavQuery
        status = _navQuery->init(_navMesh, _navQueryMaxNodes);
        if (dtStatusFailed(status))
        {
            ERR_PRINT("DTNavMeshLoad: Could not init Detour navmesh query");
            return false;
        }

        // Tiles
        int tileCount = sourceFile->get_32();
        for (int i = 0; i < tileCount; ++i)
        {
            int dataSize = sourceFile->get_32();

            // Skip empty tiles
            if (dataSize == 0)
            {
                continue;
            }
            unsigned char* data = (unsigned char*)dtAlloc(dataSize, DT_ALLOC_PERM);
            memset(data, 0, dataSize);
            PoolByteArray array = sourceFile->get_buffer(dataSize);
            memcpy(data, array.read().ptr(), dataSize);

            // Add tile
            dtCompressedTileRef tile = 0;
            dtStatus addTileStatus = _tileCache->addTile(data, dataSize, DT_COMPRESSEDTILE_FREE_DATA, &tile);
            if (dtStatusFailed(addTileStatus))
            {
                ERR_PRINT(String("DTNavMeshLoad: Unable to add tile: {0}").format(Array::make(addTileStatus)));
                dtFree(data);
                data = 0;
                return false;
            }

            // Build navmesh tile
            if (tile)
            {
                status = _tileCache->buildNavMeshTile(tile, _navMesh);
                if (dtStatusFailed(status))
                {
                    ERR_PRINT(String("DTNavMeshLoad: Could not build nav mesh tile {0}").format(Array::make(status)));
                    return false;
                }
            }
        }

        // Initialize crowd
        if (!initializeCrowd())
        {
            ERR_PRINT("DTNavMeshLoad: Unable to initialize crowd.");
            return false;
        }
    }
    else {
        ERR_PRINT(String("DTNavMeshLoad: Unknown version: {0}").format(Array::make(version)));
        return false;
    }

    return true;
}

static int
pointInPoly(int nvert, const float* verts, const float* p)
{
    int i, j, c = 0;
    for (i = 0, j = nvert-1; i < nvert; j = i++)
    {
        const float* vi = &verts[i*3];
        const float* vj = &verts[j*3];
        if (((vi[2] > p[2]) != (vj[2] > p[2])) &&
            (p[0] < (vj[0]-vi[0]) * (p[2]-vi[2]) / (vj[2]-vi[2]) + vi[0]) )
            c = !c;
    }
    return c;
}

// Helper struct for changed tiles
struct ChangedTileData
{
    dtCompressedTileRef ref;
    int                 layer;
    float               bottomY;
    float               topY;
    bool                doAll;
};

struct ChangedPosData
{
    float   lowestY;
    float   highestY;
    int     layerOffset;
};

void
DetourNavigationMesh::rebuildChangedTiles()
{
    // Get tile width/height values
    const float* bmin = _inputGeom->getNavMeshBoundsMin();
    const float* bmax = _inputGeom->getNavMeshBoundsMax();
    int gw = 0, gh = 0;
    rcCalcGridSize(bmin, bmax, _cellSize.x, &gw, &gh);
    int ts = _tileSize;
    int numTilesX = (gw + ts-1) / ts;
    int numTilesZ = (gh + ts-1) / ts;
    float singleTileWidth = ts * _cellSize.x;
    float singleTileDepth = ts * _cellSize.x;

    // Iterate over all tiles to check if they touch a convex volume
    int volumeCount = _inputGeom->getConvexVolumeCount();
    dtCompressedTileRef tileRefs[128];  // TODO: const here?
    std::map<std::pair<int, int>, std::vector<ChangedTileData> > changedTiles;
    std::map<std::pair<int, int>, ChangedPosData> changedPosData;

    // Iterate over all volumes
    for (int i = 0; i < volumeCount; ++i)
    {
        // Get volume
        ConvexVolume volume = _inputGeom->getConvexVolumes()[i];

        // If this volume has been handled before, ignore it
        if (!volume.isNew)
        {
            continue;
        }

        // Get the left-/right-/top-/bottom-most tiles affected by this volume
        // Assuming front = -z, because -z is forward in Godot
        int leftMost = (volume.left - bmin[0]) / singleTileWidth;
        int rightMost = (volume.right - bmin[0]) / singleTileWidth;
        int frontMost = (volume.front - bmin[2]) / singleTileDepth;
        int backMost = (volume.back - bmin[2]) / singleTileDepth;

        if (leftMost < 0) leftMost = 0;
        if (leftMost > numTilesX-1) continue;
        if (rightMost < 0) continue;
        if (rightMost > numTilesX-1) rightMost = numTilesX-1;
        if (frontMost < 0) frontMost = 0;
        if (frontMost > numTilesZ-1) continue;
        if (backMost < 0) continue;
        if (backMost > numTilesZ-1) backMost = numTilesZ-1;

        for (int x = leftMost; x <= rightMost; ++x)
        {
            for (int z = frontMost; z <= backMost; ++z)
            {
                std::pair<int, int> tilePos = std::make_pair(x, z);
                // Initialize Y spans
                if (changedPosData.find(tilePos) == changedPosData.end())
                {
                    changedPosData[tilePos].lowestY = 1000000.0f;
                    changedPosData[tilePos].highestY = -1000000.0f;
                    changedPosData[tilePos].layerOffset = 100000;
                }

                // Get all vertical tiles to check which ones are affected by the volume
                int numVerticalTiles = _tileCache->getTilesAt(x, z, tileRefs, 128);

                // We want to get at least the main tile, the one below and the one above (otherwise, errors appear)
                // Therefore, just add all vertical tiles if there are only that many
                float lowestY = 1000000.0f;
                float highestY = -1000000.0f;
                float lowestMinY = 1000000.0f;
                float highestMinY = -1000000.0f;
                int lowestLayer = 100000;
                if (numVerticalTiles <= 3)
                {
                    lowestLayer = 0;
                    for (int j = 0; j < numVerticalTiles; ++j)
                    {
                        const dtCompressedTile* verticalTile = _tileCache->getTileByRef(tileRefs[j]);
                        float tileMinY = verticalTile->header->bmin[1];
                        float tileMaxY = tileMinY + verticalTile->header->hmin * _cellSize.y;

                        ChangedTileData data;
                        data.ref = tileRefs[j];
                        data.layer = verticalTile->header->tlayer;
                        data.bottomY = tileMinY;
                        data.topY = tileMaxY;
                        data.doAll = true;
                        changedTiles[tilePos].push_back( data );

                        // Remember lowest & highest Y positions
                        if (tileMinY < lowestY)
                        {
                            lowestY = tileMinY;
                        }
                        if (tileMaxY > highestY)
                        {
                            highestY = tileMaxY;
                        }
                    }
                }
                // Otherwise we need to detect the main tiles and its vertical neighbors
                // This is rather complex, as getTilesAt does NOT return vertical tiles in any specific order
                else {
                    // Iterate over all vertical tiles at this position to get those directly affected
                    for (int j = 0; j < numVerticalTiles; ++j)
                    {
                        const dtCompressedTile* verticalTile = _tileCache->getTileByRef(tileRefs[j]);

                        float tileMinY = verticalTile->header->bmin[1];
                        float tileMaxY = verticalTile->header->bmax[1];

                        if (!(volume.hmin > tileMaxY || volume.hmax < tileMinY))
                        {
                            ChangedTileData data;
                            data.ref = tileRefs[j];
                            data.layer = verticalTile->header->tlayer;
                            data.bottomY = tileMinY;
                            data.topY = tileMaxY;
                            data.doAll = false;
                            changedTiles[tilePos].push_back( data );

                            // Remember lowest & highest Y positions
                            if (tileMinY < lowestY)
                            {
                                lowestY = tileMinY;
                            }
                            if (tileMaxY > highestY)
                            {
                                highestY = tileMaxY;
                            }
                            if (tileMinY < lowestMinY)
                            {
                                lowestMinY = tileMinY;
                            }
                            if (tileMinY > highestMinY)
                            {
                                highestMinY = tileMinY;
                            }
                            if (data.layer < lowestLayer)
                            {
                                lowestLayer = data.layer;
                            }

                            if (data.layer == 0)
                            {
                                lowestY = _rcConfig->bmin[1];
                            }
                            if (data.layer == numVerticalTiles-1)
                            {
                                highestY = _rcConfig->bmax[1];
                            }
                        }
                    }

                    // Iterate again to find the tiles closest to the top and bottom
                    float closestToBottom = 1000000.0f;
                    dtCompressedTileRef bottomRef = 0;
                    float closestToTop = 1000000.0f;
                    dtCompressedTileRef topRef = 0;
                    for (int j = 0; j < numVerticalTiles; ++j)
                    {
                        const dtCompressedTile* verticalTile = _tileCache->getTileByRef(tileRefs[j]);
                        float tileMinY = verticalTile->header->bmin[1];

                        float distanceBottom = lowestMinY - tileMinY;
                        float distanceTop = tileMinY - highestMinY;

                        // Is this below the bottom and the closest so far?
                        if (distanceBottom > 0.001f && distanceBottom < closestToBottom)
                        {
                            closestToBottom = distanceBottom;
                            bottomRef = tileRefs[j];
                        }
                        // Is this over the top and the closest so far?
                        if (distanceTop > 0.001f && distanceTop < closestToTop)
                        {
                            closestToTop = distanceTop;
                            topRef = tileRefs[j];
                        }
                    }

                    // If we have any refs, add them
                    if (bottomRef != 0)
                    {
                        const dtCompressedTile* verticalTile = _tileCache->getTileByRef(bottomRef);
                        float tileMinY = verticalTile->header->bmin[1];
                        float tileMaxY = tileMinY + verticalTile->header->hmin * _cellSize.y;
                        ChangedTileData data;
                        data.ref = bottomRef;
                        data.layer = verticalTile->header->tlayer;
                        data.bottomY = tileMinY;
                        data.topY = tileMaxY;
                        data.doAll = false;
                        lowestY = tileMinY;
                        lowestY -= _cellSize.y;
                        if (data.layer == 0)
                        {
                            lowestY = _rcConfig->bmin[1];
                        }
                        lowestLayer = data.layer;
                        changedTiles[tilePos].push_back( data );
                    }
                    if (topRef != 0)
                    {
                        const dtCompressedTile* verticalTile = _tileCache->getTileByRef(topRef);
                        float tileMinY = verticalTile->header->bmin[1];
                        float tileMaxY = tileMinY + verticalTile->header->hmin * _cellSize.y;
                        ChangedTileData data;
                        data.ref = topRef;
                        data.layer = verticalTile->header->tlayer;
                        data.bottomY = tileMinY;
                        data.topY = tileMaxY;
                        data.doAll = false;
                        highestY = tileMaxY;
                        highestY += _cellSize.y;
                        if (data.layer == numVerticalTiles-1)
                        {
                            highestY = _rcConfig->bmax[1];
                        }
                        changedTiles[tilePos].push_back( data );
                    }
                }

                // Remember the Y spans
                ChangedPosData posData = changedPosData[tilePos];
                if (lowestY < posData.lowestY)
                {
                    posData.lowestY = lowestY;
                }
                if (highestY > posData.highestY)
                {
                    posData.highestY = highestY;
                }
                if (lowestLayer < posData.layerOffset)
                {
                    posData.layerOffset = lowestLayer;
                }
                changedPosData[tilePos] = posData;
            }
        }
    } // Iterate over volumes

    // Iterate over all changed tiles
    for (auto const& entry : changedTiles)
    {
        std::pair<int, int> tilePos = entry.first;

        // Remove all affected layers
        int maxLayersToAdd = entry.second.size();
        for (int i = 0; i < maxLayersToAdd; ++i)
        {
            ChangedTileData data = entry.second[i];
            _tileCache->removeTile(data.ref, 0, 0);
            dtTileRef ref = _navMesh->getTileRefAt(tilePos.first, tilePos.second, data.layer);
            _navMesh->removeTile(ref, 0, 0);
        }

        ChangedTileData data = entry.second[0];
        bool doAllLayers = data.doAll;

        // Rasterize and add the affected layers
        TileCacheData tiles[maxLayersToAdd];
        memset(tiles, 0, sizeof(tiles));
        rcConfig adjustedCfg;
        memcpy(&adjustedCfg, _rcConfig, sizeof(rcConfig));
        if (!doAllLayers)
        {
            adjustedCfg.bmin[1] = changedPosData[tilePos].lowestY;
            adjustedCfg.bmax[1] = changedPosData[tilePos].highestY;
        }
        int ntiles = rasterizeTileLayers(tilePos.first, tilePos.second, adjustedCfg, tiles, maxLayersToAdd);
        if (ntiles == 0)
        {
            WARN_PRINT("DTNavMesh: rebuildChangedTiles: rasterize yielded 0 tiles.");
            return;
        }
        for (int i = 0; i < ntiles; ++i)
        {
            TileCacheData* tile = &tiles[i];
            dtTileCacheLayerHeader* header = (dtTileCacheLayerHeader*)tile->data;

            // Apply layer offset
            header->tlayer += changedPosData[tilePos].layerOffset;
            dtStatus status = _tileCache->addTile(tile->data, tile->dataSize, DT_COMPRESSEDTILE_FREE_DATA, 0);
            if (dtStatusFailed(status))
            {
                ERR_PRINT(String("DTNavMesh: rebuildChangedTiles: Could not add nav mesh tiles at {0} {1}: {2}").format(Array::make(tilePos.first, tilePos.second, status)));
                dtFree(tile->data);
                tile->data = 0;
                continue;
            }
        }

        // Build navmesh tiles
        dtStatus status = _tileCache->buildNavMeshTilesAt(tilePos.first, tilePos.second, _navMesh);
        if (dtStatusFailed(status))
        {
            ERR_PRINT(String("DTNavMesh: rebuildChangedTiles: Could not build nav mesh tiles at {0} {1}: {2}").format(Array::make(tilePos.first, tilePos.second, status)));
            return;
        }
    }
}

bool
DetourNavigationMesh::addAgent(Ref<DetourCrowdAgent> agent, Ref<DetourCrowdAgentParameters> parameters, bool main)
{
    float pos[3];
    pos[0] = parameters->position.x;
    pos[1] = parameters->position.y;
    pos[2] = parameters->position.z;

    // Set parameter values
    dtCrowdAgentParams params;
    memset(&params, 0, sizeof(params));
    params.radius = parameters->radius;
    params.height = parameters->height;
    params.maxAcceleration = parameters->maxAcceleration;
    params.maxSpeed = parameters->maxSpeed;
    // TODO: Unsure what a good range would be here, using detour demo values
    params.collisionQueryRange = params.radius * 12.0f;
    params.pathOptimizationRange = params.radius * 30.0f;
    params.updateFlags = 0;
    if (parameters->anticipateTurns)
        params.updateFlags |= DT_CROWD_ANTICIPATE_TURNS;
    if (parameters->optimizeVisibility)
        params.updateFlags |= DT_CROWD_OPTIMIZE_VIS;
    if (parameters->optimizeTopology)
        params.updateFlags |= DT_CROWD_OPTIMIZE_TOPO;
    if (parameters->avoidObstacles)
        params.updateFlags |= DT_CROWD_OBSTACLE_AVOIDANCE;
    if (parameters->avoidOtherAgents)
        params.updateFlags |= DT_CROWD_SEPARATION;
    params.obstacleAvoidanceType = (unsigned char)parameters->obstacleAvoidance;
    params.separationWeight = parameters->separationWeight;
    params.queryFilterType = agent->getFilterIndex();
    const dtQueryFilter* filter = _crowd->getFilter(params.queryFilterType);

    // Some parameters change for "shadows"
    if (!main)
    {
        params.separationWeight = 0.0f;
        params.updateFlags &= ~DT_CROWD_SEPARATION;
    }

    // Create agent in detour
    int agentIndex = _crowd->addAgent(pos, &params);
    if (agentIndex == -1)
    {
        ERR_PRINT("DTNavMesh: Unable to add agent to crowd!");
        return false;
    }

    // Add the pointer to the agent either as main or shadow
    dtCrowdAgent* crowdAgent = _crowd->getEditableAgent(agentIndex);
    if (crowdAgent->state == DT_CROWDAGENT_STATE_INVALID)
    {
        _crowd->removeAgent(agentIndex);
        ERR_PRINT("DTNavMesh: Invalid state");
        return false;
    }
    if (main)
    {
        agent->setMainAgent(crowdAgent, _crowd, agentIndex, _navQuery, _inputGeom, _navMeshIndex);
    }
    else
    {
        agent->addShadowAgent(crowdAgent);
    }

    return true;
}

void
DetourNavigationMesh::removeAgent(dtCrowdAgent* agent)
{

}

void
DetourNavigationMesh::addObstacle(Ref<DetourObstacle> obstacle)
{
    // Add the obstacle using the tile cache
    obstacle->createDetourObstacle(_tileCache);
}

void
DetourNavigationMesh::update(float timeDeltaSeconds)
{
    // Call update until everything is done
    bool upToDate = false;
    while (!upToDate)
    {
        dtStatus status = _tileCache->update(timeDeltaSeconds, _navMesh, &upToDate);
        if (dtStatusFailed(status))
        {
            ERR_PRINT("DetourNavigationmesh::update failed.");
            return;
        }
    }

    // Update the crowd
    _crowd->update(timeDeltaSeconds, 0);
}

void
DetourNavigationMesh::createDebugMesh(GodotDetourDebugDraw* debugDrawer, bool drawCacheBounds)
{
    if (!_inputGeom || !_inputGeom->getMesh())
        return;

    // TODO: Draw off-mesh connections

    // Draw tiles
    if (_tileCache && drawCacheBounds)
        debugDrawTiles(debugDrawer);

    // Draw obstacles
    if (_tileCache)
        debugDrawObstacles(debugDrawer);

    // Draw bounds
    const float* bmin = _inputGeom->getNavMeshBoundsMin();
    const float* bmax = _inputGeom->getNavMeshBoundsMax();
    duDebugDrawBoxWire(debugDrawer, bmin[0],bmin[1],bmin[2], bmax[0],bmax[1],bmax[2], duRGBA(255,255,255,128), 1.0f);

    // Tiling grid
    int gw = 0, gh = 0;
    rcCalcGridSize(bmin, bmax, _cellSize.x, &gw, &gh);
    const int tw = (gw + (int)_tileSize-1) / (int)_tileSize;
    const int th = (gh + (int)_tileSize-1) / (int)_tileSize;
    const float s = _tileSize * _cellSize.x;
    duDebugDrawGridXZ(debugDrawer, bmin[0],bmin[1],bmin[2], tw,th, s, duRGBA(0,0,0,64), 1.0f);

    // Navmesh itself
    if (_navMesh && _navQuery)
    {
        unsigned char drawFlags = DU_DRAWNAVMESH_OFFMESHCONS|DU_DRAWNAVMESH_CLOSEDLIST;
        if (!drawCacheBounds)
        {
            drawFlags |= DU_DRAWNAVMESH_COLOR_TILES;
        }
        duDebugDrawNavMeshWithClosedList(debugDrawer, *_navMesh, *_navQuery, drawFlags);

        // Render disabled polygons in black
        duDebugDrawNavMeshPolysWithFlags(debugDrawer, *_navMesh, POLY_FLAGS_DISABLED, duRGBA(0, 0, 0, 128));
    }

    // Draw convex volumes (marked areas)
    _inputGeom->drawConvexVolumes(debugDrawer);
}

float
DetourNavigationMesh::getActorFitFactor(float agentRadius, float agentHeight)
{
    // Exclude agents that are too big
    if (agentRadius > _maxAgentRadius || agentHeight > _maxAgentHeight)
    {
        return -1.0f;
    }

    // Calculate and return the factor (with more importance of radius)
    float radiusFactor = 1.0f - (agentRadius / _maxAgentRadius);
    float heightFactor = 1.0f - (agentHeight / _maxAgentHeight);
    return radiusFactor * 2.0f + heightFactor + 0.01f;
}

bool
DetourNavigationMesh::initializeCrowd()
{
    if (!_crowd->init(_maxAgents, _maxAgentRadius, _navMesh))
    {
        return false;
    }

    // Make polygons with 'disabled' flag invalid.
    _crowd->getEditableFilter(0)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(1)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(2)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(3)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(4)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(5)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(6)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(7)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(8)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(9)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(10)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(11)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(12)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(13)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(14)->setExcludeFlags(POLY_FLAGS_DISABLED);
    _crowd->getEditableFilter(15)->setExcludeFlags(POLY_FLAGS_DISABLED);

    // Setup local avoidance params to different qualities.
    dtObstacleAvoidanceParams params;
    // Use mostly default settings, copy from dtCrowd.
    memcpy(&params, _crowd->getObstacleAvoidanceParams(0), sizeof(dtObstacleAvoidanceParams));

    // Low (11)
    params.velBias = 0.5f;
    params.adaptiveDivs = 5;
    params.adaptiveRings = 2;
    params.adaptiveDepth = 1;
    _crowd->setObstacleAvoidanceParams(0, &params);

    // Medium (22)
    params.velBias = 0.5f;
    params.adaptiveDivs = 5;
    params.adaptiveRings = 2;
    params.adaptiveDepth = 2;
    _crowd->setObstacleAvoidanceParams(1, &params);

    // Good (45)
    params.velBias = 0.5f;
    params.adaptiveDivs = 7;
    params.adaptiveRings = 2;
    params.adaptiveDepth = 3;
    _crowd->setObstacleAvoidanceParams(2, &params);

    // High (66)
    params.velBias = 0.5f;
    params.adaptiveDivs = 7;
    params.adaptiveRings = 3;
    params.adaptiveDepth = 3;
    _crowd->setObstacleAvoidanceParams(3, &params);

    return true;
}

int
DetourNavigationMesh::rasterizeTileLayers(const int tileX, const int tileZ, const rcConfig& cfg, TileCacheData* tiles, const int maxTiles)
{
    if (!_inputGeom || !_inputGeom->getMesh() || !_inputGeom->getChunkyMesh())
    {
        ERR_PRINT("DTNavMesh::rasterizeTileLayers: Input mesh is not specified");
        return 0;
    }

    FastLZCompressor comp;
    RasterizationContext rc(_maxLayers);

    const float* verts = _inputGeom->getMesh()->getVerts();
    const int nverts = _inputGeom->getMesh()->getVertCount();
    const rcChunkyTriMesh* chunkyMesh = _inputGeom->getChunkyMesh();

    // Tile bounds.
    const float tcs = cfg.tileSize * cfg.cs;

    rcConfig tcfg;
    memcpy(&tcfg, &cfg, sizeof(tcfg));

    tcfg.bmin[0] = cfg.bmin[0] + tileX*tcs;
    tcfg.bmin[1] = cfg.bmin[1];
    tcfg.bmin[2] = cfg.bmin[2] + tileZ*tcs;
    tcfg.bmax[0] = cfg.bmin[0] + (tileX+1)*tcs;
    tcfg.bmax[1] = cfg.bmax[1];
    tcfg.bmax[2] = cfg.bmin[2] + (tileZ+1)*tcs;
    tcfg.bmin[0] -= tcfg.borderSize*tcfg.cs;
    tcfg.bmin[2] -= tcfg.borderSize*tcfg.cs;
    tcfg.bmax[0] += tcfg.borderSize*tcfg.cs;
    tcfg.bmax[2] += tcfg.borderSize*tcfg.cs;

    // Allocate voxel heightfield where we rasterize our input data to.
    rc.solid = rcAllocHeightfield();
    if (!rc.solid)
    {
        ERR_PRINT("DTNavMesh::rasterizeTileLayers:  Out of memory 'solid'");
        return 0;
    }
    if (!rcCreateHeightfield(_recastContext, *rc.solid, tcfg.width, tcfg.height, tcfg.bmin, tcfg.bmax, tcfg.cs, tcfg.ch))
    {
        ERR_PRINT("DTNavMesh::rasterizeTileLayers: Could not create solid heightfield");
        return 0;
    }

    // Allocate array that can hold triangle flags.
    // If you have multiple meshes you need to process, allocate
    // and array which can hold the max number of triangles you need to process.
    rc.triareas = new unsigned char[chunkyMesh->maxTrisPerChunk];
    if (!rc.triareas)
    {
        ERR_PRINT(String("DTNavMesh::rasterizeTileLayers: Out of memory 'm_triareas' {0}").format(Array::make(chunkyMesh->maxTrisPerChunk)));
        return 0;
    }

    float tbmin[2], tbmax[2];
    tbmin[0] = tcfg.bmin[0];
    tbmin[1] = tcfg.bmin[2];
    tbmax[0] = tcfg.bmax[0];
    tbmax[1] = tcfg.bmax[2];
    int cid[512];
    const int ncid = rcGetChunksOverlappingRect(chunkyMesh, tbmin, tbmax, cid, 512);
    if (!ncid)
    {
        return 0;
    }

    if (ncid > 512)
    {
        WARN_PRINT(String("DTNavMesh::rasterizeTileLayers: Got more than 512 chunks. FIXME").format(Array::make(ncid)));
    }

    for (int i = 0; i < ncid; ++i)
    {
        const rcChunkyTriMeshNode& node = chunkyMesh->nodes[cid[i]];
        const int* tris = &chunkyMesh->tris[node.i*3];
        const int ntris = node.n;

        memset(rc.triareas, 0, ntris*sizeof(unsigned char));
        rcMarkWalkableTriangles(_recastContext, tcfg.walkableSlopeAngle, verts, nverts, tris, ntris, rc.triareas);

        if (!rcRasterizeTriangles(_recastContext, verts, nverts, tris, rc.triareas, ntris, *rc.solid, tcfg.walkableClimb))
        {
            Godot::print("DTNavMesh::rasterizeTileLayers: RasterizeTriangles returned false");
            return 0;
        }
    }

    // Once all geometry is rasterized, we do initial pass of filtering to
    // remove unwanted overhangs caused by the conservative rasterization
    // as well as filter spans where the character cannot possibly stand.
    rcFilterLowHangingWalkableObstacles(_recastContext, tcfg.walkableClimb, *rc.solid);
    rcFilterLedgeSpans(_recastContext, tcfg.walkableHeight, tcfg.walkableClimb, *rc.solid);
    rcFilterWalkableLowHeightSpans(_recastContext, tcfg.walkableHeight, *rc.solid);

    rc.chf = rcAllocCompactHeightfield();
    if (!rc.chf)
    {
        ERR_PRINT("DTNavMesh::rasterizeTileLayers: Out of memory 'chf'");
        return 0;
    }
    if (!rcBuildCompactHeightfield(_recastContext, tcfg.walkableHeight, tcfg.walkableClimb, *rc.solid, *rc.chf))
    {
        ERR_PRINT("DTNavMesh::rasterizeTileLayers: Could not build compact data");
        return 0;
    }

    // Erode the walkable area by agent radius.
    if (!rcErodeWalkableArea(_recastContext, tcfg.walkableRadius, *rc.chf))
    {
        ERR_PRINT("DTNavMesh::rasterizeTileLayers: Could not erode");
        return 0;
    }

    // Mark areas (as water, grass, road, etc.), by default, everything is ground
    ConvexVolume* vols = _inputGeom->getConvexVolumes();
    for (int i  = 0; i < _inputGeom->getConvexVolumeCount(); ++i)
    {
        rcMarkConvexPolyArea(_recastContext, vols[i].verts, vols[i].nverts, vols[i].hmin, vols[i].hmax, (unsigned char)vols[i].area, *rc.chf);
        vols[i].isNew = false;
    }

    rc.lset = rcAllocHeightfieldLayerSet();
    if (!rc.lset)
    {
        ERR_PRINT("DTNavMesh::rasterizeTileLayers: Out of memory 'lset'");
        return 0;
    }
    if (!rcBuildHeightfieldLayers(_recastContext, *rc.chf, tcfg.borderSize, tcfg.walkableHeight, *rc.lset))
    {
        ERR_PRINT("DTNavMesh::rasterizeTileLayers: Could not build heighfield layers");
        return 0;
    }

    rc.ntiles = 0;
    for (int i = 0; i < rcMin(rc.lset->nlayers, _maxLayers); ++i)
    {
        TileCacheData* tile = &rc.tiles[rc.ntiles++];
        const rcHeightfieldLayer* layer = &rc.lset->layers[i];

        // Store header
        dtTileCacheLayerHeader header;
        header.magic = DT_TILECACHE_MAGIC;
        header.version = DT_TILECACHE_VERSION;

        // Tile layer location in the navmesh.
        header.tx = tileX;
        header.ty = tileZ;
        header.tlayer = i;
        dtVcopy(header.bmin, layer->bmin);
        dtVcopy(header.bmax, layer->bmax);

        // Tile info.
        header.width = (unsigned char)layer->width;
        header.height = (unsigned char)layer->height;
        header.minx = (unsigned char)layer->minx;
        header.maxx = (unsigned char)layer->maxx;
        header.miny = (unsigned char)layer->miny;
        header.maxy = (unsigned char)layer->maxy;
        header.hmin = (unsigned short)layer->hmin;
        header.hmax = (unsigned short)layer->hmax;

        dtStatus status = dtBuildTileCacheLayer(&comp, &header, layer->heights, layer->areas, layer->cons,
                                                &tile->data, &tile->dataSize);
        if (dtStatusFailed(status))
        {
            ERR_PRINT("DTNavMesh::rasterizeTileLayers: Unable to build tile cache layer");
            return 0;
        }
    }

    // Transfer ownsership of tile data from build context to the caller.
    int n = 0;
    for (int i = 0; i < rcMin(rc.ntiles, maxTiles); ++i)
    {
        tiles[n++] = rc.tiles[i];
        rc.tiles[i].data = 0;
        rc.tiles[i].dataSize = 0;
    }

    return n;
}

void
DetourNavigationMesh::debugDrawTiles(GodotDetourDebugDraw* debugDrawer)
{
    unsigned int fcol[6];
    float bmin[3], bmax[3];

    for (int i = 0; i < _tileCache->getTileCount(); ++i)
    {
        const dtCompressedTile* tile = _tileCache->getTile(i);
        if (!tile->header)
        {
            continue;
        }

        _tileCache->calcTightTileBounds(tile->header, bmin, bmax);

        const unsigned int col = duIntToCol(i, 64);
        duCalcBoxColors(fcol, col, col);
        debugDrawer->debugDrawBox(bmin[0],bmin[1],bmin[2], bmax[0],bmax[1],bmax[2], fcol);
    }

    for (int i = 0; i < _tileCache->getTileCount(); ++i)
    {
        const dtCompressedTile* tile = _tileCache->getTile(i);
        if (!tile->header)
        {
            continue;
        }

        _tileCache->calcTightTileBounds(tile->header, bmin, bmax);

        const unsigned int col = duIntToCol(i ,255);
        const float pad = 0.01f;
        duDebugDrawBoxWire(debugDrawer, bmin[0]-pad,bmin[1]-pad,bmin[2]-pad,
                           bmax[0]+pad,bmax[1]+pad,bmax[2]+pad, col, 2.0f);
    }
}

void
DetourNavigationMesh::debugDrawObstacles(GodotDetourDebugDraw* debugDrawer)
{
    // Draw obstacles
    for (int i = 0; i < _tileCache->getObstacleCount(); ++i)
    {
        const dtTileCacheObstacle* ob = _tileCache->getObstacle(i);
        if (ob->state == DT_OBSTACLE_EMPTY) continue;
        float bmin[3], bmax[3];
        _tileCache->getObstacleBounds(ob, bmin,bmax);

        unsigned int col = 0;
        if (ob->state == DT_OBSTACLE_PROCESSING)
            col = duRGBA(255,255,0,128);
        else if (ob->state == DT_OBSTACLE_PROCESSED)
            col = duRGBA(255,192,0,192);
        else if (ob->state == DT_OBSTACLE_REMOVING)
            col = duRGBA(220,0,0,128);

        duDebugDrawCylinder(debugDrawer, bmin[0],bmin[1],bmin[2], bmax[0],bmax[1],bmax[2], col);
        duDebugDrawCylinderWire(debugDrawer, bmin[0],bmin[1],bmin[2], bmax[0],bmax[1],bmax[2], duDarkenCol(col), 2);
    }
}
