#pragma once

#include "Matrix.h"
#include <cmath>

struct FBoundingBox
{
	FVector3 min;
	FVector3 max;
};

inline FBoundingBox TransformBoundingBox(const FBoundingBox& localBounds, const FMatrix& world)
{
	const FVector3 localCenter = (localBounds.min + localBounds.max) * 0.5f; 
	const FVector3 localExtent = (localBounds.max - localBounds.min) * 0.5f; 
	const FVector3 worldCenter = world.TransformPosition(localCenter);

	// 회전/비균등 스케일을 포함한 World AABB extent
	const FVector3 worldExtent(
		fabsf(world.M[0][0]) * localExtent.x +
		fabsf(world.M[1][0]) * localExtent.y +
		fabsf(world.M[2][0]) * localExtent.z,

		fabsf(world.M[0][1]) * localExtent.x +
		fabsf(world.M[1][1]) * localExtent.y +
		fabsf(world.M[2][1]) * localExtent.z,

		fabsf(world.M[0][2]) * localExtent.x +
		fabsf(world.M[1][2]) * localExtent.y +
		fabsf(world.M[2][2]) * localExtent.z
	);

	return { worldCenter - worldExtent, worldCenter + worldExtent };
}
