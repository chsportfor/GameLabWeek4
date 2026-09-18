#include "Gizmo.h"

#include "Core/Math/Color.h"
#include "Engine/Actor.h"

FVector FGizmo::AxisDirection(EGIZMO_AXIS axis) const {
	//const FMatrix Result_yaw = FMatrix::RotateZ(UpdateRotation.Yaw);
	//const FMatrix Result_pitch = FMatrix::RotateY(UpdateRotation.Pitch);
	//const FMatrix Result_roll = FMatrix::RotateX(UpdateRotation.Roll);

	// TODO: Support local space rotation gizmo
	if (eType == EGIZMO_TYPE::ROTATE)
	{

		switch (axis)
		{
		case X: return FVector::Forward();
		case Y: return FVector::Right();
		case Z: return FVector::Up();
		default: return FVector(0);
		}
	}

	else if (eType == EGIZMO_TYPE::SCALE)
	{
		switch (axis)
		{
		case X:  return FMatrix::Rotate(UpdateRotation).GetUnitAxis(EAxis::X);
		case Y:  return FMatrix::Rotate(UpdateRotation).GetUnitAxis(EAxis::Y);
		case Z:  return FMatrix::Rotate(UpdateRotation).GetUnitAxis(EAxis::Z);
		default: return FVector(0.0f, 0.0f, 0.0f);

		}


	}
	else { //Translate
		switch (axis)
		{
		case X:  return FVector::Forward();
		case Y:  return FVector::Right();
		case Z:  return FVector::Up();
		default: return FVector(0.0f, 0.0f, 0.0f);
		}
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
	EGIZMO_AXIS axis,
	FVector& outPoint) const
{
	FVector norm_ray = farPoint - nearPoint;
	norm_ray.Normalize();

	const FVector axisDir = AxisDirection(axis);

	const float dn = FVector::dot(norm_ray, axisDir);
	if (FMath::Abs(dn) < 1e-4f) return false;   // 링을 모서리로 보는 각도

	const float t = FVector::dot(planeOrigin - nearPoint, axisDir) / dn;
	if (t < 0.0f) return false;                 // 카메라 뒤쪽

	outPoint = nearPoint + norm_ray * t;

	return true;
}

bool FGizmo::GetClosestAxisParam(
	const FVector& nearPoint,
	const FVector& farPoint,
	const FVector& axisOrigin,
	EGIZMO_AXIS axis,
	float& outAxisS) const
{
	FVector norm_ray = farPoint - nearPoint;
	norm_ray.Normalize();

	const FVector axisDir = AxisDirection(axis);
	const FVector w0 = nearPoint - axisOrigin;

	const float align = FVector::dot(norm_ray, axisDir);
	const float denom = 1.0f - align * align;
	if (FMath::Abs(denom) < 1e-5f) return false;   // 레이와 축이 거의 나란함

	const float rayProj = FVector::dot(norm_ray, w0);
	const float axisProj = FVector::dot(axisDir, w0);

	outAxisS = (axisProj - align * rayProj) / denom;

	return true;
}

void FGizmo::BeginDrag(const FVector& nearPoint, const FVector& farPoint, const FTransform& ActorTransform)
{
	mDraggingAxis = eAxis;
	mDragStartTransform = ActorTransform;
	mDragStartGizmoLocation = mLocation;
	mDragStartAxisS = 0.0f;
	mDragStartAxisLength = mAxisLength * mGizmoScale;

	GetClosestAxisParam(nearPoint, farPoint, mDragStartGizmoLocation, mDraggingAxis, mDragStartAxisS);

	// 회전은 축 직선이 아니라 링 평면 위에서 잰다. 잡은 방향을 0도 기준으로 박아둔다
	mDragStartRingDir = FVector(0.0f, 0.0f, 0.0f);
	mDragAccumAngle = 0.0f;
	mDragLastAngle = 0.0f;

	FVector ringHit;
	if (GetRingPlaneHit(nearPoint, farPoint, mDragStartGizmoLocation, mDraggingAxis, ringHit))
	{
		FVector ringDir = ringHit - mDragStartGizmoLocation;
		if (ringDir.Length() > SMALL_NUMBER)
		{
			ringDir.Normalize();
			mDragStartRingDir = ringDir;
		}
	}
}

bool FGizmo::GetDragLocation(const FVector& nearPoint, const FVector& farPoint, FVector& outLocation) const
{
	if (mDraggingAxis == NONE) return false;

	float axisS = 0.0f;
	if (!GetClosestAxisParam(nearPoint, farPoint, mDragStartGizmoLocation, mDraggingAxis, axisS))
	{
		return false;
	}

	outLocation = mDragStartTransform.Location + AxisDirection(mDraggingAxis) * (axisS - mDragStartAxisS);

	return true;
}

bool FGizmo::GetDragScale(const FVector& nearPoint, const FVector& farPoint, FVector& outScale) const
{
	if (mDraggingAxis == NONE) return false;
	if (mDragStartAxisLength <= SMALL_NUMBER) return false;

	float axisS = 0.0f;
	if (!GetClosestAxisParam(nearPoint, farPoint, mDragStartGizmoLocation, mDraggingAxis, axisS))
	{
		return false;
	}

	const float ratio = 1.0f + (axisS - mDragStartAxisS) / mDragStartAxisLength;

	// 0을 지나 음수가 되면 물체가 뒤집히고, 행렬식이 무너져 레이캐스트의 Inverse()가 깨진다
	outScale = mDragStartTransform.Scale;
	switch (mDraggingAxis)
	{
	case X: outScale.x = FMath::Max(outScale.x * ratio, MIN_SCALE); break;
	case Y: outScale.y = FMath::Max(outScale.y * ratio, MIN_SCALE); break;
	case Z: outScale.z = FMath::Max(outScale.z * ratio, MIN_SCALE); break;
	default: return false;
	}

	return true;
}

bool FGizmo::GetDragRotation(const FVector& nearPoint, const FVector& farPoint, FRotator& outRotation)
{
	if (mDraggingAxis == NONE) return false;
	if (mDragStartRingDir.Length() <= SMALL_NUMBER) return false;   // 잡을 때 평면을 못 맞췄다

	FVector ringHit;
	if (!GetRingPlaneHit(nearPoint, farPoint, mDragStartGizmoLocation, mDraggingAxis, ringHit))
	{
		return false;
	}

	const FVector v = ringHit - mDragStartGizmoLocation;
	if (v.Length() <= SMALL_NUMBER) return false;   // 중심을 정확히 지나면 각도가 정의되지 않는다

	// 시작 시점에 박아둔 2D 기저. u가 0도, w가 90도 방향이다
	const FVector u = mDragStartRingDir;
	const FVector w = FVector::cross(AxisDirection(mDraggingAxis), u);   // 오른손 기준

	const float angle = FMath::RadiansToDegrees(atan2f(FVector::dot(v, w), FVector::dot(v, u)));

	// atan2는 -180~180이라 한 바퀴 넘길 때 부호가 튄다.
	// 절대각을 그대로 쓰지 않고 프레임 간 차이를 접어서 누적한다
	mDragAccumAngle += WrapAngle180(angle - mDragLastAngle);
	mDragLastAngle = angle;

	// FMatrix::Rotate를 미소각으로 전개해 보면 Yaw만 오른손이고 Pitch/Roll은 왼손이다.
	// 위에서 구한 각도는 오른손 기준이라 축에 따라 부호를 뒤집는다
	outRotation = mDragStartTransform.GetRotator();
	switch (mDraggingAxis)
	{
	case X: outRotation.Roll = outRotation.Roll - mDragAccumAngle; break;
	case Y: outRotation.Pitch = outRotation.Pitch - mDragAccumAngle; break;
	case Z: outRotation.Yaw = outRotation.Yaw + mDragAccumAngle; break;
	default: return false;
	}

	return true;
}

// New GetDragRotation function that returns a quaternion instead of a rotator
bool FGizmo::GetDragRotation(const FVector& nearPoint, const FVector& farPoint, FQuat& outRotation)
{
	if (mDraggingAxis == NONE) return false;
	if (mDragStartRingDir.Length() <= SMALL_NUMBER) return false;   // 잡을 때 평면을 못 맞췄다

	FVector ringHit;
	if (!GetRingPlaneHit(nearPoint, farPoint, mDragStartGizmoLocation, mDraggingAxis, ringHit))
	{
		return false;
	}

	const FVector v = ringHit - mDragStartGizmoLocation;
	if (v.Length() <= SMALL_NUMBER) return false;   // 중심을 정확히 지나면 각도가 정의되지 않는다

	// 시작 시점에 박아둔 2D 기저. u가 0도, w가 90도 방향이다
	const FVector u = mDragStartRingDir;
	const FVector w = FVector::cross(AxisDirection(mDraggingAxis), u);   // 오른손 기준

	const float angle = FMath::RadiansToDegrees(atan2f(FVector::dot(v, w), FVector::dot(v, u)));

	// atan2는 -180~180이라 한 바퀴 넘길 때 부호가 튄다.
	// 절대각을 그대로 쓰지 않고 프레임 간 차이를 접어서 누적한다
	mDragAccumAngle += WrapAngle180(angle - mDragLastAngle);
	mDragLastAngle = angle;

	const FVector axis = AxisDirection(mDraggingAxis);

	const float halfAngle = FMath::DegreesToRadians(mDragAccumAngle * 0.5f);

	const float s = std::sin(halfAngle);
	const float c = std::cos(halfAngle);

	const FQuat delta(axis.x * s, axis.y * s, axis.z * s, c);

	outRotation = delta * mDragStartTransform.Rotation;
	return true;
}

bool FGizmo::IsRayInGizmo(FVector nearPoint, FVector farPoint)
{

	/*
	Ray와 Axis사이의 최단거리를 구한다.
	3차원의 두 직선에 최단거리는 각 두 직선에 수직하는 선분이다.

	수직벡터 = 광선벡터 - 기즈모축벡터
	1) W = D - A (모두 단위벡터임)
	2) WxD=0, WxA=0 (수직이므로 내적값이 0)

	3)W = ray시작점 + t*(ray단위벡터) - (기즈모시작점 + s*기즈모 단위벡터)

	*/
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
	else { // TRANSLATE, SCALE
		FVector w0 = nearPoint - mLocation;
		//수학 함수 구현
		const float axisLength = mAxisLength * mGizmoScale;
		const float hitRadius = mHitRadius * mGizmoScale;

		float bestRayT = 0.0f; //near point에서 광선방향으로 얼마나 이동했냐

		for (int i = 0; i < 3; ++i)
		{
			const FVector axisDir = AxisDirection(axis[i]);

			float axisS = 0.0f;
			if (!GetClosestAxisParam(nearPoint, farPoint, mLocation, axis[i], axisS)) continue;

			axisS = FMath::Clamp(axisS, 0.0f, axisLength);  // 무한 직선 → 선분

			const FVector axisPoint = mLocation + axisDir * axisS; // 현재위치에서 기즈모방향으로 얼만큼 이동했나

			const float rayT = FVector::dot(norm_ray, axisPoint - nearPoint);
			if (rayT < 0.0f) continue;                      // 카메라 뒤쪽

			const FVector rayPoint = nearPoint + norm_ray * rayT;
			const float distance = (rayPoint - axisPoint).Length();

			if (distance > hitRadius) continue;             // 캡슐 밖

			if (eAxis == NONE || rayT < bestRayT)           // 겹치면 카메라에 가까운 축
			{
				bestRayT = rayT;
				eAxis = axis[i];
			}
		}
	}

	return eAxis != NONE;
}

void FGizmo::Reset()
{
	mbVisible = false;
	mLocation = FVector(0.0f, 0.0f, 0.0f);

	// 드래그 상태도 같이 지운다. 안 그러면 선택이 풀린 뒤에도 드래그가 살아남는다
	eAxis = NONE;
	mDraggingAxis = NONE;
}

EPrimitive FGizmo::GetAxisPrimitive() const
{
	switch (eType)
	{
	case TRANSLATE: return EPrimitive::EP_GizmoArrow;
	case ROTATE: return EPrimitive::EP_Circle; //EP_Rotate
	case SCALE: return EPrimitive::EP_Cube;
	default: return EPrimitive::EP_GizmoArrow;
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

TArray<FRenderInfo>	FGizmo::GetGizmoRenderInfo() const // Gizmo 모형 렌더정보
{
	TArray<FRenderInfo> renderInfos;

	if (!mbVisible) return renderInfos;
	const EGIZMO_AXIS axis[3] = { X, Y, Z };
	//기즈모타입을 확인후 타입에 맞는 모양을 리턴

	ERenderFlags renderFlags = ERenderFlags::RF_Gizmo;
	for (int i = 0; i < 3; ++i)
	{
		if (eType == EGIZMO_TYPE::SCALE)
		{
			{
                FRenderInfo Info{};
                Info.ePrimitive = GetAxisPrimitive();
                Info.WorldTransformMatrix = GetScaleHandleMatrix(axis[i]);
                Info.Color = GetAxisColor(axis[i]);
                Info.eRenderFlags = renderFlags;
                renderInfos.Add(Info);
            }
		}
		else if (eType == EGIZMO_TYPE::ROTATE)
		{

		}
		{
                FRenderInfo Info{};
                Info.ePrimitive = GetAxisPrimitive();
                Info.WorldTransformMatrix = GetAxisMatrix(axis[i]);
                Info.Color = GetAxisColor(axis[i]);
                Info.eRenderFlags = renderFlags;
                renderInfos.Add(Info);
            }
	}
	return renderInfos;
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
		mbVisible = false;
		return;
	}

	mbVisible = true;
	mLocation = targetActor->GetTransform().Location;
	UpdateRotation = targetActor->GetTransform().Rotation;


	float depth = FVector::dot(mLocation - cameraLocation, cameraForward);
	const float tanHalfFov = tanf(FMath::DegreesToRadians(fovDegree * 0.5f));

	float effectiveDepth = (1.0f - perspectiveRatio) * orthoDistance + perspectiveRatio * depth; // 원근보정
	mGizmoScale = effectiveDepth * tanHalfFov * mGizmoSizeRatio;

}
