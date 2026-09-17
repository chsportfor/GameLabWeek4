#pragma once

#include "Core/enum.h"
#include "Core/Math/Color.h"
#include "Core/Math/FBoundingBox.h"
#include "Core/Math/Transform.h"
#include "Core/Object/Object.h"

struct FTextMesh;
struct FSubUVMesh;
struct FRenderInfo
{
	EPrimitive ePrimitive;
	FMatrix WorldTransformMatrix;
	FObjectID ObejctID;
	FLinearColor Color;
	ERenderFlags eRenderFlags;

	const FTextMesh* Textmesh;
	const FSubUVMesh* SubUVMesh;

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
