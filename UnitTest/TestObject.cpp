#include "pch.h"

#include "ThirdParty/Json/json.hpp"

#include "Core/Object/Object.h"

namespace UObjectTest
{

	class UMockObject : public UObject
	{
		DECLARE_OBJECT(UMockObject, UObject);
	};

	class UMockObjectChildA : public UMockObject
	{
		DECLARE_OBJECT(UMockObjectChildA, UMockObject);
	};

	class UMockObjectChildB : public UMockObject
	{
		DECLARE_OBJECT(UMockObjectChildB, UMockObject);
	};

	class UMockObjectParameter : public UObject
	{
		DECLARE_OBJECT(UMockObjectParameter, UObject);

	public:
		int32 Param1;
		int32 Param2;

		void Initialize(int32 param1, int32 param2)
		{
			Param1 = param1;
			Param2 = param2;
		}

		virtual void SerializeClass(json::JSON& outJson) const override
		{
			UObject::SerializeClass(outJson);
			outJson["Properties"]["Param1"] = Param1;
			outJson["Properties"]["Param2"] = Param2;
		}

		virtual void DeserializeClass(const json::JSON& inJson) override
		{
			UObject::DeserializeClass(inJson);
			Param1 = inJson.at("Properties").at("Param1").ToInt();
			Param2 = inJson.at("Properties").at("Param2").ToInt();
		}
	};

	IMPLEMENT_CLASS(UMockObject, UObject);
	IMPLEMENT_CLASS(UMockObjectChildA, UMockObject);
	IMPLEMENT_CLASS(UMockObjectChildB, UMockObject);
	IMPLEMENT_CLASS(UMockObjectParameter, UObject);

	TEST(TestUObject, UObject_WhenCreatingInstance_ReturnsCorrectType)
	{
		UObject* obj = FObjectFactory::ConstructUnInitializedObject(UMockObject::GetClass());
		const FClassInfo* classInfo = obj->GetRuntimeClass();

		EXPECT_EQ(classInfo->Name, "UMockObject");
		EXPECT_EQ(classInfo->SuperClass->Name, "UObject");

		delete obj;
	}

	TEST(TestUObject, GetClass_WhenFunctionCalled_ReturnsCorrectClassInfo)
	{
		const FClassInfo* uMockObjectInfo = UMockObject::GetClass();
		const FClassInfo* uObjectInfo = UObject::GetClass();

		EXPECT_EQ(uMockObjectInfo->Name, "UMockObject");
		EXPECT_EQ(uMockObjectInfo->SuperClass, uObjectInfo);
		EXPECT_EQ(uObjectInfo->Name, "UObject");
		EXPECT_EQ(uObjectInfo->SuperClass, nullptr);
	}

	TEST(TestUObject, IsA_WhenInputIsItself_ReturnsTrue)
	{
		UObject* obj = FObjectFactory::ConstructUnInitializedObject(UMockObject::GetClass());
		UObject* objChildA = FObjectFactory::ConstructUnInitializedObject(UMockObjectChildA::GetClass());
		UObject* objChildB = FObjectFactory::ConstructUnInitializedObject(UMockObjectChildB::GetClass());

		EXPECT_TRUE(obj->IsA(UMockObject::GetClass()));
		EXPECT_TRUE(objChildA->IsA(UMockObjectChildA::GetClass()));
		EXPECT_TRUE(objChildB->IsA(UMockObjectChildB::GetClass()));

		delete obj;
		delete objChildA;
		delete objChildB;
	}

	TEST(TestUObject, IsA_WhenInputIsParentClass_ReturnsTrue)
	{
		UObject* objChildA = FObjectFactory::ConstructUnInitializedObject(UMockObjectChildA::GetClass());
		UObject* objChildB = FObjectFactory::ConstructUnInitializedObject(UMockObjectChildB::GetClass());

		EXPECT_TRUE(objChildA->IsA(UMockObject::GetClass()));
		EXPECT_TRUE(objChildB->IsA(UMockObject::GetClass()));
		EXPECT_TRUE(objChildA->IsA(UObject::GetClass()));
		EXPECT_TRUE(objChildB->IsA(UObject::GetClass()));

		delete objChildA;
		delete objChildB;
	}

	TEST(TestUObject, IsA_WhenInputIsUnrelatedClass_ReturnsFalse)
	{
		UObject* obj = FObjectFactory::ConstructUnInitializedObject(UMockObject::GetClass());
		UObject* objChildA = FObjectFactory::ConstructUnInitializedObject(UMockObjectChildA::GetClass());
		UObject* objChildB = FObjectFactory::ConstructUnInitializedObject(UMockObjectChildB::GetClass());

		EXPECT_FALSE(obj->IsA(UMockObjectChildA::GetClass()));
		EXPECT_FALSE(obj->IsA(UMockObjectChildB::GetClass()));
		EXPECT_FALSE(objChildA->IsA(UMockObjectChildB::GetClass()));
		EXPECT_FALSE(objChildB->IsA(UMockObjectChildA::GetClass()));

		delete obj;
		delete objChildA;
		delete objChildB;
	}

