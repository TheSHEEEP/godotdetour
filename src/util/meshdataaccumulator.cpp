#include "meshdataaccumulator.h"

#include <MeshDataTool.hpp>
#include <PoolArrays.hpp>
#include <Mesh.hpp>
#include <Material.hpp>
#include "godotgeometryparser.h"

using namespace godot;

MeshDataAccumulator::MeshDataAccumulator(MeshInstance* meshInstance)
{
    GodotGeometryParser parser;
    parser.getNodeVerticesAndIndices(meshInstance, _vertices, _triangles);

    Godot::print("Got vertices and triangles...");

    // Copy normals (we can't just copy them from the MeshDataTool since we operate on transformed values)
    // Code below mostly taken from recastnavigation sample
    size_t indexCount = _triangles.size();
    _normals.resize(indexCount);
    for (int j = 0; j < indexCount; j += 3)
    {
        const float* v0 = &_vertices[_triangles[j + 0] * 3];
        const float* v1 = &_vertices[_triangles[j + 1] * 3];
        const float* v2 = &_vertices[_triangles[j + 2] * 3];
        float e0[3], e1[3];
        for (int k = 0; k < 3; ++k)
        {
            e0[k] = v1[k] - v0[k];
            e1[k] = v2[k] - v0[k];
        }
        float* n = &_normals.data()[j];
        n[0] = e0[1]*e1[2] - e0[2]*e1[1];
        n[1] = e0[2]*e1[0] - e0[0]*e1[2];
        n[2] = e0[0]*e1[1] - e0[1]*e1[0];
        float d = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
        if (d > 0)
        {
            d = 1.0f/d;
            n[0] *= d;
            n[1] *= d;
            n[2] *= d;
        }
    }
    Godot::print("Got normals...");
}

MeshDataAccumulator::~MeshDataAccumulator()
{

}
