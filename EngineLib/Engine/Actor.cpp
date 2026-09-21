#include "Actor.h"

#include <format>
#include <unordered_map>
#include "World.h"

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
	UObject::SetName(mWorld ? mWorld->ResolveActorName(name, this, true) : name);

	// NOTE: Only the first UNameComponent will be updated.
	// If there are multiple UNameComponents, consider updating all of them
	UNameComponent* nameComponent = GetComponentByType<UNameComponent>();
	if (nameComponent)
	{
		nameComponent->SetNameText(GetName().ToString());
	}
}

// These indices belong only to this actor's serialized component array.
void AActor::SerializeClass(json::JSON& outJson) const
{
    UObject::SerializeClass(outJson);
    std::unordered_map<const UActorComponent*, int32> indices;
    for (int32 i = 0; i < mComponents.Num(); ++i) indices.emplace(mComponents[i], i);
    auto indexOf = [&](const UActorComponent* component) -> int32
    {
        if (!component) return -1;
        auto found = indices.find(component);
        if (found == indices.end()) throw std::runtime_error("Component reference is outside its actor");
        return found->second;
    };

    auto componentsJson = json::JSON::Make(json::JSON::Class::Array);
    for (const UActorComponent* component : mComponents)
    {
        json::JSON componentJson;
        component->SerializeClass(componentJson);
        const auto* sceneComponent = component->Cast<USceneComponent>();
        componentJson["ParentIndex"] = indexOf(sceneComponent ? sceneComponent->GetParent() : nullptr);
        componentsJson.append(std::move(componentJson));
    }
    outJson["Properties"]["mComponents"] = std::move(componentsJson);
    outJson["Properties"]["RootComponentIndex"] = indexOf(mRootComponent);
}

void AActor::DeserializeClass(const json::JSON& inJson)
{
    UObject::DeserializeClass(inJson);
    if (!mComponents.IsEmpty()) throw std::runtime_error("Load components into a new actor");
    const auto& properties = inJson.at("Properties");
    if (!properties.hasKey("mComponents") || properties.at("mComponents").JSONType() != json::JSON::Class::Array)
        throw std::runtime_error("mComponents requires an array");
    const auto& componentsJson = properties.at("mComponents");
    const int32 count = componentsJson.length();
    auto readIndex = [count](const json::JSON& value, const char* key) -> int32
    {
        if (!value.hasKey(key) || value.at(key).JSONType() != json::JSON::Class::Integral)
            throw std::runtime_error(std::string(key) + " requires an integer");
        const auto index = value.at(key).ToInt();
        if (index < -1 || index >= count)
            throw std::runtime_error(std::string(key) + " is outside the component array");
        return static_cast<int32>(index);
    };
    const int32 rootIndex = readIndex(properties, "RootComponentIndex");
    TArray<int32> parentIndices;
    parentIndices.Reserve(count);
    mComponents.Reserve(count);

    // Pass 1: create components in file order and retain relationship indices locally.
    for (const auto& componentJson : componentsJson.ArrayRange())
    {
        parentIndices.Add(readIndex(componentJson, "ParentIndex"));
        if (!componentJson.hasKey("ClassName") || componentJson.at("ClassName").JSONType() != json::JSON::Class::String)
            throw std::runtime_error("Component ClassName requires a string");
        const auto* classInfo = FObjectFactory::GetClassInfoByName(FString(componentJson.at("ClassName").ToString()));
        const auto* base = classInfo;
        while (base && base != UActorComponent::GetClass()) base = base->SuperClass;
        if (!base) throw std::runtime_error("Unknown or non-component class in component array");
        std::unique_ptr<UActorComponent> component(
            static_cast<UActorComponent*>(FObjectFactory::LoadObject(classInfo, componentJson)));
        if (!component) throw std::runtime_error("Could not create component");
        AddComponent(component.get());
        component.release(); // The actor now owns it, including on a later load failure.
    }

    // Pass 2: every target now exists, including parents appearing later in the file.
    for (int32 i = 0; i < count; ++i)
    {
        const int32 parentIndex = parentIndices[i];
        if (parentIndex == -1) continue;
        auto* child = mComponents[i]->Cast<USceneComponent>();
        auto* parent = mComponents[parentIndex]->Cast<USceneComponent>();
        if (!child || !parent || !child->AttachTo(*parent))
            throw std::runtime_error("Invalid component parent type or cyclic attachment");
    }
    if (rootIndex != -1)
    {
        mRootComponent = mComponents[rootIndex]->Cast<USceneComponent>();
        if (!mRootComponent || mRootComponent->GetParent())
            throw std::runtime_error("Root must be a scene component without a parent");
    }
    // Post-load work can now see the complete owner/parent/root relationships.
    for (auto* component : mComponents) component->PostDeserialize();
}

void AActor::AddComponent(UActorComponent* actorComponent)
{
	assert(actorComponent);
	assert(getComponentIndex(actorComponent) == -1);

	mComponents.Add(actorComponent);
	actorComponent->SetOwner(this);
}

void AActor::AddRootSceneComponent(USceneComponent* sceneComponent)
{
	assert(sceneComponent);
	assert(getComponentIndex(sceneComponent) == -1);

	mRootComponent = sceneComponent;
	AddComponent(sceneComponent);
}

bool AActor::RemoveComponent(UActorComponent* target)
{
	int32 componentIndex = getComponentIndex(target);
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

bool AActor::DestroyComponent(UActorComponent* target)
{
	int32 componentIndex = getComponentIndex(target);
	if (componentIndex == -1)
	{
		return false;
	}

	UActorComponent* component = mComponents[componentIndex];

	if (!RemoveComponent(target))
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

void AActor::RegisterPickTargets(FPickTargets& Targets) const
{
    for (const UActorComponent* Component : mComponents) Component->RegisterPickTarget(Targets);
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

int32 AActor::getComponentIndex(UActorComponent* target) const
{
	for (uint32 i = 0; i < mComponents.Num(); ++i)
	{
		if (mComponents[i] == target)
		{
			return i;
		}
	}

	return -1;
}
