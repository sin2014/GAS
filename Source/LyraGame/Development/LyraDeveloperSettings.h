// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPath.h"
#include "LyraDeveloperSettings.generated.h"

struct FPropertyChangedEvent;

class ULyraExperienceDefinition;

UENUM()
enum class ECheatExecutionTime
{
	// CheatManager 创建后立即执行。
	// When the cheat manager is created
	OnCheatManagerCreated,

	// 玩家 Controller 接管 Pawn 后执行。
	// When a pawn is possessed by a player
	OnPlayerPawnPossession
};

USTRUCT()
struct FLyraCheatToRun
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	ECheatExecutionTime Phase = ECheatExecutionTime::OnPlayerPawnPossession;

	UPROPERTY(EditAnywhere)
	FString Cheat;
};

/**
 * 编辑器用户级开发设置，用于覆盖 PIE Experience、Bot、流程、性能和自动作弊行为。
 */
/**
 * Developer settings / editor cheats
 */
UCLASS(config=EditorPerProjectUserSettings, MinimalAPI)
class ULyraDeveloperSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	ULyraDeveloperSettings();

	//~UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	//~End of UDeveloperSettings interface

public:
	// PIE 强制使用的 Experience；未设置时沿用当前地图 WorldSettings 的默认值。
	// The experience override to use for Play in Editor (if not set, the default for the world settings of the open map will be used)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category=Lyra, meta=(AllowedTypes="LyraExperienceDefinition"))
	FPrimaryAssetId ExperienceOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category=LyraBots, meta=(InlineEditConditionToggle))
	bool bOverrideBotCount = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category=LyraBots, meta=(EditCondition=bOverrideBotCount))
	int32 OverrideNumPlayerBotsToSpawn = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category=LyraBots)
	bool bAllowPlayerBotsToAttack = true;

	// PIE 中是否执行完整比赛流程；关闭时跳过等待玩家等阶段以加快迭代。
	// Do the full game flow when playing in the editor, or skip 'waiting for player' / etc... game phases?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category=Lyra)
	bool bTestFullGameFlowInPIE = false;

	/**
	* 是否在最近输入设备不是手柄时仍强制播放力反馈。
	* Lyra 默认仅在最近使用手柄输入时播放力反馈。
	*/
	/**
	* Should force feedback effects be played, even if the last input device was not a gamepad?
	* The default behavior in Lyra is to only play force feedback if the most recent input device was a gamepad.
	*/
	UPROPERTY(config, EditAnywhere, Category = Lyra, meta = (ConsoleVariable = "LyraPC.ShouldAlwaysPlayForceFeedback"))
	bool bShouldAlwaysPlayForceFeedback = false;

	// PIE 中是否跳过纯外观背景资源加载，以提高编辑器迭代速度。
	// Should game logic load cosmetic backgrounds in the editor or skip them for iteration speed?
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, config, Category=Lyra)
	bool bSkipLoadingCosmeticBackgroundsInPIE = false;

	// PIE 中按指定生命周期阶段自动执行的作弊命令列表。
	// List of cheats to auto-run during 'play in editor'
	UPROPERTY(config, EditAnywhere, Category=Lyra)
	TArray<FLyraCheatToRun> CheatsToRun;
	
	// 是否记录 GameplayMessageSubsystem 广播的消息。
	// Should messages broadcast through the gameplay message subsystem be logged?
	UPROPERTY(config, EditAnywhere, Category=GameplayMessages, meta=(ConsoleVariable="GameplayMessageSubsystem.LogMessages"))
	bool LogGameplayMessages = false;

#if WITH_EDITORONLY_DATA
	/** 显示在编辑器工具栏中的常用地图列表。 */
	/** A list of common maps that will be accessible via the editor detoolbar */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category=Maps, meta=(AllowedClasses="/Script/Engine.World"))
	TArray<FSoftObjectPath> CommonEditorMaps;
#endif
	
#if WITH_EDITOR
public:
	// PIE 启动时提示仍在生效的 Experience 覆盖或自动作弊设置。
	// Called by the editor engine to let us pop reminder notifications when cheats are active
	LYRAGAME_API void OnPlayInEditorStarted() const;

private:
	void ApplySettings();
#endif

public:
	//~UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
	virtual void PostInitProperties() override;
#endif
	//~End of UObject interface
};
