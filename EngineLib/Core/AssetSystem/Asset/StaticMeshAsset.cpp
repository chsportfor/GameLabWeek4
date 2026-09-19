#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/AssetSource/StaticMeshAssetSource.h"
#include "Core/AssetSystem/AssetSource/FileAssetSource.h"
#include "Engine/Assets/ObjImporter.h"
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

TSharedPtr<FAsset> FStaticMeshAssetLoader_Primitive::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
    const auto& Source = static_cast<const FStaticMeshAssetSource&>(AssetSource);
    if (Source.Vertices.empty() || Source.Indices.empty()) return nullptr;
    auto Asset = MakeShared<FStaticMeshAsset>(AssetName, Renderer, Source.Vertices.data(), static_cast<uint32>(Source.Vertices.size()),
        Source.Indices.data(), static_cast<uint32>(Source.Indices.size()));
    if (!Asset->GetVertexBuffer() || !Asset->GetIndexBuffer()) return nullptr;
    return Asset;
}

TSharedPtr<FAsset> FStaticMeshAssetLoader_File::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	const auto& Source = static_cast<const FFileAssetSource&>(AssetSource);

	FStaticMesh ParsedMesh;
	FString ParseError;
	const std::string FilePath = Source.FilePath.string();
	if (!FObjImporter::LoadFromFile(FilePath, Source.FileManager, ParsedMesh, ParseError))
	{
		OutputDebugStringA(ParseError.CStr());
		return nullptr;
	}

	TArray<FVertexSimple> Vertices;
	Vertices.Reserve(ParsedMesh.Vertices.Num());

	for (const FVertexPNCT& Vertex : ParsedMesh.Vertices)
	{
		Vertices.Add({Vertex.Position.x,Vertex.Position.y,Vertex.Position.z,
			Vertex.Normal.x,Vertex.Normal.y,Vertex.Normal.z,
			Vertex.Color.x,Vertex.Color.y,Vertex.Color.z,Vertex.Color.w,
			Vertex.UV.x,Vertex.UV.y
			});
	}
	if (Vertices.IsEmpty() || ParsedMesh.Indices.IsEmpty()){return nullptr;}
	auto Asset = MakeShared<FStaticMeshAsset>(AssetName,Renderer,Vertices.GetData(),static_cast<uint32>(Vertices.Num()),
		ParsedMesh.Indices.GetData(),static_cast<uint32>(ParsedMesh.Indices.Num()));
	if (!Asset->GetVertexBuffer() || !Asset->GetIndexBuffer()) { return nullptr; }

	return Asset;
}