	TEST(TestUObject, SerializeClass_WhenFunctionCalled_OutCorrectJson)
	{
		UObject* obj = FObjectFactory::ConstructObject<UMockObjectParameter>(1, 2);

		json::JSON json = json::JSON::Make(json::JSON::Class::Object);
		obj->SerializeClass(json);

		EXPECT_EQ(json["ClassName"].ToString(), "UMockObjectParameter");
		EXPECT_EQ(json["Properties"]["Param1"].ToInt(), 1);
		EXPECT_EQ(json["Properties"]["Param2"].ToInt(), 2);

		delete obj;
	}

	TEST(TestUObject, DeseralizeClass_WhenGivenJson_DeserializeCorrectParams)
	{
		UMockObjectParameter* obj = FObjectFactory::ConstructObject<UMockObjectParameter>(1, 2);
		EXPECT_EQ(obj->Param1, 1);
		EXPECT_EQ(obj->Param2, 2);

		json::JSON json = json::JSON::Make(json::JSON::Class::Object);
		json["ClassName"] = "UMockObjectParameter";
		json["Properties"]["UUID"] = 1234;
		json["Properties"]["Param1"] = 42;
		json["Properties"]["Param2"] = 84;

		obj->DeserializeClass(json);

		EXPECT_EQ(obj->UUID, 1234);
		EXPECT_EQ(obj->Param1, 42);
		EXPECT_EQ(obj->Param2, 84);

		delete obj;
	}

	TEST(TestFObjectFactory, ConstructObject_WhenConstructing_ReturnsCorrectType)
	{
		UObject* obj = FObjectFactory::ConstructUnInitializedObject(UMockObject::GetClass());
		EXPECT_EQ(obj->GetRuntimeClass()->Name, UMockObject::GetClass()->Name);
		delete obj;
	}

	TEST(TestFObjectFactory, ConstructObject_WhenConstructingT_ReturnsCorrectType)
	{
		UMockObject* obj = FObjectFactory::ConstructObject<UMockObject>();
		EXPECT_EQ(obj->GetRuntimeClass(), UMockObject::GetClass());
		delete obj;
	}

	TEST(TestFObjectFactory, ConstructObject_WhenConstructingTWithParameters_ReturnsCorrectTypeAndParameters)
	{
		int32 param1 = 42;
		int32 param2 = 84;
		UMockObjectParameter* obj = FObjectFactory::ConstructObject<UMockObjectParameter>(param1, param2);

		EXPECT_EQ(obj->GetRuntimeClass(), UMockObjectParameter::GetClass());
		EXPECT_EQ(obj->Param1, param1);
		EXPECT_EQ(obj->Param2, param2);

		delete obj;
	}

	TEST(TestFObjectFactory, LoadObject_WhenLoad_ReturnsCorrectObject)
	{
		json::JSON json = json::JSON::Make(json::JSON::Class::Object);
		json["ClassName"] = "UMockObjectParameter";
		json["Properties"]["UUID"] = 1234;
		json["Properties"]["Param1"] = 42;
		json["Properties"]["Param2"] = 84;

		UObject* obj = FObjectFactory::LoadObject(UMockObjectParameter::GetClass(), json);
		EXPECT_EQ(obj->GetRuntimeClass(), UMockObjectParameter::GetClass());
		UMockObjectParameter* mockObj = dynamic_cast<UMockObjectParameter*>(obj);
		EXPECT_NE(mockObj, nullptr);
		EXPECT_EQ(mockObj->UUID, 1234);
		EXPECT_EQ(mockObj->Param1, 42);
		EXPECT_EQ(mockObj->Param2, 84);

		delete obj;
	}

	TEST(TestFObjectFactory, LoadObjectT_WhenLoad_ReturnsCorrectObject)
	{
		json::JSON json = json::JSON::Make(json::JSON::Class::Object);
		json["ClassName"] = "UMockObjectParameter";
		json["Properties"]["UUID"] = 1234;
		json["Properties"]["Param1"] = 42;
		json["Properties"]["Param2"] = 84;

		UMockObjectParameter* obj = FObjectFactory::LoadObject<UMockObjectParameter>(json);

		EXPECT_EQ(obj->GetRuntimeClass(), UMockObjectParameter::GetClass());
		EXPECT_EQ(obj->UUID, 1234);
		EXPECT_EQ(obj->Param1, 42);
		EXPECT_EQ(obj->Param2, 84);

		delete obj;
	}

}
