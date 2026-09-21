#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "Core/Container/TMap.h"
#include "Core/Math/Matrix.h"
#include "RenderInfo.h"
#include "VertexType.h"
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

struct FSamplerStateKey
{
	D3D11_FILTER Filter;
	D3D11_TEXTURE_ADDRESS_MODE AddressU;
	D3D11_TEXTURE_ADDRESS_MODE AddressV;

	bool operator==(const FSamplerStateKey& Other) const
	{
		return Filter == Other.Filter && AddressU == Other.AddressU && AddressV == Other.AddressV;
	}
};

struct FSamplerStateKeyHash
{
	std::size_t operator()(const FSamplerStateKey& Key) const
	{
		return std::hash<int>()(static_cast<int>(Key.Filter)) ^ (std::hash<int>()(static_cast<int>(Key.AddressU)) << 1) ^ (std::hash<int>()(static_cast<int>(Key.AddressV)) << 2);
	}
};

class FSamplerStatePool
{
public:
	ID3D11SamplerState* GetOrCreateSamplerState(ID3D11Device* Device, const FSamplerStateKey& Key)
	{
		ID3D11SamplerState** existing = SamplerStates.Find(Key);
		if (existing)
		{
			return *existing;
		}

		D3D11_SAMPLER_DESC SamplerDesc = {};
		SamplerDesc.Filter = Key.Filter;
		SamplerDesc.AddressU = Key.AddressU;
		SamplerDesc.AddressV = Key.AddressV;
		SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		SamplerDesc.MipLODBias = 0.0f;
		SamplerDesc.MaxAnisotropy = 1;
		SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		SamplerDesc.BorderColor[0] = 0.0f;
		SamplerDesc.BorderColor[1] = 0.0f;
		SamplerDesc.BorderColor[2] = 0.0f;
		SamplerDesc.BorderColor[3] = 0.0f;
		SamplerDesc.MinLOD = 0.0f;
		SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		ID3D11SamplerState* SamplerState = nullptr;
		HRESULT Hr = Device->CreateSamplerState(&SamplerDesc, &SamplerState);
		if (FAILED(Hr))
		{
			return nullptr;
		}

		SamplerStates.Add(Key, SamplerState);

		return SamplerState;
	}

private:
	friend class URenderer;

	TMap<FSamplerStateKey, ID3D11SamplerState*, FSamplerStateKeyHash> SamplerStates;
};

struct FDepthStencilStateKey
{
	bool bEnableDepthTest;
	bool bEnableDepthWrite;
	bool bEnableStencil = false;
	D3D11_COMPARISON_FUNC StencilFunc = D3D11_COMPARISON_ALWAYS;
	D3D11_STENCIL_OP StencilPassOp = D3D11_STENCIL_OP_KEEP;

	bool operator==(const FDepthStencilStateKey& Other) const
	{
		return bEnableDepthTest == Other.bEnableDepthTest
			&& bEnableDepthWrite == Other.bEnableDepthWrite
			&& bEnableStencil == Other.bEnableStencil
			&& StencilFunc == Other.StencilFunc
			&& StencilPassOp == Other.StencilPassOp;
	}
};

struct FDepthStencilStateKeyHash
{
	std::size_t operator()(const FDepthStencilStateKey& Key) const
	{
		return std::hash<bool>()(Key.bEnableDepthTest)
			^ (std::hash<bool>()(Key.bEnableDepthWrite) << 1)
			^ (std::hash<bool>()(Key.bEnableStencil) << 2)
			^ (std::hash<int>()(static_cast<int>(Key.StencilFunc)) << 3)
			^ (std::hash<int>()(static_cast<int>(Key.StencilPassOp)) << 5);
	}
};

