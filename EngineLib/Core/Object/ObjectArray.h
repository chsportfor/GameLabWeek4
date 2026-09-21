#pragma once

#include "Core/Core.h"
#include "Core/Container/TArray.h"
#include <vector>
#include <stdexcept>

class UObject;

// Slots persist after removal so reusing an index cannot revive an old reference.
class FUObjectArray
{
public:
    static constexpr uint32 InvalidIndex = UINT32_MAX;

    uint32 Add(UObject* Object)
    {
        uint32 index = FirstFree;
        if (index == InvalidIndex)
        {
            if (Slots.size() >= InvalidIndex) throw std::overflow_error("Object slots exhausted");
            index = static_cast<uint32>(Slots.size());
            Slots.emplace_back();
        }
        else FirstFree = Slots[index].NextFree;
        Slots[index].Object = Object;
        Slots[index].NextFree = InvalidIndex;
        ++ObjectCount;
        return index;
    }

    void Remove(uint32 Index, const UObject* Object)
    {
        assert(IsValidIndex(Index) && Slots[Index].Object == Object);
        auto& slot = Slots[Index];
        slot.Object = nullptr;
        --ObjectCount;
        // Retire the slot at overflow rather than ever reusing a generation.
        if (slot.Generation != UINT64_MAX)
        {
            ++slot.Generation;
            slot.NextFree = FirstFree;
            FirstFree = Index;
        }
    }

    bool IsValidIndex(uint32 Index) const { return Index < Slots.size() && Slots[Index].Object; }
    UObject* operator[](uint32 Index) const { return IsValidIndex(Index) ? Slots[Index].Object : nullptr; }
    uint64 GetGeneration(uint32 Index) const { return IsValidIndex(Index) ? Slots[Index].Generation : 0; }
    UObject* Resolve(uint32 Index, uint64 Generation) const
    {
        return IsValidIndex(Index) && Slots[Index].Generation == Generation ? Slots[Index].Object : nullptr;
    }
    uint32 Num() const { return ObjectCount; }
    uint32 Size() const { return static_cast<uint32>(Slots.size()); }
    TArray<UObject*> ToTArray() const
    {
        TArray<UObject*> objects;
        objects.Reserve(ObjectCount);
        for (const auto& slot : Slots) if (slot.Object) objects.Add(slot.Object);
        return objects;
    }

private:
    struct FSlot
    {
        UObject* Object = nullptr;
        uint64 Generation = 1;
        uint32 NextFree = InvalidIndex;
    };
    std::vector<FSlot> Slots;
    uint32 FirstFree = InvalidIndex;
    uint32 ObjectCount = 0;
};
