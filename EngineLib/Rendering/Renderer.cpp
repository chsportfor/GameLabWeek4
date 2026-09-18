#include "Renderer.h"

#include <cstddef>
#include <cmath>
#include <string>
#include <wrl/client.h>

#include "Core/Math/MathUtility.h"
#include "Editor/Console.h"
#include "Rendering/TextMesh.h"

#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#pragma comment(lib, "dxguid.lib")

void URenderer::Create(HWND hWindow)
{
	createDeviceAndSwapChain(hWindow);
	createFrameBuffer();

	InitializeDeviceResources();
    Width = static_cast<UINT>(ViewportInfo.Width);
    Height = static_cast<UINT>(ViewportInfo.Height);
    createDepthStencilBuffer(Width, Height);
    SetViewport(0, 0, static_cast<float>(Width), static_cast<float>(Height));
}

void URenderer::InitializeDeviceResources()
{
    assert(Device && DeviceContext && !ConstantBuffer[CBT_Simple]);
	/* Create states */
	createDepthStencilState();
	createRasterizerState();
	createBlendState();

	createShader();
	createConstantBuffer();
	createLineVertexBuffer(LINE_VERTEX_CAPACITY);
	createLineIndexBuffer(LINE_INDEX_CAPACITY);

	createFullscreenShader();
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

	D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT | createDeviceFlags,
		featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
		&swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);

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

// 인스턴스 사용하여 렌더링(텍스쳐 X)
bool URenderer::RenderSimpleInstanced(
    const UStaticMeshAsset& Mesh,
	const FInstanceData* instances,
	UINT instanceCount)
{
    auto* vertexBuffer = Mesh.GetVertexBuffer().Get();
    auto* indexBuffer = Mesh.GetIndexBuffer().Get();
    const UINT indexCount = Mesh.GetIndexCount();
	if (instanceCount == 0)
		return true;

	if (!DeviceContext ||
		!vertexBuffer ||
		!instances ||
		!indexBuffer ||
		indexCount == 0 ||
		!ConstantBuffer[CBT_Simple] ||
		!VertexShader[VST_Instanced] ||
		!InstancedInputLayout ||
		!PixelShader[PST_Simple])
	{
		return false;
	}

	if (!EnsureInstanceCapacity(instanceCount))
		return false;

	// CPU의 인스턴스 배열을 GPU 버퍼에 복사
	// Map / Unmap은 “CPU가 쓸 수 있게 잠깐 문 열어주는 것”
	D3D11_MAPPED_SUBRESOURCE mapped{};

	HRESULT hr = DeviceContext->Map(InstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	if (FAILED(hr))
	{
		return false;
	}

	std::memcpy(mapped.pData, instances, static_cast<size_t>(instanceCount) * sizeof(FInstanceData));
	DeviceContext->Unmap(InstanceBuffer, 0);

	const UINT vertexStride = sizeof(FVertexSimple);
	const UINT instanceStride = sizeof(FInstanceData);
	UINT offset = 0;

	ID3D11Buffer* vbs[2] = { vertexBuffer, InstanceBuffer }; // VertexBuffer와 InstanceBuffer 각각의 슬롯 0,1에 삽입
	UINT strides[2] = { vertexStride, instanceStride };
	UINT offsets[2] = { 0, 0 };
	DeviceContext->IASetVertexBuffers(0, 2, vbs, strides, offsets);

	// 인덱스 버퍼는 정점 버퍼 슬롯과 별도로 연결
	DeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// indexCount: 인스턴스 하나를 그리는 데 사용할 인덱스 개수
	DeviceContext->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);

	return true;

}

// 인스턴스 버퍼의 크기를 확인하고 필요하면 증가
bool URenderer::EnsureInstanceCapacity(UINT count)
{
	if (count == 0)
		return true;

	const UINT maxCount = 100000; // 10만개(임의로 정함)
	const UINT instanceSize = static_cast<UINT>(sizeof(FInstanceData));

	// 기존 버퍼가 충분하면 그대로 사용한다.
	if (InstanceBuffer && count <= InstanceCapacity)
		return true;

	// 최대용량 초과시
	if (count > maxCount)
		return false;

	if (!Device)
		return false;

	UINT newCapacity = InstanceCapacity > 0 ? InstanceCapacity : 256;

	while (newCapacity < count)
	{
		if (newCapacity > maxCount / 2)
		{
			newCapacity = count;
			break;
		}

		newCapacity *= 2;
	}

	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = newCapacity * instanceSize;
	desc.Usage = D3D11_USAGE_DYNAMIC; // 동적
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 인스턴스 버퍼도 일단 버텍스 버퍼
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	ID3D11Buffer* newBuffer = nullptr;

	HRESULT hr = Device->CreateBuffer(&desc, nullptr, &newBuffer);

	if (FAILED(hr))
	{
		return false;
	}

	// 새 버퍼 생성이 성공한 뒤 기존 버퍼를 해제한다.
	if (InstanceBuffer)
		InstanceBuffer->Release();

	InstanceBuffer = newBuffer;
	InstanceCapacity = newCapacity;

	return true;
}

