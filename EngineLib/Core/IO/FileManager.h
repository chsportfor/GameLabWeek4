#pragma once

#include <filesystem>

#include "Core/Core.h"

inline constexpr std::string_view kDefaultRootPath = ".\\";
inline constexpr std::string_view kDefaultAssetsPath = ".\\Assets\\";

class FFileManager
{
public:
	FFileManager();
	FFileManager(std::string_view fileDirPath);
	FFileManager(std::string_view fileDirPath, std::string_view rootPath);

	FString ReadFileToString(std::string_view fileName) const;
	void WriteStringToFile(std::string_view fileName, std::string_view content) const;

private:
	std::filesystem::path mFileDirPath;
	std::filesystem::path mRootPath;

	bool IsUnderRoot(const std::filesystem::path& filePath) const;
	bool IsUnderFileDir(const std::filesystem::path& filePath) const;
};

bool IsUnder(const std::filesystem::path& filePath, const std::filesystem::path& rootPath);
