#include "detournavigation.h"
#include <MeshInstance.hpp>
#include <EditorNavigationMeshGenerator.hpp>
#include <NavigationMesh.hpp>
#include <Mesh.hpp>
#include <File.hpp>
#include <Directory.hpp>
#include <thread>
#include <mutex>
#include <chrono>
#include <climits>
#include <DetourCrowd.h>
#include "util/detourinputgeometry.h"
#include "util/recastcontext.h"
#include "util/godotdetourdebugdraw.h"
#include "util/navigationmeshhelpers.h"
#include "detourobstacle.h"

using namespace godot;

#define SAVE_DATA_VERSION 1

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
    register_method("removeAgent", &DetourNavigation::removeAgent);
    register_method("addBoxObstacle", &DetourNavigation::addBoxObstacle);
    register_method("addCylinderObstacle", &DetourNavigation::addCylinderObstacle);
    register_method("createDebugMesh", &DetourNavigation::createDebugMesh);
    register_method("setQueryFilter", &DetourNavigation::setQueryFilter);
    register_method("save", &DetourNavigation::save);
    register_method("load", &DetourNavigation::load);
    register_method("clear", &DetourNavigation::clear);
    register_method("getAgents", &DetourNavigation::getAgents);
    register_method("getObstacles", &DetourNavigation::getObstacles);
    register_method("getMarkedAreaIDs", &DetourNavigation::getMarkedAreaIDs);
}

DetourNavigation::DetourNavigation()
    : _inputGeometry(nullptr)
    , _recastContext(nullptr)
    , _debugDrawer(nullptr)
    , _initialized(false)
    , _ticksPerSecond(60)
    , _maxObstacles(256)
    , _defaultAreaType(0)
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
    _defaultAreaType = parameters->defaultAreaType;
    for (int i = 0; i < parameters->navMeshParameters.size(); ++i)
    {
        Ref<DetourNavigationMeshParameters> navMeshParams = parameters->navMeshParameters[i];
        DetourNavigationMesh* navMesh = new DetourNavigationMesh();

        if (!navMesh->initialize(_inputGeometry, navMeshParams, _maxObstacles, _recastContext))
        {
            ERR_PRINT("Unable to initialize detour navigation mesh!");
            return false;
        }
        _navMeshes.push_back(navMesh);
    }

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

    // Mark the volumes as handled
    int volumeCount = _inputGeometry->getConvexVolumeCount();
    for (int i = 0; i < volumeCount; ++i)
    {
        // Get volume
        ConvexVolume volume = _inputGeometry->getConvexVolumes()[i];
        volume.isNew = false;
    }
    _navigationMutex->unlock();
}

int
DetourNavigation::markConvexArea(Array vertices, float height, unsigned int areaType)
{
    // Sanity checks
    if (areaType > UCHAR_MAX)
    {
        ERR_PRINT(String("Passed areaType is too large. {0} (of max allowed {1}).").format(Array::make(areaType, UCHAR_MAX)));
        return -1;
    }
    if (_inputGeometry->getConvexVolumeCount() >= (DetourInputGeometry::MAX_VOLUMES - 1))
    {
        ERR_PRINT("Cannot mark any more convex area, limit reached.");
        return -1;
    }

    // Create the vertices array
    float* vertArray = new float[vertices.size() * 3];
    float miny = 10000000.0f;
    for (int i = 0; i < vertices.size(); ++i)
    {
        Vector3 vertex = vertices[i];
        vertArray[i * 3 + 0] = vertex.x;
        vertArray[i * 3 + 1] = vertex.y;
        vertArray[i * 3 + 2] = vertex.z;

        if (vertex.y < miny)
        {
            miny = vertex.y;
        }
    }

    // Add to the input geometry
    _inputGeometry->addConvexVolume(vertArray, vertices.size(), miny, miny + height, areaType);
    delete [] vertArray;
    int id = _inputGeometry->getConvexVolumeCount() - 1;
    _markedAreaIDs.push_back(id);
    return id;
}

void
DetourNavigation::removeConvexAreaMarker(int id)
{
    _inputGeometry->deleteConvexVolume(id);
    for (int i = 0; i < _markedAreaIDs.size(); ++i)
    {
        if (_markedAreaIDs[i] == id)
        {
            _markedAreaIDs.erase(_markedAreaIDs.begin() +i);
            break;
        }
    }
}

