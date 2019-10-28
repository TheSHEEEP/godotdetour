#include "meshdataaccumulator.h"

#include <MeshDataTool.hpp>
#include <PoolArrays.hpp>
#include <Mesh.hpp>
#include <Material.hpp>

using namespace godot;

MeshDataAccumulator::MeshDataAccumulator(Ref<ArrayMesh> arrayMesh, const Transform& transform)
{
    Godot::print(String("Accumulating surfaces from ArrayMesh"));

    // Sanity checks
    if (arrayMesh->get_faces().size() <= 0)
    {
        ERR_PRINT("Passed ArrayMesh does not have any faces!");
        return;
    }

    for (int64_t i = 0; i < arrayMesh->get_surface_count(); ++i)
    {
        if (arrayMesh->surface_get_primitive_type(i) != Mesh::PRIMITIVE_TRIANGLES)
        {
            ERR_PRINT(String("ArrayMesh surface %d does not have primitive type PRIMITIVE_TRIANGLES. Skipping.").format(Array::make(i)));
            continue;
        }

        Godot::print(String("Surface {0} ****** 0").format(Array::make(i)));

        // Get the vertices (can't use MeshDataTool due to it crashing on incomplete meshes)
        Array arrays = arrayMesh->surface_get_arrays(i);
        if (arrays.empty())
        {
            ERR_PRINT("Empty surface arrays");
            continue;
        }
        PoolVector3Array vertexArray = arrays[Mesh::ARRAY_VERTEX];
        if (vertexArray.size() == 0)
        {
            ERR_PRINT("No vertices in the array");
            continue;
        }
        PoolIntArray indexArray = arrays[Mesh::ARRAY_INDEX];
        if (indexArray.size() == 0)
        {
            WARN_PRINT("No indices in the array, creating our own now, assuming simple [0,1,2][3,4,5][6,7,8] order...");
            if (vertexArray.size() % 3 != 0)
            {
                ERR_PRINT(String("Unable to create own indices, vertex count not a multiple of 3: {0}").format(Array::make(vertexArray.size())));
                continue;
            }
            indexArray = PoolIntArray();
            indexArray.write();
            indexArray.resize(vertexArray.size());
            for (int j = 0; j < vertexArray.size(); ++j)
            {
                indexArray.set(0, j);
            }
            Godot::print("Done creating indices");
        }

        PoolVector3Array::Read vertexArrayRead = vertexArray.read();
        PoolIntArray::Read indexArrayRead = indexArray.read();
        Godot::print("Got read access to data...");

        // Copy vertices
        size_t currentNumVertexFloats = _vertices.size();
        int64_t vertexCount = vertexArray.size();
        _vertices.resize(currentNumVertexFloats + vertexCount*3);
        for (int64_t j = 0; j < vertexCount; ++j)
        {
            Vector3 vertex = transform.xform(vertexArrayRead[j]);
            _vertices[currentNumVertexFloats + j*3 + 0] = vertex.x;
            _vertices[currentNumVertexFloats + j*3 + 1] = vertex.y;
            _vertices[currentNumVertexFloats + j*3 + 2] = vertex.z;
        }
        Godot::print("Got vertices...");

        // TODO: here, fix this
        // Copy triangles
        size_t currentNumTriangleInts = _triangles.size();
        int64_t indexCount = indexArray.size();
        _triangles.resize(currentNumTriangleInts + indexCount);
        for (int64_t j = 0; j < indexCount; ++j)
        {
            _triangles[currentNumTriangleInts + j*3 + 0] = currentNumTriangleInts + indexArrayRead[j*3 + 0];
            _triangles[currentNumTriangleInts + j*3 + 1] = currentNumTriangleInts + indexArrayRead[j*3 + 1];
            _triangles[currentNumTriangleInts + j*3 + 2] = currentNumTriangleInts + indexArrayRead[j*3 + 2];
        }
        Godot::print("Got triangles...");

        // Copy normals (we can't just copy them from the MeshDataTool since we operate on transformed values)
        // Code below mostly taken from recastnavigation sample
        size_t currentNumNormals = _normals.size();
        _normals.resize(currentNumNormals + indexCount);
        for (int j = 0; j < indexCount*3; i += 3)
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
        Godot::print("Got normals...");
    }

    Godot::print("Accumulation done.");
}

MeshDataAccumulator::~MeshDataAccumulator()
{

}
