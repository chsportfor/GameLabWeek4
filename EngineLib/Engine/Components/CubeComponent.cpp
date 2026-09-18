
#include "CubeComponent.h"

IMPLEMENT_CLASS(UCubeComponent, UPrimitiveComponent);

UCubeComponent::UCubeComponent()
{
}



void UCubeComponent::Initialize()
{
	Initialize(FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f));
}

void UCubeComponent::Initialize(FVector location, FRotator rotation, FVector scale3D)
{
	UPrimitiveComponent::Initialize(EPrimitive::EP_Cube, location, rotation, scale3D);
}

UCubeComponent::~UCubeComponent()
{
}
