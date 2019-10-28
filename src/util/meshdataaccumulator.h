#ifndef MESHDATAACCUMULATOR_H
#define MESHDATAACCUMULATOR_H

#include <vector>
#include <ArrayMesh.hpp>
#include <Transform.hpp>
using namespace godot;

/**
 * @brief Gets all the vertices, faces, etc. from an ArrayMesh and combines it into a single set of data (vertices, indices, ...).
 */
class MeshDataAccumulator
{
public:
    /**
     * @brief Constructor.
     */
    MeshDataAccumulator(Ref<ArrayMesh> arrayMesh, const Transform& transform);

    /**
     * @brief Destructor.
     */
    ~MeshDataAccumulator();

    /**
     * @brief Returns a pointer to all the vertices' floats.
     */
    const float* getVerts();

    /**
     * @brief Returns the number of vertices.
     */
    int getVertCount();

    /**
     * @brief Returns a pointer to all the triangles' indices.
     */
    const int* getTris();

    /**
     * @brief Returns the number of triangles.
     */
    int getTriCount();

    /**
     * @brief Returns a pointer to all the triangles' normals.
     */
    const float* getNormals();


private:
    std::vector<float>  _vertices;
    std::vector<int>    _triangles;
    std::vector<float>  _normals;
};

// ------------------------------------------------------------
// Inlines
inline const float*
MeshDataAccumulator::getVerts()
{
    return _vertices.data();
}

inline int
MeshDataAccumulator::getVertCount()
{
    return _vertices.size() / 3;
}

inline const int*
MeshDataAccumulator::getTris()
{
    return _triangles.data();
}

inline int
MeshDataAccumulator::getTriCount()
{
    return _triangles.size() / 3;
}

inline const float*
MeshDataAccumulator::getNormals()
{
    return _normals.data();
}

#endif // MESHDATAACCUMULATOR_H
