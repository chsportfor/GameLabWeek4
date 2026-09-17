#pragma once

#include "Core/Math/Matrix.h"
#include "Core/enum.h"

#include "Core/Container/TArray.h"
#include "Core/Container/TMap.h"
#include "Renderer.h"
#include "Camera.h"
#include "RenderInfo.h"
#include "Core/Math/Vector.h"
#include "Core/Math/FBoundingBox.h"

struct FFrustum;

struct FBuffer
{
	ID3D11Buffer* Buffer;
	uint32 SourceNum;
	FBoundingBox LocalBounds;

	ID3D11Buffer* TexturedBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	UINT IndexCount = 0;
};

struct FTexture
{
	ID3D11ShaderResourceView* SRV;
	ID3D11SamplerState* Sampler;
};

enum ERenderQueueType
{
	RQT_SimplePrimitive,
	RQT_TexturedPrimitive,
	RQT_BillboardText,
	RQT_WorldAxis,
	RQT_Gizmo,
	RQT_BoundingBox,
	RQT_Particle,
};

class FGraphicsManager
{
public:
	FGraphicsManager(HWND hWindow);
	~FGraphicsManager();

	//void Prepare(const Camera* mCamera);
	void Prepare(const FCamera* mCamera);
	void PrepareForUI();

	/* Rendering functions */
	void Render(
		const TArray<FRenderInfo>& scenerRenderInfos,
		const TArray<FRenderInfo>& gizmoRenderInfos,
		const TArray<FRenderInfo>& axisRenderInfos,
		const FCamera& camera,
		const AActor* selectedActor);

	void Display();
	void Update(float deltaTime);

	float GetAspect() const { return mAspect; }
	bool GetWireFrame() const { return mbWireFrame; }
	void SetWireFrame(bool bWireFrame) { mbWireFrame = bWireFrame; }

	bool IsPerspectiveProjection() const;
	void SetPerspectiveProjection(bool bPerspectiveProjection);

	float GetPerspectiveRatio() const { return mProjectionRatio; }
	void SetPerspectiveRatio(float ratio) { mProjectionRatio = FMath::Clamp(ratio, 0.0f, 1.0f); }

	float GetCameraOrthoDistance() const { return mCameraOrthoDistance; }
	void SetCameraOrthoDistance(float distance) { mCameraOrthoDistance = distance; }

	float GetGridWidth() const;
	void SetGridWidth(float width);

	// Todo: Change name
	void CreateBuffer(EPrimitive ePrimitive, FVertexSimple* vertices, uint32 verticesSize);
	void CreateTexturedBuffer(EPrimitive ePrimitive, const FVertexTextured* vertices, uint32 verticesSize);
	void CreatePrimitiveTexture(EPrimitive ePrimitive, const wchar_t* texturePath);

	URenderer* GetRenderer() const;

	//Highlight
	//Line batch
	// 호출 즉시 그리지 않고 배열에 쌓는다. FlushLines()에서 한 번에 그린다.
	void DrawLine(const FVector& start, const FVector& end, const FVector4& color);
	void DrawAABBLine(const FBoundingBox& bounds, const FVector4& color);
	void FlushLines();

	void InitializeLoadingScreen();
	void RenderLoadingScreen();

	bool GetShowWorldAxis() const { return mbShowWorldAxis; }
	void SetShowWorldAxis(bool bShow) { mbShowWorldAxis = bShow; }

	void SetViewMode(EViewModeIndex InViewMode);
	EViewModeIndex GetViewMode() const { return mViewMode; }

	static FVector GetPrimitiveCenter(EPrimitive type);
	static FVector GetPrimitiveHalfExtent(EPrimitive type);

	// Projection ratio smoothing
	void StartProjectionTransition(bool orthographic);
	bool IsOrthographicTarget() const;
	void UpdateProjectionTransition(float deltaTime);

