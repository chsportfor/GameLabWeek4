#include "ParticleSubUVComponent.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/Object/ObjectFactory.h"
#include "Rendering/BuiltinAssetNames.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UParticleSubUVComponent, UBillboardComponent);
IMPLEMENT_SERIALIZATION(UParticleSubUVComponent, UBillboardComponent,
	{})

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
	mNextFrameIndex = (mNumRows > 1 || mNumCols > 1) ? 1 : 0;
	mbIsFinished = false;


	// Call the base class Initialize
	UBillboardComponent::Initialize(location, rotation, scale3D);
	auto* Assets = FObjectFactory::GetDefaultAssetManager();
	SetTexture(Assets ? Assets->GetAssetAs<UTexture2D>(BuiltinAssetNames::ExplosionTexture, true) : nullptr);

	mColor = FLinearColor(1.f, 1.f, 1.f, 1.f); // Set default color to white
	mBlendStateType = EBlendStateType::BST_AlphaBlend; // Set default blend state to alpha blend
}

void UParticleSubUVComponent::Update(float deltaTime)
{
	bool restarted = false;

	if (mbIsFinished && mbLooping)
	{
		mbIsFinished = false;
		mCurrentFrameIndex = 0;
		mNextFrameIndex = (mNumRows > 1 || mNumCols > 1) ? 1 : 0;
		mElapsedFrameRatio = 0;

		restarted = true;
	}

	if (!mbIsFinished && !restarted &&
		mNumRows > 0 && mNumCols > 0 && mFrameDuration > 0.0f)
	{
		const uint32 totalFrames = mNumRows * mNumCols;

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

	}

	UBillboardComponent::Update(deltaTime);
}

void UParticleSubUVComponent::SubmitRenderInfos(FRenderCollector& Collector) const
{
    const auto Model = GetRenderTransform(Collector.View.Camera);
    SubmitSelection(Collector, Model);
    if (!Collector.IsVisible(mLocalBounds.ToWorld(Model)) || mNumRows == 0 || mNumCols == 0) return;
    auto Info = MakeQuadInfo(Collector);
    if (!Info.Texture) return;
    const auto FrameUV = [this](uint32 Frame) -> FVector4
    {
        return {float(Frame % mNumCols) / mNumCols, float(Frame / mNumCols) / mNumRows,
            1.f / mNumCols, 1.f / mNumRows};
    };
    Info.SubUV = FrameUV(mCurrentFrameIndex);
    Info.NextSubUV = FrameUV(mNextFrameIndex);
    Info.FrameBlend = mElapsedFrameRatio;
    if (mbIsFinished) Info.Color.w = 0;
    Info.EnableDepthWrite = false;
    Info.AddressMode = D3D11_TEXTURE_ADDRESS_CLAMP;
    if (mBlendStateType == BST_AlphaBlend) Info.BlendMode = ERenderBlendMode::Transparent;
    else if (mBlendStateType == BST_Additive) Info.BlendMode = ERenderBlendMode::Additive;
    else if (mBlendStateType == BST_NoColorWrite) Info.BlendMode = ERenderBlendMode::NoColorWrite;
    Collector.QuadInfos.Add(Info);
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
