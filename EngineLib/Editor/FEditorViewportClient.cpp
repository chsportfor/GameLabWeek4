#include "FEditorViewportClient.h"

#include "Platform/WindowApplication.h"
#include "ThirdParty/ImGui/imgui.h"
#include "Console.h"
#include "Engine/SceneManager.h"
#include "Engine/Components/PrimitiveComponent.h"
#include "Core/Math/MathUtility.h"

// Primitive vertices definitions
#include "Rendering/Primitives/Cube.h"
#include "Rendering/Primitives/Sphere.h"
#include "Rendering/Primitives/Triangle.h"
#include "Rendering/Primitives/GizmoArrow.h"
#include "Rendering/Primitives/Circle.h"
#include "Rendering/Primitives/Primitives.h"


// 정점 배열이 보이는 스코프라 sizeof 로 개수가 나온다.
// 포인터로 받으면 배열 크기 정보가 사라지므로 여기서 개수를 같이 넘긴다.
static bool GetPrimitiveMesh(EPrimitive ePrimitive, const FVertexSimple*& OutVertices, const uint32*& OutIndices, uint32& OutCount)
{
	switch (ePrimitive)
	{
	case EPrimitive::EP_Cube:
		OutVertices = Cube_vertices;
		OutIndices = Cube_indices;
		OutCount = static_cast<uint32>(std::size(Cube_indices));
		return true;
	case EPrimitive::EP_Sphere:
		OutVertices = Sphere_vertices;
		OutIndices = Sphere_indices;
		OutCount = static_cast<uint32>(std::size(Sphere_indices));
		return true;
	case EPrimitive::EP_Triangle:
		OutVertices = Triangle_vertices;
		OutIndices = Triangle_indices;
		OutCount = static_cast<uint32>(std::size(Triangle_indices));
		return true;
	case EPrimitive::EP_GizmoArrow:
		OutVertices = GizmoArrow_vertices;
		OutIndices = GizmoArrow_indices;
		OutCount = static_cast<uint32>(std::size(GizmoArrow_indices));
		return true;
	case EPrimitive::EP_Circle:
		OutVertices = Circle_vertices;
		OutIndices = Circle_indices;
		OutCount = static_cast<uint32>(std::size(Circle_indices));
		return true;
	case EPrimitive::EP_BillboardQuad:
		OutVertices = Quad_vertices;
		OutIndices = Quad_indices;
		OutCount = static_cast<uint32>(std::size(Quad_indices));
		return true;
	}

	return false;
}

void FEditorViewportClient::Initialize(ELevelViewportType inType)
{
	ViewportType = inType;

	switch (inType) {
	case ELevelViewportType::Top:
		mCamera.Location = FVector({ 0, 0, 50 });
		mCamera.Rotation = FRotator({ -90, 0, 0 });	// pitch, yaw, roll
		break;

	case ELevelViewportType::Right:
		mCamera.Location = FVector({ 0, -50, 0 });
		mCamera.Rotation = FRotator({ 0, 90, 0 });
		break;

	case ELevelViewportType::Front:
		mCamera.Location = FVector({ -50, 0, 0 });
		mCamera.Rotation = FRotator({ 0, 0, 0 });
		break;
	default:
		break;
	}
}

bool FEditorViewportClient::RaycastBounds(
	const FVector& rayStart,
	const FVector& rayEnd,
	const FBoundingBox& bounds)
{
	const FVector direction = rayEnd - rayStart;

	float tMin = 0.0f;
	float tMax = 1.0f;

	for (int axis = 0; axis < 3; ++axis)
	{
		const float origin = rayStart[axis];
		const float dir = direction[axis];
		const float minValue = bounds.Min[axis];
		const float maxValue = bounds.Max[axis];

		if (fabsf(dir) < 1e-6f)
		{
			if (origin < minValue || origin > maxValue)
			{
				return false;
			}
			continue;
		}

		float t1 = (minValue - origin) / dir;
		float t2 = (maxValue - origin) / dir;

		if (t1 > t2)
		{
			std::swap(t1, t2);
		}

		tMin = max(tMin, t1);
		tMax = min(tMax, t2);

		if (tMin > tMax)
		{
			return false;
		}
	}

	return true;
}

