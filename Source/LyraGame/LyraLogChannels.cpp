// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraLogChannels.h"
#include "GameFramework/Actor.h"

// Lyra 通用运行时日志分类。
DEFINE_LOG_CATEGORY(LogLyra);
// Experience 选择、加载和激活流程日志分类。
DEFINE_LOG_CATEGORY(LogLyraExperience);
// Gameplay Ability System 与技能执行流程日志分类。
DEFINE_LOG_CATEGORY(LogLyraAbilitySystem);
// 团队分配和团队关系系统日志分类。
DEFINE_LOG_CATEGORY(LogLyraTeams);

// 根据 Actor 或 ActorComponent 的本地网络角色生成日志上下文；编辑器中无角色时使用 PIE 上下文字符串。
FString GetClientServerContextString(UObject* ContextObject)
{
	ENetRole Role = ROLE_None;

	if (AActor* Actor = Cast<AActor>(ContextObject))
	{
		Role = Actor->GetLocalRole();
	}
	else if (UActorComponent* Component = Cast<UActorComponent>(ContextObject))
	{
		Role = Component->GetOwnerRole();
	}

	if (Role != ROLE_None)
	{
		return (Role == ROLE_Authority) ? TEXT("Server") : TEXT("Client");
	}
	else
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			extern ENGINE_API FString GPlayInEditorContextString;
			return GPlayInEditorContextString;
		}
#endif
	}

	return TEXT("[]");
}
