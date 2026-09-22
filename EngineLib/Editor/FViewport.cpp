#include "FViewport.h"

void FViewport::SetRect(const FRect& inRect)
{
	SWindow::SetRect(inRect);

	ViewportInfo.TopLeftX = inRect.X;
	ViewportInfo.TopLeftY = inRect.Y;
	ViewportInfo.Width = inRect.Width;
	ViewportInfo.Height = inRect.Height;
	ViewportInfo.MinDepth = 0.0f;
	ViewportInfo.MaxDepth = 1.0f;
}
