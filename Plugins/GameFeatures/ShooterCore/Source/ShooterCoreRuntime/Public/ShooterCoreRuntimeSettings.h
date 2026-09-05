// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"

#include "ShooterCoreRuntimeSettings.generated.h"

class UObject;

/** ShooterCoreRuntime 插件的项目级运行时设置。 */
/** Runtime settings specific to the ShooterCoreRuntime plugin */
UCLASS(config = Game, defaultconfig)
class UShooterCoreRuntimeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UShooterCoreRuntimeSettings(const FObjectInitializer& Initializer);

	ECollisionChannel GetAimAssistCollisionChannel() const { return AimAssistCollisionChannel; }

private:

	/** 指定 Aim Assist 候选体积查询所使用的碰撞 Trace Channel，目标组件必须在此通道上可被检测。 */
	/**
	 * What trace channel should be used to find available targets for Aim Assist.
	 * @see UAimAssistTargetManagerComponent::GetVisibleTargets
	 */
	UPROPERTY(config, EditAnywhere, Category = "Aim Assist")
	TEnumAsByte<ECollisionChannel> AimAssistCollisionChannel = ECollisionChannel::ECC_EngineTraceChannel5;
};
