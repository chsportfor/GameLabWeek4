#include "SSplitter.h"
#include "SSplitterH.h"
#include <Core/Math/MathUtility.h>

FRect SSplitterH::GetHandleRect() const {
	// 경계선 부분만 클릭 가능
	const float topHeight = Rect.Y + Rect.Height * Ratio;
	return FRect({ Rect.X, topHeight - 4.0f, Rect.Width, 8.0f});
}

void SSplitterH::Drag(int32 x, int32 y)
{
	if (Rect.Height <= 0.0f) return;

	SetRatio((y - Rect.Y) / Rect.Height);
}

void SSplitterH::Split(const FRect& inRect, FRect& outLT, FRect& outRB) const
{
	// 가로 자르기
	const float topHeight = inRect.Height * Ratio;


	outLT = { inRect.X, inRect.Y, inRect.Width, topHeight };
	outRB = { inRect.X, inRect.Y + topHeight, inRect.Width, inRect.Height - topHeight };
}
