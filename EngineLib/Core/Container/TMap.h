#pragma once

#include <cassert>
#include <unordered_map>
#include <utility>
#include <initializer_list>

#include "Core/Core.h"

template <typename T, typename V>
class TMap
{
public:
	TMap() = default;
	TMap(std::initializer_list<std::pair<const T, V>> initList) : mMap(initList) {}

	~TMap() = default;

	std::unordered_map<T, V>::iterator begin();
	std::unordered_map<T, V>::iterator end();

	std::unordered_map<T, V>::const_iterator begin() const;
	std::unordered_map<T, V>::const_iterator end() const;

	void Add(const T& key, const V& Value);
	int32 Remove(const T& key);
	
	uint32 Num() const;
	void Reset();
	void Empty(int32 ExpectedNumElements = 0);
	V* Find(const T& key);
	const V* Find(const T& key) const;

	bool Contains(const T& key) const;
	bool IsEmpty() const;
	void Reserve(int32 Capacity);

	V& operator[](const T& key);
	const V& operator[](const T& key) const;

private:
	std::unordered_map<T, V> mMap;
};

template<typename T, typename V>
inline std::unordered_map<T, V>::iterator TMap<T, V>::begin()
{
	return mMap.begin();
}

template<typename T, typename V>
inline std::unordered_map<T, V>::iterator TMap<T, V>::end()
{
	return mMap.end();
}

template<typename T, typename V>
inline std::unordered_map<T, V>::const_iterator TMap<T, V>::begin() const
{
	return mMap.cbegin();
}

template<typename T, typename V>
inline std::unordered_map<T, V>::const_iterator TMap<T, V>::end() const
{
	return mMap.cend();
}

template <typename T, typename V>
inline void TMap<T, V>::Add(const T& key, const V& value)
{
	mMap[key] = value;
}

template <typename T, typename V>
inline int32 TMap<T, V>::Remove(const T& key)
{
	return static_cast<int32>(mMap.erase(key));
}

template <typename T, typename V>
inline uint32 TMap<T, V>::Num() const
{
	return static_cast<uint32>(mMap.size());
}

template <typename T, typename V>
inline void TMap<T, V>::Reset()
{
	mMap.clear();
}

template <typename T, typename V>
inline void TMap<T, V>::Empty(int32 capacity)
{
	mMap.clear();
	mMap.reserve(static_cast<size_t>(capacity));
}

template <typename T, typename V>
inline V* TMap<T, V>::Find(const T& key)
{
	auto iter = mMap.find(key);
	if (iter == mMap.end())
	{
		return nullptr;
	}

	return &iter->second;
}

template <typename T, typename V>
inline const V* TMap<T, V>::Find(const T& key) const
{
	auto iter = mMap.find(key);
	if (iter == mMap.end())
	{
		return nullptr;
	}

	return &iter->second;
}

template <typename T, typename V>
inline bool TMap<T, V>::Contains(const T& key) const
{
	return mMap.find(key) != mMap.end();
}

template <typename T, typename V>
inline bool TMap<T, V>::IsEmpty() const
{
	return mMap.empty();
}

template <typename T, typename V>
inline void TMap<T, V>::Reserve(int32 capacity)
{
	mMap.reserve(static_cast<size_t>(capacity));
}

template <typename T, typename V>
inline V& TMap<T, V>::operator[](const T& key)
{
	return mMap[key];
}

template <typename T, typename V>
inline const V& TMap<T, V>::operator[](const T& key) const
{
	return mMap.at(key);
}
