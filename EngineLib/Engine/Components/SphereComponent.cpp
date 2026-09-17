#include "SphereComponent.h"

#include "Rendering/RenderInfo.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(USphereComponent, UPrimitiveComponent);

IMPLEMENT_SERIALIZATION(USphereComponent, UPrimitiveComponent, {})

USphereComponent::USphereComponent()
{
}

/*
void USphereComponent::Initialize(GraphicsManager* graphicsManager)
{
	Initialize(graphicsManager, FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f));
}

void USphereComponent::Initialize(GraphicsManager* graphicsManager, FVector location, FRotator rotation, FVector scale3D)
{
	UPrimitiveComponent::Initialize(graphicsManager, EPrimitive::EP_Sphere, location, rotation, scale3D);
}
*/

USphereComponent::~USphereComponent()
{
}

void USphereComponent::Update(float deltaTime, TArray<FRenderInfo>* outRenderInfos)
{
	if (mbSpin)
	{
		mElapsedDegrees += mSpinSpeed * deltaTime;
		if (mElapsedDegrees >= 360.f)
		{
			mElapsedDegrees = std::fmod(mElapsedDegrees, 360.f); // Wrap around to keep the angle in [0, 360)
		}

		// Update sub uv instead of the rotation of the sphere component
		mSubUVMesh.UVOffset.x = -mElapsedDegrees / 360.f; // Assuming the texture is a horizontal strip of frames
	}

	UPrimitiveComponent::Update(deltaTime, outRenderInfos);
}

void USphereComponent::Initialize()
{
	Initialize(FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f));
}

void USphereComponent::Initialize(FVector location, FRotator rotation, FVector scale3D,
	bool bSpin, float spinSpeed)
{
	UPrimitiveComponent::Initialize(EPrimitive::EP_Sphere, location, rotation, scale3D);
	mbSpin = bSpin;
	mSpinSpeed = spinSpeed;
}

FRenderInfo USphereComponent::makeRenderInfo() const
{
	FRenderInfo renderInfo = UPrimitiveComponent::makeRenderInfo();
	renderInfo.SubUVMesh = mbUseTexture ? &mSubUVMesh : nullptr;

	return renderInfo;
}

std::span<const FPropertyInfo>
USphereComponent::GetDeclaredProperties()
{
	static const FPropertyInfo Properties[] =
	{
		REFLECT_PROPERTY(
			USphereComponent,
			mbSpin),
		REFLECT_PROPERTY(
			USphereComponent,
			mSpinSpeed)
	};

	return Properties;
}
