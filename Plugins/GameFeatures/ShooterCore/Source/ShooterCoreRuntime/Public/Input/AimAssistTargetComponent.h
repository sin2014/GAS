// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/CapsuleComponent.h"
#include "GameplayTagContainer.h"
#include "IAimAssistTargetInterface.h"

#include "AimAssistTargetComponent.generated.h"

#define UE_API SHOOTERCORERUNTIME_API

class UObject;

/**
 * 可附加到任意 Actor 的 Aim Assist 目标胶囊组件，向目标管理器提供命中形状、激活状态和筛选标签。
 */
/**
 * This component can be added to any actor to have it register with the Aim Assist Target Manager.
 */
UCLASS(MinimalAPI, BlueprintType, meta=(BlueprintSpawnableComponent))
class UAimAssistTargetComponent : public UCapsuleComponent, public IAimAssistTaget
{
	GENERATED_BODY()

public:
	
	// IAimAssistTaget 接口开始。
	//~ Begin IAimAssistTaget interface
	UE_API virtual void GatherTargetOptions(OUT FAimAssistTargetOptions& TargetData) override;
	// IAimAssistTaget 接口结束。
	//~ End IAimAssistTaget interface
	
protected:
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FAimAssistTargetOptions TargetData {};
};

#undef UE_API
