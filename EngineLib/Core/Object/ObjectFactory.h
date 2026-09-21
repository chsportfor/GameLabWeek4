#pragma once

#include <functional>
#include <memory>

#include "Core/enum.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Rotator.h"
#include "Core/Container/TMap.h"
#include "Core/Name.h"

namespace json { class JSON; }

class UObject;
class AActor;
struct FClassInfo;
class UFontAtlasAsset;
class UAssetManager;

struct FObjectFactory
{
	static void SetDefaultAssetManager(UAssetManager* InAssetManager);
	static UAssetManager* GetDefaultAssetManager();

	static void SetDefaultFontAsset(UFontAtlasAsset* FontAsset);
	static UFontAtlasAsset* GetDefaultFontAsset();

	static UObject* ConstructUnInitializedObject(const FClassInfo* classInfo);
	static UObject* LoadObject(const FClassInfo* classInfo, const json::JSON& inJson);

	template<typename TObject, typename... Args>
		requires std::derived_from<TObject, UObject>
	static TObject* ConstructObject(Args&& ...args);

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	static TObject* ConstructUnInitializedObject();

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	static TObject* ConstructUnInitializedObject(const FName& Name);

	template<typename TObject, typename... Args>
		requires std::derived_from<TObject, UObject>
	static TObject* ConstructObjectWithName(const FName& Name, Args&&... args);

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	static TObject* LoadObject(const json::JSON& inJson);

	static AActor* SpawnPrimitiveActor(EPrimitive primitiveType,
		FVector3 Location, FRotator Rotation, FVector3 Scale
	);

	static AActor* SpawnParticleActor(FVector3 Location, FRotator Rotation, FVector3 Scale);

	static const FClassInfo* GetClassInfoByName(const FString& className);

	static bool RegisterClassInfo(FString className, const FClassInfo* classInfo);

private:
	// TODO: Automate the registration of class info for all UObject-derived classes.
	static TMap<FName, std::function<const FClassInfo* ()>> mClassInfoMap;

	static UFontAtlasAsset* mDefaultFontAsset;

	static UAssetManager* mAssetManager;
};


#include "ObjectFactory.inl"
