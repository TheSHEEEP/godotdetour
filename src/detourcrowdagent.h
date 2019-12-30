#ifndef DETOURCROWDAGENT_H
#define DETOURCROWDAGENT_H

#include <Godot.hpp>
#include <vector>
#include <map>
#include <atomic>

class dtCrowdAgent;
class dtCrowd;
class dtNavMeshQuery;
class dtQueryFilter;
class DetourInputGeometry;

namespace godot
{
    class DetourNavigationMesh;

    /**
     * @brief Parameters to initialize a DetourCrowdAgent.
     */
    struct DetourCrowdAgentParameters : public Reference
    {
        GODOT_CLASS(DetourCrowdAgentParameters, Reference)

    public:
        /**
         * @brief Called when .new() is called in gdscript
         */
        void _init() {}

        static void _register_methods();

        Vector3 position;

        // These two parameters will determine into which navigation mesh & crowd this agent will be put.
        // Make sure your DetourNavigationMesh supports the radius & height.
        float   radius;
        float   height;

        float   maxAcceleration;
        float   maxSpeed;

        // The filter to use
        String filterName;

        // Check more in-depth descriptions of the optimizations here:
        // http://digestingduck.blogspot.com/2010/11/path-corridor-optimizations.html
        bool    anticipateTurns;    // If this agent should anticipate turns and move accordingly.
        bool    optimizeVisibility; // Optimize walked path based on visibility. Strongly recommended.
        bool    optimizeTopology;   // If shorter paths should be attempted under certain circumstances. Also recommended.

        bool    avoidObstacles;     // If this agent should try to avoid obstacles (dynamic obstacles).
        bool    avoidOtherAgents;   // If this agent should avoid other agents.
        int     obstacleAvoidance;  // How much this agent should avoid obstacles. 0 - 3, with 0 being low and 3 high avoidance.
        float   separationWeight;   // How strongly the other agents should try to avoid this agent (if they have avoidOtherAgents set).
    };

    /**
     * @brief A single agent in a crowd.
     */
    class DetourCrowdAgent : public Reference
    {
        GODOT_CLASS(DetourCrowdAgent, Reference)

    public:
        static void _register_methods();

        /**
         * @brief Constructor.
         */
        DetourCrowdAgent();

        /**
         * @brief Destructor.
         */
        ~DetourCrowdAgent();

        /**
         * @brief Called when .new() is called in gdscript
         */
        void _init() {}

        /**
         * @brief Sets this agent's main crowd agent.
         */
        void setMainAgent(dtCrowdAgent* crowdAgent, dtCrowd* crowd, int index, dtNavMeshQuery* query, DetourInputGeometry* geom);

        /**
         * @brief Sets the filter this agent will use.
         */
        void setFilter(int filterIndex);

        /**
         * @return Return the index of the filter.
         */
        int getFilterIndex();

        /**
         * @brief Adds the passed agent as a shadow agent that will be updated with the main agent's values regularly.
         */
        void addShadowAgent(dtCrowdAgent* crowdAgent);

        /**
         * @brief Create a debug representation of this obstacle and attach it to the passed node.
         */
        void createDebugMesh(Node* node);

        /**
         * @brief The agent will start moving as close as possible towards the passed position.
         */
        void moveTowards(Vector3 position);

        /**
         * @brief Will fill the passed vector with the current movement target, THEN RESET IT.
         * @return True if there was a new target.
         */
        void applyNewTarget();

        /**
         * @brief Stops moving entirely.
         */
        void stop();

        /**
         * @brief Will update the shadows with the current values from the primary crowd.
         */
        void update();

        /**
         * @brief Removes the agent from all crowds it is in and frees all associated memory.
         */
        void destroy();

    private:
        dtCrowdAgent*                   _agent;
        dtCrowd*                        _crowd;
        int                             _agentIndex;
        dtNavMeshQuery*                 _query;
        dtQueryFilter*                  _filter;
        int                             _filterIndex;
        DetourInputGeometry*            _inputGeom;
        std::vector<dtCrowdAgent*>      _shadows;

        Vector3             _position;
        Vector3             _velocity;
        Vector3             _targetPosition;
        std::atomic_bool    _hasNewTarget;

        bool    _isMoving;
    };
}

#endif // DETOURCROWDAGENT_H
