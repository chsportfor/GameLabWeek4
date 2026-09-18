#pragma once

#include <array>
#include <bitset>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <vector>
#include <d3d11.h>
#include <wrl/client.h>

#include "Core/Core.h"
#include "RenderInfo.h"
#include "RenderView.h"
#include "Core/enum.h"

class URenderer;

// Shared DX11 resource/configuration machinery, not a render-pass interface.
// Passes receive a typed submission array and the shared frame view.
class FGraphicsPipeline
{
public:
    virtual ~FGraphicsPipeline() = default;
    FGraphicsPipeline(const FGraphicsPipeline&) = delete;
    FGraphicsPipeline& operator=(const FGraphicsPipeline&) = delete;

protected:
    explicit FGraphicsPipeline(URenderer& InRenderer);

    void SetRasterizerState(D3D11_CULL_MODE CullMode, int32 DepthBias = 0,
        std::initializer_list<EViewModeIndex> ViewModes = { EViewModeIndex::VMI_Lit });
    void SetDepthStencilState(bool EnableTest, bool EnableWrite);
    void SetStencilState(bool EnableTest, bool EnableWrite,
        D3D11_COMPARISON_FUNC Func, D3D11_STENCIL_OP PassOp, uint32 Ref);
    void SetBlendState(ERenderBlendMode Mode);
    void SetShader(const FString& Path, bool HasVertexInput = false);
    void SetShaderResource(uint32 Slot, ID3D11ShaderResourceView* SRV);
    void SetSamplerState(uint32 Slot, D3D11_FILTER Filter,
        D3D11_TEXTURE_ADDRESS_MODE AddressU, D3D11_TEXTURE_ADDRESS_MODE AddressV);

    template <typename T>
    void AddConstantBuffer()
    {
        static_assert(sizeof(T) <= D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16);
        if (ConstantBuffers.size() >= D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT)
            throw std::out_of_range("Too many constant buffers");
        D3D11_BUFFER_DESC Desc{};
        Desc.ByteWidth = static_cast<UINT>((sizeof(T) + 15u) & ~size_t(15u));
        Desc.Usage = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
        Check(Device->CreateBuffer(&Desc, nullptr, Buffer.GetAddressOf()), "Create constant buffer");
        ConstantBuffers.push_back(std::move(Buffer));
    }

    template <typename T>
    void UpdateConstantBuffer(uint32 Slot, const T& Data)
    {
        auto* Buffer = ConstantBuffers.at(Slot).Get();
        D3D11_BUFFER_DESC Desc{};
        Buffer->GetDesc(&Desc);
        if (sizeof(T) > Desc.ByteWidth)
            throw std::out_of_range("Constant buffer data is too large");
        D3D11_MAPPED_SUBRESOURCE Mapped{};
        Check(Context->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped), "Map constant buffer");
        std::memset(Mapped.pData, 0, Desc.ByteWidth);
        std::memcpy(Mapped.pData, &Data, sizeof(T));
        Context->Unmap(Buffer, 0);
    }

    // A Draw call is the cache boundary: other pipelines and ImGui may change the context.
    void BeginDraw() { Bound = false; BuffersBound = false; }
    void Bind() const;
    void BindBuffers(ID3D11Buffer* Vertices, ID3D11Buffer* Indices) const;
    void DrawProcedural(UINT VertexCount) const;
    void DrawBuffers(ID3D11Buffer* Vertices, UINT VertexCount,
        ID3D11Buffer* Indices, UINT IndexCount, UINT FirstIndex = 0) const;
    static void Check(HRESULT Result, const char* Operation);

    URenderer& Renderer;
    ID3D11Device* Device;
    ID3D11DeviceContext* Context;

private:
    static constexpr size_t ViewModeCount = static_cast<size_t>(EViewModeIndex::VMI_Max);
    std::array<Microsoft::WRL::ComPtr<ID3D11RasterizerState>, ViewModeCount> RasterizerStates;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthStencilState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> BlendState;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
    std::vector<Microsoft::WRL::ComPtr<ID3D11Buffer>> ConstantBuffers;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> Resources;
    std::array<Microsoft::WRL::ComPtr<ID3D11SamplerState>, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> Samplers;
    mutable bool Bound = false;
    mutable bool DepthDirty = true;
    mutable bool BlendDirty = true;
    mutable std::bitset<D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> ResourceDirty;
    mutable bool BuffersBound = false;
    mutable ID3D11Buffer* BoundVertices = nullptr;
    mutable ID3D11Buffer* BoundIndices = nullptr;
    UINT StencilRef = 0;
    UINT Stride = 0;
};
