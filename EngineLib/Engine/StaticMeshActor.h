#pragma once

#include "Engine/Actor.h"

class UStaticMeshComponent;

class AStaticMeshActor : public AActor
{
    DECLARE_OBJECT(AStaticMeshActor, AActor)
public:
    void Initialize();
    UStaticMeshComponent* GetStaticMeshComponent() const;
};
