#include "SWindow.h"

bool SWindow::IsHover(int32 x, int32 y) const
{
	return Rect.Contains(x, y);
}

bool FRect::Contains(int32 x, int32 y) const
{
	const float fx = static_cast <float>(x);
	const float fy = static_cast <float>(y);
	return X <= fx && fx < X + Width
		&& Y <= fy && fy < Y + Height;
}
