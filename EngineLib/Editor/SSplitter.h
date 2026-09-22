#pragma once

#include "Core/Core.h"
#include "SWindow.h"
#include <Core/Math/MathUtility.h>


class SSplitter : public SWindow{
public:
	virtual void SetRect(const FRect& inRect) override;
	void SetRatio(float inRatio) { Ratio = FMath::Clamp(inRatio, 0.13f, 0.87f); }
	float GetRatio() const { return Ratio; }

	virtual FRect GetHandleRect() const = 0;

	virtual void Drag(int32 x, int32 y) = 0;

	SWindow* SideLT = nullptr;
	SWindow* SideRB = nullptr;

protected:
	virtual void Split(const FRect& inRect, FRect& outLT, FRect& outRB) const = 0;
	float Ratio = 0.5f;	// Ratio : 왼쪽윗칸이 차지하는 비율! (aspect아님)
};
