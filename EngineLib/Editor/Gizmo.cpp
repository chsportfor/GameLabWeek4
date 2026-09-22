#include "Gizmo.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Rendering/BuiltinAssetNames.h"

#include "Core/Math/Color.h"
#include "Engine/Actor.h"
#include <cmath>

void FGizmo::SetWorldMode(bool bInWorldMode)
{
    if (bWorldMode == bInWorldMode) return;
    EndDrag();
    bWorldMode = bInWorldMode;
}

FVector FGizmo::AxisDirection(EGIZMO_AXIS axis) const
{
    // WEEK3: scaling always uses local axes, regardless of the selected translation/rotation space.
    const bool bLocal = !bWorldMode || eType == SCALE;
    const FMatrix Rotation = bLocal ? FMatrix::Rotate(UpdateRotation) : FMatrix::Identity;
    switch (axis)
    {
    case X: return Rotation.GetUnitAxis(EAxis::X);
    case Y: return Rotation.GetUnitAxis(EAxis::Y);
    case Z: return Rotation.GetUnitAxis(EAxis::Z);
    default: return FVector(0.f);
    }
}

float FGizmo::WrapAngle180(float degree)
{
	degree = FMath::Fmod(degree + 180.0f, 360.0f);
	if (degree < 0.0f) degree += 360.0f;

	return degree - 180.0f;
}

bool FGizmo::GetRingPlaneHit(
	const FVector& nearPoint,
	const FVector& farPoint,
	const FVector& planeOrigin,
	const FVector& axisDir,
	FVector& outPoint) const
{
	FVector norm_ray = farPoint - nearPoint;
	norm_ray.Normalize();

	const float dn = FVector::dot(norm_ray, axisDir);
	if (FMath::Abs(dn) < 1e-4f) return false;   // 링을 모서리로 보는 각도

	const float t = FVector::dot(planeOrigin - nearPoint, axisDir) / dn;
	if (t < 0.0f) return false;                 // 카메라 뒤쪽

	outPoint = nearPoint + norm_ray * t;

	return true;
}

namespace
{
    bool ProjectGizmoPoint(const FVector& Point, const FVector2& Size, const FMatrix& ViewProjection,
        FVector2& Screen)
    {
        if (Size.x <= 0 || Size.y <= 0) return false;
        const FVector Clip = ViewProjection.TransformPosition(Point);
        const float W = Point.x * ViewProjection.M[0][3] + Point.y * ViewProjection.M[1][3]
            + Point.z * ViewProjection.M[2][3] + ViewProjection.M[3][3];
        if (!std::isfinite(W) || W <= SMALL_NUMBER || Clip.z < 0 || Clip.z > W) return false;
        Screen = FVector2((Clip.x / W + 1.f) * .5f * Size.x, (1.f - Clip.y / W) * .5f * Size.y);
        return std::isfinite(Screen.x) && std::isfinite(Screen.y);
    }
}

bool FGizmo::GetScreenAxis(EGIZMO_AXIS Axis, const FVector2& ViewportSize, const FMatrix& ViewProjection,
    FVector2& Start, FVector2& End) const
{
    const float Length = mAxisLength * mGizmoScale;
    if (Length <= SMALL_NUMBER) return false;
    if (!ProjectGizmoPoint(mLocation, ViewportSize, ViewProjection, Start) ||
        !ProjectGizmoPoint(mLocation + AxisDirection(Axis) * Length, ViewportSize, ViewProjection, End)) return false;
    return (End - Start).LengthSquared() >= .01f;
}

bool FGizmo::GetProjectedAxisParameter(const FVector2& MousePosition, const FVector2& ViewportSize,
    const FMatrix& InverseViewProjection, float& Parameter) const
{
    if (ViewportSize.x <= 0 || ViewportSize.y <= 0) return false;
    // WEEK3: project the cursor onto the frozen screen axis before unprojecting the ray.
    const float Distance = FVector2::dot(MousePosition - mDragScreenStart, mDragScreenDirection);
    const FVector2 Projected(mDragScreenStart.x + mDragScreenDirection.x * Distance,
        mDragScreenStart.y + mDragScreenDirection.y * Distance);
    const float X = Projected.x * 2.f / ViewportSize.x - 1.f;
    const float Y = 1.f - Projected.y * 2.f / ViewportSize.y;
    const FMatrix& Inverse = InverseViewProjection;
    auto Unproject = [&](float Z, FVector& Point)
    {
        const float W = X * Inverse.M[0][3] + Y * Inverse.M[1][3] + Z * Inverse.M[2][3] + Inverse.M[3][3];
        if (!std::isfinite(W) || FMath::Abs(W) <= SMALL_NUMBER) return false;
        Point = Inverse.TransformPosition(FVector(X, Y, Z)) * (1.f / W);
        return std::isfinite(Point.x) && std::isfinite(Point.y) && std::isfinite(Point.z);
    };
    FVector Near, Far;
    if (!Unproject(0.f, Near) || !Unproject(1.f, Far)) return false;
    FVector Direction = Far - Near;
    if (Direction.IsNearlyZero()) return false;
    Direction.Normalize();
    const FVector W = mDragStartGizmoLocation - Near;
    const float A = FVector::dot(mDragAxisDirection, mDragAxisDirection);
    const float B = FVector::dot(mDragAxisDirection, Direction);
    const float C = FVector::dot(Direction, Direction);
    const float D = FVector::dot(mDragAxisDirection, W);
    const float E = FVector::dot(Direction, W);
    const float Denominator = A * C - B * B;
    if (FMath::Abs(Denominator) <= KINDA_SMALL_NUMBER) return false;
    Parameter = (B * E - C * D) / Denominator;
    return std::isfinite(Parameter);
}

