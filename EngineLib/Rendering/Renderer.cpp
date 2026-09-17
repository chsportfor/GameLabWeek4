#include "Renderer.h"

#include <fstream>
#include <filesystem>
#include <vector>
#include <directxtk/DDSTextureLoader.h>
#include <directxtk/WICTextureLoader.h>
#include <cmath>
#include <string>
#include <wrl/client.h>

#include "Core/Math/MathUtility.h"
#include "Editor/Console.h"
#include "Rendering/Primitives/TexturedPrimitives.h"
#include "Rendering/TextMesh.h"

#pragma comment(lib, "DirectXTK.lib")
#pragma comment(lib, "dxguid.lib")

void URenderer::Create(HWND hWindow)
{
	createDeviceAndSwapChain(hWindow);
	createFrameBuffer();

	if (!createFontAtlasTexture() ||
		!createFontSamplerState())
	{
		MessageBox(
			hWindow,
			L"영어 폰트 공통 자원 생성에 실패했습니다.",
			L"English Font initialization error",
			MB_OK | MB_ICONERROR
		);

		releaseFontTexture();
		releaseFontAtlasTexture();
	}

	/* Create states */
	createDepthStencilState();
	createRasterizerState();
	createBlendState();

	createShader();
	createConstantBuffer();
	createLineVertexBuffer(LINE_VERTEX_CAPACITY);
	createLineIndexBuffer(LINE_INDEX_CAPACITY);

	/* Particle */
	createParticleStates();
	createParticleVertexBuffer();
	createParticleIndexBuffer();

	CreateLoadingScreenResources();
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

bool URenderer::createFontAtlasTexture()
{
	if (Device == nullptr)
	{
		return false;
	}

	// 함수가 다시 호출되는 경우 기존 텍스처 해제

	releaseFontAtlasTexture();

	// 추후 동적으로 텍스쳐 로드하자 
	HRESULT hr = DirectX::CreateDDSTextureFromFile(
		Device,
		L"Assets/Fonts/EnglishBigFontAtlas.dds",
		nullptr,
		&FontAtlasShaderResoruceView
	);

	if (FAILED(hr))
	{
		FontAtlasShaderResoruceView = nullptr;
		return false;
	}

	return true;
}

void URenderer::releaseFontAtlasTexture()
{
	if (FontAtlasShaderResoruceView != nullptr)
	{
		FontAtlasShaderResoruceView->Release();
		FontAtlasShaderResoruceView = nullptr;
	}
}

bool URenderer::createFontSamplerState()
{
	if (!Device)
		return false;

	if (FontSamplerState)
	{
		FontSamplerState->Release();
		FontSamplerState = nullptr;
	}

	D3D11_SAMPLER_DESC desc = {};
	desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	desc.MaxAnisotropy = 1;
	desc.MaxLOD = D3D11_FLOAT32_MAX;

	return SUCCEEDED(Device->CreateSamplerState(&desc, &FontSamplerState)
	);
}

bool URenderer::createParticleStates()
{
	if (!Device)
		return false;

	if (ParticleSamplerState)
	{
		ParticleSamplerState->Release();
		ParticleSamplerState = nullptr;
	}

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	return SUCCEEDED(Device->CreateSamplerState(&samplerDesc, &ParticleSamplerState));
}

// 인스턴스 사용하여 렌더링(텍스쳐 X)
bool URenderer::RenderSimpleInstanced(
	ID3D11Buffer* vertexBuffer,
	ID3D11Buffer* indexBuffer,
	UINT indexCount,
	const FInstanceData* instances,
	UINT instanceCount)
{
	if (instanceCount == 0)
		return true;

	if (!DeviceContext ||
		!vertexBuffer ||
		!instances ||
		!indexBuffer ||
		indexCount == 0 ||
		!ConstantBuffer ||
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

/// 
ID3D11Buffer* URenderer::CreateVertexBuffer(FVertexSimple* vertices, UINT ByteWidth)
{
	UINT numVertices = ByteWidth / sizeof(FVertexSimple);

	D3D11_BUFFER_DESC vertexbufferdesc = {};
	vertexbufferdesc.ByteWidth = ByteWidth;
	vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };

	ID3D11Buffer* vertexBuffer;
	Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

	return vertexBuffer;
}

// 텍스쳐 매핑시에 사용
ID3D11Buffer* URenderer::CreateVertexBuffer(const FVertexTextured* vertices, UINT byteWidth)
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = byteWidth;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = vertices;

	ID3D11Buffer* buffer = nullptr;
	if (FAILED(Device->CreateBuffer(&desc, &data, &buffer)))
		return nullptr;

	return buffer;
}

void URenderer::ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
{
	vertexBuffer->Release();
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

void URenderer::releaseFontBuffers()
{
	if (FontTextureBuffer)
	{
		FontTextureBuffer->Release();
		FontTextureBuffer = nullptr;
	}

	if (FontIndexBuffer)
	{
		FontIndexBuffer->Release();
		FontIndexBuffer = nullptr;
	}
}

void URenderer::releaseUnicodeFontAtlasTexture()
{
	if (UnicodeFontAtlasSRV)
	{
		UnicodeFontAtlasSRV->Release();
		UnicodeFontAtlasSRV = nullptr;
	}
}

void URenderer::releaseUnicodeFontBuffers()
{
	if (UnicodeFontVertexBuffer)
	{
		UnicodeFontVertexBuffer->Release();
		UnicodeFontVertexBuffer = nullptr;
	}

	if (UnicodeFontIndexBuffer)
	{
		UnicodeFontIndexBuffer->Release();
		UnicodeFontIndexBuffer = nullptr;
	}

	if (UnicodeFontConstantBuffer)
	{
		UnicodeFontConstantBuffer->Release();
		UnicodeFontConstantBuffer = nullptr;
	}

	UnicodeFontVertexCapacity = 0;
	UnicodeFontIndexCapacity = 0;
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

void URenderer::createParticleVertexBuffer()
{
	D3D11_BUFFER_DESC vertexbufferdesc = {};
	// Assume particle is a quad
	vertexbufferdesc.ByteWidth = sizeof(QuadTextureIndexedVertices);
	vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexbufferdesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexbufferSRD = {};
	vertexbufferSRD.pSysMem = QuadTextureIndexedVertices;

	Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &ParticleVertexBuffer);
}

void URenderer::createParticleIndexBuffer()
{
	D3D11_BUFFER_DESC indexbufferdesc = {};
	indexbufferdesc.ByteWidth = sizeof(QuadTextureIndices);
	indexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexbufferdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexbufferdesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA indexbufferSRD = {};
	indexbufferSRD.pSysMem = QuadTextureIndices;

	Device->CreateBuffer(&indexbufferdesc, &indexbufferSRD, &ParticleIndexBuffer);
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
	releaseRasterizerState();

	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	releaseDepthStencilBuffer();
	releaseDepthStencilState();
	releaseBlendState();
	releaseFrameBuffer();
	releaseLineVertexBuffer();
	releaseLineIndexBuffer();
	releaseConstantBuffer();
	releaseShader();

	// 테스트

	if (CubeIndexBuffer)
	{
		CubeIndexBuffer->Release();
		CubeIndexBuffer = nullptr;
	}
	releaseFontAtlasTexture();
	releaseFontTexture();
	releaseFontBuffers();

	// 추가: 유니코드 폰트 자원
	releaseUnicodeFontAtlasTexture();
	releaseUnicodeFontBuffers();

	//ReleaseTestTexture();
	releaseDeviceAndSwapChain();

	if (InstanceBuffer)
	{
		InstanceBuffer->Release();
		InstanceBuffer = nullptr;
	}

	/* Particle */
	if (ParticleVertexBuffer)
	{
		ParticleVertexBuffer->Release();
		ParticleVertexBuffer = nullptr;
	}
	if (ParticleIndexBuffer)
	{
		ParticleIndexBuffer->Release();
		ParticleIndexBuffer = nullptr;
	}
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

	D3DCompileFromFile(L"Shaders/ShaderTexture.hlsl", nullptr, nullptr, "billboardVS", "vs_5_0", 0, 0,
		&vertexShaderCSO[VST_Billboard], nullptr);

	Device->CreateVertexShader(
		vertexShaderCSO[VST_Billboard]->GetBufferPointer(),
		vertexShaderCSO[VST_Billboard]->GetBufferSize(), nullptr,
		&VertexShader[VST_Billboard]);

	D3DCompileFromFile(L"Shaders/ShaderTexture.hlsl", nullptr, nullptr, "billboardPS", "ps_5_0", 0, 0,
		&pixelShaderCSO[PST_Billboard], nullptr);

	Device->CreatePixelShader(
		pixelShaderCSO[PST_Billboard]->GetBufferPointer(),
		pixelShaderCSO[PST_Billboard]->GetBufferSize(), nullptr,
		&PixelShader[PST_Billboard]);

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
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3D11_INPUT_ELEMENT_DESC Linelayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3D11_INPUT_ELEMENT_DESC primitiveTextureLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }// float u, v;    // 12바이트 위치부터 시작
	};

	const D3D11_INPUT_ELEMENT_DESC layoutInstanced[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,  0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },

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
	Device->CreateInputLayout(primitiveTextureLayout, ARRAYSIZE(primitiveTextureLayout), vertexShaderCSO[VST_Texture]->GetBufferPointer(), vertexShaderCSO[VST_Texture]->GetBufferSize(), &PrimitiveTextureLayout);
	Device->CreateInputLayout(primitiveTextureLayout, ARRAYSIZE(primitiveTextureLayout), vertexShaderCSO[VST_Font]->GetBufferPointer(), vertexShaderCSO[VST_Font]->GetBufferSize(), &FontInputLayout);

	Device->CreateInputLayout(layoutInstanced, ARRAYSIZE(layoutInstanced), vertexShaderCSO[VST_Instanced]->GetBufferPointer(), vertexShaderCSO[VST_Instanced]->GetBufferSize(), &InstancedInputLayout);

	StrideSimple = sizeof(FVertexSimple);
	StrideTextured = sizeof(FVertexTextured);

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

// 유니코드 준비
bool URenderer::InitializeUnicodeFont(const wchar_t* atlasPath, float distanceRange)
{
	//  기존 스마트포인터와 같이 참조횟수가 0이 되면 메모리에서 해제
	using Microsoft::WRL::ComPtr;

	if (Device == nullptr
		|| atlasPath == nullptr
		|| atlasPath[0] == L'\0'
		|| !std::isfinite(distanceRange)
		|| distanceRange <= 0.0f)
	{
		return false;
	}

	// 성공하기 전까지 기존 멤버를 변경하지 않는다.
	ComPtr<ID3D11ShaderResourceView> newAtlasSRV;
	ComPtr<ID3D11Buffer> newConstantBuffer;
	// PNG 로딩
	// MSDF의 RGB는 색상이 아닌 거리 데이터이므로
	// sRGB 변환 없이 읽는다.
	// DeviceContext를 받지 않는 오버로드를 사용하여
	// 여기서는 mipmap을 자동 생성하지 않는다.

	HRESULT hr = DirectX::CreateWICTextureFromFileEx(
		Device,
		atlasPath,						// png 위치
		0,                              // 기본 최대 크기
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_SHADER_RESOURCE,
		0,                              // CPU 접근 없음
		0,                              // 추가 리소스 옵션 없음
		DirectX::WIC_LOADER_IGNORE_SRGB,
		nullptr,                        // 원본 texture 포인터 불필요
		newAtlasSRV.GetAddressOf());

	if (FAILED(hr))
	{
		OutputDebugStringA("InitializeUnicodeFont: PNG loading failed.\n");
		OutputDebugStringW(atlasPath);
		OutputDebugStringW(L"\n");

		return false;
	}

	FUnicodeFontConstants constants{};
	constants.DistanceRange = distanceRange;

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth =
		static_cast<UINT>(sizeof(FUnicodeFontConstants));
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	D3D11_SUBRESOURCE_DATA initialData{};
	initialData.pSysMem = &constants;

	// 상수 버퍼 생성
	hr = Device->CreateBuffer(&bufferDesc, &initialData, newConstantBuffer.GetAddressOf());

	if (FAILED(hr))
	{
		return false;
	}

	// 모두 성공했으므로 기존 자원 교체
	if (UnicodeFontAtlasSRV != nullptr)
	{
		UnicodeFontAtlasSRV->Release();
	}

	if (UnicodeFontConstantBuffer != nullptr)
	{
		UnicodeFontConstantBuffer->Release();
	}

	// Detach(): ComPtr가 가진 소유권을 멤버로 넘김
	UnicodeFontAtlasSRV = newAtlasSRV.Detach();
	UnicodeFontConstantBuffer = newConstantBuffer.Detach();

	return true;
}


// Sampler State 설정
void URenderer::CreateSamplerState(ID3D11SamplerState** outSamplerState)
{
	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	Device->CreateSamplerState(&samplerDesc, outSamplerState);
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

	/* Texture Shader */
	if (FontInputLayout)
	{
		FontInputLayout->Release();
		FontInputLayout = nullptr;
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

// 개별 이미지: 지정된 파일을 로딩해서 결과를 반환
bool URenderer::LoadTexture(const wchar_t* texturePath, ID3D11ShaderResourceView** outSRV)
{
	if (!Device || !texturePath || texturePath[0] == L'\0' || !outSRV)
	{
		OutputDebugStringA("LoadTexture: invalid argument or Device.\n");
		return false;
	}

	// 임시 포인터로 로딩해서 실패 시 기존 텍스처를 보존
	ID3D11ShaderResourceView* loadedSRV = nullptr;

	const HRESULT hr = DirectX::CreateDDSTextureFromFile(
		Device,
		texturePath,
		nullptr,
		&loadedSRV);

	if (FAILED(hr))
	{
		if (loadedSRV)
			loadedSRV->Release();

		OutputDebugStringW(L"LoadTexture failed: ");
		OutputDebugStringW(texturePath);
		OutputDebugStringW(L"\n");

		return false;
	}

	// 성공한 경우에만 기존 텍스처를 교체
	if (*outSRV)
		(*outSRV)->Release();

	*outSRV = loadedSRV;
	return true;
}

// Prepare global rendering state for a new frame
void URenderer::Prepare(bool bWireFrame)
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);

	//매 프레임 깊이 버퍼를 1.0(가장 먼 값)으로 초기화
	DeviceContext->ClearDepthStencilView(DepthStencilView,
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->RSSetViewports(1, &ViewportInfo);

	//DeviceContext->RSSetState(RasterizerState[bWireFrame ? 1 : 0]);
	mbWireFrame = bWireFrame;

	//세 번째 인자에 nullptr 대신 DSV를 넘긴다
	DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, DepthStencilView);
}

void URenderer::PrepareForUI()
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
	DeviceContext->RSSetViewports(1, &ViewportInfo);
	DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
}

