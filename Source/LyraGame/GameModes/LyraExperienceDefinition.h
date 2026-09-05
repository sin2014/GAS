// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "LyraExperienceDefinition.generated.h"

class UGameFeatureAction;
class ULyraPawnData;
class ULyraExperienceActionSet;

/** 定义一套可加载的 Gameplay Experience，包括 PawnData、GameFeature 依赖、Action 和附加 ActionSet。 */
/**
 * Definition of an experience
 */
UCLASS(BlueprintType, Const)
class ULyraExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	ULyraExperienceDefinition();

	//~UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~End of UObject interface

	//~UPrimaryDataAsset interface
#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif
	//~End of UPrimaryDataAsset interface

public:
	// 本 Experience 运行期间要求保持激活的 GameFeature 插件名称。
	// List of Game Feature Plugins this experience wants to have active
	UPROPERTY(EditDefaultsOnly, Category = Gameplay)
	TArray<FString> GameFeaturesToEnable;

	/** 玩家未被单独指定 PawnData 时使用的默认 Pawn 配置。 */
	/** The default pawn class to spawn for players */
	//@TODO：评估是否改为软引用。
	//@TODO: Make soft?
	UPROPERTY(EditDefaultsOnly, Category=Gameplay)
	TObjectPtr<const ULyraPawnData> DefaultPawnData;

	// 随 Experience 执行注册、加载、激活、停用和卸载生命周期的 GameFeatureAction。
	// List of actions to perform as this experience is loaded/activated/deactivated/unloaded
	UPROPERTY(EditDefaultsOnly, Instanced, Category="Actions")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	// 组合进本 Experience 的附加 ActionSet，用于复用插件依赖与 Action 集合。
	// List of additional action sets to compose into this experience
	UPROPERTY(EditDefaultsOnly, Category=Gameplay)
	TArray<TObjectPtr<ULyraExperienceActionSet>> ActionSets;
};
