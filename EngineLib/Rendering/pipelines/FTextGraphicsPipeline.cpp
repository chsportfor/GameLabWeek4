#include "FTextGraphicsPipeline.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Rendering/TextMesh.h"
namespace
{
    struct FTextConstants
    {
        FVector Location; float Pad0 = 0;
        FVector Scale; float Pad1 = 0;
        FVector CameraRight; float Pad2 = 0;
        FVector CameraUp; float Pad3 = 0;
        FMatrix ViewProjection;
        FLinearColor Tint;
    };
    struct FAtlasConstants { float DistanceRange; int32 MSDF; float Padding[2]{}; };
}
namespace
{
    template<typename T>
    bool UploadTextBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context,
        Microsoft::WRL::ComPtr<ID3D11Buffer>& Buffer, UINT BindFlags, const TArray<T>& Data)
    {
        if (Data.Num() == 0 || Data.Num() > UINT_MAX / sizeof(T)) return false;
        const UINT Bytes = static_cast<UINT>(Data.Num() * sizeof(T));
        D3D11_BUFFER_DESC Desc{};
        if (Buffer) Buffer->GetDesc(&Desc);
        if (!Buffer || Desc.ByteWidth < Bytes)
        {
            Desc = {};
            Desc.ByteWidth = Bytes;
            Desc.Usage = D3D11_USAGE_DYNAMIC;
            Desc.BindFlags = BindFlags;
            Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            Microsoft::WRL::ComPtr<ID3D11Buffer> NewBuffer;
            if (FAILED(Device->CreateBuffer(&Desc, nullptr, &NewBuffer))) return false;
            Buffer = std::move(NewBuffer);
        }
        D3D11_MAPPED_SUBRESOURCE Mapped{};
        if (FAILED(Context->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped))) return false;
        std::memcpy(Mapped.pData, Data.GetData(), Bytes);
        Context->Unmap(Buffer.Get(), 0);
        return true;
    }
}

FTextGraphicsPipeline::FTextGraphicsPipeline(URenderer& Renderer) : FGraphicsPipeline(Renderer)
{
    SetRasterizerState(D3D11_CULL_BACK);
    SetDepthStencilState(true, true);
    SetShader("Shaders/Text.hlsl", true);
    AddConstantBuffer<FTextConstants>();
    AddConstantBuffer<FAtlasConstants>();
    SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP);
}
void FTextGraphicsPipeline::Draw(TArray<FRenderTextInfo>& Infos, const FRenderView& View)
{
    BeginDraw();
    for (const auto& Info : Infos)
    {
        if (!Info.Textmesh || !Info.FontAtlas) continue;
        const auto& Mesh = *Info.Textmesh;
        const auto& Atlas = *Info.FontAtlas;
        if (Mesh.Indices.IsEmpty()) continue;
        if (!Atlas.GetSRV() || (Mesh.FontRenderMode == EFontRenderMode::MSDF) != Atlas.IsMSDF()) continue;
        bool ValidIndices = true;
        for (uint32 Index : Mesh.Indices)
            if (Index >= Mesh.Vertices.Num()) { ValidIndices = false; break; }
        if (!ValidIndices) continue;
        if (!UploadTextBuffer(Device, Context, Vertices, D3D11_BIND_VERTEX_BUFFER, Mesh.Vertices) ||
            !UploadTextBuffer(Device, Context, Indices, D3D11_BIND_INDEX_BUFFER, Mesh.Indices)) continue;
        SetBlendState(Atlas.IsMSDF() ? ERenderBlendMode::Transparent : ERenderBlendMode::Additive);
        UpdateConstantBuffer(0, FTextConstants{Info.Location, 0, Info.Scale, 0,
            View.Camera.GetRightVector(), 0, View.Camera.GetUpVector(), 0, View.ViewProjection, Info.Color});
        UpdateConstantBuffer(1, FAtlasConstants{Atlas.GetDistanceRange(), Atlas.IsMSDF() ? 1 : 0});
        SetShaderResource(0, Atlas.GetSRV().Get());
        DrawBuffers(Vertices.Get(), Mesh.Vertices.Num(), Indices.Get(), Mesh.Indices.Num());
    }
    SetShaderResource(0, nullptr);
}
