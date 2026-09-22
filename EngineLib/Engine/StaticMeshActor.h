#pragma once

#include "Engine/Actor.h"

class UStaticMeshComponent;

class AStaticMeshActor : public AActor
{
    DECLARE_OBJECT(AStaticMeshActor, AActor)
public:
    void Initialize();
    void Initialize(const FName& MeshAssetName, const FName& ActorName,
        FVector Location = FVector(0), FRotator Rotation = FRotator(), FVector Scale = FVector(1));
    UStaticMeshComponent* GetStaticMeshComponent() const;
};