void FEditorViewportClient::RayCast(D3D11_VIEWPORT ViewportInfo,
	const FPickTargets& PickTargets, float perspectiveRatio, bool bCheckObject)
{
	bMouseHit = false;
	mHoveredActor.Reset();

	// 투영 방식에 따라 광선을 만드는 법만 다르다. 두 점을 구하고 나면 이후 판정은 완전히 같다
	FVector NearPoint, FarPoint;
	DeprojectScreenToWorldForUnified(WindowApplication.Input.CursorX - ViewportInfo.TopLeftX, WindowApplication.Input.CursorY - ViewportInfo.TopLeftY,
		ViewportInfo.Width, ViewportInfo.Height, FCamera::NearPlane, mCamera.mFarPlane, mCamera.mOrthoDistance, perspectiveRatio, NearPoint, FarPoint);

	mRayNear = NearPoint;
	mRayFar = FarPoint;

	float NearlistT = FLT_MAX;

	// 드래그 중에는 히트 판정을 하지 않는다.
	// 빠르게 끌면 커서가 축 캡슐을 벗어나는데, 그때 eAxis가 NONE이 되면 드래그가 끊긴다.
	if (mGizmo.mDraggingAxis != EGIZMO_AXIS::NONE)
	{
		bMouseHit = true;
		mGizmo.mbHovered = true;
		mGizmo.eAxis = mGizmo.mDraggingAxis;   // 끌고 있는 축의 강조를 유지한다
		return;
	}

	// Gizmo 탐색
	if (mGizmo.IsRayInGizmo(NearPoint, FarPoint))
	{
		bMouseHit = true;
		mGizmo.mbHovered = true;
		// gizmo highlight
		return;
	}

	// 클릭하지 않았다면 Object는 검사하지 않음
	if (!bCheckObject)
	{
		return;
	}

    const FPickingRay ray{NearPoint, FarPoint};
    for (const auto& reference : PickTargets)
    {
        const auto* component = reference.Get();
        if (!component || !component->GetOwner()) continue;
        float hitT = FLT_MAX;
        if (component->RayCastComponent(ray, mCamera, hitT) && hitT < NearlistT)
        {
            NearlistT = hitT;
            bMouseHit = true;
            mHoveredActor = component->GetOwner();
        }
    }
}

void FEditorViewportClient::UpdateCameraControls(float deltaTime, float perspectiveRatio,
	bool bAllowMouseInput, bool bAllowKeyboardInput)
{
	const FInputState& Input = WindowApplication.Input;

	// Camera Rotate
	// 회전을 이동보다 먼저, 이번 프레임에 돌린 방향으로 바로 움직이게
	if (bAllowMouseInput && Input.IsDown(VK_RBUTTON))
	{
		mCamera.Rotate(Input.MouseDX, Input.MouseDY);
	}

	// Camera Velocity
	FVector MoveDir(0.f, 0.f, 0.f);
	if (bAllowKeyboardInput)
	{
		const FMatrix R = FMatrix::Rotate(mCamera.Rotation);
		const FVector Forward = R.GetUnitAxis(EAxis::X);
		const FVector Right = R.GetUnitAxis(EAxis::Y);

		if (Input.IsDown('W')) MoveDir += Forward;
		if (Input.IsDown('S')) MoveDir -= Forward;
		if (Input.IsDown('D')) MoveDir += Right;
		if (Input.IsDown('A')) MoveDir -= Right;
		if (Input.IsDown('E')) MoveDir += FVector(0.f, 0.f, 1.f);
		if (Input.IsDown('Q')) MoveDir -= FVector(0.f, 0.f, 1.f);
	}

	const bool bMoveKeyDown = !MoveDir.IsNearlyZero();
	if (bMoveKeyDown)
	{
		MoveDir.Normalize();
	}


	//Camera Translate
	if (bAllowMouseInput && Input.MouseWheelDelta != 0.0f)
	{
		//키 입력이 없으면 마우스 휠은 줌인/줌아웃
		if (!bMoveKeyDown)
		{
			if (perspectiveRatio < 1.0f)
			{
				mCamera.mOrthoDistance *= FMath::Pow(1.2f, -Input.MouseWheelDelta);
				mCamera.mOrthoDistance = FMath::Clamp(mCamera.mOrthoDistance, 0.1f, 100.0f);
			}
			else
			{
				mCamera.Location += mCamera.GetForwardVector() * 1.0f * Input.MouseWheelDelta;
			}
		}
		//입력이 있으면 마우스 휠은 카메라 이동속도 조절
		else
		{
			mCamera.Speed *= FMath::Pow(1.2f, Input.MouseWheelDelta);
			mCamera.Speed = FMath::Clamp(mCamera.Speed, 0.1f, 100.0f);
		}
	}

	const FVector TargetVelocity = MoveDir * mCamera.Speed;

	// 지수 감쇠만큼 카메라 속도가 서서히 줄어듬
	const float Alpha = FMath::Exp(-mCamera.mDamping * deltaTime);
	mCamera.Velocity = TargetVelocity + (mCamera.Velocity - TargetVelocity) * Alpha;
	if (mCamera.Velocity.IsNearlyZero())
	{
		mCamera.Velocity = FVector(0.f);
	}

	mCamera.Location += mCamera.Velocity * deltaTime;
}

