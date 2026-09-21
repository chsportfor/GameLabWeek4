#pragma once

#include "ThirdParty/Json/json.hpp"
#include "Core/IO/JsonUtil.h"
#include "Core/Name.h"
#include "Core/Container/TArray.h"
#include "Core/Math/Color.h"


template<typename T>
struct TPropertyJsonSerializer;

class UAsset;
class UObject;
struct FClassInfo;
UObject* LoadAssetReference(const FName& Name, const FClassInfo* ExpectedClass);

// Asset references are paths, not inline UObject data or runtime object IDs.
template<typename T> requires std::is_base_of_v<UAsset, T>
struct TPropertyJsonSerializer<T*>
{
    static void Serialize(json::JSON& OutJson, const char* Key, T* const& Value)
    {
        OutJson[Key] = Value ? json::JSON(Value->GetName().ToString().CStr()) : json::JSON();
    }
    static void Deserialize(const json::JSON& InJson, const char* Key, T*& OutValue)
    {
        if (!InJson.hasKey(Key)) throw std::runtime_error("Missing asset reference");
        const auto& value = InJson.at(Key);
        if (value.JSONType() == json::JSON::Class::Null) { OutValue = nullptr; return; }
        if (value.JSONType() != json::JSON::Class::String) throw std::runtime_error("Asset reference requires a path");
        OutValue = static_cast<T*>(LoadAssetReference(FName(FString(value.ToString())), T::GetClass()));
    }
};

template<typename T> requires std::is_base_of_v<UAsset, T>
struct TPropertyJsonSerializer<TArray<T*>>
{
    static void Serialize(json::JSON& OutJson, const char* Key, const TArray<T*>& Values)
    {
        auto array = json::JSON::Make(json::JSON::Class::Array);
        for (T* value : Values) array.append(value ? json::JSON(value->GetName().ToString().CStr()) : json::JSON());
        OutJson[Key] = std::move(array);
    }
    static void Deserialize(const json::JSON& InJson, const char* Key, TArray<T*>& Values)
    {
        if (!InJson.hasKey(Key) || InJson.at(Key).JSONType() != json::JSON::Class::Array)
            throw std::runtime_error("Asset reference array required");
        Values.Empty();
        for (const auto& value : InJson.at(Key).ArrayRange())
        {
            json::JSON item; item["Asset"] = value;
            T* asset = nullptr;
            TPropertyJsonSerializer<T*>::Deserialize(item, "Asset", asset);
            Values.Add(asset);
        }
    }
};

template<>
struct TPropertyJsonSerializer<int32>
{
	static void Serialize(json::JSON& OutJson,
		const char* Key, const int32& Value)
	{
		OutJson[Key] = Value;
	}

	static void Deserialize(const json::JSON& InJson,
		const char* Key, int32& OutValue)
	{
		if (!InJson.hasKey(Key))
		{
			throw std::runtime_error("Missing int32 property");
		}

		const json::JSON& Value = InJson.at(Key);

		if (Value.JSONType() != json::JSON::Class::Integral)
		{
			throw std::runtime_error("Property requires int32");
		}

		OutValue = static_cast<int32>(Value.ToInt());
	}
};

template<>
struct TPropertyJsonSerializer<bool>
{
	static void Serialize(json::JSON& OutJson,
		const char* Key, const bool& Value)
	{
		OutJson[Key] = Value;
	}

	static void Deserialize(const json::JSON& InJson,
		const char* Key, bool& OutValue)
	{
		if (!InJson.hasKey(Key))
		{
			throw std::runtime_error("Missing bool property");
		}

		const json::JSON& Value = InJson.at(Key);

		if (Value.JSONType() != json::JSON::Class::Boolean)
		{
			throw std::runtime_error("Property requires bool");
		}

		OutValue = static_cast<bool>(Value.ToBool());
	}
};

template<>
struct TPropertyJsonSerializer<float>
{
	static void Serialize(
		json::JSON& OutJson,
		const char* Key,
		const float& Value)
	{
		OutJson[Key] = Value;
	}

	static void Deserialize(
		const json::JSON& InJson,
		const char* Key,
		float& OutValue)
	{
		if (!InJson.hasKey(Key))
		{
			throw std::runtime_error("Missing float property");
		}

		const json::JSON& Value = InJson.at(Key);

		if (Value.JSONType() != json::JSON::Class::Floating)
		{
			throw std::runtime_error("Property requires float");
		}

		OutValue = static_cast<float>(Value.ToFloat());
	}
};

template<>
struct TPropertyJsonSerializer<uint32>
{
	static void Serialize(
		json::JSON& OutJson,
		const char* Key,
		const uint32& Value)
	{
		OutJson[Key] = static_cast<int64>(Value);
	}

	static void Deserialize(
		const json::JSON& InJson,
		const char* Key,
		uint32& OutValue)
	{
		if (!InJson.hasKey(Key))
		{
			throw std::runtime_error("Missing uint32 property");
		}

		const json::JSON& Value = InJson.at(Key);

		if (Value.JSONType() != json::JSON::Class::Integral)
		{
			throw std::runtime_error("Property requires uint32");
		}

		const long LoadedValue = Value.ToInt();

		if (LoadedValue < 0)
		{
			throw std::runtime_error("Property requires non-negative uint32");
		}

		OutValue = static_cast<uint32>(LoadedValue);
	}
};

