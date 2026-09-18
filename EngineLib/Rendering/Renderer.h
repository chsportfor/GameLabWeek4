#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include "Core/Container/TMap.h"
#include <d3dcompiler.h>

#include "Core/enum.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Vector.h"

#include "RenderInfo.h"
#include "VertexType.h"

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

// 선분 하나당 정점 2개. 축 6개 + 앞으로 붙을 그리드까지 감당할 만큼 잡아둔다
static constexpr uint32 LINE_VERTEX_CAPACITY = 8192;
static constexpr uint32 LINE_INDEX_CAPACITY = 16384;

struct FConstants
{
	FMatrix World; //Model
	FMatrix ViewProjection;
	FLinearColor Tint;          // rgb = 색, a = 섞는 비율
};

struct FTextureConstants
{
	FMatrix World; //Model
	FMatrix ViewProjection;
	FLinearColor Tint;          // rgb = 색, a = 섞는 비율
	FVector2 UVScale;       // 텍스처 좌표 스케일
	FVector2 UVOffset;      // 텍스처 좌표 오프셋
};

struct FBillboardConstants
{
	FVector3 Location;
	float Pad0 = 0;
	FVector3 Scale;
	float Pad1 = 0;

	FMatrix ViewProjection;
	FLinearColor Tint;

	FVector2 UVScale;
	FVector2 UVOffset;

	FVector3 CameraRight;
	float Pad2 = 0;
	FVector3 CameraUp;
	float Pad3 = 0;
};

struct alignas(16) FParticleConstants
{
	FVector3 Location;
	float pad0 = 0;
	FVector3 Scale;
	float pad1 = 0;

	FMatrix ViewProjection;

	FVector3 CameraRight;
	float pad2 = 0;
	FVector3 CameraUp;
	float pad3 = 0;

	FLinearColor Tint;
	
	int32 NumRows;
	int32 NumCols;
	int32 CurrentFrame;
	int32 NextFrame;

	float FrameRatio;
	float pad[3] = {};
};

// intancing 용
struct FInstanceData
{
	FMatrix World;
	FLinearColor Tint;
};

// HLSL의 b1에 전달할 데이터
struct FUnicodeFontConstants
{
	float DistanceRange = 4.0f;
	float Padding[3] = {};
};

struct FFontConstants
{
	FVector3 Location;
	float Pad0 = 0;
	FVector3 Scale;
	float Pad1 = 0;

	FVector3 CameraRight;
	float Pad2 = 0;
	FVector3 CameraUp;
	float Pad3 = 0;

	FMatrix ViewProjection;
	FLinearColor Tint;
};

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
		BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
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

class URenderer
{
	friend class FGraphicsPipeline;
public:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;
	ID3D11RasterizerState* RasterizerState[2] = {};
	ID3D11Buffer* ConstantBuffer[CBT_Count] = {};
	ID3D11Texture2D* DepthStencilBuffer = nullptr;			// 실제 깊이값이 저장될 메모리
	ID3D11DepthStencilView* DepthStencilView = nullptr;		// 그 메모리를 "출력 대상"으로 보는 뷰
	ID3D11DepthStencilState* DepthStencilState[4] = {};	// 깊이 테스트용 상태
	ID3D11BlendState* BlendState[4] = {}; // 블렌딩 상태
	ID3D11VertexShader* VertexShader[VST_Count] = {};
	ID3D11PixelShader* PixelShader[PST_Count] = {};

	// 기존의 ASCII 폰트
	ID3D11ShaderResourceView* FontAtlasShaderResoruceView = nullptr;
	ID3D11Buffer* FontTextureBuffer = nullptr; // TODO: Rename to FontVertexBuffer
	ID3D11InputLayout* FontInputLayout = nullptr;
	ID3D11SamplerState* FontSamplerState = nullptr;
	ID3D11Buffer* FontIndexBuffer = nullptr;

	// 유니코드 폰트
	ID3D11ShaderResourceView* UnicodeFontAtlasSRV = nullptr;
	ID3D11Buffer* UnicodeFontVertexBuffer = nullptr;
	ID3D11Buffer* UnicodeFontIndexBuffer = nullptr;
	ID3D11Buffer* UnicodeFontConstantBuffer = nullptr;
	uint32 UnicodeFontVertexCapacity = 0;
	uint32 UnicodeFontIndexCapacity = 0;

	ID3D11InputLayout* PrimitiveTextureLayout = nullptr;


	/* Particle */
	ID3D11SamplerState* ParticleSamplerState = nullptr;
	ID3D11Buffer* ParticleVertexBuffer = nullptr;
	ID3D11Buffer* ParticleIndexBuffer = nullptr;


	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT ViewportInfo;
	ID3D11InputLayout* SimpleInputLayout = nullptr;
	ID3D11InputLayout* LineSimpleInputLayout = nullptr;

	// 매 프레임 내용이 바뀌는 선분용. 메시 버퍼와 달리 IMMUTABLE이 아니라 DYNAMIC이다
	ID3D11Buffer* LineVertexBuffer = nullptr;
	uint32 LineVertexCapacity = 0;

	ID3D11Buffer* LineIndexBuffer = nullptr;
	uint32 LineIndexCapacity = 0;

	unsigned int StrideSimple;
	unsigned int StrideTextured;

	ID3D11VertexShader* LoadingScreenVertexShader = nullptr;
	ID3D11PixelShader* LoadingScreenPixelShader = nullptr;
	ID3D11InputLayout* LoadingScreenInputLayout = nullptr;
	ID3D11Buffer* LoadingScreenVertexBuffer = nullptr;
	ID3D11Buffer* LoadingScreenIndexBuffer = nullptr;
	ID3D11SamplerState* LoadingScreenSampler = nullptr;

public:
	/* Create */
	void Create(HWND hWindow);

	// Create API for GraphicsManager
	void CreateSamplerState(ID3D11SamplerState** outSamplerState);

