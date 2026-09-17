#include "ParticleSubUVComponent.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UParticleSubUVComponent, UBillboardComponent);
IMPLEMENT_SERIALIZATION(UParticleSubUVComponent, UBillboardComponent,
	{ mSubUVMesh.UpdateMesh(mNumRows, mNumCols, 0); })

	void UParticleSubUVComponent::Initialize(FVector location, FRotator rotation, FVector scale3D,
		uint32 numRows, uint32 numCols,
		bool bLooping, float playRate, float frameDuration)
{
	mNumRows = numRows;
	mNumCols = numCols;
	mbLooping = bLooping;
	mPlayRate = playRate;
	mFrameDuration = frameDuration;

	mElapsedFrameRatio = 0.0f;
	mCurrentFrameIndex = 0;
	mNextFrameIndex = 1;
	mbIsFinished = false;

	// Initialize the sub UV mesh
	mSubUVMesh.UpdateMesh(mNumRows, mNumCols, 0);

	// Call the base class Initialize
	UBillboardComponent::Initialize(location, rotation, scale3D);

	mColor = FLinearColor(1.f, 1.f, 1.f, 1.f); // Set default color to white
	mBlendStateType = EBlendStateType::BST_AlphaBlend; // Set default blend state to alpha blend
}

void UParticleSubUVComponent::Update(float deltaTime, TArray<FRenderInfo>* outRenderInfos)
{
	bool restarted = false;

	if (mbIsFinished && mbLooping)
	{
		mbIsFinished = false;
		mCurrentFrameIndex = 0;
		mNextFrameIndex = 1;
		mElapsedFrameRatio = 0;

		mSubUVMesh.UpdateMesh(mNumRows, mNumCols, mCurrentFrameIndex);
		restarted = true;
	}

	if (!mbIsFinished && !restarted &&
		mNumRows > 0 && mNumCols > 0 && mFrameDuration > 0.0f)
	{
		const uint32 totalFrames = mNumRows * mNumCols;
		const uint32 previousFrameIndex = mCurrentFrameIndex;

		if (deltaTime > 0.0f && mPlayRate > 0.0f)
		{
			mElapsedFrameRatio += (deltaTime * mPlayRate) / mFrameDuration;
		}

		while (mElapsedFrameRatio >= 1.0f)
		{
			mElapsedFrameRatio -= 1.0f;

			if (mCurrentFrameIndex + 1 < totalFrames)
			{
				++mCurrentFrameIndex;
			}
			else if (mbLooping)
			{
				mCurrentFrameIndex = 0;
			}
			else
			{
				mbIsFinished = true;
				mElapsedFrameRatio = 0.0f;

				break;
			}
		}
		mNextFrameIndex = mbLooping
			? (mCurrentFrameIndex + 1) % totalFrames
			: FMath::Min(mCurrentFrameIndex + 1, totalFrames - 1);

		if (mCurrentFrameIndex != previousFrameIndex)
		{
			mSubUVMesh.UpdateMesh(mNumRows, mNumCols, mCurrentFrameIndex);
		}
	}

	UBillboardComponent::Update(deltaTime, outRenderInfos);
}

FRenderInfo UParticleSubUVComponent::makeRenderInfo() const
{
	FRenderInfo renderInfo = UBillboardComponent::makeRenderInfo();
	renderInfo.SubUVMesh = &mSubUVMesh;

	renderInfo.eRenderFlags =
		ERenderFlags::RF_Raycastable |
		ERenderFlags::RF_Billboard |
		ERenderFlags::RF_Particle;

	renderInfo.Color = mbIsFinished
		? FLinearColor(1.f, 1.f, 1.f, 0.0f) // Fully transparent if finished
		: mColor; // Use the component's color if not finished

	renderInfo.BlendStateType = static_cast<EBlendStateType>(mBlendStateType);

	renderInfo.numRows = mNumRows;
	renderInfo.numCols = mNumCols;
	renderInfo.currentFrame = mCurrentFrameIndex;
	renderInfo.nextFrame = mNextFrameIndex;
	renderInfo.frameRatio = mElapsedFrameRatio;
	return renderInfo;
}

std::span<const FPropertyInfo> UParticleSubUVComponent::GetDeclaredProperties()
{
	static const FPropertyInfo Properties[] =
	{
		REFLECT_PROPERTY(
			UParticleSubUVComponent,
			mNumRows),
		REFLECT_PROPERTY(
			UParticleSubUVComponent,
			mNumCols),
		REFLECT_PROPERTY(
			UParticleSubUVComponent,
			mbLooping),
		REFLECT_PROPERTY(
			UParticleSubUVComponent,
			mPlayRate),
		REFLECT_PROPERTY(
			UParticleSubUVComponent,
			mFrameDuration),
		REFLECT_PROPERTY(
			UParticleSubUVComponent,
			mBlendStateType)
	};

	return Properties;
}
