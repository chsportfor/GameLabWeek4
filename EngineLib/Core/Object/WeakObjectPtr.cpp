#include "WeakObjectPtr.h"
#include "Object.h"

FWeakObjectPtr::FWeakObjectPtr(const UObject* Object)
{
    if (Object)
    {
        ObjectIndex = Object->InternalIndex;
        Generation = UObject::GetGObjectArray().GetGeneration(ObjectIndex);
    }
}

UObject* FWeakObjectPtr::Get() const
{
    return UObject::GetGObjectArray().Resolve(ObjectIndex, Generation);
}
