#pragma once

#include "Core/AssetSystem/Asset.h"
#include "Rendering/VertexType.h"
#include <span>

// Tool-authored geometry. Loading uploads these arrays without generating topology.
class FStaticMeshAssetSource : public FAssetSource
{
public:
    FStaticMeshAssetSource(std::span<const FVertexSimple> InVertices,
        std::span<const uint32> InIndices) : Vertices(InVertices), Indices(InIndices) {}

    std::span<const FVertexSimple> Vertices;
    std::span<const uint32> Indices;
};