bool FGizmo::BeginDrag(const FVector& nearPoint, const FVector& farPoint, const FTransform& ActorTransform,
    const FVector2& MousePosition, const FVector2& ViewportSize, const FMatrix& ViewProjection,
    const FMatrix& InverseViewProjection)
{
    mDraggingAxis = eAxis;
    if (mDraggingAxis == NONE) return false;
    mDragStartTransform = ActorTransform;
    mDragStartGizmoLocation = mLocation;
    mDragAxisDirection = AxisDirection(mDraggingAxis);
    mDragStartAxisS = 0.f;
    mPreviousMousePosition = MousePosition;
    if (eType != ROTATE)
    {
        FVector2 End;
        if (!GetScreenAxis(mDraggingAxis, ViewportSize, ViewProjection, mDragScreenStart, End))
        {
            mDraggingAxis = NONE;
            return false;
        }
        mDragScreenDirection = End - mDragScreenStart;
        mDragScreenDirection.Normalize();
        if (eType == TRANSLATE && !GetProjectedAxisParameter(MousePosition, ViewportSize, InverseViewProjection, mDragStartAxisS))
        {
            mDraggingAxis = NONE;
            return false;
        }
        return true;
    }

    // Rotation keeps the existing ring-plane algorithm.
    mDragStartRingDir = FVector(0.f);
    mDragAccumAngle = 0.f;
    mDragLastAngle = 0.f;
    FVector RingHit;
    if (GetRingPlaneHit(nearPoint, farPoint, mDragStartGizmoLocation, mDragAxisDirection, RingHit))
    {
        FVector RingDir = RingHit - mDragStartGizmoLocation;
        if (RingDir.Length() > SMALL_NUMBER)
        {
            RingDir.Normalize();
            mDragStartRingDir = RingDir;
        }
    }
    return true;
}

bool FGizmo::GetDragLocation(const FVector2& MousePosition, const FVector2& ViewportSize,
    const FMatrix& InverseViewProjection, FVector& outLocation) const
{
    if (mDraggingAxis == NONE || eType != TRANSLATE) return false;
    float Parameter;
    if (!GetProjectedAxisParameter(MousePosition, ViewportSize, InverseViewProjection, Parameter)) return false;
    outLocation = mDragStartTransform.Location + mDragAxisDirection * (Parameter - mDragStartAxisS);
    return true;
}

bool FGizmo::GetDragScale(const FVector2& MousePosition, const FVector& CurrentScale, FVector& outScale)
{
    if (mDraggingAxis == NONE || eType != SCALE) return false;
    const float Amount = FVector2::dot(MousePosition - mPreviousMousePosition, mDragScreenDirection) * .01f;
    if (!std::isfinite(Amount)) return false;
    mPreviousMousePosition = MousePosition;
    outScale = CurrentScale;
    // Drawing uses the rotated local axis; applying scale changes only its corresponding component.
    switch (mDraggingAxis)
    {
    case X: outScale.x = FMath::Max(CurrentScale.x + Amount, MIN_SCALE); break;
    case Y: outScale.y = FMath::Max(CurrentScale.y + Amount, MIN_SCALE); break;
    case Z: outScale.z = FMath::Max(CurrentScale.z + Amount, MIN_SCALE); break;
    default: return false;
    }
    return true;
}

void FGizmo::EndDrag()
{
    mDraggingAxis = NONE;
    eAxis = NONE;
    mbHovered = false;
}

bool FGizmo::GetDragRotation(const FVector& nearPoint, const FVector& farPoint, FRotator& outRotation)
{
    FQuat Rotation;
    if (!GetDragRotation(nearPoint, farPoint, Rotation)) return false;
    outRotation = Rotation.Rotator();
    return true;
}

