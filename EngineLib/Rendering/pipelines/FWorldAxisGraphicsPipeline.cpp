#include "FWorldAxisGraphicsPipeline.h"
#include "../Renderer.h"

namespace
{
    struct FWorldAxisConstants { FMatrix View; FMatrix Projection; FVector4 Color; FVector Axis; float Thickness; FVector2 ViewportSize; float Padding[2]{}; };
}

FWorldAxisGraphicsPipeline::FWorldAxisGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_NONE);
    SetBlendState(ERenderBlendMode::Transparent);
    SetShader("Assets/Shaders/WorldAxis.hlsl");
    AddConstantBuffer<FWorldAxisConstants>();
}

void FWorldAxisGraphicsPipeline::Draw(TArray<FRenderInfo>& Infos,
    const FMatrix& View, const FMatrix& Projection, const FVector2& ViewportSize)
{
    BeginDraw();
    for (const FRenderInfo& Info : Infos)
    {
        UpdateConstantBuffer(0, FWorldAxisConstants{ View, Projection, Info.Color, Info.Axis, Info.Thickness, ViewportSize });
        DrawProcedural(6);
    }
}
