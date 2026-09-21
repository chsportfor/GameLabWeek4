
#include "Object.h"
#include "ThirdParty/Json/json.hpp"
#include "Core/Name.h"

FUObjectArray UObject::GUObjectArray;

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
	GUObjectRevision++;
}

UObject::~UObject()
{
	GUObjectArray.Remove(InternalIndex, this);
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

std::span<const FPropertyInfo> UObject::GetDeclaredProperties()
{
	static const FPropertyInfo Properties[] =
	{
		MakeProperty<
			UObject,
			FName,
			&UObject::mName>(
				"Name")
	};

	return Properties;
}
