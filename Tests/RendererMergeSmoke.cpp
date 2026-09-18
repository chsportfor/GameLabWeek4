#include "Rendering/Renderer.h"
#include "Rendering/RenderingPipeline.h"
#include "Rendering/pipelines/FWorldGridGraphicsPipeline.h"
#include "Rendering/pipelines/FWorldAxisGraphicsPipeline.h"
#include "Rendering/pipelines/FTriangle2DGraphicsPipeline.h"
#include "Rendering/pipelines/FCircle2DGraphicsPipeline.h"
#include "Rendering/pipelines/FLine2DGraphicsPipeline.h"
#include "Rendering/pipelines/FQuadGraphicsPipeline.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Rendering/TextMesh.h"
#include <array>
#include "Core/AssetSystem/AssetSource/FontAtlasAssetSource.h"
#include "Core/AssetSystem/AssetSource/StaticMeshAssetSource.h"
#include "Core/Object/ObjectFactory.h"
#include "Engine/Actor.h"
#include "Engine/Components/NameComponent.h"
#include "Engine/Components/ParticleSubUVComponent.h"
#include "Engine/Components/SphereComponent.h"
#include "Engine/World.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Rendering/Primitives/Cube.h"
#include "Rendering/Primitives/Sphere.h"
#include "Rendering/Primitives/Triangle.h"
#include "Rendering/Primitives/Circle.h"
#include "Rendering/Primitives/GizmoArrow.h"
#include "Rendering/Primitives/Primitives.h"
#include "Engine/InitializeAssets.h"
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

static std::array<uint8, 4> ReadCenter(URenderer& Renderer, const TSharedPtr<FRenderTarget2D>& Target,
    int OffsetX = 0, int OffsetY = 0)
{
    D3D11_TEXTURE2D_DESC Desc{}; Target->Texture->GetDesc(&Desc);
    Desc.Usage = D3D11_USAGE_STAGING; Desc.BindFlags = 0; Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> Staging;
    Check(SUCCEEDED(Renderer.Device->CreateTexture2D(&Desc, nullptr, &Staging)), "Pixel readback texture");
    auto* Context = Renderer.DeviceContext;
    Context->CopyResource(Staging.Get(), Target->Texture.Get());
    D3D11_MAPPED_SUBRESOURCE Mapped{};
    Check(SUCCEEDED(Context->Map(Staging.Get(), 0, D3D11_MAP_READ, 0, &Mapped)), "Pixel readback map");
    const auto* Pixel = static_cast<const uint8*>(Mapped.pData) +
        (Desc.Height / 2 + OffsetY) * Mapped.RowPitch + (Desc.Width / 2 + OffsetX) * 4;
    std::array<uint8, 4> Result{Pixel[0], Pixel[1], Pixel[2], Pixel[3]};
    Context->Unmap(Staging.Get(), 0);
    return Result;
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
    FRenderView View;
    View.ViewportSize = FVector2(64, 64);
    View.Projection2D = Renderer.GetProjection2D();

    TArray<FRenderLineInfo> LineInfos;
    for (int I = 0; I < 1025; ++I)
        LineInfos.Add({ FVector4(1, 0, 0, 1), FVector(-.5f, 0, .5f), 1, FVector(.5f, 0, .5f), 0 });
    ExpectVertices(Renderer, 6150, [&] { Lines.Draw(LineInfos, View); });

    // Nonzero normal data exposes COLOR/TEXCOORD offset mistakes in the merged layout.
    const FVertexSimple Vertices[] = {
        { -.5f, -.5f, .5f, 0, 0, 1, 1, 0, 0, 1, 0, 0 },
        { 0, .5f, .5f, 0, 0, 1, 1, 0, 0, 1, .5f, 1 },
        { .5f, -.5f, .5f, 0, 0, 1, 1, 0, 0, 1, 1, 0 }
    };
    const uint32 Indices[] = { 0, 1, 2 };
    auto Asset = MakeShared<FStaticMeshAsset>(FName("MergeSmoke"), Renderer, Vertices, 3, Indices, 3);
    Check(Asset->GetVertexCount() == 3 && Asset->GetIndexCount() == 3, "Element-count buffer API");
    FRenderMeshInfo Info{};
    Info.StaticMesh = Asset;
    Info.WorldTransformMatrix = Identity;
    Info.Color = FLinearColor(1, 0, 0, 1);
    TArray<FRenderMeshInfo> Infos{ Info };
    Renderer.BindRenderTarget(Target, Depth);
    ExpectVertices(Renderer, 3, [&] { Mesh.Draw(Infos, View); });

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
    ExpectVertices(Renderer, 3, [&] { Mark.Draw(Infos, View); });
    ExpectVertices(Renderer, 3, [&] { Outline.Draw(Infos, View); });

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
        ExpectVertices(Renderer, 6, [&] { Quads.Draw(QuadInfos, View, static_cast<EQuadRenderPhase>(Phase)); });
    Check(QuadInfos.IsEmpty(), "Quad phases did not consume all submissions");
    TArray<FRenderLine2DInfo> Line2D{ { FVector2(0, 0), FVector2(32, 32), FVector4(1, 1, 1, 1), 2 } };
    TArray<FRenderCircle2DInfo> Circle{ { FVector2(32, 32), FVector4(1, 1, 1, 1), 4 } };
    TArray<FRenderTriangle2DInfo> Triangle{ { FVector2(32, 32), FVector4(1, 1, 1, 1), 4, 0 } };
    TArray<FRenderWorldAxisInfo> Axis{ { FVector4(1, 1, 1, 1), FVector(1, 0, 0), .002f } };
    TArray<FRenderWorldGridInfo> Grid{ { 1 } };
    ExpectVertices(Renderer, 6, [&] { Lines2D.Draw(Line2D, View); });
    ExpectVertices(Renderer, 6, [&] { Circles.Draw(Circle, View); });
    ExpectVertices(Renderer, 3, [&] { Triangles.Draw(Triangle, View); });
    ExpectVertices(Renderer, 6, [&] { Axes.Draw(Axis, View); });
    ExpectVertices(Renderer, 6, [&] { Grids.Draw(Grid, View); });
    Renderer.BindRenderTarget(Target, {}, false); // Depth is optional.
}

