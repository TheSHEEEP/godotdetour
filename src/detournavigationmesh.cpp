#include "detournavigationmesh.h"
#include <DetourTileCache.h>
#include <SurfaceTool.hpp>
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
    // TODO: is this enough? Demo has only 32, seems very little for tall vs flat levels?
    , _maxLayers(32)
    , _tileSize(0)
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
DetourNavigationMesh::initialize(DetourInputGeometry* inputGeom, Ref<DetourNavigationMeshParameters> params, int maxObstacles, RecastContext* recastContext)
{
    Godot::print("DTNavMeshInitialize: Initializing navigation mesh");

    _recastContext = recastContext;

    DetourNavigationMeshParameters* para = *params;
    _inputGeom = inputGeom;
    _meshProcess->init(_inputGeom);
    _maxAgentSlope = para->maxAgentSlope;
    _maxAgentClimb = para->maxAgentClimb;
    _maxAgentHeight = para->maxAgentHeight;
    _maxAgentRadius = para->maxAgentRadius;
    _maxAgents = para->maxNumAgents;
    _cellSize = para->cellSize;
    _tileSize = para->tileSize;

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
    tcparams.maxTiles = tw * th * para->layersPerTile;
    tcparams.maxObstacles = maxObstacles;
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
    int tileBits = rcMin((int)dtIlog2(dtNextPow2(tw*th*para->layersPerTile)), 14);
    if (tileBits > 14) tileBits = 14;
    int polyBits = 22 - tileBits;
    int maxTiles = 1 << tileBits;
    int maxPolysPerTile = 1 << polyBits;

    dtNavMeshParams nmParams;
    memset(&nmParams, 0, sizeof(nmParams));
    rcVcopy(nmParams.orig, bmin);
    nmParams.tileWidth = _tileSize * para->cellSize.x;
    nmParams.tileHeight = _tileSize * para->cellSize.y;
    nmParams.maxTiles = maxTiles;
    nmParams.maxPolys = maxPolysPerTile;

    status = _navMesh->init(&nmParams);
    if (dtStatusFailed(status))
    {
        ERR_PRINT("DTNavMeshInitialize: Could not init Detour navmesh.");
        return false;
    }
    Godot::print("DTNavMeshInitialize: Initialized Detour navmesh...");

    status = _navQuery->init(_navMesh, 2048);
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
        volume.isNew = false;

        // Get the left-/right-/top-/bottom-most tiles affected by this volume
        // Assuming top = -z, because -z is forward in Godot
        int leftMost = (volume.left - bmin[0]) / singleTileWidth;
        int rightMost = (volume.right - bmin[0]) / singleTileWidth;
        int topMost = (volume.top - bmin[2]) / singleTileDepth;
        int bottomMost = (volume.bottom - bmin[2]) / singleTileDepth;

        if (leftMost < 0) leftMost = 0;
        if (leftMost > numTilesX-1) continue;
        if (rightMost < 0) continue;
        if (rightMost > numTilesX-1) rightMost = numTilesX-1;
        if (topMost < 0) topMost = 0;
        if (topMost > numTilesZ-1) continue;
        if (bottomMost < 0) continue;
        if (bottomMost > numTilesZ-1) bottomMost = numTilesZ-1;

        for (int x = leftMost; x <= rightMost; ++x)
        {
            for (int z = topMost; z <= bottomMost; ++z)
            {
                // Get all vertical tiles to check which ones are affected by the volume
                int numVerticalTiles = _tileCache->getTilesAt(x, z, tileRefs, 128);

                // Iterate over all vertical tiles at this position to get those affected
                for (int j = 0; j < numVerticalTiles; ++j)
                {
                    const dtCompressedTile* verticalTile = _tileCache->getTileByRef(tileRefs[j]);

                    float tileMinY = verticalTile->header->bmin[1];
                    float tileMaxY = tileMinY + verticalTile->header->hmin * _cellSize.y;

                    if (!(volume.hmin > tileMaxY || volume.hmax < tileMinY))
                    {
                        ChangedTileData data;
                        data.ref = tileRefs[j];
                        data.layer = verticalTile->header->tlayer;
                        data.bottomY = tileMinY;
                        data.topY = tileMaxY;
                        changedTiles[std::make_pair(x, z)].push_back( data );
                    }
                }
            }
        }
    }

    // Iterate over all changed tiles
    for (auto const& entry : changedTiles)
    {
        std::pair<int, int> tilePos = entry.first;

        // Iterate over the tile layers
        TileCacheData tile;
        for (int i = 0; i < entry.second.size(); ++i)
        {
            // Remove this tile layer
            ChangedTileData data = entry.second[i];
            _tileCache->removeTile(data.ref, 0, 0);
            dtTileRef ref = _navMesh->getTileRefAt(tilePos.first, tilePos.second, data.layer);
            _navMesh->removeTile(ref, 0, 0);

            // Rasterize this layer
            memset(&tile, 0, sizeof(tile));
            float ySpan[2];
            ySpan[0] = data.bottomY;
            ySpan[1] = data.topY;
            int nTiles = rasterizeSingleTileLayer(tilePos.first, tilePos.second, data.layer, ySpan, *_rcConfig, tile);
            if (nTiles != 1)
            {
                ERR_PRINT("rebuildChangedTiles: Rasterizing single tile layer failed.");
                return;
            }

            // Add this tile layer
            dtStatus status = _tileCache->addTile(tile.data, tile.dataSize, DT_COMPRESSEDTILE_FREE_DATA, 0);
            if (dtStatusFailed(status))
            {
                ERR_PRINT(String("rebuildChangedTiles: Failed to add tile: {0}").format(Array::make(status)));
                dtFree(tile.data);
                tile.data = 0;
                continue;
            }

        }

        // Build navmesh tiles
        dtStatus status = _tileCache->buildNavMeshTilesAt(tilePos.first, tilePos.second, _navMesh);
        if (dtStatusFailed(status))
        {
            ERR_PRINT(String("DTNavMeshInitialize: Could not build nav mesh tiles at {0} {1}: {2}").format(Array::make(tilePos.first, tilePos.second, status)));
            return;
        }
    }
}

