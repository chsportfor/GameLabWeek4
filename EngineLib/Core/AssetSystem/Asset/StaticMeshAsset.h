#pragma once

#include "Core/Core.h"
#include "Core/AssetSystem/Asset.h"
#include "Core/Container/TArray.h"
#include "Core/Math/Color.h"
#include "Core/Math/Vector.h"
#include "Core/Math/FBoundingBox.h"
#include "Rendering/VertexType.h"
#include <d3d11.h>
#include <wrl/client.h>

class URenderer;
class FAssetManager;
class FTexture2DAsset;

struct FStaticMeshAssetSection
{
	uint32 FirstIndex = 0;
	uint32 IndexCount = 0;
	uint32 MaterialIndex = 0;
};

struct FStaticMeshAssetMaterial
{
	FString Name;
	FLinearColor DiffuseColor{1.f, 1.f, 1.f, 1.f};
	TSharedPtr<FTexture2DAsset> DiffuseTexture;
};

class FStaticMeshAsset : public FAsset
{
    DECLARE_ASSET_TYPE(FStaticMeshAsset)

public:
    FStaticMeshAsset(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount);
	FStaticMeshAsset(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices,
		uint32 InVertexCount, const uint32* InIndices, uint32 InIndexCount,
		const TArray<FStaticMeshAssetSection>& InSections = {},
		const TArray<FStaticMeshAssetMaterial>& InMaterials = {});

	inline Microsoft::WRL::ComPtr<ID3D11Buffer> GetVertexBuffer() const { return VertexBuffer; }
	inline uint32 GetVertexCount() const { return VertexCount; }
	inline Microsoft::WRL::ComPtr<ID3D11Buffer> GetIndexBuffer() const { return IndexBuffer; }
	inline uint32 GetIndexCount() const { return IndexCount; }
	inline const FBoundingBox& GetLocalBoundingBox() const { return BoundingBox; }
	inline const TArray<FStaticMeshAssetSection>& GetSections() const { return Sections; }
	inline const TArray<FStaticMeshAssetMaterial>& GetMaterials() const { return Materials; }

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
	uint32 VertexCount = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
	uint32 IndexCount = 0;

	FBoundingBox BoundingBox;
	TArray<FStaticMeshAssetSection> Sections;
	TArray<FStaticMeshAssetMaterial> Materials;
};

class FStaticMeshAssetLoader_Primitive : public FAssetLoader {
	DECLARE_ASSET_TYPE(FStaticMeshAsset)
public:
	FStaticMeshAssetLoader_Primitive(URenderer& InRenderer) : Renderer(InRenderer) {}

	virtual TSharedPtr<FAsset> LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;

private:
	URenderer& Renderer;
};

class FStaticMeshAssetLoader_File : public FAssetLoader {
	DECLARE_ASSET_TYPE(FStaticMeshAsset)
public:
	FStaticMeshAssetLoader_File(URenderer& InRenderer, FAssetManager& InAssetManager)
		: Renderer(InRenderer), AssetManager(InAssetManager) {}

	virtual TSharedPtr<FAsset> LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;

private:
	URenderer& Renderer;
	FAssetManager& AssetManager;
};
