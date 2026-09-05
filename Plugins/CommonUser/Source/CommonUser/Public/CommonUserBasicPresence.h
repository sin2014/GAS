// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "CommonUserBasicPresence.generated.h"

#define UE_API COMMONUSER_API

class UCommonSessionSubsystem;
enum class ECommonSessionInformationState : uint8;

//////////////////////////////////////////////////////////////////////
// UCommonUserBasicPresence

/**
 * 将 CommonSessionSubsystem 的会话状态转换并推送到在线 Presence 接口。
 * 该实现只用于演示从会话信息生成基础 Presence，并不是完整的 Rich Presence 系统。
 */
/**
 * This subsystem plugs into the session subsystem and pushes its information to the presence interface.
 * It is not intended to be a full featured rich presence implementation, but can be used as a proof-of-concept
 * for pushing information from the session subsystem to the presence system
 */
UCLASS(MinimalAPI, BlueprintType, Config = Engine)
class UCommonUserBasicPresence : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UE_API UCommonUserBasicPresence();


	/** 初始化子系统实例并绑定会话信息变化。 */
	/** Implement this for initialization of instances of the system */
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 子系统反初始化入口；当前实现不执行额外清理。 */
	/** Implement this for deinitialization of instances of the system */
	UE_API virtual void Deinitialize() override;

	/** 总开关；为 false 时完全停止由会话状态推送 Presence。 */
	/** False is a general purpose killswitch to stop this class from pushing presence*/
	UPROPERTY(Config)
	bool bEnableSessionsBasedPresence = false;

	/** 将“游戏中”Presence 状态映射为后端识别的键。 */
	/** Maps the presence status "In-game" to a backend key*/
	UPROPERTY(Config)
	FString PresenceStatusInGame;

	/** 将“主菜单”Presence 状态映射为后端识别的键。 */
	/** Maps the presence status "Main Menu" to a backend key*/
	UPROPERTY(Config)
	FString PresenceStatusMainMenu;

	/** 将“匹配中”Presence 状态映射为后端识别的键。 */
	/** Maps the presence status "Matchmaking" to a backend key*/
	UPROPERTY(Config)
	FString PresenceStatusMatchmaking;

	/** 将 Rich Presence 的“游戏模式”字段映射为后端键。 */
	/** Maps the "Game Mode" rich presence entry to a backend key*/
	UPROPERTY(Config)
	FString PresenceKeyGameMode;

	/** 将 Rich Presence 的“地图名称”字段映射为后端键。 */
	/** Maps the "Map Name" rich presence entry to a backend key*/
	UPROPERTY(Config)
	FString PresenceKeyMapName;

	UE_API void OnNotifySessionInformationChanged(ECommonSessionInformationState SessionStatus, const FString& GameMode, const FString& MapName);
	UE_API FString SessionStateToBackendKey(ECommonSessionInformationState SessionStatus);
};

#undef UE_API