// 선분은 매 프레임 내용이 바뀌므로 IMMUTABLE로는 만들 수 없다.
// DYNAMIC + CPU_ACCESS_WRITE 라야 Map으로 덮어쓸 수 있다. (상수 버퍼와 같은 조합)
void URenderer::createLineVertexBuffer(uint32 maxVertices)
{
	D3D11_BUFFER_DESC vertexbufferdesc = {};
	vertexbufferdesc.ByteWidth = maxVertices * sizeof(FVertexSimple);
	vertexbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (SUCCEEDED(Device->CreateBuffer(&vertexbufferdesc, nullptr, &LineVertexBuffer)))
	{
		LineVertexCapacity = maxVertices;
	}
}

void URenderer::releaseLineVertexBuffer()
{
	if (LineVertexBuffer)
	{
		LineVertexBuffer->Release();
		LineVertexBuffer = nullptr;
	}

	LineVertexCapacity = 0;
}

void URenderer::createLineIndexBuffer(uint32 maxIndices)
{
	D3D11_BUFFER_DESC indexbufferdesc = {};
	indexbufferdesc.ByteWidth = maxIndices * sizeof(uint32);
	indexbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
	indexbufferdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (SUCCEEDED(Device->CreateBuffer(&indexbufferdesc, nullptr, &LineIndexBuffer)))
	{
		LineIndexCapacity = maxIndices;
	}
}

void URenderer::releaseLineIndexBuffer()
{
	if (LineIndexBuffer)
	{
		LineIndexBuffer->Release();
		LineIndexBuffer = nullptr;
	}

	LineIndexCapacity = 0;
}

void URenderer::createRasterizerState()
{
	D3D11_RASTERIZER_DESC rasterizerdesc[2] = {};
	rasterizerdesc[0].FillMode = D3D11_FILL_SOLID;
	rasterizerdesc[0].CullMode = D3D11_CULL_BACK;
	rasterizerdesc[0].DepthClipEnable = TRUE;

	rasterizerdesc[1].FillMode = D3D11_FILL_WIREFRAME;
	rasterizerdesc[1].CullMode = D3D11_CULL_NONE;
	rasterizerdesc[1].DepthClipEnable = TRUE;

	Device->CreateRasterizerState(&rasterizerdesc[0], &RasterizerState[0]);
	Device->CreateRasterizerState(&rasterizerdesc[1], &RasterizerState[1]);
}

void URenderer::releaseRasterizerState()
{
	for (int i = 0; i < 2; ++i)
	{
		if (RasterizerState[i])
		{
			RasterizerState[i]->Release();
			RasterizerState[i] = nullptr;
		}
	}
}
void URenderer::Release()
{
    if (DeviceContext) DeviceContext->ClearState();
    auto ReleasePointer = [](auto*& Resource) { if (Resource) { Resource->Release(); Resource = nullptr; } };
    ReleasePointer(LoadingScreenInputLayout);
    ReleasePointer(LoadingScreenVertexShader);
    ReleasePointer(LoadingScreenPixelShader);
    ReleasePointer(InstanceBuffer);
    InstanceCapacity = 0;
    TextVertexBuffer.Reset();
    TextIndexBuffer.Reset();
    MSDFConstantBuffer.Reset();
    for (auto& Pair : SamplerStatePool.SamplerStates) Pair.second->Release();
    SamplerStatePool.SamplerStates.Empty();
    for (auto& Pair : DepthStencilStatePool.DepthStencilStates) Pair.second->Release();
    DepthStencilStatePool.DepthStencilStates.Empty();
    for (auto& State : BlendStatePool.BlendStates) ReleasePointer(State);
    releaseRasterizerState();
    releaseDepthStencilBuffer();
    releaseDepthStencilState();
    releaseBlendState();
    releaseFrameBuffer();
    releaseLineVertexBuffer();
    releaseLineIndexBuffer();
    releaseConstantBuffer();
    releaseShader();
    releaseDeviceAndSwapChain();
}

void URenderer::SwapBuffer()
{
	SwapChain->Present(1, 0);
}