static void TestQuadRendering(URenderer& Renderer)
{
    FQuadGraphicsPipeline Quads(Renderer);
    FRenderView View;
    auto Target = Renderer.CreateRenderTarget2D(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM);
    auto Depth = Renderer.CreateDepthStencil(64, 64);
    Renderer.SetViewport(0, 0, 64, 64);
    const uint32 Pixels[] = {0xff0000ff, 0xffff0000}; // Red, blue.
    D3D11_TEXTURE2D_DESC Desc{};
    Desc.Width = 2;
    Desc.Height = Desc.MipLevels = Desc.ArraySize = Desc.SampleDesc.Count = 1;
    Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.Usage = D3D11_USAGE_IMMUTABLE;
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    auto Image = Renderer.CreateTexture2D(Desc, Pixels);
    auto Texture = MakeShared<FTexture2DAsset>(
        FName("Test.QuadFrames"), Image, Renderer.CreateShaderResourceView(Image));
    FRenderQuadInfo Quad{};
    Quad.Model = FMatrix::Identity;
    // YZ quad -> XY screen plane, with depth .5.
    Quad.Model.M[1][0] = 1; Quad.Model.M[1][1] = 0;
    Quad.Model.M[2][1] = 1; Quad.Model.M[2][2] = 0;
    Quad.Model.M[3][2] = .5f;
    Quad.Texture = Texture;
    Quad.SubUV = FVector4(.25f, .5f, 0, 0);
    Quad.NextSubUV = FVector4(.75f, .5f, 0, 0);
    Quad.EnableDepthWrite = false;
    auto DrawOne = [&]
    {
        Renderer.BindRenderTarget(Target, Depth);
        const float Black[4]{};
        Renderer.DeviceContext->ClearRenderTargetView(Target->RTV.Get(), Black);
        TArray<FRenderQuadInfo> Infos{Quad};
        ExpectVertices(Renderer, 6, [&] { Quads.Draw(Infos, View, EQuadRenderPhase::Transparent); });
        Check(Infos.IsEmpty(), "Quad submission was not consumed");
        return ReadCenter(Renderer, Target);
    };
    for (float Blend : {0.f, .5f, 1.f})
    {
        Quad.FrameBlend = Blend;
        const auto Pixel = DrawOne();
        Check(std::abs(int(Pixel[0]) - int(255 * (1 - Blend))) <= 1 && Pixel[1] == 0 &&
            std::abs(int(Pixel[2]) - int(255 * Blend)) <= 1, "Quad frame color interpolation");
    }
    Quad.FrameBlend = 0;
    Quad.SubUV.x = 1.25f;
    Quad.AddressMode = D3D11_TEXTURE_ADDRESS_CLAMP;
    Check(DrawOne()[2] == 255, "Particle clamp sampler");
    Quad.AddressMode = D3D11_TEXTURE_ADDRESS_WRAP;
    Check(DrawOne()[0] == 255, "Ordinary quad wrap sampler after clamp");
    Quad.Texture.reset();
    Quad.Color = FVector4(0, 1, 0, 1);
    Check(DrawOne()[1] == 255, "Untextured quad color regression");

    // Three overlapping layers expose swap-pop reordering (two layers would not).
    auto Red = Quad; Red.Color = FVector4(1, 0, 0, .5f); Red.BlendMode = ERenderBlendMode::Transparent;
    auto Green = Red; Green.Color = FVector4(0, 1, 0, .5f);
    auto Blue = Red; Blue.Color = FVector4(0, 0, 1, .5f);
    auto Opaque = Quad; Opaque.EnableDepthWrite = true; Opaque.Model.M[3][0] = 10;
    auto Overlay = Opaque; Overlay.EnableDepthTest = false;
    TArray<FRenderQuadInfo> Mixed{Red, Opaque, Green, Overlay, Blue, Opaque};
    Renderer.BindRenderTarget(Target, Depth);
    const float Black[4]{};
    Renderer.DeviceContext->ClearRenderTargetView(Target->RTV.Get(), Black);
    ExpectVertices(Renderer, 12, [&] { Quads.Draw(Mixed, View, EQuadRenderPhase::Opaque); });
    Check(Mixed.Num() == 4 && Mixed[0].Color.x == 1 && Mixed[1].Color.y == 1 &&
        !Mixed[2].EnableDepthTest && Mixed[3].Color.z == 1, "Opaque partition reordered other phases");
    ExpectVertices(Renderer, 18, [&] { Quads.Draw(Mixed, View, EQuadRenderPhase::Transparent); });
    const auto Pixel = ReadCenter(Renderer, Target);
    Check(std::abs(int(Pixel[0]) - 32) <= 1 && std::abs(int(Pixel[1]) - 64) <= 1 &&
        std::abs(int(Pixel[2]) - 128) <= 1, "Transparent quad ordering/alpha blend");
    Check(Mixed.Num() == 1 && !Mixed[0].EnableDepthTest, "Transparent compaction lost overlay");
    ComPtr<ID3D11DepthStencilState> State;
    Renderer.DeviceContext->OMGetDepthStencilState(&State, nullptr);
    D3D11_DEPTH_STENCIL_DESC StateDesc{}; State->GetDesc(&StateDesc);
    Check(StateDesc.DepthEnable && StateDesc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ZERO,
        "Transparent quad must depth-test without writing depth");
    ExpectVertices(Renderer, 6, [&] { Quads.Draw(Mixed, View, EQuadRenderPhase::Overlay); });
    Check(Mixed.IsEmpty(), "Overlay was not consumed");

    // Submit nearest first: the quad pass must sort by the frame camera's depth.
    View.Camera = FCamera(FVector(0), FRotator::FromDirection(FVector(0, 0, 1)));
    Red.Model.M[3][2] = .8f; Green.Model.M[3][2] = .5f; Blue.Model.M[3][2] = .2f;
    Mixed = {Blue, Red, Green};
    Renderer.BindRenderTarget(Target, Depth);
    Renderer.DeviceContext->ClearRenderTargetView(Target->RTV.Get(), Black);
    ExpectVertices(Renderer, 18, [&] { Quads.Draw(Mixed, View, EQuadRenderPhase::Transparent); });
    const auto Sorted = ReadCenter(Renderer, Target);
    Check(std::abs(int(Sorted[0]) - 32) <= 1 && std::abs(int(Sorted[1]) - 64) <= 1 &&
        std::abs(int(Sorted[2]) - 128) <= 1, "Quad pass must sort far to near");

    Red.BlendMode = Green.BlendMode = Blue.BlendMode = ERenderBlendMode::Additive;
    Mixed = {Red, Green, Blue};
    Renderer.BindRenderTarget(Target, Depth);
    Renderer.DeviceContext->ClearRenderTargetView(Target->RTV.Get(), Black);
    Quads.Draw(Mixed, View, EQuadRenderPhase::Transparent);
    const auto Added = ReadCenter(Renderer, Target);
    for (int I = 0; I < 3; ++I) Check(std::abs(int(Added[I]) - 128) <= 1, "Quad additive blend");
    std::cout << "Quad interpolation 0/0.5/1, wrap/clamp, mixed phases, alpha/additive and depth state passed.\n";
}