class FDepthStencilStatePool
{
public:
	ID3D11DepthStencilState* GetOrCreateDepthStencilState(ID3D11Device* Device, const FDepthStencilStateKey& Key)
	{
		ID3D11DepthStencilState** existing = DepthStencilStates.Find(Key);
		if (existing)
		{
			return *existing;
		}

		D3D11_DEPTH_STENCIL_DESC DepthStencilDesc = {};
		DepthStencilDesc.DepthEnable = Key.bEnableDepthTest ? TRUE : FALSE;
		DepthStencilDesc.DepthWriteMask = Key.bEnableDepthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
		DepthStencilDesc.StencilEnable = Key.bEnableStencil ? TRUE : FALSE;
		DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

		D3D11_DEPTH_STENCILOP_DESC StencilOpDesc = {};
		StencilOpDesc.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		StencilOpDesc.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		StencilOpDesc.StencilPassOp = Key.StencilPassOp;
		StencilOpDesc.StencilFunc = Key.StencilFunc;

		DepthStencilDesc.FrontFace = StencilOpDesc;
		DepthStencilDesc.BackFace = StencilOpDesc;

		ID3D11DepthStencilState* DepthStencilState = nullptr;
		HRESULT Hr = Device->CreateDepthStencilState(&DepthStencilDesc, &DepthStencilState);
		if (FAILED(Hr))
		{
			return nullptr;
		}

		DepthStencilStates.Add(Key, DepthStencilState);

		return DepthStencilState;
	}

private:

	friend class URenderer;

	TMap<FDepthStencilStateKey, ID3D11DepthStencilState*, FDepthStencilStateKeyHash> DepthStencilStates;
};

class FBlendStatePool
{
public:
	FBlendStatePool()
	{
		for (int i = 0; i < static_cast<int>(ERenderBlendMode::Count); ++i)
		{
			BlendStates[i] = nullptr;
		}
	}

	ID3D11BlendState* GetOrCreateBlendState(ID3D11Device* Device, ERenderBlendMode BlendMode)
	{
		ID3D11BlendState* Result = BlendStates[static_cast<int>(BlendMode)];
		if (Result)
		{
			return Result;
		}

		CD3D11_BLEND_DESC BlendDesc = {};
		BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		switch (BlendMode)
		{
		case ERenderBlendMode::Opaque:
		case ERenderBlendMode::Masked:
			BlendDesc.RenderTarget[0].BlendEnable = FALSE;
			break;
		case ERenderBlendMode::Transparent:
			BlendDesc.RenderTarget[0].BlendEnable = TRUE;
			BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			break;
		case ERenderBlendMode::Additive:
			BlendDesc.RenderTarget[0].BlendEnable = TRUE;
			BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
			BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			break;
		case ERenderBlendMode::NoColorWrite:
			BlendDesc.RenderTarget[0].BlendEnable = FALSE;
			BlendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
			break;
		}

		ID3D11BlendState* BlendState = nullptr;
		HRESULT Hr = Device->CreateBlendState(&BlendDesc, &BlendState);
		if (FAILED(Hr))
		{
			return nullptr;
		}

		BlendStates[static_cast<int>(BlendMode)] = BlendState;

		return BlendState;
	}

private:
	friend class URenderer;

	ID3D11BlendState* BlendStates[static_cast<int>(ERenderBlendMode::Count)];
};

struct FRenderTarget2D
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
	UINT Width;
	UINT Height;
};

struct FDepthStencil
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DSV;
	UINT Width;
	UINT Height;
};

struct FStructuredBuffer
{
	ID3D11DeviceContext* DeviceContext;

	Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
	UINT ElementSize;
	UINT ElementCount;

	void UpdateStructuredBuffer(const void* Data, uint32 DataCount)
	{
		D3D11_BOX Box = {};
		Box.left = 0;
		Box.right = DataCount * ElementSize;
		Box.top = 0;
		Box.bottom = 1;
		Box.front = 0;
		Box.back = 1;

		DeviceContext->UpdateSubresource(Buffer.Get(), 0, &Box, Data, 0, 0);
	}
};

struct FRenderStats
{
	float VSMemoryByte = 0.0f;
	float StaticmeshMemoryByte = 0.0f;
	float PSmemoryByte = 0.0f;

	int32 VSResourceCount = 0;
	int32 StaticmeshResourceCount = 0;
	int32 PSResourceCount = 0;
};

// Device, render targets and shared GPU resources. Passes own drawing state.
class URenderer
{
    friend class FGraphicsPipeline;
public:
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;
    ID3D11Texture2D* FrameBuffer = nullptr;
    ID3D11RenderTargetView* FrameBufferRTV = nullptr;
    ID3D11Texture2D* DepthStencilBuffer = nullptr;
    ID3D11DepthStencilView* DepthStencilView = nullptr;
    FLOAT ClearColor[4] = {0.025f, 0.025f, 0.025f, 1};
    D3D11_VIEWPORT ViewportInfo{};

