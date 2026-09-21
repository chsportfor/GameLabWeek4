#pragma once

#include <type_traits>

#include "Core/Core.h"

// Direction-independent byte archive. Serializers use the same operator<< path
// for loading and saving; platform archives provide the actual byte transport.
class FArchive
{
public:
	virtual ~FArchive() = default;

	virtual void Serialize(void* data, uint64 size) = 0;
	void Serialize(const void* data, uint64 size)
	{
		if (IsLoading())
		{
			SetError();
			return;
		}
		Serialize(const_cast<void*>(data), size);
	}
	virtual bool IsLoading() const = 0;
	bool IsSaving() const { return !IsLoading(); }
	bool IsError() const { return bError; }

	// Readers use this before allocating variable-sized data. Writers return true
	// while their output stream remains usable.
	virtual bool CanSerialize(uint64 size) const = 0;

	template <typename T>
	requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
	FArchive& operator<<(T& value)
	{
		Serialize(&value, sizeof(T));
		return *this;
	}

protected:
	void SetError() { bError = true; }

private:
	bool bError = false;
};
