#pragma once

#include "Core/Core.h"

class UObject;

// Runtime identity only. Never serialized and never extends object lifetime.
class FWeakObjectPtr
{
public:
    FWeakObjectPtr() = default;
    FWeakObjectPtr(const UObject* Object);
    UObject* Get() const;
    bool IsValid() const { return Get() != nullptr; }
    void Reset() { *this = FWeakObjectPtr{}; }
    bool operator==(const FWeakObjectPtr&) const = default;

private:
    uint32 ObjectIndex = UINT32_MAX;
    uint64 Generation = 0;
};

template<typename T>
class TWeakObjectPtr
{
public:
    TWeakObjectPtr() = default;
    TWeakObjectPtr(T* Object) : Reference(Object) {}
    T* Get() const { return static_cast<T*>(Reference.Get()); }
    bool IsValid() const { return Reference.IsValid(); }
    explicit operator bool() const { return IsValid(); }
    void Reset() { Reference.Reset(); }
    bool operator==(const TWeakObjectPtr&) const = default;

private:
    FWeakObjectPtr Reference;
};
