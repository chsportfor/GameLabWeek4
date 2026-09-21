#include "ObjectFactory.h"

#include "ThirdParty/Json/json.hpp"

#include "Engine/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Components/PrimitiveComponent.h"
#include "Engine/Components/UStaticMeshComponent.h"
#include "Engine/Components/NameComponent.h"
#include "Engine/Components/ParticleSubUVComponent.h"
#include "Engine/Components/CubeComponent.h"
#include "Engine/Components/SphereComponent.h"

#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Core/AssetSystem/AssetManager.h"

#include "Object.h"

UAssetManager* FObjectFactory::mAssetManager = nullptr;

UFontAtlasAsset* FObjectFactory::mDefaultFontAsset;

void FObjectFactory::SetDefaultAssetManager(UAssetManager* InAssetManager)
{
	mAssetManager = std::move(InAssetManager);
}

UAssetManager* FObjectFactory::GetDefaultAssetManager()
{
	return mAssetManager;
}

void FObjectFactory::SetDefaultFontAsset(UFontAtlasAsset* FontAsset)
{
    mDefaultFontAsset = FontAsset;
}

UFontAtlasAsset* FObjectFactory::GetDefaultFontAsset()
{
    return mDefaultFontAsset;
}

UObject* FObjectFactory::ConstructUnInitializedObject(const FClassInfo* classInfo)
{
	if (!classInfo || !classInfo->Constructor)
	{
		return nullptr;
	}

	UObject* instance = classInfo->CreateInstance();

	if (instance)
	{
		instance->mClassInfo = classInfo;
		instance->mName = FName(classInfo->Name);
	}
	return instance;
}

UObject* FObjectFactory::LoadObject(const FClassInfo* classInfo, const json::JSON& inJson)
{
	std::unique_ptr<UObject> instance(ConstructUnInitializedObject(classInfo));

	if (instance)
	{
		instance->DeserializeClass(inJson);
	}
	return instance.release();
}

AActor* FObjectFactory::SpawnPrimitiveActor(
	EPrimitive primitiveType,
	FVector3 Location, FRotator Rotation, FVector3 Scale)
{
	FName PrimitiveName(PrimitiveToString(primitiveType));

	AActor* actor = ConstructObjectWithName<AActor>(PrimitiveName);

	UPrimitiveComponent* component = nullptr;

	if (primitiveType == EPrimitive::EP_Cube)
	{
		component = ConstructObject<UCubeComponent>(Location, Rotation, Scale);
	}
	else if (primitiveType == EPrimitive::EP_Sphere)
	{
		component = ConstructObject<USphereComponent>(Location, Rotation, Scale);
	}
	else
	{
		component = ConstructObject<UPrimitiveComponent>(
			primitiveType, Location, Rotation, Scale);
	}

	actor->AddRootSceneComponent(component);

	/* DEBUG */
	assert(mDefaultFontAsset && "FObjectFactory::SetDefaultFontAsset must be called before SpawnPrimitiveActor.");
	UNameComponent& billboardComponent = actor->CreateAndAddComponent<UNameComponent>(
		actor->GetName().ToString(), FVector3{0, 0, 1}, mDefaultFontAsset);
	billboardComponent.AttachTo(*component);
	return actor;
}

AActor* FObjectFactory::SpawnParticleActor(FVector3 Location, FRotator Rotation, FVector3 Scale)
{
	FName ParticleName("Particle");
	AActor* actor = ConstructObjectWithName<AActor>(ParticleName);

	UParticleSubUVComponent* component = ConstructObject<UParticleSubUVComponent>(
		Location, Rotation, Scale, 6, 6, true, 1.0f, 0.1f);
	actor->AddRootSceneComponent(component);

	/* DEBUG */




	return actor;
}

const FClassInfo* FObjectFactory::GetClassInfoByName(const FString& className)
{
	const FName classKey(className);

	if (!mClassInfoMap.Contains(classKey))
	{
		return nullptr;
	}

	return mClassInfoMap[classKey]();
}

bool FObjectFactory::RegisterClassInfo(FString className, const FClassInfo* classInfo)
{
	const FName classKey(className);

	if (mClassInfoMap.Contains(classKey))
	{
		return false;
	}
	mClassInfoMap.Add(classKey, [classInfo]() -> const FClassInfo* { return classInfo; });
	return true;
}

#include "Engine/Components/SceneComponent.h"
#include "Engine/Components/CubeComponent.h"
#include "Engine/Components/SphereComponent.h"
#include "Engine/World.h"
#include "Engine/Components/BillboardComponent.h"
#include "Engine/Components/ParticleSubUVComponent.h"

TMap<FName, std::function<const FClassInfo* ()>> FObjectFactory::mClassInfoMap = {
	{"UObject", &UObject::GetClass },
	{"UAsset", &UAsset::GetClass },
	{"UAssetManager", &UAssetManager::GetClass },
	{"UStaticMeshAsset", &UStaticMeshAsset::GetClass },
	{"UTexture2D", &UTexture2D::GetClass },
	{"UFontAtlasAsset", &UFontAtlasAsset::GetClass },
	{"UMaterial", &UMaterial::GetClass },
	{"UMeshComponent", &UMeshComponent::GetClass },
	{"AActor", &AActor::GetClass },
	{"AStaticMeshActor", &AStaticMeshActor::GetClass },
	{"UActorComponent", &UActorComponent::GetClass },
	{"USceneComponent", &USceneComponent::GetClass },
	{"UPrimitiveComponent", &UPrimitiveComponent::GetClass },
	{"UCubeComponent", &UCubeComponent::GetClass },
	{"USphereComponent", &USphereComponent::GetClass },
	{"UBillboardComponent", &UBillboardComponent::GetClass },
	{"UWorld", &UWorld::GetClass },
	{"UNameComponent",& UNameComponent::GetClass },
	{"UParticleSubUVComponent",&UParticleSubUVComponent::GetClass },
	{ "UStaticMeshComponent",& UStaticMeshComponent::GetClass }
};
