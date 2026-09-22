
#include "Object.h"
#include "ThirdParty/Json/json.hpp"
#include "Core/Name.h"

FUObjectArray UObject::GUObjectArray;

void* UObject::operator new(std::size_t Size)
{
	void* Memory = ::operator new(Size);
	TotalAllocationBytes.fetch_add(Size, std::memory_order_relaxed);
	TotalAllocationCount.fetch_add(1, std::memory_order_relaxed);
	return Memory;
}

void UObject::operator delete(void* Memory, std::size_t Size) noexcept
{
	if (!Memory) return;
	TotalAllocationBytes.fetch_sub(Size, std::memory_order_relaxed);
	TotalAllocationCount.fetch_sub(1, std::memory_order_relaxed);
	::operator delete(Memory);
}

void* UObject::operator new(std::size_t Size, std::align_val_t Alignment)
{
	void* Memory = ::operator new(Size, Alignment);
	TotalAllocationBytes.fetch_add(Size, std::memory_order_relaxed);
	TotalAllocationCount.fetch_add(1, std::memory_order_relaxed);
	return Memory;
}

void UObject::operator delete(void* Memory, std::size_t Size, std::align_val_t Alignment) noexcept
{
	if (!Memory) return;
	TotalAllocationBytes.fetch_sub(Size, std::memory_order_relaxed);
	TotalAllocationCount.fetch_sub(1, std::memory_order_relaxed);
	::operator delete(Memory, Alignment);
}

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
