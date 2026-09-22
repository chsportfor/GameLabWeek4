#include "StaticMeshActor.h"

#include "Core/AssetSystem/AssetManager.h"
#include "Engine/Components/UStaticMeshComponent.h"
#include "Engine/Components/NameComponent.h"
#include "Rendering/BuiltinAssetNames.h"

IMPLEMENT_CLASS(AStaticMeshActor, AActor);

void AStaticMeshActor::Initialize()
{
    Initialize(BuiltinAssetNames::CubeMesh, "StaticMesh");
}

void AStaticMeshActor::Initialize(const FName& MeshAssetName, const FName& ActorName,
    FVector Location, FRotator Rotation, FVector Scale)
{
    AActor::Initialize();
    SetName(ActorName);

    auto* assets = FObjectFactory::GetDefaultAssetManager();
    auto* mesh = assets ? assets->GetAssetAs<UStaticMeshAsset>(MeshAssetName, true) : nullptr;
    if (!mesh) throw std::runtime_error("Static mesh asset is unavailable.");

    auto* component = FObjectFactory::ConstructObject<UStaticMeshComponent>(Location, Rotation, Scale);
    AddRootSceneComponent(component);
    component->SetStaticMesh(mesh);

    auto& label = CreateAndAddComponent<UNameComponent>(
        GetName().ToString(), FVector(0, 0, 1), FObjectFactory::GetDefaultFontAsset());
    label.AttachTo(*component);
}

UStaticMeshComponent* AStaticMeshActor::GetStaticMeshComponent() const
{
    return GetComponentByType<UStaticMeshComponent>();
}
