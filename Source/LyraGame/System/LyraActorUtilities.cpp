// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraActorUtilities.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraActorUtilities)

// 将传入 Actor 或 ActorComponent 的 NetMode 转为蓝图枚举；当前实现对其他 UObject 会回退为 Standalone。
EBlueprintExposedNetMode ULyraActorUtilities::SwitchOnNetMode(const UObject* WorldContextObject)
{
	ENetMode NetMode = NM_Standalone;
	for (const UObject* TestObject = WorldContextObject; TestObject != nullptr; TestObject = TestObject->GetOuter())
	{
		if (const UActorComponent* Component = Cast<const UActorComponent>(WorldContextObject))
		{
			NetMode = Component->GetNetMode();
			break;
		}
		else if (const AActor* Actor = Cast<const AActor>(WorldContextObject))
		{
			NetMode = Actor->GetNetMode();
			break;
		}
	}

	switch (NetMode)
	{
	case NM_Client:
		return EBlueprintExposedNetMode::Client;
	case NM_Standalone:
		return EBlueprintExposedNetMode::Standalone;
	case NM_DedicatedServer:
		return EBlueprintExposedNetMode::DedicatedServer;
	case NM_ListenServer:
		return EBlueprintExposedNetMode::ListenServer;
	default:
		ensure(false);
		return EBlueprintExposedNetMode::Standalone;
	}
}

