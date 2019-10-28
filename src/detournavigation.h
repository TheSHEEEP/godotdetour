#ifndef DETOURNAVIGATION_H
#define DETOURNAVIGATION_H

#include <Godot.hpp>
#include <Array.hpp>
#include <vector>
#include "detournavigationmesh.h"
#include "detourcrowdagent.h"

class DetourInputGeometry;
class RecastContext;

namespace godot
{
    class DetourObstacle;
    class MeshInstance;

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
         * @brief initalize     Initialize the navigation. If called on an already initialized instance, will return false.
         * @param inputMesh     The input MeshInstance to build the navigation mesh(es) from.
         * @param parameters    The parameters for setting up the navigation.
         * @return True if everything worked fine. False otherwise.
         */
        bool initialize(Variant inputMeshInstance, Ref<DetourNavigationParameters> parameters);

        /**
         * @brief Adds an agent to the navigation.
         * @param parameters    The parameters to initialize the agent with.
         * @return  The instance of the agent. nullptr if an error occurred.
         */
        DetourCrowdAgent* addAgent(Ref<DetourCrowdAgentParameters> parameters);

        /**
         * @brief Add a cylindric dynamic obstacle.
         * @param position  The position of the obstacle (this is the bottom center of the cylinder.
         * @param radius    The radius of the obstacle.
         * @param height    The height of the obstacle.
         * @return  The obstacle, if everything worked out. nullptr if otherwise.
         */
        DetourObstacle* addCylinderObstacle(Vector3 position, float radius, float height);

        /**
         * @brief Add a box dynamic obstacle.
         * @param position      The position of the obstacle (this is the bottom center of the cylinder.
         * @param dimensions    The dimensions of the box.
         * @param rotationRad   The rotation around the y-axis, in radians.
         * @return  The obstacle, if everything worked out. nullptr if otherwise.
         */
        DetourObstacle* addBoxObstacle(Vector3 position, Vector3 dimensions, float rotationRad);

        /**
         * @brief Creates a debug mesh for the navmesh at the passed index.
         * @param index     The index of the navmesh to enable/disable debug drawing for.
         * @return  The MeshInstance holding all the debug meshes.
         */
        MeshInstance* createDebugMesh(int index);

    private:
        DetourInputGeometry*                _inputGeometry;
        std::vector<DetourNavigationMesh*>  _navMeshes;

        RecastContext*      _recastContext;

        bool    _initialized;
        // TODO: threading
    };
}

#endif // DETOURNAVIGATION_H
