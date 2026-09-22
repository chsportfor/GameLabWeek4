#pragma once

#include <functional>
#include <atomic>
#include <new>
#include <span>
#include "PropertyInfo.h"

#include "Core/Core.h"
#include "Core/Container/TArray.h"
#include "ObjectArray.h"
#include "WeakObjectPtr.h"
#include "ObjectFactory.h"
#include "Core/Name.h"

namespace json { class JSON; }

class UObject;

//using ConstructorFunc = UObject * (*)();

struct FClassInfo
{
	FString Name;
	const FClassInfo* SuperClass;
	std::function<UObject* ()> Constructor;

	std::span<const FPropertyInfo> DeclaredProperties = {};

	FClassInfo(FString name, const FClassInfo* superClass,
		std::function<UObject* ()> constructor, std::span<const FPropertyInfo> declaredProperties = {})
		: Name(std::move(name)), SuperClass(superClass), Constructor(std::move(constructor)),
		DeclaredProperties(declaredProperties) {
	}

	UObject* CreateInstance() const;

private:
};

class UObject
{
public:
	uint32 InternalIndex;

	inline const FName& GetName() const
	{
		return mName;
	}

	inline virtual void SetName(const FName& name)
	{
		mName = name;
	}

	virtual ~UObject();

	static void* operator new(std::size_t Size);
	static void operator delete(void* Memory, std::size_t Size) noexcept;
	static void* operator new(std::size_t Size, std::align_val_t Alignment);
	static void operator delete(void* Memory, std::size_t Size, std::align_val_t Alignment) noexcept;
	static void* operator new[](std::size_t) = delete;
	static void operator delete[](void*) = delete;

	// Live UObject allocations only; excludes separately allocated member data and GPU resources.
	static uint64 GetTotalAllocationCount() { return TotalAllocationCount.load(std::memory_order_relaxed); }
	static uint64 GetTotalAllocationBytes() { return TotalAllocationBytes.load(std::memory_order_relaxed); }
	UObject(const UObject&) = delete;
	UObject& operator=(const UObject&) = delete;
	virtual void Destroy();

	void Initialize();

	// StaticClass() in Unreal Engine
	static FClassInfo ClassInfo;
	static const FClassInfo* GetClass();

	// GetClass() in Unreal Engine
	inline const FClassInfo* GetRuntimeClass() const { return mClassInfo; }

	// TODO?: Replace json type with a more generic type, such as a variant or a map
	virtual void SerializeClass(json::JSON& outJson) const;
	virtual void DeserializeClass(const json::JSON& inJson);

	virtual void PostDeserialize() {}

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	bool IsA() const;

	bool IsA(const FClassInfo* classInfo) const;

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	TObject* Cast();

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	const TObject* Cast() const;
	

	static const FUObjectArray& GetGObjectArray() { return GUObjectArray; }
	inline static uint64 GetGObjectRevision() { return GUObjectRevision; }

	static std::span<const FPropertyInfo> GetDeclaredProperties();

private:
	static FUObjectArray GUObjectArray;
	inline static std::atomic<uint64> TotalAllocationCount{0};
	inline static std::atomic<uint64> TotalAllocationBytes{0};

protected:
	UObject();
	inline static uint64 GUObjectRevision = 0;

private:
	friend struct FObjectFactory;

	FName mName;
	const FClassInfo* mClassInfo = nullptr;
};

#include  "Object.inl"
