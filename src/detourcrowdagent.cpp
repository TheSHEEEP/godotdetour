#include "detourcrowdagent.h"
#include <Node.hpp>

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
}

DetourCrowdAgent::DetourCrowdAgent()
{

}

DetourCrowdAgent::~DetourCrowdAgent()
{

}

void
DetourCrowdAgent::setMainAgent(dtCrowdAgent* crowdAgent)
{
    _agent = crowdAgent;
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

}

void
DetourCrowdAgent::stop()
{
    // Stop all movement
}

void
DetourCrowdAgent::update()
{
    // Update all the shadows with the main agent's values
}

void
DetourCrowdAgent::destroy()
{
    // Remove the main agent and all shadows from their navmeshes
    // In contrast to obstacles, agents really shouldn't be removed during the thread update, so this has to be done thread safe
}
