#pragma once

#include "Core/Math/Matrix.h"
#include "Core/enum.h"

#include "Editor/FViewport.h"
#include "Core/Container/TArray.h"
#include "Renderer.h"
#include "pipelines/FFullscreenGraphicsPipeline.h"
#include "pipelines/FQuadGraphicsPipeline.h"
#include "pipelines/FTextGraphicsPipeline.h"
#include "pipelines/FStencilOutlineGraphicsPipeline.h"
#include "pipelines/FStencilMarkGraphicsPipeline.h"
#include "pipelines/FMeshGraphicsPipeline.h"
#include "pipelines/FLineGraphicsPipeline.h"
#include "pipelines/FWorldAxisGraphicsPipeline.h"
#include "pipelines/FWorldGridGraphicsPipeline.h"
#include "Camera.h"
#include "RenderInfo.h"
#include "Core/Math/Vector.h"
#include "Core/Math/FBoundingBox.h"


class FRenderingPipeline
{
public:
	explicit FRenderingPipeline(HWND hWindow);
    // Borrow an initialized device/target for offscreen rendering; caller owns its lifetime.
    explicit FRenderingPipeline(URenderer& Renderer);
	~FRenderingPipeline();


	/* Rendering functions */
    FRenderCollector BeginFrame(const FCamera& Camera, UAssetManager& AssetManager, const FViewport& viewport, const FMatrix &projection, const AActor* SelectedActor = nullptr);
    void Render(FRenderCollector& Collector);

	void Display();


	float GetPerspectiveRatio() const { return mProjectionRatio; }

	float GetGridWidth() const;
	void SetGridWidth(float width);

	URenderer* GetRenderer() const;

	void RenderLoadingScreen(UAssetManager& AssetManager);

	EViewModeIndex GetViewModeIndex() const { return mViewMode; }
    void SetViewModeIndex(EViewModeIndex Mode) { mViewMode = Mode; }
    void OnResize(UINT Width, UINT Height) { mRenderer->OnResize(Width, Height); }

	// Projection ratio smoothing
	void StartProjectionTransition(bool orthographic);
	bool IsOrthographicTarget() const;
	void UpdateProjectionTransition(float deltaTime);

	bool HasShowFlag(EEngineShowFlags Flag) const;
	uint32 GetShowFlags() const { return mShowFlags; }
	void SetShowFlag(EEngineShowFlags Flag, bool bEnable);
	void SetShowFlags(uint32 flags) { mShowFlags = flags; }
	void SetMeshTwoSided(bool bTwoSided);

private:
	URenderer* mRenderer;
    bool bOwnRenderer = false;
    void InitializePipelines();
    std::unique_ptr<FLineGraphicsPipeline> mLinePipeline;
    std::unique_ptr<FWorldAxisGraphicsPipeline> mWorldAxisPipeline;
    std::unique_ptr<FWorldGridGraphicsPipeline> mWorldGridPipeline;
    std::unique_ptr<FMeshGraphicsPipeline> mMeshPipeline;
    std::unique_ptr<FStencilMarkGraphicsPipeline> mStencilMarkPipeline;
    std::unique_ptr<FStencilOutlineGraphicsPipeline> mStencilOutlinePipeline;
    std::unique_ptr<FTextGraphicsPipeline> mTextPipeline;
    std::unique_ptr<FQuadGraphicsPipeline> mQuadPipeline;
    std::unique_ptr<FFullscreenGraphicsPipeline> mFullscreenPipeline;
    std::unique_ptr<FMeshGraphicsPipeline> mGizmoPipeline;




	float mProjectionRatio; // 0.0f ~ 1.0f, 0이면 직교, 1이면 원근, 그 사이면 혼합

	// Projection ratio smoothing
	float mProjectionStartRatio = 1.0f;
	float mProjectionTargetRatio = 1.0f;
	float mProjectionElapsed = 0.0f;
	float mProjectionDuration = 1.0f;
	bool mbProjectionTransitioning = false;

	// Grid spacing; the world-grid shader determines the visible extent.
	float mgridSpacing = 1.0f;

	EViewModeIndex mViewMode = EViewModeIndex::VMI_Lit;

	uint32 mShowFlags = ~0;

};
