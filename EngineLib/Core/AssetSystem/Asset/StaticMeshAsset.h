#pragma once

#include "Core/Core.h"
#include "Core/AssetSystem/Asset.h"
#include "Core/AssetSystem/AssetSource/FileAssetSource.h"
#include "Texture2DAsset.h"
#include "Core/Container/TArray.h"
#include "Core/Math/Vector.h"
#include "Core/Math/FBoundingBox.h"
#include "Rendering/VertexType.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
//#include <ft2build.h>
//#include FT_FREETYPE_H

class FFileManager;
class FFontManager;
class URenderer;

class UStaticMeshAsset : public UAsset
{
	DECLARE_OBJECT(UStaticMeshAsset, UAsset)
public:
	void Initialize(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount);
	void Initialize(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount, const uint32* InIndices, uint32 InIndexCount);

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

class FStaticMeshAssetLoader : public FAssetLoader {
public:
	FStaticMeshAssetLoader(URenderer& InRenderer) : Renderer(InRenderer) {}

	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;
	virtual void UnloadAsset(UAsset* Asset) override;
private:
	URenderer& Renderer;
};
