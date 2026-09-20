#include "World.h"

#include <format>

#include "Rendering/RenderInfo.h"
#include "Core/IO/JsonUtil.h"
#include "Engine/Serialization/PropertyJson.h"
#include "Editor/Console.h"

IMPLEMENT_CLASS(UWorld, UObject);

UWorld::~UWorld()
{
	for (AActor* removeActor : mActors)
	{
		removeActor->mWorld = nullptr;
		delete removeActor;
	}
}

void UWorld::SerializeClass(json::JSON& outJson) const
{
	UObject::SerializeClass(outJson);
	json::JSON actorsJson = json::JSON::Make(json::JSON::Class::Array);

	for (const AActor* actor : mActors)
	{
		json::JSON actorJson;
		actor->SerializeClass(actorJson);
		actorsJson.append(std::move(actorJson));
	}
	outJson["Properties"]["mActors"] = actorsJson;
}

void UWorld::DeserializeClass(const json::JSON& inJson)
{
	UObject::DeserializeClass(inJson);

	const json::JSON& propertiesJson = inJson.at("Properties");

	if (!propertiesJson.hasKey("mActors") || propertiesJson.at("mActors").JSONType() != json::JSON::Class::Array)
	{
		throw std::runtime_error(std::format("{}: mActors requires an array", GetRuntimeClass()->Name));
	}

	const json::JSON& actorsJson = propertiesJson.at("mActors");

	// Reserve every saved number first so repairing a duplicate cannot take a
	// distinct name belonging to an actor later in the file.
	for (const auto& actorJson : actorsJson.ArrayRange())
	{
		FName name;
		TPropertyJsonSerializer<FName>::Deserialize(actorJson.at("Properties"), "Name", name);
		ObserveActorName(name);
	}

	for (const auto& actorJson : actorsJson.ArrayRange())
	{
		if (!actorJson.hasKey("ClassName") || actorJson.at("ClassName").JSONType() != json::JSON::Class::String)
		{
			throw std::runtime_error(std::format("{}: ClassName requires a string", GetRuntimeClass()->Name));
		}
		FString className(actorJson.at("ClassName").ToString());

		const FClassInfo* classInfo = FObjectFactory::GetClassInfoByName(className);
		if (!classInfo)
		{
			throw std::runtime_error(std::format("{}: Unknown class name: {}", GetRuntimeClass()->Name, className));
		}
		AActor* actor = static_cast<AActor*>(FObjectFactory::LoadObject(classInfo, actorJson));
		RegisterActor(actor, true);
	}
}

void UWorld::AddActor(AActor* actor)
{
	RegisterActor(actor, false);
}

void UWorld::RegisterActor(AActor* actor, bool preserveName)
{
	assert(actor != nullptr);
	assert(actor->mWorld == nullptr);
	assert(getActorIndex(actor->UUID) == -1); // TODO

	actor->SetName(ResolveActorName(actor->GetName(), nullptr, preserveName));
	mActors.Add(actor);
	actor->mWorld = this;
}

void UWorld::ObserveActorName(const FName& name)
{
	uint64& nextNumber = mNextActorNameNumbers[name.ComparisonIndex];
	const uint64 afterName = static_cast<uint64>(name.Number) + 1;
	if (nextNumber < afterName) nextNumber = afterName;
}

bool UWorld::IsActorNameUsed(const FName& name, const AActor* ignoredActor) const
{
	for (const AActor* actor : mActors)
	{
		if (actor != ignoredActor && actor->GetName() == name) return true;
	}
	return false;
}

FName UWorld::ResolveActorName(const FName& name, const AActor* ignoredActor, bool preserveName)
{
	FName resolvedName = name.IsValid() ? name : FName("Actor");
	// Loading and explicit renaming keep an available name. Normal spawning
	// and copying use the next number, even if an earlier actor was removed.
	if (!preserveName || IsActorNameUsed(resolvedName, ignoredActor))
	{
		const uint64* nextNumber = mNextActorNameNumbers.Find(resolvedName.ComparisonIndex);
		if (nextNumber && *nextNumber > resolvedName.Number)
		{
			if (*nextNumber > UINT32_MAX)
				throw std::overflow_error("Actor name number exhausted");
			resolvedName.Number = static_cast<uint32>(*nextNumber);
		}
	}
	ObserveActorName(resolvedName);
	return resolvedName;
}

bool UWorld::RemoveActor(uint32 componentUUID)
{
	int32 componentIndex = getActorIndex(componentUUID);
	if (componentIndex == -1)
	{
		return false;
	}

	mActors[componentIndex]->mWorld = nullptr;
	mActors.RemoveAtSwap(componentIndex);

	return true;
}

void UWorld::SubmitRenderInfos(FRenderCollector& Collector) const
{
    for (const AActor* Actor : mActors) Actor->SubmitRenderInfos(Collector);
}

void UWorld::SubmitPickInfos(TArray<FPickInfo>& Infos, const FCamera& Camera) const
{
    for (const AActor* Actor : mActors) Actor->SubmitPickInfos(Infos, Camera);
}

void UWorld::Update(float deltaTime)
{

	for (AActor* actor : mActors)
	{
		actor->Update(deltaTime);
	}
}



int32 UWorld::getActorIndex(uint32 actorUUID) const
{
	for (uint32 i = 0; i < mActors.Num(); ++i)
	{
		if (mActors[i]->UUID == actorUUID)
		{
			return i;
		}
	}

	return -1;
}
