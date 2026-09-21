
#include "PrimitiveComponent.h"
#include "Core/AssetSystem/AssetManager.h"
#include "Core/AssetSystem/Asset/StaticMeshAsset.h"
#include "Core/AssetSystem/Asset/FontAtlasAsset.h"
#include "Rendering/BuiltinAssetNames.h"

#include <format>

#include "Rendering/RenderInfo.h"
#include "Core/enum.h"
#include "Core/IO/JsonUtil.h"
#include "Editor/Console.h"
#include "Engine/Actor.h"

#include "Rendering/Primitives/Cube.h"
#include "Rendering/Primitives/Sphere.h"
#include "Rendering/Primitives/Triangle.h"
#include "Rendering/Primitives/GizmoArrow.h"
#include "Rendering/Primitives/Circle.h"
#include "Rendering/Primitives/Primitives.h"

static FBoundingBox CalculateBounds(const FVertexSimple* vertices, uint32 count);
static const FBoundingBox& GetPrimitiveLocalBounds(EPrimitive primitive);

IMPLEMENT_CLASS_WITH_PROPERTIES(UPrimitiveComponent, USceneComponent);
IMPLEMENT_SERIALIZATION(UPrimitiveComponent, USceneComponent,
	{ mLocalBounds = GetPrimitiveLocalBounds(mePrimitive); })

static bool GetPrimitiveMesh(EPrimitive ePrimitive, std::span<const FVertexSimple>& OutVertices, std::span<const uint32>& OutIndices)
{
	switch (ePrimitive)
	{
	case EPrimitive::EP_Cube:
		OutVertices = Cube_vertices;
		OutIndices = Cube_indices;
		return true;
	case EPrimitive::EP_Sphere:
		OutVertices = Sphere_vertices;
		OutIndices = Sphere_indices;
		return true;
	case EPrimitive::EP_Triangle:
		OutVertices = Triangle_vertices;
		OutIndices = Triangle_indices;
		return true;
	case EPrimitive::EP_GizmoArrow:
		OutVertices = GizmoArrow_vertices;
		OutIndices = GizmoArrow_indices;
		return true;
	case EPrimitive::EP_Circle:
		OutVertices = Circle_vertices;
		OutIndices = Circle_indices;
		return true;
	case EPrimitive::EP_BillboardQuad:
		OutVertices = Quad_vertices;
		OutIndices = Quad_indices;
		return true;
	}

	return false;
}

UPrimitiveComponent::UPrimitiveComponent()
{
}



void UPrimitiveComponent::Initialize(EPrimitive ePrimitive)
{
	Initialize(ePrimitive, FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f));
}

void UPrimitiveComponent::Initialize(EPrimitive ePrimitive, FVector location, FRotator rotation, FVector scale3D)
{
	USceneComponent::Initialize(location, rotation, scale3D);

	mePrimitive = ePrimitive;
	mLocalBounds = GetPrimitiveLocalBounds(ePrimitive);
	mColor = FLinearColor(1.f, 1.f, 1.f, 0.f);
}

void UPrimitiveComponent::Initialize(EPrimitive ePrimitive, FVector location, FRotator rotation, FVector scale3D, bool bUseTexture)
{
	USceneComponent::Initialize(location, rotation, scale3D);
	mePrimitive = ePrimitive;
	mLocalBounds = GetPrimitiveLocalBounds(ePrimitive);
	mbUseTexture = bUseTexture;
	mColor = bUseTexture
		? FLinearColor(1.f, 1.f, 1.f, 1.f)
		: FLinearColor(1.f, 1.f, 1.f, 0.f);
}

UPrimitiveComponent::~UPrimitiveComponent()
{
}

FMatrix UPrimitiveComponent::GetRenderTransform(const FCamera&) const
{
    return GetTransformMatrix();
}

void UPrimitiveComponent::RegisterPickTarget(FPickTargets& Targets) const
{
    Targets.Add(this);
}

bool UPrimitiveComponent::RayCastComponent(const FPickingRay& Ray, const FCamera& Camera, float& OutHitT) const
{
    FPickingRay localRay;
    if (!MakeLocalPickingRay(Ray, GetRenderTransform(Camera), mLocalBounds, localRay)) return false;
    std::span<const FVertexSimple> vertices;
    std::span<const uint32> indices;
    return GetPrimitiveMesh(mePrimitive, vertices, indices) && RayCastTriangles(localRay, vertices, indices, OutHitT);
}

