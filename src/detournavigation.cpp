#include "detournavigation.h"
#include <MeshInstance.hpp>
#include <Mesh.hpp>
#include "util/detourinputgeometry.h"
#include "detourobstacle.h"

using namespace godot;

void
DetourNavigationParameters::_register_methods()
{
    register_method("initialize", &DetourNavigation::initialize);
    register_method("addAgent", &DetourNavigation::addAgent);
    register_method("addBoxObstacle", &DetourNavigation::addBoxObstacle);
    register_method("addCylinderObstacle", &DetourNavigation::addCylinderObstacle);
    register_method("createDebugMesh", &DetourNavigation::createDebugMesh);
}

void
DetourNavigation::_register_methods()
{
}

DetourNavigation::DetourNavigation()
    : _inputGeometry(nullptr)
    , _recastContext(nullptr)
    , _initialized(false)
{
    _inputGeometry = new DetourInputGeometry();
    _recastContext = new RecastContext();
}

DetourNavigation::~DetourNavigation()
{
    delete _inputGeometry;
    delete _recastContext;
}

bool
DetourNavigation::initalize(Variant inputMeshInstance, DetourNavigationParameters parameters)
{
    // Don't do anything if already initialized
    if (_initialized)
    {
        ERR_PRINT("DetourNavigation already initialized.");
        return false;
    }

    // Make sure we got the input we need
    Ref<MeshInstance> inputMeshInstance = Object::cast_to<MeshInstance>(inputMeshInstance.operator Object*());
    if (inputMeshInstance.ptr() == nullptr)
    {
        ERR_PRINT("Passed inputMesh must be of type Mesh or MeshInstance.");
        return false;
    }

    // Check if the mesh instance actually has a mesh
    Ref<Mesh> meshToConvert = inputMeshInstance->get_mesh().operator *();
    if (meshToConvert.ptr() == nullptr)
    {
        ERR_PRINT("Passed MeshInstance does not have a mesh.");
        return false;
    }

    // Create the input geometry from the passed mesh
    if (!_inputGeometry->loadMesh(_recastContext, inputMeshInstance))
    {
        ERR_PRINT("Input geometry failed to load the mesh.");
        return false;
    }

    // Initialize the navigation mesh(es)

    // Start the working thread

    _initialized = true;
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
