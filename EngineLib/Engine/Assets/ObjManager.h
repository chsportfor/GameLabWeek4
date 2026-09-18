#pragma once

#include "Core/Container/TMap.h"
#include "StaticMesh.h"
#include "Engine/Components/UStaticMesh.h"
#include "Core/Object/ObjectIterator.h"
#include "Core/Object/ObjectFactory.h"

class FObjManager
{
private:
	static TMap<FString, FStaticMesh*> ObjStaticMeshMap;

public:
	static FStaticMesh* LoadObjStaticMeshAsset(const std::string& PathFileName) {

		/*if (It = ObjStaticMeshMap.Find(FileName))
		{
			return It;
		}
		*/
		FStaticMesh* NewFStaticMesh;
		// OBJ Parsing and create a new FStaticMesh
		ObjStaticMeshMap[PathFileName] = NewFStaticMesh;
		return NewFStaticMesh;
	}

	static UStaticMesh* LoadObjStaticMesh(const std::string& PathFileName) {
		for (FObjectIterator<UStaticMesh> It; It; ++It)
		{
			// UStaticMesh* StaticMesh = *It;
			// if (StaticMesh->GetAssetPathFileName() == PathFileName)
			// return It;
		}

		FStaticMesh* Asset = FObjManager::LoadObjStaticMeshAsset(PathFileName);
		UStaticMesh* StaticMesh = FObjectFactory::ConstructObject<UStaticMesh>();
		//StaticMesh->SetStaticMeshAsset(Asset);
	}
};
