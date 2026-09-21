#pragma once

#include <filesystem>

#include "Core/Core.h"
#include "Core/Math/Rotator.h"
#include "Core/Math/Vector.h"

struct FEditorViewportClient;
class FFileManager;
class UAssetManager;
class URenderer;
class UStaticMeshAsset;
struct FBoundingBox;
struct FRenderCollector;

class FObjViewer
{
public:
	FObjViewer(UAssetManager& AssetManager, URenderer& Renderer,
		FFileManager& FileManager, FEditorViewportClient& ViewportClient);

	void DrawControls();
	void UpdateModelControls(bool bViewportHovered);
	void SubmitRenderInfos(FRenderCollector& Collector) const;
	bool LoadObjFile(const std::filesystem::path& FilePath);
	void Reset();

private:
	void OpenObjFileDialog();
	void FrameCamera(const FBoundingBox& Bounds);

	UAssetManager& AssetManager;
	URenderer& Renderer;
	FFileManager& FileManager;
	FEditorViewportClient& ViewportClient;

	UStaticMeshAsset* Mesh = nullptr;
	FString Path;
	FString Error;
	uint32 VertexCount = 0;
	uint32 TriangleCount = 0;
	uint32 SectionCount = 0;
	uint32 MaterialCount = 0;
	FRotator Rotation{0.0f, 0.0f, 0.0f};
	FVector Center{0.0f};
};
