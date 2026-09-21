#include "SSplitter.h"
#include "SSplitterH.h"

void SSplitterH::Split(const FRect& inRect, FRect& outLT, FRect& outRB) const
{
	// 가로 자르기
	const float topHeight = inRect.Height * Ratio;


	outLT = { inRect.X, inRect.Y, inRect.Width, topHeight };
	outRB = { inRect.X, inRect.Y + topHeight, inRect.Width, inRect.Height - topHeight };
}
