#pragma once

#include "Object.h"

TSparseArray<UObject*> UObject::GUObjectArray;

template<typename TObject>
class FObjectIterator
{
private:
	int32 CurrentIndex;

	FObjectIterator& operator++()
	{
		++CurrentIndex;
		AdvanceToNextValidObject();
		return *this;
	}

	void AdvanceToNextValidObject() {
		// GObjObjects
		// Obj->IsA<TObject>()
	}
};
