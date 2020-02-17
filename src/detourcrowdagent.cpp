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
    register_property<DetourCrowdAgent, Vector3>("target", &DetourCrowdAgent::_targetPosition, Vector3(0.0f, 0.0f, 0.0f));
    register_property<DetourCrowdAgent, bool>("isMoving", &DetourCrowdAgent::_isMoving, false);

    register_signal<DetourCrowdAgent>("arrived_at_target", "node", Variant::OBJECT);
    register_signal<DetourCrowdAgent>("no_progress", "node", Variant::OBJECT, "distanceLeft", Variant::REAL);
    register_signal<DetourCrowdAgent>("no_movement", "node", Variant::OBJECT);
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
    , _state(AGENT_STATE_INVALID)
    , _lastDistanceToTarget(0.0f)
    , _distanceTotal(0.0f)
{
    _hasNewTarget = false;
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
    targetFile->store_16(_state);

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
        _state = (DetourCrowdAgentState)sourceFile->get_16();
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
    _state = AGENT_STATE_IDLE;
    _distanceTotal = 0.0f;
    _lastDistanceToTarget = 0.0f;
    _distanceTime = 0.0f;
    _movementOverTime = 0.0f;
    _movementTime = 0.0f;
    _lastPosition = Vector3(_agent->npos[0], _agent->npos[1], _agent->npos[2]);
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
    _distanceTotal = 0.0f;
    _lastDistanceToTarget = 0.0f;
    _movementTime = 0.0f;
    _movementOverTime = 0.0f;
    _lastPosition = _position;
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
    _state = AGENT_STATE_GOING_TO_TARGET;
}

void
DetourCrowdAgent::stop()
{
    // Stop all movement
    _crowd->resetMoveTarget(_agentIndex);
    _hasNewTarget = false;
    _isMoving = false;
    _state = AGENT_STATE_IDLE;
    _distanceTotal = 0.0f;
    _lastDistanceToTarget = 0.0f;
    _distanceTime = 0.0f;
    _movementTime = 0.0f;
    _movementOverTime = 0.0f;
}

void
DetourCrowdAgent::update(float secondsSinceLastTick)
{
    // Update all the shadows with the main agent's values
    for (int i = 0; i < _shadows.size(); ++i)
    {
        _shadows[i]->npos[0] = _agent->npos[0];
        _shadows[i]->npos[1] = _agent->npos[1];
        _shadows[i]->npos[2] = _agent->npos[2];
    }

    // Various state-dependent calculations
    switch(_state)
    {
        case AGENT_STATE_GOING_TO_TARGET:
        {
            // Update the values available to GDScript
            _position.x = _agent->npos[0];
            _position.y = _agent->npos[1];
            _position.z = _agent->npos[2];
            _velocity.x = _agent->vel[0];
            _velocity.y = _agent->vel[1];
            _velocity.z = _agent->vel[2];


            // Get distance to target and other statistics
            float distanceToTarget = _targetPosition.distance_to(_position);
            _distanceTime += secondsSinceLastTick;
            _distanceTotal += fabs(_lastDistanceToTarget - distanceToTarget);

            _movementOverTime += _position.distance_squared_to(_lastPosition);
            _lastPosition = _position;
            _movementTime += secondsSinceLastTick;

            // If the agent has not moved noticeably in a while, report that
            if (_movementTime >= 1.0f)
            {
                _movementTime -= 1.0f;
                if (_movementOverTime < (_agent->params.maxSpeed * 0.01f))
                {
                    emit_signal("no_movement", this, distanceToTarget);
                }
                _movementOverTime = 0.0f;
            }

            // If we haven't made enough progress in a second, tell the user
            if (_distanceTime >= 5.0f)
            {
                _distanceTime -= 5.0f;
                if (_distanceTotal < (_agent->params.maxSpeed * 0.03f))
                {
                    emit_signal("no_progress", this, distanceToTarget);
                }
                _distanceTotal = 0.0f;
            }

            // Mark moving or not
            if (_velocity.distance_to(Vector3(0.0f, 0.0f, 0.0f)) > 0.001f)
            {
                _isMoving = true;
            }
            else
            {
                _isMoving = false;
            }

            // Arrived?
            if (distanceToTarget < 0.1f)
            {
                _isMoving = false;
                _crowd->resetMoveTarget(_agentIndex);
                _state = AGENT_STATE_IDLE;
                _distanceTotal = 0.0f;
                _lastDistanceToTarget = 0.0f;
                _distanceTime = 0.0f;
                _movementTime = 0.0f;
                _movementOverTime = 0.0f;
                emit_signal("arrived_at_target", this);
            }
            _lastDistanceToTarget = distanceToTarget;
            break;
        }
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
    _distanceTotal = 0.0f;
    _lastDistanceToTarget = 0.0f;
    _distanceTime = 0.0f;
    _movementTime = 0.0f;
    _movementOverTime = 0.0f;
}
