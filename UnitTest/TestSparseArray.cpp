#include "pch.h"

#include "Core/Container/TSparseArray.h"

namespace TSparseArrayTest
{
	TEST(TestSparseArray, Add_WhenAddAndAccessElements_ReturnsCorrectValues)
	{
		TSparseArray<int> sparseArray;
		// Add elements to the sparse array
		uint32 index1 = sparseArray.Add(10);
		uint32 index2 = sparseArray.Add(20);
		uint32 index3 = sparseArray.Add(30);
		// Access elements using the indices
		EXPECT_EQ(sparseArray[index1], 10);
		EXPECT_EQ(sparseArray[index2], 20);
		EXPECT_EQ(sparseArray[index3], 30);
	}

	TEST(TestSparseArray, RemoveAt_WhenRemoveElements_ReturnsCorrectValues)
	{
		TSparseArray<int> sparseArray;
		// Add elements to the sparse array
		uint32 index1 = sparseArray.Add(10);
		uint32 index2 = sparseArray.Add(20);
		uint32 index3 = sparseArray.Add(30);
		// Remove an element
		sparseArray.RemoveAt(index2);
		// Access elements using the indices
		EXPECT_EQ(sparseArray[index1], 10);
		EXPECT_EQ(sparseArray[index3], 30);
	}

	TEST(TestSparseArray, RemoveAt_WhenRemoveAndAdd_PutElementsInFreedIndex)
	{
		TSparseArray<int> sparseArray;

		uint32 index1 = sparseArray.Add(10);
		uint32 index2 = sparseArray.Add(20);
		uint32 index3 = sparseArray.Add(30);
		uint32 index4 = sparseArray.Add(40);
		uint32 index5 = sparseArray.Add(50);

		EXPECT_EQ(index1, 0);
		EXPECT_EQ(index2, 1);
		EXPECT_EQ(index3, 2);
		EXPECT_EQ(index4, 3);
		EXPECT_EQ(index5, 4);

		sparseArray.RemoveAt(index2);
		sparseArray.RemoveAt(index4);

		uint32 index6 = sparseArray.Add(60);
		uint32 index7 = sparseArray.Add(70);

		EXPECT_EQ(index6, index4);
		EXPECT_EQ(index7, index2);
	}

	TEST(TestSparseArray, Iterator_WhenIteratingFreedArray_ReturnsOnlyOccupiedValues)
	{
		TSparseArray<int> sparseArray;
		uint32 index1 = sparseArray.Add(10);
		uint32 index2 = sparseArray.Add(20);
		uint32 index3 = sparseArray.Add(30);
		uint32 index4 = sparseArray.Add(40);
		uint32 index5 = sparseArray.Add(50);
		sparseArray.RemoveAt(index2);
		sparseArray.RemoveAt(index4);

		std::vector<int> expectedValues = { 10, 30, 50 };
		std::vector<int> actualValues;
		for (auto it = sparseArray.begin(); it != sparseArray.end(); ++it)
		{
			actualValues.push_back(*it);
		}
		EXPECT_EQ(actualValues, expectedValues);

		actualValues.clear();

		for (auto i : sparseArray)
		{
			actualValues.push_back(i);
		}
		EXPECT_EQ(actualValues, expectedValues);
	}

}
