/*************************************************************************/
/*  navigation_mesh_generator.h                                          */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef GDGEOMETRYPARSER_H
#define GDGEOMETRYPARSER_H

#include <NavigationMesh.hpp>
#include <Godot.hpp>
#include <Transform.hpp>
#include <Vector2.hpp>
#include <ArrayMesh.hpp>
#include <PoolArrays.hpp>
#include <vector>

using namespace godot;
class GodotGeometryParser {

private:
    void _add_vertex(const Vector3& p_vec3, std::vector<float>& _verticies);
    void _add_mesh(const Ref<ArrayMesh>& p_mesh, const Transform& p_xform, std::vector<float>& p_verticies, std::vector<int>& p_indices);
    void _add_faces(const PoolVector3Array& p_faces, const Transform& p_xform, std::vector<float>& p_verticies, std::vector<int>& p_indices);
    void _parse_geometry(Node *p_node, std::vector<float>& p_verticies, std::vector<int>& p_indices);

public:
    GodotGeometryParser();
    ~GodotGeometryParser();

    void getNodeVerticesAndIndices(Node* node, std::vector<float>& outVertices, std::vector<int>& outIndices);
};

#endif // GDGEOMETRYPARSER_H
