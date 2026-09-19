#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/Texture2DAsset.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/AssetSource/StaticMeshAssetSource.h"
#include "Core/AssetSystem/AssetSource/FileAssetSource.h"
#include "Engine/Assets/ObjImporter.h"
#include "Rendering/Renderer.h"

FStaticMeshAsset::FStaticMeshAsset(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount)
    : FStaticMeshAsset(InAssetName, InRenderer, InVertices, InVertexCount, nullptr, 0)
{
}

FStaticMeshAsset::FStaticMeshAsset(const FName& InAssetName, URenderer& InRenderer,
	const FVertexSimple* InVertices, uint32 InVertexCount, const uint32* InIndices, uint32 InIndexCount,
	const TArray<FStaticMeshAssetSection>& InSections,
	const TArray<FStaticMeshAssetMaterial>& InMaterials)
	: FAsset(InAssetName), Sections(InSections), Materials(InMaterials)
{
    BoundingBox = FBoundingBox(FVector(0), FVector(0));
    if (!InVertices || InVertexCount == 0) return;
    if (InIndexCount && !InIndices) throw std::invalid_argument("Missing mesh indices");
    for (uint32 I = 0; I < InIndexCount; ++I)
        if (InIndices[I] >= InVertexCount) throw std::out_of_range("Mesh index exceeds vertex count");
	for (const FStaticMeshAssetSection& Section : Sections)
	{
		if (Section.FirstIndex > InIndexCount || Section.IndexCount > InIndexCount - Section.FirstIndex)
			throw std::out_of_range("Mesh section exceeds index count");
		if (Section.MaterialIndex >= static_cast<uint32>(Materials.Num()))
			throw std::out_of_range("Mesh section exceeds material count");
	}
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
	if (Vertices.IsEmpty() || ParsedMesh.Indices.IsEmpty()) return nullptr;

	TArray<FStaticMeshAssetMaterial> Materials;
	Materials.Reserve(ParsedMesh.Materials.Num());
	auto TextureLoader = MakeShared<FTexture2DAssetLoader>(Renderer.GetDevice());
	for (const FStaticMaterial& ParsedMaterial : ParsedMesh.Materials)
	{
		FStaticMeshAssetMaterial Material;
		Material.Name = ParsedMaterial.Name;
		Material.DiffuseColor = {ParsedMaterial.DiffuseColor.x, ParsedMaterial.DiffuseColor.y,
			ParsedMaterial.DiffuseColor.z, ParsedMaterial.DiffuseColor.w};

		if (ParsedMaterial.DiffuseTexturePath.Len() > 0)
		{
			const std::string TexturePath(static_cast<std::string_view>(ParsedMaterial.DiffuseTexturePath));
			FString TextureAssetName("ObjViewer.Texture.");
			TextureAssetName.Append(std::string_view(TexturePath));
			const FName TextureName(TextureAssetName);
			AssetManager.RegisterAsset(TextureName, TextureLoader,
				MakeShared<FFileAssetSource>(Source.FileManager, std::filesystem::path(TexturePath)));
			Material.DiffuseTexture = AssetManager.GetAssetAs<FTexture2DAsset>(TextureName, true);
			if (!Material.DiffuseTexture) return nullptr;
		}

		Materials.Add(Material);
	}

	TArray<FStaticMeshAssetSection> Sections;
	Sections.Reserve(ParsedMesh.Sections.Num());
	for (const FStaticMeshSection& ParsedSection : ParsedMesh.Sections)
	{
		if (ParsedSection.MaterialIndex >= static_cast<uint32>(Materials.Num())) return nullptr;
		Sections.Add({ParsedSection.FirstIndex, ParsedSection.NumIndices, ParsedSection.MaterialIndex});
	}

	auto Asset = MakeShared<FStaticMeshAsset>(AssetName, Renderer, Vertices.GetData(),
		static_cast<uint32>(Vertices.Num()), ParsedMesh.Indices.GetData(),
		static_cast<uint32>(ParsedMesh.Indices.Num()), Sections, Materials);
	if (!Asset->GetVertexBuffer() || !Asset->GetIndexBuffer()) { return nullptr; }

	return Asset;
}
