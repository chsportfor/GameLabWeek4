#pragma once

#include "Core/Core.h"
#include "SSplitter.h"

class SSplitterV : public SSplitter{
public:
	FRect GetHandleRect() const override;
	void Drag(int32 x, int32 y) override;

protected:
	void Split(const FRect& inRect, FRect& outLT, FRect& outRB) const override;
};
