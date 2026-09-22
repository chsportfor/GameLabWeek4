#pragma once

#include "Core/Math/Vector.h"
#include "Rendering/RenderInfo.h"
#include "Core/Container/TArray.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Color.h"
#include "Core/enum.h"
#include "Core/Object/WeakObjectPtr.h"

class AActor;

struct FGizmo {
	FVector mLocation; // 기즈모의 위치

	// 드래그 기준값. 기즈모는 액터를 따라 움직이므로, 기준선을 시작 시점에 고정해 두지 않으면
	// 결과가 기준을 다시 움직여서 발산한다.
	FTransform mDragStartTransform;       // 드래그 시작 시점의 액터 트랜스폼
	FVector mDragStartGizmoLocation;  // 드래그 시작 시점의 기즈모 위치 = 축 직선의 원점
	float mDragStartAxisS = 0.0f;     // 그 직선 위에서 처음 잡은 지점
	FVector mDragAxisDirection;
	FVector2 mDragScreenStart;
	FVector2 mDragScreenDirection;
	FVector2 mPreviousMousePosition;
	FWeakObjectPtr mTarget;

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
	void SetWorldMode(bool bInWorldMode);
	bool IsWorldMode() const { return bWorldMode; }

	// -180 ~ 180 으로 접는다
	static float WrapAngle180(float degree);

	// 링이 놓인 평면(원점 planeOrigin, 법선 axis)과 레이의 교점.
	// 레이가 평면과 나란하면 교점이 없거나 무한히 많아서 false.
	bool GetRingPlaneHit(
		const FVector& nearPoint,
		const FVector& farPoint,
		const FVector& planeOrigin,
		const FVector& axisDirection,
		FVector& outPoint) const;

    // MousePosition is in viewport-local pixels; ViewportSize belongs to this viewport.
    bool BeginDrag(const FVector& nearPoint, const FVector& farPoint, const FTransform& ActorTransform,
        const FVector2& MousePosition, const FVector2& ViewportSize, const FMatrix& ViewProjection,
        const FMatrix& InverseViewProjection);
    bool GetDragLocation(const FVector2& MousePosition, const FVector2& ViewportSize,
        const FMatrix& InverseViewProjection, FVector& outLocation) const;
    // WEEK3: add 0.01 scale units per pixel along the projected axis, then clamp.
    bool GetDragScale(const FVector2& MousePosition, const FVector& CurrentScale, FVector& outScale);
    void EndDrag();

	// 드래그 중인 링을 따라 액터가 가져야 할 회전.
	// 누적각을 갱신하므로 const가 아니다.
	bool GetDragRotation(const FVector& nearPoint, const FVector& farPoint, FRotator& outRotation);
	bool GetDragRotation(const FVector& nearPoint, const FVector& farPoint, FQuat& outRotation);
	bool IsRayInGizmo(FVector nearPoint, FVector farPoint, const FVector2& MousePosition,
        const FVector2& ViewportSize, const FMatrix& ViewProjection);
	void Reset();

	const char* GetAxisMeshName() const;
	FMatrix GetAxisMatrix(EGIZMO_AXIS axis) const; // 축모양 도형을 반환
	

	FLinearColor GetAxisColor(EGIZMO_AXIS axis) const;

	FMatrix GetScaleHandleMatrix(EGIZMO_AXIS axis) const;

	void SubmitRenderInfos(FRenderCollector& Collector) const; // Gizmo 모형 렌더정보

	void SetGizmoType(EGIZMO_TYPE type) { if (eType != type) EndDrag(); eType = type; }
	void CycleGizmoType() { SetGizmoType(static_cast<EGIZMO_TYPE>((static_cast<int>(eType) + 1) % 3)); }

	// Gizmo 깊이에따른 원근크기 보정
	void Update(
		const AActor* targetActor,
		const FVector& cameraLocation,
		const FVector& cameraForward,
		float fovDegree,
		float perspectiveRatio,
		float orthoDistance);

private:
    bool bWorldMode = true;
    bool GetScreenAxis(EGIZMO_AXIS Axis, const FVector2& ViewportSize, const FMatrix& ViewProjection,
        FVector2& Start, FVector2& End) const;
    bool GetProjectedAxisParameter(const FVector2& MousePosition, const FVector2& ViewportSize,
        const FMatrix& InverseViewProjection, float& Parameter) const;
};
