#include "detournavigationmesh.h"
#include <DetourTileCache.h>
#include <SurfaceTool.hpp>
#include <Recast.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshQuery.h>
#include <DetourNavMeshBuilder.h>
#include <DetourDebugDraw.h>
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
    , _tileCache(nullptr)
    , _navMesh(nullptr)
    , _navQuery(nullptr)
    , _allocator(nullptr)
    , _compressor(nullptr)
    , _meshProcess(nullptr)
    , _inputGeom(nullptr)
    , _maxAgentSlope(0.0f)
    , _maxAgentClimb(0.0f)
    , _maxAgentHeight(0.0f)
    , _maxAgentRadius(0.0f)
    // TODO: is this enough? Demo has only 32, seems very little for high levels?
    , _maxLayers(32)
    , _tileSize(0)
{
    _navQuery = dtAllocNavMeshQuery();
    _allocator = new LinearAllocator(_maxLayers * 1000);
    _compressor = new FastLZCompressor();
    _meshProcess = new MeshProcess();
}

DetourNavigationMesh::~DetourNavigationMesh()
{
    dtFreeNavMeshQuery(_navQuery);
    dtFreeNavMesh(_navMesh);
    dtFreeTileCache(_tileCache);
    delete _allocator;
    delete _compressor;
    delete _meshProcess;
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
    rcConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
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

    return true;
}

void
DetourNavigationMesh::rebuildChangedTiles()
{
    // Get tile width/height values
    const float* bmin = _inputGeom->getNavMeshBoundsMin();
    const float* bmax = _inputGeom->getNavMeshBoundsMax();
    int gw = 0, gh = 0;
    rcCalcGridSize(bmin, bmax, _cellSize.x, &gw, &gh);
    const int ts = _tileSize;
    const int numTilesX = (gw + ts-1) / ts;
    const int numTilesZ = (gh + ts-1) / ts;
    const float singleTileWidth = ts * _cellSize.x;
    const float singleTileDepth = ts * _cellSize.x;
    const float singleTileHeight = ts * _cellSize.y;

    // Iterate over all marked cylinder/boxes/convex-polygons to find every tileX/tileZ position + layers that was changed
    // use calcTightTileBounds to check if certain layer is touched

    // Iterate over all changed tile positions
    {
        // Get all tiles at x/y

        // Remove all tiles at x/y

        // Rasterize tiles

        // Add new tiles

        // Build navmesh tiles
    }

    // TODO: Navigation, make use of area info via
    // crowd->getEditableFilter(0)->setAreaCost(SAMPLE_POLYAREA_WATER, 1000.0);
}

DetourCrowdAgent*
DetourNavigationMesh::addAgent(Ref<DetourCrowdAgentParameters> parameters)
{
    DetourCrowdAgent* agent = nullptr;

    return agent;
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

int
DetourNavigationMesh::rasterizeTileLayers(const int tileX, const int tileY, const rcConfig& cfg, TileCacheData* tiles, const int maxTiles)
{
    if (!_inputGeom || !_inputGeom->getMesh() || !_inputGeom->getChunkyMesh())
    {
        ERR_PRINT("DTNavMeshRasterizeTileLayers: Input mesh is not specified");
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
    tcfg.bmin[2] = cfg.bmin[2] + tileY*tcs;
    tcfg.bmax[0] = cfg.bmin[0] + (tileX+1)*tcs;
    tcfg.bmax[1] = cfg.bmax[1];
    tcfg.bmax[2] = cfg.bmin[2] + (tileY+1)*tcs;
    tcfg.bmin[0] -= tcfg.borderSize*tcfg.cs;
    tcfg.bmin[2] -= tcfg.borderSize*tcfg.cs;
    tcfg.bmax[0] += tcfg.borderSize*tcfg.cs;
    tcfg.bmax[2] += tcfg.borderSize*tcfg.cs;

    // Allocate voxel heightfield where we rasterize our input data to.
    rc.solid = rcAllocHeightfield();
    if (!rc.solid)
    {
        ERR_PRINT("DTNavMeshRasterizeTileLayers:  Out of memory 'solid'");
        return 0;
    }
    if (!rcCreateHeightfield(_recastContext, *rc.solid, tcfg.width, tcfg.height, tcfg.bmin, tcfg.bmax, tcfg.cs, tcfg.ch))
    {
        ERR_PRINT("DTNavMeshRasterizeTileLayers: Could not create solid heightfield");
        return 0;
    }

    // Allocate array that can hold triangle flags.
    // If you have multiple meshes you need to process, allocate
    // and array which can hold the max number of triangles you need to process.
    rc.triareas = new unsigned char[chunkyMesh->maxTrisPerChunk];
    if (!rc.triareas)
    {
        ERR_PRINT(String("DTNavMeshRasterizeTileLayers: Out of memory 'm_triareas' {0}").format(Array::make(chunkyMesh->maxTrisPerChunk)));
        return 0;
    }

    float tbmin[2], tbmax[2];
    tbmin[0] = tcfg.bmin[0];
    tbmin[1] = tcfg.bmin[2];
    tbmax[0] = tcfg.bmax[0];
    tbmax[1] = tcfg.bmax[2];
    int cid[512];// TODO: Make grow when returning too many items.
    const int ncid = rcGetChunksOverlappingRect(chunkyMesh, tbmin, tbmax, cid, 512);
    if (!ncid)
    {
        return 0;
    }

    if (ncid > 512)
    {
        WARN_PRINT(String("DTNavMeshRasterizeTileLayers: Got more than 512 chunks. FIXME").format(Array::make(ncid)));
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
            Godot::print("DTNavMeshRasterizeTileLayers: RasterizeTriangles returned false");
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
        ERR_PRINT("DTNavMeshRasterizeTileLayers: Out of memory 'chf'");
        return 0;
    }
    if (!rcBuildCompactHeightfield(_recastContext, tcfg.walkableHeight, tcfg.walkableClimb, *rc.solid, *rc.chf))
    {
        ERR_PRINT("DTNavMeshRasterizeTileLayers: Could not build compact data");
        return 0;
    }

    // Erode the walkable area by agent radius.
    if (!rcErodeWalkableArea(_recastContext, tcfg.walkableRadius, *rc.chf))
    {
        ERR_PRINT("DTNavMeshRasterizeTileLayers: Could not erode");
        return 0;
    }

    // Mark areas (as water, grass, road, etc.), by default, everything is ground
    const ConvexVolume* vols = _inputGeom->getConvexVolumes();
    for (int i  = 0; i < _inputGeom->getConvexVolumeCount(); ++i)
    {
        rcMarkConvexPolyArea(_recastContext, vols[i].verts, vols[i].nverts, vols[i].hmin, vols[i].hmax, (unsigned char)vols[i].area, *rc.chf);
    }

    rc.lset = rcAllocHeightfieldLayerSet();
    if (!rc.lset)
    {
        ERR_PRINT("DTNavMeshRasterizeTileLayers: Out of memory 'lset'");
        return 0;
    }
    if (!rcBuildHeightfieldLayers(_recastContext, *rc.chf, tcfg.borderSize, tcfg.walkableHeight, *rc.lset))
    {
        ERR_PRINT("DTNavMeshRasterizeTileLayers: Could not build heighfield layers");
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
        header.ty = tileY;
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
            ERR_PRINT("DTNavMeshRasterizeTileLayers: Unable to build tile cache layer");
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
