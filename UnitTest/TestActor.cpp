#include "pch.h"

#include "Engine/Actor.h"
#include "Engine/Components/SceneComponent.h"
#include "Core/Object/ObjectFactory.h"
#include "Core/IO/JsonUtil.h"

namespace AActorTest
{


	class AMockActor : public AActor
	{
		DECLARE_OBJECT(AMockActor, AActor)
	public:
	};

	class UMockSceneComponent : public USceneComponent
	{
		DECLARE_OBJECT(UMockSceneComponent, USceneComponent)
	public:
	};

	TEST(TestActor, WhenRemoveAddedComponent_ReturnTrue)
	{
		AMockActor* mockActor = FObjectFactory::ConstructObject<AMockActor>();
		EXPECT_EQ(mockActor->GetClass(), AMockActor::GetClass());

		UMockSceneComponent* mockSceneComponent
			= FObjectFactory::ConstructObject<UMockSceneComponent>(
				FVector(0.0f, 0.0f, 0.0f),
				FRotator(0.0f, 0.0f, 0.0f),
				FVector(1.0f, 1.0f, 1.0f)
			);

		uint32 uuid = mockSceneComponent->UUID;
		mockActor->AddComponent(mockSceneComponent);
		EXPECT_TRUE(mockActor->RemoveComponent(uuid));

		delete mockActor;
		delete mockSceneComponent;
	}

	IMPLEMENT_CLASS(AMockActor, AActor)
	IMPLEMENT_CLASS(UMockSceneComponent, USceneComponent)
	TEST(TestActor, WhenRemoveNotAddedComponent_ReturnFalse)
	{
		AMockActor* mockActor = FObjectFactory::ConstructObject<AMockActor>();
		EXPECT_EQ(mockActor->GetClass(), AMockActor::GetClass());

		UMockSceneComponent* mockSceneComponent
			= FObjectFactory::ConstructObject<UMockSceneComponent>(
				FVector(0.0f, 0.0f, 0.0f),
				FRotator(0.0f, 0.0f, 0.0f),
				FVector(1.0f, 1.0f, 1.0f)
			);

		uint32 uuid = mockSceneComponent->UUID;
		EXPECT_FALSE(mockActor->RemoveComponent(uuid));

		delete mockActor;
		delete mockSceneComponent;
	}

	TEST(TestActor, WhenRemoveRemovedComponent_ReturnFalse)
	{
		AMockActor* mockActor = FObjectFactory::ConstructObject<AMockActor>();
		EXPECT_EQ(mockActor->GetClass(), AMockActor::GetClass());

		UMockSceneComponent* mockSceneComponent
			= FObjectFactory::ConstructObject<UMockSceneComponent>(
				FVector(0.0f, 0.0f, 0.0f),
				FRotator(0.0f, 0.0f, 0.0f),
				FVector(1.0f, 1.0f, 1.0f)
			);

		uint32 uuid = mockSceneComponent->UUID;
		mockActor->AddComponent(mockSceneComponent);
		EXPECT_TRUE(mockActor->RemoveComponent(uuid));
		EXPECT_FALSE(mockActor->RemoveComponent(uuid));

		delete mockActor;
		delete mockSceneComponent;
	}

