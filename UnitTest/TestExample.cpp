#include "pch.h"

#include "Test11.h"

int Add(int a, int b)
{
	return a + b;
}

float Divide(float a, float b)
{
	if (b == 0) {
		throw std::invalid_argument("Division by zero");
	}
	return a / b;
}

TEST(TestCaseName, TestName) {
	EXPECT_EQ(1, 1);
	EXPECT_TRUE(true);
}

TEST(TestAdd, WhenTwoInputsAreGiven_ReturnTheirSum)
{
	EXPECT_EQ(Add(2, 3), 5);
	EXPECT_EQ(Add(1, 2), 3);
}

TEST(TestDivide, WhenValidInputsAreGiven_ReturnCorrectResult)
{
	EXPECT_FLOAT_EQ(Divide(6.0f, 2.0f), 3.0f);
	EXPECT_FLOAT_EQ(Divide(5.0f, 2.0f), 2.5f);
}

TEST(TestDivide, WhenDividingByZero_ThrowsInvalidArgument)
{
	EXPECT_THROW(Divide(5.0f, 0.0f), std::invalid_argument);
}

TEST(TestReturnTrue, WhenCalled_ReturnsTrue)
{
	EXPECT_TRUE(ReturnTrue());
}
