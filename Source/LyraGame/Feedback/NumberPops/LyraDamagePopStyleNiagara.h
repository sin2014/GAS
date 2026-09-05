// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "LyraDamagePopStyleNiagara.generated.h"

class UNiagaraSystem;

/* 定义伤害数字表现所使用的 NiagaraSystem 及其数据数组参数名。 */
/*PopStyle is used to define what Niagara asset should be used for the Damage System representation*/
UCLASS()
class ULyraDamagePopStyleNiagara : public UDataAsset
{
	GENERATED_BODY()

public:

	// NiagaraSystem 中接收伤害位置与数值数组的参数名。
	//Name of the Niagra Array to set the Damage informations
	UPROPERTY(EditDefaultsOnly, Category="DamagePop")
	FName NiagaraArrayName;

	// 用于显示伤害数字的 NiagaraSystem。
	//Niagara System used to display the damages
	UPROPERTY(EditDefaultsOnly, Category="DamagePop")
	TObjectPtr<UNiagaraSystem> TextNiagara;
};