    void Create(HWND Window);
    void Release();
	void PrepareFrame();
	void PrepareViewport(const D3D11_VIEWPORT& viewInfo);
    void SwapBuffer();
    void ClearDepth();
    void OnResize(UINT Width, UINT Height);
    void SetViewport(float X, float Y, float Width, float Height);
private:
    void createDeviceAndSwapChain(HWND Window);
    void releaseDeviceAndSwapChain();
    void createFrameBuffer();
    void releaseFrameBuffer();
    void createDepthStencilBuffer(UINT Width, UINT Height);
    void releaseDepthStencilBuffer();
public:
	template <typename T>
	Microsoft::WRL::ComPtr<ID3D11Buffer> CreateVertexBuffer(const T* Vertices, UINT Count)
	{
		if (!Vertices || Count == 0 || Count > UINT_MAX / sizeof(T)) return {};
		D3D11_BUFFER_DESC VertexBufferDesc = {};
		VertexBufferDesc.ByteWidth = sizeof(T) * Count;
		VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA VertexBufferSRD = { Vertices };

		Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
		Device->CreateBuffer(&VertexBufferDesc, &VertexBufferSRD, VertexBuffer.GetAddressOf());

		return VertexBuffer;
	}

	Microsoft::WRL::ComPtr<ID3D11Buffer> CreateIndexBuffer(const uint32* Indices, UINT Count);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData = nullptr);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateShaderResourceView(Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture, const D3D11_SHADER_RESOURCE_VIEW_DESC* Desc = nullptr);

	template <typename T>
	TSharedPtr<FStructuredBuffer> CreateStructuredBuffer(uint32 ElementCount)
	{
		TSharedPtr<FStructuredBuffer> StructuredBuffer = MakeShared<FStructuredBuffer>();
		StructuredBuffer->DeviceContext = DeviceContext;

		D3D11_BUFFER_DESC StructuredBufferDesc = {};
		StructuredBufferDesc.ByteWidth = sizeof(T) * ElementCount;
		StructuredBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		StructuredBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		StructuredBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		StructuredBufferDesc.StructureByteStride = sizeof(T);

		Device->CreateBuffer(&StructuredBufferDesc, nullptr, StructuredBuffer->Buffer.GetAddressOf());

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.FirstElement = 0;
		SRVDesc.Buffer.NumElements = ElementCount;

		Device->CreateShaderResourceView(StructuredBuffer->Buffer.Get(), &SRVDesc, StructuredBuffer->SRV.GetAddressOf());

		StructuredBuffer->ElementSize = sizeof(T);
		StructuredBuffer->ElementCount = ElementCount;

		return StructuredBuffer;
	}

	TSharedPtr<FRenderTarget2D> CreateRenderTarget2D(uint32 Width, uint32 Height, DXGI_FORMAT Format);
	TSharedPtr<FDepthStencil> CreateDepthStencil(uint32 Width, uint32 Height);


	void BindFrameBuffer();
	void BindRenderTarget(const TSharedPtr<FRenderTarget2D>& RenderTarget, const TSharedPtr<FDepthStencil>& DepthStencil, bool bClear = true);
	const FMatrix& GetProjection2D() const { return Projection2D; }
	EViewModeIndex GetViewModeIndex() const { return ViewModeIndex; }
	FORCEINLINE uint32 GetWidth() const { return Width; }
	FORCEINLINE uint32 GetHeight() const { return Height; }
	FORCEINLINE const D3D11_VIEWPORT& GetViewport() const { return ViewportInfo; }
	FORCEINLINE ID3D11Device* GetDevice() const { return Device; }
	FORCEINLINE ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext; }
	FORCEINLINE void SetViewModeIndex(EViewModeIndex InViewModeIndex) { ViewModeIndex = InViewModeIndex; }
	const FRenderStats& GetRenderStats() const { return RenderStats; }
	FRenderStats& GetMutableRenderStats() { return RenderStats; }
private:
	FRenderStats RenderStats;
	FSamplerStatePool SamplerStatePool;
	FDepthStencilStatePool DepthStencilStatePool;
	FBlendStatePool BlendStatePool;
	UINT Width = 0, Height = 0;
	FMatrix Projection2D;
	EViewModeIndex ViewModeIndex = EViewModeIndex::VMI_Lit;
};
