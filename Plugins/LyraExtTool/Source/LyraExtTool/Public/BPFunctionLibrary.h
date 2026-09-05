// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "BPFunctionLibrary.generated.h"

class UMaterialInterface;
class UObject;
class UStaticMesh;
struct FFrame;

/** 为蓝图提供批量修改静态网格资产材质槽的编辑工具函数。 */
/**
 *
 */

UCLASS()
class UBPFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

    UFUNCTION(BlueprintCallable, Category="LyraExt")
    static bool ChangeMeshMaterials(TArray<UStaticMesh*> Mesh, UMaterialInterface* Material);
};
