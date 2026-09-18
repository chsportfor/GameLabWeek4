#include "Actor.h"

#include <format>

#include "Core/IO/JsonUtil.h"
#include "Rendering/RenderInfo.h"
#include "Engine/Components/SceneComponent.h"

#include "Components/NameComponent.h"

IMPLEMENT_CLASS(AActor, UObject);

AActor::~AActor()
{
	for (UActorComponent* removeComponent : mComponents)
	{
		if (removeComponent)
		{
			removeComponent->Destroy();
		}
	}
}

void AActor::Initialize()
{
	UObject::Initialize();

	mbPressed = false;
	mbStarted = false;
}

void AActor::SetName(const FName& name)
{
	UObject::SetName(name);

	// NOTE: Only the first UNameComponent will be updated.
	// If there are multiple UNameComponents, consider updating all of them
	UNameComponent* nameComponent = GetComponentByType<UNameComponent>();
	if (nameComponent)
	{
		nameComponent->SetNameText(name.ToString());
	}
}

void AActor::SerializeClass(json::JSON& outJson) const
{
	UObject::SerializeClass(outJson);
	json::JSON componentsJson = json::JSON::Make(json::JSON::Class::Array);

	for (const UActorComponent* component : mComponents)
	{
		json::JSON componentJson;
		component->SerializeClass(componentJson);
		componentsJson.append(std::move(componentJson));
	}
	outJson["Properties"]["mComponents"] = componentsJson;
	outJson["Properties"]["mRootComponentUUID"] = mRootComponent ? mRootComponent->UUID : -1;
}

void AActor::DeserializeClass(const json::JSON& inJson)
{
	UObject::DeserializeClass(inJson);

	const json::JSON& propertiesJson = inJson.at("Properties");

	if (!propertiesJson.hasKey("mComponents") || propertiesJson.at("mComponents").JSONType() != json::JSON::Class::Array)
	{
		throw std::runtime_error(std::format("{}: mComponents requires an array", GetRuntimeClass()->Name));
	}

	const json::JSON& componentsJson = propertiesJson.at("mComponents");

	for (const auto& componentJson : componentsJson.ArrayRange())
	{
		if (!componentJson.hasKey("ClassName") || componentJson.at("ClassName").JSONType() != json::JSON::Class::String)
		{
			throw std::runtime_error(std::format("{}: ClassName requires a string", GetRuntimeClass()->Name));
		}
		FString className(componentJson.at("ClassName").ToString());

		const FClassInfo* classInfo = FObjectFactory::GetClassInfoByName(className);
		if (!classInfo)
		{
			throw std::runtime_error(std::format("{}: Unknown class name: {}", GetRuntimeClass()->Name, className));
		}
		UActorComponent* component = static_cast<UActorComponent*>(FObjectFactory::LoadObject(classInfo, componentJson));
		AddComponent(component);
		component->PostDeserialize();
	}

	for (UActorComponent* Component : mComponents)
	{
		USceneComponent* SceneComponent = Component->Cast<USceneComponent>();
		if (SceneComponent == nullptr)
		{
			continue;
		}

		const int32 ParentUUID = SceneComponent->GetSerializedParentUUID();
		if (ParentUUID == -1)
		{
			continue;
		}

		USceneComponent* Parent = UObject::GetObjectByUUID<USceneComponent>(ParentUUID);
		if (Parent == nullptr)
		{
			throw std::runtime_error("Failed to restore component parent");
		}

		SceneComponent->AttachTo(*Parent);
	}

	if (!propertiesJson.hasKey("mRootComponentUUID") || propertiesJson.at("mRootComponentUUID").JSONType() != json::JSON::Class::Integral)
	{
		throw std::runtime_error(std::format("{}: mRootComponentUUID requires an integral", GetRuntimeClass()->Name));
	}
	int32 rootComponentUUID = propertiesJson.at("mRootComponentUUID").ToInt();
	if (rootComponentUUID == -1)
	{
		mRootComponent = nullptr;
	}
	else
	{
		int32 rootComponentIndex = getComponentIndex(rootComponentUUID);
		if (rootComponentIndex == -1)
		{
			throw std::runtime_error(std::format("{}: Invalid root component UUID: {}", GetRuntimeClass()->Name, rootComponentUUID));
		}
		mRootComponent = static_cast<USceneComponent*>(mComponents[rootComponentIndex]);
	}

}