static void TestAssetManager(URenderer& Renderer)
{
    FAssetManager Manager;
    auto Loader = MakeShared<FStaticMeshAssetLoader>(Renderer);
    auto Source = MakeShared<FStaticMeshAssetSource>(Cube_vertices, Cube_indices);
    Manager.RegisterAsset("Test.ManagedMesh", Loader, Source);
    Check(!Manager.GetAsset("Test.ManagedMesh"), "Registration must not load an asset");
    auto Mesh = Manager.GetAssetAs<FStaticMeshAsset>("Test.ManagedMesh", true);
    Check(Mesh && Mesh->GetIndexCount() == 36, "Lazy asset loading");
    Check(!Manager.GetAssetAs<FTexture2DAsset>("Test.ManagedMesh"), "Typed lookup must reject a different asset type");
    Manager.RegisterAsset("Test.ManagedMesh", Loader, Source);
    Check(Manager.GetAsset("Test.ManagedMesh") == Mesh, "Repeated registration must reuse the loaded asset");
    std::weak_ptr<FStaticMeshAsset> Weak = Mesh;
    Manager.UnloadAsset("Test.ManagedMesh");
    Check(!Manager.GetAsset("Test.ManagedMesh") && !Weak.expired() && Mesh->GetVertexBuffer(),
        "Unloading must release only the manager reference");
    Mesh.reset();
    Check(Weak.expired(), "An unloaded asset must die after its final user releases it");
    Mesh = Manager.GetAssetAs<FStaticMeshAsset>("Test.ManagedMesh", true);
    Check(Mesh && Mesh->GetIndexCount() == 36, "Unloaded registrations must remain reloadable");
    Weak = Mesh;
    Manager.UnregisterAsset("Test.ManagedMesh");
    Check(!Manager.GetAsset("Test.ManagedMesh", true), "Unregistered assets must not reload");
    Mesh.reset();
    Check(Weak.expired(), "Unregister must release the loaded asset");
    int Registrations = 0;
    Manager.ForEachMetaInfo([&](const FAssetMetaInfo&) { ++Registrations; });
    Check(Registrations == 0, "Unregister must remove registration metadata");
    std::cout << "Asset registration, typed lookup, unload, reload and lifetime passed.\n";
}

