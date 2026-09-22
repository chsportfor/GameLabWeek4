#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Core/AssetSystem/AssetFile/StaticMeshAssetFile.h"
#include "Core/AssetSystem/AssetFile/MaterialAssetFile.h"
#include "Core/AssetSystem/AssetFile/Texture2DAssetFile.h"
#include "Core/AssetSystem/AssetFile/FontAtlasAssetFile.h"
#include "Core/IO/FileManager.h"
#include "Core/Object/ObjectArray.h"
#include "Core/Object/WeakObjectPtr.h"
#include "Engine/Serialization/PropertyJson.h"
#include "Engine/Assets/Importers/BuiltinAssetImporter.h"
#include "Engine/Assets/Importers/StaticMeshImporter.h"
#include "Rendering/BuiltinAssetNames.h"
#include "Rendering/Renderer.h"
#include "Engine/Assets/ObjImporter.h"
#include "Engine/Components/UStaticMeshComponent.h"
#include "Engine/SceneManager.h"
#include "Engine/World.h"
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
static void Check(bool Value, const char* Message) { if (!Value) throw std::runtime_error(Message); }
static void Write(const fs::path& Path, const TArray<uint8>& Bytes)
{
    fs::create_directories(Path.parent_path());
    std::ofstream Out(Path, std::ios::binary);
    Out.write(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num());
    Check(bool(Out), "Write fixture");
}
static FTexture2D_uasset ReadTexture(const fs::path& Path)
{
    auto Bytes = FFileManager::Get().ReadFileToString(Path);
    return AssetFile::DeserializeTexture2D({reinterpret_cast<const uint8*>(Bytes.CStr()), size_t(Bytes.Len())});
}
static FStaticMesh_uasset ReadMesh(const fs::path& Path)
{
    auto Bytes = FFileManager::Get().ReadFileToString(Path);
    return AssetFile::DeserializeStaticMesh({reinterpret_cast<const uint8*>(Bytes.CStr()), size_t(Bytes.Len())});
}

int main(int Argc, char** Argv)
{
    try
    {
        Microsoft::WRL::ComPtr<ID3D11Device> Device;
        Check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &Device, nullptr, nullptr)), "Create WARP device");
        URenderer Renderer; Renderer.Device = Device.Get();
        auto& Files = FFileManager::Get();
        Files.Initialize("EngineLib/Assets");
        if (Argc == 4 && std::string_view(Argv[1]) == "--import-obj")
        {
            Check(FStaticMeshImporter::ImportUStaticMesh(Renderer, fs::u8path(Argv[2]), fs::u8path(Argv[3])), "Import OBJ file");
            UAssetManager Assets; Assets.Initialize(Renderer);
            Check(Assets.ScanAssets(), "Scan imported files");
            auto* Mesh = Assets.GetAssetAs<UStaticMeshAsset>(UAssetManager::MakeFileAssetName(fs::u8path(Argv[3]), Files), true);
            Check(Mesh && Mesh->GetCpuGeometry() && Mesh->GetVertexBuffer(), "Load imported mesh and dependencies");
            std::cout << "PASS: imported and loaded " << Argv[3] << " (" << Mesh->GetVertexCount() << " vertices)\n";
            return 0;
        }
        if (Argc == 2 && std::string_view(Argv[1]) == "--prepare-builtins")
        {
            Check(FBuiltinAssetImporter::ImportMissingBuiltins(Renderer), "Prepare builtin files");
            UAssetManager Assets; Assets.Initialize(Renderer);
            Check(Assets.ScanAssets(), "Scan builtin headers");
            for (const auto* Name : {BuiltinAssetNames::CubeMesh, BuiltinAssetNames::SphereMesh,
                BuiltinAssetNames::TriangleMesh, BuiltinAssetNames::QuadMesh, BuiltinAssetNames::FullscreenMesh,
                BuiltinAssetNames::GizmoArrowMesh, BuiltinAssetNames::CircleMesh})
                Check(Assets.GetAssetAs<UStaticMeshAsset>(Name, true) != nullptr, "Load builtin mesh");
            for (const auto* Name : {BuiltinAssetNames::CubeTexture, BuiltinAssetNames::EarthTexture,
                BuiltinAssetNames::ExplosionTexture, BuiltinAssetNames::LoadingScreen, BuiltinAssetNames::DefaultTexture})
                Check(Assets.GetAssetAs<UTexture2D>(Name, true) != nullptr, "Load builtin texture");
            auto* Font = Assets.GetAssetAs<UFontAtlasAsset>(BuiltinAssetNames::DefaultFont, true);
            Check(Font && Font->FindUnicodeCharacter(44032), "Load builtin Korean atlas");
            std::cout << "PASS: prepared and loaded builtin mesh/material/texture/font files\n";
            return 0;
        }
        const auto SourceRoot = Files.GetFileDirectoryPath();
        auto Texture = ReadTexture(SourceRoot / BuiltinAssetNames::DefaultTexture);
        auto MeshFile = ReadMesh(SourceRoot / BuiltinAssetNames::CubeMesh);
        const auto Parent = fs::absolute("Tools/bin/AssetSystemCheck");
        const auto Root = Parent / ("fixtures-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        fs::create_directories(Root);
        Files.Initialize(Root.string());
        Write(Root / BuiltinAssetNames::DefaultTexture, AssetFile::Serialize(Texture));
        FMaterial_uasset DefaultMaterial; DefaultMaterial.bStandalone = true;
        Write(Root / BuiltinAssetNames::DefaultMaterial, AssetFile::Serialize(DefaultMaterial));
        Write(Root / BuiltinAssetNames::CubeMesh, AssetFile::Serialize(MeshFile));
        FFontAtlas_uasset FontFile; FontFile.bStandalone = true; FontFile.Data = Texture.Data;
        Write(Root / BuiltinAssetNames::DefaultFont, AssetFile::Serialize(FontFile));

        // Two sections share a texture through separate material files. Runtime load needs no sources.
        fs::create_directories(Root / "Source");
        Write(Root / "Source/White.dds", Texture.Data);
        { std::ofstream Out(Root / "Source/Test.mtl"); Out << "newmtl B\nKd 1 0 0\nmap_Kd White.dds\nnewmtl C\nKd 0 1 0\nmap_Kd White.dds\n"; }
        { std::ofstream Out(Root / "Source/Test.obj"); Out << "mtllib Test.mtl\nv 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nusemtl B\nf 1 2 3\nusemtl C\nf 1 3 4\n"; }
        Check(FStaticMeshImporter::ImportUStaticMesh(Renderer, "Source/Test.obj", "Imported/A.uasset"), "Import OBJ");
        Check(fs::exists(Root / "Source/Test.pmesh"), "OBJ importer writes source cache");
        Check(FStaticMeshImporter::ImportUStaticMeshFromBinary(Renderer, "Source/Test.pmesh", "Binary/Test.uasset"),
            "Explicit binary importer shares OBJ cache reader");
        FStaticMesh Cached; FString CacheError;
        Check(FObjImporter::LoadFromFile(Root / "Source/Test.obj", Files, Cached, CacheError) && Cached.Indices.Num() == 6,
            "OBJ cache reuse preserves triangles");
        fs::rename(Root / "Source", Root / "HiddenSource");
        Check(!fs::exists(Root / "cachelibrary"), "Importer must not create library");
        UAssetManager Assets; Assets.Initialize(Renderer);
        const auto BeforeScan = UObject::GetGObjectArray().Num();
        Check(Assets.ScanAssets() && UObject::GetGObjectArray().Num() == BeforeScan, "Header scan creates no objects");
        const auto RegistrySize = UStaticMeshAsset::GetRegisteredAssetNames().Num();
        Check(Assets.RegisterAsset("Imported/A.uasset") && Assets.ScanAssets() &&
            UStaticMeshAsset::GetRegisteredAssetNames().Num() == RegistrySize, "Idempotent registration");
        Check(Assets.GetReferencers("Imported/A/White.uasset").Num() == 2, "Unique reverse edges");
        Check(!Assets.GetAssetAs<UTexture2D>(BuiltinAssetNames::DefaultFont, true), "Exact type lookup");
        auto* Font = Assets.GetAssetAs<UFontAtlasAsset>(BuiltinAssetNames::DefaultFont, true);
        Check(Font && Font->GetTexture(), "Font virtual load");
        auto* Mesh = Assets.GetAssetAs<UStaticMeshAsset>("Imported/A.uasset", true);
        Check(Mesh && Mesh->GetVertexBuffer() && Mesh->GetCpuGeometry() && Mesh->GetSections().Num() == 2, "Mesh virtual load");
        Check(Mesh->GetMaterial(0)->DiffuseTexture == Mesh->GetMaterial(1)->DiffuseTexture, "Shared loaded dependency");
        Check(!Assets.DeleteAsset("Imported/A/White.uasset"), "Cannot delete referenced file");
        TWeakObjectPtr<UStaticMeshAsset> LiveMesh(Mesh);
        Check(Assets.DeleteAsset("Imported/A.uasset"), "Delete mesh with shared dependencies");
        Check(!fs::exists(Root / "Imported/A/B.uasset") && !fs::exists(Root / "Imported/A/C.uasset") &&
            !fs::exists(Root / "Imported/A/White.uasset"), "Cascade revisits shared texture after second material");
        Check(LiveMesh.Get() == Mesh && Mesh->GetMaterial(0)->DiffuseTexture->GetTexture(), "Deletion keeps live objects/resources");
        Mesh->UnloadCpuGeometry(); Check(!Mesh->LoadCpuGeometry(), "Deleted mesh cannot reload CPU geometry");
        Check(!Assets.GetAssetAs<UStaticMeshAsset>("Imported/A.uasset"), "Deleted identity leaves typed cache lookup");
        Check(Assets.ScanAssets() && !Assets.GetAsset("Imported/A.uasset"), "Refresh does not restore retired identities");
        Write(Root / "Imported/A.uasset", AssetFile::Serialize(MeshFile));
        Check(Assets.RegisterAsset("Imported/A.uasset"), "Register recreated path");
        auto* NewMesh = Assets.GetAssetAs<UStaticMeshAsset>("Imported/A.uasset", true);
        TWeakObjectPtr<UStaticMeshAsset> LiveNewMesh(NewMesh);
        Check(NewMesh && NewMesh != Mesh && LiveMesh.Get() == Mesh &&
            NewMesh->GetIndexCount() == MeshFile.Geometry.Indices.Num(), "Recreated file loads new data without destroying old object");
        Check(Assets.RegisterAsset("Imported/A.uasset") &&
            Assets.GetAssetAs<UStaticMeshAsset>("Imported/A.uasset", true) == NewMesh, "Reregistration without deletion keeps active object");

        Write(Root / "Refresh.uasset", AssetFile::Serialize(DefaultMaterial));
        Check(Assets.RegisterAsset("Refresh.uasset"), "Register refresh fixture");
        TWeakObjectPtr<UMaterial> RemovedExternally(Assets.GetAssetAs<UMaterial>("Refresh.uasset", true));
        fs::remove(Root / "Refresh.uasset");
        Check(Assets.ScanAssets() && !Assets.GetAsset("Refresh.uasset") && RemovedExternally.Get(),
            "Explicit refresh retires externally deleted files without destroying objects");

        FMaterial_uasset MissingTexture; MissingTexture.DiffuseTexturePath = FString("MissingTexture.uasset");
        Write(Root / "MissingTextureMaterial.uasset", AssetFile::Serialize(MissingTexture));
        auto MissingMaterialMesh = MeshFile;
        MissingMaterialMesh.MaterialPaths = {FString("MissingMaterial.uasset")};
        Write(Root / "MissingMaterialMesh.uasset", AssetFile::Serialize(MissingMaterialMesh));
        Check(Assets.ScanAssets(), "Register dependency fallback fixtures");
        auto* MaterialFallback = Assets.GetAssetAs<UMaterial>("MissingTextureMaterial.uasset", true);
        Check(MaterialFallback && MaterialFallback->DiffuseTexture ==
            Assets.GetAssetAs<UTexture2D>(UTexture2D::GetDefaultAssetName(), true), "Missing texture dependency fallback");
        auto* MeshFallback = Assets.GetAssetAs<UStaticMeshAsset>("MissingMaterialMesh.uasset", true);
        Check(MeshFallback && MeshFallback->GetMaterial(0) ==
            Assets.GetAssetAs<UMaterial>(UMaterial::GetDefaultAssetName(), true), "Missing material dependency fallback");
        Check(!Assets.GetAssetAs<UMaterial>("MissingMaterial.uasset") &&
            !Assets.GetAssetAs<UMaterial>(FName{}, true), "Cache-only queries and empty names remain null");
        Write(Root / "Disappeared.uasset", AssetFile::Serialize(Texture));
        Check(Assets.RegisterAsset("Disappeared.uasset"), "Register unloaded texture");
        fs::remove(Root / "Disappeared.uasset");
        Check(Assets.GetAssetAs<UTexture2D>("Disappeared.uasset", true) == MaterialFallback->DiffuseTexture,
            "Missing file with retained metadata falls back");

        // A different root asset keeps a shared non-standalone texture alive.
        Texture.bStandalone = false;
        Write(Root / "Shared.uasset", AssetFile::Serialize(Texture));
        FMaterial_uasset M; M.bStandalone = true; M.DiffuseTexturePath = FString("Shared.uasset");
        Write(Root / "First.uasset", AssetFile::Serialize(M));
        Write(Root / "Second.uasset", AssetFile::Serialize(M));
        Check(Assets.ScanAssets(), "Register shared dependency graph");
        Check(Assets.DeleteAsset("First.uasset") && fs::exists(Root / "Shared.uasset"), "Other asset retains shared texture");
        Check(Assets.DeleteAsset("Second.uasset") && !fs::exists(Root / "Shared.uasset"), "Last referencer releases dependent file");
        // Standalone material stops traversal, preserving its non-standalone texture.
        Write(Root / "Shared.uasset", AssetFile::Serialize(Texture));
        Write(Root / "Keep.uasset", AssetFile::Serialize(M));
        MeshFile.MaterialPaths = {FString("Keep.uasset")};
        Write(Root / "Root.uasset", AssetFile::Serialize(MeshFile));
        Check(Assets.ScanAssets(), "Register standalone dependency graph");
        Check(Assets.DeleteAsset("Root.uasset") && fs::exists(Root / "Keep.uasset") &&
            fs::exists(Root / "Shared.uasset"), "Standalone stops cascade");
        Check(Assets.DeleteAsset("Keep.uasset") && !fs::exists(Root / "Shared.uasset"), "Direct deletion ignores standalone");
        // Non-standalone unreferenced files are also valid explicit deletion targets.
        Write(Root / "Orphan.uasset", AssetFile::Serialize(Texture));
        Check(Assets.RegisterAsset("Orphan.uasset"), "Register orphan");
        Check(Assets.DeleteAsset("Orphan.uasset"), "Delete non-standalone root");

        FObjectFactory::SetDefaultAssetManager(&Assets);
        {
            FCamera Camera;
            FSceneManager Scene(Camera); Scene.NewScene();
            TArray<FViewportCameraData> Cameras;
            const auto ScenePath = (SourceRoot / "SceneData/22.Scene").string();
            Scene.LoadScene(ScenePath, Files, Cameras);
            Check(Scene.GetCurrentWorld()->GetActors().Num() == 1 && Cameras.Num() == 2,
                "Upstream sample scene loads current mesh assets and two cameras");
            auto* Component = Scene.GetCurrentWorld()->GetActors()[0]->GetComponentByType<UStaticMeshComponent>();
            Check(Component && Component->GetStaticMesh(), "Sample cube uses static mesh component");
            Component->SetRelativeScale3D(FVector(0.0001f));
            float HitT = 0;
            Check(Component->RayCastComponent({FVector(-1, 0, 0), FVector(1, 0, 0)}, Camera, HitT),
                "Tiny static mesh remains pickable after merge");
            fs::create_directories(Root / "SceneData");
            Scene.SaveScene("MergeCameras", Files, Cameras);
            TArray<FViewportCameraData> RestoredCameras;
            Scene.LoadScene((Root / "SceneData/MergeCameras.Scene").string(), Files, RestoredCameras);
            Check(RestoredCameras.Num() == 2 && RestoredCameras[1].ViewportIndex == Cameras[1].ViewportIndex &&
                (RestoredCameras[1].Camera.Location - Cameras[1].Camera.Location).IsNearlyZero(), "Multiple camera round trip");
            Scene.SaveScene("NoPerspective", Files, {});
            TArray<FViewportCameraData> NoCameras;
            Scene.LoadScene((Root / "SceneData/NoPerspective.Scene").string(), Files, NoCameras);
            Check(NoCameras.IsEmpty(), "Scene round trip with all viewports orthographic");
        }
        json::JSON Json; Json["Mesh"] = "Missing.uasset";
        UStaticMeshAsset* Restored = nullptr;
        TPropertyJsonSerializer<UStaticMeshAsset*>::Deserialize(Json, "Mesh", Restored);
        Check(Restored && Restored->GetName() == UStaticMeshAsset::GetDefaultAssetName(), "Missing scene reference fallback");
        TPropertyJsonSerializer<UStaticMeshAsset*>::Serialize(Json, "Mesh", Restored);
        Check(Json.at("Mesh").ToString() == UStaticMeshAsset::GetDefaultAssetName().ToString().CStr(), "Save replacement path");
        UMaterial* Override = Restored->GetMaterial(0);
        Json["Material"] = json::JSON();
        TPropertyJsonSerializer<UMaterial*>::Deserialize(Json, "Material", Override);
        Check(!Override, "Null override remains null");
        Check(LoadAssetReference("Missing.uasset", UTexture2D::GetClass(), UTexture2D::GetDefaultAssetName())->IsA<UTexture2D>(),
            "Default texture fallback");
        Check(LoadAssetReference("Missing.uasset", UMaterial::GetClass(), UMaterial::GetDefaultAssetName())->IsA<UMaterial>(),
            "Default material fallback");
        Check(LoadAssetReference("Missing.uasset", UFontAtlasAsset::GetClass(), UFontAtlasAsset::GetDefaultAssetName()) == Font,
            "Default font fallback");
        Json["Mesh"] = BuiltinAssetNames::DefaultMaterial;
        bool Rejected = false;
        try { TPropertyJsonSerializer<UStaticMeshAsset*>::Deserialize(Json, "Mesh", Restored); }
        catch (...) { Rejected = true; }
        Check(Rejected, "Wrong class must not use fallback");
        // Header-only body registers, but a load failure must not be hidden by a default.
        FFile_uasset Header; Header.AssetType = FString("UTexture2D"); Header.bStandalone = true;
        Write(Root / "Truncated.uasset", AssetFile::SerializeHeader(Header));
        Check(Assets.RegisterAsset("Truncated.uasset"), "Header-only registration");
        Json["Texture"] = "Truncated.uasset"; UTexture2D* Broken = nullptr; Rejected = false;
        try { TPropertyJsonSerializer<UTexture2D*>::Deserialize(Json, "Texture", Broken); }
        catch (...) { Rejected = true; }
        Check(Rejected, "Corrupt body must not use fallback");
        Check(Assets.DeleteAsset(BuiltinAssetNames::DefaultFont), "Delete default font file");
        Check(!Assets.GetAssetAs<UFontAtlasAsset>("Missing.uasset", true) && Font->GetTexture(),
            "Retired default stays alive but is not returned by name");
        Rejected = false;
        {
            UAssetManager Fresh; Fresh.Initialize(Renderer);
            Check(Fresh.ScanAssets(), "Fresh registry without deleted default");
            Check(!Fresh.GetAssetAs<UFontAtlasAsset>("Missing.uasset", true), "Missing default returns null without recursion");
            FObjectFactory::SetDefaultAssetManager(&Fresh);
            try { LoadAssetReference("Missing.uasset", UFontAtlasAsset::GetClass(), UFontAtlasAsset::GetDefaultAssetName()); }
            catch (...) { Rejected = true; }
            FObjectFactory::SetDefaultAssetManager(&Assets);
        }
        Check(Rejected && Font->GetTexture(), "Scene missing default fails; other manager's old instance survives");
        // Unrelated disk files are not scanned implicitly by deletion.
        { std::ofstream Out(Root / "Bad.uasset"); Out << "broken"; }
        Check(Assets.DeleteAsset("Imported/A.uasset") && !fs::exists(Root / "Imported/A.uasset"), "Deletion uses registered index without scanning");
        fs::remove(Root / "Bad.uasset");
        FObjectFactory::SetDefaultAssetManager(nullptr);
        Assets.Clear(); Check(!LiveMesh.Get() && !LiveNewMesh.Get() && !RemovedExternally.Get(),
            "Manager shutdown destroys all retired generations");
        Check(Root.parent_path() == Parent && fs::weakly_canonical(Root).parent_path() == fs::weakly_canonical(Parent), "Cleanup boundary");
        fs::remove_all(Root);
        std::cout << "PASS: header-only registry, four virtual loaders, OBJ/cache imports, tiny-mesh picking, camera round trip, deletion/lifetime, scene fallback\n";
    }
    catch (const std::exception& Error)
    {
        FObjectFactory::SetDefaultAssetManager(nullptr);
        std::cerr << "FAIL: " << Error.what() << '\n';
        return 1;
    }
}

