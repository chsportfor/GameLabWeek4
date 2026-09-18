#include "UStaticMeshComponent.h"

IMPLEMENT_CLASS(UStaticMeshComponent, UMeshComponent)
IMPLEMENT_SERIALIZATION(UStaticMeshComponent, UMeshComponent, {});

void Serialize(bool bIsLoading, json::JSON Handle);
