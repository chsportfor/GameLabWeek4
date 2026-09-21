#include "Platform/WindowsBinArchive.h"

#include <limits>

FWindowsBinWriter::FWindowsBinWriter(const std::filesystem::path& path)
	: Stream(path, std::ios::out | std::ios::binary | std::ios::trunc)
{
	if (!Stream.is_open()) SetError();
}

FWindowsBinWriter::~FWindowsBinWriter()
{
	if (Stream.is_open()) Stream.close();
}

void FWindowsBinWriter::Serialize(void* data, uint64 size)
{
	if (!CanSerialize(size))
	{
		SetError();
		return;
	}
	Stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
	if (!Stream.good()) SetError();
}

bool FWindowsBinWriter::CanSerialize(uint64 size) const
{
	return Stream.is_open() && Stream.good()
		&& size <= static_cast<uint64>((std::numeric_limits<std::streamsize>::max)());
}

bool FWindowsBinWriter::Close()
{
	if (!Stream.is_open()) return !IsError();
	Stream.flush();
	if (!Stream.good()) SetError();
	Stream.close();
	return !IsError();
}

FWindowsBinReader::FWindowsBinReader(const std::filesystem::path& path)
	: Stream(path, std::ios::in | std::ios::binary)
{
	if (!Stream.is_open())
	{
		SetError();
		return;
	}
	Stream.seekg(0, std::ios::end);
	const std::streamoff size = Stream.tellg();
	if (size < 0)
	{
		SetError();
		return;
	}
	Remaining = static_cast<uint64>(size);
	Stream.seekg(0, std::ios::beg);
}

void FWindowsBinReader::Serialize(void* data, uint64 size)
{
	if (!CanSerialize(size))
	{
		SetError();
		return;
	}
	Stream.read(static_cast<char*>(data), static_cast<std::streamsize>(size));
	if (!Stream.good())
	{
		SetError();
		return;
	}
	Remaining -= size;
}

bool FWindowsBinReader::CanSerialize(uint64 size) const
{
	return Stream.is_open() && Stream.good() && size <= Remaining
		&& size <= static_cast<uint64>((std::numeric_limits<std::streamsize>::max)());
}
