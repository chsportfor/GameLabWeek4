#include "FEditorViewportClient.h"

#include "Platform/WindowApplication.h"
#include "ThirdParty/ImGui/imgui.h"
#include "Console.h"
#include "Engine/SceneManager.h"
#include "Engine/Components/PrimitiveComponent.h"
#include "Core/Math/MathUtility.h"

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

void FEditorViewportClient::Update(float deltaTime, D3D11_VIEWPORT ViewportInfo, FSceneManager* sceneManager, float perspectiveRatio)
{
	const FInputState& Input = WindowApplication.Input;
	ImGuiIO& io = ImGui::GetIO();

	// Camera Rotate
	// 회전을 이동보다 먼저, 이번 프레임에 돌린 방향으로 바로 움직이게
	if (!io.WantCaptureMouse && Input.IsDown(VK_RBUTTON))
	{
		mCamera.Rotate(Input.MouseDX, Input.MouseDY);
	}

	// Camera Velocity
	FVector MoveDir(0.f, 0.f, 0.f);
	if (!io.WantCaptureKeyboard)
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
	if (!io.WantCaptureMouse && Input.MouseWheelDelta != 0.0f)
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
	mGizmo.Update(
		sceneManager->GetSelectedActor(),
		mCamera.Location,
		mCamera.GetForwardVector(),
		mCamera.mFovDegree,
		perspectiveRatio,
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

	const FMatrix invProjection = mCamera.GetInverseUnifiedProjectionMatrix(
		ScreenW / ScreenH, mCamera.mFovDegree, orthoDistance, NearZ, FarZ, perspectiveRatio
	);

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
