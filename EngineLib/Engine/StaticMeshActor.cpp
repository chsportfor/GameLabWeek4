#include "StaticMeshActor.h"

#include "Core/AssetSystem/AssetManager.h"
#include "Engine/Components/UStaticMeshComponent.h"
#include "Engine/Components/NameComponent.h"
#include "Rendering/BuiltinAssetNames.h"

IMPLEMENT_CLASS(AStaticMeshActor, AActor);

void AStaticMeshActor::Initialize()
{
    AActor::Initialize();
    SetName("StaticMesh");

    auto* assets = FObjectFactory::GetDefaultAssetManager();
    auto* mesh = assets ? assets->GetAssetAs<UStaticMeshAsset>(BuiltinAssetNames::Mesh(EPrimitive::EP_Cube), true) : nullptr;
    if (!mesh) throw std::runtime_error("Default static mesh asset is unavailable.");

    auto* component = FObjectFactory::ConstructObject<UStaticMeshComponent>(FVector(0), FRotator(), FVector(1));
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
