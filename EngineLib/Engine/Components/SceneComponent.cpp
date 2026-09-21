#include "SceneComponent.h"

#include <format>

#include "Core/Math/Transform.h"
#include "Core/IO/JsonUtil.h"
#include "Engine/Actor.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(USceneComponent, UActorComponent);

void USceneComponent::Initialize(FVector location, FRotator rotation, FVector scale3D)
{
	UActorComponent::Initialize();

	mRelativeLocation = location;
	mRelativeRotation = rotation;
	mRelativeScale3D = scale3D;
	updateComponentToWorld();
}

USceneComponent::~USceneComponent()
{
}

void USceneComponent::SerializeClass(json::JSON& outJson) const
{
	UActorComponent::SerializeClass(outJson);

	for (const FPropertyInfo& Property : ClassInfo.DeclaredProperties)
	{
		Property.Serialize(
			Property,
			this,
			outJson["Properties"]);
	}
}

void USceneComponent::DeserializeClass(const json::JSON& inJson)
{
	UActorComponent::DeserializeClass(inJson);
	const json::JSON& propertiesJson = inJson.at("Properties");

	for (const FPropertyInfo& Property : ClassInfo.DeclaredProperties)
	{
		Property.Deserialize(
			Property,
			this,
			propertiesJson);
	}

	updateComponentToWorld();
}

// Attach this component to a parent scene component
bool USceneComponent::AttachTo(USceneComponent& parent)
{
	if (&parent == this)
	{
		return false;
	}
	if (mParent == &parent)
	{
		return true; // Already attached to the same parent
	}
	if (parent.isChildOf(*this))
	{
		return false; // Prevent circular attachment
	}

	if (mOwner && mOwner != parent.GetOwner())
	{
		mOwner->RemoveComponent(this);

		// Add this component to the owner actor's component list
		parent.GetOwner()->AddComponent(this);
	}

	if (mParent && mParent != &parent)
	{
		mParent->RemoveChild(*this);
	}

	mParent = &parent;
	parent.mChildren.Add(this);

	updateComponentToWorld(parent.GetTransformMatrix());

	return true;
}

bool USceneComponent::RemoveChild(USceneComponent& child)
{
	if (&child == this)
	{
		return false; // Cannot remove self
	}
	if (child.mParent != this)
	{
		return false; // The specified child is not a child of this component
	}

	if (!mChildren.Remove(&child))
	{
		return false;
	}

	child.mParent = nullptr;

	// Reset the child's transform to world space
	child.updateComponentToWorld();
	return true;
}

void USceneComponent::DetachFromParent()
{
	if (mParent)
	{
		mParent->RemoveChild(*this);
	}
}

void  USceneComponent::DetachAllChildren()
{
	while (mChildren.Num() > 0)
	{
		RemoveChild(*mChildren[0]);
	}
}

FBoundingBox USceneComponent::CalcBounds(const FMatrix& LocalToWorld) const
{
	return FBoundingBox{LocalToWorld.GetTranslation(), LocalToWorld.GetTranslation() };
}

FVector USceneComponent::GetRelativeLocation() const
{
	return mRelativeLocation;
}

void USceneComponent::SetRelativeLocation(FVector location)
{
	mRelativeLocation = location;
	updateComponentToWorld();
}

FRotator USceneComponent::GetRelativeRotation() const
{
	return mRelativeRotation;
}

void USceneComponent::SetRelativeRotation(FRotator rotation)
{
	mRelativeRotation = rotation;
	updateComponentToWorld();
}

void USceneComponent::SetRelativeRotation(FQuat rotation)
{
	mRelativeRotation = rotation.Rotator();
	updateComponentToWorld();
}

FVector USceneComponent::GetRelativeScale3D() const
{
	return mRelativeScale3D;
}

void USceneComponent::SetRelativeScale3D(FVector scale)
{
	mRelativeScale3D = scale;
	updateComponentToWorld();
}

FMatrix USceneComponent::GetTransformMatrix() const
{
	return mComponentToWorld;
}

FTransform USceneComponent::GetRelativeTransform() const
{
	return FTransform(mRelativeLocation, mRelativeRotation, mRelativeScale3D);
}

void USceneComponent::SetRelativeTransform(const FTransform& transform)
{
	mRelativeLocation = transform.GetLocation();
	mRelativeRotation = transform.GetRotator();
	mRelativeScale3D = transform.GetScale();
	updateComponentToWorld();
}

void USceneComponent::updateComponentToWorld(const FMatrix& parentTransform)
{
	mComponentToWorld = FTransform(mRelativeLocation, mRelativeRotation, mRelativeScale3D).MakeMatrix() * parentTransform;

	for (USceneComponent* child : mChildren)
	{
		child->updateComponentToWorld(mComponentToWorld);
	}
}

void USceneComponent::updateComponentToWorld()
{
	updateComponentToWorld(mParent ? mParent->GetTransformMatrix() : FMatrix::Identity);
}


bool USceneComponent::isChildOf(const USceneComponent& component) const
{
	const USceneComponent* current = mParent;
	while (current)
	{
		if (current == &component)
		{
			return true;
		}
		current = current->mParent;
	}
	return false;
}

std::span<const FPropertyInfo> USceneComponent::GetDeclaredProperties()
{
	static const FPropertyInfo Properties[] =
	{
		REFLECT_PROPERTY(
			USceneComponent,
			mRelativeLocation),
		REFLECT_PROPERTY(
			USceneComponent,
			mRelativeRotation),
		REFLECT_PROPERTY(
			USceneComponent,
			mRelativeScale3D)
	};

	return Properties;
}