void URenderer::PrepareSimplePrimitive()
{
	prepareSimpleShader();

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->RSSetState(RasterizerState[mbWireFrame ? 1 : 0]);

	DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_Default], 0);
	DeviceContext->OMSetBlendState(BlendState[BST_Default], nullptr, 0xffffffff);
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

void URenderer::PrepareFont()
{
	prepareFontShader();

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Always render solid
	DeviceContext->RSSetState(RasterizerState[0]);

	DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_Default], 0);
	DeviceContext->OMSetBlendState(BlendState[BST_Additive], nullptr, 0xffffffff);
}

void URenderer::PrepareUnicodeFont()
{
	prepareUnicodeFontShader();

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 기존 PrepareFont와 동일하게 면으로 렌더링
	DeviceContext->RSSetState(RasterizerState[0]);

	DeviceContext->OMSetBlendState(BlendState[BST_AlphaBlend], nullptr, 0xffffffff);
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

	// Set depth stencil state in the RenderHighlight method
	//DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_Default], 0);
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

void URenderer::prepareBillboardTextureShader()
{
	DeviceContext->VSSetShader(VertexShader[VST_Billboard], nullptr, 0);
	DeviceContext->PSSetShader(PixelShader[PST_Billboard], nullptr, 0);
	DeviceContext->IASetInputLayout(PrimitiveTextureLayout);

	if (ConstantBuffer[CBT_BillboardTexture])
	{
		DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_BillboardTexture]);
		DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_BillboardTexture]);
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

