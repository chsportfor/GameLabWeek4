#include "Rendering/Renderer.h"
#include "Rendering/RenderingPipeline.h"
#include "Rendering/RenderAssets.h"
#include "Rendering/TextMesh.h"
#include "Core/AssetSystem/AssetSource/FontAtlasAssetSource.h"
#include "Core/AssetSystem/AssetSource/StaticMeshAssetSource.h"
#include "Core/Object/ObjectFactory.h"
#include "Engine/Actor.h"
#include "Engine/Components/NameComponent.h"
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

static void TestAssetRendering(URenderer& Renderer)
{
    Renderer.InitializeDeviceResources();
    FFileManager Files("Assets");
    FRenderAssets Assets;
    Assets.LoadLoadingScreen(Renderer, Files);
    Assets.LoadSceneAssets(Renderer, Files);
    const auto Cube = Assets.GetMesh(EPrimitive::EP_Cube);
    const auto Loading = Assets.GetLoadingScreen();
    Assets.LoadLoadingScreen(Renderer, Files);
    Assets.LoadSceneAssets(Renderer, Files);
    Check(Assets.GetLoadingScreen() == Loading && Assets.GetMesh(EPrimitive::EP_Cube) == Cube,
        "Repeated catalog load must reuse assets");
    Check(Cube->GetIndexCount() == 36 && Assets.GetParticleMesh()->GetIndexCount() == 6,
        "Built-in mesh upload");
    for (auto Type : {EPrimitive::EP_Cube, EPrimitive::EP_Sphere, EPrimitive::EP_GizmoArrow,
        EPrimitive::EP_Circle, EPrimitive::EP_Triangle, EPrimitive::EP_BillboardQuad})
        Check(Assets.GetMesh(Type) && Assets.GetMesh(Type)->GetVertexBuffer(), "Missing primitive asset");

    auto Target = Renderer.CreateRenderTarget2D(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM);
    auto Depth = Renderer.CreateDepthStencil(64, 64);
    Renderer.BindRenderTarget(Target, Depth);
    Renderer.SetViewport(0, 0, 64, 64);
    auto* Context = Renderer.GetDeviceContext();
    const auto Identity = FMatrix::Identity;
    ExpectVertices(Renderer, 6, [&] { Renderer.RenderFullscreenTexture(*Assets.GetFullscreenMesh(), *Loading); });
    ComPtr<ID3D11ShaderResourceView> Bound;
    Context->PSGetShaderResources(0, 1, &Bound);
    Check(Bound == Loading->GetSRV(), "Loading screen asset binding");

    // A known texel catches incorrect FVertexSimple UV offsets and draw-state regressions.
    const uint32 Green = 0xff00ff00;
    D3D11_TEXTURE2D_DESC Desc{};
    Desc.Width = Desc.Height = Desc.MipLevels = Desc.ArraySize = Desc.SampleDesc.Count = 1;
    Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.Usage = D3D11_USAGE_IMMUTABLE;
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    auto Image = Renderer.CreateTexture2D(Desc, &Green);
    auto GreenAsset = TSharedPtr<UTexture2DAsset>(FObjectFactory::ConstructObject<UTexture2DAsset>(
        FName("Test.Green"), Image, Renderer.CreateShaderResourceView(Image)));
    Renderer.RenderFullscreenTexture(*Assets.GetFullscreenMesh(), *GreenAsset);
    Target->Texture->GetDesc(&Desc);
    Desc.Usage = D3D11_USAGE_STAGING;
    Desc.BindFlags = 0;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> Readback;
    Check(SUCCEEDED(Renderer.Device->CreateTexture2D(&Desc, nullptr, &Readback)), "Asset readback allocation");
    Context->CopyResource(Readback.Get(), Target->Texture.Get());
    D3D11_MAPPED_SUBRESOURCE Mapped{};
    Check(SUCCEEDED(Context->Map(Readback.Get(), 0, D3D11_MAP_READ, 0, &Mapped)), "Asset readback");
    const auto* Pixel = static_cast<const uint8*>(Mapped.pData) + 32 * Mapped.RowPitch + 32 * 4;
    const bool IsGreen = Pixel[0] == 0 && Pixel[1] == 255 && Pixel[2] == 0;
    Context->Unmap(Readback.Get(), 0);
    Check(IsGreen, "Asset texture did not reach the framebuffer");

    Renderer.PrepareTexturedPrimitive();
    Renderer.UpdateTextureConstant(Identity, Identity, FLinearColor(1, 1, 1, 1));
    ExpectVertices(Renderer, 36, [&] { Renderer.RenderTexturedMesh(
        *Assets.GetMesh(EPrimitive::EP_Cube, true), *Assets.GetTexture(EPrimitive::EP_Cube)); });
    Renderer.PrepareSimpleInstanced();
    Renderer.UpdateSimpleConstant(Identity, Identity);
    const FInstanceData Instances[] = {{Identity, FLinearColor(1, 0, 0, 1)}, {Identity, FLinearColor(0, 1, 0, 1)}};
    ExpectVertices(Renderer, 72, [&] { Check(Renderer.RenderSimpleInstanced(*Cube, Instances, 2), "Instanced asset draw"); });
    Renderer.PrepareGizmo();
    ExpectVertices(Renderer, 180, [&] { Renderer.RenderSimplePrimitive(*Assets.GetMesh(EPrimitive::EP_GizmoArrow)); });
    Renderer.PrepareHighlight();
    ExpectVertices(Renderer, 72, [&] { Renderer.RenderHighlight(*Cube, Identity, Identity, Identity); });
    Renderer.PrepareParticle();
    Renderer.UpdateParticleConstant(FVector(0, 0, .5f), FVector(1), Identity,
        FVector(1, 0, 0), FVector(0, 1, 0), 6, 6, 0, 1, .5f, FLinearColor(1, 1, 1, 1));
    ExpectVertices(Renderer, 6, [&] { Renderer.RenderTexturedMesh(*Assets.GetParticleMesh(),
        *Assets.GetTexture(EPrimitive::EP_BillboardQuad), D3D11_TEXTURE_ADDRESS_CLAMP); });

    const auto Font = Assets.GetDefaultFont();
    Renderer.UpdateFontConstant(FVector(0, 0, .5f), FVector(1), Identity,
        FVector(1, 0, 0), FVector(0, 1, 0), FLinearColor(1, 1, 1, 1));
    FTextMesh Text;
    Text.SetUnicodeText("ASCII + 한글", Font->GetFontResource());
    ExpectVertices(Renderer, Text.Indices.Num(), [&] { Check(Renderer.RenderText(Text, *Font), "MSDF asset draw"); });
    Bound.Reset();
    Context->PSGetShaderResources(0, 1, &Bound);
    Check(Bound == Font->GetSRV(), "MSDF asset binding");
    FAssetManager Manager;
    FFontAtlasAssetLoader FontLoader(Renderer.Device);
    FFontAtlasAssetSource BitmapSource(Files, "Fonts/EnglishBigFontAtlas.dds");
    auto Bitmap = Manager.Load<UFontAtlasAsset>("Test.Bitmap", FontLoader, BitmapSource);
    Text.SetText("A", Bitmap->GetFontResource());
    ExpectVertices(Renderer, 6, [&] { Check(Renderer.RenderText(Text, *Bitmap), "Bitmap after larger MSDF text"); });
    Bound.Reset();
    Context->PSGetShaderResources(0, 1, &Bound);
    Check(Bound == Bitmap->GetSRV(), "Bitmap must use its own atlas");
    Check(!Renderer.RenderText(Text, *Font), "Mismatched text/atlas must fail");
    Text.Indices[0] = Text.Vertices.Num();
    Check(!Renderer.RenderText(Text, *Bitmap), "Invalid dynamic text index must fail");
    Text.SetText("", Bitmap->GetFontResource());
    ExpectVertices(Renderer, 0, [&] { Check(Renderer.RenderText(Text, *Bitmap), "Empty text"); });

    // Cache ownership must not invalidate components/submissions that retain an asset.
    std::weak_ptr<UFontAtlasAsset> WeakBitmap = Bitmap;
    Manager.Clear();
    Check(!WeakBitmap.expired() && Bitmap->GetSRV(), "Retained asset after cache clear");
    Bitmap.reset();
    Check(WeakBitmap.expired(), "Unreferenced asset must be destroyed");
    FObjectFactory::SetDefaultFontAsset(Font);
    auto Actor = std::unique_ptr<AActor>(FObjectFactory::SpawnPrimitiveActor(EPrimitive::EP_Cube,
        FVector(0), FRotator(), FVector(1)));
    Actor->SetName("에셋 이름표");
    TArray<FRenderInfo> Infos;
    Actor->GetRenderInfos(&Infos);
    bool FoundName = false;
    for (const auto& Info : Infos)
        if (Info.Textmesh) { Check(Info.FontAtlas == Font && !Info.Textmesh->Indices.IsEmpty(), "Name component atlas"); FoundName = true; }
    Check(FoundName, "Actor name submission");
    FObjectFactory::SetDefaultFontAsset(nullptr);
    Assets.Clear();
    for (const auto& Info : Infos)
        if (Info.Textmesh) Check(Renderer.RenderText(*Info.Textmesh, *Info.FontAtlas), "Live component after catalog clear");
    std::cout << "Asset catalog/cache, meshes, textures, loading screen, instancing, gizmos, particles, bitmap/MSDF text and lifetime passed.\n";
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
        TestAssetRendering(Renderer);
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
