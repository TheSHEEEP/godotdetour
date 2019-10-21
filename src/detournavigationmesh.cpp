#include "detournavigationmesh.h"
#include <DetourTileCache.h>
#include <SurfaceTool.hpp>
#include "util/detourinputgeometry.h"
#include "detourobstacle.h"

using namespace godot;

void
DetourNavigationMeshParameters::_register_methods()
{
    // TODO: Do we really need to expose this class to GDScript?
}

void
DetourNavigationMesh::_register_methods()
{
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
DetourNavigationMesh::addAgent(DetourCrowdAgentParameters parameters)
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