void URenderer::prepareFontShader()
{
	DeviceContext->VSSetShader(VertexShader[VST_Font], nullptr, 0);
	DeviceContext->PSSetShader(PixelShader[PST_Font], nullptr, 0);
	DeviceContext->IASetInputLayout(FontInputLayout);
	if (ConstantBuffer[CBT_Font])
	{
		DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Font]);
		DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Font]);
	}
}

void URenderer::prepareUnicodeFontShader()
{
	DeviceContext->VSSetShader(VertexShader[VST_Font], nullptr, 0);
	DeviceContext->IASetInputLayout(FontInputLayout);

	// MSDF 전용 픽셀 셰이더
	DeviceContext->PSSetShader(PixelShader[PST_UnicodeFont], nullptr, 0);

	DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Font]);
	DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer[CBT_Font]);

	// b1: DistanceRange
	DeviceContext->PSSetConstantBuffers(1, 1, &UnicodeFontConstantBuffer);
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

void URenderer::RenderSimplePrimitive(ID3D11Buffer* pBuffer, UINT numVertices)
{
	UINT offset = 0;
	// Bind the vertex buffer
	DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &StrideSimple, &offset);
	DeviceContext->Draw(numVertices, 0);
}

void URenderer::RenderTexturePrimitive(ID3D11Buffer* pBuffer, UINT numVertices,
	ID3D11ShaderResourceView* texture, ID3D11SamplerState* samplerState, ID3D11Buffer* indexBuffer, UINT indexCount)
{
	UINT offset = 0;
	// Bind the vertex buffer
	DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &StrideTextured, &offset);

	// Bind the texture resource
	DeviceContext->PSSetShaderResources(0, 1, &texture);
	DeviceContext->PSSetSamplers(0, 1, &samplerState);


	//DeviceContext->Draw(numVertices, 0);


	DeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// 임시 큐브
	if (indexBuffer)
		DeviceContext->DrawIndexed(indexCount, 0, 0);
	else
		DeviceContext->Draw(numVertices, 0);
}


