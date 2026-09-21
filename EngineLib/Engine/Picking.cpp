#include "Picking.h"
#include <algorithm>
#include <cfloat>

namespace
{
    bool RayIntersectsBounds(const FPickingRay& Ray, const FBoundingBox& Bounds)
    {
        const FVector direction = Ray.Far - Ray.Near;
        float tMin = 0.f;
        float tMax = 1.f;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (fabsf(direction[axis]) < 1e-6f)
            {
                if (Ray.Near[axis] < Bounds.Min[axis] || Ray.Near[axis] > Bounds.Max[axis]) return false;
                continue;
            }
            float t1 = (Bounds.Min[axis] - Ray.Near[axis]) / direction[axis];
            float t2 = (Bounds.Max[axis] - Ray.Near[axis]) / direction[axis];
            if (t1 > t2) std::swap(t1, t2);
            tMin = (std::max)(tMin, t1);
            tMax = (std::min)(tMax, t2);
            if (tMin > tMax) return false;
        }
        return true;
    }
}

bool MakeLocalPickingRay(const FPickingRay& Ray, const FMatrix& World,
    const FBoundingBox& LocalBounds, FPickingRay& OutLocalRay)
{
    if (!RayIntersectsBounds(Ray, LocalBounds.ToWorld(World))) return false;
    const FMatrix inverse = World.Inverse();
    if (inverse == FMatrix::Zero) return false;
    OutLocalRay = {inverse.TransformPosition(Ray.Near), inverse.TransformPosition(Ray.Far)};
    return RayIntersectsBounds(OutLocalRay, LocalBounds);
}

bool RayCastTriangles(const FPickingRay& Ray, std::span<const FVertexSimple> Vertices,
    std::span<const uint32> Indices, float& OutHitT)
{
    const FVector direction = Ray.Far - Ray.Near;
    float nearest = FLT_MAX;
    for (size_t i = 0; i + 2 < Indices.size(); i += 3)
    {
        const FVector v0 = Vertices[Indices[i]].GetPosition();
        const FVector e1 = Vertices[Indices[i + 1]].GetPosition() - v0;
        const FVector e2 = Vertices[Indices[i + 2]].GetPosition() - v0;
        const FVector p = FVector::cross(direction, e2);
        const float determinant = FVector::dot(e1, p);
        if (fabsf(determinant) < 1e-6f) continue;
        const float inverse = 1.f / determinant;
        const FVector t = Ray.Near - v0;
        const float u = FVector::dot(t, p) * inverse;
        if (u < 0.f || u > 1.f) continue;
        const FVector q = FVector::cross(t, e1);
        const float v = FVector::dot(direction, q) * inverse;
        if (v < 0.f || u + v > 1.f) continue;
        const float hitT = FVector::dot(e2, q) * inverse;
        if (hitT > 1e-6f && hitT <= 1.f && hitT < nearest) nearest = hitT;
    }
    if (nearest == FLT_MAX) return false;
    OutHitT = nearest;
    return true;
}
