
#include "Object.h"
#include "Engine/EngineStatics.h"
#include "ThirdParty/Json/json.hpp"
#include "Core/Name.h"

TSparseArray<UObject*> UObject::GUObjectArray;

UObject* FClassInfo::CreateInstance() const
{
	if (Constructor)
	{
		return Constructor();
	}
	return nullptr;
}


UObject::UObject()
{
	InternalIndex = GUObjectArray.Add(this);
	UUID = UEngineStatics::GenerateUUID();
	GUObjectRevision++;
}

UObject::~UObject()
{
	/*
	// Ensure that the object is in the GUObjectArray before attempting to remove it
	if (GUObjectArray.Num() < InternalIndex || GUObjectArray[InternalIndex] != this)
	{
		assert(false && "Invalid InternalIndex or GUObjectArray mismatch.");
		return;
	}
	*/

	GUObjectArray.RemoveAt(InternalIndex);
	GUObjectRevision++;
}

void UObject::Destroy()
{
	delete this;
}

void UObject::Initialize()
{

}

FClassInfo UObject::ClassInfo(
	"UObject",
	nullptr,
	[]() -> UObject*
	{
		return new UObject();
	},
	UObject::GetDeclaredProperties()
);

const FClassInfo* UObject::GetClass()
{
	return &ClassInfo;
}

void UObject::SerializeClass(json::JSON& outJson) const
{
	outJson["ClassName"] = GetRuntimeClass()->Name;
	json::JSON propertiesJson = json::JSON::Make(json::JSON::Class::Object);

	for (const FPropertyInfo& Property : ClassInfo.DeclaredProperties)
	{
		Property.Serialize(
			Property,
			this,
			propertiesJson);
	}

	outJson["Properties"] = propertiesJson;
}

void UObject::DeserializeClass(const json::JSON& inJson)
{
	if (!inJson.hasKey("Properties") || inJson.at("Properties").JSONType() != json::JSON::Class::Object)
	{
		throw std::runtime_error("Invalid JSON format for Properties");
	}
	const json::JSON& propertiesJson = inJson.at("Properties");

	for (const FPropertyInfo& Property : ClassInfo.DeclaredProperties)
	{
		Property.Deserialize(
			Property,
			this,
			propertiesJson);
	}
}

bool UObject::IsA(const FClassInfo* classInfo) const
{
	const FClassInfo* currentClass = GetRuntimeClass();
	while (currentClass)
	{
		if (currentClass == classInfo)
		{
			return true;
		}
		currentClass = currentClass->SuperClass;
	}
	return false;
}

UObject* UObject::GetObjectByUUID(int32 uuid)
{
	for (const auto& object : GUObjectArray)
	{
		if (object && object->UUID == uuid)
		{
			return object;
		}
	}
	return nullptr;
}

UObject* UObject::GetObjectByInternalIndex(uint32 internalIndex)
{
	if (GUObjectArray.IsValidIndex(internalIndex))
	{
		return GUObjectArray[internalIndex];
	}
	return nullptr;
}

std::span<const FPropertyInfo> UObject::GetDeclaredProperties()
{
	static const FPropertyInfo Properties[] =
	{
		MakeProperty<
			UObject,
			int32,
			&UObject::UUID>(
				"UUID"),

		MakeProperty<
			UObject,
			FName,
			&UObject::mName>(
				"Name")
	};

	return Properties;
}
