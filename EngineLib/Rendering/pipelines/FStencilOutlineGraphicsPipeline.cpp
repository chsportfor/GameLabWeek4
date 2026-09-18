#include "FStencilOutlineGraphicsPipeline.h"
#include "../Renderer.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"

#include "FMeshShaderConstants.h"

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

static FMatrix MakeOutlineTransform(const FRenderMeshInfo& RI, const FRenderView& View)
{

	const FBoundingBox& Bounds = RI.StaticMesh->GetLocalBoundingBox();
    const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
    const FVector HalfExtent = (Bounds.Max - Bounds.Min) * 0.5f;
	FMatrix worldTransformMatrix = RI.WorldTransformMatrix;

	// 화면에서 OUTLINE_PIXELS 만큼 보이려면 이 깊이에서 월드로 얼마여야 하는지 환산한다.
	// 깊이 d에서 뷰포트가 담는 월드 높이가 2*d*tan(fov/2) 이므로, 그걸 픽셀 수로 나누면 픽셀당 월드 크기다.
	const FVector ObjectLocation = worldTransformMatrix.TransformPosition(Center);
	const float Depth = FVector::dot(ObjectLocation - View.Camera.Location, View.Camera.GetForwardVector());
	const float TanHalfFov = tanf(FMath::DegreesToRadians(View.Camera.mFovDegree * 0.5f));
	const float effectiveDepth = FMath::Max(
		(1.0f - View.PerspectiveRatio) * View.Camera.mOrthoDistance + View.PerspectiveRatio * Depth
		, 0.01f);
	const float H = 2.0f * effectiveDepth * TanHalfFov;
	const float WorldThickness = OUTLINE_PIXELS * H / View.ViewportSize.y;

	// 축마다 월드 공간에서 WorldThickness 만큼만 자라도록 배율을 따로 구한다.
    const FVector WorldScale = worldTransformMatrix.GetScale();

	FVector OutlineScale = {
		GetOutlineAxisScale(HalfExtent.x * WorldScale.x, WorldThickness),
		GetOutlineAxisScale(HalfExtent.y * WorldScale.y, WorldThickness),
		GetOutlineAxisScale(HalfExtent.z * WorldScale.z, WorldThickness) };

	const FMatrix Outline = FMatrix::Translation(FVector(-Center.x, -Center.y, -Center.z))
		* FMatrix::Scale(OutlineScale)
		* FMatrix::Translation(Center)
		* worldTransformMatrix;

    return Outline;
}

FStencilOutlineGraphicsPipeline::FStencilOutlineGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_BACK);
    SetStencilState(false, false, D3D11_COMPARISON_NOT_EQUAL, D3D11_STENCIL_OP_KEEP, 1);
    SetShader("Assets/Shaders/Mesh.hlsl", true);
    AddConstantBuffer<FMeshShaderConstants>();
    AddConstantBuffer<FMatrix>();
    SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
}

void FStencilOutlineGraphicsPipeline::Draw(TArray<FRenderMeshInfo>& Infos, const FRenderView& View)
{
    BeginDraw();
    UpdateConstantBuffer(1, View.ViewProjection);
    for (const FRenderMeshInfo& Info : Infos)
    {
        if (!Info.StaticMesh) continue;
        const UStaticMeshAsset& Mesh = *Info.StaticMesh;
        const auto Vertices = Mesh.GetVertexBuffer();
        const auto Indices = Mesh.GetIndexBuffer();
        if (!Vertices) continue;
        UpdateConstantBuffer(0, FMeshShaderConstants{ MakeOutlineTransform(Info, View), FVector4(1, .6f, 0, 1), 0, 0 });
        DrawBuffers(Vertices.Get(), Mesh.GetVertexCount(), Indices.Get(), Indices ? Mesh.GetIndexCount() : 0);
    }
}
