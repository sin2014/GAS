// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/WorldSettings.h"
#include "LyraWorldSettings.generated.h"

#define UE_API LYRAGAME_API

class ULyraExperienceDefinition;

/**
 * Lyra 的 WorldSettings，主要用于为地图指定服务器打开时默认加载的 Gameplay Experience。
 */
/**
 * The default world settings object, used primarily to set the default gameplay experience to use when playing on this map
 */
UCLASS(MinimalAPI)
class ALyraWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:

	UE_API ALyraWorldSettings(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	UE_API virtual void CheckForErrors() override;
#endif

public:
	// 返回服务器打开本地图时使用的默认 Experience；用户界面发起的 Experience 可覆盖该值。
	// Returns the default experience to use when a server opens this map if it is not overridden by the user-facing experience
	UE_API FPrimaryAssetId GetDefaultGameplayExperience() const;

protected:
	// 地图级默认 Experience 资产；仅在没有更高优先级覆盖时使用。
	// The default experience to use when a server opens this map if it is not overridden by the user-facing experience
	UPROPERTY(EditDefaultsOnly, Category=GameMode)
	TSoftClassPtr<ULyraExperienceDefinition> DefaultGameplayExperience;

public:

#if WITH_EDITORONLY_DATA
	// 标记该关卡是否属于前端或其他独立体验。
	// 启用后，在编辑器中 Play 会强制使用 Standalone NetMode。
	// Is this level part of a front-end or other standalone experience?
	// When set, the net mode will be forced to Standalone when you hit Play in the editor
	UPROPERTY(EditDefaultsOnly, Category=PIE)
	bool ForceStandaloneNetMode = false;
#endif
};

#undef UE_API
