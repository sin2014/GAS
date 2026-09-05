// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraCosmeticDeveloperSettings.h"
#include "Cosmetics/LyraCharacterPartTypes.h"
#include "Misc/App.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "System/LyraDevelopmentStatics.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "LyraControllerComponent_CharacterParts.h"
#include "EngineUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCosmeticDeveloperSettings)

#define LOCTEXT_NAMESPACE "LyraCheats"

// 初始化编辑器外观作弊为 ReplaceParts 模式，并注册设置变更所需默认状态。
ULyraCosmeticDeveloperSettings::ULyraCosmeticDeveloperSettings()
{
}

// 把该开发者设置归入 Lyra 编辑器设置分类。
FName ULyraCosmeticDeveloperSettings::GetCategoryName() const
{
	return FApp::GetProjectName();
}

#if WITH_EDITOR

// 编辑器属性变化后重新应用 CVar 设置，并在 PIE 中刷新所有玩家外观。
void ULyraCosmeticDeveloperSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplySettings();
}

// 配置重载后重新应用设置，并在 PIE 中刷新角色装配。
void ULyraCosmeticDeveloperSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);

	ApplySettings();
}

// 对象属性初始化完成后应用外观开发设置。
void ULyraCosmeticDeveloperSettings::PostInitProperties()
{
	Super::PostInitProperties();

	ApplySettings();
}

// 把设置对象中由 CVar 支持的属性值同步到控制台变量。
void ULyraCosmeticDeveloperSettings::ApplySettings()
{
	if (GIsEditor && (GEngine != nullptr))
	{
		ReapplyLoadoutIfInPIE();
	}
}

// 找到 PIE 权威 World，遍历 PlayerController 并让每个外观组件重新应用开发者部件。
void ULyraCosmeticDeveloperSettings::ReapplyLoadoutIfInPIE()
{
#if WITH_SERVER_CODE
	// 在 PIE 权威 World 中为所有玩家重新应用当前外观装配设置。
	// Update the loadout on all players
	UWorld* ServerWorld = ULyraDevelopmentStatics::FindPlayInEditorAuthorityWorld();
	if (ServerWorld != nullptr)
	{
		ServerWorld->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([=]()
			{
				for (TActorIterator<APlayerController> PCIterator(ServerWorld); PCIterator; ++PCIterator)
				{
					if (APlayerController* PC = *PCIterator)
					{
						if (ULyraControllerComponent_CharacterParts* CosmeticComponent = PC->FindComponentByClass<ULyraControllerComponent_CharacterParts>())
						{
							CosmeticComponent->ApplyDeveloperSettings();
						}
					}
				}
			}));
	}
#endif	// WITH_SERVER_CODE
}

// PIE 启动时若配置了作弊外观，显示提醒通知以避免误认为正式装配。
void ULyraCosmeticDeveloperSettings::OnPlayInEditorStarted() const
{
	// 外观作弊部件非空时显示通知，提醒测试结果受编辑器覆盖影响。
	// Show a notification toast to remind the user that there's an experience override set
	if (CheatCosmeticCharacterParts.Num() > 0)
	{
		FNotificationInfo Info(LOCTEXT("CosmeticOverrideActive", "Applying Cosmetic Override"));
		Info.ExpireDuration = 2.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE

