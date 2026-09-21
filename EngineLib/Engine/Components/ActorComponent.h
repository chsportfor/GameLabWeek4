#pragma once

#include "Core/Object/Object.h"
#include "Engine/Picking.h"

struct FRenderCollector;
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
	virtual void RegisterPickTarget(FPickTargets& Targets) const;

protected:
	AActor* mOwner;
};

