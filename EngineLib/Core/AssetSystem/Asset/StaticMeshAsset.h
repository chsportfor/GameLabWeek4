#pragma once

#include "Core/Core.h"
#include "Core/AssetSystem/Asset.h"
#include "Core/Container/TArray.h"
#include "Core/Math/Vector.h"
#include "Core/Math/FBoundingBox.h"
#include "Rendering/VertexType.h"
#include <d3d11.h>
#include <wrl/client.h>

class URenderer;

class FStaticMeshAsset : public FAsset
{
    DECLARE_ASSET_TYPE(FStaticMeshAsset)

public:
    FStaticMeshAsset(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount);
	FStaticMeshAsset(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount, const uint32* InIndices, uint32 InIndexCount);

	inline Microsoft::WRL::ComPtr<ID3D11Buffer> GetVertexBuffer() const { return VertexBuffer; }
	inline uint32 GetVertexCount() const { return VertexCount; }
	inline Microsoft::WRL::ComPtr<ID3D11Buffer> GetIndexBuffer() const { return IndexBuffer; }
	inline uint32 GetIndexCount() const { return IndexCount; }
	inline const FBoundingBox& GetLocalBoundingBox() const { return BoundingBox; }

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
	uint32 VertexCount = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
	uint32 IndexCount = 0;

	FBoundingBox BoundingBox;
};

class FStaticMeshAssetLoader_Primitive : public FAssetLoader {
public:
	FStaticMeshAssetLoader_Primitive(URenderer& InRenderer) : Renderer(InRenderer) {}

	virtual TSharedPtr<FAsset> LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;

private:
	URenderer& Renderer;
};

class FStaticMeshAssetLoader_File : public FAssetLoader {
public:
	FStaticMeshAssetLoader_File(URenderer& InRenderer) : Renderer(InRenderer) {}

	virtual TSharedPtr<FAsset> LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;

private:
	URenderer& Renderer;
};
