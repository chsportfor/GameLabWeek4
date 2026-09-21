#pragma once

#include <filesystem>
#include <fstream>

#include "Core/Serialization/Archive.h"

class FWindowsBinWriter final : public FArchive
{
public:
	explicit FWindowsBinWriter(const std::filesystem::path& path);
	~FWindowsBinWriter() override;

	using FArchive::Serialize;
	void Serialize(void* data, uint64 size) override;
	bool IsLoading() const override { return false; }
	bool CanSerialize(uint64 size) const override;
	bool Close();

private:
	std::ofstream Stream;
};

class FWindowsBinReader final : public FArchive
{
public:
	explicit FWindowsBinReader(const std::filesystem::path& path);

	using FArchive::Serialize;
	void Serialize(void* data, uint64 size) override;
	bool IsLoading() const override { return true; }
	bool CanSerialize(uint64 size) const override;
	bool IsAtEnd() const { return Remaining == 0; }

private:
	std::ifstream Stream;
	uint64 Remaining = 0;
};