bool
DetourNavigationMesh::addAgent(Ref<DetourCrowdAgent> agent, Ref<DetourCrowdAgentParameters> parameters, bool main)
{
    // Calculate optimal placement position to avoid errors later
    float rayStart[3];
    float rayEnd[3];
    float hitTime = 0.0f;
    rayStart[0] = rayEnd[0] = parameters->position.x;
    rayStart[1] = rayEnd[1] = parameters->position.y;
    rayStart[2] = rayEnd[2] = parameters->position.z;
    rayStart[1] -= 0.4f;
    rayEnd[1] += 0.4f;
    bool hit = _inputGeom->raycastMesh(rayStart, rayEnd, hitTime);
    if (!hit)
    {
        // Try again
        for (int i = 0; i < 10; ++i)
        {
            rayStart[1] -= 0.4f;
            rayEnd[1] += 0.4f;
            hit = _inputGeom->raycastMesh(rayStart, rayEnd, hitTime);
            if (hit)
            {
                break;
            }
        }
        if (!hit)
        {
            ERR_PRINT(String("addAgent: Input geometry couldn't do raycast: {0} {1} {2} {3} {4} {5}").format(Array::make(rayStart[0], rayStart[1], rayStart[2], rayEnd[0], rayEnd[1], rayEnd[2])));
            return false;
        }
    }
    float pos[3];
    pos[0] = rayStart[0] + (rayEnd[0] - rayStart[0]) * hitTime;
    pos[1] = rayStart[1] + (rayEnd[1] - rayStart[1]) * hitTime;
    pos[2] = rayStart[2] + (rayEnd[2] - rayStart[2]) * hitTime;

    // Create agent in detour
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
        agent->setMainAgent(crowdAgent, _crowd, agentIndex, _navQuery, _inputGeom);
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

int
DetourNavigationMesh::rasterizeSingleTileLayer(const int tileX, const int tileZ, const int tileLayer, const float* tileYSpan, const rcConfig& cfg, TileCacheData& outTile)
{
    if (!_inputGeom || !_inputGeom->getMesh() || !_inputGeom->getChunkyMesh())
    {
        ERR_PRINT("DTNavMesh::rasterizeSingleTileLayer: Input mesh is not specified");
        return 0;
    }

    FastLZCompressor comp;
    RasterizationContext rc(1);

    const float* verts = _inputGeom->getMesh()->getVerts();
    const int nverts = _inputGeom->getMesh()->getVertCount();
    const rcChunkyTriMesh* chunkyMesh = _inputGeom->getChunkyMesh();

    // Tile bounds.
    const float tcs = cfg.tileSize * cfg.cs;

    rcConfig tcfg;
    memcpy(&tcfg, &cfg, sizeof(tcfg));

    // Adjust the boundaries so that only the picked layer is contained in it
    tcfg.bmin[0] = cfg.bmin[0] + tileX*tcs;
    tcfg.bmin[1] = tileYSpan[0] - _cellSize.y;  // To account for inaccuracy in "touching" the lower bounds
    tcfg.bmin[2] = cfg.bmin[2] + tileZ*tcs;
    tcfg.bmax[0] = cfg.bmin[0] + (tileX+1)*tcs;
    tcfg.bmax[1] = tileYSpan[1];
    tcfg.bmax[2] = cfg.bmin[2] + (tileZ+1)*tcs;
    tcfg.bmin[0] -= tcfg.borderSize*tcfg.cs;
    tcfg.bmin[2] -= tcfg.borderSize*tcfg.cs;
    tcfg.bmax[0] += tcfg.borderSize*tcfg.cs;
    tcfg.bmax[2] += tcfg.borderSize*tcfg.cs;

    // Allocate voxel heightfield where we rasterize our input data to.
    rc.solid = rcAllocHeightfield();
    if (!rc.solid)
    {
        ERR_PRINT("DTNavMesh::rasterizeSingleTileLayer:  Out of memory 'solid'");
        return 0;
    }
    if (!rcCreateHeightfield(_recastContext, *rc.solid, tcfg.width, tcfg.height, tcfg.bmin, tcfg.bmax, tcfg.cs, tcfg.ch))
    {
        ERR_PRINT("DTNavMesh::rasterizeSingleTileLayer: Could not create solid heightfield");
        return 0;
    }

    // Allocate array that can hold triangle flags.
    // If you have multiple meshes you need to process, allocate
    // and array which can hold the max number of triangles you need to process.
    rc.triareas = new unsigned char[chunkyMesh->maxTrisPerChunk];
    if (!rc.triareas)
    {
        ERR_PRINT(String("DTNavMesh::rasterizeSingleTileLayer: Out of memory 'm_triareas' {0}").format(Array::make(chunkyMesh->maxTrisPerChunk)));
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
        WARN_PRINT(String("DTNavMesh::rasterizeSingleTileLayer: Got more than 512 chunks. FIXME").format(Array::make(ncid)));
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
            Godot::print("DTNavMesh::rasterizeSingleTileLayer: RasterizeTriangles returned false");
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
        ERR_PRINT("DTNavMesh::rasterizeSingleTileLayer: Out of memory 'chf'");
        return 0;
    }
    if (!rcBuildCompactHeightfield(_recastContext, tcfg.walkableHeight, tcfg.walkableClimb, *rc.solid, *rc.chf))
    {
        ERR_PRINT("DTNavMesh::rasterizeSingleTileLayer: Could not build compact data");
        return 0;
    }

    // Erode the walkable area by agent radius.
    if (!rcErodeWalkableArea(_recastContext, tcfg.walkableRadius, *rc.chf))
    {
        ERR_PRINT("DTNavMesh::rasterizeSingleTileLayer: Could not erode");
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
        ERR_PRINT("DTNavMesh::rasterizeSingleTileLayer: Out of memory 'lset'");
        return 0;
    }
    if (!rcBuildHeightfieldLayers(_recastContext, *rc.chf, tcfg.borderSize, tcfg.walkableHeight, *rc.lset))
    {
        ERR_PRINT("DTNavMesh::rasterizeSingleTileLayer: Could not build heighfield layers");
        return 0;
    }

    // Create the tile cache layer
    TileCacheData* tile = &rc.tiles[rc.ntiles];

    const rcHeightfieldLayer* layer = &rc.lset->layers[0];

    // Store header
    dtTileCacheLayerHeader header;
    header.magic = DT_TILECACHE_MAGIC;
    header.version = DT_TILECACHE_VERSION;

    // Tile layer location in the navmesh.
    header.tx = tileX;
    header.ty = tileZ;
    header.tlayer = tileLayer;
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

    // Build tile cache layer
    dtStatus status = dtBuildTileCacheLayer(&comp, &header, layer->heights, layer->areas, layer->cons,
                                            &tile->data, &tile->dataSize);
    if (dtStatusFailed(status))
    {
        ERR_PRINT("DTNavMesh::rasterizeSingleTileLayer: Unable to build tile cache layer");
        return 0;
    }

    // Transfer ownsership of tile data from build context to the caller.
    outTile.data = rc.tiles[0].data;
    outTile.dataSize = rc.tiles[0].dataSize;
    rc.tiles[0].data = 0;
    rc.tiles[0].dataSize = 0;

    return 1;
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