	bool HasShowFlag(EEngineShowFlags Flag) const;
	uint32 GetShowFlags() const { return mShowFlags; }
	void SetShowFlag(EEngineShowFlags Flag, bool bEnable);
	void SetShowFlags(uint32 flags) { mShowFlags = flags; }

private:
	URenderer* mRenderer;
	FMatrix mViewUnifiedProjectionMatrix;

	ID3D11ShaderResourceView* mLoadingScreenSRV = nullptr;

	// Prepare에서 갱신. 하이라이트 두께의 픽셀 → 월드 환산에 쓴다
	FVector mCameraLocation;
	FVector mCameraForward;
	float mCameraFovDegree = 60.0f;
	float mCameraOrthoDistance = 10.0f;

	TMap<EPrimitive, FBuffer> mBufferMap;

	// 텍스처 정점으로 만든 버퍼
	TMap<EPrimitive, FBuffer> mTexturedBufferMap;

	// Texture sub resource view and sampler for each primitive type
	TMap<EPrimitive, FTexture> mPrimitiveTextureMap;

	// Graphics config
	// 이번 프레임에 쌓인 선분. 정점 2개가 선분 하나
	TArray<FVertexSimple> mLineVertices;
	TArray<uint32> mLineIndices;

	bool mbWireFrame;
	bool mbPerspectiveProjection;
	bool mbShowWorldAxis = true;
	float mAspect;
	float mProjectionRatio; // 0.0f ~ 1.0f, 0이면 직교, 1이면 원근, 그 사이면 혼합

	// Projection ratio smoothing
	float mProjectionStartRatio = 1.0f;
	float mProjectionTargetRatio = 1.0f;
	float mProjectionElapsed = 0.0f;
	float mProjectionDuration = 1.0f;
	bool mbProjectionTransitioning = false;

	// Grid 간격, 최대 한계선
	float mgridExtent = 1000.0f;
	float mgridSpacing = 1.0f;

	EViewModeIndex mViewMode = EViewModeIndex::VMI_Lit;

	uint32 mShowFlags = ~0;
		//static_cast<uint32>(EEngineShowFlags::SF_Primitives) |
		//static_cast<uint32>(EEngineShowFlags::SF_BillboardText) |
		//static_cast<uint32>(EEngineShowFlags::SF_WorldAxis);

	bool mbShowPrimitives = true;
	//void RenderBillboardText();

	void updateRenderQueue(
		const TArray<FRenderInfo>& renderInfos,
		TMap<ERenderQueueType, TArray<const FRenderInfo*>>& outRenderQueueMap,
		const FFrustum* frustum);

	/* Rendering Functions */
	void renderSimplePrimitive(const TArray<const FRenderInfo*>& renderInfos, const FCamera& camera);
	void renderTexturedPrimitive(const TArray<const FRenderInfo*>& renderInfos, const FCamera& camera);
	void renderBillboardText(const TArray<const FRenderInfo*>& renderInfos, const FCamera& camera);
	void renderWorldAxis(const TArray<const FRenderInfo*>& renderInfos);
	void renderBoundingBox(const TArray<const FRenderInfo*>& renderInfos, const FRotator& cameraRotation);
	// Instancing
	void renderSimplePrimitiveInstanced(const TArray<const FRenderInfo*>& renderInfos, const FCamera& camera);
	//void RenderOverlay(const TArray<const FRenderInfo*>& renderInfos, const FCamera& camera);
	void renderHighLight(const FRenderInfo& RI, const FCamera& camera);
	void renderGrid();
	void renderGizmo(const TArray<const FRenderInfo*>& renderInfos, const FCamera& camera);
	void renderParticle(const TArray<const FRenderInfo*>& renderInfos, const FCamera& camera);

	void CalculateLineBuffer(const TArray<const FRenderInfo*>& renderInfos);

	// Instancing Test
	void RenderInstancingTest();
	ID3D11Buffer* mTestInstanceIndexBuffer = nullptr;
};
