#include "Rendering/Renderer.h"
#include "Rendering/RenderingPipeline.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Rendering/Primitives/Cube.h"
#include "Rendering/Primitives/Sphere.h"
#include "Rendering/Primitives/Triangle.h"
#include "Rendering/Primitives/Circle.h"
#include "Rendering/Primitives/GizmoArrow.h"
#include "Rendering/Primitives/Primitives.h"
#include "Rendering/Primitives/TexturedPrimitives.h"
#include <d3d11sdklayers.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

static void Check(bool Value, const char* Message)
{
    if (!Value) throw std::runtime_error(Message);
}

template<size_t V, size_t I>
static void CheckMesh(const FVertexSimple (&Vertices)[V], const uint32 (&Indices)[I])
{
    static_assert(I % 3 == 0);
    for (uint32 Index : Indices) Check(Index < V, "Primitive index out of range");
    for (const auto& Vertex : Vertices)
    {
        const float NormalLength = Vertex.nx * Vertex.nx + Vertex.ny * Vertex.ny + Vertex.nz * Vertex.nz;
        Check(std::abs(NormalLength - 1) < 0.001f, "Primitive normal is not normalized");
        Check(std::isfinite(Vertex.u) && std::isfinite(Vertex.v), "Invalid primitive UV");
    }
}

template<class F>
static void ExpectVertices(URenderer& Renderer, UINT64 Count, F Draw)
{
    ComPtr<ID3D11Query> Query;
    D3D11_QUERY_DESC Desc{ D3D11_QUERY_PIPELINE_STATISTICS, 0 };
    Check(SUCCEEDED(Renderer.GetDevice()->CreateQuery(&Desc, &Query)), "Create GPU query");
    auto* Context = Renderer.GetDeviceContext();
    Context->Begin(Query.Get());
    Draw();
    Context->End(Query.Get());
    D3D11_QUERY_DATA_PIPELINE_STATISTICS Stats{};
    const auto Deadline = GetTickCount64() + 10000;
    HRESULT Result;
    while ((Result = Context->GetData(Query.Get(), &Stats, sizeof(Stats), 0)) == S_FALSE)
    {
        Check(GetTickCount64() < Deadline, "GPU query timed out");
        Sleep(1);
    }
    Check(SUCCEEDED(Result) && Stats.IAVertices == Count, "Unexpected GPU vertex count");
}

