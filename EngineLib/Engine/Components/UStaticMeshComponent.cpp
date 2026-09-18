#include "UStaticMeshComponent.h"

IMPLEMENT_CLASS(UStaticMeshComponent, UMeshComponent)

void Serialize(bool bIsLoading, json::JSON Handle);
