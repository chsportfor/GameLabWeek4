#include "FQuadGraphicsPipeline.h"
#include "../Renderer.h"
#include <algorithm>
#include <functional>

namespace
{
    struct FQuadConstants { FMatrix Model; FVector4 Color; FVector4 SubUV; int32 HasTexture; int32 GrayscaleMode; int32 Padding[2]{}; };
}

FQuadGraphicsPipeline::FQuadGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_NONE);
    SetShader("Assets/Shaders/Quad.hlsl");
    AddConstantBuffer<FQuadConstants>();
    AddConstantBuffer<FMatrix>();
    SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
}

EQuadRenderPhase FQuadGraphicsPipeline::GetPhase(const FRenderInfo& Info)
{
    // Classification previously lived in FRenderCollector::AddQuadInfo.
    if (!Info.EnableDepthTest) return EQuadRenderPhase::Overlay;
    return Info.EnableDepthWrite ? EQuadRenderPhase::Opaque : EQuadRenderPhase::Transparent;
}

void FQuadGraphicsPipeline::Draw(TArray<FRenderInfo>& Infos,
    EQuadRenderPhase Phase, const FMatrix& ViewProjection)
{
    BeginDraw();
    UpdateConstantBuffer(1, ViewProjection);
    auto DrawInfo = [&](const FRenderInfo& Info)
    {
        SetDepthStencilState(Info.EnableDepthTest, Info.EnableDepthWrite);
        SetBlendState(Info.BlendMode);
        SetShaderResource(0, Info.TextureSRV.Get());
        D3D11_SHADER_RESOURCE_VIEW_DESC Desc{};
        if (Info.TextureSRV) Info.TextureSRV->GetDesc(&Desc);
        UpdateConstantBuffer(0, FQuadConstants{ Info.Model, Info.Color, Info.SubUV,
            Info.TextureSRV ? 1 : 0, Desc.Format == DXGI_FORMAT_R8_UNORM ? 1 : 0 });
        DrawProcedural(6);

    };
    if (Phase == EQuadRenderPhase::Opaque)
    {
        // Partition only reorderable opaque items into a suffix, then pop in state order.
        const auto First = std::partition(Infos.begin(), Infos.end(), [](const FRenderInfo& Info)
        {
            return GetPhase(Info) != EQuadRenderPhase::Opaque ||
                (Info.BlendMode != ERenderBlendMode::Opaque && Info.BlendMode != ERenderBlendMode::Masked);
        });
        const int32 Remaining = static_cast<int32>(First - Infos.begin());
        std::sort(First, Infos.end(), [](const FRenderInfo& A, const FRenderInfo& B)
        {
            if (A.BlendMode != B.BlendMode) return A.BlendMode > B.BlendMode;
            return std::less<ID3D11ShaderResourceView*>{}(B.TextureSRV.Get(), A.TextureSRV.Get());
        });
        while (Infos.Num() > Remaining)
        {
            DrawInfo(Infos[Infos.Num() - 1]);
            Infos.RemoveLast();
        }
    }
    // Transparent/overlay retain the existing traversal and swap-pop behavior.
    int32 Index = 0;
    while (Index < Infos.Num())
    {
        if (GetPhase(Infos[Index]) != Phase)
        {
            ++Index;
            continue;
        }
        DrawInfo(Infos[Index]);
        Infos.RemoveAtSwap(Index);
    }
    SetShaderResource(0, nullptr);
}
