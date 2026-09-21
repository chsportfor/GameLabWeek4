#include "SceneData.h"

#include "ThirdParty/Json/json.hpp"

#include "Core/IO/JsonUtil.h"

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
