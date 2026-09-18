#pragma once

#include "Camera.h"
#include "Core/Math/Frustum.h"

// Immutable during submission/drawing: every pass sees the same camera and viewport.
struct FRenderView
{
    FCamera Camera;
    FMatrix View = FMatrix::Identity;
    FMatrix Projection = FMatrix::Identity;
    FMatrix ViewProjection = FMatrix::Identity;
    FMatrix Projection2D = FMatrix::Identity;
    FVector2 ViewportSize{1, 1};
    FFrustum Frustum{};
    float PerspectiveRatio = 1.f;
};