void FEditorViewportClient::Update(float deltaTime, D3D11_VIEWPORT ViewportInfo, FSceneManager* sceneManager, float perspectiveRatio)
{
	const FInputState& Input = WindowApplication.Input;
	ImGuiIO& io = ImGui::GetIO();
	UpdateCameraControls(deltaTime, perspectiveRatio,
		!io.WantCaptureMouse, !io.WantCaptureKeyboard);

	if (!io.WantCaptureKeyboard && Input.WasPressed(VK_SPACE))
	{
		mGizmo.CycleGizmoType();
	}

	const bool bLeftClicked = !io.WantCaptureMouse && Input.WasPressed(VK_LBUTTON);

	RayCast(ViewportInfo, bLeftClicked ? sceneManager->GetPickTargets() : FPickTargets{}, perspectiveRatio, bLeftClicked);

	if (sceneManager->GetSelectedActor())
	{
		if (Input.IsDown(VK_CONTROL) && Input.WasPressed('C'))
		{
			sceneManager->GetSelectedActor()->SerializeClass(mActorClipBoard);
		}
	}

	if (!mActorClipBoard.IsNull() && Input.IsDown(VK_CONTROL) && Input.WasPressed('V'))
	{
		const auto& copyObject = mActorClipBoard;
		FString className(copyObject.at("ClassName").ToString());
		const FClassInfo* classinfo = FObjectFactory::GetClassInfoByName(className); // Actor Class 이름을 읽어서 classinfo 가져옴
		if (classinfo == nullptr)
		{
			return;
		}
		UObject* LoadActor = FObjectFactory::LoadObject(classinfo,copyObject); // classinfo 바탕으로 object 생성
		if (LoadActor == nullptr)
		{
			return;
		}
		AActor* NewActor = LoadActor->Cast<AActor>(); // AActor로 캐스팅 => 실제 하는 작업은 다 AActor를 이용하는 작업
		if (NewActor == nullptr)
		{
			LoadActor->Destroy();
			return;
		}
		UWorld * CurrentWorld = sceneManager->GetCurrentWorld();
		CurrentWorld->AddActor(NewActor);
		NewActor->SetLocation(NewActor->GetTransform().Location + FVector(1.0f, 1.0f, 0.0f)); // 겹치지 않게 위치 변경
		sceneManager->SetSelectedActor(NewActor); // Select 변경
	}

	// 누른 순간에만 선택을 갱신한다. 떼는 것으로는 선택이 풀리지 않는다.
	if (bLeftClicked)
	{
		AActor* Hit = nullptr;

		if (IsMouseHit())
		{
			//Gizmo라면 드래그 기준값을 저장
			if (mGizmo.eAxis != EGIZMO_AXIS::NONE &&
				sceneManager->IsActorSelected() &&
				mGizmo.mDraggingAxis == EGIZMO_AXIS::NONE)
			{
				mGizmo.BeginDrag(mRayNear, mRayFar, sceneManager->GetSelectedActor()->GetTransform());
			}

			//Actor라면 액터를 저장
			else
			{
				Hit = mHoveredActor.Get();
			}
		}

		//Gizmo를 제외한 다른 것을 눌렀을 때, ClickedActor로 갱신
		if (!mGizmo.mbHovered)
		{
			if (Hit != nullptr)
			{
				sceneManager->SetSelectedActor(Hit);
			}
			else
			{
				sceneManager->ResetSelectedActor();
			}
		}


	}

	//Gizmo 축을 클릭한 상태로 마우스 이동이 있으면 해당 축 방향으로 ClickedActor을 변형한다.
	if (mGizmo.mDraggingAxis != EGIZMO_AXIS::NONE && sceneManager->IsActorSelected())
	{
		if (mGizmo.eType == EGIZMO_TYPE::TRANSLATE)
		{
			// 절대 좌표가 아니라 시작 시점 대비 변위. 축 직선도 시작 시점에 고정돼 있다
			FVector newLocation;
			if (mGizmo.GetDragLocation(mRayNear, mRayFar, newLocation))
			{
				sceneManager->GetSelectedActor()->SetLocation(newLocation);
			}
		}
		if (mGizmo.eType == EGIZMO_TYPE::ROTATE)
		{
			// 링 평면 위에서 잰 각도. 시작 회전에 누적각을 한 번만 얹는다
			FQuat newRotation;
			if (mGizmo.GetDragRotation(mRayNear, mRayFar, newRotation))
			{
				mGizmo.UpdateRotation = newRotation;
				sceneManager->GetSelectedActor()->SetRotation(newRotation);
			}
		}
		if (mGizmo.eType == EGIZMO_TYPE::SCALE)
		{
			FVector newScale;
			if (mGizmo.GetDragScale(mRayNear, mRayFar, newScale))
			{
				sceneManager->GetSelectedActor()->SetScale(newScale);
			}
		}
	}

	if (!ImGui::GetIO().WantCaptureMouse && Input.WasReleased(VK_LBUTTON))
	{
		mGizmo.mDraggingAxis = EGIZMO_AXIS::NONE;
	}

	//변형된 Actor를 바탕으로 Gizmo를 위치시킨다.
	UpdateGizmo(sceneManager->GetSelectedActor());


}

