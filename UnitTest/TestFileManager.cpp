#include "pch.h"

#include <fstream>

#include "Core/IO/FileManager.h"

namespace FFileManagerTest
{
	class TestFFileManager : public testing::Test
	{
	protected:
		static void SetUpTestCase()
		{
			auto cwd = std::filesystem::current_path();
			std::cout << "CWD: " << cwd << std::endl;

			// Create test assets directory if it doesn't exist
			std::filesystem::create_directory(".\\TestAssets\\");
			ASSERT_TRUE(std::filesystem::exists(".\\TestAssets\\")) << "Failed to create test assets directory.";

			// Create a test file with known content
			std::ofstream testFile(".\\TestAssets\\TestFile.txt");
			ASSERT_TRUE(testFile.is_open()) << "Failed to create test file.";
			testFile << "This is a test file.\n";
			testFile.close();
		}

		static void TearDownTestCase()
		{
			// Clean up test assets directory and files
			std::filesystem::remove_all(".\\TestAssets\\");
		}

		//void SetUp() override
		//{
		//	throw std::runtime_error("TestFFileManager::SetUp() is not implemented yet.");
		//}
	};

	TEST_F(TestFFileManager, ReadFileToString_WhenReadingFile_ReturnsCorrectContent)
	{
		auto cwd = std::filesystem::current_path();
		std::cout << "CWD: " << cwd << std::endl;

		FFileManager fileManager(".\\TestAssets\\", ".\\");
		FString result = fileManager.ReadFileToString("TestFile.txt");
		EXPECT_EQ(result, "This is a test file.\n");
	}

	TEST_F(TestFFileManager, WriteStringToFile_WhenWritingFile_WritesCorrectContent)
	{
		FFileManager fileManager(".\\TestAssets\\", ".\\");
		FString contentToWrite = "This is a test write.";
		fileManager.WriteStringToFile("TestWrite.txt", contentToWrite);
		FString readContent = fileManager.ReadFileToString("TestWrite.txt");

		EXPECT_EQ(readContent, contentToWrite);
	}
	TEST_F(TestFFileManager, ReadFileToString_WhenReadingNonExistentFile_ThrowsRuntimeError)
	{
		FFileManager fileManager(".\\TestAssets\\", ".\\");
		EXPECT_THROW(fileManager.ReadFileToString("NonExistent.txt"), std::runtime_error);
	}
	TEST_F(TestFFileManager, WriteStringToFile_WhenWritingOutsideRoot_ThrowsRuntimeError)
	{
		FFileManager fileManager(".\\TestAssets\\", ".\\");
		FString contentToWrite = "This should not be written.";
		EXPECT_THROW(fileManager.WriteStringToFile("..\\OutsideRoot.txt", contentToWrite), std::runtime_error);
	}

	TEST_F(TestFFileManager, IsUnder_WhenGivenPathIsUnderRoot_ReturnsTrue)
	{
		std::filesystem::path rootPath = ".\\TestAssets\\";
		std::filesystem::path filePath = ".\\TestAssets\\SubDir\\File.txt";
		EXPECT_TRUE(IsUnder(filePath, rootPath));
	}

	TEST_F(TestFFileManager, IsUnder_WhenGivenPathIsNotUnderRoot_ReturnsFalse)
	{
		std::filesystem::path rootPath = ".\\TestAssets\\";
		std::filesystem::path filePath = ".\\OtherDir\\File.txt";
		EXPECT_FALSE(IsUnder(filePath, rootPath));
	}
}
