#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/IO/FileManager.h"
#include "Rendering/Renderer.h"
#include "Core/Object/ObjectFactory.h"

IMPLEMENT_CLASS(UStaticMeshAsset, UAsset);

void UStaticMeshAsset::Initialize(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount)
{
    Initialize(InAssetName, InRenderer, InVertices, InVertexCount, nullptr, 0);
}

void UStaticMeshAsset::Initialize(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount, const uint32* InIndices, uint32 InIndexCount)
{
    SetName(InAssetName);
    VertexCount = 0;
    IndexCount = 0;
    VertexBuffer.Reset();
    IndexBuffer.Reset();
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

UAsset* FStaticMeshAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	FFileAssetSource& FileSource = static_cast<FFileAssetSource&>(AssetSource);
	FString FileContent = FileSource.ReadFileToString();
	// TODO: do something...

	const FVertexSimple* InVertices = nullptr;
	const uint32* InIndices = nullptr;
	uint32 vertexCount = 0;
	uint32 indexCount = 0;

	return FObjectFactory::ConstructObject<UStaticMeshAsset>(AssetName, Renderer, InVertices, vertexCount, InIndices, indexCount);
}

void FStaticMeshAssetLoader::UnloadAsset(UAsset* Asset)
{
	//TODO: do something with StrongPtr...
}