template<>
struct TPropertyJsonSerializer<FString>
{
	static void Serialize(
		json::JSON& OutJson,
		const char* Key,
		const FString& Value)
	{
		OutJson[Key] = Value.CStr();
	}

	static void Deserialize(
		const json::JSON& InJson,
		const char* Key,
		FString& OutValue)
	{
		if (!InJson.hasKey(Key))
		{
			throw std::runtime_error("Missing FString property");
		}

		const json::JSON& Value = InJson.at(Key);

		if (Value.JSONType() != json::JSON::Class::String)
		{
			throw std::runtime_error("Property requires string");
		}

		OutValue = FString(Value.ToString());
	}
};

template<>
struct TPropertyJsonSerializer<FVector>
{
	static void Serialize(
		json::JSON& OutJson,
		const char* Key,
		const FVector& Value)
	{
		OutJson[Key] = FVectorToJson(Value);
	}

	static void Deserialize(
		const json::JSON& InJson,
		const char* Key,
		FVector& OutValue)
	{
		if (!InJson.hasKey(Key))
		{
			throw std::runtime_error("Missing FVector property");
		}

		const json::JSON& Value = InJson.at(Key);

		if (Value.JSONType() != json::JSON::Class::Array)
		{
			throw std::runtime_error("Property requires FVector");
		}

		OutValue = FVectorFromJson(Value);
	}
};

template<>
struct TPropertyJsonSerializer<FRotator>
{
	static void Serialize(
		json::JSON& OutJson,
		const char* Key,
		const FRotator& Value)
	{
		OutJson[Key] = FRotatorToJson(Value);
	}

	static void Deserialize(
		const json::JSON& InJson,
		const char* Key,
		FRotator& OutValue)
	{
		if (!InJson.hasKey(Key))
		{
			throw std::runtime_error("Missing FRotator property");
		}

		const json::JSON& Value = InJson.at(Key);

		if (Value.JSONType() != json::JSON::Class::Array)
		{
			throw std::runtime_error("Property requires FRotator");
		}

		OutValue = FRotatorFromJson(Value);
	}
};

template<>
struct TPropertyJsonSerializer<EPrimitive>
{
	static void Serialize(
		json::JSON& OutJson,
		const char* Key,
		const EPrimitive& Value)
	{
		OutJson[Key] = EPrimitiveToJson(Value);
	}

	static void Deserialize(
		const json::JSON& InJson,
		const char* Key,
		EPrimitive& OutValue)
	{
		if (!InJson.hasKey(Key))
		{
			throw std::runtime_error("Missing EPrimitive property");
		}

		const json::JSON& Value = InJson.at(Key);

		if (Value.JSONType() != json::JSON::Class::String)
		{
			throw std::runtime_error("Property requires EPrimitive");
		}

		OutValue = EPrimitiveFromJson(Value);
	}
};

template<>
struct TPropertyJsonSerializer<FName>
{
	static void Serialize(
		json::JSON& OutJson,
		const char* Key,
		const FName& Value)
	{
		OutJson[Key] = Value.ToString();
	}

	static void Deserialize(
		const json::JSON& InJson,
		const char* Key,
		FName& OutValue)
	{
		if (!InJson.hasKey(Key))
		{
			throw std::runtime_error("Missing FName property");
		}

		const json::JSON& Value = InJson.at(Key);

		if (Value.JSONType() != json::JSON::Class::String)
		{
			throw std::runtime_error("Property requires FName");
		}

		const FString LoadedName(Value.ToString());
		OutValue = FName(LoadedName);
	}
};

template<>
struct TPropertyJsonSerializer<FLinearColor>
{
	static void Serialize(
		json::JSON& OutJson,
		const char* Key,
		const FLinearColor& Value)
	{
		json::JSON colorJson = json::JSON::Make(json::JSON::Class::Array);
		colorJson[0] = Value.R;
		colorJson[1] = Value.G;
		colorJson[2] = Value.B;
		colorJson[3] = Value.A;

		OutJson[Key] = std::move(colorJson);
	}

	static void Deserialize(
		const json::JSON& InJson,
		const char* Key,
		FLinearColor& OutValue)
	{
		if (!InJson.hasKey(Key))
		{
			throw std::runtime_error("Missing FLinearColor property");
		}

		const json::JSON& Value = InJson.at(Key);
		if (Value.JSONType() != json::JSON::Class::Array
			|| Value.length() != 4)
		{
			throw std::runtime_error("Property requires [R, G, B, A]");
		}

		OutValue = FLinearColor(
			static_cast<float>(Value.at(0).ToFloat()),
			static_cast<float>(Value.at(1).ToFloat()),
			static_cast<float>(Value.at(2).ToFloat()),
			static_cast<float>(Value.at(3).ToFloat()));
	}
};