void FEditorViewportClient::UpdateGizmo(const AActor* selectedActor)
{
	float t = (IsOrtho() ? 0.0f : 1.0f);
	mGizmo.Update(
		selectedActor,
		mCamera.Location,
		mCamera.GetForwardVector(),
		mCamera.mFovDegree,
		t,
		mCamera.mOrthoDistance);
}

void FEditorViewportClient::DeprojectScreenToWorldForUnified(
	int32 MouseX, int32 MouseY,
	float ScreenW, float ScreenH, float NearZ, float FarZ,
	float orthoDistance, float perspectiveRatio,
	FVector& OutNearPoint, FVector& OutFarPoint
)
{
	const float ndcX = (2.0f * (MouseX + 0.5f) / ScreenW) - 1.0f;
	const float ndcY = 1.0f - (2.0f * (MouseY + 0.5f) / ScreenH);

	// 원근, 직교 모두 한번에 처리 (0(직교) ~ 1(원근))
	const FMatrix invProjection = GetInverseProjectionMatrix(ScreenW / ScreenH);

	const FMatrix invViewProj = invProjection * mCamera.GetViewMatrix().Inverse();

	const auto Unproject = [&](float ndcZ) -> FVector
		{
			const FVector xyz = invViewProj.TransformPosition(FVector(ndcX, ndcY, ndcZ));

			const float w =
				ndcX * invViewProj.M[0][3] +
				ndcY * invViewProj.M[1][3] +
				ndcZ * invViewProj.M[2][3] +
				invViewProj.M[3][3];

			return xyz * (1.0f / w);
		};

	OutNearPoint = Unproject(0.0f);
	OutFarPoint = Unproject(1.0f);
}

void FEditorViewportClient::Reset()
{
	mHoveredActor.Reset();
	bMouseHit = false;
	mGizmo.Reset();
}

FMatrix FEditorViewportClient::GetProjectionMatrix(float aspect) const
{
	const float t = IsOrtho() ? 0.0f : 1.0f;
	return mCamera.GetUnifiedProjectionMatrix(aspect,
		mCamera.mFovDegree, mCamera.mOrthoDistance, FCamera::NearPlane, FCamera::FarPlane, t);
}

FMatrix FEditorViewportClient::GetInverseProjectionMatrix(float aspect) const
{
	const float t = IsOrtho() ? 0.0f : 1.0f;
	return mCamera.GetInverseUnifiedProjectionMatrix(aspect,
		mCamera.mFovDegree, mCamera.mOrthoDistance, FCamera::NearPlane, FCamera::FarPlane, t);
}
