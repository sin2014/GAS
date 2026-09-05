// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraNumberPopComponent.h"

#include "LyraNumberPopComponent_NiagaraText.generated.h"

class ULyraDamagePopStyleNiagara;
class UNiagaraComponent;
class UObject;

UCLASS(Blueprintable)
class ULyraNumberPopComponent_NiagaraText : public ULyraNumberPopComponent
{
	GENERATED_BODY()

public:

	ULyraNumberPopComponent_NiagaraText(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ULyraNumberPopComponent interface
	virtual void AddNumberPop(const FLyraNumberPopRequest& NewRequest) override;
	//~End of ULyraNumberPopComponent interface

protected:
	
	TArray<int32> DamageNumberArray;

	/** 按顺序尝试匹配传入请求的 Niagara 数字样式。 */
	/** Style patterns to attempt to apply to the incoming number pops */
	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Style")
	TObjectPtr<ULyraDamagePopStyleNiagara> Style;

	// 复用的 NiagaraComponent，用于渲染累计提交的伤害数字。
	//Niagara Component used to display the damage
	UPROPERTY(EditDefaultsOnly, Category = "Number Pop|Style")
	TObjectPtr<UNiagaraComponent> NiagaraComp;
};
