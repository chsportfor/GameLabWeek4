#include "FWorldGridGraphicsPipeline.h"
#include "../Renderer.h"

namespace
{
    struct FWorldGridConstants { FMatrix ViewProjection; FVector CameraLocation; float GridGap; };
}

FWorldGridGraphicsPipeline::FWorldGridGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_NONE);
    SetDepthStencilState(true, true);
    SetBlendState(ERenderBlendMode::Transparent);
    SetShader("Assets/Shaders/WorldGrid.hlsl");
    AddConstantBuffer<FWorldGridConstants>();
}

void FWorldGridGraphicsPipeline::Draw(TArray<FRenderInfo>& Infos,
    const FMatrix& ViewProjection, const FVector& CameraLocation)
{
    BeginDraw();
    for (const FRenderInfo& Info : Infos)
    {
        UpdateConstantBuffer(0, FWorldGridConstants{ ViewProjection, CameraLocation, Info.GridGap });
        DrawProcedural(6);
    }
}
