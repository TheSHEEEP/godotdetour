#include "detourcrowdagent.h"
#include <Node.hpp>
#include <DetourCrowd.h>
#include <DetourNavMeshQuery.h>
#include "util/detourinputgeometry.h"

using namespace godot;

#define AGENT_SAVE_VERSION 1

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
    , _crowdIndex(0)
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

bool
DetourCrowdAgent::save(Ref<File> targetFile)
{
    // Sanity check
    if (!_agent)
    {
        ERR_PRINT("AgentSave: No detour agent present!");
        return false;
    }

    // Version
    targetFile->store_16(AGENT_SAVE_VERSION);

    // Properties
    targetFile->store_32(_agentIndex);
    targetFile->store_32(_crowdIndex);
    targetFile->store_32(_filterIndex);
    targetFile->store_var(_position);
    targetFile->store_var(_velocity);
    targetFile->store_var(_targetPosition);
    targetFile->store_8(_hasNewTarget.load());
    targetFile->store_8(_isMoving);

    // Parameter values
    targetFile->store_float(_agent->params.radius);
    targetFile->store_float(_agent->params.height);
    targetFile->store_float(_agent->params.maxAcceleration);
    targetFile->store_float(_agent->params.maxSpeed);
    targetFile->store_32(_agent->params.updateFlags);
    targetFile->store_8(_agent->params.obstacleAvoidanceType);
    targetFile->store_float(_agent->params.separationWeight);

    return true;
}

bool
DetourCrowdAgent::load(Ref<File> sourceFile)
{
    // Version
    int version = sourceFile->get_16();

    if (version == AGENT_SAVE_VERSION)
    {
        _agentIndex = sourceFile->get_32();
        _crowdIndex = sourceFile->get_32();
        _filterIndex = sourceFile->get_32();
        _position = sourceFile->get_var(true);
        _velocity = sourceFile->get_var(true);
        _targetPosition= sourceFile->get_var(true);
        _hasNewTarget = sourceFile->get_8();
        _isMoving = sourceFile->get_8();
    }
    else
    {
        ERR_PRINT(String("Unable to load agent. Unknown save version: {0}").format(Array::make(version)));
        return false;
    }

    return true;
}

bool
DetourCrowdAgent::loadParameterValues(Ref<DetourCrowdAgentParameters> params, Ref<File> sourceFile)
{
    params->radius = sourceFile->get_float();
    params->height = sourceFile->get_float();
    params->maxAcceleration = sourceFile->get_float();
    params->maxSpeed = sourceFile->get_float();
    params->position = _position;
    int updateFlags = sourceFile->get_32();
    params->anticipateTurns = updateFlags & DT_CROWD_ANTICIPATE_TURNS;
    params->optimizeVisibility = updateFlags & DT_CROWD_OPTIMIZE_VIS;
    params->optimizeTopology = updateFlags & DT_CROWD_OPTIMIZE_TOPO;
    params->avoidObstacles = updateFlags & DT_CROWD_OBSTACLE_AVOIDANCE;
    params->avoidOtherAgents = updateFlags & DT_CROWD_SEPARATION;
    params->obstacleAvoidance = sourceFile->get_8();
    params->separationWeight = sourceFile->get_float();

    return true;
}

void
DetourCrowdAgent::setMainAgent(dtCrowdAgent* crowdAgent, dtCrowd* crowd, int index, dtNavMeshQuery* query, DetourInputGeometry* geom, int crowdIndex)
{
    _agent = crowdAgent;
    _crowd = crowd;
    _agentIndex = index;
    _crowdIndex = crowdIndex;
    _query = query;
    _inputGeom = geom;
}

void
DetourCrowdAgent::setFilter(int filterIndex)
{
    _filter = _crowd->getEditableFilter(filterIndex);
    _filterIndex = filterIndex;
}

void
DetourCrowdAgent::addShadowAgent(dtCrowdAgent* crowdAgent)
{
    _shadows.push_back(crowdAgent);
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
    float pos[3];
    pos[0] = _targetPosition.x;
    pos[1] = _targetPosition.y;
    pos[2] = _targetPosition.z;
    float extents[3];
    extents[0] = halfExtents[0] * 1.0f;
    extents[1] = halfExtents[1] * 1.0f;
    extents[2] = halfExtents[2] * 1.0f;

    // Do the query to find the nearest poly & point
    dtStatus status = _query->findNearestPoly(pos, extents, _filter, &targetRef, finalTargetPos);
    if (dtStatusFailed(status))
    {
        _hasNewTarget = true;
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