FRenderMeshInfo UPrimitiveComponent::MakeMeshInfo(const FRenderCollector& Collector) const
{
    FRenderMeshInfo Info{};
    Info.StaticMesh = Collector.AssetManager->GetAssetAs<UStaticMeshAsset>(BuiltinAssetNames::Mesh(mePrimitive), true);
    if (mbUseTexture) Info.Texture = Collector.AssetManager->GetAssetAs<UTexture2D>(BuiltinAssetNames::Texture(mePrimitive), true);
    Info.WorldTransformMatrix = GetRenderTransform(Collector.View.Camera);
    Info.Color = mColor;
    return Info;
}

void UPrimitiveComponent::SubmitSelection(FRenderCollector& Collector, const FMatrix& Model) const
{
    if (!mOwner || mOwner != Collector.SelectedActor) return;
    auto Info = MakeMeshInfo(Collector);
    Info.WorldTransformMatrix = Model;
    Collector.SelectionInfos.Add(Info);
    if (mbShowBoundingBox && Collector.HasShowFlag(EEngineShowFlags::SF_BoundingBox))
    {
        const auto Bounds = mLocalBounds.ToWorld(Model);
        if (Collector.IsVisible(Bounds)) Collector.AddBounds(Bounds);
    }
}

void UPrimitiveComponent::SubmitRenderInfos(FRenderCollector& Collector) const
{
    const auto Model = GetRenderTransform(Collector.View.Camera);
    const auto Bounds = mLocalBounds.ToWorld(Model);
    SubmitSelection(Collector, Model);
    if (!Collector.IsVisible(Bounds)) return;
    if (!Collector.HasShowFlag(EEngineShowFlags::SF_Primitives)) return;
    auto Info = MakeMeshInfo(Collector);
    if (mbUseTexture) Collector.MeshInfos.Add(Info);
    else Collector.InstancedMeshInfos.Add(Info);
}

static FBoundingBox CalculateBounds(
	const FVertexSimple* vertices,
	uint32 count)
{
	FBoundingBox result{};
	result.Min = vertices[0].GetPosition();
	result.Max = result.Min;

	for (uint32 i = 1; i < count; ++i)
	{
		const FVector position = vertices[i].GetPosition();

		result.Min.x = min(result.Min.x, position.x);
		result.Min.y = min(result.Min.y, position.y);
		result.Min.z = min(result.Min.z, position.z);

		result.Max.x = max(result.Max.x, position.x);
		result.Max.y = max(result.Max.y, position.y);
		result.Max.z = max(result.Max.z, position.z);
	}

	return result;
}

static const FBoundingBox& GetPrimitiveLocalBounds(EPrimitive primitive)
{
	switch (primitive)
	{
	case EPrimitive::EP_Cube:
	{
		static const FBoundingBox bounds =
			CalculateBounds(Cube_vertices, _countof(Cube_vertices));
		return bounds;
	}
	case EPrimitive::EP_Sphere:
	{
		static const FBoundingBox bounds =
			CalculateBounds(Sphere_vertices, _countof(Sphere_vertices));
		return bounds;
	}
	case EPrimitive::EP_Triangle:
	{
		static const FBoundingBox bounds =
			CalculateBounds(Triangle_vertices, _countof(Triangle_vertices));
		return bounds;
	}
	case EPrimitive::EP_GizmoArrow:
	{
		static const FBoundingBox bounds =
			CalculateBounds(GizmoArrow_vertices, _countof(GizmoArrow_vertices));
		return bounds;
	}
	case EPrimitive::EP_Circle:
	{
		static const FBoundingBox bounds =
			CalculateBounds(Circle_vertices, _countof(Circle_vertices));
		return bounds;
	}
	case EPrimitive::EP_BillboardQuad:
	{
		static const FBoundingBox bounds =
			CalculateBounds(Quad_vertices, _countof(Quad_vertices));
		return bounds;
	}
	}

	static const FBoundingBox emptyBounds{};
	return emptyBounds;
}


std::span<const FPropertyInfo>
UPrimitiveComponent::GetDeclaredProperties()
{
	static const FPropertyInfo Properties[] =
	{
		REFLECT_PROPERTY(
			UPrimitiveComponent,
			mePrimitive),

		REFLECT_PROPERTY(
			UPrimitiveComponent,
			mbUseTexture),

		REFLECT_PROPERTY(
			UPrimitiveComponent,
			mbShowBoundingBox),

		REFLECT_PROPERTY(
			UPrimitiveComponent,
			mColor),
	};

	return Properties;
}
