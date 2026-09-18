#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/AssetSource/StaticMeshAssetSource.h"
#include "Rendering/Renderer.h"

FStaticMeshAsset::FStaticMeshAsset(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount)
    : FStaticMeshAsset(InAssetName, InRenderer, InVertices, InVertexCount, nullptr, 0)
{
}

FStaticMeshAsset::FStaticMeshAsset(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount, const uint32* InIndices, uint32 InIndexCount)
    : FAsset(InAssetName)
{
    BoundingBox = FBoundingBox(FVector(0), FVector(0));
    if (!InVertices || InVertexCount == 0) return;
    if (InIndexCount && !InIndices) throw std::invalid_argument("Missing mesh indices");
    for (uint32 I = 0; I < InIndexCount; ++I)
        if (InIndices[I] >= InVertexCount) throw std::out_of_range("Mesh index exceeds vertex count");
    VertexBuffer = InRenderer.CreateVertexBuffer(InVertices, InVertexCount);
    IndexBuffer = InRenderer.CreateIndexBuffer(InIndices, InIndexCount);
    VertexCount = VertexBuffer ? InVertexCount : 0;
    IndexCount = IndexBuffer ? InIndexCount : 0;
    BoundingBox = FBoundingBox(InVertices[0].GetPosition(), InVertices[0].GetPosition());
    for (uint32 I = 1; I < InVertexCount; ++I)
        BoundingBox.ExpandToInclude(InVertices[I].GetPosition());
}

TSharedPtr<FAsset> FStaticMeshAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
    const auto& Source = static_cast<const FStaticMeshAssetSource&>(AssetSource);
    if (Source.Vertices.empty() || Source.Indices.empty()) return nullptr;
    auto Asset = MakeShared<FStaticMeshAsset>(AssetName, Renderer, Source.Vertices.data(), static_cast<uint32>(Source.Vertices.size()),
        Source.Indices.data(), static_cast<uint32>(Source.Indices.size()));
    if (!Asset->GetVertexBuffer() || !Asset->GetIndexBuffer()) return nullptr;
    return Asset;
}
