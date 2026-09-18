#pragma once

#include <functional>

#include "Rendering/TextMesh.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"

#include "BillboardComponent.h"
#include "Core/Object/ObjectFactory.h"

class UNameComponent : public UBillboardComponent
{
	DECLARE_OBJECT(UNameComponent, UBillboardComponent)
	DECLARE_SERIALIZATION()

public:
	UNameComponent() = default;

	virtual ~UNameComponent();

	void Initialize(const FString& nameText, FVector worldPositionOffset, TSharedPtr<UFontAtlasAsset> FontAsset);
	void SetNameText(const FString& nameText);


	virtual bool AttachTo(USceneComponent& parent) override;

	static std::span<const FPropertyInfo> GetDeclaredProperties();

protected:
	// NameComponent always located over the actor's world position,
	// so we reuse mRelativeLocation as a world position offset from the actor's world position.
	// FVector mRelativeLocation

	FString mNameText;

	FTextMesh mTextMesh;
	TSharedPtr<UFontAtlasAsset> mFontAsset;
	void RebuildTextMesh();

	virtual void updateComponentToWorld(const FMatrix& parentTransform) override;
	//virtual void updateComponentToWorld() override;

	virtual FRenderInfo makeRenderInfo() const override;
};
