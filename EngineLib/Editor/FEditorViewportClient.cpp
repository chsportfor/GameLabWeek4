#include "FEditorViewportClient.h"

#include "Platform/WindowApplication.h"
#include "ThirdParty/ImGui/imgui.h"
#include "Console.h"
#include "Engine/SceneManager.h"
#include "Engine/Components/PrimitiveComponent.h"
#include "Core/Math/MathUtility.h"

void FEditorViewportClient::Initialize(ELevelViewportType inType)
{
	ViewportType = inType;

	switch (inType) {
	case ELevelViewportType::Perspective:
		mCamera.Location = FVector(-5.0f, -5.0f, 4.0f);
		mCamera.LookAt(FVector(0.0f, 0.0f, 0.0f));
		break;
	case ELevelViewportType::Top:
		mCamera.Location = FVector({ 0, 0, 50 });
		mCamera.Rotation = FRotator({ -90, 0, 0 });	// pitch, yaw, roll
		break;
	case ELevelViewportType::Bottom:
		mCamera.Location = FVector({ 0, 0, -50 });
		mCamera.Rotation = FRotator({ 90, 0, 0 });	// pitch, yaw, roll
		break;
	case ELevelViewportType::Left:
		mCamera.Location = FVector({ 0, 50, 0 });
		mCamera.Rotation = FRotator({ 0, -90, 0 });
		break;
	case ELevelViewportType::Right:
		mCamera.Location = FVector({ 0, -50, 0 });
		mCamera.Rotation = FRotator({ 0, 90, 0 });
		break;
	case ELevelViewportType::Front:
		mCamera.Location = FVector({ 50, 0, 0 });
		mCamera.Rotation = FRotator({ 0, 180, 0 });
		break;
	case ELevelViewportType::Back:
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
	// 커서가 화면 축을 벗어나도 처음 선택한 축을 유지한다.
	if (mGizmo.mDraggingAxis != EGIZMO_AXIS::NONE)
	{
		bMouseHit = true;
		mGizmo.mbHovered = true;
		mGizmo.eAxis = mGizmo.mDraggingAxis;   // 끌고 있는 축의 강조를 유지한다
		return;
	}

	// Gizmo 탐색
	if (mGizmo.IsRayInGizmo(NearPoint, FarPoint,
        FVector2(WindowApplication.Input.CursorX - ViewportInfo.TopLeftX + .5f,
            WindowApplication.Input.CursorY - ViewportInfo.TopLeftY + .5f),
        FVector2(ViewportInfo.Width, ViewportInfo.Height),
        mCamera.GetViewMatrix() * GetProjectionMatrix(ViewportInfo.Width / ViewportInfo.Height)))
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
		if (IsOrtho()) {
			const float distance = mCamera.mOrthoDistance * 0.002f;	// 1픽셸이 월드에서 몇 미터?
			mCamera.Location -= mCamera.GetRightVector() * distance * Input.MouseDX;	// 몇픽셸씩 움직였는가?
			mCamera.Location += mCamera.GetUpVector() * distance * Input.MouseDY;
		}
		else
			mCamera.Rotate(Input.MouseDX, Input.MouseDY);
	}

	// Camera Velocity
	FVector MoveDir(0.f, 0.f, 0.f);
	if (bAllowKeyboardInput && !IsOrtho())
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
			if (GetPerspectiveRatio() < 1.0f)
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

    // Sync target/rotation before picking. A release must end the drag even over editor UI.
    UpdateGizmo(sceneManager->GetSelectedActor());
    const FVector2 MousePosition(Input.CursorX - ViewportInfo.TopLeftX + .5f,
        Input.CursorY - ViewportInfo.TopLeftY + .5f);
    const FVector2 ViewportSize(ViewportInfo.Width, ViewportInfo.Height);
    const FMatrix ViewProjection = mCamera.GetViewMatrix() * GetProjectionMatrix(ViewportInfo.Width / ViewportInfo.Height);
    // FMatrix::Inverse is affine-only; use the camera's explicit inverse projection.
    const FMatrix InverseViewProjection = GetInverseProjectionMatrix(ViewportInfo.Width / ViewportInfo.Height)
        * mCamera.GetViewMatrix().Inverse();
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
				mGizmo.BeginDrag(mRayNear, mRayFar, sceneManager->GetSelectedActor()->GetTransform(),
                    MousePosition, ViewportSize, ViewProjection, InverseViewProjection);
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
			if (mGizmo.GetDragLocation(MousePosition, ViewportSize, InverseViewProjection, newLocation))
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
			if (mGizmo.GetDragScale(MousePosition, sceneManager->GetSelectedActor()->GetTransform().Scale, newScale))
			{
				sceneManager->GetSelectedActor()->SetScale(newScale);
			}
		}
	}


	//변형된 Actor를 바탕으로 Gizmo를 위치시킨다.
	UpdateGizmo(sceneManager->GetSelectedActor());


}

void FEditorViewportClient::UpdateGizmo(const AActor* selectedActor)
{
    // Called for inactive/UI-covered viewports too; do not rely on seeing the release edge.
    if (mGizmo.mDraggingAxis != NONE && !WindowApplication.Input.IsDown(VK_LBUTTON)) mGizmo.EndDrag();
	float t = GetPerspectiveRatio();
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
	const float t = GetPerspectiveRatio();
	return mCamera.GetUnifiedProjectionMatrix(aspect,
		mCamera.mFovDegree, mCamera.mOrthoDistance, FCamera::NearPlane, FCamera::FarPlane, t);
}

FMatrix FEditorViewportClient::GetInverseProjectionMatrix(float aspect) const
{
	const float t = GetPerspectiveRatio();
	return mCamera.GetInverseUnifiedProjectionMatrix(aspect,
		mCamera.mFovDegree, mCamera.mOrthoDistance, FCamera::NearPlane, FCamera::FarPlane, t);
}
