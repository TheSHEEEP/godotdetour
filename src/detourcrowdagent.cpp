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
    register_property<DetourCrowdAgentParameters, bool>("anticipateTurns", &DetourCrowdAgentParameters::anticipateTurns, true);
    register_property<DetourCrowdAgentParameters, bool>("optimizeVisibility", &DetourCrowdAgentParameters::optimizeVisibility, true);
    register_property<DetourCrowdAgentParameters, bool>("optimizeTopology", &DetourCrowdAgentParameters::optimizeTopology, true);
    register_property<DetourCrowdAgentParameters, bool>("avoidObstacles", &DetourCrowdAgentParameters::avoidObstacles, true);
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

}

void
DetourCrowdAgent::update()
{

}
