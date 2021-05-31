#ifndef MESHDATAACCUMULATOR_H
#define MESHDATAACCUMULATOR_H

#include <vector>
#include <ArrayMesh.hpp>
#include <Transform.hpp>
#include <MeshInstance.hpp>
#include <Godot.hpp>

namespace godot
{
    class File;
}

/**
 * @brief Gets all the vertices, faces, etc. from an ArrayMesh and combines it into a single set of data (vertices, indices, ...).
 */
class MeshDataAccumulator
{
public:
    /**
     * @brief Constructor.
     */
    MeshDataAccumulator(godot::MeshInstance* meshInstance);
    MeshDataAccumulator();

    /**
     * @brief Destructor.
     */
    ~MeshDataAccumulator();

    /**
     * @brief Returns a pointer to all the vertices' floats.
     */
    const float* getVerts() const;

    /**
     * @brief Returns the number of vertices.
     */
    int getVertCount() const;

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

    /**
     * @brief Store the mesh to the target file.
     */
    void save(godot::Ref<godot::File> targetFile);

    /**
     * @brief Load the mesh from the source file.
     */
    bool load(godot::Ref<godot::File> sourceFile);


private:
    std::vector<float>  _vertices;
    std::vector<int>    _triangles;
    std::vector<float>  _normals;
};

// ------------------------------------------------------------
// Inlines
inline const float*
MeshDataAccumulator::getVerts() const
{
    return _vertices.data();
}

inline int
MeshDataAccumulator::getVertCount() const
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
