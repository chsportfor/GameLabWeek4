#pragma once

#include "Core/Object/Object.h"

struct FRenderCollector;
struct FPickInfo;
class FCamera;

class UActorComponent : public UObject
{
	DECLARE_OBJECT(UActorComponent, UObject)
public:
	UActorComponent();
	virtual ~UActorComponent();

	void SetOwner(AActor* owner);
	void ClearOwner();
	AActor* GetOwner() const;

	virtual void Update(float deltaTime);
	virtual void SubmitRenderInfos(FRenderCollector& Collector) const;
	virtual void SubmitPickInfos(TArray<FPickInfo>& Infos, const FCamera& Camera) const;

protected:
	AActor* mOwner;
};

