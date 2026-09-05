// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPlatformEmulationSettings.h"
#include "CommonUIVisibilitySubsystem.h"
#include "Engine/PlatformSettingsManager.h"
#include "Misc/App.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "DeviceProfiles/DeviceProfile.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPlatformEmulationSettings)

#define LOCTEXT_NAMESPACE "LyraCheats"

// 构造平台模拟设置，并初始化尚未应用任何 PretendPlatform。
ULyraPlatformEmulationSettings::ULyraPlatformEmulationSettings()
{
}

// 把平台模拟设置归入 Lyra 编辑器分类。
FName ULyraPlatformEmulationSettings::GetCategoryName() const
{
	return FApp::GetProjectName();
}

// 仅在 PIE DeviceProfile 模拟启用时返回配置的基础 Profile，否则返回 NAME_None。
FName ULyraPlatformEmulationSettings::GetPretendBaseDeviceProfile() const
{
	return PretendBaseDeviceProfile;
}

// 返回当前配置的模拟平台名称；未设置时为 NAME_None。
FName ULyraPlatformEmulationSettings::GetPretendPlatformName() const
{
	return PretendPlatform;
}

#if WITH_EDITOR
// 编辑器修改平台模拟属性后重新选择兼容 Profile、应用 Trait/CVar，并更新活动模拟平台。
void ULyraPlatformEmulationSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplySettings();
}

// 配置重载后重新选择合理 Profile 并应用平台模拟。
void ULyraPlatformEmulationSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);

	ApplySettings();
}

// 属性初始化后选择兼容 DeviceProfile，并应用平台 Trait 和 CVar 覆盖。
void ULyraPlatformEmulationSettings::PostInitProperties()
{
	Super::PostInitProperties();

	ApplySettings();
}

// PIE 启动时分别提示强制启用 Trait、抑制 Trait 和 PretendPlatform 覆盖。
void ULyraPlatformEmulationSettings::OnPlayInEditorStarted() const
{
	// 存在强制启用 Trait 时显示 PIE 提醒通知。
	// Show a notification toast to remind the user that there's a tag enable override set
	if (!AdditionalPlatformTraitsToEnable.IsEmpty())
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("PlatformTraitEnableActive", "Platform Trait Override\nEnabling {0}"),
			FText::AsCultureInvariant(AdditionalPlatformTraitsToEnable.ToStringSimple())
		));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	// 存在强制抑制 Trait 时显示 PIE 提醒通知。
	// Show a notification toast to remind the user that there's a tag suppression override set
	if (!AdditionalPlatformTraitsToSuppress.IsEmpty())
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("PlatformTraitSuppressionActive", "Platform Trait Override\nSuppressing {0}"),
			FText::AsCultureInvariant(AdditionalPlatformTraitsToSuppress.ToStringSimple())
		));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	// 指定 PretendPlatform 时显示 PIE 平台模拟提醒。
	// Show a notification toast to remind the user that there's a platform override set
	if (PretendPlatform != NAME_None)
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("PlatformOverrideActive", "Platform Override Active\nPretending to be {0}"),
			FText::FromName(PretendPlatform)
		));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

// 更新 PlatformTrait 启用/抑制覆盖，同步 CVar，并切换当前活动 PretendPlatform。
void ULyraPlatformEmulationSettings::ApplySettings()
{
	UCommonUIVisibilitySubsystem::SetDebugVisibilityConditions(AdditionalPlatformTraitsToEnable, AdditionalPlatformTraitsToSuppress);

	if (GIsEditor && PretendPlatform != LastAppliedPretendPlatform)
	{
		ChangeActivePretendPlatform(PretendPlatform);
	}

	PickReasonableBaseDeviceProfile();
}

// 撤销上一次平台模拟 Trait，再向 CommonUI 和平台设置应用新平台名称。
void ULyraPlatformEmulationSettings::ChangeActivePretendPlatform(FName NewPlatformName)
{
	LastAppliedPretendPlatform = NewPlatformName;
	PretendPlatform = NewPlatformName;

	UPlatformSettingsManager::SetEditorSimulatedPlatform(PretendPlatform);
}

#endif

// 从 DataDrivenPlatformInfoRegistry 收集全部有效平台 Ini 名称并排序返回。
TArray<FName> ULyraPlatformEmulationSettings::GetKnownPlatformIds() const
{
	TArray<FName> Results;

#if WITH_EDITOR
	Results.Add(NAME_None);
	Results.Append(UPlatformSettingsManager::GetKnownAndEnablePlatformIniNames());
#endif

	return Results;
}

// 从 DeviceProfileManager 收集全部 Profile 名称，并在设置 PretendPlatform 时只保留设备类型匹配项。
TArray<FName> ULyraPlatformEmulationSettings::GetKnownDeviceProfiles() const
{
	TArray<FName> Results;
	
#if WITH_EDITOR
	const UDeviceProfileManager& Manager = UDeviceProfileManager::Get();
	Results.Reserve(Manager.Profiles.Num() + 1);

	if (PretendPlatform == NAME_None)
	{
		Results.Add(NAME_None);
	}

	for (const TObjectPtr<UDeviceProfile>& Profile : Manager.Profiles)
	{
		bool bIncludeEntry = true;
		if (PretendPlatform != NAME_None)
		{
			if (Profile->DeviceType != PretendPlatform.ToString())
			{
				bIncludeEntry = false;
			}
		}

		if (bIncludeEntry)
		{
			Results.Add(Profile->GetFName());
		}
	}
#endif

	return Results;
}

// 保留已兼容的 Profile；否则从目标平台候选项中选择名称最短者，找不到时清空配置。
void ULyraPlatformEmulationSettings::PickReasonableBaseDeviceProfile()
{
	// 若当前模拟 DeviceProfile 已属于目标平台，则无需重新选择。
	// First see if our pretend device profile is already compatible, if so we don't need to do anything
	UDeviceProfileManager& Manager = UDeviceProfileManager::Get();
	if (UDeviceProfile* ProfilePtr = Manager.FindProfile(PretendBaseDeviceProfile.ToString(), /*bCreateOnFail=*/ false))
	{
		const bool bIsCompatible = (PretendPlatform == NAME_None) || (ProfilePtr->DeviceType == PretendPlatform.ToString());
		if (!bIsCompatible)
		{
			PretendBaseDeviceProfile = NAME_None;
		}
	}

	if ((PretendPlatform != NAME_None) && (PretendBaseDeviceProfile == NAME_None))
	{
		// 模拟平台但没有兼容基础 Profile 时，自动选择该平台的候选项；
		// 以名称最短者作为简单启发式的默认选择。
		// If we're pretending we're a platform and don't have a pretend base profile, pick a reasonable one,
		// preferring the one with the shortest name as a simple heuristic
		FName ShortestMatchingProfileName;
		const FString PretendPlatformStr = PretendPlatform.ToString();
		for (const TObjectPtr<UDeviceProfile>& Profile : Manager.Profiles)
		{
			if (Profile->DeviceType == PretendPlatformStr)
			{
				const FName TestName = Profile->GetFName();
				if ((ShortestMatchingProfileName == NAME_None) || (TestName.GetStringLength() < ShortestMatchingProfileName.GetStringLength()))
				{
					ShortestMatchingProfileName = TestName;
				}
			}
		}
		PretendBaseDeviceProfile = ShortestMatchingProfileName;
	}
}

#undef LOCTEXT_NAMESPACE

