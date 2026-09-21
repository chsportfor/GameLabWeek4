#include "FViewport.h"

void FViewport::SetRect(float x, float y, float w, float h)
{
	ViewportInfo.TopLeftX = x;
	ViewportInfo.TopLeftY = y;
	ViewportInfo.Width = w;
	ViewportInfo.Height = h;
	ViewportInfo.MinDepth = 0.0f;
	ViewportInfo.MaxDepth = 1.0f;
}

bool FViewport::IsHover(int32 x, int32 y) const
{
	const float fx = static_cast <float> (x);
	const float fy = static_cast <float> (y);
	if (ViewportInfo.TopLeftX <= fx && fx < ViewportInfo.TopLeftX + ViewportInfo.Width
		&& ViewportInfo.TopLeftY <= fy && fy < ViewportInfo.TopLeftY + ViewportInfo.Height) {
		return true;
	}

	return false;
}
