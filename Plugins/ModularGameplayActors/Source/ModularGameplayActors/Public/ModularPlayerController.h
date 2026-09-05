// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerController.h"

#include "ModularPlayerController.generated.h"

#define UE_API MODULARGAMEPLAYACTORS_API

class UObject;

/** 可由游戏特性插件动态扩展的最小 PlayerController 基类。 */
/** Minimal class that supports extension by game feature plugins */
UCLASS(MinimalAPI, Blueprintable)
class AModularPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	//~ 开始实现 AActor 接口。
	//~ Begin AActor interface
	UE_API virtual void PreInitializeComponents() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ 结束实现 AActor 接口。
	//~ End AActor interface

	//~ 开始实现 APlayerController 接口。
	//~ Begin APlayerController interface
	UE_API virtual void ReceivedPlayer() override;
	UE_API virtual void PlayerTick(float DeltaTime) override;
	//~ 结束实现 APlayerController 接口。
	//~ End APlayerController interface
};

#undef UE_API
