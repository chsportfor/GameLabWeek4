#include "JsonUtil.h"

#include "ThirdParty/Json/json.hpp"

json::JSON FVectorToJson(const FVector& Vector)
{
	json::JSON vectorJson = json::JSON::Make(json::JSON::Class::Array);
	vectorJson[0] = Vector.x;
	vectorJson[1] = Vector.y;
	vectorJson[2] = Vector.z;
	return vectorJson;
}

json::JSON FRotatorToJson(const FRotator& Rotator)
{
	json::JSON rotatorJson = json::JSON::Make(json::JSON::Class::Array);
	rotatorJson[0] = Rotator.Pitch;
	rotatorJson[1] = Rotator.Yaw;
	rotatorJson[2] = Rotator.Roll;
	return rotatorJson;
}

json::JSON FloatToJson(const float& value)
{
	json::JSON floatJson = json::JSON::Make(json::JSON::Class::Floating);
	floatJson = value;
	return floatJson;
}

FVector FVectorFromJson(const json::JSON& json)
{
	if (json.JSONType() != json::JSON::Class::Array)
	{
		throw std::runtime_error("Json Array expected for FVector");
	}

	return FVector(json.at(0).ToFloat(), json.at(1).ToFloat(), json.at(2).ToFloat());
}

FRotator FRotatorFromJson(const json::JSON& json)
{
	if (json.JSONType() != json::JSON::Class::Array)
	{
		throw std::runtime_error("Json Array expected for FRotator");
	}

	return FRotator(json.at(0).ToFloat(), json.at(1).ToFloat(), json.at(2).ToFloat());
}

float FloatFromJson(const json::JSON& json)
{
	if (json.JSONType() != json::JSON::Class::Floating)
	{
		throw std::runtime_error("Json Number expected for float");
	}

	return json.ToFloat();
}
