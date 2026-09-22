#include "RenderingPipeline.h"

#include <cmath>

#include "Core/enum.h"
#include "Editor/FViewport.h"

#include "Camera.h"
#include "Renderer.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Rendering/BuiltinAssetNames.h"

FRenderingPipeline::FRenderingPipeline(HWND Window)
    : mRenderer(new URenderer), bOwnRenderer(true), mProjectionRatio(1)
{
    try { mRenderer->Create(Window); InitializePipelines(); }
    catch (...) { mRenderer->Release(); delete mRenderer; throw; }
}

FRenderingPipeline::FRenderingPipeline(URenderer& Renderer)
    : mRenderer(&Renderer), mProjectionRatio(1)
{
    InitializePipelines();
}

void FRenderingPipeline::InitializePipelines()
{
    mLinePipeline = std::make_unique<FLineGraphicsPipeline>(*mRenderer);
    mWorldAxisPipeline = std::make_unique<FWorldAxisGraphicsPipeline>(*mRenderer);
    mWorldGridPipeline = std::make_unique<FWorldGridGraphicsPipeline>(*mRenderer);
    mMeshPipeline = std::make_unique<FMeshGraphicsPipeline>(*mRenderer);
    mStencilMarkPipeline = std::make_unique<FStencilMarkGraphicsPipeline>(*mRenderer);
    mStencilOutlinePipeline = std::make_unique<FStencilOutlineGraphicsPipeline>(*mRenderer);
    mTextPipeline = std::make_unique<FTextGraphicsPipeline>(*mRenderer);
    mQuadPipeline = std::make_unique<FQuadGraphicsPipeline>(*mRenderer);
    mFullscreenPipeline = std::make_unique<FFullscreenGraphicsPipeline>(*mRenderer);
    mGizmoPipeline = std::make_unique<FMeshGraphicsPipeline>(*mRenderer, true);
}

FRenderingPipeline::~FRenderingPipeline()
{
    mLinePipeline.reset();
    mWorldAxisPipeline.reset();
    mWorldGridPipeline.reset();
    mMeshPipeline.reset();
    mStencilMarkPipeline.reset();
    mStencilOutlinePipeline.reset();
    mTextPipeline.reset();
    mQuadPipeline.reset();
    mFullscreenPipeline.reset();
    mGizmoPipeline.reset();
    if (bOwnRenderer) { mRenderer->Release(); delete mRenderer; }
}

FRenderCollector FRenderingPipeline::BeginFrame(const FCamera& Camera, UAssetManager& AssetManager, const FViewport &viewport, const FMatrix& projection, const AActor* SelectedActor)
{
    mRenderer->SetViewModeIndex(mViewMode);

    FRenderCollector Collector;
    Collector.AssetManager = &AssetManager;
    Collector.SelectedActor = SelectedActor;
    Collector.ShowFlags = mShowFlags;

    auto& View = Collector.View;
    View.Camera = Camera;
    View.PerspectiveRatio = mProjectionRatio;
    View.ViewportSize = FVector2(viewport.GetViewport().Width, viewport.GetViewport().Height);
    View.Projection2D = mRenderer->GetProjection2D();
    View.View = Camera.GetViewMatrix();
	View.Projection = projection;
    View.ViewProjection = View.View * View.Projection;
    View.Frustum = FFrustum::FrustumFromViewProjection(View.ViewProjection);

    const bool ShowGrid = Collector.HasShowFlag(EEngineShowFlags::SF_Grid);
    if (Collector.HasShowFlag(EEngineShowFlags::SF_WorldAxis))
    {
        // WEEK3's grid shader already supplies the X/Y axes on its plane.
        if (!ShowGrid)
        {
            Collector.WorldAxisInfos.Add({FVector4(1, 0, 0, 1), FVector(1, 0, 0), .002f});
            Collector.WorldAxisInfos.Add({FVector4(0, 1, 0, 1), FVector(0, 1, 0), .002f});
        }
        Collector.WorldAxisInfos.Add({FVector4(0, 0, 1, 1), FVector(0, 0, 1), .002f});
    }
    if (ShowGrid) Collector.WorldGridInfos.Add({mgridSpacing});
    return Collector;
}

