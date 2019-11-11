#include "godotdetourdebugdraw.h"
#include <SurfaceTool.hpp>
#include <Mesh.hpp>
#include <SpatialMaterial.hpp>
#include "navigationmeshhelpers.h"

using namespace godot;

Color
godotColorFromDetourColor(unsigned int input)
{
    float colorf[3];
    duIntToCol(input, colorf);
    unsigned int alpha = (input >> 24) & 0xff;
    return Color(colorf[0], colorf[1], colorf[2], alpha / 255.0f);
}

GodotDetourDebugDraw::GodotDetourDebugDraw()
{
    _surfaceTool = SurfaceTool::_new();

    // Create the material
    Ref<SpatialMaterial> mat = SpatialMaterial::_new();
    mat->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
    mat->set_flag(SpatialMaterial::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    mat->set_flag(SpatialMaterial::FLAG_USE_POINT_SIZE, true);
    mat->set_flag(SpatialMaterial::FLAG_DISABLE_AMBIENT_LIGHT, true);
    mat->set_flag(SpatialMaterial::FLAG_DONT_RECEIVE_SHADOWS, true);
    mat->set_feature(SpatialMaterial::FEATURE_TRANSPARENT, true);
    mat->set_cull_mode(SpatialMaterial::CULL_DISABLED);

    _material = mat;
}

GodotDetourDebugDraw::~GodotDetourDebugDraw()
{
    _material.unref();
    delete _surfaceTool;
}

void
GodotDetourDebugDraw::setMaterial(Ref<Material> material)
{
    _material = material;
}

void
GodotDetourDebugDraw::clear()
{
    _arrayMesh.unref();
}

void
GodotDetourDebugDraw::debugDrawBox(float minx, float miny, float minz, float maxx, float maxy, float maxz, unsigned int* fcol)
{
    begin(DU_DRAW_TRIS);

    const float verts[8*3] =
    {
        minx, miny, minz, // bbl
        maxx, miny, minz, // bbr
        maxx, miny, maxz, // bfr
        minx, miny, maxz, // bfl
        minx, maxy, minz, // tbl
        maxx, maxy, minz, // tbr
        maxx, maxy, maxz, // tfr
        minx, maxy, maxz  // tfl
    };
    static const unsigned char inds[6*6] =
    {
        // back
        5, 4, 0, 5, 0, 1,
        // front
        6, 2, 3, 6, 3, 7,
        // bottom
        3, 2, 1, 3, 1, 0,
        // top
        7, 4, 5, 7, 5, 6,
        // right
        6, 5, 2, 6, 2, 1,
        // left
        7, 3, 0, 7, 0, 4
    };

    const unsigned char* in = inds;
    for (int i = 0; i < 6; ++i)
    {
        vertex(&verts[*in*3], fcol[i]); in++;
        vertex(&verts[*in*3], fcol[i]); in++;
        vertex(&verts[*in*3], fcol[i]); in++;
        vertex(&verts[*in*3], fcol[i]); in++;
        vertex(&verts[*in*3], fcol[i]); in++;
        vertex(&verts[*in*3], fcol[i]); in++;
    }

    end();
}

unsigned int
GodotDetourDebugDraw::areaToCol(unsigned int area)
{
    switch(area)
    {
        // Ground (0) : light blue
        case POLY_AREA_GROUND: return duRGBA(0, 192, 255, 255);
        // Water : blue
        case POLY_AREA_WATER: return duRGBA(0, 0, 255, 255);
        // Road : brown
        case POLY_AREA_ROAD: return duRGBA(50, 20, 12, 255);
        // Door : cyan
        case POLY_AREA_DOOR: return duRGBA(0, 255, 255, 255);
        // Grass : green
        case POLY_AREA_GRASS: return duRGBA(0, 255, 0, 255);
        // Jump : yellow
        case POLY_AREA_JUMP: return duRGBA(255, 255, 0, 255);
        // Unexpected : red
        default: return duRGBA(255, 0, 0, 255);
    }
}

void
GodotDetourDebugDraw::depthMask(bool state)
{
    // Not needed as we are creating a mesh, not directly drawing to the output
}

void
GodotDetourDebugDraw::texture(bool state)
{
    // We always use a texture as we use the SurfaceTool, which requires some settings to be set via material
}

void
GodotDetourDebugDraw::begin(duDebugDrawPrimitives prim, float size)
{
    // Begin & set size if applicable
    switch (prim)
    {
        case DU_DRAW_POINTS:
            _surfaceTool->begin(Mesh::PRIMITIVE_POINTS);
            _material->set_point_size(size);
            break;
        case DU_DRAW_LINES:
            _surfaceTool->begin(Mesh::PRIMITIVE_LINES);
            _material->set_line_width(size);
            break;
        case DU_DRAW_TRIS:
            _surfaceTool->begin(Mesh::PRIMITIVE_TRIANGLES);
            break;
        case DU_DRAW_QUADS:
            WARN_PRINT("Trying to use primitive type quad. Not supported by Godot. Use triangle_strip instead - will look messy!");
            _surfaceTool->begin(Mesh::PRIMITIVE_TRIANGLE_STRIP);
            break;
    };

    // Set the material
    _surfaceTool->set_material(_material->duplicate(true));
}

void
GodotDetourDebugDraw::vertex(const float* pos, unsigned int color)
{
    _surfaceTool->add_color(godotColorFromDetourColor(color));
    _surfaceTool->add_vertex(Vector3(pos[0], pos[1], pos[2]));
}

void
GodotDetourDebugDraw::vertex(const float x, const float y, const float z, unsigned int color)
{
    _surfaceTool->add_color(godotColorFromDetourColor(color));
    _surfaceTool->add_vertex(Vector3(x, y, z));
}

void
GodotDetourDebugDraw::vertex(const float* pos, unsigned int color, const float* uv)
{
    _surfaceTool->add_color(godotColorFromDetourColor(color));
    _surfaceTool->add_uv(Vector2(uv[0], uv[1]));
    _surfaceTool->add_vertex(Vector3(pos[0], pos[1], pos[2]));
}

void
GodotDetourDebugDraw::vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v)
{
    _surfaceTool->add_color(godotColorFromDetourColor(color));
    _surfaceTool->add_uv(Vector2(u, v));
    _surfaceTool->add_vertex(Vector3(x, y, z));
}

void
GodotDetourDebugDraw::end()
{
    _arrayMesh = _surfaceTool->commit(_arrayMesh);
}