static void TestAssetRendering(URenderer& Renderer)
{
    FFullscreenGraphicsPipeline Fullscreen(Renderer);
    FMeshGraphicsPipeline MeshPass(Renderer);
    FMeshGraphicsPipeline GizmoPass(Renderer, true);
    FInstancedMeshGraphicsPipeline Instanced(Renderer);
    FTextGraphicsPipeline TextPass(Renderer);
    FStencilMarkGraphicsPipeline MarkPass(Renderer);
    FStencilOutlineGraphicsPipeline OutlinePass(Renderer);
    FFileManager Files("Assets");
    FAssetManager Assets;
    RegisterLoadingScreenAssets(Assets, Renderer, Files);
    RegisterSceneAssets(Assets, Renderer, Files);
    const auto Cube = Assets.GetAssetAs<FStaticMeshAsset>(BuiltinAssetNames::Mesh(EPrimitive::EP_Cube), true);
    const auto Loading = Assets.GetAssetAs<FTexture2DAsset>(BuiltinAssetNames::LoadingScreen, true);
    RegisterLoadingScreenAssets(Assets, Renderer, Files);
    RegisterSceneAssets(Assets, Renderer, Files);
    Check(Assets.GetAssetAs<FTexture2DAsset>(BuiltinAssetNames::LoadingScreen, true) == Loading && Assets.GetAssetAs<FStaticMeshAsset>(BuiltinAssetNames::Mesh(EPrimitive::EP_Cube), true) == Cube,
        "Repeated catalog load must reuse assets");
    Check(Cube->GetIndexCount() == 36 && Assets.GetAssetAs<FStaticMeshAsset>(BuiltinAssetNames::Mesh(EPrimitive::EP_BillboardQuad), true)->GetIndexCount() == 6,
        "Built-in mesh upload");
    for (auto Type : {EPrimitive::EP_Cube, EPrimitive::EP_Sphere, EPrimitive::EP_GizmoArrow,
        EPrimitive::EP_Circle, EPrimitive::EP_Triangle, EPrimitive::EP_BillboardQuad})
        Check(Assets.GetAssetAs<FStaticMeshAsset>(BuiltinAssetNames::Mesh(Type), true) && Assets.GetAssetAs<FStaticMeshAsset>(BuiltinAssetNames::Mesh(Type), true)->GetVertexBuffer(), "Missing primitive asset");

    auto Target = Renderer.CreateRenderTarget2D(64, 64, DXGI_FORMAT_R8G8B8A8_UNORM);
    auto Depth = Renderer.CreateDepthStencil(64, 64);
    Renderer.BindRenderTarget(Target, Depth);
    Renderer.SetViewport(0, 0, 64, 64);
    auto* Context = Renderer.GetDeviceContext();
    const auto Identity = FMatrix::Identity;
    FRenderView View;
    View.ViewportSize = FVector2(64, 64);
    View.Projection2D = Renderer.GetProjection2D();
    TArray<FRenderFullscreenInfo> FullscreenInfos{{Assets.GetAssetAs<FStaticMeshAsset>(BuiltinAssetNames::FullscreenMesh, true), Loading}};
    ExpectVertices(Renderer, 6, [&] { Fullscreen.Draw(FullscreenInfos); });
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
    auto GreenAsset = MakeShared<FTexture2DAsset>(
        FName("Test.Green"), Image, Renderer.CreateShaderResourceView(Image));
    FullscreenInfos[0].Texture = GreenAsset;
    Fullscreen.Draw(FullscreenInfos);
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

    // Texture tint and authored SubUV must survive the old/new shader transition.
    const uint32 AtlasPixels[] = {0xff0000ff, 0xffff0000};
    D3D11_TEXTURE2D_DESC AtlasDesc{};
    AtlasDesc.Width = 2; AtlasDesc.Height = AtlasDesc.MipLevels = AtlasDesc.ArraySize = AtlasDesc.SampleDesc.Count = 1;
    AtlasDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; AtlasDesc.Usage = D3D11_USAGE_IMMUTABLE;
    AtlasDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    auto AtlasImage = Renderer.CreateTexture2D(AtlasDesc, AtlasPixels);
    auto AtlasAsset = MakeShared<FTexture2DAsset>(
        FName("Test.SubUV"), AtlasImage, Renderer.CreateShaderResourceView(AtlasImage));
    FRenderMeshInfo SubUVInfo{};
    SubUVInfo.StaticMesh = Assets.GetAssetAs<FStaticMeshAsset>(BuiltinAssetNames::FullscreenMesh, true); SubUVInfo.Texture = AtlasAsset;
    SubUVInfo.UVScale = FVector2(0, 0); SubUVInfo.UVOffset = FVector2(.25f, .5f); SubUVInfo.WorldTransformMatrix = Identity;
    SubUVInfo.Color = FLinearColor(.25f, 1, 1, 1);
    TArray<FRenderMeshInfo> SubUVInfos{SubUVInfo};
    Renderer.BindRenderTarget(Target, Depth);
    MeshPass.Draw(SubUVInfos, View);
    auto Texel = ReadCenter(Renderer, Target);
    Check(std::abs(int(Texel[0]) - 64) <= 1 && Texel[1] == 0 && Texel[2] == 0, "Texture tint/SubUV red sample");
    Renderer.BindRenderTarget(Target, Depth);
    SubUVInfos[0].UVOffset.x = .75f;
    MeshPass.Draw(SubUVInfos, View);
    Texel = ReadCenter(Renderer, Target);
    Check(Texel[0] == 0 && Texel[1] == 0 && Texel[2] == 255, "SubUV blue sample");
    FRenderMeshInfo MeshInfo{};
    MeshInfo.StaticMesh = Assets.GetAssetAs<FStaticMeshAsset>(BuiltinAssetNames::Mesh(EPrimitive::EP_Cube), true);
    MeshInfo.Texture = Assets.GetAssetAs<FTexture2DAsset>(BuiltinAssetNames::Texture(EPrimitive::EP_Cube), true);
    MeshInfo.WorldTransformMatrix = Identity;
    MeshInfo.Color = FLinearColor(1, 1, 1, 1);
    TArray<FRenderMeshInfo> MeshInfos{MeshInfo};
    ExpectVertices(Renderer, 36, [&] { MeshPass.Draw(MeshInfos, View); });
    MeshInfo.StaticMesh = Cube;
    MeshInfo.Texture.reset();
    TArray<FRenderMeshInfo> Instances{MeshInfo, MeshInfo};
    ExpectVertices(Renderer, 72, [&] { Instanced.Draw(Instances, View); });
    Instances.Reset();
    for (int I = 0; I < 1025; ++I) Instances.Add(MeshInfo);
    ExpectVertices(Renderer, 36 * 1025, [&] { Instanced.Draw(Instances, View); });
    MeshInfo.StaticMesh = Assets.GetAssetAs<FStaticMeshAsset>(BuiltinAssetNames::Mesh(EPrimitive::EP_GizmoArrow), true);
    MeshInfos = {MeshInfo};
    ExpectVertices(Renderer, 180, [&] { GizmoPass.Draw(MeshInfos, View); });
    MeshInfos = {Instances[0]};
    ExpectVertices(Renderer, 72, [&] { MarkPass.Draw(MeshInfos, View); OutlinePass.Draw(MeshInfos, View); });
    const auto Font = Assets.GetAssetAs<FFontAtlasAsset>(BuiltinAssetNames::DefaultFont, true);
    auto DrawText = [&](const FTextMesh& Mesh, TSharedPtr<FFontAtlasAsset> Atlas)
    {
        FRenderTextInfo Info{};
        Info.Textmesh = &Mesh; Info.FontAtlas = Atlas;
        Info.Location = FVector(0, 0, .5f);
        Info.Color = FLinearColor(1, 1, 1, 1);
        TArray<FRenderTextInfo> Infos{Info};
        TextPass.Draw(Infos, View);
    };
    FTextMesh Text;
    Text.SetUnicodeText("ASCII + 한글", Font->GetFontResource());
    ExpectVertices(Renderer, Text.Indices.Num(), [&] { DrawText(Text, Font); });
    Bound.Reset();
    Context->PSGetShaderResources(0, 1, &Bound);
    Check(Bound == Font->GetSRV(), "MSDF asset binding");
    FAssetManager Manager;
    Manager.RegisterAsset("Test.Bitmap", MakeShared<FFontAtlasAssetLoader>(Renderer.Device),
        MakeShared<FFontAtlasAssetSource>(Files, "Fonts/EnglishBigFontAtlas.dds"));
    auto Bitmap = Manager.GetAssetAs<FFontAtlasAsset>("Test.Bitmap", true);
    Text.SetText("A", Bitmap->GetFontResource());
    ExpectVertices(Renderer, 6, [&] { DrawText(Text, Bitmap); });
    Bound.Reset();
    Context->PSGetShaderResources(0, 1, &Bound);
    Check(Bound == Bitmap->GetSRV(), "Bitmap must use its own atlas");
    ExpectVertices(Renderer, 0, [&] { DrawText(Text, Font); });
    Text.Indices[0] = Text.Vertices.Num();
    ExpectVertices(Renderer, 0, [&] { DrawText(Text, Bitmap); });
    Text.SetText("", Bitmap->GetFontResource());
    ExpectVertices(Renderer, 0, [&] { DrawText(Text, Bitmap); });

    // Cache ownership must not invalidate components/submissions that retain an asset.
    std::weak_ptr<FFontAtlasAsset> WeakBitmap = Bitmap;
    Manager.Clear();
    Check(!WeakBitmap.expired() && Bitmap->GetSRV(), "Retained asset after cache clear");
    Bitmap.reset();
    Check(WeakBitmap.expired(), "Unreferenced asset must be destroyed");
    FObjectFactory::SetDefaultFontAsset(Font);
    auto Actor = std::unique_ptr<AActor>(FObjectFactory::SpawnPrimitiveActor(EPrimitive::EP_Cube,
        FVector(0), FRotator(), FVector(1)));
    Actor->SetName("에셋 이름표");
    FCamera Camera(FVector(-3, 0, 0), FRotator());
    FRenderCollector Collector;
    Collector.View.Camera = Camera; Collector.AssetManager = &Assets;
    Actor->SubmitRenderInfos(Collector);
    Check(!Collector.TextInfos.IsEmpty(), "Actor name submission");
    for (const auto& Info : Collector.TextInfos)
        Check(Info.FontAtlas == Font && !Info.Textmesh->Indices.IsEmpty(), "Name component atlas");
    FObjectFactory::SetDefaultFontAsset(nullptr);
    Assets.Clear();
    for (const auto& Info : Collector.TextInfos)
        ExpectVertices(Renderer, Info.Textmesh->Indices.Num(), [&] { DrawText(*Info.Textmesh, Info.FontAtlas); });
    std::cout << "Asset catalog/cache, meshes, textures, loading screen, instancing, gizmos, particles, bitmap/MSDF text and lifetime passed.\n";
}

