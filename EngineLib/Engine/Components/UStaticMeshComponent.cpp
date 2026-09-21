#include "UStaticMeshComponent.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UStaticMeshComponent, UMeshComponent);
IMPLEMENT_SERIALIZATION(UStaticMeshComponent, UMeshComponent, { SetStaticMesh(StaticMesh); });

void UStaticMeshComponent::Initialize(FVector Location, FRotator Rotation, FVector Scale)
{
    USceneComponent::Initialize(Location, Rotation, Scale);
}

UMaterial* UStaticMeshComponent::GetMaterial(int32 Slot) const
{
    if (Slot < 0) return nullptr;
    if (Slot < OverrideMaterials.Num() && OverrideMaterials[Slot]) return OverrideMaterials[Slot];
    return StaticMesh ? StaticMesh->GetMaterial(Slot) : nullptr;
}

int32 UStaticMeshComponent::GetNumMaterial() const
{
    return StaticMesh ? StaticMesh->GetMaterials().Num() : 0;
}

void UStaticMeshComponent::SetStaticMesh(UStaticMeshAsset* Mesh)
{
    StaticMesh = Mesh;
    mLocalBounds = Mesh ? Mesh->GetLocalBoundingBox() : FBoundingBox(FVector(0), FVector(0));
    updateComponentToWorld();
}

UStaticMeshAsset* UStaticMeshComponent::GetStaticMesh() const { return StaticMesh; }

std::span<const FPropertyInfo> UStaticMeshComponent::GetDeclaredProperties()
{
    static const FPropertyInfo Properties[] = {REFLECT_PROPERTY(UStaticMeshComponent, StaticMesh)};
    return Properties;
}

void UStaticMeshComponent::SubmitRenderInfos(FRenderCollector& Collector) const
{
    if (!StaticMesh) return;
    const auto Model = GetRenderTransform(Collector.View.Camera);
    SubmitSelection(Collector, Model);
    if (!Collector.HasShowFlag(EEngineShowFlags::SF_Primitives) || !Collector.IsVisible(mLocalBounds.ToWorld(Model))) return;
    for (const FMeshSection& Section : StaticMesh->GetSections())
    {
        const UMaterial* Material = GetMaterial(Section.MaterialIndex);
        if (!Material) continue;
        FRenderStaticMeshInfo Info{};
        Info.WorldTransformMatrix = Model;
        Info.VertexBuffer = StaticMesh->GetVertexBuffer();
        Info.IndexBuffer = StaticMesh->GetIndexBuffer();
        Info.VertexCount = StaticMesh->GetVertexCount();
        Info.FirstIndex = Section.FirstIndex;
        Info.IndexCount = Section.IndexCount;
        Info.Color = Material->DiffuseColor;
        Info.Texture = Material->DiffuseTexture;
        Collector.StaticMeshInfos.Add(Info);
    }
}

FRenderMeshInfo UStaticMeshComponent::MakeMeshInfo(const FRenderCollector& Collector) const
{
    FRenderMeshInfo Info{};
    Info.StaticMesh = StaticMesh;
    Info.WorldTransformMatrix = GetRenderTransform(Collector.View.Camera);
    return Info;
}

void UStaticMeshComponent::RegisterPickTarget(FPickTargets& Targets) const
{
    if (StaticMesh) UPrimitiveComponent::RegisterPickTarget(Targets);
}

bool UStaticMeshComponent::RayCastComponent(const FPickingRay& Ray, const FCamera& Camera, float& OutHitT) const
{
    if (!StaticMesh) return false;
    FPickingRay localRay;
    if (!MakeLocalPickingRay(Ray, GetRenderTransform(Camera), mLocalBounds, localRay)) return false;
    const bool wasUnloaded = !StaticMesh->GetCpuGeometry();
    if (!StaticMesh->LoadCpuGeometry()) return false;
    const auto* geometry = StaticMesh->GetCpuGeometry();
    const bool hit = RayCastTriangles(localRay,
        {geometry->Vertices.GetData(), static_cast<size_t>(geometry->Vertices.Num())},
        {geometry->Indices.GetData(), static_cast<size_t>(geometry->Indices.Num())}, OutHitT);
    if (wasUnloaded) StaticMesh->UnloadCpuGeometry();
    return hit;
}
