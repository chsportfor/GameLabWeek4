#pragma once
#include "Core/Math/Vector.h"

#include <d3d11.h>
#include "Engine/World.h"
#include "Engine/EngineStatics.h"
#include "Rendering/Camera.h"
#include "Rendering/RenderInfo.h"
#include "Gizmo.h"
#include "Core/Math/FBoundingBox.h"
#include "Core/enum.h"


class AActor;
class FSceneManager;

struct FEditorViewportClient
{
public:
	void Initialize(ELevelViewportType inType);
	ELevelViewportType GetViewportType() { return ViewportType; }
	EViewModeIndex GetViewMode() const { return mViewMode; }
	void SetViewMode(EViewModeIndex inMode) { mViewMode = inMode; }

	bool RaycastBounds(
		const FVector& rayStart,
		const FVector& rayEnd,
		const FBoundingBox& bounds);
	void RayCast(D3D11_VIEWPORT ViewportInfo, const FPickTargets& PickTargets,
		float perspectiveRatio, bool bCheckObject);
	void UpdateCameraControls(float deltaTime, float perspectiveRatio,
		bool bAllowMouseInput, bool bAllowKeyboardInput);
	float GetFov() const { return mCamera.mFovDegree; }
	void Update(float deltaTime, D3D11_VIEWPORT ViewportInfo, FSceneManager* sceneManager, float perspectiveRatio);
	void UpdateGizmo(const AActor* selectedActor);


	void Reset();

	FCamera& GetCamera() { return mCamera; }
	const FCamera& GetCamera() const { return mCamera; }

	FMatrix GetProjectionMatrix(float aspect) const;
	FMatrix GetInverseProjectionMatrix(float aspect) const;

	bool IsMouseHit() const { return bMouseHit; }
	bool IsOrtho() const {
		return ViewportType != ELevelViewportType::Perspective; // 0 : 직교, 1 : 원근
	}
	void SetPerspectiveRatio(float r) { mPerspectiveRatio = r; }
	float GetPerspectiveRatio() const { return IsOrtho() ? 0.0f : mPerspectiveRatio; }


	FCamera mCamera;
	FGizmo mGizmo;

private:
	//마우스 밑 무언가의
	TWeakObjectPtr<AActor> mHoveredActor;

	void DeprojectScreenToWorldForUnified(int32 MouseX, int32 MouseY,
		float ScreenW, float ScreenH, float NearZ, float FarZ,
		float orthoDistance, float perspectiveRatio,
		FVector& OutNearPoint, FVector& OutFarPoint
	);

	bool bMouseHit = false;

	float mPerspectiveRatio = 1.0f;
	EViewModeIndex mViewMode = EViewModeIndex::VMI_Lit;

	// RayCast가 이번 프레임에 쏜 광선. 기즈모 드래그가 같은 광선을 다시 쓴다
	FVector mRayNear;
	FVector mRayFar;

	// 복사용 클립보드
	json::JSON mActorClipBoard;
	json::JSON copyObject;

	ELevelViewportType ViewportType = {};
};