	// 인스턴싱
	bool RenderSimpleInstanced(
		ID3D11Buffer* vertexBuffer,
		ID3D11Buffer* indexBuffer,
		UINT indexCount,
		const FInstanceData* instances,
		UINT instanceCount);
	// texturedPrimitive용
	//void RenderTexture(const FMatrix& world, const FMatrix& viewProjection);


	//void RenderTexturedPrimitive(ID3D11Buffer* vertexBuffer, UINT numVertices, ID3D11ShaderResourceView* textureSRV);

	// PNG·MSDF 셰이더·필요 리소스 준비
	bool InitializeUnicodeFont(const wchar_t* atlasPath, float distanceRange);

	bool LoadTexture(const wchar_t* texturePath, ID3D11ShaderResourceView** outSRV);

	void ReleasePrimitiveTextureResources(
		ID3D11ShaderResourceView* textureSRV, ID3D11SamplerState* samplerState);

	// Release all resources that this render holds.
	void Release();

	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);


	// Gloabal prepare method
	void Prepare();
	void PrepareForUI();

	/* Prepare methods for each rendering type */
	void PrepareSimplePrimitive();
	void PrepareSimpleInstanced();
	void PrepareTexturedPrimitive();
	void PrepareLine();
	void PrepareFont();
	void PrepareGizmo();
	void PrepareHighlight();
	// 셰이더, 입력 레이아웃, 블렌딩 상태 설정
	void PrepareUnicodeFont();
	void PrepareParticle();

	void UpdateSimpleConstant(FMatrix world, FMatrix viewProjection, FLinearColor tint = FLinearColor(0, 0, 0, 0));
	void UpdateTextureConstant(FMatrix world, FMatrix viewProjection, FLinearColor tint = FLinearColor(0, 0, 0, 0),
		FVector2 uvScale = { 1.0f, 1.0f }, FVector2 uvOffset = { 0.0f, 0.0f });
	void UpdateBillboardConstant(FVector3 location, FVector3 scale, FMatrix viewProjection,
		FVector3 cameraRight, FVector3 cameraUp,
		FLinearColor tint = FLinearColor(0, 0, 0, 0),
		FVector2 uvScale = { 1.0f, 1.0f }, FVector2 uvOffset = { 0.0f, 0.0f });
	void UpdateFontConstant(FVector3 location, FVector3 scale, FMatrix viewProjection,
		FVector3 cameraRight, FVector3 cameraUp,
		FLinearColor tint = FLinearColor(0, 0, 0, 0));
	void UpdateParticleConstant(FVector3 location, FVector3 scale, FMatrix viewProjection,
		FVector3 cameraRight, FVector3 cameraUp,
		int32 numRows = 1, int32 numCols = 1, int32 currentFrame = 0, int32 nextFrame = 0, float frameRatio = 0.0f,
		FLinearColor tint = FLinearColor(0, 0, 0, 0)
	);

	void UpdateFontBuffer(const TArray<FVertexTextured>& vertices, const TArray<uint32>& indices, uint32 numCharacter);
	bool UpdateUnicodeFontBuffer(const FTextMesh& textMesh);

	void UpdateBlendState(EBlendStateType blendStateType);

	void RenderSimplePrimitive(ID3D11Buffer* Vertices, ID3D11Buffer* Indices, UINT IndexCount);
	void RenderTexturePrimitive(ID3D11Buffer* pBuffer, UINT numVertices,
		ID3D11ShaderResourceView* texture, ID3D11SamplerState* samplerState, ID3D11Buffer* indexBuffer = nullptr, UINT numIndices = 0);
	// Render Text in the FontTextureBuffer and FontIndexBuffer.
	// It doesn't recieve buffer parameters since it use the internal buffers.
	void RenderFontTexture(uint32 numCharacter);
	void RenderLines(const FVertexSimple* vertices, uint32 numVertices, const uint32* indices, uint32 numindices);
	void RenderHighlight(ID3D11Buffer* pBuffer, ID3D11Buffer* Indices, uint32 Num, FMatrix mViewProjectionMatrix, FMatrix OutlineMatrix, const FMatrix originalMatrix);
	void RenderUnicodeFontTexture(uint32 indexCount);
	void RenderParticle(ID3D11ShaderResourceView* texture);

	void SwapBuffer();


	//Initialize
	void ClearDepth();
	//=============================================
	//해상도 변경 시 호출
	//void OnResize(UINT Width, UINT Height);
	void OnResize(UINT width, UINT height);
	void SetViewport(float X, float Y, float Width, float Height);

	void CreateLoadingScreenResources();
	void RenderFullscreenTexture(ID3D11ShaderResourceView* texture);

private:
	bool ensureFontIndexBuffer(UINT fontCount);
	UINT mTextVertexCapacity = 0; // 저장할 수 있는 최대 정점 수
	UINT mTextIndexCapacity = 0;

	bool ensureUnicodeFontIndexBuffer(UINT quadCount);


	ID3D11Buffer* InstanceBuffer = nullptr;
	UINT InstanceCapacity = 0;

	ID3D11InputLayout* InstancedInputLayout = nullptr;

	bool EnsureInstanceCapacity(UINT count);

	/* Internal global rendering state */
	bool mbWireFrame = false;

	/* Create methods for each resources*/
	void createDeviceAndSwapChain(HWND hWindow);
	void createShader();
	void createFrameBuffer();
	void createLineVertexBuffer(uint32 maxVertices);
	void createLineIndexBuffer(uint32 maxIndices);
	void createParticleVertexBuffer();
	void createParticleIndexBuffer();
	void createRasterizerState();
	void createConstantBuffer();
	void createDepthStencilBuffer(UINT width, UINT height);

	void createDepthStencilState();
	void createBlendState();
	bool createFontAtlasTexture();
	bool createFontSamplerState();
	bool createParticleStates();

	/* Prepare methods for each shader */
	void prepareSimpleShader();
	void prepareInstancedShader();
	void prepareTextureShader();
	void prepareBillboardTextureShader();
	void prepareLineShader();
	void prepareFontShader();
	void prepareUnicodeFontShader();
	void prepareParticleShader();

	/* Release methods for all resources */
	void releaseDeviceAndSwapChain();
	void releaseShader();
	void releaseFrameBuffer();
	void releaseLineVertexBuffer();
	void releaseLineIndexBuffer();
	void releaseFontBuffers();
	void releaseRasterizerState();
	void releaseConstantBuffer();
	void releaseDepthStencilBuffer();
	void releaseDepthStencilState();
	void releaseBlendState();
	void releaseFontTexture();
	void releaseFontAtlasTexture();
	void releaseUnicodeFontAtlasTexture();
	void releaseUnicodeFontBuffers();
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

