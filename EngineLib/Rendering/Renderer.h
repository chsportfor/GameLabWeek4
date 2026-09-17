#pragma once

#include <d3d11.h>
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

class URenderer
{
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
	ID3D11Buffer* CubeIndexBuffer = nullptr;     // 큐브 인덱스 저장
	ID3D11Buffer* SphereIndexBuffer = nullptr;
	UINT SphereIndexCount = 0;

	/* Particle */
	ID3D11SamplerState* ParticleSamplerState = nullptr;
	ID3D11Buffer* ParticleVertexBuffer = nullptr;
	ID3D11Buffer* ParticleIndexBuffer = nullptr;


	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT ViewportInfo;
	ID3D11InputLayout* SimpleInputLayout;
	ID3D11InputLayout* LineSimpleInputLayout;

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
	ID3D11SamplerState* LoadingScreenSampler = nullptr;

public:
	/* Create */
	void Create(HWND hWindow);

	// Create API for GraphicsManager
	void CreateSamplerState(ID3D11SamplerState** outSamplerState);
	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT ByteWidth);
	ID3D11Buffer* CreateVertexBuffer(const FVertexTextured* vertices, UINT ByteWidth);
	ID3D11Buffer* CreatePrimitiveIndexBuffer(const uint32* indices, UINT ByteWidth);

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
	void Prepare(bool bWireFrame);
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

	void RenderSimplePrimitive(ID3D11Buffer* pBuffer, UINT numVertices);
	void RenderTexturePrimitive(ID3D11Buffer* pBuffer, UINT numVertices,
		ID3D11ShaderResourceView* texture, ID3D11SamplerState* samplerState, ID3D11Buffer* indexBuffer = nullptr, UINT numIndices = 0);
	// Render Text in the FontTextureBuffer and FontIndexBuffer.
	// It doesn't recieve buffer parameters since it use the internal buffers.
	void RenderFontTexture(uint32 numCharacter);
	void RenderLines(const FVertexSimple* vertices, uint32 numVertices, const uint32* indices, uint32 numindices);
	void RenderHighlight(ID3D11Buffer* pBuffer, uint32 Num, FMatrix mViewProjectionMatrix, FMatrix OutlineMatrix, const FMatrix originalMatrix);
	void RenderUnicodeFontTexture(uint32 indexCount);
	void RenderParticle(ID3D11ShaderResourceView* texture);

	void SwapBuffer();


	//Initialize
	void ClearDepth();
	//=============================================
	//해상도 변경 시 호출
	//void OnResize(UINT Width, UINT Height);
	void OnResize(UINT width, UINT height, float viewportWidth, float viewportHeight);

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
};

