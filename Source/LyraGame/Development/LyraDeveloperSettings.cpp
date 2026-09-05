// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraDeveloperSettings.h"
#include "Misc/App.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraDeveloperSettings)

#define LOCTEXT_NAMESPACE "LyraCheats"

// 构造编辑器用户级 Lyra 开发设置，并保留配置文件中的 Experience、Bot 和作弊覆盖。
ULyraDeveloperSettings::ULyraDeveloperSettings()
{
}

// 把通用开发者设置归入 Lyra 编辑器分类。
FName ULyraDeveloperSettings::GetCategoryName() const
{
	return FApp::GetProjectName();
}

#if WITH_EDITOR
// 属性在编辑器中修改后应用 CVar 支持的设置，再调用父类完成通知。
void ULyraDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplySettings();
}

// 配置文件重载后重新应用 CVar 设置。
void ULyraDeveloperSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);

	ApplySettings();
}

// 对象初始化完成后把配置值应用到对应控制台变量。
void ULyraDeveloperSettings::PostInitProperties()
{
	Super::PostInitProperties();

	ApplySettings();
}

// 将 UDeveloperSettingsBackedByCVars 属性同步到控制台变量。
void ULyraDeveloperSettings::ApplySettings()
{
}

// PIE 启动时若 ExperienceOverride 有效则显示提醒，说明当前地图默认 Experience 被覆盖。
void ULyraDeveloperSettings::OnPlayInEditorStarted() const
{
	// ExperienceOverride 有效时显示通知，提醒 PIE 未使用地图默认 Experience。
	// Show a notification toast to remind the user that there's an experience override set
	if (ExperienceOverride.IsValid())
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("ExperienceOverrideActive", "Developer Settings Override\nExperience {0}"),
			FText::FromName(ExperienceOverride.PrimaryAssetName)
		));
		Info.ExpireDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}
#endif

#undef LOCTEXT_NAMESPACE

