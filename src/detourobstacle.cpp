#include "detourobstacle.h"
#include <CylinderMesh.hpp>
#include <QuadMesh.hpp>

using namespace godot;

void
DetourObstacle::_register_methods()
{
}

DetourObstacle::DetourObstacle(ObstacleType type, const Vector3& position, const Vector3& dimensions)
    : _type(type)
    , _position(position)
    , _dimensions(dimensions)
{

}

DetourObstacle::~DetourObstacle()
{
    if (!_references.empty())
    {
        ERR_PRINT("Destructor of obstacle was called with non-empty references!");
    }
}

void
DetourObstacle::addReference(unsigned int ref, dtTileCache* cache)
{
    _references[cache] = ref;
}

void
DetourObstacle::move(Vector3 position)
{
    // Iterate over all tile caches
    for (auto it = _references.begin(); it != _references.end(); ++it)
    {
        // Remove the obstacle

        // Add the obstacle again at a new position
    }
}

void
DetourObstacle::destroy()
{
    // Iterate over all tile caches
    for (auto it = _references.begin(); it != _references.end(); ++it)
    {
        // Remove the obstacle
    }
    _references.clear();
}

void
DetourObstacle::createDebugMesh(Node* target)
{

}
