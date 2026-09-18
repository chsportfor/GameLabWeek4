#include "SceneData.h"

#include "ThirdParty/Json/json.hpp"

#include "Core/IO/JsonUtil.h"

FPrimitiveData::FPrimitiveData()
	: Location(0.f, 0.f, 0.f)
	, Rotation(0.f, 0.f, 0.f)
	, Scale(1.f, 1.f, 1.f)
	, PrimitiveType(EPrimitive::EP_Sphere)
{
}

FPrimitiveData::FPrimitiveData(json::JSON json)
{
	if (!json.hasKey("Location") || !json.hasKey("Rotation") || !json.hasKey("Scale") || !json.hasKey("PrimitiveType"))
	{
		throw std::runtime_error("Invalid JSON format for FPrimitiveData");
		return;
	}

	Location = FVectorFromJson(json["Location"]);
	Rotation = FRotatorFromJson(json["Rotation"]);
	Scale = FVectorFromJson(json["Scale"]);
	PrimitiveType = EPrimitiveFromJson(json["PrimitiveType"]);
}

json::JSON FPrimitiveData::ToJson() const
{
	json::JSON json;
	json["Location"] = FVectorToJson(Location);
	json["Rotation"] = FRotatorToJson(Rotation);
	json["Scale"] = FVectorToJson(Scale);
	json["PrimitiveType"] = EPrimitiveToJson(PrimitiveType);
	return json;
}

FString FPrimitiveData::ToJsonString() const
{
	return FString(ToJson().dump());
}

FSceneData::FSceneData()
	: Version(0)
	, NextUUID(0)
{
}

FSceneData::FSceneData(json::JSON json)
{
	if (!json.hasKey("Version") || !json.hasKey("NextUUID") || !json.hasKey("Primitives"))
	{
		throw std::runtime_error("Invalid JSON format for FSceneData");
		return;
	}

	Version = json["Version"].ToInt();
	NextUUID = json["NextUUID"].ToInt();

	for (const auto& [key, value] : json["Primitives"].ObjectRange())
	{
		uint32 UUID = std::stoul(key);
		FPrimitiveData PrimitiveData(value);
		Primitives.Add(UUID, PrimitiveData);
	}
}

json::JSON FSceneData::ToJson() const
{
	json::JSON json;
	json["Version"] = Version;
	json["NextUUID"] = NextUUID;
	json::JSON primitivesJson = json::JSON::Make(json::JSON::Class::Object);
	for (const auto& [UUID, PrimitiveData] : Primitives)
	{
		primitivesJson[std::to_string(UUID)] = PrimitiveData.ToJson();
	}
	json["Primitives"] = primitivesJson;
	return json;
}

FString FSceneData::ToJsonString() const
{
	return FString(ToJson().dump());
}

//

FCameraData::FCameraData()
	: Location(0.f, 0.f, 0.f)
	, Rotation(0.f, 0.f, 0.f)
	, FOV (60.0f)
	, NearClip (.1f)
	, FarClip (100.f)
{
}

FCameraData::FCameraData(json::JSON json)
{
	if (!json.hasKey("Location") || !json.hasKey("Rotation") || !json.hasKey("FOV") || !json.hasKey("NearClip") || !json.hasKey("FarClip"))
	{
		throw std::runtime_error("Invalid JSON format for FCameraData");
		return;
	}

	Location = FVectorFromJson(json["Location"]);
	Rotation = FRotatorFromJson(json["Rotation"]);
	FOV = FloatFromJson(json["FOV"]);
	NearClip = FloatFromJson(json["NearClip"]);
	FarClip = FloatFromJson(json["FarClip"]);
}

json::JSON FCameraData::ToJson() const
{
	json::JSON json;
	json["Location"] = FVectorToJson(Location);
	json["Rotation"] = FRotatorToJson(Rotation);
	json["FOV"] = FloatToJson(FOV);
	json["NearClip"] = FloatToJson(NearClip);
	json["FarClip"] = FloatToJson(FarClip);
	return json;
}

FString FCameraData::ToJsonString() const
{
	return FString(ToJson().dump());
}
