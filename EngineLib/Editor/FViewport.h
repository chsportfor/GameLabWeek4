#pragma once

#include "Core/Core.h"
#include <d3d11.h>

struct FEditorViewportClient;

enum class ELevelViewportType : uint8 {
	Perspective,
	Top,
	Bottom,
	Left,
	Right,
	Front,
	Back
};


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
