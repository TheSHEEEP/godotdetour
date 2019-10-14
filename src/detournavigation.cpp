#include "detournavigation.h"
#include <MeshInstance.hpp>
#include "util/detourinputgeometry.h"
#include "detourobstacle.h"

using namespace godot;

void
DetourNavigationParameters::_register_methods()
{
}

void
DetourNavigation::_register_methods()
{
}

DetourNavigation::DetourNavigation()
    : _inputGeometry(nullptr)
{
    _inputGeometry = new DetourInputGeometry();
}

DetourNavigation::~DetourNavigation()
{
    delete _inputGeometry;
}

bool
DetourNavigation::initalize(Variant inputMesh, DetourNavigationMeshParameters parameters)
{
    // Don't do anything if already initialized

    // Make sure we got the input we need

    // Create the input geometry from the passed mesh

    // Initialize the navigation mesh(es)

    // Start the working thread

    return true;
}

DetourCrowdAgent*
DetourNavigation::addAgent(DetourCrowdAgentParameters parameters)
{
    // Find the correct crowd based on the parameters
    DetourNavigationMesh* navMesh = nullptr;

    // Create and add the agent
    return navMesh->addAgent(parameters);
}

DetourObstacle*
DetourNavigation::addCylinderObstacle(Vector3 position, float radius, float height)
{
    // Create the obstacle
    DetourObstacle* obstacle = new DetourObstacle(OBSTACLE_TYPE_CYLINDER);

    // Add the obstacle to all navmeshes

    return obstacle;
}

DetourObstacle*
DetourNavigation::addBoxObstacle(Vector3 position, Vector3 dimensions, float rotationRad)
{
    // Create the obstacle
    DetourObstacle* obstacle = new DetourObstacle(OBSTACLE_TYPE_BOX);

    // Add the obstacle to all navmeshes

    return obstacle;
}

MeshInstance*
DetourNavigation::createDebugMesh(int index)
{
    // TODO: Make sure only one node exists per navmesh
    MeshInstance* debugNode;

    // Get the navmesh

    // Create the debug mesh

    return debugNode;
}
