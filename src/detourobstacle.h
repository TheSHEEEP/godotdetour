#ifndef DETOUROBSTACLE_H
#define DETOUROBSTACLE_H

#include <Godot.hpp>
#include <map>

class dtTileCache;

// Obstacle types
enum ObstacleType
{
    OBSTACLE_TYPE_INVALID = -1,
    OBSTACLE_TYPE_CYLINDER,
    OBSTACLE_TYPE_BOX,
    NUM_OBSTACLE_TYPES
};

namespace godot
{
    /**
     * @brief A single obstacle. Can be moved around or destroyed.
     */
    class DetourObstacle : public Reference
    {
        GODOT_CLASS(DetourObstacle, Reference)

    public:
        static void _register_methods();

        /**
         * @brief Constructor.
         */
        DetourObstacle(ObstacleType type, const Vector3& position, const Vector3& dimensions);

        /**
         * @brief Destructor.
         */
        ~DetourObstacle();

        /**
         * @brief Pass the reference of this obstacle and the navmesh it is in.
         */
        void addReference(unsigned int ref, dtTileCache* cache);

        /**
         * @brief Move this obstacle to a new position.
         */
        void move(Vector3 position);

        /**
         * @brief Destroy this obstacle, removing it from all navmeshes.
         */
        void destroy();

        /**
         * @brief Create a debug representation of this obstacle and attach it to the passed node.
         */
        void createDebugMesh(Node* target);

    private:
        ObstacleType _type;

        Vector3 _position;
        Vector3 _dimensions; // In case of cylinder, x = radius, y = height, z = unused

        std::map<dtTileCache*, unsigned int> _references;
    };
}

#endif // DETOUROBSTACLE_H
