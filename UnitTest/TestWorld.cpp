#include "pch.h"

#include "Engine/Actor.h"
#include "Core/Core.h"
#include "Core/IO/JsonUtil.h"
#include "Core/Object/ObjectFactory.h"
#include "Engine/World.h"

namespace UWorldTest
{
	static uint32 gUpdateCounter;

	class AMockActor : public AActor
	{
		DECLARE_OBJECT(AMockActor, AActor)

		virtual void Update(TArray<FRenderInfo>* outRenderInfos) override
		{
			++gUpdateCounter;
		}
	};
	IMPLEMENT_CLASS(AMockActor, AActor)

	class TestUWorld : public testing::Test
	{
	protected:
		virtual void SetUp() override
		{
			gUpdateCounter = 0;
		}

		static void SetUpTestCase()
		{
			FObjectFactory::RegisterClassInfo("AMockActor", AMockActor::GetClass());
		}
	};


	TEST_F(TestUWorld, AddActor_WhenGivenValidActor_AddsActorToWorld)
	{
		EXPECT_EQ(gUpdateCounter, 0);

		UWorld* world = FObjectFactory::ConstructObject<UWorld>();
		EXPECT_EQ(world->GetClass(), UWorld::GetClass());

		AMockActor* mockActor = FObjectFactory::ConstructObject<AMockActor>();
		EXPECT_EQ(mockActor->GetClass(), AMockActor::GetClass());

		world->AddActor(mockActor);
		world->Update();
		world->Update();

		EXPECT_EQ(gUpdateCounter, 2);

		delete world;
	}

	TEST_F(TestUWorld, AddActor_WhenGivenInvalidActor_Assert)
	{
		UWorld* world = FObjectFactory::ConstructObject<UWorld>();

		EXPECT_EQ(world->GetClass(), UWorld::GetClass());
		EXPECT_DEATH(world->AddActor(nullptr), ".*");

		delete world;
	}

	TEST_F(TestUWorld, RemoveActor_WhenGivenValidActorUUID_RemovesActorFromWorld)
	{
		UWorld* world = FObjectFactory::ConstructObject<UWorld>();
		EXPECT_EQ(world->GetClass(), UWorld::GetClass());

		AMockActor* mockActor = FObjectFactory::ConstructObject<AMockActor>();
		EXPECT_EQ(mockActor->GetClass(), AMockActor::GetClass());

		world->AddActor(mockActor);
		EXPECT_TRUE(world->RemoveActor(mockActor->UUID));

		delete world;
	}

	TEST_F(TestUWorld, SerializeClass_WhenFunctionCalled_OutCorrectJson)
	{
		UWorld* world = FObjectFactory::ConstructObject<UWorld>();
		EXPECT_EQ(world->GetClass(), UWorld::GetClass());

		AMockActor* mockActor = FObjectFactory::ConstructObject<AMockActor>();
		EXPECT_EQ(mockActor->GetClass(), AMockActor::GetClass());

		world->AddActor(mockActor);
		json::JSON json = json::JSON::Make(json::JSON::Class::Object);
		world->SerializeClass(json);

		EXPECT_EQ(json["ClassName"].ToString(), "UWorld");
		EXPECT_EQ(json["Properties"]["mActors"].length(), 1);
		EXPECT_EQ(json["Properties"]["mActors"][0]["ClassName"].ToString(), "AMockActor");

		delete world;
	}

	TEST_F(TestUWorld, DeserializeClass_WhenFunctionCalled_InCorrectJson)
	{
		std::string jsonString = R"(
		{
			"ClassName": "UWorld",
			"Properties": {
				"UUID": 42,
				"mActors": [
					{
						"ClassName": "AMockActor",
						"Properties": {
							"mRootComponentUUID": -1,
							"UUID": 123,
							"mComponents": []
						}
					},
					{
						"ClassName": "AMockActor",
						"Properties": {
							"mRootComponentUUID": -1,
							"UUID": 456,
							"mComponents": []
						}
					}	
				]
			}
		})";

		json::JSON jsonObject = json::JSON::Load(jsonString);

		UWorld* world = FObjectFactory::LoadObject<UWorld>(jsonObject);

		EXPECT_EQ(world->GetRuntimeClass(), UWorld::GetClass());
		EXPECT_EQ(world->UUID, 42);

		world->Update();
		world->Update();

		EXPECT_EQ(gUpdateCounter, 4);

		EXPECT_TRUE(world->RemoveActor(123));
		EXPECT_TRUE(world->RemoveActor(456));

		delete world;
	}
}
