// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPlayerInput.h"
#include "Performance/LatencyMarkerModule.h"
#include "Settings/LyraSettingsLocal.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPlayerInput)

// 非 CDO 实例绑定延迟 Flash 设置变化，使运行时输入对象持续同步平台延迟标记模块状态。
ULyraPlayerInput::ULyraPlayerInput()
	: Super()
{
	// CDO/Archetype 不处理输入也不会 Tick，不能绑定设置委托，否则会留下永久无效监听者。
	// Don't bind to any settings delegates on the CDO, otherwise there would be a constant bound listener
	// and it wouldn't even do anything because it doesn't get ticked/process input
	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}
	
	BindToLatencyMarkerSettingChange();
}

// 销毁输入对象时解除设置委托，避免设置单例继续回调失效实例。
ULyraPlayerInput::~ULyraPlayerInput()
{
	UnbindLatencyMarkerSettingChangeListener();
}

// 保留父类按键处理结果，并在每次按键事件后尝试发送启用中的延迟 Flash 标记。
bool ULyraPlayerInput::InputKey(const FInputKeyEventArgs& Params)
{
	const bool bResult = Super::InputKey(Params);

	// Lyra 当前仅通过 Reflex 插件处理延迟标记，理论上可在非桌面平台编译剔除；为保留项目扩展性，此处仍跨平台执行入口。
	// Note: Since Lyra is only going to support the "Reflex" plugin to handle latency markers,
	// we could #if PLATFORM_DESKTOP this away to save on other platforms. However, for the sake
	// of extensibility for this same project we will not do that. 
	ProcessInputEventForLatencyMarker(Params);

	return bResult;
}

// 当用户启用延迟指示器时，在鼠标左键事件上向所有延迟标记模块写入当前帧的 Trigger Flash 标记。
void ULyraPlayerInput::ProcessInputEventForLatencyMarker(const FInputKeyEventArgs& Params)
{
	if (!bShouldTriggerLatencyFlash)
	{
		return;
	}
	
	// 鼠标左键按下时向所有延迟标记模块发送 Flash 标记。
	// Flash the latency marker on left mouse down
	if (Params.Key == EKeys::LeftMouseButton)
	{
		TArray<ILatencyMarkerModule*> LatencyMarkerModules = IModularFeatures::Get().GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());

		for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
		{
			// 自定义标记编号 7 对应 TRIGGER_FLASH。
			// TRIGGER_FLASH is 7
			LatencyMarkerModule->SetCustomLatencyMarker(7, GFrameCounter);
		}
	}
}

// 仅在平台支持时监听延迟 Flash 设置，并立即同步当前设置到已注册模块。
void ULyraPlayerInput::BindToLatencyMarkerSettingChange()
{
	if (!ULyraSettingsLocal::DoesPlatformSupportLatencyMarkers())
	{
		return;
	}
	
	ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
	if (!Settings)
	{
		return;
	}

	Settings->OnLatencyFlashInidicatorSettingsChangedEvent().AddUObject(this, &ThisClass::HandleLatencyMarkerSettingChanged);

	// 读取当前设置，并确保需要的输入延迟模块处于启用状态。
	// Initalize the settings and make sure that the input latency modules are enabled
	HandleLatencyMarkerSettingChanged();
}

// 从本地设置单例移除当前输入对象的全部延迟指示器设置回调。
void ULyraPlayerInput::UnbindLatencyMarkerSettingChangeListener()
{
	ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
	if (!Settings)
	{
		return;
	}

	Settings->OnLatencyFlashInidicatorSettingsChangedEvent().RemoveAll(this);
}

// 读取用户的延迟 Flash 开关，更新本地触发条件并统一通知所有延迟标记模块启停可视指示器。
void ULyraPlayerInput::HandleLatencyMarkerSettingChanged()
{
	// 该设置回调只应出现在支持延迟标记的平台。
	// Make sure that we only ever get this callback on platforms which support latency markers
	ensure(ULyraSettingsLocal::DoesPlatformSupportLatencyMarkers());
	
	const ULyraSettingsLocal* Settings = ULyraSettingsLocal::Get();
	if (!Settings)
	{
		return;
	}

	// 根据用户设置统一启用或禁用所有模块的延迟 Flash 标记。
	// Enable or disable the latency flash on all the marker modules according to the settings change
	bShouldTriggerLatencyFlash = Settings->GetEnableLatencyFlashIndicators();
	
	TArray<ILatencyMarkerModule*> LatencyMarkerModules = IModularFeatures::Get().GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());
	for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
	{
		LatencyMarkerModule->SetFlashIndicatorEnabled(bShouldTriggerLatencyFlash);
	}
}
