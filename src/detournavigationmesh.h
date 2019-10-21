#ifndef DETOURNAVIGATIONMESH_H
#define DETOURNAVIGATIONMESH_H

#include <Godot.hpp>
#include <Vector2.hpp>
#include "detourcrowdagent.h"

class DetourInputGeometry;
class dtTileCache;

namespace godot
{
    class DetourObstacle;

    /**
     * @brief Parameters to initialize a DetourNavigationMesh.
     */
    struct DetourNavigationMeshParameters : public Reference
    {
        GODOT_CLASS(DetourNavigationMeshParameters, Reference)

        public:

        static void _register_methods();

        // It is important to understand that recast/detour operates on a voxel field internally.
        // The size of a single voxel (often called cell internally) has significant influence on how a navigation mesh is created.
        // A tile is a rectangular region within the navigation mesh. In other words, every navmesh is divided into equal-sized tiles.
        // The detail mesh is a mesh used for determining surface height on the polygons of the navigation mesh.
        // Units are usually in world units [wu] (e.g. meters, or whatever you use), but some may be in voxel units [vx] (multiples of cellSize).
        Vector2     cellSize;               // x = width & depth of a single cell (only one value as both must be the same) | y = height of a single cell. [wu]
        float       maxAgentSlope;          // How steep an angle can be to still be considered walkable. In degree. Max 90.0.
        float       maxAgentHeight;         // The maximum height of an agent supported in this navigation mesh. [wu]
        float       maxAgentClimb;          // How high a single "stair" can be to be considered walkable by an agent. [wu]
        float       maxAgentRadius;         // The maximum radius of an agent in this navigation mesh. [wu]
        float       maxEdgeLength;          // The maximum allowed length for contour edges along the border of the mesh. [wu]
        float       maxSimplificationError; // The maximum distance a simplified contour's border edges should deviate the original raw contour. [vx]
        int         minNumCellsPerIsland;   // How many cells an isolated area must at least have to become part of the navmesh.
        int         minCellSpanCount;       // Any regions with a span count smaller than this value will, if possible, be merged with larger regions.
        int         maxVertsPerPoly;        // Maximum number of vertices per polygon in the navigation mesh.
        int         tileSize;               // The width,depth & height of a single tile. [vx]
        int         layersPerTile;          // How many vertical layers a single tile is expected to have. Should be less for "flat" levels, more for something like tall, multi-floored buildings.
        float       detailSampleDistance;   // The sampling distance to use when generating the detail mesh. [wu]
        float       detailSampleMaxError;   // The maximum allowed distance the detail mesh should deviate from the source data. [wu]
    };

    /**
     * @brief Representation of a single TileMesh and Crowd.
     */
    class DetourNavigationMesh : public Reference
    {
        GODOT_CLASS(DetourNavigationMesh, Reference)

    public:
        static void _register_methods();

        /**
         * @brief Constructor.
         */
        DetourNavigationMesh();

        /**
         * @brief Destructor.
         */
        ~DetourNavigationMesh();

        /**
         * @brief initialize    Initializes the navigation mesh & crowd.
         * @param inputGeom     The input geometry.
         * @param params        The parameters for setting up this navigation mesh + crowd.
         * @param maxObstacles  The maximum amount of obstacles supported.
         * @return True if everything was successful. False otherwise.
         */
        bool initialize(DetourInputGeometry* inputGeom, const DetourNavigationMeshParameters& params, int maxObstacles);

        /**
         * @brief Adds an agent to the navigation.
         * @param parameters    The parameters to initialize the agent with.
         * @return  The instance of the agent. nullptr if an error occurred.
         */
        DetourCrowdAgent* addAgent(Ref<DetourCrowdAgentParameters> parameters);

        /**
         * @brief Adds the passed obstacle to this navmesh.
         */
        void addObstacle(DetourObstacle* obstacle);

        /**
         * @brief Create a debug representation of this obstacle and attach it to the passed node.
         */
        void createDebugMesh(Node* node);

    private:
        dtTileCache*    _tileCache;
    };
}

#endif // DETOURNAVIGATIONMESH_H