void URenderer::createShader()
{
	ID3DBlob* vertexShaderCSO[VST_Count] = {};
	ID3DBlob* pixelShaderCSO[PST_Count] = {};

	D3DCompileFromFile(L"Shaders/ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0,
		&vertexShaderCSO[VST_Simple], nullptr);

	Device->CreateVertexShader(
		vertexShaderCSO[VST_Simple]->GetBufferPointer(),
		vertexShaderCSO[VST_Simple]->GetBufferSize(), nullptr,
		&VertexShader[VST_Simple]);

	D3DCompileFromFile(L"Shaders/ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0,
		&pixelShaderCSO[PST_Simple], nullptr);

	Device->CreatePixelShader(
		pixelShaderCSO[PST_Simple]->GetBufferPointer(),
		pixelShaderCSO[PST_Simple]->GetBufferSize(), nullptr,
		&PixelShader[PST_Simple]);

	D3DCompileFromFile(L"Shaders/ShaderLine.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0,
		&vertexShaderCSO[VST_Line], nullptr);

	Device->CreateVertexShader(
		vertexShaderCSO[VST_Line]->GetBufferPointer(),
		vertexShaderCSO[VST_Line]->GetBufferSize(), nullptr,
		&VertexShader[VST_Line]);

	D3DCompileFromFile(L"Shaders/ShaderLine.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0,
		&pixelShaderCSO[PST_Line], nullptr);

	Device->CreatePixelShader(
		pixelShaderCSO[PST_Line]->GetBufferPointer(),
		pixelShaderCSO[PST_Line]->GetBufferSize(), nullptr,
		&PixelShader[PST_Line]);

	D3DCompileFromFile(L"Shaders/ShaderTexture.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0,
		&vertexShaderCSO[VST_Texture], nullptr);

	Device->CreateVertexShader(
		vertexShaderCSO[VST_Texture]->GetBufferPointer(),
		vertexShaderCSO[VST_Texture]->GetBufferSize(), nullptr,
		&VertexShader[VST_Texture]);

	D3DCompileFromFile(L"Shaders/ShaderTexture.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0,
		&pixelShaderCSO[PST_Texture], nullptr);

	Device->CreatePixelShader(
		pixelShaderCSO[PST_Texture]->GetBufferPointer(),
		pixelShaderCSO[PST_Texture]->GetBufferSize(), nullptr,
		&PixelShader[PST_Texture]);

	// 인스턴싱
	D3DCompileFromFile(L"Shaders/ShaderW0.hlsl", nullptr, nullptr, "mainVSInstanced", "vs_5_0", 0, 0,
		&vertexShaderCSO[VST_Instanced], nullptr);

	Device->CreateVertexShader(
		vertexShaderCSO[VST_Instanced]->GetBufferPointer(),
		vertexShaderCSO[VST_Instanced]->GetBufferSize(), nullptr,
		&VertexShader[VST_Instanced]);

	D3DCompileFromFile(L"Shaders/ShaderFont.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0,
		&vertexShaderCSO[VST_Font], nullptr);

	Device->CreateVertexShader(
		vertexShaderCSO[VST_Font]->GetBufferPointer(),
		vertexShaderCSO[VST_Font]->GetBufferSize(), nullptr,
		&VertexShader[VST_Font]);

	D3DCompileFromFile(L"Shaders/ShaderFont.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0,
		&pixelShaderCSO[PST_Font], nullptr);

	Device->CreatePixelShader(
		pixelShaderCSO[PST_Font]->GetBufferPointer(),
		pixelShaderCSO[PST_Font]->GetBufferSize(), nullptr,
		&PixelShader[PST_Font]);

	D3DCompileFromFile(L"Shaders/ShaderFontMSDF.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0,
		&pixelShaderCSO[PST_UnicodeFont], nullptr);

	Device->CreatePixelShader(
		pixelShaderCSO[PST_UnicodeFont]->GetBufferPointer(),
		pixelShaderCSO[PST_UnicodeFont]->GetBufferSize(), nullptr,
		&PixelShader[PST_UnicodeFont]);

	// Particle
	D3DCompileFromFile(L"Shaders/ShaderParticle.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0,
		&vertexShaderCSO[VST_Particle], nullptr);

	Device->CreateVertexShader(
		vertexShaderCSO[VST_Particle]->GetBufferPointer(),
		vertexShaderCSO[VST_Particle]->GetBufferSize(), nullptr,
		&VertexShader[VST_Particle]);

	D3DCompileFromFile(L"Shaders/ShaderParticle.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0,
		&pixelShaderCSO[PST_Particle], nullptr);

	Device->CreatePixelShader(
		pixelShaderCSO[PST_Particle]->GetBufferPointer(),
		pixelShaderCSO[PST_Particle]->GetBufferSize(), nullptr,
		&PixelShader[PST_Particle]);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FVertexSimple, r), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3D11_INPUT_ELEMENT_DESC Linelayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FVertexSimple, r), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3D11_INPUT_ELEMENT_DESC primitiveTextureLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }// float u, v;    // 12바이트 위치부터 시작
	};

	const D3D11_INPUT_ELEMENT_DESC layoutInstanced[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,  0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, offsetof(FVertexSimple, r), D3D11_INPUT_PER_VERTEX_DATA, 0 },

		// 슬롯 1: 인스턴스의 World 행렬(행렬을 한꺼번에 넣는 건 불가능, 한줄 씩 넣는다)
	   { "INSTANCE_WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	   { "INSTANCE_WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, // 16: 내부 오프셋
	   { "INSTANCE_WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, // 32: 내부 오프셋
	   { "INSTANCE_WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, // 48: 내부 오프셋

	   //// 슬롯 1: 인스턴스의 Tint
	   { "INSTANCE_TINT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};

	Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexShaderCSO[VST_Simple]->GetBufferPointer(), vertexShaderCSO[VST_Simple]->GetBufferSize(), &SimpleInputLayout);
	Device->CreateInputLayout(Linelayout, ARRAYSIZE(Linelayout), vertexShaderCSO[VST_Line]->GetBufferPointer(), vertexShaderCSO[VST_Line]->GetBufferSize(), &LineSimpleInputLayout);
	primitiveTextureLayout[1].AlignedByteOffset = offsetof(FVertexSimple, u);
	Device->CreateInputLayout(primitiveTextureLayout, ARRAYSIZE(primitiveTextureLayout), vertexShaderCSO[VST_Texture]->GetBufferPointer(), vertexShaderCSO[VST_Texture]->GetBufferSize(), &PrimitiveTextureLayout);
	primitiveTextureLayout[1].AlignedByteOffset = offsetof(FVertexTextured, u);
	Device->CreateInputLayout(primitiveTextureLayout, ARRAYSIZE(primitiveTextureLayout), vertexShaderCSO[VST_Font]->GetBufferPointer(), vertexShaderCSO[VST_Font]->GetBufferSize(), &FontInputLayout);

	Device->CreateInputLayout(layoutInstanced, ARRAYSIZE(layoutInstanced), vertexShaderCSO[VST_Instanced]->GetBufferPointer(), vertexShaderCSO[VST_Instanced]->GetBufferSize(), &InstancedInputLayout);

	StrideSimple = sizeof(FVertexSimple);

	for (auto& blob : vertexShaderCSO)
	{
		if (blob)
		{
			blob->Release();
		}
	}
	for (auto& blob : pixelShaderCSO)
	{
		if (blob)
		{
			blob->Release();
		}
	}
}

void URenderer::releaseShader()
{
	/* Simple Shader */
	if (SimpleInputLayout)
	{
		SimpleInputLayout->Release();
		SimpleInputLayout = nullptr;
	}

	/* Line Shader */
	if (LineSimpleInputLayout)
	{
		LineSimpleInputLayout->Release();
		LineSimpleInputLayout = nullptr;
	}

	/* Primitive Texture Shader */
	if (PrimitiveTextureLayout)
	{
		PrimitiveTextureLayout->Release();
		PrimitiveTextureLayout = nullptr;
	}

	/*Instancing*/
	if (InstancedInputLayout)
	{
		InstancedInputLayout->Release();
		InstancedInputLayout = nullptr;
	}
	/* Font Shader */
	if (FontInputLayout)
	{
		FontInputLayout->Release();
		FontInputLayout = nullptr;
	}

	for (auto& vs : VertexShader)
	{
		if (vs)
		{
			vs->Release();
			vs = nullptr;
		}
	}

	for (auto& ps : PixelShader)
	{
		if (ps)
		{
			ps->Release();
			ps = nullptr;
		}
	}
}

// Prepare global rendering state for a new frame
void URenderer::Prepare()
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);

	//매 프레임 깊이 버퍼를 1.0(가장 먼 값)으로 초기화
	DeviceContext->ClearDepthStencilView(DepthStencilView,
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->RSSetViewports(1, &ViewportInfo);
	mbWireFrame = ViewModeIndex == EViewModeIndex::VMI_Wireframe;

	//세 번째 인자에 nullptr 대신 DSV를 넘긴다
	DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, DepthStencilView);
}

