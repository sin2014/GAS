// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UserSettings/EnhancedInputUserSettings.h"
#include "PlayerMappableKeySettings.h"

#include "LyraInputUserSettings.generated.h"

#define UE_API LYRAGAME_API

/**
 * Lyra 的输入用户设置，随 SharedSettings 一同序列化，并可通过 Serialize 接入云存档。
 */
/** 
 * Custom settings class for any input related settings for the Lyra game.
 * This will be serialized out at the same time as the Lyra Shared Settings and is
 * compatible with cloud saves through by calling the "Serialize" function.
 */
UCLASS(MinimalAPI)
class ULyraInputUserSettings : public UEnhancedInputUserSettings
{
	GENERATED_BODY()
public:
	//~ Begin UEnhancedInputUserSettings interface
	UE_API virtual void ApplySettings() override;
	//~ End UEnhancedInputUserSettings interface

	// 可在此添加切换/按住行为、瞄准灵敏度等额外输入设置。
	// Add any additional Input Settings here!
	// Some ideas could be:
	// - "toggle vs. hold" to trigger in game actions
	// - aim sensitivity should go here
	// - etc

	// 需要持久化的属性必须添加 SaveGame 元数据，才能随设置保存序列化。
	// Make sure to mark your properties with the "SaveGame" metadata to have them serialize when saved
	//UPROPERTY(SaveGame, BlueprintReadWrite, Category="Enhanced Input|User Settings")
	// bool bSomeExampleProperty;
};

/**
 * 每个可重映射按键条目的扩展元数据，可供设置 UI、InputTrigger 和其他输入逻辑查询。
 */
/**
 * Player Mappable Key settings are settings that are accessible per-action key mapping.
 * This is where you could place additional metadata that may be used by your settings UI,
 * input triggers, or other places where you want to know about a key setting.
 */
UCLASS(MinimalAPI)
class ULyraPlayerMappableKeySettings : public UPlayerMappableKeySettings
{
	GENERATED_BODY()
	
public:

	/** 返回设置界面为该按键映射显示的提示文本。 */
	/** Returns the tooltip that should be displayed on the settings screen for this key */
	UE_API const FText& GetTooltipText() const;

protected:
	/** 该 Action 在按键设置界面显示时使用的提示文本。 */
	/** The tooltip that should be associated with this action when displayed on the settings screen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta=(AllowPrivateAccess=true))
	FText Tooltip = FText::GetEmpty();
};

#undef UE_API
