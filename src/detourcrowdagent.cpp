#include "detourcrowdagent.h"
#include <Node.hpp>
#include <DetourCrowd.h>
#include <DetourNavMeshQuery.h>
#include "util/detourinputgeometry.h"

using namespace godot;

void
DetourCrowdAgentParameters::_register_methods()
{
    register_property<DetourCrowdAgentParameters, Vector3>("position", &DetourCrowdAgentParameters::position, Vector3(0.0f, 0.0f, 0.0f));
    register_property<DetourCrowdAgentParameters, float>("radius", &DetourCrowdAgentParameters::radius, 0.0f);
    register_property<DetourCrowdAgentParameters, float>("height", &DetourCrowdAgentParameters::height, 0.0f);
    register_property<DetourCrowdAgentParameters, float>("maxAcceleration", &DetourCrowdAgentParameters::maxAcceleration, 0.0f);
    register_property<DetourCrowdAgentParameters, float>("maxSpeed", &DetourCrowdAgentParameters::maxSpeed, 0.0f);
    register_property<DetourCrowdAgentParameters, String>("filterName", &DetourCrowdAgentParameters::filterName, "default");
    register_property<DetourCrowdAgentParameters, bool>("anticipateTurns", &DetourCrowdAgentParameters::anticipateTurns, true);
    register_property<DetourCrowdAgentParameters, bool>("optimizeVisibility", &DetourCrowdAgentParameters::optimizeVisibility, true);
    register_property<DetourCrowdAgentParameters, bool>("optimizeTopology", &DetourCrowdAgentParameters::optimizeTopology, true);
    register_property<DetourCrowdAgentParameters, bool>("avoidObstacles", &DetourCrowdAgentParameters::avoidObstacles, true);
    register_property<DetourCrowdAgentParameters, bool>("avoidOtherAgents", &DetourCrowdAgentParameters::avoidOtherAgents, true);
    register_property<DetourCrowdAgentParameters, int>("obstacleAvoidance", &DetourCrowdAgentParameters::obstacleAvoidance, 0);
    register_property<DetourCrowdAgentParameters, float>("separationWeight", &DetourCrowdAgentParameters::separationWeight, 0.0f);
}

void
DetourCrowdAgent::_register_methods()
{
    register_method("createDebugMesh", &DetourCrowdAgent::createDebugMesh);
    register_method("moveTowards", &DetourCrowdAgent::moveTowards);
    register_method("stop", &DetourCrowdAgent::stop);

    register_property<DetourCrowdAgent, Vector3>("position", &DetourCrowdAgent::_position, Vector3(0.0f, 0.0f, 0.0f));
    register_property<DetourCrowdAgent, Vector3>("velocity", &DetourCrowdAgent::_velocity, Vector3(0.0f, 0.0f, 0.0f));
    register_property<DetourCrowdAgent, bool>("isMoving", &DetourCrowdAgent::_isMoving, false);
}

DetourCrowdAgent::DetourCrowdAgent()
    : _agent(nullptr)
    , _crowd(nullptr)
    , _agentIndex(0)
    , _query(nullptr)
    , _filter(nullptr)
    , _filterIndex(0)
    , _inputGeom(nullptr)
    , _isMoving(false)
{

}

DetourCrowdAgent::~DetourCrowdAgent()
{

}

void
DetourCrowdAgent::setMainAgent(dtCrowdAgent* crowdAgent, dtCrowd* crowd, int index, dtNavMeshQuery* query, DetourInputGeometry* geom)
{
    _agent = crowdAgent;
    _crowd = crowd;
    _agentIndex = index;
    _query = query;
    _inputGeom = geom;
}

void
DetourCrowdAgent::setFilter(int filterIndex)
{
    _filter = _crowd->getEditableFilter(filterIndex);
    _filterIndex = filterIndex;
}

int
DetourCrowdAgent::getFilterIndex()
{
    return _filterIndex;
}

void
DetourCrowdAgent::addShadowAgent(dtCrowdAgent* crowdAgent)
{
    _shadows.push_back(crowdAgent);
}

void
DetourCrowdAgent::createDebugMesh(Node* node)
{

}

void
DetourCrowdAgent::moveTowards(Vector3 position)
{
    _targetPosition = position;
    _hasNewTarget = true;
}

