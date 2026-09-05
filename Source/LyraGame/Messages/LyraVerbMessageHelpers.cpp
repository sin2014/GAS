// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraVerbMessageHelpers.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "Messages/LyraVerbMessage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraVerbMessageHelpers)

//////////////////////////////////////////////////////////////////////
// FLyraVerbMessage

// 通过 UScriptStruct 文本导出把 Verb 消息全部字段序列化为可读调试字符串。
FString FLyraVerbMessage::ToString() const
{
	FString HumanReadableMessage;
	FLyraVerbMessage::StaticStruct()->ExportText(/*out*/ HumanReadableMessage, this, /*Defaults=*/ nullptr, /*OwnerObject=*/ nullptr, PPF_None, /*ExportRootScope=*/ nullptr);
	return HumanReadableMessage;
}

//////////////////////////////////////////////////////////////////////
// 

// 从 PlayerController、PlayerState 或 Pawn 解析关联 PlayerState；无法解析时返回空指针。
APlayerState* ULyraVerbMessageHelpers::GetPlayerStateFromObject(UObject* Object)
{
	if (APlayerController* PC = Cast<APlayerController>(Object))
	{
		return PC->PlayerState;
	}

	if (APlayerState* TargetPS = Cast<APlayerState>(Object))
	{
		return TargetPS;
	}
	
	if (APawn* TargetPawn = Cast<APawn>(Object))
	{
		if (APlayerState* TargetPS = TargetPawn->GetPlayerState())
		{
			return TargetPS;
		}
	}
	return nullptr;
}

// 从 PlayerController、PlayerState 或 Pawn 解析玩家控制器；AI 控制器和无效对象返回空指针。
APlayerController* ULyraVerbMessageHelpers::GetPlayerControllerFromObject(UObject* Object)
{
	if (APlayerController* PC = Cast<APlayerController>(Object))
	{
		return PC;
	}

	if (APlayerState* TargetPS = Cast<APlayerState>(Object))
	{
		return TargetPS->GetPlayerController();
	}

	if (APawn* TargetPawn = Cast<APawn>(Object))
	{
		return Cast<APlayerController>(TargetPawn->GetController());
	}

	return nullptr;
}

// 把 Verb、施加者、目标、标签和数值映射为 Gameplay Cue 参数；ContextTags 当前不参与转换。
FGameplayCueParameters ULyraVerbMessageHelpers::VerbMessageToCueParameters(const FLyraVerbMessage& Message)
{
	FGameplayCueParameters Result;

	Result.OriginalTag = Message.Verb;
	Result.Instigator = Cast<AActor>(Message.Instigator);
	Result.EffectCauser = Cast<AActor>(Message.Target);
	Result.AggregatedSourceTags = Message.InstigatorTags;
	Result.AggregatedTargetTags = Message.TargetTags;
	//@TODO: = Message.ContextTags;
	Result.RawMagnitude = Message.Magnitude;

	return Result;
}

// 从 Gameplay Cue 参数还原 Verb 消息的对象、标签和数值；ContextTags 当前无法恢复。
FLyraVerbMessage ULyraVerbMessageHelpers::CueParametersToVerbMessage(const FGameplayCueParameters& Params)
{
	FLyraVerbMessage Result;
	
	Result.Verb = Params.OriginalTag;
	Result.Instigator = Params.Instigator.Get();
	Result.Target = Params.EffectCauser.Get();
	Result.InstigatorTags = Params.AggregatedSourceTags;
	Result.TargetTags = Params.AggregatedTargetTags;
	//@TODO: Result.ContextTags = ???;
	Result.Magnitude = Params.RawMagnitude;

	return Result;
}