static void TestTypedCollector(URenderer& Renderer)
{
    auto Target = Renderer.CreateRenderTarget2D(128, 96, DXGI_FORMAT_R8G8B8A8_UNORM);
    auto Depth = Renderer.CreateDepthStencil(128, 96);
    Renderer.FrameBufferRTV = Target->RTV.Get(); Renderer.FrameBufferRTV->AddRef();
    Renderer.DepthStencilView = Depth->DSV.Get(); Renderer.DepthStencilView->AddRef();
    Renderer.SetViewport(16, 8, 96, 80);
    FRenderingPipeline Pipeline(Renderer);
    FFileManager Files("Assets");
    FAssetManager Assets;
    RegisterSceneAssets(Assets, Renderer, Files);
    FObjectFactory::SetDefaultFontAsset(Assets.GetAssetAs<FFontAtlasAsset>(BuiltinAssetNames::DefaultFont, true));
    Check(FObjectFactory::GetDefaultFontAsset() != nullptr, "Exact font type lookup");
    Check(!Assets.GetAssetAs<FTexture2DAsset>(BuiltinAssetNames::DefaultFont),
        "Exact type lookup must reject a font requested as its texture base");
    TSharedPtr<FTexture2DAsset> FontTexture = FObjectFactory::GetDefaultFontAsset();
    Check(FontTexture->GetSRV() != nullptr, "Normal font-to-texture upcast remains valid");
    FCamera Camera(FVector(-3, 0, 0), FRotator());
    Pipeline.SetShowFlags(static_cast<uint32>(EEngineShowFlags::SF_Primitives) |
        static_cast<uint32>(EEngineShowFlags::SF_BillboardText));
    auto Cube = std::unique_ptr<AActor>(FObjectFactory::SpawnPrimitiveActor(EPrimitive::EP_Cube,
        FVector(0), FRotator(), FVector(1)));
    auto Sphere = std::unique_ptr<AActor>(FObjectFactory::SpawnPrimitiveActor(EPrimitive::EP_Sphere,
        FVector(0), FRotator(), FVector(1)));
    auto* SphereComponent = Sphere->GetComponentByType<USphereComponent>();
    SphereComponent->SetUseTexture(true); SphereComponent->SetSpin(true); SphereComponent->SetSpinSpeed(90);
    Sphere->Update(.5f);
    auto Particle = std::unique_ptr<AActor>(FObjectFactory::SpawnParticleActor(FVector(0), FRotator(), FVector(1, 2, .5f)));
    auto* ParticleComponent = Particle->GetComponentByType<UParticleSubUVComponent>();
    ParticleComponent->Initialize(FVector(0), FRotator(), FVector(1, 2, .5f), 2, 2, true, 1, 1);
    Particle->Update(.5f);

    auto Collector = Pipeline.BeginFrame(Camera, Assets);
    Cube->SubmitRenderInfos(Collector);
    Sphere->SubmitRenderInfos(Collector);
    Particle->SubmitRenderInfos(Collector);
    Check(Collector.InstancedMeshInfos.Num() == 1 && Collector.MeshInfos.Num() == 1 &&
        Collector.QuadInfos.Num() == 1 && !Collector.TextInfos.IsEmpty(), "Component selected wrong collector array");
    Check(std::abs(Collector.MeshInfos[0].UVOffset.x + .125f) < .0001f, "Sphere UV payload");
    const auto& Quad = Collector.QuadInfos[0];
    Check(std::abs(Quad.FrameBlend - .5f) < .0001f && Quad.SubUV.x == 0 && Quad.SubUV.z == .5f &&
        Quad.NextSubUV.x == .5f && Quad.NextSubUV.y == 0, "Particle must author UV and blend directly");
    Check(!Quad.EnableDepthWrite && Quad.EnableDepthTest && Quad.AddressMode == D3D11_TEXTURE_ADDRESS_CLAMP,
        "Particle render state payload");
    TArray<FPickInfo> Picks;
    Cube->SubmitPickInfos(Picks, Camera); Sphere->SubmitPickInfos(Picks, Camera); Particle->SubmitPickInfos(Picks, Camera);
    Check(Picks.Num() == 3, "Names must not enter picking");
    UINT64 Vertices = 36 + Collector.MeshInfos[0].StaticMesh->GetIndexCount() + 6;
    for (const auto& Text : Collector.TextInfos) Vertices += Text.Textmesh->Indices.Num();
    // A malformed item between valid text submissions must not abort the batch.
    Collector.TextInfos.Insert({}, 1);
    ExpectVertices(Renderer, Vertices, [&] { Pipeline.Render(Collector); });

    const auto CubeMesh = Collector.InstancedMeshInfos[0].StaticMesh;
    auto* CubeComponent = Cube->GetComponentByType<UPrimitiveComponent>();
    CubeComponent->SetUseTexture(true);
    Collector = Pipeline.BeginFrame(Camera, Assets); Cube->SubmitRenderInfos(Collector);
    Check(Collector.MeshInfos.Num() == 1 && Collector.MeshInfos[0].StaticMesh == CubeMesh &&
        Collector.MeshInfos[0].Texture, "Texture enable must reuse the same mesh asset");
    CubeComponent->SetUseTexture(false);

    Pipeline.SetShowFlags(static_cast<uint32>(EEngineShowFlags::SF_Primitives));
    Collector = Pipeline.BeginFrame(Camera, Assets, Cube.get()); Cube->SubmitRenderInfos(Collector);
    Check(Collector.SelectionInfos.Num() == 1, "Selected component must submit its stencil mesh");
    ExpectVertices(Renderer, 108, [&] { Pipeline.Render(Collector); });
    Pipeline.SetShowFlags(static_cast<uint32>(EEngineShowFlags::SF_BoundingBox));
    for (const auto* Selected : {Cube.get(), Particle.get(), static_cast<AActor*>(nullptr)})
    {
        Collector = Pipeline.BeginFrame(Camera, Assets, Selected);
        Cube->SubmitRenderInfos(Collector);
        Sphere->SubmitRenderInfos(Collector);
        Particle->SubmitRenderInfos(Collector);
        Check(Collector.SelectionInfos.Num() == (Selected ? 1 : 0) &&
            Collector.LineInfos.Num() == (Selected ? 12 : 0),
            "Only the selected primitive may submit selection and bounding-box information");
    }
    Pipeline.SetShowFlags(0);
    Collector = Pipeline.BeginFrame(Camera, Assets, Cube.get()); Cube->SubmitRenderInfos(Collector);
    Check(Collector.SelectionInfos.Num() == 1 && Collector.LineInfos.IsEmpty(),
        "Bounding-box visibility must not disable the selection outline");
    Pipeline.SetShowFlags(0);
    Collector = Pipeline.BeginFrame(Camera, Assets); Cube->SubmitRenderInfos(Collector);
    Picks.Reset(); Cube->SubmitPickInfos(Picks, Camera);
    Check(Collector.InstancedMeshInfos.IsEmpty() && Collector.TextInfos.IsEmpty() && Picks.Num() == 1,
        "Show flags must not disable picking");
    Cube->SetLocation(FVector(10000));
    Pipeline.SetShowFlags(~0u);
    Collector = Pipeline.BeginFrame(Camera, Assets); Cube->SubmitRenderInfos(Collector);
    Check(Collector.InstancedMeshInfos.IsEmpty() && Collector.LineInfos.IsEmpty(), "Component culling");

    Camera = FCamera(FVector(0, -3, 0), FRotator(0, 90, 0));
    Pipeline.SetShowFlags(0);
    Collector = Pipeline.BeginFrame(Camera, Assets); Particle->SubmitRenderInfos(Collector);
    Picks.Reset(); Particle->SubmitPickInfos(Picks, Camera);
    Check(Picks.Num() == 1 && Picks[0].WorldTransformMatrix.Equals(Collector.QuadInfos[0].Model),
        "Picking and drawing must share the billboard transform");
    const auto& Billboard = Collector.QuadInfos[0].Model;
    Check(std::abs(Billboard.GetUnitAxis(EAxis::Y).Length() - 2) < .0001f &&
        std::abs(Billboard.GetUnitAxis(EAxis::Z).Length() - .5f) < .0001f, "Billboard scale");
    ExpectVertices(Renderer, 6, [&] { Pipeline.Render(Collector); });
    Collector.Clear();
    Check(Collector.SelectionInfos.IsEmpty() && Collector.WorldAxisInfos.IsEmpty() && Collector.WorldGridInfos.IsEmpty() && Collector.QuadInfos.IsEmpty() && Collector.MeshInfos.IsEmpty(), "Collector reset");
    Pipeline.SetGridWidth(100);
    Pipeline.SetShowFlags(static_cast<uint32>(EEngineShowFlags::SF_WorldAxis) |
        static_cast<uint32>(EEngineShowFlags::SF_Grid));
    Collector = Pipeline.BeginFrame(Camera, Assets);
    Collector.LineInfos.Add({FVector4(1, 0, 0, 1), FVector(0), 2, FVector(0, 1, 0), 0});
    Collector.AddBounds(FBoundingBox(FVector(-.5f), FVector(.5f)));
    // Thirteen ordinary lines, plus one axis quad and one grid quad (WEEK3 path).
    ExpectVertices(Renderer, 15 * 6, [&] { Pipeline.Render(Collector); });
    Check(Collector.LineInfos.Num() == 13 && Collector.LineInfos[0].Thickness == 2,
        "World axes/grid must not add ordinary lines");
    Pipeline.SetShowFlags(static_cast<uint32>(EEngineShowFlags::SF_WorldAxis));
    Collector = Pipeline.BeginFrame(Camera, Assets);
    ExpectVertices(Renderer, 18, [&] { Pipeline.Render(Collector); });
    Pipeline.SetShowFlags(static_cast<uint32>(EEngineShowFlags::SF_Grid));
    Collector = Pipeline.BeginFrame(Camera, Assets);
    ExpectVertices(Renderer, 6, [&] { Pipeline.Render(Collector); });
    Pipeline.SetShowFlags(0);
    Collector = Pipeline.BeginFrame(Camera, Assets);
    ExpectVertices(Renderer, 0, [&] { Pipeline.Render(Collector); });
    FObjectFactory::SetDefaultFontAsset(nullptr);
    std::cout << "Typed component submission, UV/blend, culling, show flags, selection and billboard picking passed.\n";
}

