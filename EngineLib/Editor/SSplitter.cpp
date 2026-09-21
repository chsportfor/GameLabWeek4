#include "SSplitter.h"

void SSplitter::SetRect(const FRect& inRect)
{
	SWindow::SetRect(inRect);

	FRect lt, rb;
	Split(inRect, lt, rb);

	if (SideLT) SideLT->SetRect(lt);
	if (SideRB) SideRB->SetRect(rb);
}