//void URenderer::RenderTexturedPrimitive(ID3D11Buffer* vertexBuffer,	UINT numVertices)
//{
//	if (!DeviceContext ||
//		!vertexBuffer ||
//		numVertices == 0 ||
//		!ConstantBuffer ||
//		!PrimitiveTextureVertexShader ||
//		!PrimitiveTexturePixelShader ||
//		!PrimitiveTextureLayout ||
//		!PrimitiveTextureSRV ||
//		!PrimitiveTextureSampler ||
//		!DepthStencilState)
//	{
//		return;
//	}
//
//	// 위치 + UV 정점 버퍼 연결
//	const UINT stride = sizeof(FVertexTextured);
//	const UINT offset = 0;
//
//	DeviceContext->IASetVertexBuffers(
//		0, 1, &vertexBuffer, &stride, &offset);
//
//	DeviceContext->IASetInputLayout(PrimitiveTextureLayout);
//
//	DeviceContext->IASetPrimitiveTopology(
//		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
//	// 텍스처용 셰이더 연결
//	DeviceContext->VSSetShader(PrimitiveTextureVertexShader, nullptr, 0);
//
//	DeviceContext->PSSetShader(PrimitiveTexturePixelShader, nullptr, 0);
//
//	//  GraphicsManager에서 갱신한 변환 행렬 연결
//	DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
//
//	// HLSL의 t0에 텍스처, s0에 샘플러 연결
//	DeviceContext->PSSetShaderResources(0, 1, &PrimitiveTextureSRV);
//
//	DeviceContext->PSSetSamplers(0, 1, &PrimitiveTextureSampler);
//
//	// 일반 불투명 Primitive용 상태 설정
//	DeviceContext->OMSetDepthStencilState(DepthStencilState, 0);
//
//	DeviceContext->OMSetBlendState(	nullptr, nullptr, 0xffffffff);
//
//	// 그리기
//	DeviceContext->Draw(numVertices, 0);
//
//	//  사용한 텍스처와 샘플러 연결 해제
//	ID3D11ShaderResourceView* nullSRV = nullptr;
//	ID3D11SamplerState* nullSampler = nullptr;
//
//	DeviceContext->PSSetShaderResources(0, 1, &nullSRV);
//
//	DeviceContext->PSSetSamplers(0, 1, &nullSampler);
//
//	// 기존 색상 셰이더로 돌아가기
//	PrepareSimpleShader();
//}

