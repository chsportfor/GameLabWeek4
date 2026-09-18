#pragma once

#include "Core/Math/Matrix.h"
#include "Core/enum.h"

#include "Core/Container/TArray.h"
#include "Core/Container/TMap.h"
#include "Renderer.h"
#include "pipelines/FLineGraphicsPipeline.h"
#include "pipelines/FMeshGraphicsPipeline.h"
#include "pipelines/FQuadGraphicsPipeline.h"
#include "pipelines/FStencilMarkGraphicsPipeline.h"
#include "pipelines/FStencilOutlineGraphicsPipeline.h"
#include "pipelines/FLine2DGraphicsPipeline.h"
#include "pipelines/FCircle2DGraphicsPipeline.h"
#include "pipelines/FTriangle2DGraphicsPipeline.h"
#include "pipelines/FWorldAxisGraphicsPipeline.h"
#include "pipelines/FWorldGridGraphicsPipeline.h"
#include "Camera.h"
#include "RenderInfo.h"
#include "Core/Math/Vector.h"


class FAssetManager;

class FRenderingPipeline // TODO: 역할에 맞지 않는 변수/함수의 소유권 이전
{
public:
	FRenderingPipeline(HWND hWindow);
	~FRenderingPipeline();

	//void Prepare(const Camera* mCamera);
	void Prepare(const FCamera* Camera,float viewportWidth, float viewportHeight);

	//void Render(FTransform worldTransformMatrix, EPrimitive ePrimitive); // FRenderInfo
	//void Render(const TArray<FRenderInfo> renderInfos);
	void Render();
	//void RenderOverlay(const TArray<FRenderInfo> renderInfos); //깊이버퍼 초기화
	// FRenderInfo

	void Display();
	void Update(float deltaTime);

	float GetAspect() const { return mAspect; }
	EViewModeIndex GetViewModeIndex() const { return mViewModeIndex; }
	void SetViewModeIndex(EViewModeIndex viewModeIndex) { mViewModeIndex = viewModeIndex; }

	bool IsPerspectiveProjection() const;
	void SetPerspectiveProjection(bool bPerspectiveProjection);

	float GetPerspectiveRatio() const { return mProjectionRatio; }
	const FMatrix& GetViewProjectionMatrix() const { return mViewUnifiedProjectionMatrix; }
	void SetPerspectiveRatio(float ratio) { mProjectionRatio = FMath::Clamp(ratio, 0.0f, 1.0f); }

	float GetCameraOrthoDistance() const { return mCameraOrthoDistance; }
	void SetCameraOrthoDistance(float distance) { mCameraOrthoDistance = distance; }

	// Todo: Change name
	URenderer* GetRenderer() const;
	void OnResize(UINT width, UINT height);

	//Highlight
	//Line batch
	// 호출 즉시 그리지 않고 배열에 쌓는다. FlushLines()에서 한 번에 그린다.
	void DrawLine(const FVector& start, const FVector& end, const FVector4& color);
	void FlushLines();

	// 표시 옵션은 FShowFlags가 들고 있다. 여기서 중계하지 않는다.

	void RenderHighLight(const FRenderInfo& RI);

	// Projection ratio smoothing
	void StartProjectionTransition(bool orthographic);
	bool IsOrthographicTarget() const;
	void UpdateProjectionTransition(float deltaTime);

	inline const TSharedPtr<FRenderTarget2D>& GetSceneRenderTarget() const { return mSceneRenderTarget; }
	inline FRenderCollector& GetRenderCollector() { return mRenderCollector; }
	inline TArray<FRenderInfo>& GetRenderInfos() { return mRenderCollector.RenderInfos; }

	inline int32 GetGridGap() { return GridGap; }
	void SetGridGap(int32 GridGap);
	FLine2DGraphicsPipeline& GetLine2DPipeline() { return *mLine2DPipeline; }
	FCircle2DGraphicsPipeline& GetCircle2DPipeline() { return *mCircle2DPipeline; }
	FTriangle2DGraphicsPipeline& GetTriangle2DPipeline() { return *mTriangle2DPipeline; }
	bool HasShowFlag(EEngineShowFlags Flag) const { return (ShowFlags & static_cast<uint32>(Flag)) != 0; }
	void SetShowFlag(EEngineShowFlags Flag, bool Enabled)
	{
		if (Enabled) ShowFlags |= static_cast<uint32>(Flag);
		else ShowFlags &= ~static_cast<uint32>(Flag);
	}
private:
	uint32 ShowFlags = ~0u;
	URenderer* mRenderer;
	FMatrix mViewMatrix;
	FMatrix mProjectionMatrix;
	FMatrix mViewProjectionMatrix;
	FMatrix mViewOrthogonalProjectionMatrix;
	FMatrix mViewUnifiedProjectionMatrix;

	// Prepare에서 갱신. 하이라이트 두께의 픽셀 → 월드 환산에 쓴다
	FVector mCameraLocation;
	FVector mCameraForward;
	float mCameraFovDegree = 60.0f;
	float mCameraOrthoDistance = 10.0f;

	// Graphics config
	// 이번 프레임에 쌓인 선분. 정점 2개가 선분 하나
	TArray<FVertexSimple> mLineVertices;

	EViewModeIndex mViewModeIndex = EViewModeIndex::VMI_Lit;
	bool mbPerspectiveProjection;
	float mAspect;
	float mProjectionRatio; // 0.0f ~ 1.0f, 0이면 직교, 1이면 원근, 그 사이면 혼합

	// Projection ratio smoothing
	float mProjectionStartRatio = 1.0f;
	float mProjectionTargetRatio = 1.0f;
	float mProjectionElapsed = 0.0f;
	float mProjectionDuration = 1.0f;
	bool mbProjectionTransitioning = false;

	TSharedPtr<FRenderTarget2D> mSceneRenderTarget;
	TSharedPtr<FDepthStencil> mSceneDepthStencil;
	std::unique_ptr<FLineGraphicsPipeline> mLinePipeline;
	std::unique_ptr<FMeshGraphicsPipeline> mMeshPipeline;
	std::unique_ptr<FQuadGraphicsPipeline> mQuadPipeline;
	std::unique_ptr<FStencilMarkGraphicsPipeline> mStencilMarkPipeline;
	std::unique_ptr<FStencilOutlineGraphicsPipeline> mStencilOutlinePipeline;
	std::unique_ptr<FLine2DGraphicsPipeline> mLine2DPipeline;
	std::unique_ptr<FCircle2DGraphicsPipeline> mCircle2DPipeline;
	std::unique_ptr<FTriangle2DGraphicsPipeline> mTriangle2DPipeline;
	std::unique_ptr<FWorldAxisGraphicsPipeline> mWorldAxisPipeline;
	std::unique_ptr<FWorldGridGraphicsPipeline> mWorldGridPipeline;

	FRenderCollector mRenderCollector;

	int32 GridGap = 1;
};
