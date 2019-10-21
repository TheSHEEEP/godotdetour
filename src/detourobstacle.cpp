#include "detourobstacle.h"
#include <CylinderMesh.hpp>
#include <QuadMesh.hpp>

using namespace godot;

void
DetourObstacle::_register_methods()
{
    register_method("move", &DetourObstacle::move);
    register_method("destroy", &DetourObstacle::destroy);
}

DetourObstacle::DetourObstacle()
    : _type(OBSTACLE_TYPE_INVALID)
    , _position(Vector3(0.0f, 0.0f, 0.0f))
    , _dimensions(Vector3(0.0f, 0.0f, 0.0f))
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
DetourObstacle::initialize(DetourObstacleType type, const Vector3& position, const Vector3& dimensions, float rotationRad)
{
    _type = type;
    _position = position;
    _dimensions = dimensions;
    _rotationRad = rotationRad;
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