bool FGizmo::GetDragRotation(const FVector& nearPoint, const FVector& farPoint, FQuat& outRotation)
{
	if (mDraggingAxis == NONE) return false;
	if (mDragStartRingDir.Length() <= SMALL_NUMBER) return false;   // 잡을 때 평면을 못 맞췄다

	FVector ringHit;
	if (!GetRingPlaneHit(nearPoint, farPoint, mDragStartGizmoLocation, mDragAxisDirection, ringHit))
	{
		return false;
	}

	const FVector v = ringHit - mDragStartGizmoLocation;
	if (v.Length() <= SMALL_NUMBER) return false;   // 중심을 정확히 지나면 각도가 정의되지 않는다

	// 시작 시점에 박아둔 2D 기저. u가 0도, w가 90도 방향이다
	const FVector u = mDragStartRingDir;
	const FVector w = FVector::cross(mDragAxisDirection, u);   // 오른손 기준

	const float angle = FMath::RadiansToDegrees(atan2f(FVector::dot(v, w), FVector::dot(v, u)));

	// atan2는 -180~180이라 한 바퀴 넘길 때 부호가 튄다.
	// 절대각을 그대로 쓰지 않고 프레임 간 차이를 접어서 누적한다
	mDragAccumAngle += WrapAngle180(angle - mDragLastAngle);
	mDragLastAngle = angle;

	// Freeze the world-space rotation axis at drag start, even while local axes are redrawn.
	const FVector axis = mDragAxisDirection;

	const float halfAngle = FMath::DegreesToRadians(mDragAccumAngle * 0.5f);

	const float s = std::sin(halfAngle);
	const float c = std::cos(halfAngle);

	const FQuat delta(axis.x * s, axis.y * s, axis.z * s, c);

	outRotation = delta * mDragStartTransform.Rotation;
	return true;
}

bool FGizmo::IsRayInGizmo(FVector nearPoint, FVector farPoint, const FVector2& MousePosition,
    const FVector2& ViewportSize, const FMatrix& ViewProjection)
{

	mbHovered = false;
	eAxis = NONE;
	if (!mbVisible) return false;
	FVector norm_ray = (farPoint - nearPoint);
	norm_ray.Normalize(); // norm_ray= ray의 단위벡터
	const EGIZMO_AXIS axis[3] = { X, Y, Z };

	if (eType == ROTATE) //회전 기즈모의 충돌처리
	{
		const float ringRadius = mRingRadiusRatio * mGizmoScale;
		float shortAxisLen = 0.0f;
		for (int i = 0; i < 3; ++i)
		{
			float dn = FVector::dot(norm_ray, AxisDirection(axis[i]));
			if (FMath::Abs(dn) < 1e-4f) continue;

			//t x norm_ray = 링위의점
			float t = FVector::dot((mLocation - nearPoint), AxisDirection(axis[i]))
				/ dn;

			if (t < 0.0f) continue;

			FVector H = (norm_ray * t) + nearPoint;
			float r = (H - mLocation).Length(); //구 중심과 평면교점사이의 거리

			if (FMath::Abs(r - ringRadius) > ringRadius * mRingHitRadius) continue;

			if (eAxis == NONE || t < shortAxisLen) {
				shortAxisLen = t;
				eAxis = axis[i];
			}
		}


	}
    else // WEEK3: hit-test projected line segments with a fixed pixel tolerance.
    {
        constexpr float HitRadius = 5.f;
        for (EGIZMO_AXIS Axis : axis)
        {
            FVector2 Start, End;
            if (!GetScreenAxis(Axis, ViewportSize, ViewProjection, Start, End)) continue;
            const FVector2 Segment = End - Start;
            const float T = FMath::Clamp(FVector2::dot(MousePosition - Start, Segment) / Segment.LengthSquared(), 0.f, 1.f);
            const FVector2 Closest(Start.x + Segment.x * T, Start.y + Segment.y * T);
            if ((MousePosition - Closest).LengthSquared() >= HitRadius * HitRadius) continue;
            eAxis = Axis;
            break;
        }
    }

	return eAxis != NONE;
}

void FGizmo::Reset()
{
	mbVisible = false;
	mLocation = FVector(0.0f, 0.0f, 0.0f);

	// 드래그 상태도 같이 지운다. 안 그러면 선택이 풀린 뒤에도 드래그가 살아남는다
	EndDrag();
	mTarget.Reset();
}

const char* FGizmo::GetAxisMeshName() const
{
	switch (eType)
	{
	case TRANSLATE: return BuiltinAssetNames::GizmoArrowMesh;
	case ROTATE: return BuiltinAssetNames::CircleMesh;
	case SCALE: return BuiltinAssetNames::CubeMesh;
	default: return BuiltinAssetNames::GizmoArrowMesh;
	}
}

