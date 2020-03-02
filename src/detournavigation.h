#ifndef DETOURNAVIGATION_H
#define DETOURNAVIGATION_H

#include <Godot.hpp>
#include <Array.hpp>
#include <vector>
#include <atomic>
#include <map>
#include "detournavigationmesh.h"
#include "detourcrowdagent.h"

class DetourInputGeometry;
class RecastContext;
class GodotDetourDebugDraw;

namespace std
{
    class thread;
    class mutex;
}

namespace godot
{
    class DetourObstacle;
    class MeshInstance;
    class Material;

    /**
     * @brief Parameters to initialize a DetourNavigation.
     */
    struct DetourNavigationParameters : public Reference
    {
        GODOT_CLASS(DetourNavigationParameters, Reference)

    public:
        /**
         * @brief Called when .new() is called in gdscript
         */
        void _init() {}

        static void _register_methods();

        Array navMeshParameters;    // The number of elements in this array determines how many DetourNavigationMeshes there will be.
        int ticksPerSecond;         // How many updates per second the navigation shall do in its thread.
        int maxObstacles;           // The maximum amount of obstacles allowed at the same time. Obstacles beyond this amount will be rejected.
        int defaultAreaType;        // The default area type to mark geometry as
    };

    /**
     * @brief Main class to initialize GodotDetour and interact with it.
     */
    class DetourNavigation : public Reference
    {
        GODOT_CLASS(DetourNavigation, Reference)

    public:
        static void _register_methods();

        /**
         * @brief Constructor.
         */
        DetourNavigation();

        /**
         * @brief Destructor.
         */
        ~DetourNavigation();

        /**
         * @brief Called when .new() is called in gdscript
         */
        void _init() {}

        /**
         * @return True if the Navigation is initialized.
         */
        bool isInitialized();

        /**
         * @brief initalize     Initialize the navigation. If called on an already initialized instance, will return false.
         * @param inputMesh     The input MeshInstance to build the navigation mesh(es) from.
         * @param parameters    The parameters for setting up the navigation.
         * @return True if everything worked fine. False otherwise.
         */
        bool initialize(Variant inputMeshInstance, Ref<DetourNavigationParameters> parameters);

        /**
         * @brief Rebuilds all tiles that have changed (by marking areas).
         */
        void rebuildChangedTiles();

        /**
         * @brief Marks the area as the passed type (influencing crowds based on their area filters).
         * @param vertices  The vertices forming the bottom of the polygon.
         * @param height    The height of the polygon (extended upwards from the vertices).
         * @param areaType  Which area type to mark as.
         */
        int markConvexArea(Array vertices, float height, unsigned int areaType);

        /**
         * @brief Removes the convex marked area with the passed id.
         */
        void removeConvexAreaMarker(int id);

        /**
         * @brief Creates and adds an off-mesh connection.
         * @param from  The source point.
         * @param to    The target point.
         * @param bidirectional If this connection is bi-directional.
         * @param radius    The radius of the entry/exit points.
         * @param areaType  The area type of the entry/exit points (ground, grass, etc.).
         * @return -1 if there was an error, the ID of the connection otherwise.
         */
        int addOffMeshConnection(Vector3 from, Vector3 to, bool bidirectional, float radius, int areaType);

        /**
         * @brief Removes the off-mesh connection with the passed ID.
         */
        void removeOffMeshConnection(int id);

        /**
         * @brief setQueryFilter    Sets the filter at the passed index.
         * @param index     The index to set. 0-15.
         * @param name      The name the filter at this index shall have.
         * @param weights   The weight of each area type in an int : float fashion.
         * @return True if everything worked out fine.
         */
        bool setQueryFilter(int index, String name, Dictionary weights);

        /**
         * @brief Adds an agent to the navigation.
         * @param parameters    The parameters to initialize the agent with.
         * @return  The instance of the agent. nullptr if an error occurred.
         */
        Ref<DetourCrowdAgent> addAgent(Ref<DetourCrowdAgentParameters> parameters);

        /**
         * @brief Will remove the passed agent.
         */
        void removeAgent(Ref<DetourCrowdAgent> agent);

        /**
         * @brief Add a cylindric dynamic obstacle.
         * @param position  The position of the obstacle (this is the bottom center of the cylinder.
         * @param radius    The radius of the obstacle.
         * @param height    The height of the obstacle.
         * @return  The obstacle, if everything worked out. nullptr if otherwise.
         */
        Ref<DetourObstacle> addCylinderObstacle(Vector3 position, float radius, float height);

        /**
         * @brief Add a box dynamic obstacle.
         * @param position      The position of the obstacle (this is the bottom center of the cylinder.
         * @param dimensions    The dimensions of the box.
         * @param rotationRad   The rotation around the y-axis, in radians.
         * @return  The obstacle, if everything worked out. nullptr if otherwise.
         */
        Ref<DetourObstacle> addBoxObstacle(Vector3 position, Vector3 dimensions, float rotationRad);

        /**
         * @brief Creates a debug mesh for the navmesh at the passed index.
         * @param index     The index of the navmesh to enable/disable debug drawing for.
         * @return  The MeshInstance holding all the debug meshes.
         */
        MeshInstance* createDebugMesh(int index, bool drawCacheBounds);

        /**
         * @brief Saves the current state of the navigation, including all marked areas, temp obstacles and agents.
         * @param path          The path to the file to save to.
         * @param compressed    If the data should be compressed.
         */
        bool save(String path, bool compressed);

        /**
         * @brief Loads an entire navigation state, including marked areas, temp obstacles and agents.
         * @param path          The path to the file to load from.
         * @param compressed    If the data is expected to be compressed.
         */
        bool load(String path, bool compressed);

        /**
         * @brief Clears the entire navigation (all the data) and stops the navigation thread.
         *          After this, a new initialize (or load) will be required.
         */
        void clear();

        /**
         * @brief Returns all current agents.
         */
        Array getAgents();

        /**
         * @brief Returns all current temporary obstacles.
         */
        Array getObstacles();

        /**
         * @brief Returns all currently marked areas' IDs.
         */
        Array getMarkedAreaIDs();

        /**
         * @brief This function is the thread running in the background, taking care of navigation updates.
         */
        void navigationThreadFunction();

    private:
        DetourInputGeometry*                _inputGeometry;
        std::vector<DetourNavigationMesh*>  _navMeshes;
        std::vector<Ref<DetourCrowdAgent> > _agents;
        std::vector<Ref<DetourObstacle> >   _obstacles;
        std::vector<int>                    _markedAreaIDs;
        std::vector<int>                    _offMeshConnections;


        RecastContext*          _recastContext;
        GodotDetourDebugDraw*   _debugDrawer;


        bool    _initialized;
        int     _ticksPerSecond;
        int     _maxObstacles;
        int     _defaultAreaType;

        std::thread*        _navigationThread;
        std::atomic_bool    _stopThread;
        std::mutex*         _navigationMutex;

        std::map<String, int>   _queryFilterIndices;
    };

    // INLINES

    inline bool
    DetourNavigation::isInitialized()
    {
        return _initialized;
    }
}

#endif // DETOURNAVIGATION_H
