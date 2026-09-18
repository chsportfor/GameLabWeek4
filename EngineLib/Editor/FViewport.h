#pragma once

#include <d3d11.h>
#include "Core/enum.h"

struct FEditorViewportClient;


struct FViewport {
public:
	void SetRect(float x, float y, float w, float h);

	void SetViewport(D3D11_VIEWPORT inViewport) { ViewportInfo = inViewport; };
	const D3D11_VIEWPORT& GetViewport() const { return ViewportInfo; }

	float GetAspect() const {
		if (ViewportInfo.Height == 0) return 1.0f;
		return ViewportInfo.Width / ViewportInfo.Height;
	}

	bool IsHover(int32 x, int32 y) const;

private:
	D3D11_VIEWPORT ViewportInfo = {};
	FEditorViewportClient* Client = {};
};