static void TestRendering(URenderer& Renderer)
{
    auto Target = Renderer.CreateRenderTarget2D(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM);
    auto Depth = Renderer.CreateDepthStencil(64, 64);
    Check(Target->RTV && Depth->DSV, "Create render targets");
    Renderer.BindRenderTarget(Target, Depth);
    Renderer.SetViewport(0, 0, 64, 64);
    FLineGraphicsPipeline Lines(Renderer);
    FMeshGraphicsPipeline Mesh(Renderer);
    FQuadGraphicsPipeline Quads(Renderer);
    FStencilMarkGraphicsPipeline Mark(Renderer);
    FStencilOutlineGraphicsPipeline Outline(Renderer);
    FLine2DGraphicsPipeline Lines2D(Renderer);
    FCircle2DGraphicsPipeline Circles(Renderer);
    FTriangle2DGraphicsPipeline Triangles(Renderer);
    FWorldAxisGraphicsPipeline Axes(Renderer);
    FWorldGridGraphicsPipeline Grids(Renderer);
    const auto Identity = FMatrix::Identity;

    TArray<FRenderLineInfo> LineInfos;
    for (int I = 0; I < 1025; ++I)
        LineInfos.Add({ FVector4(1, 0, 0, 1), FVector(-.5f, 0, .5f), 1, FVector(.5f, 0, .5f), 0 });
    ExpectVertices(Renderer, 6150, [&] { Lines.Draw(LineInfos, Identity, FVector2(64, 64)); });

    // Nonzero normal data exposes COLOR/TEXCOORD offset mistakes in the merged layout.
    const FVertexSimple Vertices[] = {
        { -.5f, -.5f, .5f, 0, 0, 1, 1, 0, 0, 1, 0, 0 },
        { 0, .5f, .5f, 0, 0, 1, 1, 0, 0, 1, .5f, 1 },
        { .5f, -.5f, .5f, 0, 0, 1, 1, 0, 0, 1, 1, 0 }
    };
    const uint32 Indices[] = { 0, 1, 2 };
    auto Asset = MakeShared<UStaticMeshAsset>();
    Asset->Initialize(FName("MergeSmoke"), Renderer, Vertices, 3, Indices, 3);
    Check(Asset->GetVertexCount() == 3 && Asset->GetIndexCount() == 3, "Element-count buffer API");
    FRenderInfo Info{};
    Info.StaticMesh = Asset;
    Info.WorldTransformMatrix = Identity;
    Info.Color = FLinearColor(1, 0, 0, 1);
    TArray<FRenderInfo> Infos{ Info };
    Renderer.BindRenderTarget(Target, Depth);
    ExpectVertices(Renderer, 3, [&] { Mesh.Draw(Infos, Identity); });

    D3D11_TEXTURE2D_DESC ReadDesc{};
    Target->Texture->GetDesc(&ReadDesc);
    ReadDesc.Usage = D3D11_USAGE_STAGING;
    ReadDesc.BindFlags = 0;
    ReadDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> Readback;
    Check(SUCCEEDED(Renderer.GetDevice()->CreateTexture2D(&ReadDesc, nullptr, &Readback)), "Create readback");
    auto* Context = Renderer.GetDeviceContext();
    Context->CopyResource(Readback.Get(), Target->Texture.Get());
    D3D11_MAPPED_SUBRESOURCE Mapped{};
    Check(SUCCEEDED(Context->Map(Readback.Get(), 0, D3D11_MAP_READ, 0, &Mapped)), "Read rendered mesh");
    const auto* Pixel = static_cast<const uint8*>(Mapped.pData) + 32 * Mapped.RowPitch + 32 * 4;
    const bool Red = Pixel[0] == 255 && Pixel[1] == 0 && Pixel[2] == 0;
    Context->Unmap(Readback.Get(), 0);
    Check(Red, "Merged mesh layout did not render the red vertex color");
    ExpectVertices(Renderer, 3, [&] { Mark.Draw(Infos, Identity); });
    ExpectVertices(Renderer, 3, [&] { Outline.Draw(Infos, Identity); });

    TArray<FRenderQuadInfo> QuadInfos;
    for (int Phase = 0; Phase < 3; ++Phase)
    {
        FRenderQuadInfo Quad{};
        Quad.Model = Identity;
        Quad.EnableDepthTest = Phase != 2;
        Quad.EnableDepthWrite = Phase == 0;
        QuadInfos.Add(Quad);
    }
    for (int Phase = 0; Phase < 3; ++Phase)
        ExpectVertices(Renderer, 6, [&] { Quads.Draw(QuadInfos, static_cast<EQuadRenderPhase>(Phase), Identity); });
    Check(QuadInfos.IsEmpty(), "Quad phases did not consume all submissions");
    TArray<FRenderLine2DInfo> Line2D{ { FVector2(0, 0), FVector2(32, 32), FVector4(1, 1, 1, 1), 2 } };
    TArray<FRenderCircle2DInfo> Circle{ { FVector2(32, 32), FVector4(1, 1, 1, 1), 4 } };
    TArray<FRenderTriangle2DInfo> Triangle{ { FVector2(32, 32), FVector4(1, 1, 1, 1), 4, 0 } };
    TArray<FRenderWorldAxisInfo> Axis{ { FVector4(1, 1, 1, 1), FVector(1, 0, 0), .002f } };
    TArray<FRenderWorldGridInfo> Grid{ { 1 } };
    ExpectVertices(Renderer, 6, [&] { Lines2D.Draw(Line2D, Renderer.GetProjection2D()); });
    ExpectVertices(Renderer, 6, [&] { Circles.Draw(Circle, Renderer.GetProjection2D()); });
    ExpectVertices(Renderer, 3, [&] { Triangles.Draw(Triangle, Renderer.GetProjection2D()); });
    ExpectVertices(Renderer, 6, [&] { Axes.Draw(Axis, Identity, Identity, FVector2(64, 64)); });
    ExpectVertices(Renderer, 6, [&] { Grids.Draw(Grid, Identity, FVector(0, 0, 1)); });
    Renderer.BindRenderTarget(Target, {}, false); // Depth is optional.
}

int main()
{
    try
    {
        CheckMesh(Cube_vertices, Cube_indices);
        CheckMesh(Sphere_vertices, Sphere_indices);
        CheckMesh(Triangle_vertices, Triangle_indices);
        CheckMesh(Circle_vertices, Circle_indices);
        CheckMesh(GizmoArrow_vertices, GizmoArrow_indices);
        CheckMesh(Quad_vertices, Quad_indices);
        CheckMesh(Fullscreen_vertices, Fullscreen_indices);
        CheckMesh(CubeTextureVertices, CubeTextureIndices);
        CheckMesh(SphereTextureVertices, SphereTextureIndices);
        CheckMesh(QuadTextureIndexedVertices, QuadTextureIndexedIndices);
        URenderer Renderer;
        Check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_DEBUG,
            nullptr, 0, D3D11_SDK_VERSION, &Renderer.Device, nullptr, &Renderer.DeviceContext)), "Create WARP device");
        TestRendering(Renderer);
        ComPtr<ID3D11InfoQueue> Queue;
        Check(SUCCEEDED(Renderer.Device->QueryInterface(IID_PPV_ARGS(&Queue))), "DX11 debug queue");
        for (UINT64 I = 0; I < Queue->GetNumStoredMessages(); ++I)
        {
            SIZE_T Size = 0;
            Queue->GetMessage(I, nullptr, &Size);
            std::vector<uint8> Storage(Size);
            auto* Message = reinterpret_cast<D3D11_MESSAGE*>(Storage.data());
            Queue->GetMessage(I, Message, &Size);
            if (Message->Severity <= D3D11_MESSAGE_SEVERITY_ERROR)
                throw std::runtime_error(Message->pDescription);
        }
        Renderer.Release();
        std::cout << "Baked primitives, 10 pipelines, indexed GPU draws, color readback and DX11 validation passed.\n";
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << Error.what() << '\n';
        return 1;
    }
}