int main(int argc, char** argv)
{
    try
    {
        const auto ScaledRotation = FMatrix::Scale(FVector(2, 3, 4)) * FMatrix::Rotate(FRotator(23, 41, 17));
        const auto Scale = ScaledRotation.GetScale();
        Check(std::abs(Scale.x - 2) < .0001f && std::abs(Scale.y - 3) < .0001f &&
            std::abs(Scale.z - 4) < .0001f, "Rotated nonuniform matrix scale");
        CheckMesh(Cube_vertices, Cube_indices);
        CheckMesh(Sphere_vertices, Sphere_indices);
        CheckMesh(Triangle_vertices, Triangle_indices);
        CheckMesh(Circle_vertices, Circle_indices);
        CheckMesh(GizmoArrow_vertices, GizmoArrow_indices);
        CheckMesh(Quad_vertices, Quad_indices);
        CheckMesh(Fullscreen_vertices, Fullscreen_indices);
        URenderer Renderer;
        Check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_DEBUG,
            nullptr, 0, D3D11_SDK_VERSION, &Renderer.Device, nullptr, &Renderer.DeviceContext)), "Create WARP device");
        const bool Quick = argc < 2 || std::string(argv[1]) != "--full";
        if (!Quick) { TestRendering(Renderer); TestAssetRendering(Renderer); }
        TestAssetManager(Renderer);
        TestQuadRendering(Renderer);
        TestTypedCollector(Renderer);
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
        std::cout << "Renderer smoke tests and DX11 validation passed.\n";
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << Error.what() << '\n';
        return 1;
    }
}
