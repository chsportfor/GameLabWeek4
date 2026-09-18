#include "RenderingPipeline.h"
#include "Renderer.h"
#include "Camera.h"
#include "Editor/Console.h"
#include "Core/Object/ObjectFactory.h"

// 선분 하나당 정점 2개. 축 6개 + 앞으로 붙을 그리드까지 감당할 만큼 잡아둔다


FRenderingPipeline::FRenderingPipeline(HWND hWindow) :
	mbPerspectiveProjection(true)
	, mProjectionRatio(1.0f)
{
	mRenderer = new URenderer;
	mRenderer->Create(hWindow);

	mAspect = mRenderer->GetWidth() / static_cast<float>(mRenderer->GetHeight());
	mSceneRenderTarget = mRenderer->CreateRenderTarget2D(mRenderer->GetWidth(), mRenderer->GetHeight(), DXGI_FORMAT_R8G8B8A8_UNORM);
	mSceneDepthStencil = mRenderer->CreateDepthStencil(mRenderer->GetWidth(), mRenderer->GetHeight());

	mLinePipeline = std::make_unique<FLineGraphicsPipeline>(*mRenderer);
	mMeshPipeline = std::make_unique<FMeshGraphicsPipeline>(*mRenderer);
	mQuadPipeline = std::make_unique<FQuadGraphicsPipeline>(*mRenderer);
	mStencilMarkPipeline = std::make_unique<FStencilMarkGraphicsPipeline>(*mRenderer);
	mStencilOutlinePipeline = std::make_unique<FStencilOutlineGraphicsPipeline>(*mRenderer);
	mLine2DPipeline = std::make_unique<FLine2DGraphicsPipeline>(*mRenderer);
	mCircle2DPipeline = std::make_unique<FCircle2DGraphicsPipeline>(*mRenderer);
	mTriangle2DPipeline = std::make_unique<FTriangle2DGraphicsPipeline>(*mRenderer);
	mWorldAxisPipeline = std::make_unique<FWorldAxisGraphicsPipeline>(*mRenderer);
	mWorldGridPipeline = std::make_unique<FWorldGridGraphicsPipeline>(*mRenderer);
}

FRenderingPipeline::~FRenderingPipeline()
{
	mLinePipeline.reset();
	mMeshPipeline.reset();
	mQuadPipeline.reset();
	mStencilMarkPipeline.reset();
	mStencilOutlinePipeline.reset();
	mLine2DPipeline.reset();
	mCircle2DPipeline.reset();
	mTriangle2DPipeline.reset();
	mWorldAxisPipeline.reset();
	mWorldGridPipeline.reset();

	mRenderer->Release();

	delete mRenderer;
}

void FRenderingPipeline::Prepare(const FCamera* mCamera, float viewportWidth, float viewportHeight)
{
	// Cache view and projection matrices for rendering
	const float nearZ = 0.1f;
	const float farZ = 2000.0f;

	float d = mCamera->mOrthoDistance;
	
	mAspect = viewportWidth / viewportHeight;

	FMatrix view = mCamera->GetViewMatrix();
	FMatrix projection_u_p = mCamera->GetUnifiedProjectionMatrix(mAspect, mCamera->mFovDegree, d, nearZ, farZ, 1.0f);
	FMatrix projection_u_o = mCamera->GetUnifiedProjectionMatrix(mAspect, mCamera->mFovDegree, d, nearZ, farZ, 0.0f);
	FMatrix projection_u = mCamera->GetUnifiedProjectionMatrix(mAspect, mCamera->mFovDegree, d, nearZ, farZ, mProjectionRatio);

	//mViewProjectionMatrix = view * mCamera->GetProjectionMatrix(mAspect, mCamera->mFovDegree, nearZ, farZ);
	mViewMatrix = view;
	mProjectionMatrix = projection_u;
	mViewProjectionMatrix = view * projection_u_p;

	// 뷰 모드를 렌더러에 전달한다. 각 파이프라인이 드로우마다 이 값을 보고
	// 솔리드/와이어프레임 래스터라이저를 고른다.
	mRenderer->SetViewModeIndex(mViewModeIndex);

	mRenderer->Prepare();

	float orthoHeight = mCamera->mOrthoHeight;
	float orthoWidth = orthoHeight * mAspect;
	//mViewOrthogonalProjectionMatrix = view * mCamera->GetOrthographicMatrix(orthoWidth, orthoHeight, nearZ, farZ);
	mViewOrthogonalProjectionMatrix = view * projection_u_o;
	mViewUnifiedProjectionMatrix = view * projection_u;

	// 하이라이트 두께를 화면 픽셀 기준으로 환산할 때 쓴다
	mCameraLocation = mCamera->Location;
	mCameraForward = mCamera->GetForwardVector();
	mCameraFovDegree = mCamera->mFovDegree;
	mCameraOrthoDistance = mCamera->mOrthoDistance;

	// 그리는 순서가 중요하다: 가까운 것을 먼저, 먼 것을 나중에.
	// 깊이 테스트가 켜져 있으면 나중에 그린 FarCube 가 깊이 비교에서 탈락해
	// NearCube(주황)가 앞에 남고, 꺼져 있으면 FarCube(파랑)가 그 위를 덮어쓴다.
	//mRenderer->UpdateConstantViewProjection(viewProjection);

	mRenderer->BindRenderTarget(mSceneRenderTarget, mSceneDepthStencil);
}

