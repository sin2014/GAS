// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerState.h"

#include "ModularPlayerState.generated.h"

#define UE_API MODULARGAMEPLAYACTORS_API

namespace EEndPlayReason { enum Type : int; }

class UObject;

/** 可由游戏特性插件动态扩展的最小 PlayerState 基类。 */
/** Minimal class that supports extension by game feature plugins */
UCLASS(MinimalAPI, Blueprintable)
class AModularPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	//~ 开始实现 AActor 接口。
	//~ Begin AActor interface
	UE_API virtual void PreInitializeComponents() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void Reset() override;
	//~ 结束实现 AActor 接口。
	//~ End AActor interface

protected:
	//~ 开始实现 APlayerState 接口。
	//~ Begin APlayerState interface
	UE_API virtual void CopyProperties(APlayerState* PlayerState);
	//~ 结束实现 APlayerState 接口。
	//~ End APlayerState interface
};

#undef UE_API