FMatrix FGizmo::GetAxisMatrix(EGIZMO_AXIS axis) const // 축모양 도형을 반환
{
	const float length = mAxisLength * mGizmoScale;
	const float thickness = mAxisThickness * mGizmoScale;
	const float ScaleBarthickness = mScaleBarThickness * mGizmoScale;

	const FRotator rotation = FRotator::FromDirection(AxisDirection(axis));
	if (eType == ROTATE)
	{
		const EGIZMO_AXIS axis[3] = { X, Y, Z };
		for (int i = 0;i < 3;i++) {
			return FMatrix::Scale(FVector(mGizmoScale * mRingRadiusRatio))
				* FMatrix::Rotate(rotation)
				* FMatrix::Translation(mLocation);
		}
	}
	else if (eType == TRANSLATE) {
		return FMatrix::Scale(FVector(length, thickness, thickness))
			* FMatrix::Translation(FVector(0.0f, -thickness * 0.5f, -thickness * 0.5f)) // 긴막대기 모양으로변환
			* FMatrix::Rotate(rotation)
			* FMatrix::Translation(mLocation);
	}

	else { // eType == SCALE
		return FMatrix::Scale(FVector(length, ScaleBarthickness, ScaleBarthickness))
			* FMatrix::Translation(FVector(length * 0.5f, 0.0f, 0.0f))
			* FMatrix::Rotate(rotation)
			* FMatrix::Translation(mLocation);
	}

}


FLinearColor FGizmo::GetAxisColor(EGIZMO_AXIS axis) const
{
	float alpha = 1.0f;
	if (axis == eAxis) return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);   // 마우스가 올라간 축

	switch (axis)
	{
	case X:  return FLinearColor(1.0f, 0.0f, 0.0f, alpha);
	case Y:  return FLinearColor(0.0f, 1.0f, 0.0f, alpha);
	case Z:  return FLinearColor(0.0f, 0.0f, 1.0f, alpha);
	default: return FLinearColor(0.0f, 0.0f, 0.0f, alpha);
	}
}

FMatrix FGizmo::GetScaleHandleMatrix(EGIZMO_AXIS axis) const
{
	const float len = mAxisLength * mGizmoScale;
	const float handle = mScaleHandleSize * mGizmoScale;

	return FMatrix::Scale(FVector(handle))
		* FMatrix::Translation(FVector(len - handle * 0.5f, 0.0f, 0.0f))
		* FMatrix::Rotate(FRotator::FromDirection(AxisDirection(axis)))
		* FMatrix::Translation(mLocation);
}

void FGizmo::SubmitRenderInfos(FRenderCollector& Collector) const // Gizmo 모형 렌더정보
{

	if (!mbVisible) return;
	const EGIZMO_AXIS axis[3] = { X, Y, Z };
	//기즈모타입을 확인후 타입에 맞는 모양을 리턴

	for (int i = 0; i < 3; ++i)
	{
		if (eType == EGIZMO_TYPE::SCALE)
		{
			{
                FRenderMeshInfo Info{};
                Info.StaticMesh = Collector.AssetManager->GetAssetAs<UStaticMeshAsset>(GetAxisMeshName(), true);
                Info.WorldTransformMatrix = GetScaleHandleMatrix(axis[i]);
                Info.Color = GetAxisColor(axis[i]);
                Collector.GizmoInfos.Add(Info);
            }
		}
		else if (eType == EGIZMO_TYPE::ROTATE)
		{

		}
		{
                FRenderMeshInfo Info{};
                Info.StaticMesh = Collector.AssetManager->GetAssetAs<UStaticMeshAsset>(GetAxisMeshName(), true);
                Info.WorldTransformMatrix = GetAxisMatrix(axis[i]);
                Info.Color = GetAxisColor(axis[i]);
                Collector.GizmoInfos.Add(Info);
            }
	}
	return;
}

void FGizmo::Update(
	const AActor* targetActor,
	const FVector& cameraLocation,
	const FVector& cameraForward,
	float fovDegree,
	float perspectiveRatio,
	float orthoDistance) // Gizmo 깊이에따른 원근크기 보정
{
    if (!targetActor)
    {
        Reset();
        return;
    }
    const FWeakObjectPtr Target(targetActor);
    if (!(mTarget == Target))
    {
        EndDrag();
        mTarget = Target;
    }

	mbVisible = true;
	mLocation = targetActor->GetTransform().Location;
	UpdateRotation = targetActor->GetTransform().Rotation;


	float depth = FVector::dot(mLocation - cameraLocation, cameraForward);
	const float tanHalfFov = tanf(FMath::DegreesToRadians(fovDegree * 0.5f));

	float effectiveDepth = (1.0f - perspectiveRatio) * orthoDistance + perspectiveRatio * depth; // 원근보정
	mGizmoScale = effectiveDepth * tanHalfFov * mGizmoSizeRatio;

}
