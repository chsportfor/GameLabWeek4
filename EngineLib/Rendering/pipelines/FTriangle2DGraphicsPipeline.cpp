#include "FTriangle2DGraphicsPipeline.h"
#include "../Renderer.h"

namespace
{
    struct FTriangle2DConstants { FMatrix Projection; FVector4 Color; FVector2 Center; float Size; float Rotation; };
}

FTriangle2DGraphicsPipeline::FTriangle2DGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_NONE);
    SetDepthStencilState(false, false);
    SetShader("Shaders/Triangle2D.hlsl");
    AddConstantBuffer<FTriangle2DConstants>();
}

void FTriangle2DGraphicsPipeline::Draw(TArray<FRenderTriangle2DInfo>& Infos, const FRenderView& View)
{
    BeginDraw();
    for (const FRenderTriangle2DInfo& Info : Infos)
    {
        UpdateConstantBuffer(0, FTriangle2DConstants{ View.Projection2D, Info.Color, Info.Center, Info.Size, Info.Rotation - PI * 0.5f });
        DrawProcedural(3);
    }
}
