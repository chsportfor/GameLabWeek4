#pragma once

#include "Core/Core.h"
#include "SSplitter.h"

class SSplitterV : public SSplitter{

protected:
	void Split(const FRect& inRect, FRect& outLT, FRect& outRB) const override;
};
