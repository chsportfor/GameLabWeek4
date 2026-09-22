#pragma once

#include "Core/Container/TArray.h"
#include "Rendering/VertexType.h"

struct FMeshSection
{
    uint32 FirstIndex = 0;
    uint32 IndexCount = 0;
    uint32 MaterialIndex = 0;
};

struct FMeshGeometry
{
    TArray<FVertexSimple> Vertices;
    TArray<uint32> Indices;
};
