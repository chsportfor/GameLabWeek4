#include "Core.h"
#include "Core/Container/TArray.h"
#include "Core/Container/TMap.h"
#include "Name.h"

#include <vector>
#include <algorithm>
#include <unordered_map>

namespace
{
	struct FNameStorage
	{
		TArray<FString> DisplayPool;
		TArray<FString> ComparisonPool;
		TMap<FString, int32> DisplayLookup;
		TMap<FString, int32> ComparisonLookup;
	};

	FNameStorage& GetNameStorage()
	{
		// FName can be constructed by static initializers in other translation units.
		static FNameStorage Storage;
		return Storage;
	}

	static FString ToLower(FString str)
	{
		std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			}
		);

		return str;
	}

	static int32 FindOrAdd(TArray<FString>& Pool, TMap<FString, int32>& Lookup, const FString& Name)
	{
		int32* FoundIndex = Lookup.Find(Name);

		if (FoundIndex)
		{
			return *FoundIndex;
		}

		const int32 NewIndex = static_cast<int32>(Pool.Num());
		Pool.Add(Name);
		Lookup.Add(Name, NewIndex);

		return NewIndex;
	}
}

FName::FName()
	: DisplayIndex(-1),
	ComparisonIndex(-1)
{
}


FName::FName(const char* pStr)
{
	auto& Storage = GetNameStorage();
	FString DisplayName = pStr;
	FString ComparisonName = ToLower(DisplayName);

	DisplayIndex = FindOrAdd(Storage.DisplayPool, Storage.DisplayLookup, DisplayName);
	ComparisonIndex = FindOrAdd(Storage.ComparisonPool, Storage.ComparisonLookup, ComparisonName);
}

FName::FName(const FString& str)
	: FName(str.CStr())
{
}

FString FName::ToString() const
{
	auto& Storage = GetNameStorage();
	if (DisplayIndex < 0 || DisplayIndex >= static_cast<int32>(Storage.DisplayPool.Num()))
	{
		return FString("None");
	}

	return FString(Storage.DisplayPool[DisplayIndex]);
}


bool FName::operator==(const FName& Other) const
{
	return ComparisonIndex == Other.ComparisonIndex;
}

int32 FName::Compare(const FName& Other) const
{
	auto& Storage = GetNameStorage();
	const FString& A = Storage.ComparisonPool[ComparisonIndex];
	const FString& B = Storage.ComparisonPool[Other.ComparisonIndex];

	int Result = std::strcmp(A.CStr(), B.CStr());

	if (Result < 0)
	{
		return -1;
	}

	if (Result > 0)
	{
		return 1;
	}

	return 0;
}

