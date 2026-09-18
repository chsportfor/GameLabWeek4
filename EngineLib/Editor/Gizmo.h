#pragma once

#include "Core/Math/Vector.h"
#include "Rendering/RenderInfo.h"
#include "Core/Container/TArray.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Color.h"
#include "Core/enum.h"

class AActor;

struct FGizmo {
	FVector mLocation; // 기즈모의 위치

	// 드래그 기준값. 기즈모는 액터를 따라 움직이므로, 기준선을 시작 시점에 고정해 두지 않으면
	// 결과가 기준을 다시 움직여서 발산한다.
	FTransform mDragStartTransform;       // 드래그 시작 시점의 액터 트랜스폼
	FVector mDragStartGizmoLocation;  // 드래그 시작 시점의 기즈모 위치 = 축 직선의 원점
	float mDragStartAxisS = 0.0f;     // 그 직선 위에서 처음 잡은 지점
	float mDragStartAxisLength = 1.0f; // 그 시점의 막대 길이. 스케일 비율의 분모라 같이 고정해야 한다

	// 회전용. 링 평면 안에 시작 시점 기준으로 2D 기저를 박아두고 그 기준으로 각도를 잰다.
	FVector mDragStartRingDir;         // 잡은 방향. 이게 0도
	float mDragAccumAngle = 0.0f;      // 시작 이후 누적 회전각(도)
	float mDragLastAngle = 0.0f;       // 직전 프레임 각도. ±180 넘김을 잇는 데 쓴다

	FMatrix TargetObjectTransformMatrix;
	bool mbVisible = false;
	bool mbHovered = false;
	float mGizmoScale=1.0f;
	float mAxisLength = mGizmoScale * 0.5f;
	float mAxisThickness = mGizmoScale * 0.1f;
	float mHitRadius= mAxisThickness*1.1f; // Translate 마우스 판정보정
	float mRingHitRadius = 0.08f; // Rotate마우스 판정보정 (+0.08배)
	float mRingRadiusRatio = 0.4f;
	float mScaleBarThickness = mAxisLength * 0.035f;
	float mScaleHandleSize = mAxisLength * 0.13;
	float mGizmoSizeRatio = 0.3f;
	FQuat UpdateRotation = {};
	
	EGIZMO_AXIS eAxis = NONE; // 축위에 있는지
	EGIZMO_AXIS mDraggingAxis = NONE; // Drag중인 축
	EGIZMO_TYPE eType= TRANSLATE;

	FVector AxisDirection(EGIZMO_AXIS axis) const;

	// -180 ~ 180 으로 접는다
	static float WrapAngle180(float degree);

	// 링이 놓인 평면(원점 planeOrigin, 법선 axis)과 레이의 교점.
	// 레이가 평면과 나란하면 교점이 없거나 무한히 많아서 false.
	bool GetRingPlaneHit(
		const FVector& nearPoint,
		const FVector& farPoint,
		const FVector& planeOrigin,
		EGIZMO_AXIS axis,
		FVector& outPoint) const;

	// 레이와 축 직선의 최단거리 지점을 축 파라미터 s로 돌려준다.
	bool GetClosestAxisParam(
		const FVector& nearPoint,
		const FVector& farPoint,
		const FVector& axisOrigin,
		EGIZMO_AXIS axis,
		float& outAxisS) const;

	// 축을 잡은 순간의 기준값을 저장한다. 이후 드래그는 전부 이 기준에 대한 상대량이다.
	void BeginDrag(const FVector& nearPoint, const FVector& farPoint, const FTransform& ActorTransform);
	// 드래그 중인 축을 따라 액터가 있어야 할 위치. 축이 시선과 나란하면 false (이번 프레임은 건너뛴다)
	bool GetDragLocation(const FVector& nearPoint, const FVector& farPoint, FVector& outLocation) const;

	// 드래그 중인 축의 스케일. 이동과 달리 거리를 그대로 더하지 않고 막대 길이 대비 비율로 환산한다.
	// 그래야 감도가 카메라 거리에 좌우되지 않고, 막대 끝까지 끌면 언제나 2배가 된다.
	bool GetDragScale(const FVector& nearPoint, const FVector& farPoint, FVector& outScale) const;

	// 드래그 중인 링을 따라 액터가 가져야 할 회전.
	// 누적각을 갱신하므로 const가 아니다.
	bool GetDragRotation(const FVector& nearPoint, const FVector& farPoint, FRotator& outRotation);
	bool GetDragRotation(const FVector& nearPoint, const FVector& farPoint, FQuat& outRotation);
	bool IsRayInGizmo(FVector nearPoint, FVector farPoint);
	void Reset();

	EPrimitive GetAxisPrimitive() const;
	FMatrix GetAxisMatrix(EGIZMO_AXIS axis) const; // 축모양 도형을 반환
	

	FLinearColor GetAxisColor(EGIZMO_AXIS axis) const;

	FMatrix GetScaleHandleMatrix(EGIZMO_AXIS axis) const;

	void SubmitRenderInfos(FRenderCollector& Collector) const; // Gizmo 모형 렌더정보

	void SetGizmoType(EGIZMO_TYPE type) { eType = type; }
	void CycleGizmoType() { eType = static_cast<EGIZMO_TYPE>((static_cast<int>(eType) + 1) % 3); }

	// Gizmo 깊이에따른 원근크기 보정
	void Update(
		const AActor* targetActor,
		const FVector& cameraLocation,
		const FVector& cameraForward,
		float fovDegree,
		float perspectiveRatio,
		float orthoDistance);

};