void AActor::AddComponent(UActorComponent* actorComponent)
{
	assert(actorComponent);
	assert(getComponentIndex(actorComponent->UUID) == -1);

	mComponents.Add(actorComponent);
	actorComponent->SetOwner(this);
}

void AActor::AddRootSceneComponent(USceneComponent* sceneComponent)
{
	assert(sceneComponent);
	assert(getComponentIndex(sceneComponent->UUID) == -1);

	mRootComponent = sceneComponent;
	AddComponent(sceneComponent);
}

bool AActor::RemoveComponent(uint32 componentUUID)
{
	int32 componentIndex = getComponentIndex(componentUUID);
	if (componentIndex == -1)
	{
		return false;
	}

	UActorComponent* component = mComponents[componentIndex];

	if (auto* sceneComponent = component->Cast<USceneComponent>())
	{
		sceneComponent->DetachFromParent();
		sceneComponent->DetachAllChildren();
	}
	if (component == mRootComponent)
	{
		mRootComponent = nullptr;
	}

	mComponents.RemoveAtSwap(componentIndex);
	component->ClearOwner();

	return true;
}

bool AActor::DestroyComponent(uint32 componentUUID)
{
	int32 componentIndex = getComponentIndex(componentUUID);
	if (componentIndex == -1)
	{
		return false;
	}

	UActorComponent* component = mComponents[componentIndex];

	if (!RemoveComponent(componentUUID))
	{
		return false;
	}

	component->Destroy();
	return true;
}

FTransform AActor::GetTransform() const
{
	if (mRootComponent)
	{
		// Return the transform of the root component
		return {
			mRootComponent->GetRelativeLocation(),
			mRootComponent->GetRelativeRotation().Quaternion(),
			mRootComponent->GetRelativeScale3D(),
		};
	}
	else
	{
		return FTransform();
	}
}

FRotator AActor::GetRotator() const
{
	if (mRootComponent)
	{
		return mRootComponent->GetRelativeRotation();
	}
	else
	{
		return FRotator();
	}
}

FQuat AActor::GetRotation() const
{
	if (mRootComponent)
	{
		return mRootComponent->GetRelativeRotation().Quaternion();
	}
	else
	{
		return FQuat();
	}
}


void AActor::Update(float deltaTime)
{
	for (UActorComponent* component : mComponents)
	{
		component->Update(deltaTime);
	}
}

void AActor::SubmitRenderInfos(FRenderCollector& Collector) const
{
    for (const UActorComponent* Component : mComponents) Component->SubmitRenderInfos(Collector);
}

void AActor::SubmitPickInfos(TArray<FPickInfo>& Infos, const FCamera& Camera) const
{
    for (const UActorComponent* Component : mComponents) Component->SubmitPickInfos(Infos, Camera);
}

void AActor::SetLocation(FVector location)
{
	if (mRootComponent)
	{
		mRootComponent->SetRelativeLocation(location);
	}
}

void AActor::SetRotation(FRotator rotation)
{
	if (mRootComponent)
	{
		mRootComponent->SetRelativeRotation(rotation);
	}
}

void AActor::SetRotation(FQuat rotation)
{
	if (mRootComponent)
	{
		mRootComponent->SetRelativeRotation(rotation);
	}
}

void AActor::SetScale(FVector scale)
{
	if (mRootComponent)
	{
		mRootComponent->SetRelativeScale3D(scale);
	}
}

int32 AActor::getComponentIndex(uint32 componentUUID) const
{
	for (uint32 i = 0; i < mComponents.Num(); ++i)
	{
		if (mComponents[i]->UUID == componentUUID)
		{
			return i;
		}
	}

	return -1;
}
