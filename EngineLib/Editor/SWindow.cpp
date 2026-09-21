#include "SWindow.h"

bool SWindow::IsHover(int32 x, int32 y) const
{
	const float fx = static_cast <float> (x);
	const float fy = static_cast <float> (y);
	if (Rect.X <= fx && fx < Rect.X + Rect.Width
		&& Rect.Y <= fy && fy < Rect.Y + Rect.Height) {
		return true;
	}

	return false;
}
