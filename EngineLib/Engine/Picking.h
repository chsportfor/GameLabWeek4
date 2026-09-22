#pragma once

#include "Core/Container/TArray.h"
#include "Core/Object/WeakObjectPtr.h"
#include "Core/Math/FBoundingBox.h"
#include "Rendering/VertexType.h"
#include <span>

class UPrimitiveComponent;
using FPickTargets = TArray<TWeakObjectPtr<const UPrimitiveComponent>>;

// HitT is the fraction along Near -> Far (0..1), even after transforming to local space.
struct FPickingRay
{
    FVector Near{0};
    FVector Far{0};
};

bool RayIntersectsBounds(const FPickingRay& Ray, const FBoundingBox& Bounds);
bool MakeLocalPickingRay(const FPickingRay& Ray, const FMatrix& World,
    const FBoundingBox& LocalBounds, FPickingRay& OutLocalRay);
bool RayCastTriangles(const FPickingRay& Ray, std::span<const FVertexSimple> Vertices,
    std::span<const uint32> Indices, float& OutHitT);
