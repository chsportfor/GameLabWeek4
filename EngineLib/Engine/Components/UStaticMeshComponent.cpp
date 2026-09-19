#include "UStaticMeshComponent.h"

IMPLEMENT_CLASS_WITH_PROPERTIES(UStaticMeshComponent, UMeshComponent)

IMPLEMENT_SERIALIZATION(UStaticMeshComponent, UMeshComponent,
	{
		//if (!ObjStaticMeshAsset.empty())
		//{
			//StaticMesh = FObjManager::Get().LoadObjStaticMesh(ObjStaticMeshAsset);
		//}
	}
);

FStaticMaterial* UStaticMeshComponent::GetMaterial(uint32 slotIndex) const
{

}

uint32 UStaticMeshComponent::GetNumMaterial() const
{
	return StaticMesh ? StaticMesh->GetNumMaterial() : 0;
}

std::span<const FPropertyInfo>
UStaticMeshComponent::GetDeclaredProperties()
{
	static const FPropertyInfo Properties[] =
	{
		REFLECT_PROPERTY(
			UStaticMeshComponent,
			ObjStaticMeshAsset)
	};
	return Properties;
}
