#include "pch.h"

#include "Engine/Serialization/SceneData.h"
#include "ThirdParty/Json/json.hpp"
#include "Core/IO/JsonUtil.h"

namespace FSceneDataTest
{
	TEST(TestFSceneData, WhenGivenJson_ConstructCorrectData)
	{
		json::JSON json;
		json["Version"] = 1;
		json["NextUUID"] = 42;
		json::JSON primitivesJson;
		primitivesJson["1"] = json::JSON();
		primitivesJson["1"]["Location"] = FVectorToJson(FVector(1.f, 2.f, 3.f));
		primitivesJson["1"]["Rotation"] = FRotatorToJson(FRotator(10.f, 20.f, 30.f));
		primitivesJson["1"]["Scale"] = FVectorToJson(FVector(0.5f, 0.5f, 0.5f));
		primitivesJson["1"]["PrimitiveType"] = EPrimitiveToJson(EPrimitive::EP_Cube);
		json["Primitives"] = primitivesJson;

		FSceneData sceneData(json);

		EXPECT_EQ(sceneData.Version, 1);
		EXPECT_EQ(sceneData.NextUUID, 42);
		EXPECT_EQ(sceneData.Primitives.Num(), 1);

		const FPrimitiveData& primitiveData = sceneData.Primitives[1];
		EXPECT_EQ(primitiveData.Location.x, 1.f);
		EXPECT_EQ(primitiveData.Location.y, 2.f);
		EXPECT_EQ(primitiveData.Location.z, 3.f);
		EXPECT_EQ(primitiveData.Rotation.Pitch, 10.f);
		EXPECT_EQ(primitiveData.Rotation.Yaw, 20.f);
		EXPECT_EQ(primitiveData.Rotation.Roll, 30.f);
		EXPECT_EQ(primitiveData.Scale.x, 0.5f);
		EXPECT_EQ(primitiveData.Scale.y, 0.5f);
		EXPECT_EQ(primitiveData.Scale.z, 0.5f);
		EXPECT_EQ(primitiveData.PrimitiveType, EPrimitive::EP_Cube);
	}

	TEST(TestFSceneData, WhenGivenString_ConstructCorrectData)
	{
		std::string jsonString = R"({
			"Version": 1,
			"NextUUID": 42,
			"Primitives": {
				"1": {
					"Location": [1.0, 2.0, 3.0],
					"Rotation": [10.0, 20.0, 30.0],
					"Scale": [0.5, 0.5, 0.5],
					"PrimitiveType": "Cube"
				}
			}
		})";
		json::JSON json = json::JSON::Load(jsonString);
		FSceneData sceneData(json);
		EXPECT_EQ(sceneData.Version, 1);
		EXPECT_EQ(sceneData.NextUUID, 42);
		EXPECT_EQ(sceneData.Primitives.Num(), 1);
		const FPrimitiveData& primitiveData = sceneData.Primitives[1];
		EXPECT_EQ(primitiveData.Location.x, 1.f);
		EXPECT_EQ(primitiveData.Location.y, 2.f);
		EXPECT_EQ(primitiveData.Location.z, 3.f);
		EXPECT_EQ(primitiveData.Rotation.Pitch, 10.f);
		EXPECT_EQ(primitiveData.Rotation.Yaw, 20.f);
		EXPECT_EQ(primitiveData.Rotation.Roll, 30.f);
		EXPECT_EQ(primitiveData.Scale.x, 0.5f);
		EXPECT_EQ(primitiveData.Scale.y, 0.5f);
		EXPECT_EQ(primitiveData.Scale.z, 0.5f);
		EXPECT_EQ(primitiveData.PrimitiveType, EPrimitive::EP_Cube);
	}

	TEST(TestToJson, WhenFunctionCalled_ReturnCorrectJson)
	{
		FSceneData sceneData;
		sceneData.Version = 1;
		sceneData.NextUUID = 42;

		FPrimitiveData primitiveData1;
		FPrimitiveData primitiveData2;

		primitiveData1.Location = FVector(1.f, 2.f, 3.f);
		primitiveData1.Rotation = FRotator(10.f, 20.f, 30.f);
		primitiveData1.Scale = FVector(0.5f, 0.5f, 0.5f);
		primitiveData1.PrimitiveType = EPrimitive::EP_Cube;

		primitiveData2.Location = FVector(4.f, 5.f, 6.f);
		primitiveData2.Rotation = FRotator(40.f, 50.f, 60.f);
		primitiveData2.Scale = FVector(1.f, 1.f, 1.f);
		primitiveData2.PrimitiveType = EPrimitive::EP_Sphere;

		sceneData.Primitives.Add(1, primitiveData1);
		sceneData.Primitives.Add(3, primitiveData2);

		json::JSON json = sceneData.ToJson();
		EXPECT_EQ(json["Version"].ToInt(), 1);
		EXPECT_EQ(json["NextUUID"].ToInt(), 42);
		EXPECT_EQ(json["Primitives"]["1"]["Location"][0].ToFloat(), 1.f);
		EXPECT_EQ(json["Primitives"]["1"]["Location"][1].ToFloat(), 2.f);
		EXPECT_EQ(json["Primitives"]["1"]["Location"][2].ToFloat(), 3.f);
		EXPECT_EQ(json["Primitives"]["1"]["Rotation"][0].ToFloat(), 10.f);
		EXPECT_EQ(json["Primitives"]["1"]["Rotation"][1].ToFloat(), 20.f);
		EXPECT_EQ(json["Primitives"]["1"]["Rotation"][2].ToFloat(), 30.f);
		EXPECT_EQ(json["Primitives"]["1"]["Scale"][0].ToFloat(), 0.5f);
		EXPECT_EQ(json["Primitives"]["1"]["Scale"][1].ToFloat(), 0.5f);
		EXPECT_EQ(json["Primitives"]["1"]["Scale"][2].ToFloat(), 0.5f);
		EXPECT_EQ(json["Primitives"]["1"]["PrimitiveType"].ToString(), "Cube");
		EXPECT_EQ(json["Primitives"]["3"]["Location"][0].ToFloat(), 4.f);
		EXPECT_EQ(json["Primitives"]["3"]["Location"][1].ToFloat(), 5.f);
		EXPECT_EQ(json["Primitives"]["3"]["Location"][2].ToFloat(), 6.f);
		EXPECT_EQ(json["Primitives"]["3"]["Rotation"][0].ToFloat(), 40.f);
		EXPECT_EQ(json["Primitives"]["3"]["Rotation"][1].ToFloat(), 50.f);
		EXPECT_EQ(json["Primitives"]["3"]["Rotation"][2].ToFloat(), 60.f);
		EXPECT_EQ(json["Primitives"]["3"]["Scale"][0].ToFloat(), 1.f);
		EXPECT_EQ(json["Primitives"]["3"]["Scale"][1].ToFloat(), 1.f);
		EXPECT_EQ(json["Primitives"]["3"]["Scale"][2].ToFloat(), 1.f);
		EXPECT_EQ(json["Primitives"]["3"]["PrimitiveType"].ToString(), "Sphere");
	}
}
