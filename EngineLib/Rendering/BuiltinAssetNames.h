#pragma once

#include "Core/Name.h"
#include "Core/enum.h"

// Names only: assets are stored exclusively in FAssetManager.
namespace BuiltinAssetNames
{
    inline FName Mesh(EPrimitive Primitive)
    {
        switch (Primitive)
        {
        case EPrimitive::EP_Cube: return "Mesh.Cube";
        case EPrimitive::EP_Sphere: return "Mesh.Sphere";
        case EPrimitive::EP_GizmoArrow: return "Mesh.GizmoArrow";
        case EPrimitive::EP_Circle: return "Mesh.Circle";
        case EPrimitive::EP_Triangle: return "Mesh.Triangle";
        case EPrimitive::EP_BillboardQuad: return "Mesh.Quad";
        default: return FName();
        }
    }

    inline FName Texture(EPrimitive Primitive)
    {
        switch (Primitive)
        {
        case EPrimitive::EP_Cube: return "Texture.Cube";
        case EPrimitive::EP_Sphere: return "Texture.Earth";
        case EPrimitive::EP_BillboardQuad: return "Texture.Explosion";
        default: return FName();
        }
    }

    inline constexpr const char* DefaultFont = "Font.Korean";
    inline constexpr const char* FullscreenMesh = "Mesh.Fullscreen";
    inline constexpr const char* LoadingScreen = "Texture.LoadingScreen";
}
