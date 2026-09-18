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

	// Relative paths resolve under the configured directory; absolute paths are used as given.
	FString ReadFileToString(const std::filesystem::path& filePath) const;

	void WriteStringToFile(const std::filesystem::path& filePath, std::string_view content) const;

private:
	std::filesystem::path mFileDirPath;
	std::filesystem::path mRootPath;

	std::filesystem::path ResolvePath(const std::filesystem::path& filePath) const;

	bool IsUnderRoot(const std::filesystem::path& filePath) const;
	bool IsUnderFileDir(const std::filesystem::path& filePath) const;
};

bool IsUnder(const std::filesystem::path& filePath, const std::filesystem::path& rootPath);
