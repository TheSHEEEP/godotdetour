#include "detourcrowdagent.h"

using namespace godot;

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