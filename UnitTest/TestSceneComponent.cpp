#include "pch.h"

#include "Engine/Components/SceneComponent.h"
#include "Engine/Actor.h"
#include "Core/Math/Matrix.h"

namespace SceneComponentTest
{

	void ExpectMatrixNear(const FMatrix& actual, const FMatrix& expected, float tolerance = KINDA_SMALL_NUMBER)
	{
		for (int32 row = 0; row < 4; ++row)
		{
			for (int32 col = 0; col < 4; ++col)
			{
				EXPECT_NEAR(actual.M[row][col], expected.M[row][col], tolerance) << "Mismatch at (" << row << ", " << col << ")";
			}
		}
	}
	TEST(USceneComponentTest, SetRelativeTransform_WhenCalled_UpdatesChildComponentToWorld)
	{
		// Arrange
		AActor actor;
		actor.Initialize();

		USceneComponent* parentComponent = new USceneComponent();
		parentComponent->Initialize(FVector(1.0f, 2.0f, 3.0f), FRotator(10.0f, 20.0f, 30.0f), FVector(1.0f, 1.0f, 1.0f));

		actor.AddRootSceneComponent(parentComponent);

		USceneComponent* childComponent = new USceneComponent();
		childComponent->Initialize(FVector(4.0f, 5.0f, 6.0f), FRotator(40.0f, 50.0f, 60.0f), FVector(2.0f, 2.0f, 2.0f));

		childComponent->AttachTo(*parentComponent);
		FTransform newRelativeTransform(FVector(7.0f, 8.0f, 9.0f), FRotator(70.0f, 80.0f, 90.0f), FVector(3.0f, 3.0f, 3.0f));
		ExpectMatrixNear(parentComponent->GetTransformMatrix(), parentComponent->GetRelativeTransform().MakeMatrix());
		ExpectMatrixNear(childComponent->GetTransformMatrix(), childComponent->GetRelativeTransform().MakeMatrix() * parentComponent->GetTransformMatrix());

		// Act
		parentComponent->SetRelativeTransform(newRelativeTransform);

		// Assert
		FMatrix expectedParentToWorld = newRelativeTransform.MakeMatrix();
		FMatrix expectedChildToWorld = childComponent->GetRelativeTransform().MakeMatrix() * expectedParentToWorld;
		ExpectMatrixNear(parentComponent->GetTransformMatrix(), expectedParentToWorld);
		ExpectMatrixNear(childComponent->GetTransformMatrix(), expectedChildToWorld);
	}
}