private:
	FSamplerStatePool SamplerStatePool;
	FDepthStencilStatePool DepthStencilStatePool;
	FBlendStatePool BlendStatePool;
	UINT Width = 0, Height = 0;
	FMatrix Projection2D;
	EViewModeIndex ViewModeIndex = EViewModeIndex::VMI_Lit;
};


// WEEK4 renderer before WEEK3 migration (preserved for reference).
// #pragma once
//
// //#include <d3d11.h>
// //#include <d3dcompiler.h>
// //
// //#include "Core/enum.h"
// //#include "Core/Math/Matrix.h"
// //#include "Core/Math/Vector.h"
// //
// //#include "RenderInfo.h"
// //#include "VertexType.h"
// //
// //#pragma comment(lib, "user32")
// //#pragma comment(lib, "d3d11")
// //#pragma comment(lib, "d3dcompiler")
// //
// //// 선분 하나당 정점 2개. 축 6개 + 앞으로 붙을 그리드까지 감당할 만큼 잡아둔다
// //static constexpr uint32 LINE_VERTEX_CAPACITY = 8192;
// //static constexpr uint32 LINE_INDEX_CAPACITY = 16384;
// //
// //struct FConstants
// //{
// //	FMatrix World; //Model
// //	FMatrix ViewProjection;
// //	FLinearColor Tint;          // rgb = 색, a = 섞는 비율
// //};
// //
// //struct FTextureConstants
// //{
// //	FMatrix World; //Model
// //	FMatrix ViewProjection;
// //	FLinearColor Tint;          // rgb = 색, a = 섞는 비율
// //	FVector2 UVScale;       // 텍스처 좌표 스케일
// //	FVector2 UVOffset;      // 텍스처 좌표 오프셋
// //};
// //
// //struct FBillboardConstants
// //{
// //	FVector3 Location;
// //	float Pad0 = 0;
// //	FVector3 Scale;
// //	float Pad1 = 0;
// //
// //	FMatrix ViewProjection;
// //	FLinearColor Tint;
// //
// //	FVector2 UVScale;
// //	FVector2 UVOffset;
// //
// //	FVector3 CameraRight;
// //	float Pad2 = 0;
// //	FVector3 CameraUp;
// //	float Pad3 = 0;
// //};
// //
// //struct alignas(16) FParticleConstants
// //{
// //	FVector3 Location;
// //	float pad0 = 0;
// //	FVector3 Scale;
// //	float pad1 = 0;
// //
// //	FMatrix ViewProjection;
// //
// //	FVector3 CameraRight;
// //	float pad2 = 0;
// //	FVector3 CameraUp;
// //	float pad3 = 0;
// //
// //	FLinearColor Tint;
// //	
// //	int32 NumRows;
// //	int32 NumCols;
// //	int32 CurrentFrame;
// //	int32 NextFrame;
// //
// //	float FrameRatio;
// //	float pad[3] = {};
// //};
// //
// //// intancing 용
// //struct FInstanceData
// //{
// //	FMatrix World;
// //	FLinearColor Tint;
// //};
// //
// //// HLSL의 b1에 전달할 데이터
// //struct FUnicodeFontConstants
// //{
// //	float DistanceRange = 4.0f;
// //	float Padding[3] = {};
// //};
// //
// //struct FFontConstants
// //{
// //	FVector3 Location;
// //	float Pad0 = 0;
// //	FVector3 Scale;
// //	float Pad1 = 0;
// //
// //	FVector3 CameraRight;
// //	float Pad2 = 0;
// //	FVector3 CameraUp;
// //	float Pad3 = 0;
// //
// //	FMatrix ViewProjection;
// //	FLinearColor Tint;
// //};
// //
// //class URenderer
// //{
// //public:
// //	ID3D11Device* Device = nullptr;
// //	ID3D11DeviceContext* DeviceContext = nullptr;
// //	IDXGISwapChain* SwapChain = nullptr;
// //
// //	ID3D11Texture2D* FrameBuffer = nullptr;
// //	ID3D11RenderTargetView* FrameBufferRTV = nullptr;
// //	ID3D11RasterizerState* RasterizerState[2] = {};
// //	ID3D11Buffer* ConstantBuffer[CBT_Count] = {};
// //	ID3D11Texture2D* DepthStencilBuffer = nullptr;			// 실제 깊이값이 저장될 메모리
// //	ID3D11DepthStencilView* DepthStencilView = nullptr;		// 그 메모리를 "출력 대상"으로 보는 뷰
// //	ID3D11DepthStencilState* DepthStencilState[4] = {};	// 깊이 테스트용 상태
// //	ID3D11BlendState* BlendState[4] = {}; // 블렌딩 상태
// //	ID3D11VertexShader* VertexShader[VST_Count] = {};
// //	ID3D11PixelShader* PixelShader[PST_Count] = {};
// //
// //	// 기존의 ASCII 폰트
// //	ID3D11ShaderResourceView* FontAtlasShaderResoruceView = nullptr;
// //	ID3D11Buffer* FontTextureBuffer = nullptr; // TODO: Rename to FontVertexBuffer
// //	ID3D11InputLayout* FontInputLayout = nullptr;
// //	ID3D11SamplerState* FontSamplerState = nullptr;
// //	ID3D11Buffer* FontIndexBuffer = nullptr;
// //
// //	// 유니코드 폰트
// //	ID3D11ShaderResourceView* UnicodeFontAtlasSRV = nullptr;
// //	ID3D11Buffer* UnicodeFontVertexBuffer = nullptr;
// //	ID3D11Buffer* UnicodeFontIndexBuffer = nullptr;
// //	ID3D11Buffer* UnicodeFontConstantBuffer = nullptr;
// //	uint32 UnicodeFontVertexCapacity = 0;
// //	uint32 UnicodeFontIndexCapacity = 0;
// //
// //	ID3D11InputLayout* PrimitiveTextureLayout = nullptr;
// //	ID3D11Buffer* CubeIndexBuffer = nullptr;     // 큐브 인덱스 저장
// //	ID3D11Buffer* SphereIndexBuffer = nullptr;
// //	UINT SphereIndexCount = 0;
// //
// //	/* Particle */
// //	ID3D11SamplerState* ParticleSamplerState = nullptr;
// //	ID3D11Buffer* ParticleVertexBuffer = nullptr;
// //	ID3D11Buffer* ParticleIndexBuffer = nullptr;
// //
// //
// //	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
// //	D3D11_VIEWPORT ViewportInfo;
// //	ID3D11InputLayout* SimpleInputLayout;
// //	ID3D11InputLayout* LineSimpleInputLayout;
// //
// //	// 매 프레임 내용이 바뀌는 선분용. 메시 버퍼와 달리 IMMUTABLE이 아니라 DYNAMIC이다
// //	ID3D11Buffer* LineVertexBuffer = nullptr;
// //	uint32 LineVertexCapacity = 0;
// //
// //	ID3D11Buffer* LineIndexBuffer = nullptr;
// //	uint32 LineIndexCapacity = 0;
// //
// //	unsigned int StrideSimple;
// //	unsigned int StrideTextured;
// //
// //	ID3D11VertexShader* LoadingScreenVertexShader = nullptr;
// //	ID3D11PixelShader* LoadingScreenPixelShader = nullptr;
// //	ID3D11InputLayout* LoadingScreenInputLayout = nullptr;
// //	ID3D11Buffer* LoadingScreenVertexBuffer = nullptr;
// //	ID3D11SamplerState* LoadingScreenSampler = nullptr;
// //
// //public:
// //	/* Create */
// //	void Create(HWND hWindow);
// //
// //	// Create API for GraphicsManager
// //	void CreateSamplerState(ID3D11SamplerState** outSamplerState);
// //	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT ByteWidth);
// //	ID3D11Buffer* CreateVertexBuffer(const FVertexTextured* vertices, UINT ByteWidth);
// //	ID3D11Buffer* CreatePrimitiveIndexBuffer(const uint32* indices, UINT ByteWidth);
// //
// //	// 인스턴싱
// //	bool RenderSimpleInstanced(
// //		ID3D11Buffer* vertexBuffer,
// //		ID3D11Buffer* indexBuffer,
// //		UINT indexCount,
// //		const FInstanceData* instances,
// //		UINT instanceCount);
// //	// texturedPrimitive용
// //	//void RenderTexture(const FMatrix& world, const FMatrix& viewProjection);
// //
// //
// //	//void RenderTexturedPrimitive(ID3D11Buffer* vertexBuffer, UINT numVertices, ID3D11ShaderResourceView* textureSRV);
// //
// //	// PNG·MSDF 셰이더·필요 리소스 준비
// //	bool InitializeUnicodeFont(const wchar_t* atlasPath, float distanceRange);
// //
// //	bool LoadTexture(const wchar_t* texturePath, ID3D11ShaderResourceView** outSRV);
// //
// //	void ReleasePrimitiveTextureResources(
// //		ID3D11ShaderResourceView* textureSRV, ID3D11SamplerState* samplerState);
// //
// //	// Release all resources that this render holds.
// //	void Release();
// //
// //	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);
// //
// //
// //	// Gloabal prepare method
// //	void Prepare(bool bWireFrame);
// //	void PrepareForUI();
// //
// //	/* Prepare methods for each rendering type */
// //	void PrepareSimplePrimitive();
// //	void PrepareSimpleInstanced();
// //	void PrepareTexturedPrimitive();
// //	void PrepareLine();
// //	void PrepareFont();
// //	void PrepareGizmo();
// //	void PrepareHighlight();
// //	// 셰이더, 입력 레이아웃, 블렌딩 상태 설정
// //	void PrepareUnicodeFont();
// //	void PrepareParticle();
// //
// //	void UpdateSimpleConstant(FMatrix world, FMatrix viewProjection, FLinearColor tint = FLinearColor(0, 0, 0, 0));
// //	void UpdateTextureConstant(FMatrix world, FMatrix viewProjection, FLinearColor tint = FLinearColor(0, 0, 0, 0),
// //		FVector2 uvScale = { 1.0f, 1.0f }, FVector2 uvOffset = { 0.0f, 0.0f });
// //	void UpdateBillboardConstant(FVector3 location, FVector3 scale, FMatrix viewProjection,
// //		FVector3 cameraRight, FVector3 cameraUp,
// //		FLinearColor tint = FLinearColor(0, 0, 0, 0),
// //		FVector2 uvScale = { 1.0f, 1.0f }, FVector2 uvOffset = { 0.0f, 0.0f });
// //	void UpdateFontConstant(FVector3 location, FVector3 scale, FMatrix viewProjection,
// //		FVector3 cameraRight, FVector3 cameraUp,
// //		FLinearColor tint = FLinearColor(0, 0, 0, 0));
// //	void UpdateParticleConstant(FVector3 location, FVector3 scale, FMatrix viewProjection,
// //		FVector3 cameraRight, FVector3 cameraUp,
// //		int32 numRows = 1, int32 numCols = 1, int32 currentFrame = 0, int32 nextFrame = 0, float frameRatio = 0.0f,
// //		FLinearColor tint = FLinearColor(0, 0, 0, 0)
// //	);
// //
// //	void UpdateFontBuffer(const TArray<FVertexTextured>& vertices, const TArray<uint32>& indices, uint32 numCharacter);
// //	bool UpdateUnicodeFontBuffer(const FTextMesh& textMesh);
// //
// //	void UpdateBlendState(EBlendStateType blendStateType);
// //
// //	void RenderSimplePrimitive(ID3D11Buffer* pBuffer, UINT numVertices);
// //	void RenderTexturePrimitive(ID3D11Buffer* pBuffer, UINT numVertices,
// //		ID3D11ShaderResourceView* texture, ID3D11SamplerState* samplerState, ID3D11Buffer* indexBuffer = nullptr, UINT numIndices = 0);
// //	// Render Text in the FontTextureBuffer and FontIndexBuffer.
// //	// It doesn't recieve buffer parameters since it use the internal buffers.
// //	void RenderFontTexture(uint32 numCharacter);
// //	void RenderLines(const FVertexSimple* vertices, uint32 numVertices, const uint32* indices, uint32 numindices);
// //	void RenderHighlight(ID3D11Buffer* pBuffer, uint32 Num, FMatrix mViewProjectionMatrix, FMatrix OutlineMatrix, const FMatrix originalMatrix);
// //	void RenderUnicodeFontTexture(uint32 indexCount);
// //	void RenderParticle(ID3D11ShaderResourceView* texture);
// //
// //	void SwapBuffer();
// //
// //
// //	//Initialize
// //	void ClearDepth();
// //	//=============================================
// //	//해상도 변경 시 호출
// //	//void OnResize(UINT Width, UINT Height);
// //	void OnResize(UINT width, UINT height, float viewportWidth, float viewportHeight);
// //
// //	void CreateLoadingScreenResources();
// //	void RenderFullscreenTexture(ID3D11ShaderResourceView* texture);
// //
// //private:
// //	bool ensureFontIndexBuffer(UINT fontCount);
// //	UINT mTextVertexCapacity = 0; // 저장할 수 있는 최대 정점 수
// //	UINT mTextIndexCapacity = 0;
// //
// //	bool ensureUnicodeFontIndexBuffer(UINT quadCount);
// //
// //
// //	ID3D11Buffer* InstanceBuffer = nullptr;
// //	UINT InstanceCapacity = 0;
// //
// //	ID3D11InputLayout* InstancedInputLayout = nullptr;
// //
// //	bool EnsureInstanceCapacity(UINT count);
// //
// //	/* Internal global rendering state */
// //	bool mbWireFrame = false;
// //
// //	/* Create methods for each resources*/
// //	void createDeviceAndSwapChain(HWND hWindow);
// //	void createShader();
// //	void createFrameBuffer();
// //	void createLineVertexBuffer(uint32 maxVertices);
// //	void createLineIndexBuffer(uint32 maxIndices);
// //	void createParticleVertexBuffer();
// //	void createParticleIndexBuffer();
// //	void createRasterizerState();
// //	void createConstantBuffer();
// //	void createDepthStencilBuffer(UINT width, UINT height);
// //
// //	void createDepthStencilState();
// //	void createBlendState();
// //	bool createFontAtlasTexture();
// //	bool createFontSamplerState();
// //	bool createParticleStates();
// //
// //	/* Prepare methods for each shader */
// //	void prepareSimpleShader();
// //	void prepareInstancedShader();
// //	void prepareTextureShader();
// //	void prepareBillboardTextureShader();
// //	void prepareLineShader();
// //	void prepareFontShader();
// //	void prepareUnicodeFontShader();
// //	void prepareParticleShader();
// //
// //	/* Release methods for all resources */
// //	void releaseDeviceAndSwapChain();
// //	void releaseShader();
// //	void releaseFrameBuffer();
// //	void releaseLineVertexBuffer();
// //	void releaseLineIndexBuffer();
// //	void releaseFontBuffers();
// //	void releaseRasterizerState();
// //	void releaseConstantBuffer();
// //	void releaseDepthStencilBuffer();
// //	void releaseDepthStencilState();
// //	void releaseBlendState();
// //	void releaseFontTexture();
// //	void releaseFontAtlasTexture();
// //	void releaseUnicodeFontAtlasTexture();
// //	void releaseUnicodeFontBuffers();
// //};
// //
// #include "Core/Core.h"
// #include <d3d11.h>
// #include <d3dcompiler.h>
// #include <wrl/client.h>
// #include "Core/Math/Matrix.h"
// #include "Core/Math/Vector.h"
// #include "RenderInfo.h"
// #include "RenderPipeline.h"
//
//
// struct FGraphicsPipelineStateDesc {
//
// };
//
//
// struct FCameraConstants
// {
// 	FMatrix ViewProjectionMatrix;
// 	FVector2 ViewportSize;
// 	float Padding[2];
// };
//
// struct FConstants
// {
// 	FMatrix Matrix;
// 	FVector4 Color;
// 	int32 UseVertexColor;
// 	int32 HasTexture;
// 	int32 Padding[2];
// };
//
// struct FLine2DConstants
// {
// 	FMatrix Projection;
// 	FVector4 Color;
// 	FVector2 Start;
// 	FVector2 End;
// 	float Thickness;
// 	float Padding[3];
// };
//
// struct FCircle2DConstants
// {
// 	FMatrix Projection;
// 	FVector4 Color;
// 	FVector2 Center;
// 	float Radius;
// 	float Padding[2];
// };
//
// struct FTriangle2DConstants
// {
// 	FMatrix Projection;
// 	FVector4 Color;
// 	FVector2 Center;
// 	float Size;
// 	float Rotation;
// };
//
// struct FWorldAxisConstants
// {
// 	FMatrix View;
// 	FMatrix Projection;
// 	FVector4 Color;
// 	FVector Axis;
// 	float Thickness;
// 	FVector2 ViewportSize;
// 	float Padding[2] = {};
// };
//
// struct FWorldGridConstants
// {
// 	FMatrix ViewProjection;
// 	FVector CameraLocation;
// 	float GridGap = 1.0f;
// };
//
// struct FQuadConstants
// {
// 	FMatrix Model;
// 	FVector4 Color;
// 	FVector4 SubUV;
// 	int32 HasTexture;
// 	int32 GrayscaleMode;
// 	int32 Padding[2];
// };
//
// struct FSamplerStateKey
// {
// 	D3D11_FILTER Filter;
// 	D3D11_TEXTURE_ADDRESS_MODE AddressU;
// 	D3D11_TEXTURE_ADDRESS_MODE AddressV;
//
// 	bool operator==(const FSamplerStateKey& Other) const
// 	{
// 		return Filter == Other.Filter && AddressU == Other.AddressU && AddressV == Other.AddressV;
// 	}
// };
//
// struct FSamplerStateKeyHash
// {
// 	std::size_t operator()(const FSamplerStateKey& Key) const
// 	{
// 		return std::hash<int>()(static_cast<int>(Key.Filter)) ^ (std::hash<int>()(static_cast<int>(Key.AddressU)) << 1) ^ (std::hash<int>()(static_cast<int>(Key.AddressV)) << 2);
// 	}
// };
//
// class FSamplerStatePool
// {
// public:
// 	ID3D11SamplerState* GetOrCreateSamplerState(ID3D11Device* Device, const FSamplerStateKey& Key)
// 	{
// 		ID3D11SamplerState** existing = SamplerStates.Find(Key);
// 		if (existing)
// 		{
// 			return *existing;
// 		}
//
// 		D3D11_SAMPLER_DESC SamplerDesc = {};
// 		SamplerDesc.Filter = Key.Filter;
// 		SamplerDesc.AddressU = Key.AddressU;
// 		SamplerDesc.AddressV = Key.AddressV;
// 		SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
// 		SamplerDesc.MipLODBias = 0.0f;
// 		SamplerDesc.MaxAnisotropy = 1;
// 		SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
// 		SamplerDesc.BorderColor[0] = 0.0f;
// 		SamplerDesc.BorderColor[1] = 0.0f;
// 		SamplerDesc.BorderColor[2] = 0.0f;
// 		SamplerDesc.BorderColor[3] = 0.0f;
// 		SamplerDesc.MinLOD = 0.0f;
// 		SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
//
// 		ID3D11SamplerState* SamplerState = nullptr;
// 		HRESULT Hr = Device->CreateSamplerState(&SamplerDesc, &SamplerState);
// 		if (FAILED(Hr))
// 		{
// 			return nullptr;
// 		}
//
// 		SamplerStates.Add(Key, SamplerState);
//
// 		return SamplerState;
// 	}
//
// private:
// 	friend class URenderer;
//
// 	TMap<FSamplerStateKey, ID3D11SamplerState*, FSamplerStateKeyHash> SamplerStates;
// };
//
// struct FDepthStencilStateKey
// {
// 	bool bEnableDepthTest;
// 	bool bEnableDepthWrite;
// 	bool bEnableStencil = false;
// 	D3D11_COMPARISON_FUNC StencilFunc = D3D11_COMPARISON_ALWAYS;
// 	D3D11_STENCIL_OP StencilPassOp = D3D11_STENCIL_OP_KEEP;
//
// 	bool operator==(const FDepthStencilStateKey& Other) const
// 	{
// 		return bEnableDepthTest == Other.bEnableDepthTest
// 			&& bEnableDepthWrite == Other.bEnableDepthWrite
// 			&& bEnableStencil == Other.bEnableStencil
// 			&& StencilFunc == Other.StencilFunc
// 			&& StencilPassOp == Other.StencilPassOp;
// 	}
// };
//
// struct FDepthStencilStateKeyHash
// {
// 	std::size_t operator()(const FDepthStencilStateKey& Key) const
// 	{
// 		return std::hash<bool>()(Key.bEnableDepthTest)
// 			^ (std::hash<bool>()(Key.bEnableDepthWrite) << 1)
// 			^ (std::hash<bool>()(Key.bEnableStencil) << 2)
// 			^ (std::hash<int>()(static_cast<int>(Key.StencilFunc)) << 3)
// 			^ (std::hash<int>()(static_cast<int>(Key.StencilPassOp)) << 5);
// 	}
// };
//
// class FDepthStencilStatePool
// {
// public:
// 	ID3D11DepthStencilState* GetOrCreateDepthStencilState(ID3D11Device* Device, const FDepthStencilStateKey& Key)
// 	{
// 		ID3D11DepthStencilState** existing = DepthStencilStates.Find(Key);
// 		if (existing)
// 		{
// 			return *existing;
// 		}
//
// 		D3D11_DEPTH_STENCIL_DESC DepthStencilDesc = {};
// 		DepthStencilDesc.DepthEnable = Key.bEnableDepthTest ? TRUE : FALSE;
// 		DepthStencilDesc.DepthWriteMask = Key.bEnableDepthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
// 		DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
// 		DepthStencilDesc.StencilEnable = Key.bEnableStencil ? TRUE : FALSE;
// 		DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
// 		DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
//
// 		D3D11_DEPTH_STENCILOP_DESC StencilOpDesc = {};
// 		StencilOpDesc.StencilFailOp = D3D11_STENCIL_OP_KEEP;
// 		StencilOpDesc.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
// 		StencilOpDesc.StencilPassOp = Key.StencilPassOp;
// 		StencilOpDesc.StencilFunc = Key.StencilFunc;
//
// 		DepthStencilDesc.FrontFace = StencilOpDesc;
// 		DepthStencilDesc.BackFace = StencilOpDesc;
//
// 		ID3D11DepthStencilState* DepthStencilState = nullptr;
// 		HRESULT Hr = Device->CreateDepthStencilState(&DepthStencilDesc, &DepthStencilState);
// 		if (FAILED(Hr))
// 		{
// 			return nullptr;
// 		}
//
// 		DepthStencilStates.Add(Key, DepthStencilState);
//
// 		return DepthStencilState;
// 	}
//
// private:
// 	friend class URenderer;
//
// 	TMap<FDepthStencilStateKey, ID3D11DepthStencilState*, FDepthStencilStateKeyHash> DepthStencilStates;
// };
//
// class FBlendStatePool
// {
// public:
// 	FBlendStatePool()
// 	{
// 		for (int i = 0; i < static_cast<int>(ERenderBlendMode::Count); ++i)
// 		{
// 			BlendStates[i] = nullptr;
// 		}
// 	}
//
// 	ID3D11BlendState* GetOrCreateBlendState(ID3D11Device* Device, ERenderBlendMode BlendMode)
// 	{
// 		ID3D11BlendState* Result = BlendStates[static_cast<int>(BlendMode)];
// 		if (Result)
// 		{
// 			return Result;
// 		}
//
// 		CD3D11_BLEND_DESC BlendDesc = {};
// 		BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
// 		BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
// 		BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
// 		BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
//
// 		switch (BlendMode)
// 		{
// 		case ERenderBlendMode::Opaque:
// 		case ERenderBlendMode::Masked:
// 			BlendDesc.RenderTarget[0].BlendEnable = FALSE;
// 			break;
// 		case ERenderBlendMode::Transparent:
// 			BlendDesc.RenderTarget[0].BlendEnable = TRUE;
// 			BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
// 			BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
// 			BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
// 			break;
// 		case ERenderBlendMode::Additive:
// 			BlendDesc.RenderTarget[0].BlendEnable = TRUE;
// 			BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
// 			BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
// 			BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
// 			break;
// 		case ERenderBlendMode::NoColorWrite:
// 			BlendDesc.RenderTarget[0].BlendEnable = FALSE;
// 			BlendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
// 			break;
// 		}
//
// 		ID3D11BlendState* BlendState = nullptr;
// 		HRESULT Hr = Device->CreateBlendState(&BlendDesc, &BlendState);
// 		if (FAILED(Hr))
// 		{
// 			return nullptr;
// 		}
//
// 		BlendStates[static_cast<int>(BlendMode)] = BlendState;
//
// 		return BlendState;
// 	}
//
// private:
// 	friend class URenderer;
//
// 	ID3D11BlendState* BlendStates[static_cast<int>(ERenderBlendMode::Count)];
// };
//
// struct FRenderTarget2D
// {
// 	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
// 	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RTV;
// 	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
// 	UINT Width;
// 	UINT Height;
// };
//
// struct FDepthStencil
// {
// 	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
// 	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DSV;
// 	UINT Width;
// 	UINT Height;
// };
//
// struct FStructuredBuffer
// {
// 	ID3D11DeviceContext* DeviceContext;
//
// 	Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
// 	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
// 	UINT ElementSize;
// 	UINT ElementCount;
//
// 	void UpdateStructuredBuffer(const void* Data, uint32 DataCount)
// 	{
// 		D3D11_BOX Box = {};
// 		Box.left = 0;
// 		Box.right = DataCount * ElementSize;
// 		Box.top = 0;
// 		Box.bottom = 1;
// 		Box.front = 0;
// 		Box.back = 1;
//
// 		DeviceContext->UpdateSubresource(Buffer.Get(), 0, &Box, Data, 0, 0);
// 	}
// };
//
// class URenderer
// {
// public:
// 	//create
// 	void Create(HWND hWindow);
// 	void Release();
//
// #if 0
// 	void CreateLineVertexBuffer(uint32 maxVertices);
//
// 	void CreateStencilMarkState();
// 	void CreateStencilOutlineState();
// 	void CreateNoColorWriteBlendState();
//
// 	//release
// 	void ReleaseLineVertexBuffer();
// #endif
//
// 	template <typename T>
// 	Microsoft::WRL::ComPtr<ID3D11Buffer> CreateVertexBuffer(T* Vertices, UINT Count)
// 	{
// 		D3D11_BUFFER_DESC VertexBufferDesc = {};
// 		VertexBufferDesc.ByteWidth = sizeof(T) * Count;
// 		VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
// 		VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
//
// 		D3D11_SUBRESOURCE_DATA VertexBufferSRD = { Vertices };
//
// 		Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
// 		Device->CreateBuffer(&VertexBufferDesc, &VertexBufferSRD, VertexBuffer.GetAddressOf());
//
// 		return VertexBuffer;
// 	}
//
// 	Microsoft::WRL::ComPtr<ID3D11Buffer> CreateIndexBuffer(const uint32* Indices, UINT Count);
//
// 	Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData = nullptr);
// 	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateShaderResourceView(Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture, const D3D11_SHADER_RESOURCE_VIEW_DESC* Desc = nullptr);
//
// 	template <typename T>
// 	TSharedPtr<FStructuredBuffer> CreateStructuredBuffer(uint32 ElementCount)
// 	{
// 		TSharedPtr<FStructuredBuffer> StructuredBuffer = MakeShared<FStructuredBuffer>();
// 		StructuredBuffer->DeviceContext = DeviceContext;
//
// 		D3D11_BUFFER_DESC StructuredBufferDesc = {};
// 		StructuredBufferDesc.ByteWidth = sizeof(T) * ElementCount;
// 		StructuredBufferDesc.Usage = D3D11_USAGE_DEFAULT;
// 		StructuredBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
// 		StructuredBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
// 		StructuredBufferDesc.StructureByteStride = sizeof(T);
//
// 		Device->CreateBuffer(&StructuredBufferDesc, nullptr, StructuredBuffer->Buffer.GetAddressOf());
//
// 		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
// 		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
// 		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
// 		SRVDesc.Buffer.FirstElement = 0;
// 		SRVDesc.Buffer.NumElements = ElementCount;
//
// 		Device->CreateShaderResourceView(StructuredBuffer->Buffer.Get(), &SRVDesc, StructuredBuffer->SRV.GetAddressOf());
//
// 		StructuredBuffer->ElementSize = sizeof(T);
// 		StructuredBuffer->ElementCount = ElementCount;
//
// 		return StructuredBuffer;
// 	}
//
// 	TSharedPtr<FRenderTarget2D> CreateRenderTarget2D(uint32 Width, uint32 Height, DXGI_FORMAT Format);
// 	TSharedPtr<FDepthStencil> CreateDepthStencil(uint32 Width, uint32 Height);
//
// 	//Update
// 	void RSUpdateState();
//
// 	//Rendering
// 	void Prepare(const FMatrix& ViewProjectionMatrix);
// #if 0
// 	void RenderLines(const FVertexSimple* vertices, uint32 numVertices);
// #endif
//
// 	TSharedPtr<FRenderPipeline> CreateRenderPipeline();
//
// 	void BindPipeline(const TSharedPtr<FRenderPipeline>& Pipeline) const;
//
// 	void BindFrameBuffer();
// 	void BindRenderTarget(const TSharedPtr<FRenderTarget2D>& RenderTarget, const TSharedPtr<FDepthStencil>& DepthStencil, bool bClear = true);
//
// 	void RenderLines(const TArray<FRenderLineInfo>& Lines) const;
//
// 	void RenderHighlight(Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer, UINT NumVertices, Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer, UINT NumIndices, const FMatrix& Model, const FMatrix& OutlineModel, const FVector4& OutlineColor) const;
//
// 	void RenderQuad(const FRenderQuadInfo& Info) const;
//
// 	void RenderPrimitive(const TSharedPtr<FRenderPipeline>& Pipeline, Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer, UINT NumVertices) const;
// 	void RenderPrimitive(Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer, UINT NumVertices, const FMatrix& Model) const;
// 	void RenderPrimitive(Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer, UINT NumVertices, const FMatrix& Model, const FVector4& Color) const;
// 	void RenderPrimitiveIndexed(const TSharedPtr<FRenderPipeline>& Pipeline, Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer, Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer, UINT NumIndices) const;
// 	void RenderPrimitiveIndexed(Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer, Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer, UINT NumIndices, const FMatrix& Model) const;
//
// 	void RenderLine2D(const FVector2& Start, const FVector2& End, const FVector4& Color, float Thickness = 1.0f) const;
// 	void RenderCircle2D(const FVector2& Center, const FVector4& Color, float Radius = 1.0f) const;
// 	void RenderTriangle2D(const FVector2& Center, const FVector4& Color, float Size = 1.0f, float Rotation = 0.0f) const;
// 	// Thickness is the full world-space width, matching the grid's 0.001 half-width.
// 	void RenderWorldAxis(const FMatrix& View, const FMatrix& Projection, const FVector4& Color, const FVector& Axis, float Thickness = 0.002f) const;
// 	void RenderWorldGrid(const FMatrix& ViewProjection, const FVector& CameraLocation, float GridGap) const;
//
// 	void SwapBuffer();
//
// 	//=============================================
// 	//해상도 변경 시 호출
// 	//void OnResize(UINT Width, UINT Height);
// 	void OnResize(UINT width, UINT height);
//
// 	FORCEINLINE uint32 GetWidth() const { return Width; }
// 	FORCEINLINE uint32 GetHeight() const { return Height; }
// 	FORCEINLINE const D3D11_VIEWPORT& GetViewport() const { return ViewportInfo; }
// 	FORCEINLINE ID3D11Device* GetDevice() const { return Device; }
// 	FORCEINLINE ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext; }
// 	FORCEINLINE void SetViewModeIndex(EViewModeIndex InViewModeIndex) { ViewModeIndex = InViewModeIndex; }
//
// private:
// 	void CreateDeviceAndSwapChain(HWND hWindow);
// 	void ReleaseDeviceAndSwapChain();
//
// 	void CreateFrameBuffer();
// 	void ReleaseFrameBuffer();
//
// 	void CreateDepthStencilBuffer();
//
// private:
// 	ID3D11Device* Device = nullptr;
// 	ID3D11DeviceContext* DeviceContext = nullptr;
// 	IDXGISwapChain* SwapChain = nullptr;
//
// 	FSamplerStatePool SamplerStatePool;
// 	FDepthStencilStatePool DepthStencilStatePool;
// 	FBlendStatePool BlendStatePool;
//
// 	ID3D11Texture2D* FrameBuffer = nullptr;
// 	ID3D11RenderTargetView* FrameBufferRTV = nullptr;
//
// 	ID3D11Texture2D* DepthStencilBuffer = nullptr;			// 실제 깊이값이 저장될 메모리
// 	ID3D11DepthStencilView* DepthStencilView = nullptr;		// 그 메모리를 "출력 대상"으로 보는 뷰
//
// 	TSharedPtr<FStructuredBuffer> LineStructuredBuffer;
//
// 	TSharedPtr<FRenderPipeline> LinePipeline;
// 	TSharedPtr<FRenderPipeline> PrimitivePipeline;
// 	TSharedPtr<FRenderPipeline> StencilMarkPipeline;
// 	TSharedPtr<FRenderPipeline> StencilOutlinePipeline;
// 	TSharedPtr<FRenderPipeline> Line2DPipeline;
// 	TSharedPtr<FRenderPipeline> Circle2DPipeline;
// 	TSharedPtr<FRenderPipeline> Triangle2DPipeline;
// 	TSharedPtr<FRenderPipeline> WorldAxisPipeline;
// 	TSharedPtr<FRenderPipeline> WorldGridPipeline;
// 	TSharedPtr<FRenderPipeline> QuadPipeline;
//
// 	UINT Width, Height;
// 	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
// 	D3D11_VIEWPORT ViewportInfo;
// 	FMatrix Projection2D;
//
// 	// 와이어프레임 여부. Prepare에서 갱신하고 BindPipeline이 읽는다.
// 	// RSSetState는 드로우 직전마다 덮어써지므로 플래그로 들고 있어야 한다.
// 	EViewModeIndex ViewModeIndex = EViewModeIndex::VMI_Lit;
//
// #if 1
// 	ID3D11RasterizerState* RasterizerState[2] = {};
// 	ID3D11DepthStencilState* StencilMarkState = nullptr;	// 스텐실에 1 마킹용 상태
// 	ID3D11DepthStencilState* StencilOutlineState = nullptr; // 아웃라인 그리기용
// 	ID3D11BlendState* NoColorWriteBlendState = nullptr;		// 스텐실만 찍고 색은 쓰지 않는 상태
//
// 	// 매 프레임 내용이 바뀌는 선분용. 메시 버퍼와 달리 IMMUTABLE이 아니라 DYNAMIC이다
// 	ID3D11Buffer* LineVertexBuffer = nullptr;
// 	uint32 LineVertexCapacity = 0;
// #endif
// };
