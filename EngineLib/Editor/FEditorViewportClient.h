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

	bool RaycastBounds(
		const FVector& rayStart,
		const FVector& rayEnd,
		const FBoundingBox& bounds);
	void RayCast(D3D11_VIEWPORT ViewportInfo, const TArray<FPickInfo>& renderInfos,
		float perspectiveRatio, bool bCheckObject);
	float GetFov() const { return mCamera.mFovDegree; }
	void Update(float deltaTime, D3D11_VIEWPORT ViewportInfo, FSceneManager* sceneManager, float perspectiveRatio);
	bool IsMouseHit() const { return bMouseHit; }

	void Reset();

	FCamera& GetCamera() { return mCamera; }
	const FCamera& GetCamera() const { return mCamera; }

	FMatrix GetProjectionMatrix(float aspect) const;
	FMatrix GetInverseProjectionMatrix(float aspect) const;

	bool IsOrtho() const { return ViewportType != ELevelViewportType::Perspective; }

	FCamera mCamera;
	FGizmo mGizmo;

private:
	//마우스 밑 무언가의
	FPickInfo mHoveredPickInfo;

	// 피킹 시 현재 컴포넌트의 변환과 바운드를 수집한다.
	bool RayIntersectsTriangle( // 두개의 
		const FVector& Origin,
		const FVector& Dir,
		const FVector& V0,
		const FVector& V1,
		const FVector& V2,
		float& OutT, float& OutU, float& OutV);

	void DeprojectScreenToWorldForUnified(int32 MouseX, int32 MouseY,
		float ScreenW, float ScreenH, float NearZ, float FarZ,
		float orthoDistance, float perspectiveRatio,
		FVector& OutNearPoint, FVector& OutFarPoint
	);

	bool bMouseHit = false;

	
	// RayCast가 이번 프레임에 쏜 광선. 기즈모 드래그가 같은 광선을 다시 쓴다
	FVector mRayNear;
	FVector mRayFar;

	// 복사용 클립보드
	TMap<int32, int32> UUIDChangeMap;
	json::JSON mActorClipBoard;
	json::JSON copyObject;

	ELevelViewportType ViewportType = {};
};
