#pragma once

#include "Core/enum.h"
#include <d3d11.h>
#include <wrl/client.h>
#include "Core/Container/TArray.h"
#include "Core/Math/Color.h"
#include "Core/Math/FBoundingBox.h"
#include "RenderView.h"
#include "Core/Math/Transform.h"
#include "Core/Object/Object.h"

class FStaticMeshAsset;
class FTexture2DAsset;
class FFontAtlasAsset;
class FCamera;
class AActor;
struct FTextMesh;
class FAssetManager;

// Draw payloads contain only data consumed by their pipeline.
struct FRenderMeshInfo
{
    TSharedPtr<FStaticMeshAsset> StaticMesh;
    TSharedPtr<FTexture2DAsset> Texture;
    FMatrix WorldTransformMatrix = FMatrix::Identity;
    FLinearColor Color{1, 1, 1, 0};
    FVector2 UVScale{1, 1};
    FVector2 UVOffset{0, 0};
    uint32 FirstIndex = 0;
    uint32 IndexCount = 0;
};

struct FRenderFullscreenInfo
{
    TSharedPtr<FStaticMeshAsset> StaticMesh;
    TSharedPtr<FTexture2DAsset> Texture;
};

struct FRenderTextInfo
{
    const FTextMesh* Textmesh = nullptr;
    TSharedPtr<FFontAtlasAsset> FontAtlas;
    FVector Location{0};
    FVector Scale{1};
    FLinearColor Color{1, 1, 1, 1};
};

// CPU selection/bounds metadata; never passed to a graphics pipeline.
struct FPickInfo
{
    EPrimitive Primitive{};
    FObjectID ObjectID{};
    FMatrix WorldTransformMatrix = FMatrix::Identity;
    FBoundingBox LocalBounds{};
    FBoundingBox WorldBounds{};
};

enum class ERenderBlendMode
{
	Opaque,
	Masked,
	Transparent,
	Additive,
	NoColorWrite,
	Count
};

enum class EQuadRenderPhase { Opaque, Transparent, Overlay };

struct FRenderQuadInfo
{
	FMatrix Model;
	FVector4 Color = { 1.f, 1.f, 1.f, 1.f };
	TSharedPtr<FTexture2DAsset> Texture;
	FVector4 SubUV = { 0.f, 0.f, 1.f, 1.f };
	ERenderBlendMode BlendMode = ERenderBlendMode::Opaque;
	bool EnableDepthTest = true;
	bool EnableDepthWrite = true;
	// UV rectangles: offset.xy, size.zw. Zero blend preserves a single-frame quad.
	FVector4 NextSubUV = { 0.f, 0.f, 1.f, 1.f };
	float FrameBlend = 0.f;
	D3D11_TEXTURE_ADDRESS_MODE AddressMode = D3D11_TEXTURE_ADDRESS_WRAP;
};

struct FRenderLineInfo
{
	FVector4 Color;
	FVector3 Start;
	float Thickness;
	FVector3 End;
	float Padding;
};

struct FRenderLine2DInfo
{
    FVector2 Start, End;
    FVector4 Color;
    float Thickness = 1.f;
};

struct FRenderCircle2DInfo
{
    FVector2 Center;
    FVector4 Color;
    float Radius = 1.f;
};

struct FRenderTriangle2DInfo
{
    FVector2 Center;
    FVector4 Color;
    float Size = 1.f;
    float Rotation = 0.f;
};

struct FRenderWorldAxisInfo
{
    FVector4 Color;
    FVector Axis;
    float Thickness = 0.002f;
};

struct FRenderWorldGridInfo
{
    float GridGap = 1.f;
};

// Components select the destination array. Update and submission are separate.
struct FRenderCollector
{
    FRenderView View;
    const AActor* SelectedActor = nullptr;
    FAssetManager* AssetManager = nullptr;
    uint32 ShowFlags = ~0u;
    TArray<FRenderMeshInfo> MeshInfos;
    TArray<FRenderMeshInfo> InstancedMeshInfos;
    TArray<FRenderMeshInfo> GizmoInfos;
    TArray<FRenderTextInfo> TextInfos;
    TArray<FRenderQuadInfo> QuadInfos;
    TArray<FRenderLineInfo> LineInfos;
    TArray<FRenderMeshInfo> SelectionInfos;
    TArray<FRenderWorldAxisInfo> WorldAxisInfos;
    TArray<FRenderWorldGridInfo> WorldGridInfos;

    bool IsVisible(const FBoundingBox& Bounds) const { return View.Frustum.Intersects(Bounds); }
    bool HasShowFlag(EEngineShowFlags Flag) const { return (ShowFlags & static_cast<uint32>(Flag)) != 0; }
    void AddBounds(const FBoundingBox& Bounds)
    {
        Bounds.ForEachCornerLines([&](const FVector& Start, const FVector& End)
        {
            LineInfos.Add({FVector4(1, 1, 1, 1), Start, 1.f, End, 0});
        });
    }
    void Clear()
    {
        MeshInfos.Reset(); InstancedMeshInfos.Reset(); GizmoInfos.Reset();
        TextInfos.Reset(); QuadInfos.Reset(); LineInfos.Reset();
        SelectionInfos.Reset(); WorldAxisInfos.Reset(); WorldGridInfos.Reset();
    }
};
