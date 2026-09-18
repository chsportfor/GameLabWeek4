#include "GraphicsPipeline.h"
#include "Renderer.h"
#include <d3dcompiler.h>
#include <cstddef>

using Microsoft::WRL::ComPtr;

FGraphicsPipeline::FGraphicsPipeline(URenderer& InRenderer)
    : Renderer(InRenderer), Device(InRenderer.GetDevice()), Context(InRenderer.GetDeviceContext())
{
    SetBlendState(ERenderBlendMode::Opaque);
}

void FGraphicsPipeline::Check(HRESULT Result, const char* Operation)
{
    if (FAILED(Result))
        throw std::runtime_error(std::string(Operation) + " failed (HRESULT " + std::to_string(Result) + ")");
}

void FGraphicsPipeline::SetRasterizerState(D3D11_CULL_MODE CullMode, int32 DepthBias,
    std::initializer_list<EViewModeIndex> ViewModes)
{
    Bound = false;
    for (auto& State : RasterizerStates) State.Reset();
    D3D11_RASTERIZER_DESC Desc{};
    Desc.CullMode = CullMode;
    Desc.DepthClipEnable = TRUE;
    Desc.DepthBias = DepthBias;
    Desc.SlopeScaledDepthBias = DepthBias ? 1.0f : 0.0f;
    for (EViewModeIndex Mode : ViewModes)
    {
        const size_t Index = static_cast<size_t>(Mode);
        if (Index >= ViewModeCount) continue;
        Desc.FillMode = Mode == EViewModeIndex::VMI_Wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
        Check(Device->CreateRasterizerState(&Desc, RasterizerStates[Index].GetAddressOf()), "Create rasterizer state");
    }
    auto& Fallback = RasterizerStates[static_cast<size_t>(EViewModeIndex::VMI_Lit)];
    if (!Fallback)
    {
        Desc.FillMode = D3D11_FILL_SOLID;
        Check(Device->CreateRasterizerState(&Desc, Fallback.GetAddressOf()), "Create fallback rasterizer state");
    }
}

void FGraphicsPipeline::SetDepthStencilState(bool EnableTest, bool EnableWrite)
{
    auto* State = Renderer.DepthStencilStatePool.GetOrCreateDepthStencilState(Device, { EnableTest, EnableWrite });
    DepthDirty = DepthDirty || DepthStencilState.Get() != State || StencilRef != 0;
    DepthStencilState = State;
    if (!DepthStencilState) throw std::runtime_error("Create depth stencil state failed");
    StencilRef = 0;
}

void FGraphicsPipeline::SetStencilState(bool EnableTest, bool EnableWrite,
    D3D11_COMPARISON_FUNC Func, D3D11_STENCIL_OP PassOp, uint32 Ref)
{
    Bound = false;
    DepthStencilState = Renderer.DepthStencilStatePool.GetOrCreateDepthStencilState(
        Device, { EnableTest, EnableWrite, true, Func, PassOp });
    if (!DepthStencilState) throw std::runtime_error("Create stencil state failed");
    StencilRef = Ref;
}

void FGraphicsPipeline::SetBlendState(ERenderBlendMode Mode)
{
    auto* State = Renderer.BlendStatePool.GetOrCreateBlendState(Device, Mode);
    BlendDirty = BlendDirty || BlendState.Get() != State;
    BlendState = State;
    if (!BlendState) throw std::runtime_error("Create blend state failed");
}

void FGraphicsPipeline::SetShader(const FString& Path, bool HasVertexInput)
{
    Bound = false;
    const std::wstring WidePath = Utf2Wide(Path);
    auto Compile = [&](const char* Entry, const char* Target)
    {
        ComPtr<ID3DBlob> Code, Errors;
        const HRESULT Result = D3DCompileFromFile(WidePath.c_str(), nullptr, nullptr,
            Entry, Target, 0, 0, Code.GetAddressOf(), Errors.GetAddressOf());
        if (FAILED(Result) && Errors)
            throw std::runtime_error(std::string(static_cast<const char*>(Errors->GetBufferPointer()), Errors->GetBufferSize()));
        Check(Result, "Compile shader");
        return Code;
    };
    auto VS = Compile("mainVS", "vs_5_0");
    auto PS = Compile("mainPS", "ps_5_0");
    Check(Device->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), nullptr,
        VertexShader.ReleaseAndGetAddressOf()), "Create vertex shader");
    Check(Device->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), nullptr,
        PixelShader.ReleaseAndGetAddressOf()), "Create pixel shader");
    InputLayout.Reset();
    Stride = 0;
    if (HasVertexInput)
    {
        const D3D11_INPUT_ELEMENT_DESC Layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FVertexSimple, r), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FVertexSimple, u), D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        Check(Device->CreateInputLayout(Layout, ARRAYSIZE(Layout), VS->GetBufferPointer(),
            VS->GetBufferSize(), InputLayout.GetAddressOf()), "Create input layout");
        Stride = sizeof(FVertexSimple);
    }
}