void URenderer::PrepareForUI()
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
	DeviceContext->RSSetViewports(1, &ViewportInfo);
	DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
}

void URenderer::PrepareTexturedPrimitive()
{
	prepareTextureShader();

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->RSSetState(RasterizerState[mbWireFrame ? 1 : 0]);

	DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_Default], 0);
	DeviceContext->OMSetBlendState(BlendState[BST_Default], nullptr, 0xffffffff);
}

void URenderer::PrepareLine()
{
	prepareLineShader();

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	DeviceContext->RSSetState(RasterizerState[mbWireFrame ? 1 : 0]);

	DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_Default], 0);
	DeviceContext->OMSetBlendState(BlendState[BST_Default], nullptr, 0xffffffff);
}

void URenderer::PrepareGizmo()
{
	prepareSimpleShader();

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Always render solid
	DeviceContext->RSSetState(RasterizerState[0]);

	DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_Default], 0);
	DeviceContext->OMSetBlendState(BlendState[BST_Default], nullptr, 0xffffffff);
}

void URenderer::PrepareParticle()
{
	prepareParticleShader();

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Always render solid
	DeviceContext->RSSetState(RasterizerState[0]);

	DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_NoWrite], 0);
	DeviceContext->OMSetBlendState(BlendState[BST_Additive], nullptr, 0xffffffff);
}

void URenderer::PrepareHighlight()
{
	prepareSimpleShader();

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Always render solid
	DeviceContext->RSSetState(RasterizerState[0]);
	DeviceContext->OMSetBlendState(BlendState[BST_Default], nullptr, 0xffffffff);
}

void URenderer::PrepareSimpleInstanced()
{
	prepareInstancedShader();

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->RSSetState(RasterizerState[mbWireFrame ? 1 : 0]);

	DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_Default], 0);
	DeviceContext->OMSetBlendState(BlendState[BST_Default], nullptr, 0xffffffff);
}

void URenderer::prepareInstancedShader()
{
	DeviceContext->VSSetShader(VertexShader[VST_Instanced], nullptr, 0);
	DeviceContext->PSSetShader(PixelShader[PST_Simple], nullptr, 0);
	DeviceContext->IASetInputLayout(InstancedInputLayout);

	if (ConstantBuffer[CBT_Simple])
	{
		DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Simple]);
	}
}

void URenderer::prepareSimpleShader()
{
	DeviceContext->VSSetShader(VertexShader[VST_Simple], nullptr, 0);
	DeviceContext->PSSetShader(PixelShader[PST_Simple], nullptr, 0);
	DeviceContext->IASetInputLayout(SimpleInputLayout);

	if (ConstantBuffer[CBT_Simple])
	{
		DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Simple]);
	}
}

void URenderer::prepareTextureShader()
{
	DeviceContext->VSSetShader(VertexShader[VST_Texture], nullptr, 0);
	DeviceContext->PSSetShader(PixelShader[PST_Texture], nullptr, 0);
	DeviceContext->IASetInputLayout(PrimitiveTextureLayout);

	if (ConstantBuffer[CBT_Texture])
	{
		DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Texture]);
		DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Texture]);
	}
}

