#pragma once

#include <d3d11.h>
#include "Core/enum.h"
#include "SWindow.h"

struct FEditorViewportClient;


struct FViewport : public SWindow{
public:
	void SetRect(const FRect & inRect) override;

	void SetViewport(const D3D11_VIEWPORT& vp) { SetRect({ vp.TopLeftX, vp.TopLeftY, vp.Width, vp.Height }); }
	const D3D11_VIEWPORT& GetViewport() const { return ViewportInfo; }

	float GetAspect() const {
		if (ViewportInfo.Height == 0) return 1.0f;
		return ViewportInfo.Width / ViewportInfo.Height;
	}

	void SetClient(FEditorViewportClient& inClient) { Client = &inClient; }
	FEditorViewportClient* GetClient() const { return Client; }

private:
	D3D11_VIEWPORT ViewportInfo = {};
	FEditorViewportClient* Client = {};
};
