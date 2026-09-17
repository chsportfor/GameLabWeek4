#pragma once

#include "BillboardComponent.h"

#include "Core/enum.h"
#include "Rendering/SubUVMesh.h"

class UParticleSubUVComponent : public UBillboardComponent
{
	DECLARE_OBJECT(UParticleSubUVComponent, UBillboardComponent)
	DECLARE_SERIALIZATION()

public:
	UParticleSubUVComponent() = default;

	void Initialize(FVector location, FRotator rotation, FVector scale3D,
		uint32 numRows, uint32 numCols,
		bool bLooping = true, float playRate = 1.0f, float frameDuration = 1.0f);

	virtual void Update(float deltaTime, TArray<FRenderInfo>* outRenderInfos) override;

	static std::span<const FPropertyInfo> GetDeclaredProperties();

	bool IsLooping() const { return mbLooping; }
	void SetLooping(bool bLooping) { mbLooping = bLooping; }

	float GetPlayRate() const { return mPlayRate; }
	void SetPlayRate(float playRate) { mPlayRate = playRate; }

	EBlendStateType GetBlendStateType() const { return static_cast<EBlendStateType>(mBlendStateType); }
	void SetBlendStateType(EBlendStateType blendStateType) { mBlendStateType = blendStateType; }

private:
	FSubUVMesh mSubUVMesh;

	/* Particle texture rows and columns */
	uint32 mNumRows = 1;
	uint32 mNumCols = 1;

	bool mbLooping = true;
	float mPlayRate = 1.0f; // Play speed multiplier
	float mFrameDuration = 1.0f; // Duration of each frame in seconds
	uint32 mBlendStateType = EBlendStateType::BST_Additive;

	/* Internal State */
	//float mElapsedTime = 0.0f; // Time elapsed since the last frame change
	float mElapsedFrameRatio = 0.0f; // Ratio of elapsed time to frame duration (0.0 to 1.0)
	uint32 mCurrentFrameIndex = 0;
	uint32 mNextFrameIndex = 0;
	bool mbIsFinished = false;

	virtual FRenderInfo makeRenderInfo() const override;
};
