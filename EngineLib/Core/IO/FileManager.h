#pragma once

#include <filesystem>

#include "Core/Core.h"

inline constexpr std::string_view kDefaultRootPath = ".\\";
inline constexpr std::string_view kDefaultAssetsPath = ".\\Assets\\";

class FFileManager
{
public:
	static FFileManager& Get();
	void Initialize(std::string_view fileDirPath = kDefaultAssetsPath,
		std::string_view rootPath = kDefaultRootPath);

	FFileManager(const FFileManager&) = delete;
	FFileManager& operator=(const FFileManager&) = delete;
	FFileManager(FFileManager&&) = delete;
	FFileManager& operator=(FFileManager&&) = delete;

	// Relative paths resolve under the configured directory; absolute paths are used as given.
	FString ReadFileToString(const std::filesystem::path& filePath) const;

	void WriteStringToFile(const std::filesystem::path& filePath, std::string_view content) const;
	std::filesystem::path GetFileDirectoryPath() const;
	std::filesystem::path ResolvePath(const std::filesystem::path& filePath) const;

private:
	FFileManager() = default;
	~FFileManager() = default;

	std::filesystem::path mFileDirPath;
	std::filesystem::path mRootPath;


	bool IsUnderRoot(const std::filesystem::path& filePath) const;
	bool IsUnderFileDir(const std::filesystem::path& filePath) const;
};

bool IsUnder(const std::filesystem::path& filePath, const std::filesystem::path& rootPath);
