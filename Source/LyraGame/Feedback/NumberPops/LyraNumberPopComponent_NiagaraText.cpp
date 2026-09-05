// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraNumberPopComponent_NiagaraText.h"

#include "Feedback/NumberPops/LyraNumberPopComponent.h"
#include "LyraDamagePopStyleNiagara.h"
#include "LyraLogChannels.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraNumberPopComponent_NiagaraText)

// 构造 Niagara 数字组件，运行时 NiagaraComponent 延迟到首个请求时创建。
ULyraNumberPopComponent_NiagaraText::ULyraNumberPopComponent_NiagaraText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{


}

// 复用 Style 指定的 Niagara 系统，将暴击编码到数值符号，并把位置与伤害追加到 Niagara 数组。
void ULyraNumberPopComponent_NiagaraText::AddNumberPop(const FLyraNumberPopRequest& NewRequest)
{
	int32 LocalDamage = NewRequest.NumberToDisplay;

	// 将暴击伤害编码为负数，使 Niagara 端可在同一数值通道中区分暴击与普通命中。
	//Change Damage to negative to differentiate Critial vs Normal hit
	if (NewRequest.bIsCriticalDamage)
	{
		LocalDamage *= -1;
	}

	// 首次收到请求时创建并激活用于显示数字的 NiagaraComponent，之后持续复用。
	//Add a NiagaraComponent if we don't already have one
	if (!NiagaraComp)
	{
		NiagaraComp = NewObject<UNiagaraComponent>(GetOwner());
		if (Style != nullptr)
		{
			NiagaraComp->SetAsset(Style->TextNiagara);
			NiagaraComp->bAutoActivate = false;
			
		}
		NiagaraComp->SetupAttachment(nullptr);
		check(NiagaraComp);
		NiagaraComp->RegisterComponent();
	}


	NiagaraComp->Activate(false);
	NiagaraComp->SetWorldLocation(NewRequest.WorldLocation);

	UE_LOG(LogLyra, Log, TEXT("DamageHit location : %s"), *(NewRequest.WorldLocation.ToString()));
	// 将本次伤害追加到 Niagara 数组；FVector4 的 XYZ 保存世界位置，W 保存编码后的伤害值。
	//Add Damage information to the current Niagara list - Damage informations are packed inside a FVector4 where XYZ = Position, W = Damage
	TArray<FVector4> DamageList = UNiagaraDataInterfaceArrayFunctionLibrary::GetNiagaraArrayVector4(NiagaraComp, Style->NiagaraArrayName);
	DamageList.Add(FVector4(NewRequest.WorldLocation.X, NewRequest.WorldLocation.Y, NewRequest.WorldLocation.Z, LocalDamage));
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(NiagaraComp, Style->NiagaraArrayName, DamageList);
	
}