void FRenderingPipeline::Render()
{
    D3D11_VIEWPORT Viewport{};
    UINT ViewportCount = 1;
    mRenderer->GetDeviceContext()->RSGetViewports(&ViewportCount, &Viewport);
    const FVector2 ViewportSize(Viewport.Width, Viewport.Height);

    mLinePipeline->Draw(mRenderCollector.LineInfos, mViewUnifiedProjectionMatrix, ViewportSize);
    mMeshPipeline->Draw(mRenderCollector.RenderInfos, mViewUnifiedProjectionMatrix);
    mQuadPipeline->Draw(mRenderCollector.QuadInfos, EQuadRenderPhase::Opaque, mViewUnifiedProjectionMatrix);

    if (HasShowFlag(EEngineShowFlags::SF_Grid))
    {
        TArray<FWorldAxisGraphicsPipeline::FRenderInfo> Axes;
        Axes.Add({ FVector4(0, 0, 1, 1), FVector(0, 0, 1), 0.002f });
        TArray<FWorldGridGraphicsPipeline::FRenderInfo> Grids;
        Grids.Add({ static_cast<float>(GridGap) });
        mWorldAxisPipeline->Draw(Axes, mViewMatrix, mProjectionMatrix, ViewportSize);
        mWorldGridPipeline->Draw(Grids, mViewUnifiedProjectionMatrix, mCameraLocation);
    }

    mQuadPipeline->Draw(mRenderCollector.QuadInfos, EQuadRenderPhase::Transparent, mViewUnifiedProjectionMatrix);
    mQuadPipeline->Draw(mRenderCollector.QuadInfos, EQuadRenderPhase::Overlay, mViewUnifiedProjectionMatrix);
    mRenderCollector.Clear();
}

void FRenderingPipeline::DrawLine(const FVector& start, const FVector& end, const FVector4& color)
{
	// 월드 좌표 그대로 넣는다. 그래서 그릴 때 World 행렬이 단위행렬이다
	mLineVertices.Add({ start.x, start.y, start.z, color.x, color.y, color.z, color.w });
	mLineVertices.Add({ end.x,   end.y,   end.z,   color.x, color.y, color.z, color.w });
}

void FRenderingPipeline::FlushLines()
{
}

void FRenderingPipeline::Display()
{
	mRenderer->SwapBuffer();
}

void FRenderingPipeline::Update(float deltaTime)
{
}

bool FRenderingPipeline::IsPerspectiveProjection() const
{
	return mbPerspectiveProjection;
}

void FRenderingPipeline::SetPerspectiveProjection(bool bPerspectiveProjection)
{
	mbPerspectiveProjection = bPerspectiveProjection;
}

URenderer* FRenderingPipeline::GetRenderer() const
{
	assert(mRenderer != nullptr);

	return mRenderer;
}