void URenderer::prepareLineShader()
{
	DeviceContext->VSSetShader(VertexShader[VST_Line], nullptr, 0);
	DeviceContext->PSSetShader(PixelShader[PST_Line], nullptr, 0);
	DeviceContext->IASetInputLayout(LineSimpleInputLayout);

	if (ConstantBuffer[CBT_Simple])
	{
		DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Simple]);
	}
}

void URenderer::prepareParticleShader()
{
	DeviceContext->VSSetShader(VertexShader[VST_Particle], nullptr, 0);
	DeviceContext->PSSetShader(PixelShader[PST_Particle], nullptr, 0);
	DeviceContext->IASetInputLayout(PrimitiveTextureLayout);
	if (ConstantBuffer[CBT_Particle])
	{
		DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Particle]);
		DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Particle]);
	}
}

void URenderer::RenderSimplePrimitive(const UStaticMeshAsset& Mesh)
{
    auto* Vertices = Mesh.GetVertexBuffer().Get();
    UINT Stride = sizeof(FVertexSimple), Offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &Vertices, &Stride, &Offset);
    DeviceContext->IASetIndexBuffer(Mesh.GetIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
    DeviceContext->DrawIndexed(Mesh.GetIndexCount(), 0, 0);
}

void URenderer::RenderTexturedMesh(const UStaticMeshAsset& Mesh, const UTexture2DAsset& Texture,
    D3D11_TEXTURE_ADDRESS_MODE AddressMode)
{
    auto* SRV = Texture.GetSRV().Get();
    auto* Sampler = SamplerStatePool.GetOrCreateSamplerState(Device,
        { D3D11_FILTER_MIN_MAG_MIP_LINEAR, AddressMode, AddressMode });
    DeviceContext->PSSetShaderResources(0, 1, &SRV);
    DeviceContext->PSSetSamplers(0, 1, &Sampler);
    RenderSimplePrimitive(Mesh);
}

// 쌓아둔 선분 전체를 한 번의 Draw로 그린다.
// 토폴로지를 바꾸므로 반드시 이 함수 안에서 되돌린다. 안 그러면 뒤에 그리는 것들이 전부 깨진다.
void URenderer::RenderLines(const FVertexSimple* vertices, uint32 numVertices, const uint32* indices, uint32 numindices)
{
	if (!LineVertexBuffer || vertices == nullptr || numVertices == 0) return;

	if (numVertices > LineVertexCapacity)
	{
		numVertices = LineVertexCapacity;   // 넘치면 자른다. 늘리려면 CreateLineVertexBuffer의 인자를 키운다
	}

	// WRITE_DISCARD: 이전 내용을 버리고 새 메모리를 받는다.
	// GPU가 지난 프레임 데이터를 아직 읽고 있어도 CPU가 기다리지 않는다.
	D3D11_MAPPED_SUBRESOURCE lineBufferMSR;
	if (FAILED(DeviceContext->Map(LineVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &lineBufferMSR)))
	{
		return;
	}
	memcpy(lineBufferMSR.pData, vertices, numVertices * sizeof(FVertexSimple));
	DeviceContext->Unmap(LineVertexBuffer, 0);

	if (!LineIndexBuffer || indices == nullptr || numindices == 0) return;

	if (numindices > LineIndexCapacity)
	{
		numindices = LineIndexCapacity;   // 넘치면 자른다. 늘리려면 CreateLineIndexBuffer의 인자를 키운다
	}

	// WRITE_DISCARD: 이전 내용을 버리고 새 메모리를 받는다.
	// GPU가 지난 프레임 데이터를 아직 읽고 있어도 CPU가 기다리지 않는다.
	D3D11_MAPPED_SUBRESOURCE lineBufferMSRI;
	if (FAILED(DeviceContext->Map(LineIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &lineBufferMSRI)))
	{
		return;
	}
	memcpy(lineBufferMSRI.pData, indices, numindices * sizeof(uint32));
	DeviceContext->Unmap(LineIndexBuffer, 0);

	// 직전에 메시 버퍼가 물려 있으므로 갈아끼워야 한다
	UINT offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &LineVertexBuffer, &StrideSimple, &offset);
	DeviceContext->IASetIndexBuffer(LineIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	DeviceContext->DrawIndexed(numindices, 0, 0);
}

void URenderer::RenderHighlight(const UStaticMeshAsset& Mesh, FMatrix mViewProjectionMatrix, FMatrix OutlineMatrix, const FMatrix originalMatrix)
{
	// (a) 스텐실에 1 마킹. 색은 쓰지 않으므로 화면 변화 없음.
	//     다른 오브젝트에 가려진 부분도 반드시 마킹해야 한다. 여기서 빠지면
	//     (b)의 != 1 조건을 통과해 버려서 겹친 영역 전체가 단색으로 칠해진다.
	DeviceContext->OMSetBlendState(BlendState[BST_NoColorWrite], nullptr, 0xffffffff);
	DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_StencilMark], 1);
	UpdateSimpleConstant(originalMatrix, mViewProjectionMatrix);
	RenderSimplePrimitive(Mesh);

	// (b) 확대판을 단색으로. 스텐실 != 1 인 곳만 통과 -> 테두리
	DeviceContext->OMSetBlendState(BlendState[BST_Default], nullptr, 0xffffffff);
	DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_StencilOutline], 1);
	UpdateSimpleConstant(OutlineMatrix, mViewProjectionMatrix, FLinearColor(1.f, 0.6f, 0.f, 1.f));
	RenderSimplePrimitive(Mesh);
}

