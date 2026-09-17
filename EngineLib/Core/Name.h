#pragma once

struct FName
{
	FName();
	FName(const char* pStr);
	FName(const FString& str);

	FString ToString() const;

	int32 Compare(const FName& other) const;
	bool operator==(const FName& other) const;

	int32 DisplayIndex;
	int32 ComparisonIndex;
};

template<>
struct std::hash<FName>
{
	std::size_t operator()(const FName& name) const noexcept
	{
		return std::hash<int32>{}(name.ComparisonIndex);
	}
};