bool
DetourNavigation::setQueryFilter(int index, String name, Dictionary weights)
{
    // Check index
    if (index >= 16)
    {
        ERR_PRINT(String("Index exceeds allowed number of query filters: {0}").format(Array::make(index)));
        return false;
    }

    // Set weights
    for (int i = 0; i < _navMeshes.size(); ++i)
    {
        dtCrowd* crowd = _navMeshes[i]->getCrowd();
        dtQueryFilter* filter = crowd->getEditableFilter(index);

        for (int j = 0; j < weights.keys().size(); ++j)
        {
            int areaIndex = weights.keys()[j];
            float weight = weights[weights.keys()[j]];
            filter->setAreaCost(areaIndex, weight);

            if (weight > 10000.0f)
            {
                switch (areaIndex)
                {
                case POLY_AREA_WATER:
                    filter->setExcludeFlags(filter->getExcludeFlags() ^ POLY_FLAGS_SWIM);
                    break;
                case POLY_AREA_JUMP:
                    filter->setExcludeFlags(filter->getExcludeFlags() ^ POLY_FLAGS_JUMP);
                    break;
                case POLY_AREA_DOOR:
                    filter->setExcludeFlags(filter->getExcludeFlags() ^ POLY_FLAGS_DOOR);
                    break;
                case POLY_AREA_GRASS:
                case POLY_AREA_GROUND:
                case POLY_AREA_ROAD:
                    filter->setExcludeFlags(filter->getExcludeFlags() ^ POLY_FLAGS_WALK);
                    break;
                }

            }

        }
    }

    // Assign name
    _queryFilterIndices[name] = index;

    return true;
}

Ref<DetourCrowdAgent> DetourNavigation::addAgent(Ref<DetourCrowdAgentParameters> parameters)
{
    _navigationMutex->lock();

    // Find the correct crowd based on the parameters
    DetourNavigationMesh* navMesh = nullptr;
    float bestFitFactor = 10000.0f;
    for (int i = 0; i < _navMeshes.size(); ++i)
    {
        float fitFactor = _navMeshes[i]->getActorFitFactor(parameters->radius, parameters->height);
        if (fitFactor > 0.0f && fitFactor < bestFitFactor)
        {
            bestFitFactor = fitFactor;
            navMesh = _navMeshes[i];
        }
    }

    // Make sure we got something
    if (navMesh == nullptr)
    {
        ERR_PRINT(String("Unable to add agent: Too big for any crowd: radius: {0} width: {1}").format(Array::make(parameters->radius, parameters->height)));
        _navigationMutex->unlock();
        return nullptr;
    }

    // Make sure the agent uses a known filter
    if (_queryFilterIndices.find(parameters->filterName) == _queryFilterIndices.end())
    {
        ERR_PRINT(String("Unable to add agent: Unknown filter: {0}").format(Array::make(parameters->filterName)));
        _navigationMutex->unlock();
        return nullptr;
    }

    // Create and add the agent as main
    Ref<DetourCrowdAgent> agent = DetourCrowdAgent::_new();
    if (!navMesh->addAgent(agent, parameters))
    {
        ERR_PRINT("Unable to add agent.");
        _navigationMutex->unlock();
        return nullptr;
    }
    agent->setFilter(_queryFilterIndices[parameters->filterName]);

    // Add the agent's shadows
    for (int i = 0; i < _navMeshes.size(); ++i)
    {
        if (_navMeshes[i] != navMesh)
        {
            if(!_navMeshes[i]->addAgent(agent, parameters, false))
            {
                ERR_PRINT(String("Unable to add agent's shadow: {0}.").format(Array::make(i)));
                _navigationMutex->unlock();
                return nullptr;
            }
        }
    }

    // Add to our list of agents
    _agents.push_back(agent);

    _navigationMutex->unlock();
    return agent;
}


void
DetourNavigation::removeAgent(Ref<DetourCrowdAgent> agent)
{
    _navigationMutex->lock();

    // Agents should not be removed while the nav thread is busy
    // Thus this function is used instead of exposing destroy() to GDScript
    agent->destroy();

    // Remove from the vector
    for (int i = 0; i < _agents.size(); ++i)
    {
        if (_agents[i] == agent)
        {
            _agents.erase(_agents.begin() + i);
            break;
        }
    }

    _navigationMutex->unlock();
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

    _obstacles.push_back(obstacle);
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

    _obstacles.push_back(obstacle);
    _navigationMutex->unlock();
    return obstacle;
}

