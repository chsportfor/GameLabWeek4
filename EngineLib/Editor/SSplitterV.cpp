#include "SSplitter.h"
#include "SSplitterV.h"

FRect SSplitterV::GetHandleRect() const
{
	// 경계선 부분만 클릭 가능
	const float leftWidth = Rect.X + Rect.Width * Ratio;
	return FRect({leftWidth - 4.0f, Rect.Y, 8.0f, Rect.Height});
}

void SSplitterV::Drag(int32 x, int32 y)
{
	if (Rect.Width <= 0.0f) return;

	SetRatio((x - Rect.X) / Rect.Width);
}

void SSplitterV::Split(const FRect& inRect, FRect& outLT, FRect& outRB) const
{
	// 세로 자르기
	const float leftWidth = inRect.Width * Ratio;


	outLT = { inRect.X, inRect.Y, leftWidth, inRect.Height };
	outRB = { outLT.X + leftWidth, inRect.Y, inRect.Width - leftWidth, inRect.Height };
}
