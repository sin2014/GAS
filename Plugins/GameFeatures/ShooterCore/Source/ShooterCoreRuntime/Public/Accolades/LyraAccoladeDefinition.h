// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"

#include "LyraAccoladeDefinition.generated.h"

class UObject;
class USoundBase;

USTRUCT(BlueprintType)
struct FLyraAccoladeDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 展示给玩家的嘉奖文本。
	// The message to display
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	// 嘉奖显示时异步加载并播放的声音。
	// The sound to play
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<USoundBase> Sound;

	// 嘉奖显示时异步加载的图标资源。
	// The icon to display	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayThumbnail="true", AllowedClasses="/Script/Engine.Texture,/Script/Engine.MaterialInterface,/Script/Engine.SlateTextureAtlasInterface", DisallowedClasses="/Script/MediaAssets.MediaTexture"))
	TSoftObjectPtr<UObject> Icon;

	// 此嘉奖保持显示的时长，单位为秒。
	// Duration (in seconds) to display this accolade
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DisplayDuration = 1.0f;

	// 指定由哪个位置标签的 Host Widget 展示此嘉奖。
	// Location to display this accolade
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag LocationTag;

	// 用于标识此嘉奖类别的 GameplayTag。
	// Tags associated with this accolade
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer AccoladeTags;

	// 此嘉奖入队时，移除已显示或待显示列表中命中这些标签的旧嘉奖，例如 Triple Elim 可抑制尚未结束的 Double Elim。
	// When this accolade is displayed, any existing displayed/pending accolades with any of
	// these tags will be removed (e.g., getting a triple-elim will suppress a double-elim)
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer CancelAccoladesWithTag;
};

/** 旧版基于 DataAsset 的嘉奖资源定义，保存声音、图标及互斥标签信息。 */
/**
 * 
 */
UCLASS(BlueprintType)
class ULyraAccoladeDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// 嘉奖显示时播放的声音。
	// The sound to play
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<USoundBase> Sound;

	// 嘉奖显示时使用的图标资源。
	// The icon to display	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayThumbnail="true", AllowedClasses="/Script/Engine.Texture,/Script/Engine.MaterialInterface,/Script/Engine.SlateTextureAtlasInterface", DisallowedClasses="/Script/MediaAssets.MediaTexture"))
	TObjectPtr<UObject> Icon;

	// 用于标识此嘉奖类别的 GameplayTag。
	// Tags associated with this accolade
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer AccoladeTags;

	// 此嘉奖显示时，移除命中这些标签的已显示或待显示嘉奖。
	// When this accolade is displayed, any existing displayed/pending accolades with any of
	// these tags will be removed (e.g., getting a triple-elim will suppress a double-elim)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer CancelAccoladesWithTag;
};