void FGraphicsPipeline::SetShaderResource(uint32 Slot, ID3D11ShaderResourceView* SRV)
{
    if (Resources.at(Slot).Get() != SRV)
    {
        Resources.at(Slot) = SRV;
        ResourceDirty.set(Slot);
    }
}

void FGraphicsPipeline::SetSamplerState(uint32 Slot, D3D11_FILTER Filter,
    D3D11_TEXTURE_ADDRESS_MODE AddressU, D3D11_TEXTURE_ADDRESS_MODE AddressV)
{
    Bound = false;
    Samplers.at(Slot) = Renderer.SamplerStatePool.GetOrCreateSamplerState(Device, { Filter, AddressU, AddressV });
    if (!Samplers.at(Slot)) throw std::runtime_error("Create sampler state failed");
}

void FGraphicsPipeline::Bind() const
{
    if (Bound)
    {
        if (DepthDirty) Context->OMSetDepthStencilState(DepthStencilState.Get(), StencilRef);
        if (BlendDirty) Context->OMSetBlendState(BlendState.Get(), nullptr, 0xffffffff);
        if (ResourceDirty.any())
        {
            for (size_t Slot = 0; Slot < Resources.size(); ++Slot)
            {
                if (!ResourceDirty.test(Slot)) continue;
                auto* SRV = Resources[Slot].Get();
                Context->VSSetShaderResources(static_cast<UINT>(Slot), 1, &SRV);
                Context->PSSetShaderResources(static_cast<UINT>(Slot), 1, &SRV);
            }
        }
        DepthDirty = BlendDirty = false;
        ResourceDirty.reset();
        return;
    }
    size_t Mode = static_cast<size_t>(Renderer.GetViewModeIndex());
    if (Mode >= ViewModeCount || !RasterizerStates[Mode]) Mode = static_cast<size_t>(EViewModeIndex::VMI_Lit);
    Context->RSSetState(RasterizerStates[Mode].Get());
    Context->OMSetDepthStencilState(DepthStencilState.Get(), StencilRef);
    Context->OMSetBlendState(BlendState.Get(), nullptr, 0xffffffff);
    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->IASetInputLayout(InputLayout.Get());
    Context->VSSetShader(VertexShader.Get(), nullptr, 0);
    Context->PSSetShader(PixelShader.Get(), nullptr, 0);

    // Apply empty slots as well: a zero-count Set call does not unbind old resources.
    std::array<ID3D11Buffer*, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> CBs{};
    std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> SRVs{};
    std::array<ID3D11SamplerState*, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> States{};
    for (size_t I = 0; I < ConstantBuffers.size(); ++I) CBs[I] = ConstantBuffers[I].Get();
    for (size_t I = 0; I < Resources.size(); ++I) SRVs[I] = Resources[I].Get();
    for (size_t I = 0; I < Samplers.size(); ++I) States[I] = Samplers[I].Get();
    Context->VSSetConstantBuffers(0, static_cast<UINT>(CBs.size()), CBs.data());
    Context->PSSetConstantBuffers(0, static_cast<UINT>(CBs.size()), CBs.data());
    Context->VSSetShaderResources(0, static_cast<UINT>(SRVs.size()), SRVs.data());
    Context->PSSetShaderResources(0, static_cast<UINT>(SRVs.size()), SRVs.data());
    Context->PSSetSamplers(0, static_cast<UINT>(States.size()), States.data());
    Bound = true;
    DepthDirty = BlendDirty = false;
    ResourceDirty.reset();
}

void FGraphicsPipeline::BindBuffers(ID3D11Buffer* Vertices, ID3D11Buffer* Indices) const
{
    UINT Offset = 0;
    UINT VertexStride = Vertices ? Stride : 0;
    if (!BuffersBound || BoundVertices != Vertices)
        Context->IASetVertexBuffers(0, 1, &Vertices, &VertexStride, &Offset);
    if (!BuffersBound || BoundIndices != Indices)
        Context->IASetIndexBuffer(Indices, DXGI_FORMAT_R32_UINT, 0);
    BoundVertices = Vertices;
    BoundIndices = Indices;
    BuffersBound = true;
}

void FGraphicsPipeline::DrawProcedural(UINT VertexCount) const
{
    Bind();
    BindBuffers(nullptr, nullptr);
    Context->Draw(VertexCount, 0);
}

void FGraphicsPipeline::DrawBuffers(ID3D11Buffer* Vertices, UINT VertexCount,
    ID3D11Buffer* Indices, UINT IndexCount) const
{
    if (!Vertices) return;
    Bind();
    BindBuffers(Vertices, Indices);
    if (Indices && IndexCount) Context->DrawIndexed(IndexCount, 0, 0);
    else Context->Draw(VertexCount, 0);
}