void
DetourCrowdAgent::applyNewTarget()
{
    if (!_hasNewTarget)
    {
        return;
    }
    _hasNewTarget = false;

    // Get the final target position and poly reference
    // TODO: Optimization, create new method to assign same target to multiple agents at once (only one findNearestPoly per agent)
    dtPolyRef targetRef;
    float finalTargetPos[3];
    const float* halfExtents = _crowd->getQueryExtents();
    finalTargetPos[0] = 0.0f;
    finalTargetPos[1] = 0.0f;
    finalTargetPos[2] = 0.0f;

    // Shoot another query in the input geometry to get a better position
    float rayStart[3];
    float rayEnd[3];
    float hitTime = 0.0f;
    rayStart[0] = rayEnd[0] = _targetPosition.x;
    rayStart[1] = rayEnd[1] = _targetPosition.y;
    rayStart[2] = rayEnd[2] = _targetPosition.z;
    rayStart[1] -= 0.4f;
    rayEnd[1] += 0.4f;
    bool hit = _inputGeom->raycastMesh(rayStart, rayEnd, hitTime);
    if (!hit)
    {
        // Try again
        for (int i = 0; i < 10; ++i)
        {
            rayStart[1] -= 0.4f;
            rayEnd[1] += 0.4f;
            hit = _inputGeom->raycastMesh(rayStart, rayEnd, hitTime);
            if (hit)
            {
                break;
            }
        }
        if (!hit)
        {
            ERR_PRINT(String("Input geometry couldn't do raycast: {0} {1} {2} {3} {4} {5}").format(Array::make(rayStart[0], rayStart[1], rayStart[2], rayEnd[0], rayEnd[1], rayEnd[2])));
            return;
        }
    }
    // TODO: Find out (ask on mailing list/github?) why some higher plateaus are part of bottom layer
    float pos[3];
    pos[0] = rayStart[0] + (rayEnd[0] - rayStart[0]) * hitTime;
    pos[1] = rayStart[1] + (rayEnd[1] - rayStart[1]) * hitTime;
    pos[2] = rayStart[2] + (rayEnd[2] - rayStart[2]) * hitTime;
    float extents[3];
    extents[0] = halfExtents[0] * 1.0f;
    extents[1] = halfExtents[1] * 1.0f;
    extents[2] = halfExtents[2] * 1.0f;

    // Do the query to find the nearest poly & point
    dtStatus status = _query->findNearestPoly(pos, extents, _filter, &targetRef, finalTargetPos);
    if (dtStatusFailed(status))
    {
        ERR_PRINT(String("applyNewTarget: findPoly failed: {0}").format(Array::make(status)));
        return;
    }

    // Set the movement target
    if (!_crowd->requestMoveTarget(_agentIndex, targetRef, finalTargetPos))
    {
        ERR_PRINT("Unable to request detour move target.");
    }
}

void
DetourCrowdAgent::stop()
{
    // Stop all movement
    _crowd->resetMoveTarget(_agentIndex);
    _hasNewTarget = false;
    _isMoving = false;
}

void
DetourCrowdAgent::update()
{
    // Update all the shadows with the main agent's values
    for (int i = 0; i < _shadows.size(); ++i)
    {
        _shadows[i]->npos[0] = _agent->npos[0];
        _shadows[i]->npos[1] = _agent->npos[1];
        _shadows[i]->npos[2] = _agent->npos[2];
    }

    // Update the values available to GDScript
    _position.x = _agent->npos[0];
    _position.y = _agent->npos[1];
    _position.z = _agent->npos[2];
    _velocity.x = _agent->vel[0];
    _velocity.y = _agent->vel[1];
    _velocity.z = _agent->vel[2];

    if (_velocity.distance_to(Vector3(0.0f, 0.0f, 0.0f)) > 0.001f)
    {
        _isMoving = true;
    }
    else
    {
        _isMoving = false;
    }
}

void
DetourCrowdAgent::destroy()
{
    // In contrast to obstacles, agents really shouldn't be removed during the thread update, so this has to be done thread safe

    // Simply setting an agent's active value to false should remove it from all detour calculations
    _agent->active = false;
    for (int i = 0; i < _shadows.size(); ++i)
    {
        _shadows[i]->active = false;
    }
    _shadows.clear();
    _agent = nullptr;
    _isMoving = false;
}