void FRenderingPipeline::Render(FRenderCollector& Collector)
{
    const auto& View = Collector.View;
    mMeshPipeline->Draw(Collector.StaticMeshInfos, View);
	mMeshPipeline->Draw(Collector.MeshInfos, View);
    mQuadPipeline->Draw(Collector.QuadInfos, View, EQuadRenderPhase::Opaque);
    mTextPipeline->Draw(Collector.TextInfos, View);
    mLinePipeline->Draw(Collector.LineInfos, View);
    mWorldAxisPipeline->Draw(Collector.WorldAxisInfos, View);
    mWorldGridPipeline->Draw(Collector.WorldGridInfos, View);
    mQuadPipeline->Draw(Collector.QuadInfos, View, EQuadRenderPhase::Transparent);
    mStencilMarkPipeline->Draw(Collector.SelectionInfos, View);
    mStencilOutlinePipeline->Draw(Collector.SelectionInfos, View);
    mQuadPipeline->Draw(Collector.QuadInfos, View, EQuadRenderPhase::Overlay);
    mRenderer->ClearDepth();
    mGizmoPipeline->Draw(Collector.GizmoInfos, View);
}

void FRenderingPipeline::RenderLoadingScreen(UAssetManager& AssetManager)
{
    const auto Mesh = AssetManager.GetAssetAs<UStaticMeshAsset>(BuiltinAssetNames::FullscreenMesh, true);
    const auto Texture = AssetManager.GetAssetAs<UTexture2D>(BuiltinAssetNames::LoadingScreen, true);
    if (!Mesh || !Texture) return;
    mRenderer->PrepareFrame();
    mRenderer->PrepareViewport(mRenderer->GetViewport());
    TArray<FRenderFullscreenInfo> Infos{{Mesh, Texture}};
    mFullscreenPipeline->Draw(Infos);
}

void FRenderingPipeline::Display()
{
    mRenderer->SwapBuffer();
}

URenderer* FRenderingPipeline::GetRenderer() const
{
	assert(mRenderer != nullptr);

	return mRenderer;
}

float FRenderingPipeline::GetGridWidth() const
{
	return mgridSpacing;
}

void  FRenderingPipeline::SetGridWidth(float width)
{
	if (std::isfinite(width) && width > 0) mgridSpacing = width;
}

void FRenderingPipeline::StartProjectionTransition(bool orthographic)
{
	mProjectionStartRatio = mProjectionRatio;
	mProjectionTargetRatio = orthographic ? 0.0f : 1.0f;
	mProjectionElapsed = 0.0f;

	mbProjectionTransitioning =
		mProjectionStartRatio != mProjectionTargetRatio;
}

bool FRenderingPipeline::IsOrthographicTarget() const
{
	return mProjectionTargetRatio == 0.0f;
}

void FRenderingPipeline::UpdateProjectionTransition(float deltaTime)
{
	if (!mbProjectionTransitioning)
	{
		return;
	}

	mProjectionElapsed += deltaTime;

	const float u = FMath::Clamp(
		mProjectionElapsed / mProjectionDuration, 0.0f, 1.0f);

	// Smoothstep interpolation for a smoother transition
	const float blend = u * u * (3.0f - 2.0f * u);

	mProjectionRatio = mProjectionStartRatio + (mProjectionTargetRatio - mProjectionStartRatio) * blend;

	if (u >= 1.0f)
	{
		mProjectionRatio = mProjectionTargetRatio;
		mbProjectionTransitioning = false;
	}
}

bool FRenderingPipeline::HasShowFlag(EEngineShowFlags Flag) const
{
	const uint32 FlagValue = static_cast<uint32>(Flag);
	return (mShowFlags & FlagValue) != 0;
}

void FRenderingPipeline::SetShowFlag(EEngineShowFlags Flag, bool bEnable)
{
	const uint32 FlagValue = static_cast<uint32>(Flag);

	if (bEnable)
	{
		mShowFlags |= FlagValue;
	}
	else
	{
		mShowFlags &= ~FlagValue;
	}
}