void URenderer::RenderFontTexture(uint32 numCharacter)
{
	UINT offset = 0;
	// Bind the vertex buffer
	DeviceContext->IASetVertexBuffers(0, 1, &FontTextureBuffer, &StrideTextured, &offset);

	// Bind the texture resource
	DeviceContext->PSSetShaderResources(0, 1, &FontAtlasShaderResoruceView);
	DeviceContext->PSSetSamplers(0, 1, &FontSamplerState);

	// Bind the index buffer
	DeviceContext->IASetIndexBuffer(FontIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	DeviceContext->DrawIndexed(numCharacter * 6, 0, 0);
}

void URenderer::RenderUnicodeFontTexture(uint32 indexCount)
{
	UINT offset = 0;

	DeviceContext->IASetVertexBuffers(0, 1, &UnicodeFontVertexBuffer, &StrideTextured, &offset);

	DeviceContext->IASetIndexBuffer(UnicodeFontIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	DeviceContext->PSSetShaderResources(0, 1, &UnicodeFontAtlasSRV);

	DeviceContext->PSSetSamplers(0, 1, &FontSamplerState);

	// 이미 인덱스 개수를 받으므로 6을 곱하지 않음
	DeviceContext->DrawIndexed(indexCount, 0, 0);
}
void URenderer::RenderParticle(ID3D11ShaderResourceView* texture)
{
	if (!ParticleVertexBuffer || texture == nullptr) return;

	UINT offset = 0;

	// Bind the vertex buffer
	DeviceContext->IASetVertexBuffers(0, 1, &ParticleVertexBuffer, &StrideTextured, &offset);

	// Bind the texture resource
	DeviceContext->PSSetShaderResources(0, 1, &texture);
	DeviceContext->PSSetSamplers(0, 1, &ParticleSamplerState);

	// Bind the index buffer
	DeviceContext->IASetIndexBuffer(ParticleIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	DeviceContext->DrawIndexed(6, 0, 0);
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

void URenderer::RenderHighlight(ID3D11Buffer* pBuffer, uint32 Num, FMatrix mViewProjectionMatrix, FMatrix OutlineMatrix, const FMatrix originalMatrix)
{
	// (a) 스텐실에 1 마킹. 색은 쓰지 않으므로 화면 변화 없음.
	//     다른 오브젝트에 가려진 부분도 반드시 마킹해야 한다. 여기서 빠지면
	//     (b)의 != 1 조건을 통과해 버려서 겹친 영역 전체가 단색으로 칠해진다.
	DeviceContext->OMSetBlendState(BlendState[BST_NoColorWrite], nullptr, 0xffffffff);
	DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_StencilMark], 1);
	UpdateSimpleConstant(originalMatrix, mViewProjectionMatrix);
	RenderSimplePrimitive(pBuffer, Num);

	// (b) 확대판을 단색으로. 스텐실 != 1 인 곳만 통과 -> 테두리
	DeviceContext->OMSetBlendState(BlendState[BST_Default], nullptr, 0xffffffff);
	DeviceContext->OMSetDepthStencilState(DepthStencilState[DSS_StencilOutline], 1);
	UpdateSimpleConstant(OutlineMatrix, mViewProjectionMatrix, FLinearColor(1.f, 0.6f, 0.f, 1.f));
	RenderSimplePrimitive(pBuffer, Num);
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

	// Billboard Texture
	desc[CBT_BillboardTexture].ByteWidth = sizeof(FBillboardConstants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
	desc[CBT_BillboardTexture].Usage = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
	desc[CBT_BillboardTexture].CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc[CBT_BillboardTexture].BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&desc[CBT_BillboardTexture], nullptr, &ConstantBuffer[CBT_BillboardTexture]);

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

void URenderer::UpdateBillboardConstant(FVector3 location, FVector3 scale, FMatrix viewProjection,
	FVector3 cameraRight, FVector3 cameraUp,
	FLinearColor tint,
	FVector2 uvScale, FVector2 uvOffset)
{
	if (ConstantBuffer[CBT_BillboardTexture])
	{
		D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
		DeviceContext->Map(ConstantBuffer[CBT_BillboardTexture], 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
		FBillboardConstants* constants = (FBillboardConstants*)constantbufferMSR.pData;
		{
			constants->Location = location;
			constants->Scale = scale;

			constants->ViewProjection = viewProjection;
			constants->Tint = tint;

			constants->UVOffset = uvOffset;
			constants->UVScale = uvScale;

			constants->CameraRight = cameraRight;
			constants->CameraUp = cameraUp;
		}
		DeviceContext->Unmap(ConstantBuffer[CBT_BillboardTexture], 0);
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

void URenderer::UpdateFontBuffer(const TArray<FVertexTextured>& vertices, const TArray<uint32>& indices, uint32 numCharacter)
{
	const uint32 numVertices = vertices.Num();

	// Increase Buffer Size if Needed
	// TODO: Move to a separate function to handle buffer resizing
	if (!FontTextureBuffer || numVertices > mTextVertexCapacity)
	{
		const uint32 newCapacity = FMath::Max(numVertices, static_cast<uint32>(mTextVertexCapacity * 2));

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.ByteWidth = newCapacity * sizeof(FVertexTextured);
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC; // 동적 설정
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // 동적으로 CPU가 쓰기 가능

		ID3D11Buffer* newBuffer = nullptr;
		Device->CreateBuffer(&bufferDesc, nullptr, &newBuffer);
		UE_LOG(Log, Render, "Font buffer CREATE: capacity %u -> %u",
			mTextVertexCapacity, newCapacity);

		// 새 버퍼 생성에 성공한 뒤 기존 버퍼를 교체
		//ReleaseFontAtlasQuad();
		if (FontTextureBuffer)
		{
			FontTextureBuffer->Release();
		}
		FontTextureBuffer = newBuffer;
		mTextVertexCapacity = newCapacity;
	}

	// Map the vertex buffer and copy the vertex data
	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
	DeviceContext->Map(FontTextureBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, vertices.GetData(), numVertices * sizeof(FVertexTextured));
	DeviceContext->Unmap(FontTextureBuffer, 0);



	const uint32 numIndices = indices.Num();
	ensureFontIndexBuffer(numIndices / 4);
}

bool URenderer::UpdateUnicodeFontBuffer(const FTextMesh& textMesh)
{
	if (!Device || !DeviceContext)
	{
		return false;
	}

	if (textMesh.FontRenderMode != EFontRenderMode::MSDF
		|| textMesh.Vertices.Num() <= 0
		|| textMesh.Indices.Num() <= 0)
	{
		return false;
	}

	const UINT vertexCount =
		static_cast<UINT>(textMesh.Vertices.Num());

	const UINT indexCount =
		static_cast<UINT>(textMesh.Indices.Num());

	// 현재 폰트 메시: 사각형마다 정점 4개, 인덱스 6개
	if (vertexCount % 4 != 0
		|| indexCount % 6 != 0
		|| vertexCount / 4 != indexCount / 6)
	{
		return false;
	}

	const UINT quadCount = vertexCount / 4;

	constexpr UINT vertexSize =
		static_cast<UINT>(sizeof(FVertexTextured));

	const UINT maxVertexCount =
		(std::numeric_limits<UINT>::max)() / vertexSize;

	if (vertexCount > maxVertexCount)
	{
		return false;
	}

	// 정점 버퍼가 부족하면 확장
	if (!UnicodeFontVertexBuffer
		|| vertexCount > UnicodeFontVertexCapacity)
	{
		UINT newCapacity = vertexCount;

		if (UnicodeFontVertexCapacity <= maxVertexCount / 2)
		{
			newCapacity = (std::max)(
				vertexCount, UnicodeFontVertexCapacity * 2);
		}

		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = newCapacity * vertexSize;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		Microsoft::WRL::ComPtr<ID3D11Buffer> newBuffer;

		const HRESULT hr = Device->CreateBuffer(
			&desc, nullptr, newBuffer.GetAddressOf());

		if (FAILED(hr))
		{
			return false;
		}

		if (UnicodeFontVertexBuffer)
		{
			UnicodeFontVertexBuffer->Release();
		}

		UnicodeFontVertexBuffer = newBuffer.Detach();
		UnicodeFontVertexCapacity = newCapacity;
	}

	// 사각형 개수에 맞는 인덱스 버퍼 확보
	if (!ensureUnicodeFontIndexBuffer(quadCount))
	{
		return false;
	}

	// 정점의 위치와 UV 업로드
	D3D11_MAPPED_SUBRESOURCE mapped{};

	const HRESULT hr = DeviceContext->Map(UnicodeFontVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	if (FAILED(hr))
	{
		return false;
	}

	std::memcpy(mapped.pData, textMesh.Vertices.GetData(), static_cast<size_t>(vertexCount) * vertexSize);

	DeviceContext->Unmap(UnicodeFontVertexBuffer, 0);

	return true;
}

void URenderer::UpdateBlendState(EBlendStateType blendState)
{
	if (!DeviceContext || blendState >= EBlendStateType::BST_Count)
	{
		return;
	}
	DeviceContext->OMSetBlendState(BlendState[blendState], nullptr, 0xffffffff);
}



//void URenderer::UpdateParticleBuffer(const TArray<FVertexTextured>& vertices, const TArray<uint32>& indices)
//{
//	assert(ParticleVertexBuffer || BlendState[BST_Additive]);
//
//	const uint32 numVertices = vertices.Num();
//
//	// Map the vertex buffer and copy the vertex data
//	D3D11_MAPPED_SUBRESOURCE mappedResource = {};
//	DeviceContext->Map(ParticleVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
//	memcpy(mappedResource.pData, vertices.GetData(), numVertices * sizeof(FVertexTextured));
//	DeviceContext->Unmap(ParticleVertexBuffer, 0);
//
//	// Map the index buffer and copy the index data
//	D3D11_MAPPED_SUBRESOURCE mappedIndexResource = {};
//	DeviceContext->Map(ParticleIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedIndexResource);
//	memcpy(mappedIndexResource.pData, indices.GetData(), indices.Num() * sizeof(uint32));
//	DeviceContext->Unmap(ParticleIndexBuffer, 0);
//}

void URenderer::OnResize(UINT width, UINT height, float viewportWidth, float viewportHeight)
{
	if (!SwapChain || width == 0 || height == 0) return;
	if (ViewportInfo.Width == viewportWidth && ViewportInfo.Height == viewportHeight) return;

	//해상도에 의존하는 프레임 버퍼와 뎁스 스텐실 버퍼를 재생성한다.
	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	releaseFrameBuffer();
	releaseDepthStencilBuffer();

	HRESULT hr = SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) return;

	DXGI_SWAP_CHAIN_DESC desc;
	SwapChain->GetDesc(&desc);

	ViewportInfo = { viewportWidth, 0.0f, static_cast<float>(width) - viewportWidth, viewportHeight, 0.0f, 1.0f };

	//상태는 이전에 생성한 걸 그대로 재사용
	createFrameBuffer();
	createDepthStencilBuffer(width, height);
}

void URenderer::CreateLoadingScreenResources()
{
	const FVertexTextured vertices[] =
	{
		{ -1.0f,  1.0f, 0.0f, 0.0f, 0.0f },
		{ 1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },

		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
		{ 1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
		{ 1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
	};

	LoadingScreenVertexBuffer = CreateVertexBuffer(
		vertices,
		sizeof(vertices));

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
			12,
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

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	Device->CreateSamplerState(&samplerDesc, &LoadingScreenSampler);

	if (vertexShaderBlob)
		vertexShaderBlob->Release();
	if (pixelShaderBlob)
		pixelShaderBlob->Release();
}

void URenderer::RenderFullscreenTexture(ID3D11ShaderResourceView* texture)
{
	UINT stride = sizeof(FVertexTextured);
	UINT offset = 0;

	DeviceContext->IASetVertexBuffers(
		0, 1, &LoadingScreenVertexBuffer, &stride, &offset);
	DeviceContext->IASetInputLayout(LoadingScreenInputLayout);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->VSSetShader(LoadingScreenVertexShader, nullptr, 0);
	DeviceContext->PSSetShader(LoadingScreenPixelShader, nullptr, 0);
	DeviceContext->PSSetShaderResources(0, 1, &texture);
	DeviceContext->PSSetSamplers(0, 1, &LoadingScreenSampler);
	DeviceContext->OMSetBlendState(BlendState[BST_Default], nullptr, 0xffffffff);

	DeviceContext->OMSetDepthStencilState(nullptr, 0);
	DeviceContext->Draw(6, 0);
}

void URenderer::ClearDepth()
{
	DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

ID3D11Buffer* URenderer::CreatePrimitiveIndexBuffer(const UINT* indices, UINT indexCount)
{
	if (!indices || indexCount == 0)
		return nullptr;

	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.ByteWidth = indexCount * sizeof(unsigned int);
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER; // 인데스 버퍼라고 명시

	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = indices;

	ID3D11Buffer* buffer = nullptr;
	HRESULT hr = Device->CreateBuffer(&desc, &data, &buffer);

	return SUCCEEDED(hr) ? buffer : nullptr;
}

bool URenderer::ensureFontIndexBuffer(UINT fontCpunt)
{
	if (fontCpunt == 0)
		return true;

	if (FontIndexBuffer && fontCpunt <= mTextIndexCapacity)
		return true;

	const UINT newCapacity = (std::max)(fontCpunt, mTextIndexCapacity * 2);

	std::vector<UINT> indices;
	indices.reserve(static_cast<size_t>(newCapacity) * 6);

	for (UINT i = 0; i < newCapacity; ++i)
	{
		const UINT base = i * 4; // 다음 문자의 시작 정점 번호로 이동

		indices.push_back(base);
		indices.push_back(base + 1);
		indices.push_back(base + 2);

		indices.push_back(base + 2);
		indices.push_back(base + 3);
		indices.push_back(base + 0);
	}

	/*	D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(UINT));
		desc.Usage = D3D11_USAGE_IMMUTABLE; //
		desc.BindFlags = D3D11_BIND_INDEX_BUFFER; // 인덱스 버퍼라고 명시

		D3D11_SUBRESOURCE_DATA data = {};
		data.pSysMem = indices.data();

		ID3D11Buffer* newBuffer = nullptr;
		HRESULT hr = Device->CreateBuffer(&desc, &data, &newBuffer);*/

	ID3D11Buffer* newBuffer = CreatePrimitiveIndexBuffer(indices.data(), static_cast<UINT>(indices.size())
	);

	if (!newBuffer)
		return false;

	if (FontIndexBuffer)
		FontIndexBuffer->Release();

	FontIndexBuffer = newBuffer;
	mTextIndexCapacity = newCapacity;
	return true;
}

bool URenderer::ensureUnicodeFontIndexBuffer(UINT quadCount)
{
	if (quadCount == 0)
	{
		return true;
	}

	if (!Device)
	{
		return false;
	}

	// 이미 충분하면 기존 버퍼 재사용
	if (UnicodeFontIndexBuffer
		&& quadCount <= UnicodeFontIndexCapacity)
	{
		return true;
	}

	// 사각형 하나 = 인덱스 6개
	constexpr UINT indicesPerQuad = 6;
	constexpr UINT bytesPerQuad =
		indicesPerQuad * static_cast<UINT>(sizeof(uint32));

	const UINT maxQuadCount =
		(std::numeric_limits<UINT>::max)() / bytesPerQuad;

	if (quadCount > maxQuadCount)
	{
		return false;
	}

	UINT newCapacity = quadCount;

	if (UnicodeFontIndexCapacity <= maxQuadCount / 2)
	{
		newCapacity = (std::max)(
			quadCount, UnicodeFontIndexCapacity * 2);
	}

	std::vector<uint32> indices;
	indices.reserve(
		static_cast<size_t>(newCapacity) * indicesPerQuad);

	for (UINT i = 0; i < newCapacity; ++i)
	{
		const uint32 base = i * 4;

		indices.push_back(base + 0);
		indices.push_back(base + 1);
		indices.push_back(base + 2);

		indices.push_back(base + 2);
		indices.push_back(base + 3);
		indices.push_back(base + 0);
	}

	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = newCapacity * bytesPerQuad;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initialData{};
	initialData.pSysMem = indices.data();

	Microsoft::WRL::ComPtr<ID3D11Buffer> newBuffer;

	const HRESULT hr = Device->CreateBuffer(&desc, &initialData, newBuffer.GetAddressOf());

	if (FAILED(hr))
	{
		return false;
	}

	// 생성에 성공한 뒤 교체
	if (UnicodeFontIndexBuffer)
	{
		UnicodeFontIndexBuffer->Release();
	}

	UnicodeFontIndexBuffer = newBuffer.Detach();
	UnicodeFontIndexCapacity = newCapacity;

	return true;
}

void URenderer::releaseFontTexture()
{
	// 매번 해제하면 버퍼를 재사용 불가능
	//ReleaseFontAtlasQuad();

	if (FontSamplerState)
	{
		FontSamplerState->Release();
		FontSamplerState = nullptr;
	}
}

void URenderer::ReleasePrimitiveTextureResources(
	ID3D11ShaderResourceView* textureSRV, ID3D11SamplerState* samplerState)
{
	if (textureSRV)
	{
		textureSRV->Release();
		textureSRV = nullptr;
	}

	if (samplerState)
	{
		samplerState->Release();
		samplerState = nullptr;
	}
}


