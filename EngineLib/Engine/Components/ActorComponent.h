#pragma once

#include "Core/Object/Object.h"

struct FRenderInfo;

class UActorComponent : public UObject
{
	DECLARE_OBJECT(UActorComponent, UObject)
public:
	UActorComponent();
	virtual ~UActorComponent();

	void SetOwner(AActor* owner);
	void ClearOwner();
	AActor* GetOwner() const;

	// Todo: Make as pure class
	virtual void Update(float deltaTime, TArray<FRenderInfo>* outRenderInfos);
	virtual void GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const;

protected:
	AActor* mOwner;
};

