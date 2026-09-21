#include "FQuadGraphicsPipeline.h"
#include "../Renderer.h"
#include "Core/AssetSystem/Asset/Texture2DAsset.h"
#include <algorithm>
#include <functional>

namespace
{
    struct FQuadConstants
    {
        FMatrix Model;
        FVector4 Color;
        FVector4 SubUV;
        FVector4 NextSubUV;
        float FrameBlend;
        int32 HasTexture;
        int32 GrayscaleMode;
        int32 Padding = 0;
    };
    static_assert(sizeof(FQuadConstants) == 128);
}

FQuadGraphicsPipeline::FQuadGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_NONE);
    SetShader("Assets/Shaders/Quad.hlsl");
    AddConstantBuffer<FQuadConstants>();
    AddConstantBuffer<FMatrix>();
    SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);
}

EQuadRenderPhase FQuadGraphicsPipeline::GetPhase(const FRenderQuadInfo& Info)
{
    // The component's depth settings select when its quad is drawn.
    if (!Info.EnableDepthTest) return EQuadRenderPhase::Overlay;
    return Info.EnableDepthWrite ? EQuadRenderPhase::Opaque : EQuadRenderPhase::Transparent;
}

void FQuadGraphicsPipeline::Draw(TArray<FRenderQuadInfo>& Infos, const FRenderView& View, EQuadRenderPhase Phase)
{
    BeginDraw();
    UpdateConstantBuffer(1, View.ViewProjection);
    if (Phase == EQuadRenderPhase::Transparent)
    {
        const auto Forward = View.Camera.GetForwardVector();
        std::stable_sort(Infos.begin(), Infos.end(), [&](const auto& A, const auto& B)
        {
            if (GetPhase(A) != EQuadRenderPhase::Transparent || GetPhase(B) != EQuadRenderPhase::Transparent)
                return GetPhase(A) < GetPhase(B);
            return FVector::dot(A.Model.GetTranslation() - View.Camera.Location, Forward) >
                FVector::dot(B.Model.GetTranslation() - View.Camera.Location, Forward);
        });
    }
    auto AddressMode = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(0);
    auto DrawInfo = [&](const FRenderQuadInfo& Info)
    {
        SetDepthStencilState(Info.EnableDepthTest, Info.EnableDepthWrite);
        SetBlendState(Info.BlendMode);
        if (AddressMode != Info.AddressMode)
        {
            SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, Info.AddressMode, Info.AddressMode);
            AddressMode = Info.AddressMode;
        }
        auto* SRV = Info.Texture ? Info.Texture->GetSRV().Get() : nullptr;
        SetShaderResource(0, SRV);
        D3D11_SHADER_RESOURCE_VIEW_DESC Desc{};
        if (SRV) SRV->GetDesc(&Desc);
        UpdateConstantBuffer(0, FQuadConstants{ Info.Model, Info.Color, Info.SubUV,
            Info.NextSubUV, Info.FrameBlend, SRV ? 1 : 0, Desc.Format == DXGI_FORMAT_R8_UNORM ? 1 : 0 });
        DrawProcedural(6);

    };
    if (Phase == EQuadRenderPhase::Opaque)
    {
        // Partition only reorderable opaque items into a suffix, then pop in state order.
        const auto First = std::stable_partition(Infos.begin(), Infos.end(), [](const FRenderQuadInfo& Info)
        {
            return GetPhase(Info) != EQuadRenderPhase::Opaque ||
                (Info.BlendMode != ERenderBlendMode::Opaque && Info.BlendMode != ERenderBlendMode::Masked);
        });
        const int32 Remaining = static_cast<int32>(First - Infos.begin());
        std::sort(First, Infos.end(), [](const FRenderQuadInfo& A, const FRenderQuadInfo& B)
        {
            if (A.BlendMode != B.BlendMode) return A.BlendMode > B.BlendMode;
            return std::less<UTexture2D*>{}(B.Texture, A.Texture);
        });
        while (Infos.Num() > Remaining)
        {
            DrawInfo(Infos[Infos.Num() - 1]);
            Infos.RemoveLast();
        }
    }
    // Draw in submission order and compact remaining phases without reordering them.
    int32 Remaining = 0;
    const int32 Count = Infos.Num();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        if (GetPhase(Infos[Index]) == Phase)
            DrawInfo(Infos[Index]);
        else
        {
            if (Remaining != Index) Infos[Remaining] = std::move(Infos[Index]);
            ++Remaining;
        }
    }
    if (Remaining < Count) Infos.RemoveAt(Remaining, Count - Remaining);
    SetShaderResource(0, nullptr);
}
