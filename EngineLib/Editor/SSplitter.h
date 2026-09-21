#pragma once

#include "Core/Core.h"
#include "SWindow.h"

class SSplitter : public SWindow{
public:
	virtual void SetRect(const FRect& inRect) override;

	SWindow* SideLT = nullptr;
	SWindow* SideRB = nullptr;
	float Ratio = 0.5f;	// Ratio : 왼쪽윗칸이 차지하는 비율! (aspect아님)

protected:
	virtual void Split(const FRect& inRect, FRect& outLT, FRect& outRB) const = 0;
};
