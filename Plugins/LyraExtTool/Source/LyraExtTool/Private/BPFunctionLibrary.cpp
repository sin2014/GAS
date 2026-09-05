// Copyright Epic Games, Inc. All Rights Reserved.

#include "BPFunctionLibrary.h"

#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BPFunctionLibrary)

class UMaterialInterface;

// 将每个静态网格的全部材质槽替换为同一材质，并通过 Modify/PostEditChange 记录编辑器事务和刷新资产；输入不会判空且始终返回 true。
bool UBPFunctionLibrary::ChangeMeshMaterials(TArray<UStaticMesh*> Mesh, UMaterialInterface* Material)
{

	for (int i = 0; i < Mesh.Num(); i++)
	{
		Mesh[i]->Modify();
		TArray<FStaticMaterial>& Mats = Mesh[i]->GetStaticMaterials();
		for (int j = 0; j < Mats.Num(); j++)
		{
			Mats[j].MaterialInterface = Material;

		}
		Mesh[i]->PostEditChange();
	}
	return true;
}
