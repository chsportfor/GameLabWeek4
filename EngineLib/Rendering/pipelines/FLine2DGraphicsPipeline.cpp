#include "FLine2DGraphicsPipeline.h"
#include "../Renderer.h"

namespace
{
    struct FLine2DConstants { FMatrix Projection; FVector4 Color; FVector2 Start; FVector2 End; float Thickness; float Padding[3]{}; };
}

FLine2DGraphicsPipeline::FLine2DGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_NONE);
    SetDepthStencilState(false, false);
    SetShader("Shaders/Line2D.hlsl");
    AddConstantBuffer<FLine2DConstants>();
}

void FLine2DGraphicsPipeline::Draw(TArray<FRenderLine2DInfo>& Infos, const FRenderView& View)
{
    BeginDraw();
    for (const FRenderLine2DInfo& Info : Infos)
    {
        UpdateConstantBuffer(0, FLine2DConstants{ View.Projection2D, Info.Color, Info.Start, Info.End, Info.Thickness });
        DrawProcedural(6);
    }
}