//=============================================

void URenderer::createConstantBuffer()
{
	D3D11_BUFFER_DESC desc[CBT_Count] = {};

	// Simple Primitive용 상수 버퍼
	desc[CBT_Simple].ByteWidth = sizeof(FConstants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
	desc[CBT_Simple].Usage = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
	desc[CBT_Simple].CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc[CBT_Simple].BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&desc[CBT_Simple], nullptr, &ConstantBuffer[CBT_Simple]);

	// Texture Primitive용 상수 버퍼
	desc[CBT_Texture].ByteWidth = sizeof(FTextureConstants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
	desc[CBT_Texture].Usage = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
	desc[CBT_Texture].CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc[CBT_Texture].BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&desc[CBT_Texture], nullptr, &ConstantBuffer[CBT_Texture]);

	// Font
	desc[CBT_Font].ByteWidth = sizeof(FFontConstants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
	desc[CBT_Font].Usage = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
	desc[CBT_Font].CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc[CBT_Font].BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&desc[CBT_Font], nullptr, &ConstantBuffer[CBT_Font]);

	// Particle
	desc[CBT_Particle].ByteWidth = sizeof(FParticleConstants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
	desc[CBT_Particle].Usage = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
	desc[CBT_Particle].CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc[CBT_Particle].BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&desc[CBT_Particle], nullptr, &ConstantBuffer[CBT_Particle]);
}

