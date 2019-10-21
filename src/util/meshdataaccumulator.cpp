#include "meshdataaccumulator.h"

#include <MeshDataTool.hpp>
using namespace godot;

MeshDataAccumulator::MeshDataAccumulator(Ref<ArrayMesh> arrayMesh, const Transform& transform)
{
    Godot::print("Accumulating surfaces from ArrayMesh.");
    for (int64_t i = 0; i < arrayMesh->get_surface_count(); ++i)
    {
        MeshDataTool tool;
        tool.create_from_surface(arrayMesh, i);

        // Copy vertices
        size_t currentNumVertices = _vertices.size();
        int64_t vertexCount = tool.get_vertex_count();
        _vertices.resize(currentNumVertices + vertexCount*3);
        for (int64_t j = 0; j < vertexCount; ++j)
        {
            Vector3& vertex = transform.xform(tool.get_vertex(j));
            _vertices[currentNumVertices + j*3 + 0] = vertex.x;
            _vertices[currentNumVertices + j*3 + 1] = vertex.y;
            _vertices[currentNumVertices + j*3 + 2] = vertex.z;
        }

        // Copy triangles
        size_t currentNumTriangles = _triangles.size();
        int64_t faceCount = tool.get_face_count();
        _triangles.resize(currentNumTriangles + faceCount*3);
        for (int64_t j = 0; j < faceCount; ++j)
        {
            _triangles[currentNumTriangles + j*3 + 0] = tool.get_face_vertex(j, 0);
            _triangles[currentNumTriangles + j*3 + 1] = tool.get_face_vertex(j, 1);
            _triangles[currentNumTriangles + j*3 + 2] = tool.get_face_vertex(j, 2);
        }

        // Copy normals (we can't just copy them from the MeshDataTool since we operate on transformed values)
        // Code below mostly taken from recastnavigation sample
        size_t currentNumNormals = _normals.size();
        _normals.resize(currentNumNormals + faceCount);
        for (int j = 0; j < faceCount*3; i += 3)
        {
            const float* v0 = &_vertices.data()[_triangles[i]*3];
            const float* v1 = &_vertices.data()[_triangles[i+1]*3];
            const float* v2 = &_vertices.data()[_triangles[i+2]*3];
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
    }

    Godot::print("Accumulation done.");
}

MeshDataAccumulator::~MeshDataAccumulator()
{

}
