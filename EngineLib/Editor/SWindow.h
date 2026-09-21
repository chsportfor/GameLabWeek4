#pragma once

#include "Core/Core.h"

struct FRect {
	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;

	bool Contains(int32 x, int32 y) const;
};

class SWindow {
public:
	virtual ~SWindow() = default;
	bool IsHover(int32 x, int32 y) const;
	

	virtual void SetRect(const FRect& inRect) { Rect = inRect; }

protected:
	FRect Rect;
};