	TEST(TestActor, SerializeClass_WhenFunctionCalled_OutCorrectJson)
	{
		AMockActor* mockActor = FObjectFactory::ConstructObject<AMockActor>();
		EXPECT_EQ(mockActor->GetClass(), AMockActor::GetClass());

		UMockSceneComponent* mockSceneComponent0
			= FObjectFactory::ConstructObject<UMockSceneComponent>(
				FVector(0.0f, 0.0f, 0.0f),
				FRotator(0.0f, 0.0f, 0.0f),
				FVector(1.0f, 1.0f, 1.0f)
			);
		UMockSceneComponent* mockSceneComponent1
			= FObjectFactory::ConstructObject<UMockSceneComponent>(
				FVector(1.0f, 2.0f, 3.0f),
				FRotator(10.0f, 20.0f, 30.0f),
				FVector(1.0f, 2.0f, 3.0f)
			);
		mockActor->AddComponent(mockSceneComponent0);
		mockActor->AddComponent(mockSceneComponent1);

		json::JSON outJson = json::JSON::Make(json::JSON::Class::Object);
		mockActor->SerializeClass(outJson);
		EXPECT_EQ(outJson["ClassName"].ToString(), "AMockActor");
		EXPECT_EQ(outJson.hasKey("Properties"), true);

		json::JSON& propertiesJson = outJson["Properties"];
		EXPECT_EQ(propertiesJson["UUID"].ToInt(), mockActor->UUID);
		EXPECT_EQ(propertiesJson.hasKey("mComponents"), true);

		json::JSON& componentsJson = propertiesJson["mComponents"];
		EXPECT_EQ(componentsJson.JSONType(), json::JSON::Class::Array);
		EXPECT_EQ(componentsJson.length(), 2);

		EXPECT_EQ(componentsJson[0]["ClassName"].ToString(), "UMockSceneComponent");
		EXPECT_EQ(componentsJson[1]["ClassName"].ToString(), "UMockSceneComponent");

		json::JSON& component0PropertiesJson = componentsJson[0]["Properties"];
		json::JSON& component1PropertiesJson = componentsJson[1]["Properties"];

		EXPECT_EQ(component0PropertiesJson["UUID"].ToInt(), mockSceneComponent0->UUID);
		EXPECT_EQ(component0PropertiesJson["mRelativeLocation"][0].ToFloat(), 0.0f);
		EXPECT_EQ(component0PropertiesJson["mRelativeLocation"][1].ToFloat(), 0.0f);
		EXPECT_EQ(component0PropertiesJson["mRelativeLocation"][2].ToFloat(), 0.0f);

		EXPECT_EQ(component1PropertiesJson["UUID"].ToInt(), mockSceneComponent1->UUID);
		EXPECT_EQ(component1PropertiesJson["mRelativeLocation"][0].ToFloat(), 1.0f);
		EXPECT_EQ(component1PropertiesJson["mRelativeLocation"][1].ToFloat(), 2.0f);
		EXPECT_EQ(component1PropertiesJson["mRelativeLocation"][2].ToFloat(), 3.0f);

		delete mockActor;
	}

	TEST(TestActor, DeserializeClass_WhenFunctionCalled_InCorrectJson)
	{
		std::string jsonString = R"(
		{
			"ClassName": "AMockActor",
			"Properties": {
				"UUID": 12345,
				"mRootComponentUUID": 13579,
				"mComponents": [
					{
						"ClassName": "USceneComponent",
						"Properties": {
							"UUID": 67890,
							"mRelativeLocation": [1.0, 2.0, 3.0],
							"mRelativeRotation": [10.0, 20.0, 30.0],
							"mRelativeScale3D": [1.0, 2.0, 3.0]
						}
					},
					{
						"ClassName": "USceneComponent",
						"Properties": {
							"UUID": 13579,
							"mRelativeLocation": [4.0, 5.0, 6.0],
							"mRelativeRotation": [40.0, 50.0, 60.0],
							"mRelativeScale3D": [4.0, 5.0, 6.0]
						}
					}
				]
			}
		})";

		json::JSON jsonObject = json::JSON::Load(jsonString);

		AMockActor* mockActor = FObjectFactory::LoadObject<AMockActor>(jsonObject);

		EXPECT_EQ(mockActor->GetRuntimeClass(), AMockActor::GetClass());
		EXPECT_EQ(mockActor->UUID, 12345);
		EXPECT_TRUE(mockActor->RemoveComponent(67890));
		EXPECT_TRUE(mockActor->RemoveComponent(13579));
		EXPECT_FALSE(mockActor->RemoveComponent(1234));

		delete mockActor;
	}
}
