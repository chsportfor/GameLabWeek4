#pragma once

#include "Object.h"

template<typename TObject>
class FObjectIterator
{
public:
	FObjectIterator() : CurrentIndex(0)
	{
		AdvanceToNextValidObject();
	}

	operator bool() const
	{
		return UObject::GetGObjectArray().IsValidIndex(CurrentIndex);
	}

	bool operator !() const
	{
		return !(UObject::GetGObjectArray().IsValidIndex(CurrentIndex));
	}

	UObject* GetObject()
	{
		return UObject::GetGObjectArray()[CurrentIndex];
	}

	FObjectIterator& operator++()
	{
		++CurrentIndex;
		AdvanceToNextValidObject();
		return *this;
	}

	TObject* operator* () const
	{
		return static_cast<TObject*>(UObject::GetGObjectArray()[CurrentIndex]);
	}

	TObject* operator-> () const
	{
		return static_cast<TObject*>(UObject::GetGObjectArray()[CurrentIndex]);
	}

	bool operator==(const TObjectIterator& Rhs) const { return Index == Rhs.Index; }
	bool operator!=(const TObjectIterator& Rhs) const { return Index != Rhs.Index; }

private:
	int32 CurrentIndex;

	void AdvanceToNextValidObject() {
		for (;CurrentIndex < UObject::GetGObjectArray().Size();CurrentIndex++)
		{
			if (UObject::GetGObjectArray().IsValidIndex(CurrentIndex))
			{
				UObject* obj = UObject::GetGObjectArray()[CurrentIndex];
				if (obj != nullptr && obj->IsA<TObject>())
				{
					return;
				}
			}
		}
	}
};
