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

void FWorldGridGraphicsPipeline::Draw(TArray<FRenderWorldGridInfo>& Infos, const FRenderView& View)
{
    BeginDraw();
    for (const FRenderWorldGridInfo& Info : Infos)
    {
        UpdateConstantBuffer(0, FWorldGridConstants{ View.ViewProjection, View.Camera.Location, Info.GridGap });
        DrawProcedural(6);
    }
}
