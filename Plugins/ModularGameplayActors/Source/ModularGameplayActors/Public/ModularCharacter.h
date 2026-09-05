// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Character.h"

#include "ModularCharacter.generated.h"

#define UE_API MODULARGAMEPLAYACTORS_API

class UObject;

/** 可由游戏特性插件动态扩展的最小 Character 基类。 */
/** Minimal class that supports extension by game feature plugins */
UCLASS(MinimalAPI, Blueprintable)
class AModularCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	//~ 开始实现 AActor 接口。
	//~ Begin AActor Interface
	UE_API virtual void PreInitializeComponents() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ 结束实现 AActor 接口。
	//~ End AActor Interface
};

#undef UE_API