MeshInstance*
DetourNavigation::createDebugMesh(int index, bool drawCacheBounds)
{
    _navigationMutex->lock();

    // Sanity check
    if (index > _navMeshes.size() - 1)
    {
        ERR_PRINT(String("Index higher than number of available navMeshes: {0} {1}").format(Array::make(index, _navMeshes.size())));
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

bool
DetourNavigation::save(String path, bool compressed)
{
    // Sanity check
    if (!_initialized)
    {
        ERR_PRINT("Unable to save navigation data. Navigation not initialized.");
        return false;
    }

    // Open the file
    Ref<File> saveFile = File::_new();
    Error result;
    Ref<Directory> dir = Directory::_new();
    result = dir->make_dir_recursive(path.left(path.find_last("/")));
    if (result != Error::OK)
    {
        ERR_PRINT(String("Error while creating navigation file path: {0} {1}").format(Array::make(path, (int)result)));
        return false;
    }
    if (compressed)
    {
        result = saveFile->open_compressed(path, File::WRITE, File::COMPRESSION_ZSTD);
    }
    else
    {
        result = saveFile->open(path, File::WRITE);
    }
    if (result != Error::OK)
    {
        ERR_PRINT(String("Error while opening navigation save file: {0} {1}").format(Array::make(path, (int)result)));
        return false;
    }

    // Version
    saveFile->store_16(SAVE_DATA_VERSION);

    // Input geometry

    _navigationMutex->lock();

    // Navmeshes

    // Agents

    // Obstacles

    // Marked area IDs

    _navigationMutex->unlock();

    saveFile->close();

    return true;
}

bool
DetourNavigation::load(String path, bool compressed)
{
    // Sanity check
    if (_initialized)
    {
        ERR_PRINT("Unable to load new navigation data. Navigation still running, please use clear().");
        return false;
    }

    // Load from file

    // Decompress

    // Version

    // Input geometry

    // Navmesh(es)

    // Agents

    // Obstacles

    // Marked area IDs

    // Done. Start the thread.
    _navigationThread = new std::thread(&DetourNavigation::navigationThreadFunction, this);
    _initialized = true;
    return true;
}

void
DetourNavigation::clear()
{
    // Stop the thread
    _stopThread = true;
    _navigationThread->join();
    delete _navigationThread;

    // Remove all agents
    for (int i = 0; i < _agents.size(); ++i)
    {
        _agents[i]->destroy();
    }
    _agents.clear();

    // Remove all obstacles
    for (int i = 0; i < _obstacles.size(); ++i)
    {
        _obstacles[i]->destroy();
    }
    _obstacles.clear();

    // Remove all marked areas
    for (int i = 0; i < _markedAreaIDs.size(); ++i)
    {
        removeConvexAreaMarker(_markedAreaIDs[i]);
    }
    _markedAreaIDs.clear();

    // Clear the input geometry data
    _inputGeometry->clearData();

    // Free the navigation meshes
    for (int i = 0; i < _navMeshes.size(); ++i)
    {
        delete _navMeshes[i];
    }
    _navMeshes.clear();

    // Other misc stuff
    _queryFilterIndices.clear();
    _initialized = false;
}

Array
DetourNavigation::getAgents()
{
    Array result;

    for (int i = 0; _agents.size(); ++i)
    {
        result.append(_agents[i]);
    }

    return result;
}

Array
DetourNavigation::getObstacles()
{
    Array result;

    for (int i = 0; _obstacles.size(); ++i)
    {
        if (_obstacles[i]->isDestroyed())
        {
            continue;
        }
        result.append(_obstacles[i]);
    }

    return result;
}

Array
DetourNavigation::getMarkedAreaIDs()
{
    Array result;

    for (int i = 0; _markedAreaIDs.size(); ++i)
    {
        result.append(_markedAreaIDs[i]);
    }

    return result;
}

void
DetourNavigation::navigationThreadFunction()
{
    double lastExecutionTime = 0.0;
    double secondsToSleepPerFrame = 1.0 / _ticksPerSecond;
    int64_t millisecondsToSleep = 0;
    auto start = std::chrono::system_clock::now();
    while (!_stopThread)
    {
        millisecondsToSleep = (secondsToSleepPerFrame - lastExecutionTime) * 1000.0 + 0.5;
        std::this_thread::sleep_for(std::chrono::milliseconds(millisecondsToSleep));

        start = std::chrono::system_clock::now();
        _navigationMutex->lock();

        // Remove obstacles from list if they were destroyed
        for (int i = 0; i < _obstacles.size(); ++i)
        {
            if (_obstacles[i]->isDestroyed())
            {
                _obstacles.erase(_obstacles.begin() + i);
                i--;
            }
        }

        // Apply new movement requests (won't do anything if there's no new target)
        for (int i = 0; i < _agents.size(); ++i)
        {
            _agents[i]->applyNewTarget();
        }

        // Update the navmeshes
        for (int i = 0; i < _navMeshes.size(); ++i)
        {
            _navMeshes[i]->update(secondsToSleepPerFrame);
        }

        // Update the agents
        for (int i = 0; i < _agents.size(); ++i)
        {
            _agents[i]->update();
        }

        _navigationMutex->unlock();
        auto timeTaken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start).count();
        lastExecutionTime = timeTaken / 1000.0;
    }
}
