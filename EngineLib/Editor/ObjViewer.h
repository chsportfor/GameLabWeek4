#pragma once

#include <filesystem>
#include <d3d11.h>
#include <wrl/client.h>

#include "Core/Core.h"
#include "Core/Container/TArray.h"
#include "Core/Math/Rotator.h"
#include "Core/Math/Vector.h"

struct FEditorViewportClient;
class FFileManager;
class UAssetManager;
class URenderer;
class UTexture2D;
struct FStaticMesh;
struct FBoundingBox;
struct FRenderCollector;

class FObjViewer
{
public:
	FObjViewer(UAssetManager& AssetManager, URenderer& Renderer,
		FFileManager& FileManager, FEditorViewportClient& ViewportClient);
	~FObjViewer();

	void DrawControls();
	void UpdateControls(float DeltaTime, float PerspectiveRatio,
		bool bAllowMouseInput, bool bAllowKeyboardInput);
	void SubmitRenderInfos(FRenderCollector& Collector) const;
	bool LoadObjFile(const std::filesystem::path& FilePath);
	void Reset();

private:
	void UpdateModelControls(bool bAllowMouseInput);
	void OpenObjFileDialog();
	void ImportPreview();
	void ReleasePreview();
	void FrameCamera(const FBoundingBox& Bounds);

	UAssetManager& AssetManager;
	URenderer& Renderer;
	FFileManager& FileManager;
	FEditorViewportClient& ViewportClient;

	FStaticMesh* PreviewMesh = nullptr;
	Microsoft::WRL::ComPtr<ID3D11Buffer> PreviewVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> PreviewIndexBuffer;
	TArray<UTexture2D*> OwnedPreviewTextures;
	TArray<UTexture2D*> PreviewMaterialTextures;
	std::filesystem::path SourcePath;
	FString Path;
	FString Error;
	FString ImportedAssetName;
	uint32 VertexCount = 0;
	uint32 TriangleCount = 0;
	uint32 SectionCount = 0;
	uint32 MaterialCount = 0;
	bool bFlipTextureV = false;
	FRotator Rotation{0.0f, 0.0f, 0.0f};
	FVector Center{0.0f};
};
