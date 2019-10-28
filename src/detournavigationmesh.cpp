#include "detournavigationmesh.h"
#include <DetourTileCache.h>
#include <SurfaceTool.hpp>
#include "util/detourinputgeometry.h"
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
{

}

DetourNavigationMesh::~DetourNavigationMesh()
{

}

bool
DetourNavigationMesh::initialize(DetourInputGeometry* inputGeom, const DetourNavigationMeshParameters& params, int maxObstacles)
{
    return true;
}

DetourCrowdAgent*
DetourNavigationMesh::addAgent(Ref<DetourCrowdAgentParameters> parameters)
{
    DetourCrowdAgent* agent = nullptr;

    return agent;
}

void
DetourNavigationMesh::addObstacle(DetourObstacle* obstacle)
{
    // Add the obstacle and get the reference
    dtObstacleRef ref;

    // Add the reference to the obstacle
    obstacle->addReference(ref, _tileCache);
}

void
DetourNavigationMesh::createDebugMesh(Node* node)
{

}
