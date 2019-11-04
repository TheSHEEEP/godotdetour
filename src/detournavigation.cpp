#include "detournavigation.h"
#include <MeshInstance.hpp>
#include <EditorNavigationMeshGenerator.hpp>
#include <NavigationMesh.hpp>
#include <Mesh.hpp>
#include "util/detourinputgeometry.h"
#include "util/recastcontext.h"
#include "detourobstacle.h"

using namespace godot;

void
DetourNavigationParameters::_register_methods()
{
    register_property<DetourNavigationParameters, Array>("navMeshParameters", &DetourNavigationParameters::navMeshParameters, Array());
    register_property<DetourNavigationParameters, int>("ticksPerSecond", &DetourNavigationParameters::ticksPerSecond, 60);
    register_property<DetourNavigationParameters, int>("maxObstacles", &DetourNavigationParameters::maxObstacles, 256);
}

void
DetourNavigation::_register_methods()
{
    register_method("initialize", &DetourNavigation::initialize);
    register_method("addAgent", &DetourNavigation::addAgent);
    register_method("addBoxObstacle", &DetourNavigation::addBoxObstacle);
    register_method("addCylinderObstacle", &DetourNavigation::addCylinderObstacle);
    register_method("createDebugMesh", &DetourNavigation::createDebugMesh);
}

DetourNavigation::DetourNavigation()
    : _inputGeometry(nullptr)
    , _recastContext(nullptr)
    , _initialized(false)
    , _ticksPerSecond(60)
    , _maxObstacles(256)
{
    _inputGeometry = new DetourInputGeometry();
    _recastContext = new RecastContext();
}

DetourNavigation::~DetourNavigation()
{
    for (int i = 0; i < _navMeshes.size(); ++i)
    {
        delete _navMeshes[i];
    }
    _navMeshes.clear();

    delete _inputGeometry;
    delete _recastContext;
}

bool
DetourNavigation::initialize(Variant inputMeshInstance, Ref<DetourNavigationParameters> parameters)
{
    // Don't do anything if already initialized
    if (_initialized)
    {
        ERR_PRINT("DetourNavigation already initialized.");
        return false;
    }

    // Make sure we got the input we need
    MeshInstance* meshInstance = Object::cast_to<MeshInstance>(inputMeshInstance.operator Object*());
    if (meshInstance == nullptr)
    {
        ERR_PRINT("Passed inputMesh must be of type Mesh or MeshInstance.");
        return false;
    }

    // Check if the mesh instance actually has a mesh
    Ref<Mesh> meshToConvert = meshInstance->get_mesh();
    if (meshToConvert.ptr() == nullptr)
    {
        ERR_PRINT("Passed MeshInstance does not have a mesh.");
        return false;
    }

    // Create the input geometry from the passed mesh
    if (!_inputGeometry->loadMesh(_recastContext, meshInstance))
    {
        ERR_PRINT("Input geometry failed to load the mesh.");
        return false;
    }

    // Initialize the navigation mesh(es)
    _ticksPerSecond = parameters->ticksPerSecond;
    _maxObstacles = parameters->maxObstacles;
    for (int i = 0; parameters->navMeshParameters.size(); ++i)
    {
        Ref<DetourNavigationMeshParameters> navMeshParams = parameters->navMeshParameters[i];
        DetourNavigationMesh* navMesh = new DetourNavigationMesh();
        if (!navMesh->initialize(_inputGeometry, navMeshParams, _maxObstacles, _recastContext))
        {
            ERR_PRINT("Unable to initialize detour navigation mesh!");
            return false;
        }
        _navMeshes.push_back(navMesh);

        // TODO: Remove this! Only one navmesh at first for testing
        break;
    }

    // Start the navigation thread

    _initialized = true;
    return true;
}

DetourCrowdAgent*
DetourNavigation::addAgent(Ref<DetourCrowdAgentParameters> parameters)
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
    DetourObstacle* obstacle = new DetourObstacle();
    obstacle->initialize(OBSTACLE_TYPE_CYLINDER, position, Vector3(radius, height, 0.0f), 0.0f);

    // Add the obstacle to all navmeshes

    return obstacle;
}

DetourObstacle*
DetourNavigation::addBoxObstacle(Vector3 position, Vector3 dimensions, float rotationRad)
{
    // Create the obstacle
    DetourObstacle* obstacle = new DetourObstacle();
    obstacle->initialize(OBSTACLE_TYPE_BOX, position, dimensions, rotationRad);

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
