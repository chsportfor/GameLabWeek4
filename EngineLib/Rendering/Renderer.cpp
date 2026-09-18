#include "Renderer.h"
#include <stdexcept>

void URenderer::Create(HWND hWindow)
{
	createDeviceAndSwapChain(hWindow);
	createFrameBuffer();

    Width = static_cast<UINT>(ViewportInfo.Width);
    Height = static_cast<UINT>(ViewportInfo.Height);
    createDepthStencilBuffer(Width, Height);
    SetViewport(0, 0, static_cast<float>(Width), static_cast<float>(Height));
}

void URenderer::createDeviceAndSwapChain(HWND hWindow)
{
	D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

	DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
	swapchaindesc.BufferDesc.Width = 0;
	swapchaindesc.BufferDesc.Height = 0;
	swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapchaindesc.SampleDesc.Count = 1;
	swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchaindesc.BufferCount = 2;
	swapchaindesc.OutputWindow = hWindow;
	swapchaindesc.Windowed = TRUE;
	swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	UINT createDeviceFlags = 0;

#if defined(_DEBUG)
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	const HRESULT Result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT | createDeviceFlags,
		featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
		&swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);

	if (FAILED(Result)) throw std::runtime_error("Create D3D11 device/swap chain failed");
	SwapChain->GetDesc(&swapchaindesc);

	ViewportInfo = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };
}

void URenderer::releaseDeviceAndSwapChain()
{
	if (DeviceContext)
	{
		DeviceContext->Flush();
	}

	if (SwapChain)
	{
		SwapChain->Release();
		SwapChain = nullptr;
	}

	if (Device)
	{
		Device->Release();
		Device = nullptr;
	}

	if (DeviceContext)
	{
		DeviceContext->Release();
		DeviceContext = nullptr;
	}
}

void URenderer::createFrameBuffer()
{
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

	D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
	framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
}

void URenderer::releaseFrameBuffer()
{
	if (FrameBuffer)
	{
		FrameBuffer->Release();
		FrameBuffer = nullptr;
	}

	if (FrameBufferRTV)
	{
		FrameBufferRTV->Release();
		FrameBufferRTV = nullptr;
	}
}

void URenderer::SwapBuffer()
{
    if (SwapChain) SwapChain->Present(1, 0);
}

void URenderer::Prepare()
{
    if (FrameBufferRTV) DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
    if (DepthStencilView) DeviceContext->ClearDepthStencilView(DepthStencilView,
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
    DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, DepthStencilView);
    DeviceContext->RSSetViewports(1, &ViewportInfo);
}

void URenderer::createDepthStencilBuffer(UINT width, UINT height)
{
	D3D11_TEXTURE2D_DESC desc = {};

	desc.Width = width;   // 백버퍼와 크기가 정확히 같아야 함
	desc.Height = height;

	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  // 깊이 24비트 + 스텐실 8비트
	desc.SampleDesc.Count = 1;                    // 스왑체인의 SampleDesc와 반드시 동일
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;    // 이 플래그가 없으면 DSV 생성 실패

	Device->CreateTexture2D(&desc, nullptr, &DepthStencilBuffer);

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvdesc = {};
	dsvdesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvdesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

	Device->CreateDepthStencilView(DepthStencilBuffer, &dsvdesc, &DepthStencilView);
}

void URenderer::releaseDepthStencilBuffer()
{
	if (DepthStencilView) { DepthStencilView->Release();   DepthStencilView = nullptr; }
	if (DepthStencilBuffer) { DepthStencilBuffer->Release(); DepthStencilBuffer = nullptr; }
}

void URenderer::OnResize(UINT width, UINT height)
{
    if (!SwapChain || width == 0 || height == 0 || (Width == width && Height == height)) return;
    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    releaseFrameBuffer();
    releaseDepthStencilBuffer();
    if (FAILED(SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) return;
    Width = width; Height = height;
    createFrameBuffer();
    createDepthStencilBuffer(width, height);
    SetViewport(0, 0, static_cast<float>(width), static_cast<float>(height));
}

void URenderer::ClearDepth()
{
    if (DepthStencilView) DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1, 0);
}

namespace
{
	UINT GetByteSizeFromFormat(DXGI_FORMAT Format)
	{
		switch (Format)
		{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return 16;
		case DXGI_FORMAT_R32G32B32_FLOAT:
			return 12;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			return 8;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			return 4;
		default:
			return 0; // Unknown format
		}
	}
}

