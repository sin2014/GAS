// Copyright Epic Games, Inc. All Rights Reserved.

#include "Messaging/CommonMessagingSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "UObject/UObjectHash.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonMessagingSubsystem)

class FSubsystemCollectionBase;
class UClass;

// 初始化本地玩家消息子系统，并准备使用项目配置的对话框类。
void UCommonMessagingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

// 释放消息子系统持有的对话框资源并完成反初始化。
void UCommonMessagingSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

// 仅在没有项目派生消息子系统覆盖当前类时创建实例。
bool UCommonMessagingSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	UGameInstance* GameInstance = CastChecked<ULocalPlayer>(Outer)->GetGameInstance();
	if (GameInstance && !GameInstance->IsDedicatedServerInstance())
	{
		TArray<UClass*> ChildClasses;
		GetDerivedClasses(GetClass(), ChildClasses, false);

		// 仅当项目未提供覆盖实现时创建基类实例，避免同时存在两套子系统。
		// Only create an instance if there is no override implementation defined elsewhere
		return ChildClasses.Num() == 0;
	}

	return false;
}

// 请求项目消息层显示确认描述，并将结果传给调用方回调。
void UCommonMessagingSubsystem::ShowConfirmation(UCommonGameDialogDescriptor* DialogDescriptor, FCommonMessagingResultDelegate ResultCallback)
{
	
}

// 请求项目消息层以错误样式显示描述，并转发关闭结果。
void UCommonMessagingSubsystem::ShowError(UCommonGameDialogDescriptor* DialogDescriptor, FCommonMessagingResultDelegate ResultCallback)
{
	
}
