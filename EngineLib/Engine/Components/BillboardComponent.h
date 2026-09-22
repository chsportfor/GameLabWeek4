#pragma once

#include "PrimitiveComponent.h"
#include "Core/AssetSystem/Asset/Texture2DAsset.h"

// Billboard rendered texture 2d quad component, always facing the **Viewport camera**.
class UBillboardComponent : public UPrimitiveComponent
{
	DECLARE_OBJECT(UBillboardComponent, UPrimitiveComponent)
	DECLARE_SERIALIZATION()
public:
	UBillboardComponent();
	bool RayCastComponent(const FPickingRay& Ray, const FCamera& Camera, float& OutHitT) const override;
	void SetTexture(UTexture2D* Texture) { mTexture = Texture; }
	const FLinearColor& GetColor() const { return mColor; }
	void SetColor(const FLinearColor& Color) { mColor = Color; }
	static std::span<const FPropertyInfo> GetDeclaredProperties();

	void Initialize(FVector location, FRotator rotation, FVector scale3D);

	virtual ~UBillboardComponent() = default;
	void SubmitRenderInfos(FRenderCollector& Collector) const override;

protected:
	FRenderMeshInfo MakeMeshInfo(const FRenderCollector& Collector) const override;
	UTexture2D* mTexture = nullptr;
	FLinearColor mColor{1.f, 1.f, 1.f, 1.f};
	FMatrix GetRenderTransform(const FCamera& Camera) const override;
	FRenderQuadInfo MakeQuadInfo(const FRenderCollector& Collector) const;
};
