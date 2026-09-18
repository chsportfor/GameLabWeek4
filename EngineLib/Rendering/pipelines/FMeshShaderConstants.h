#pragma once

#include "Core/Math/Matrix.h"

// Shared by every pass that uses Mesh.hlsl.
struct FMeshShaderConstants
{
    FMatrix Model;
    FVector4 Color;
    int32 UseVertexColor;
    int32 HasTexture;
    int32 Padding[2]{};
    FVector2 UVScale{1, 1};
    FVector2 UVOffset{0, 0};
};
static_assert(sizeof(FMeshShaderConstants) == 112);
