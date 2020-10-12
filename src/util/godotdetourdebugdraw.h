#ifndef GODOTDETOURDEBUGDRAW_H
#define GODOTDETOURDEBUGDRAW_H

#include "DebugDraw.h"
#include <Ref.hpp>
#include <ArrayMesh.hpp>
#include <Material.hpp>
#include <SpatialMaterial.hpp>
#include <SurfaceTool.hpp>

class GodotDetourDebugDraw : public duDebugDraw
{
public:
    /**
     * @brief Constructor.
     */
    GodotDetourDebugDraw();

    /**
     * @brief Destructor.
     */
    ~GodotDetourDebugDraw();

    /**
     * @brief Sets the material to use.
     */
    void setMaterial(godot::Ref<godot::Material> material);

    /**
     * @brief Returns the ArrayMesh constructed so far.
     */
    godot::Ref<godot::ArrayMesh> getArrayMesh();

    /**
     * @brief Clear all the mesh data constructed so far.
     */
    void clear();

    /**
     * @brief Custom box drawing function as Godot does not support PRIMITVE_TYPE_QUAD.
     */
    void debugDrawBox(float minx, float miny, float minz, float maxx, float maxy, float maxz, unsigned int* fcol);

    // Interface overwrites
    virtual unsigned int areaToCol(unsigned int area);
    virtual void depthMask(bool state);
    virtual void texture(bool state);
    virtual void begin(duDebugDrawPrimitives prim, float size = 1.0f);
    virtual void vertex(const float* pos, unsigned int color);
    virtual void vertex(const float x, const float y, const float z, unsigned int color);
    virtual void vertex(const float* pos, unsigned int color, const float* uv);
    virtual void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v);
    virtual void end();

private:
    godot::Ref<godot::SurfaceTool>      _surfaceTool;
    godot::Ref<godot::SpatialMaterial>  _material;
    godot::Ref<godot::ArrayMesh>        _arrayMesh;
};

//// INLINES
inline godot::Ref<godot::ArrayMesh>
GodotDetourDebugDraw::getArrayMesh()
{
    return _arrayMesh;
}

#endif // GODOTDETOURDEBUGDRAW_H