Microsoft::WRL::ComPtr<ID3D11Buffer> URenderer::CreateIndexBuffer(const uint32* Indices, UINT Count)
{
	if (!Indices || Count == 0 || Count > UINT_MAX / sizeof(uint32)) return {};
	D3D11_BUFFER_DESC IndexBufferDesc = {};
	IndexBufferDesc.ByteWidth = Count * sizeof(uint32);
	IndexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IndexBufferSRD = { Indices };

	Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
	Device->CreateBuffer(&IndexBufferDesc, &IndexBufferSRD, IndexBuffer.GetAddressOf());

	return IndexBuffer;
}
Microsoft::WRL::ComPtr<ID3D11Texture2D> URenderer::CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData)
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;

	if (InitialData)
	{
		D3D11_SUBRESOURCE_DATA TextureData = {};
		TextureData.pSysMem = InitialData;
		TextureData.SysMemPitch = Desc.Width * GetByteSizeFromFormat(Desc.Format);

		Device->CreateTexture2D(&Desc, &TextureData, &Texture);
	}
	else
	{
		Device->CreateTexture2D(&Desc, nullptr, &Texture);
	}

	return Texture;
}
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> URenderer::CreateShaderResourceView(Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture, const D3D11_SHADER_RESOURCE_VIEW_DESC* Desc)
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
	Device->CreateShaderResourceView(Texture.Get(), Desc, &SRV);
	return SRV;
}
TSharedPtr<FRenderTarget2D> URenderer::CreateRenderTarget2D(uint32 Width, uint32 Height, DXGI_FORMAT Format)
{
	TSharedPtr<FRenderTarget2D> RenderTarget = MakeShared<FRenderTarget2D>();

	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = Width;
	TextureDesc.Height = Height;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = Format;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	RenderTarget->Texture = CreateTexture2D(TextureDesc);

	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	RTVDesc.Format = Format;
	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	Device->CreateRenderTargetView(RenderTarget->Texture.Get(), &RTVDesc, RenderTarget->RTV.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;
	Device->CreateShaderResourceView(RenderTarget->Texture.Get(), &SRVDesc, RenderTarget->SRV.GetAddressOf());

	RenderTarget->Width = Width;
	RenderTarget->Height = Height;

	return RenderTarget;
}
TSharedPtr<FDepthStencil> URenderer::CreateDepthStencil(uint32 Width, uint32 Height)
{
	TSharedPtr<FDepthStencil> DepthStencil = MakeShared<FDepthStencil>();

	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = Width;
	TextureDesc.Height = Height;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	DepthStencil->Texture = CreateTexture2D(TextureDesc);

	D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
	DsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DsvDesc.Texture2D.MipSlice = 0;
	Device->CreateDepthStencilView(DepthStencil->Texture.Get(), &DsvDesc, DepthStencil->DSV.GetAddressOf());

	DepthStencil->Width = Width;
	DepthStencil->Height = Height;

	return DepthStencil;
}
void URenderer::BindFrameBuffer()
{
	DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
	DeviceContext->RSSetViewports(1, &ViewportInfo);
}
void URenderer::BindRenderTarget(const TSharedPtr<FRenderTarget2D>& RenderTarget, const TSharedPtr<FDepthStencil>& DepthStencil, bool bClear)
{
	DeviceContext->OMSetRenderTargets(1, RenderTarget->RTV.GetAddressOf(), DepthStencil ? DepthStencil->DSV.Get() : nullptr);
	if (bClear)
	{
		DeviceContext->ClearRenderTargetView(RenderTarget->RTV.Get(), ClearColor);

		if (DepthStencil)
		{
			DeviceContext->ClearDepthStencilView(DepthStencil->DSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		}
	}

	D3D11_VIEWPORT Viewport = {};
	Viewport.TopLeftX = 0.0f;
	Viewport.TopLeftY = 0.0f;
	Viewport.Width = static_cast<float>(RenderTarget->Width);
	Viewport.Height = static_cast<float>(RenderTarget->Height);
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	DeviceContext->RSSetViewports(1, &Viewport);
}
void URenderer::SetViewport(float X, float Y, float InWidth, float InHeight)
{
    if (InWidth <= 0 || InHeight <= 0) return;
    ViewportInfo = { X, Y, InWidth, InHeight, 0, 1 };
    Projection2D = FMatrix::Ortho(0, InWidth, InHeight, 0, 0, 1);
    DeviceContext->RSSetViewports(1, &ViewportInfo);
}


void URenderer::Release()
{
    if (DeviceContext) DeviceContext->ClearState();
    for (auto& Pair : SamplerStatePool.SamplerStates) Pair.second->Release();
    SamplerStatePool.SamplerStates.Empty();
    for (auto& Pair : DepthStencilStatePool.DepthStencilStates) Pair.second->Release();
    DepthStencilStatePool.DepthStencilStates.Empty();
    for (auto& State : BlendStatePool.BlendStates) { if (State) State->Release(); State = nullptr; }
    releaseDepthStencilBuffer();
    releaseFrameBuffer();
    releaseDeviceAndSwapChain();
}
