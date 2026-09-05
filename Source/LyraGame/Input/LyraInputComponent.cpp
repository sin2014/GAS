// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraInputComponent.h"

#include "EnhancedInputSubsystems.h"
#include "Player/LyraLocalPlayer.h"
#include "Settings/LyraSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraInputComponent)

class ULyraInputConfig;

// 构造 Lyra 的 EnhancedInputComponent 扩展，当前不额外修改父类初始化结果。
ULyraInputComponent::ULyraInputComponent(const FObjectInitializer& ObjectInitializer)
{
}

// 校验输入配置和本地玩家子系统，并保留从 InputConfig 安装项目自定义映射资源的对称扩展点。
void ULyraInputComponent::AddInputMappings(const ULyraInputConfig* InputConfig, UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const
{
	check(InputConfig);
	check(InputSubsystem);

	// 可在此扩展从 InputConfig 添加项目特定 MappingContext 或其他输入资源的逻辑。
	// Here you can handle any custom logic to add something from your input config if required
}

// 校验输入配置和本地玩家子系统，并保留撤销 AddInputMappings 所安装项目映射的扩展点。
void ULyraInputComponent::RemoveInputMappings(const ULyraInputConfig* InputConfig, UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const
{
	check(InputConfig);
	check(InputSubsystem);

	// 在此对称移除 AddInputMappings 中添加的项目特定输入映射。
	// Here you can handle any custom logic to remove input mappings that you may have added above
}

// 按句柄移除一组 Enhanced Input 绑定并清空调用方数组，防止后续重复解除同一绑定。
void ULyraInputComponent::RemoveBinds(TArray<uint32>& BindHandles)
{
	for (uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}
	BindHandles.Reset();
}
