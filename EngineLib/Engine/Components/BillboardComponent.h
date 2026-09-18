#pragma once

#include "PrimitiveComponent.h"

// Billboard rendered texture 2d quad component, always facing the **Viewport camera**.
class UBillboardComponent : public UPrimitiveComponent
{
	DECLARE_OBJECT(UBillboardComponent, UPrimitiveComponent)
public:
	UBillboardComponent();

	void Initialize(FVector location, FRotator rotation, FVector scale3D);

	virtual ~UBillboardComponent() = default;
	void SubmitRenderInfos(FRenderCollector& Collector) const override;

protected:
	FMatrix GetRenderTransform(const FCamera& Camera) const override;
	FRenderQuadInfo MakeQuadInfo(const FRenderCollector& Collector) const;
};
