
#include "pch.h"
#include "Core/Container/TArray.h"

namespace TArrayTest
{
	class TempObject
	{
	public:
		TempObject(int data)
			: Data(data)
		{
		}

	public:
		int Data;
	};

	TEST(TestRemove, TestRemoveAtSwap)
	{
		{
			TArray<int> intArray;
			intArray.Add(1);
			intArray.Add(2);
			intArray.Add(3);
			intArray.Add(4);
			intArray.Add(5);

			int removeIndex = 2;
			EXPECT_EQ(intArray[removeIndex], 3);

			intArray.RemoveAtSwap(removeIndex);
			EXPECT_EQ(intArray[removeIndex], 5);

			intArray.RemoveAtSwap(removeIndex);
			EXPECT_EQ(intArray[removeIndex], 4);

			removeIndex = 0;
			EXPECT_EQ(intArray[removeIndex], 1);

			intArray.RemoveAtSwap(removeIndex);
			EXPECT_EQ(intArray[removeIndex], 4);

			intArray.RemoveAtSwap(removeIndex);
			EXPECT_EQ(intArray[removeIndex], 2);

			intArray.RemoveAtSwap(removeIndex);
			EXPECT_EQ(intArray.IsEmpty(), true);
		}

		{
			TArray<TempObject*> objectArray;

			TempObject* object = new TempObject(1);
			objectArray.Add(object);

			object = new TempObject(2);
			objectArray.Add(object);

			object = new TempObject(3);
			objectArray.Add(object);

			object = new TempObject(4);
			objectArray.Add(object);

			object = new TempObject(5);
			objectArray.Add(object);

			int removeIndex = 2;
			EXPECT_EQ(objectArray[removeIndex]->Data, 3);

			objectArray.RemoveAtSwap(removeIndex);
			EXPECT_EQ(objectArray[removeIndex]->Data, 5);

			objectArray.RemoveAtSwap(removeIndex);
			EXPECT_EQ(objectArray[removeIndex]->Data, 4);

			removeIndex = 0;
			EXPECT_EQ(objectArray[removeIndex]->Data, 1);

			objectArray.RemoveAtSwap(removeIndex);
			EXPECT_EQ(objectArray[removeIndex]->Data, 4);

			objectArray.RemoveAtSwap(removeIndex);
			EXPECT_EQ(objectArray[removeIndex]->Data, 2);

			objectArray.RemoveAtSwap(removeIndex);
			EXPECT_EQ(objectArray.IsEmpty(), true);


			for (TempObject* delObject : objectArray)
			{
				delete delObject;
			}
		}



	}


}
