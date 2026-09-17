#pragma once

#include "Engine/Serialization/PropertyJson.h"

namespace json
{
	class JSON;
}

class UObject;

struct FPropertyInfo
{
	const char* JsonKey = nullptr;

	void (*Serialize)(
		const FPropertyInfo& Property,
		const UObject* Object,
		json::JSON& OutProperties) = nullptr;

	void (*Deserialize)(
		const FPropertyInfo& Property,
		UObject* Object,
		const json::JSON& InProperties) = nullptr;
};

#define REFLECT_PROPERTY(OwnerType, MemberName)                 \
    MakeProperty<                                               \
        OwnerType,                                              \
        decltype(OwnerType::MemberName),                        \
        &OwnerType::MemberName                                  \
>(#MemberName)

template<
	typename TOwner,
	typename TValue,
	TValue TOwner::* Member>
FPropertyInfo MakeProperty(const char* JsonKey)
{
	FPropertyInfo Property;

	Property.JsonKey = JsonKey;

	Property.Serialize =
		[](const FPropertyInfo& Property,
			const UObject* Object,
			json::JSON& OutJson)
		{
			const TOwner* Owner = static_cast<const TOwner*>(Object);

			const TValue& Value = Owner->*Member;

			TPropertyJsonSerializer<TValue>::Serialize(
				OutJson,
				Property.JsonKey,
				Value);
		};

	Property.Deserialize =
		[](const FPropertyInfo& Property,
			UObject* Object,
			const json::JSON& InJson)
		{
			TOwner* Owner = static_cast<TOwner*>(Object);

			TValue& Value = Owner->*Member;

			TPropertyJsonSerializer<TValue>::Deserialize(
				InJson,
				Property.JsonKey,
				Value);
		};

	return Property;
}
