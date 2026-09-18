#include "FCircle2DGraphicsPipeline.h"
#include "../Renderer.h"

namespace
{
    struct FCircle2DConstants { FMatrix Projection; FVector4 Color; FVector2 Center; float Radius; float Padding[2]{}; };
}

FCircle2DGraphicsPipeline::FCircle2DGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_NONE);
    SetDepthStencilState(false, false);
    SetShader("Assets/Shaders/Circle2D.hlsl");
    AddConstantBuffer<FCircle2DConstants>();
}

void FCircle2DGraphicsPipeline::Draw(TArray<FRenderCircle2DInfo>& Infos, const FRenderView& View)
{
    BeginDraw();
    for (const FRenderCircle2DInfo& Info : Infos)
    {
        UpdateConstantBuffer(0, FCircle2DConstants{ View.Projection2D, Info.Color, Info.Center, Info.Radius });
        DrawProcedural(6);
    }
}
