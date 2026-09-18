#include "NameComponent.h"
#include "Core/IO/JsonUtil.h"

#include <format>

#include "Engine/Actor.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UNameComponent, UBillboardComponent);
IMPLEMENT_SERIALIZATION(UNameComponent, UBillboardComponent,
	{
		mFontAsset = FObjectFactory::GetDefaultFontAsset();
		RebuildTextMesh();
	}
)

void UNameComponent::Initialize(const FString& nameText, FVector worldPositionOffset, TSharedPtr<UFontAtlasAsset> FontAsset)
{
	UBillboardComponent::Initialize(worldPositionOffset, FRotator(), FVector(1));

	mFontAsset = std::move(FontAsset);
	mNameText = nameText;
	mColor = FLinearColor(1.f, 1.f, 1.f, 1.f); // Set default color to white
}

void UNameComponent::updateComponentToWorld(const FMatrix& parentTransform)
{
	// NameComponent always located over the actor's world position,
	// so we reuse mRelativeLocation as a world position offset from the actor's world position.

	FVector parentTranslation = parentTransform.GetTranslation();
	FVector worldPosition = parentTranslation + mRelativeLocation;

	if (mParent)
	{
		worldPosition.z = mParent->CalcBounds(parentTransform).Max.z+0.2f;
	}
	mComponentToWorld = FTransform(worldPosition, FQuat::Identity(), mRelativeScale3D).MakeMatrix();
}

void UNameComponent::SubmitRenderInfos(FRenderCollector& Collector) const
{
    if (!Collector.HasShowFlag(EEngineShowFlags::SF_BillboardText)) return;
    const auto Model = GetRenderTransform(Collector.View.Camera);
    if (!Collector.IsVisible(mLocalBounds.ToWorld(Model))) return;
    FRenderTextInfo Info{};
    Info.Textmesh = &mTextMesh;
    Info.FontAtlas = mFontAsset;
    Info.Location = Model.GetTranslation();
    Info.Scale = Model.GetScale();
    Info.Color = mColor;
    Collector.TextInfos.Add(Info);
}

void UNameComponent::SetNameText(const FString& nameText)
{
	assert(mOwner);

	FString text = FString(std::format("Name: {}, UUID: {}", nameText, mOwner->UUID));
	mNameText = text;

	RebuildTextMesh();
}

void UNameComponent::RebuildTextMesh()
{
    if (!mFontAsset) { mTextMesh = FTextMesh{}; return; }
    if (mFontAsset->IsMSDF())
        mTextMesh.SetUnicodeText(mNameText, mFontAsset->GetFontResource(), 0.2f);
    else
        mTextMesh.SetText(mNameText, mFontAsset->GetFontResource());
}

bool UNameComponent::AttachTo(USceneComponent& parent)
{
	if (!UBillboardComponent::AttachTo(parent))
	{
		return false;
	}

	SetNameText(mOwner->GetName().ToString());
	return true;
}

UNameComponent::~UNameComponent()
{
}

std::span<const FPropertyInfo> UNameComponent::GetDeclaredProperties()
{
	static const FPropertyInfo Properties[] =
	{
		REFLECT_PROPERTY(
			UNameComponent,
			mNameText),
	};

	return Properties;
}
