#include "SSplitter.h"
#include "SSplitterV.h"

void SSplitterV::Split(const FRect& inRect, FRect& outLT, FRect& outRB) const
{
	// 세로 자르기
	const float leftWidth = inRect.Width * Ratio;


	outLT = { inRect.X, inRect.Y, leftWidth, inRect.Height };
	outRB = { outLT.X + leftWidth, inRect.Y, inRect.Width - leftWidth, inRect.Height };
}
