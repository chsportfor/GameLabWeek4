#pragma once

#include "Rendering/SubUVMesh.h"
#include "Rendering/RenderInfo.h"

#include "PrimitiveComponent.h"

class USphereComponent : public UPrimitiveComponent
{
	DECLARE_OBJECT(USphereComponent, UPrimitiveComponent)
	DECLARE_SERIALIZATION()

public:
	USphereComponent();
	virtual ~USphereComponent();

	virtual void Update(float deltaTime, TArray<FRenderInfo>* outRenderInfos) override;

	void Initialize();
	void Initialize(FVector location, FRotator rotation, FVector scale3D,
		bool bSpin = false, float spinSpeed = 90.0f);

	bool GetSpin() const { return mbSpin; }
	void SetSpin(bool bSpin) { mbSpin = bSpin; }

	float GetSpinSpeed() const { return mSpinSpeed; }
	void SetSpinSpeed(float spinSpeed) { mSpinSpeed = spinSpeed; }

	static std::span<const FPropertyInfo> GetDeclaredProperties();

private:
	FSubUVMesh mSubUVMesh;

	bool mbSpin = false;
	float mSpinSpeed = 90.0f; // degrees per second

	/* Internal State */
	float mElapsedDegrees = 0.0f; // Total degrees rotated

	virtual FRenderInfo makeRenderInfo() const override;
};
