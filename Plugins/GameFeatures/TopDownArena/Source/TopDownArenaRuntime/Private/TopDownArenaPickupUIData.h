// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectUIData.h"
#include "UObject/ObjectPtr.h"

#include "TopDownArenaPickupUIData.generated.h"

class UNiagaraSystem;
class UObject;
class USoundBase;
class UTexture2D;

// 为 TopDownArena 拾取物提供文本、图标以及可覆盖的视听反馈资源。
// Icon and display name for pickups in the top-down arena game
UCLASS(BlueprintType)
class UTopDownArenaPickupUIData : public UGameplayEffectUIData
{
	GENERATED_BODY()

public:

	// 拾取物的完整说明文本。
	// The full description of the pickup
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Data, meta=(MultiLine="true"))
	FText Description;

	// 拾取时显示在玩家名称旁提示中的简短说明。
	// The short description of the pickup (displayed by the player name when picked up)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Data, meta=(MultiLine="true"))
	FText ShortDescriptionForToast;
	
	// 在世界内表现该拾取物所使用的图标纹理。
	// The icon material used to show the pickup in the world
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Data)
	TObjectPtr<UTexture2D> IconTexture;

	// 用于替换默认拾取特效的 Niagara 系统。
	// The pickup VFX override
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Data)
	TObjectPtr<UNiagaraSystem> PickupVFX;

	// 用于替换默认拾取音效的声音资源；未设置时播放默认音效。
	// The pickup SFX override (if not set, a default will play)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Data)
	TObjectPtr<USoundBase> PickupSFX;
};

