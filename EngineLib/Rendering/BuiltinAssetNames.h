#pragma once

#include "Core/Name.h"
#include "Core/enum.h"

// Names only: assets are stored exclusively in UAssetManager.
namespace BuiltinAssetNames
{
    inline FName Mesh(EPrimitive Primitive)
    {
        switch (Primitive)
        {
        case EPrimitive::EP_Cube: return "/Engine/Primitives/Cube";
        case EPrimitive::EP_Sphere: return "/Engine/Primitives/Sphere";
        case EPrimitive::EP_GizmoArrow: return "/Engine/Primitives/GizmoArrow";
        case EPrimitive::EP_Circle: return "/Engine/Primitives/Circle";
        case EPrimitive::EP_Triangle: return "/Engine/Primitives/Triangle";
        case EPrimitive::EP_BillboardQuad: return "/Engine/Primitives/Quad";
        default: return FName();
        }
    }

    inline FName Texture(EPrimitive Primitive)
    {
        switch (Primitive)
        {
        case EPrimitive::EP_Cube: return "Textures/CubeTextureSample.dds";
        case EPrimitive::EP_Sphere: return "Textures/EarthTexture.dds";
        case EPrimitive::EP_BillboardQuad: return "Textures/Explosion_Alpha.dds";
        default: return FName();
        }
    }

    inline constexpr const char* DefaultFont = "Fonts/KoreanFullAtlas.json";
    inline constexpr const char* FullscreenMesh = "/Engine/Primitives/Fullscreen";
    inline constexpr const char* LoadingScreen = "Textures/LoadingScreen.dds";
}
