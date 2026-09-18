#pragma once

#include "Core/enum.h"
#include <d3d11.h>
#include <wrl/client.h>
#include "Core/Container/TArray.h"
#include "Core/Math/Color.h"
#include "Core/Math/FBoundingBox.h"
#include "Core/Math/Transform.h"
#include "Core/Object/Object.h"

class UStaticMeshAsset;
class UTexture2DAsset;
class UFontAtlasAsset;
class FCamera;
class UPrimitiveComponent;
struct FTextMesh;
struct FSubUVMesh;
struct FRenderInfo
{
	TSharedPtr<UStaticMeshAsset> StaticMesh;
	TSharedPtr<UTexture2DAsset> Texture;
	TSharedPtr<UFontAtlasAsset> FontAtlas;
	EPrimitive ePrimitive;
	FMatrix WorldTransformMatrix;
	FObjectID ObejctID;
	FLinearColor Color;
	ERenderFlags eRenderFlags;

	const FTextMesh* Textmesh = nullptr;
	const FSubUVMesh* SubUVMesh = nullptr;

	// For particle rendering
	int32 numRows;
	int32 numCols;
	int32 currentFrame;
	int32 nextFrame;
	float frameRatio;

	// For billboard rendering

	FBoundingBox LocalBounds{};
	FBoundingBox WorldBounds{};

	EBlendStateType BlendStateType = EBlendStateType::BST_Default;

	// Return world matrix for billboard quads to face the camera
	// Get FRotator input because current camera rotation is stored in FRotator.
	// If camera stores rotation in FQuat, we can use FQuat to calculate billboard matrix.
	FMatrix GetTransformMatrix(const FRotator& cameraRotation) const
	{
		if (!HasAllRenderFlags(eRenderFlags, ERenderFlags::RF_Billboard))
		{
			return WorldTransformMatrix;
		}
		const FMatrix& world = WorldTransformMatrix;
		const FVector location = FVector(world.M[3][0], world.M[3][1], world.M[3][2]);
		//const FVector scale = {
		//	world.GetUnitAxis(EAxis::X).Length(),
		//	world.GetUnitAxis(EAxis::Y).Length(),
		//	world.GetUnitAxis(EAxis::Z).Length(),
		//};
		const FVector scale = FVector(1); // Billboard quad should not be scaled by world matrix, keep it uniform scale
		return FMatrix::Scale(scale) * FMatrix::Rotate(cameraRotation) * FMatrix::Translation(location);
	}

	FVector3 GetLocation() const
	{
		return FVector3(
			WorldTransformMatrix.M[3][0],
			WorldTransformMatrix.M[3][1],
			WorldTransformMatrix.M[3][2]
		);
	}

	FVector3 GetScale() const
	{
		return FVector3(
			WorldTransformMatrix.GetUnitAxis(EAxis::X).Length(),
			WorldTransformMatrix.GetUnitAxis(EAxis::Y).Length(),
			WorldTransformMatrix.GetUnitAxis(EAxis::Z).Length()
		);
	}
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
	TSharedPtr<UTexture2DAsset> Texture;
	FVector4 SubUV = { 0.f, 0.f, 1.f, 1.f };
	ERenderBlendMode BlendMode = ERenderBlendMode::Opaque;
	bool EnableDepthTest = true;
	bool EnableDepthWrite = true;
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

// Frame submissions. Quad producers do not select a rendering phase.
struct FRenderCollector
{
    enum { DEFAULT_RESERVE_MEM = 1024U };
    FCamera* Camera = nullptr;
    TArray<FRenderInfo> RenderInfos;
    TArray<FRenderLineInfo> LineInfos;
    TArray<FRenderQuadInfo> QuadInfos;
    TArray<UPrimitiveComponent*> PickTargets;

    void AddQuadInfo(const FRenderQuadInfo& Info) { QuadInfos.Add(Info); }
    void Clear()
    {
        RenderInfos.Reset();
        LineInfos.Reset();
        QuadInfos.Reset();
        PickTargets.Reset();
    }
};
