// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingValueDiscreteDynamic_AudioOutputDevice.h"

#include "AudioDeviceNotificationSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingValueDiscreteDynamic_AudioOutputDevice)

#define LOCTEXT_NAMESPACE "LyraSettings"

// 初始化音频输出设备设置，注册设备变化通知并请求当前设备列表。
void ULyraSettingValueDiscreteDynamic_AudioOutputDevice::OnInitialized()
{
	Super::OnInitialized();

	DevicesObtainedCallback.BindUFunction(this, FName("OnAudioOutputDevicesObtained"));
	DevicesSwappedCallback.BindUFunction(this, FName("OnCompletedDeviceSwap"));

	if (UAudioDeviceNotificationSubsystem* AudioDeviceNotifSubsystem = UAudioDeviceNotificationSubsystem::Get())
	{
		AudioDeviceNotifSubsystem->DeviceAddedNative.AddUObject(this, &ULyraSettingValueDiscreteDynamic_AudioOutputDevice::DeviceAddedOrRemoved);
		AudioDeviceNotifSubsystem->DeviceRemovedNative.AddUObject(this, &ULyraSettingValueDiscreteDynamic_AudioOutputDevice::DeviceAddedOrRemoved);
		//AudioDeviceNotifSubsystem->DeviceSwitchedNative.AddUObject(this, &ULyraSettingValueDiscreteDynamic_AudioOutputDevice::DeviceSwitched);
		AudioDeviceNotifSubsystem->DefaultRenderDeviceChangedNative.AddUObject(this, &ULyraSettingValueDiscreteDynamic_AudioOutputDevice::DefaultDeviceChanged);
	}

	UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices(this, DevicesObtainedCallback);
}

// 用枚举到的输出设备重建离散选项，并保留或回退当前选择。
void ULyraSettingValueDiscreteDynamic_AudioOutputDevice::OnAudioOutputDevicesObtained(const TArray<FAudioOutputDeviceInfo>& AvailableDevices)
{
	int32 NewSize = AvailableDevices.Num();
	OutputDevices.Reset(NewSize++);
	OutputDevices.Append(AvailableDevices);

	OptionValues.Reset(NewSize);
	OptionDisplayTexts.Reset(NewSize);

	// 先保留索引 0 作为“系统默认设备”选项，枚举完成后再用实际默认设备名称格式化其显示文本。
	// Placeholder - needs to be first option so we can format the default device string later
	AddDynamicOption(TEXT(""), FText::GetEmpty());
	FString SystemDefaultDeviceName;

	for (const FAudioOutputDeviceInfo& DeviceInfo : OutputDevices)
	{
		if (!DeviceInfo.DeviceId.IsEmpty() && !DeviceInfo.Name.IsEmpty())
		{
			// 记录操作系统当前指定的默认音频输出设备。
			// System Default 
			if (DeviceInfo.bIsSystemDefault)
			{
				SystemDefaultDeviceId = DeviceInfo.DeviceId;
				SystemDefaultDeviceName = DeviceInfo.Name;
			}

			// 记录音频混音器当前实际使用的输出设备。
			// Current Device
			if (DeviceInfo.bIsCurrentDevice)
			{
				CurrentDeviceId = DeviceInfo.DeviceId;
			}

			// 将有效设备加入设置菜单的动态选项列表。
			// Add the menu option
			AddDynamicOption(DeviceInfo.DeviceId, FText::FromString(DeviceInfo.Name));
		}
	}

	OptionDisplayTexts[0] = FText::Format(LOCTEXT("DefaultAudioOutputDevice", "Default Output - {0}"), FText::FromString(SystemDefaultDeviceName));
	SetDefaultValueFromString(TEXT(""));
	RefreshEditableState();

	//LastKnownGoodIndex = GetDiscreteOptionDefaultIndex();
	//SetDiscreteOptionByIndex(GetDiscreteOptionIndex());
}

// 处理音频输出设备切换结果，失败时恢复原选择并刷新设置。
void ULyraSettingValueDiscreteDynamic_AudioOutputDevice::OnCompletedDeviceSwap(const FSwapAudioOutputResult& SwapResult)
{
	//if (SwapResult.Result == ESwapAudioOutputDeviceResultState::Failure)
	//{
	//	UE_LOG(LogLyra, VeryVerbose, TEXT("AudioOutputDevice failure! Resetting to: %s"), *(OptionDisplayTexts[LastKnownGoodIndex].ToString()));
	//	if (OptionValues.Num() < LastKnownGoodIndex && SwapResult.RequestedDeviceId != OptionValues[LastKnownGoodIndex])
	//	{
	//		SetDiscreteOptionByIndex(LastKnownGoodIndex);
	//	}

	//	// Remove the invalid device
	//	if (SwapResult.RequestedDeviceId != SystemDefaultDeviceId)
	//	{
	//		OutputDevices.RemoveAll([&SwapResult](FAudioOutputDeviceInfo& Device)
	//		{
	//			return Device.DeviceId == SwapResult.RequestedDeviceId;
	//		});

	//		RemoveDynamicOption(SwapResult.RequestedDeviceId);
	//		RefreshEditableState();
	//	}
	//}
}

// 音频设备增删后重新请求完整输出设备列表。
void ULyraSettingValueDiscreteDynamic_AudioOutputDevice::DeviceAddedOrRemoved(FString DeviceId)
{
	UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices(this, DevicesObtainedCallback);
}

// 系统默认输出设备变化后重新请求设备列表。
void ULyraSettingValueDiscreteDynamic_AudioOutputDevice::DefaultDeviceChanged(EAudioDeviceChangedRole InRole, FString DeviceId)
{
	UAudioMixerBlueprintLibrary::GetAvailableAudioOutputDevices(this, DevicesObtainedCallback);
}

// 请求切换到有效索引对应的音频输出设备；无效索引或重复选择时不处理。
void ULyraSettingValueDiscreteDynamic_AudioOutputDevice::SetDiscreteOptionByIndex(int32 Index)
{
	Super::SetDiscreteOptionByIndex(Index);
	//UE_LOG(LogLyra, VeryVerbose, TEXT("AudioOutputDevice set to %s - %s"), *(OptionDisplayTexts[Index].ToString()), *OptionValues[Index]);
	//bRequestDefault = false;

	//FString RequestedAudioDeviceId = GetValueAsString();

	//// Grab the correct deviceId if the user has selected default
	//const int32 DefaultOptionIndex = GetDiscreteOptionDefaultIndex();
	//if (Index == DefaultOptionIndex)
	//{
	//	RequestedAudioDeviceId = SystemDefaultDeviceId;
	//}

	//// Only swap if the requested deviceId is different than our current
	//if (RequestedAudioDeviceId == CurrentDeviceId)
	//{
	//	LastKnownGoodIndex = Index;
	//	UE_LOG(LogLyra, VeryVerbose, TEXT("AudioOutputDevice (Not Swapping) - LKG set to index :%d"), LastKnownGoodIndex);
	//}
	//else
	//{
	//	bRequestDefault = (Index == DefaultOptionIndex);
	//	UAudioMixerBlueprintLibrary::SwapAudioOutputDevice(LocalPlayer, RequestedAudioDeviceId, DevicesSwappedCallback);
	//	UE_LOG(LogLyra, VeryVerbose, TEXT("AudioOutputDevice requesting %s"), *RequestedAudioDeviceId);
	//}
}


#undef LOCTEXT_NAMESPACE