void URenderer::releaseConstantBuffer()
{
	for (auto& buffer : ConstantBuffer)
	{
		if (buffer)
		{
			buffer->Release();
			buffer = nullptr;
		}
	}
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

void URenderer::createDepthStencilState()
{
	D3D11_DEPTH_STENCIL_DESC desc[4] = {};

	// Default Depth Stencil State
	desc[DSS_Default].DepthEnable = TRUE;							 // 깊이 테스트 켜기
	desc[DSS_Default].DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;    // 통과한 픽셀의 z를 기록
	desc[DSS_Default].DepthFunc = D3D11_COMPARISON_LESS;				 // 더 가까우면(작으면) 통과
	desc[DSS_Default].StencilEnable = FALSE;

	// No writing Depth Stencil State
	desc[DSS_NoWrite].DepthEnable = TRUE;							 // 깊이 테스트 켜기
	desc[DSS_NoWrite].DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;    // 깊이는 건드리지 않는다
	desc[DSS_NoWrite].DepthFunc = D3D11_COMPARISON_LESS;				 // 더 가까우면(작으면) 통과
	desc[DSS_NoWrite].StencilEnable = FALSE;

	// Stencil Mark State
	desc[DSS_StencilMark].DepthEnable = FALSE;						 // 깊이 테스트 끄기
	desc[DSS_StencilMark].DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 깊이는 건드리지 않는다
	desc[DSS_StencilMark].DepthFunc = D3D11_COMPARISON_ALWAYS;		 // 항상 통과

	desc[DSS_StencilMark].StencilEnable = TRUE;						 // 스텐실 사용
	desc[DSS_StencilMark].StencilReadMask = 0xFF;
	desc[DSS_StencilMark].StencilWriteMask = 0xFF;

	// 스텐실 마킹: 모든 픽셀에 StencilRef를 기록
	desc[DSS_StencilMark].FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS; // 항상 통과
	desc[DSS_StencilMark].FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE; // 통과하면 스텐실에 StencilRef 기록
	desc[DSS_StencilMark].FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_REPLACE; // 깊이 테스트 실패 시에도 기록
	desc[DSS_StencilMark].FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP; // 스텐실 테스트 실패 시 기록하지 않음
	desc[DSS_StencilMark].BackFace = desc[DSS_StencilMark].FrontFace; // 뒷면도 동일

	// Stencil Outline State
	desc[DSS_StencilOutline].DepthEnable = FALSE;						 // 깊이 테스트 끄기
	desc[DSS_StencilOutline].DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 깊이는 건드리지 않는다

	desc[DSS_StencilOutline].StencilEnable = TRUE;						 // 스텐실 사용
	desc[DSS_StencilOutline].StencilReadMask = 0xFF;
	desc[DSS_StencilOutline].StencilWriteMask = 0x00;					 // 읽기만, 쓰지 않는다

	// 스텐실 아웃라인: 마킹된 곳(=원본 실루엣)은 통과 못 함 -> 바깥 테두리만 남는다
	desc[DSS_StencilOutline].FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL; // 스텐실 값이 StencilRef와 다르면 통과
	desc[DSS_StencilOutline].FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP; // 통과해도 스텐실에 기록하지 않음
	desc[DSS_StencilOutline].FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP; // 깊이 테스트 실패 시에도 기록하지 않음
	desc[DSS_StencilOutline].FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP; // 스텐실 테스트 실패 시 기록하지 않음
	desc[DSS_StencilOutline].BackFace = desc[DSS_StencilOutline].FrontFace; // 뒷면도 동일

	Device->CreateDepthStencilState(&desc[DSS_Default], &DepthStencilState[DSS_Default]);
	Device->CreateDepthStencilState(&desc[DSS_NoWrite], &DepthStencilState[DSS_NoWrite]);
	Device->CreateDepthStencilState(&desc[DSS_StencilMark], &DepthStencilState[DSS_StencilMark]);
	Device->CreateDepthStencilState(&desc[DSS_StencilOutline], &DepthStencilState[DSS_StencilOutline]);
}

void URenderer::createBlendState()
{
	D3D11_BLEND_DESC desc[4] = {};

	// Default Blend State (No Blending)
	// Do nothing -> nullptr

	// Standard Alpha Blending
	{
		auto& rt = desc[BST_AlphaBlend].RenderTarget[0];
		rt.BlendEnable = TRUE;

		// RGB = Src.rgb * src.a + Dest.rgb * (1 - src.a)
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D11_BLEND_OP_ADD;

		// Alpha = Src.a + Dest.a * (1 - src.a)
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		Device->CreateBlendState(&desc[BST_AlphaBlend], &BlendState[BST_AlphaBlend]);
	}

	// Additive Blending
	{
		auto& rt = desc[BST_Additive].RenderTarget[0];
		rt.BlendEnable = TRUE;

		// RGB = Src.rgb * src.a + Dest.rgb * 1
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_ONE;
		rt.BlendOp = D3D11_BLEND_OP_ADD;

		// Alpha = Src.a + Dest.a * (1 - src.a)
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		Device->CreateBlendState(&desc[BST_Additive], &BlendState[BST_Additive]);
	}

	// No Color Write (Stencil Marking)
	{
		auto& rt = desc[BST_NoColorWrite].RenderTarget[0];
		rt.BlendEnable = FALSE;
		rt.RenderTargetWriteMask = 0;
		Device->CreateBlendState(&desc[BST_NoColorWrite], &BlendState[BST_NoColorWrite]);
	}
}

void URenderer::releaseBlendState()
{
	for (auto& state : BlendState)
	{
		if (state) { state->Release(); state = nullptr; }
	}
}

void URenderer::releaseDepthStencilBuffer()
{
	if (DepthStencilView) { DepthStencilView->Release();   DepthStencilView = nullptr; }
	if (DepthStencilBuffer) { DepthStencilBuffer->Release(); DepthStencilBuffer = nullptr; }
}

void URenderer::releaseDepthStencilState()
{
	for (auto& state : DepthStencilState)
	{
		if (state) { state->Release(); state = nullptr; }
	}
}

void URenderer::UpdateSimpleConstant(FMatrix world, FMatrix viewProjection, FLinearColor tint)
{
	if (ConstantBuffer[CBT_Simple])
	{
		D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

		DeviceContext->Map(ConstantBuffer[CBT_Simple], 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
		FConstants* constants = (FConstants*)constantbufferMSR.pData;
		{
			constants->World = world;
			constants->ViewProjection = viewProjection;
			constants->Tint = tint;
		}
		DeviceContext->Unmap(ConstantBuffer[CBT_Simple], 0);
	}
}

void URenderer::UpdateTextureConstant(FMatrix world, FMatrix viewProjection, FLinearColor tint,
	FVector2 uvScale, FVector2 uvOffset)
{
	if (ConstantBuffer[CBT_Texture])
	{
		D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
		DeviceContext->Map(ConstantBuffer[CBT_Texture], 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
		FTextureConstants* constants = (FTextureConstants*)constantbufferMSR.pData;
		{
			constants->World = world;
			constants->ViewProjection = viewProjection;
			constants->Tint = tint;
			constants->UVOffset = uvOffset;
			constants->UVScale = uvScale;
		}
		DeviceContext->Unmap(ConstantBuffer[CBT_Texture], 0);
	}
}

void URenderer::UpdateFontConstant(FVector3 location, FVector3 scale, FMatrix viewProjection,
	FVector3 cameraRight, FVector3 cameraUp,
	FLinearColor tint)
{
	assert(ConstantBuffer[CBT_Font]);

	D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
	DeviceContext->Map(ConstantBuffer[CBT_Font], 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
	FFontConstants* constants = (FFontConstants*)constantbufferMSR.pData;
	{
		constants->Location = location;
		constants->Scale = scale;
		constants->ViewProjection = viewProjection;
		constants->Tint = tint;
		constants->CameraRight = cameraRight;
		constants->CameraUp = cameraUp;
	}
	DeviceContext->Unmap(ConstantBuffer[CBT_Font], 0);

}

void URenderer::UpdateParticleConstant(FVector3 location, FVector3 scale, FMatrix viewProjection,
	FVector3 cameraRight, FVector3 cameraUp,
	int32 numRows, int32 numCols, int32 currentFrame, int32 nextFrame, float frameRatio,
	FLinearColor tint
)
{
	assert(ConstantBuffer[CBT_Particle]);
	D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
	DeviceContext->Map(ConstantBuffer[CBT_Particle], 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
	FParticleConstants* constants = (FParticleConstants*)constantbufferMSR.pData;
	{
		constants->Location = location;
		constants->Scale = scale;
		constants->ViewProjection = viewProjection;
		constants->Tint = tint;
		constants->CameraRight = cameraRight;
		constants->CameraUp = cameraUp;
		constants->NumRows = numRows;
		constants->NumCols = numCols;
		constants->CurrentFrame = currentFrame;
		constants->NextFrame = nextFrame;
		constants->FrameRatio = frameRatio;
	}
	DeviceContext->Unmap(ConstantBuffer[CBT_Particle], 0);
}

void URenderer::UpdateBlendState(EBlendStateType blendState)
{
	if (!DeviceContext || blendState >= EBlendStateType::BST_Count)
	{
		return;
	}
	DeviceContext->OMSetBlendState(BlendState[blendState], nullptr, 0xffffffff);
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

void URenderer::createFullscreenShader()
{

	ID3DBlob* vertexShaderBlob = nullptr;
	ID3DBlob* pixelShaderBlob = nullptr;

	D3DCompileFromFile(L"Shaders/ShaderLoadingScreen.hlsl", nullptr, nullptr, "mainVS", "vs_5_0",
		0, 0, &vertexShaderBlob, nullptr);
	Device->CreateVertexShader(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(),
		nullptr, &LoadingScreenVertexShader);

	D3DCompileFromFile(L"Shaders/ShaderLoadingScreen.hlsl", nullptr, nullptr, "mainPS", "ps_5_0",
		0, 0, &pixelShaderBlob, nullptr);
	Device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize(),
		nullptr, &LoadingScreenPixelShader);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D11_INPUT_PER_VERTEX_DATA,
			0
		},
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			static_cast<UINT>(offsetof(FVertexSimple, u)),
			D3D11_INPUT_PER_VERTEX_DATA,
			0
		}
	};

	Device->CreateInputLayout(
		layout,
		ARRAYSIZE(layout),
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize(),
		&LoadingScreenInputLayout);

	if (vertexShaderBlob)
		vertexShaderBlob->Release();
	if (pixelShaderBlob)
		pixelShaderBlob->Release();
}

void URenderer::RenderFullscreenTexture(const UStaticMeshAsset& Mesh, const UTexture2DAsset& Texture)
{
    DeviceContext->IASetInputLayout(LoadingScreenInputLayout);
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->VSSetShader(LoadingScreenVertexShader, nullptr, 0);
    DeviceContext->PSSetShader(LoadingScreenPixelShader, nullptr, 0);
    DeviceContext->RSSetState(RasterizerState[0]);
    DeviceContext->OMSetBlendState(BlendState[BST_Default], nullptr, 0xffffffff);
    auto* NoDepth = DepthStencilStatePool.GetOrCreateDepthStencilState(Device, {false, false});
    DeviceContext->OMSetDepthStencilState(NoDepth, 0);
    RenderTexturedMesh(Mesh, Texture, D3D11_TEXTURE_ADDRESS_CLAMP);
}

void URenderer::ClearDepth()
{
	DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
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

bool URenderer::RenderText(const FTextMesh& Mesh, const UFontAtlasAsset& Atlas)
{
    if (Mesh.Indices.IsEmpty()) return true;
    if (!Atlas.GetSRV() || (Mesh.FontRenderMode == EFontRenderMode::MSDF) != Atlas.IsMSDF()) return false;
    for (uint32 Index : Mesh.Indices)
        if (Index >= Mesh.Vertices.Num()) return false;
    if (!UploadTextBuffer(Device, DeviceContext, TextVertexBuffer, D3D11_BIND_VERTEX_BUFFER, Mesh.Vertices) ||
        !UploadTextBuffer(Device, DeviceContext, TextIndexBuffer, D3D11_BIND_INDEX_BUFFER, Mesh.Indices)) return false;

    if (Atlas.IsMSDF())
    {
        if (!MSDFConstantBuffer)
        {
            D3D11_BUFFER_DESC Desc{};
            Desc.ByteWidth = sizeof(FUnicodeFontConstants);
            Desc.Usage = D3D11_USAGE_DEFAULT;
            Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            if (FAILED(Device->CreateBuffer(&Desc, nullptr, &MSDFConstantBuffer))) return false;
        }
        FUnicodeFontConstants Constants{};
        Constants.DistanceRange = Atlas.GetDistanceRange();
        DeviceContext->UpdateSubresource(MSDFConstantBuffer.Get(), 0, nullptr, &Constants, 0, 0);
    }
    DeviceContext->VSSetShader(VertexShader[VST_Font], nullptr, 0);
    DeviceContext->PSSetShader(PixelShader[Atlas.IsMSDF() ? PST_UnicodeFont : PST_Font], nullptr, 0);
    DeviceContext->IASetInputLayout(FontInputLayout);
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->RSSetState(RasterizerState[0]);
    DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_Default], 0);
    DeviceContext->OMSetBlendState(BlendState[Atlas.IsMSDF() ? BST_AlphaBlend : BST_Additive], nullptr, 0xffffffff);
    DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Font]);
    DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Font]);
    auto* MSDFBuffer = Atlas.IsMSDF() ? MSDFConstantBuffer.Get() : nullptr;
    DeviceContext->PSSetConstantBuffers(1, 1, &MSDFBuffer);
    auto* Vertices = TextVertexBuffer.Get();
    UINT Stride = sizeof(FVertexTextured), Offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &Vertices, &Stride, &Offset);
    DeviceContext->IASetIndexBuffer(TextIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    auto* SRV = Atlas.GetSRV().Get();
    auto* Sampler = SamplerStatePool.GetOrCreateSamplerState(Device,
        {D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP});
    DeviceContext->PSSetShaderResources(0, 1, &SRV);
    DeviceContext->PSSetSamplers(0, 1, &Sampler);
    DeviceContext->DrawIndexed(Mesh.Indices.Num(), 0, 0);
    return true;
}
