#include "detournavigation.h"
#include <MeshInstance.hpp>
#include <EditorNavigationMeshGenerator.hpp>
#include <NavigationMesh.hpp>
#include <Mesh.hpp>
#include <thread>
#include <mutex>
#include <chrono>
#include <climits>
#include "util/detourinputgeometry.h"
#include "util/recastcontext.h"
#include "util/godotdetourdebugdraw.h"
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
    register_method("rebuildChangedTiles", &DetourNavigation::rebuildChangedTiles);
    register_method("markConvexArea", &DetourNavigation::markConvexArea);
    register_method("addAgent", &DetourNavigation::addAgent);
    register_method("addBoxObstacle", &DetourNavigation::addBoxObstacle);
    register_method("addCylinderObstacle", &DetourNavigation::addCylinderObstacle);
    register_method("createDebugMesh", &DetourNavigation::createDebugMesh);
}

DetourNavigation::DetourNavigation()
    : _inputGeometry(nullptr)
    , _recastContext(nullptr)
    , _debugDrawer(nullptr)
    , _initialized(false)
    , _ticksPerSecond(60)
    , _maxObstacles(256)
    , _navigationThread(nullptr)
    , _stopThread(false)
    , _navigationMutex(nullptr)
{
    _inputGeometry = new DetourInputGeometry();
    _recastContext = new RecastContext();
    _navigationMutex = new std::mutex();
}

DetourNavigation::~DetourNavigation()
{
    _stopThread = true;
    if (_navigationThread)
    {
        _navigationThread->join();
    }
    delete _navigationThread;
    delete _navigationMutex;

    for (int i = 0; i < _navMeshes.size(); ++i)
    {
        delete _navMeshes[i];
    }
    _navMeshes.clear();

    if (_debugDrawer)
    {
        delete _debugDrawer;
    }

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
    for (int i = 0; i < parameters->navMeshParameters.size(); ++i)
    {
        Ref<DetourNavigationMeshParameters> navMeshParams = parameters->navMeshParameters[i];
        DetourNavigationMesh* navMesh = new DetourNavigationMesh();

        if (!navMesh->initialize(_inputGeometry, navMeshParams, _maxObstacles, _recastContext, _unappliedConvexVolumes))
        {
            ERR_PRINT("Unable to initialize detour navigation mesh!");
            return false;
        }
        _navMeshes.push_back(navMesh);
    }

    // Clear stored convex volumes
    for (int i = 0; i < _unappliedConvexVolumes.size(); ++i)
    {
        ConvexVolumeData* data = _unappliedConvexVolumes[i];
        delete data;
    }
    _unappliedConvexVolumes.clear();

    // Start the navigation thread
    _navigationThread = new std::thread(&DetourNavigation::navigationThreadFunction, this);

    _initialized = true;
    return true;
}

void
DetourNavigation::rebuildChangedTiles()
{
    _navigationMutex->lock();
    for (int i = 0; i < _navMeshes.size(); ++i)
    {
        _navMeshes[i]->rebuildChangedTiles();
    }
    _navigationMutex->unlock();
}

void
DetourNavigation::markConvexArea(Array vertices, float height, unsigned int areaType)
{
    // Sanity check
    if (areaType > UCHAR_MAX)
    {
        ERR_PRINT(String("Passed areaType is too large. {0} (of max allowed {1}).").format(Array::make(areaType, UCHAR_MAX)));
        return;
    }

    // Remember this if the navmeshes weren't initialized yet
    if (_navMeshes.size() == 0)
    {
        ConvexVolumeData* data = new ConvexVolumeData();
        data->vertices = vertices;
        data->height = height;
        data->areaType = areaType;
        _unappliedConvexVolumes.push_back(data);
    }
    else
    {
        for (int i = 0; i < _navMeshes.size(); ++i)
        {
            _navMeshes[i]->markConvexArea(vertices, height, (unsigned char)areaType);
        }
    }
}

DetourCrowdAgent*
DetourNavigation::addAgent(Ref<DetourCrowdAgentParameters> parameters)
{
    _navigationMutex->lock();

    // Find the correct crowd based on the parameters
    DetourNavigationMesh* navMesh = nullptr;

    // Create and add the agent
    DetourCrowdAgent* agent = navMesh->addAgent(parameters);

    _navigationMutex->unlock();
    return agent;
}

Ref<DetourObstacle>
DetourNavigation::addCylinderObstacle(Vector3 position, float radius, float height)
{
    _navigationMutex->lock();

    // Create the obstacle
    Ref<DetourObstacle> obstacle = DetourObstacle::_new();
    obstacle->initialize(OBSTACLE_TYPE_CYLINDER, position, Vector3(radius, height, 0.0f), 0.0f);

    // Add the obstacle to all navmeshes
    for (int i = 0; i < _navMeshes.size(); ++i)
    {
        _navMeshes[i]->addObstacle(obstacle);
    }

    _navigationMutex->unlock();
    return obstacle;
}

Ref<DetourObstacle>
DetourNavigation::addBoxObstacle(Vector3 position, Vector3 dimensions, float rotationRad)
{
    _navigationMutex->lock();

    // Create the obstacle
    Ref<DetourObstacle> obstacle = DetourObstacle::_new();
    obstacle->initialize(OBSTACLE_TYPE_BOX, position, dimensions, rotationRad);

    // Add the obstacle to all navmeshes
    for (int i = 0; i < _navMeshes.size(); ++i)
    {
        _navMeshes[i]->addObstacle(obstacle);
    }

    _navigationMutex->unlock();
    return obstacle;
}

MeshInstance*
DetourNavigation::createDebugMesh(int index, bool drawCacheBounds)// Ref<Material> material)
{
    _navigationMutex->lock();

    // Sanity check
    if (index > _navMeshes.size() - 1)
    {
        ERR_PRINT(String("Index higher than number of available navMeshes:").format(Array::make(index, _navMeshes.size())));
        return nullptr;
    }

    // Create the debug drawing object if it doesn't exist yet
    if (!_debugDrawer)
    {
        _debugDrawer = new GodotDetourDebugDraw();
    }
    //_debugDrawer->setMaterial(material);

    // Get the navmesh
    DetourNavigationMesh* navMesh = _navMeshes[index];

    // Create the debug mesh
    _debugDrawer->clear();
    navMesh->createDebugMesh(_debugDrawer, drawCacheBounds);

    // Add the result to the MeshInstance and return it
    MeshInstance* meshInst = MeshInstance::_new();
    meshInst->set_mesh(_debugDrawer->getArrayMesh());


    _navigationMutex->unlock();
    return meshInst;
}

void
DetourNavigation::navigationThreadFunction()
{
    float timeDelta = 0.0f;
    float lastExecutionTime = 0.0f;
    float secondsToSleepPerFrame = 1.0f / _ticksPerSecond;
    int64_t millisecondsToSleep = 0;
    auto start = std::chrono::system_clock::now();
    while(!_stopThread)
    {
        millisecondsToSleep = (secondsToSleepPerFrame - lastExecutionTime) * 1000.0 + 0.5f;
        std::this_thread::sleep_for(std::chrono::milliseconds(millisecondsToSleep));

        start = std::chrono::system_clock::now();
        _navigationMutex->lock();
        for(int i = 0; i < _navMeshes.size(); ++i)
        {
            _navMeshes[i]->update(secondsToSleepPerFrame);
        }
        _navigationMutex->unlock();
        auto timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count();
        lastExecutionTime = timeTaken / 1000.0f;
    }
}