void FRenderingPipeline::OnResize(UINT width, UINT height)
{
	if (width == 0 || height == 0)
	{
		return;
	}

	if (mSceneRenderTarget)
	{
		mSceneRenderTarget = mRenderer->CreateRenderTarget2D(width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
	}
	
	if (mSceneDepthStencil)
	{
		mSceneDepthStencil = mRenderer->CreateDepthStencil(width, height);
	}

	mRenderer->OnResize(width, height);
}

FVector FRenderingPipeline::GetPrimitiveCenter(EPrimitive type)
{
	switch (type)
	{
	case EPrimitive::EP_Sphere:	return FVector(0, 0, 0);
	case EPrimitive::EP_Cube:	return FVector(0, 0, 0);
	default:					return FVector(0, 0, 0);
	}
}

// 테두리가 화면에서 차지할 두께(픽셀). 물체 크기와 카메라 거리 어느 쪽에도 영향받지 않는다.
static constexpr float OUTLINE_PIXELS = 3.0f;

// 월드 공간 반지름이 worldHalfExtent인 축을 worldThickness 만큼 키우는 배율
static float GetOutlineAxisScale(float worldHalfExtent, float worldThickness)
{
	if (worldHalfExtent <= SMALL_NUMBER)
	{
		return 1.0f;   // 납작하게 눌린 축은 건드리지 않는다. 안 그러면 배율이 발산한다
	}

	return 1.0f + worldThickness / worldHalfExtent;
}

FVector FRenderingPipeline::GetPrimitiveHalfExtent(EPrimitive type)
{
	switch (type)
	{
	case EPrimitive::EP_Sphere:	return FVector(1.0f, 1.0f, 1.0f);
	case EPrimitive::EP_Cube:	return FVector(0.5f, 0.5f, 0.5f);
	default:					return FVector(0.5f, 0.5f, 0.5f);
	}
}

void FRenderingPipeline::RenderHighLight(const FRenderInfo& RI)
{
	if (!RI.StaticMesh)
	{
		return;
	}

	const FVector Center = GetPrimitiveCenter(RI.ePrimitive);
	const FVector HalfExtent = GetPrimitiveHalfExtent(RI.ePrimitive);

	// 화면에서 OUTLINE_PIXELS 만큼 보이려면 이 깊이에서 월드로 얼마여야 하는지 환산한다.
	// 깊이 d에서 뷰포트가 담는 월드 높이가 2*d*tan(fov/2) 이므로, 그걸 픽셀 수로 나누면 픽셀당 월드 크기다.
	const FVector ObjectLocation = RI.WorldTransformMatrix.TransformPosition(Center);
	const float Depth = FVector::dot(ObjectLocation - mCameraLocation, mCameraForward);
	const float TanHalfFov = tanf(FMath::DegreesToRadians(mCameraFovDegree * 0.5f));
	const float effectiveDepth = FMath::Max(
		(1.0f - mProjectionRatio) * mCameraOrthoDistance + mProjectionRatio * Depth
		, 0.01f);
	//const float H = mbPerspectiveProjection ? 2.0f * Depth * TanHalfFov : 5.774f;
	const float H = 2.0f * effectiveDepth * TanHalfFov;
	const float WorldThickness = OUTLINE_PIXELS * H / mRenderer->GetHeight();

	// 축마다 월드 공간에서 WorldThickness 만큼만 자라도록 배율을 따로 구한다.
	const FVector WorldScale(
		RI.WorldTransformMatrix.GetUnitAxis(EAxis::X).Length(),
		RI.WorldTransformMatrix.GetUnitAxis(EAxis::Y).Length(),
		RI.WorldTransformMatrix.GetUnitAxis(EAxis::Z).Length());

	FVector OutlineScale = {
		GetOutlineAxisScale(HalfExtent.x * WorldScale.x, WorldThickness),
		GetOutlineAxisScale(HalfExtent.y * WorldScale.y, WorldThickness),
		GetOutlineAxisScale(HalfExtent.z * WorldScale.z, WorldThickness) };

	const FMatrix Outline = FMatrix::Translation(FVector(-Center.x, -Center.y, -Center.z))
		* FMatrix::Scale(OutlineScale)
		* FMatrix::Translation(Center)
		* RI.WorldTransformMatrix;

    TArray<FStencilMarkGraphicsPipeline::FRenderInfo> MarkInfos;
    MarkInfos.Add(RI);
    TArray<FStencilOutlineGraphicsPipeline::FRenderInfo> OutlineInfos;
    FStencilOutlineGraphicsPipeline::FRenderInfo OutlineInfo = RI;
    OutlineInfo.WorldTransformMatrix = Outline;
    OutlineInfo.Color = FLinearColor(1.f, 0.6f, 0.f, 1.f);
    OutlineInfos.Add(OutlineInfo);
    mStencilMarkPipeline->Draw(MarkInfos, mViewUnifiedProjectionMatrix);
    mStencilOutlinePipeline->Draw(OutlineInfos, mViewUnifiedProjectionMatrix);
}

void FRenderingPipeline::StartProjectionTransition(bool orthographic)
{
	mProjectionStartRatio = mProjectionRatio;
	mProjectionTargetRatio = orthographic ? 0.0f : 1.0f;
	mProjectionElapsed = 0.0f;

	mbProjectionTransitioning = mProjectionStartRatio != mProjectionTargetRatio;
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

void FRenderingPipeline::SetGridGap(int32 GridGap)
{
	if (GridGap > 75000)
		GridGap = 100000;
	else if (GridGap > 30000)
		GridGap = 50000;
	else if (GridGap > 7500)
		GridGap = 10000;
	else if (GridGap > 3000)
		GridGap = 5000;
	else if (GridGap > 750)
		GridGap = 1000;
	else if (GridGap > 300)
		GridGap = 500;
	else if (GridGap > 75)
		GridGap = 100;
	else if (GridGap > 30)
		GridGap = 50;
	else if (GridGap > 7)
		GridGap = 10;
	else if (GridGap > 3)
		GridGap = 5;
	else
		GridGap = 1;
	this->GridGap = GridGap;
}
