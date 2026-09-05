// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingValueDiscrete_Language.h"

#include "Internationalization/Culture.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Messaging/CommonGameDialog.h"
#include "Messaging/CommonMessagingSubsystem.h"
#include "Player/LyraLocalPlayer.h"
#include "Settings/LyraSettingsShared.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingValueDiscrete_Language)

#define LOCTEXT_NAMESPACE "Lyra"

// 保留索引 0 表示跟随系统默认语言。
static const int32 SettingSystemDefaultLanguageIndex = 0;

// 构造界面语言设置项并初始化内部状态。
ULyraSettingValueDiscrete_Language::ULyraSettingValueDiscrete_Language()
{
}

// 初始化界面语言选项、数据源及依赖关系。
void ULyraSettingValueDiscrete_Language::OnInitialized()
{
	Super::OnInitialized();

	const TArray<FString> AllCultureNames = FTextLocalizationManager::Get().GetLocalizedCultureNames(ELocalizationLoadFlags::Game);
	for (const FString& CultureName : AllCultureNames)
	{
		if (FInternationalization::Get().IsCultureAllowed(CultureName))
		{
			AvailableCultureNames.Add(CultureName);
		}
	}

	AvailableCultureNames.Insert(TEXT(""), SettingSystemDefaultLanguageIndex);
}

// 此设置不另存初始语言；取消时通过清除共享设置中的待选文化区域完成恢复。
void ULyraSettingValueDiscrete_Language::StoreInitial()
{
	// ?
}

// 应用语言设置时提示玩家：完整切换语言相关内容需要重启游戏。
void ULyraSettingValueDiscrete_Language::OnApply()
{
	if (UCommonMessagingSubsystem* Messaging = LocalPlayer->GetSubsystem<UCommonMessagingSubsystem>())
	{
		Messaging->ShowConfirmation(
			UCommonGameDialogDescriptor::CreateConfirmationOk(
			LOCTEXT("WarningLanguage_Title", "Language Changed"),
			LOCTEXT("WarningLanguage_Message", "You will need to restart the game completely for all language related changes to take effect.")
			)
		);
	}
}

// 将待应用语言重置为系统默认文化区域。
void ULyraSettingValueDiscrete_Language::ResetToDefault()
{
	SetDiscreteOptionByIndex(SettingSystemDefaultLanguageIndex);
}

// 清除待应用文化区域，使界面语言恢复为当前已生效状态。
void ULyraSettingValueDiscrete_Language::RestoreToInitial()
{
	if (ULyraSettingsShared* Settings = CastChecked<ULyraLocalPlayer>(LocalPlayer)->GetSharedSettings())
	{
		Settings->ClearPendingCulture();
		NotifySettingChanged(EGameSettingChangeReason::RestoreToInitial);
	}
}

// 索引 0 请求恢复系统默认文化区域，其余有效索引写入待应用文化区域，并通知设置变化。
void ULyraSettingValueDiscrete_Language::SetDiscreteOptionByIndex(int32 Index)
{
	if (ULyraSettingsShared* Settings = CastChecked<ULyraLocalPlayer>(LocalPlayer)->GetSharedSettings())
	{
		if (Index == SettingSystemDefaultLanguageIndex)
		{
			Settings->ResetToDefaultCulture();
		}
		else if (AvailableCultureNames.IsValidIndex(Index))
		{
			Settings->SetPendingCulture(AvailableCultureNames[Index]);
		}
		
		NotifySettingChanged(EGameSettingChangeReason::Change);
	}
}

// 优先匹配待应用文化区域，其次匹配当前文化区域；无法匹配时回退到系统默认项 0。
int32 ULyraSettingValueDiscrete_Language::GetDiscreteOptionIndex() const
{
	if (const ULyraSettingsShared* Settings = CastChecked<ULyraLocalPlayer>(LocalPlayer)->GetSharedSettings())
	{
		if (Settings->ShouldResetToDefaultCulture())
		{
			return SettingSystemDefaultLanguageIndex;
		}

	// 设置界面会先更新待应用区域文化，再在应用设置时真正切换，因此这里优先使用待应用值，确保 UI 立即显示用户的新选择。
	// We prefer the pending culture to the current culture as the options UI updates the pending culture before it 
	// gets applied, and we need the UI to reflect that choice
		FString PendingCulture = Settings->GetPendingCulture();
		if (PendingCulture.IsEmpty())
		{
			if (Settings->IsUsingDefaultCulture())
			{
				return SettingSystemDefaultLanguageIndex;
			}

			PendingCulture = FInternationalization::Get().GetCurrentCulture()->GetName();
		}

		// 首先按完整区域文化名称查找精确匹配。
		// Try to find an exact match 
		{
			const int32 ExactMatchIndex = AvailableCultureNames.IndexOfByKey(PendingCulture);
			if (ExactMatchIndex != INDEX_NONE)
			{
				return ExactMatchIndex;
			}
		}

		// 精确匹配失败后按区域文化优先级查找兼容项，例如允许 en-US 在 UI 中对应到 en。
		// Try to find a prioritized match (eg, allowing "en-US" to show as "en" in the UI)
		const TArray<FString> PrioritizedPendingCultures = FInternationalization::Get().GetPrioritizedCultureNames(PendingCulture);
		for (int32 i = 0; i < AvailableCultureNames.Num(); ++i)
		{
			if (PrioritizedPendingCultures.Contains(AvailableCultureNames[i]))
			{
				return i;
			}
		}
	}

	return 0;
}

// 返回当前可供界面显示的界面语言选项文本。
TArray<FText> ULyraSettingValueDiscrete_Language::GetDiscreteOptions() const
{
	TArray<FText> Options;

	for (const FString& CultureName : AvailableCultureNames)
	{
		if (CultureName == TEXT(""))
		{
			const FCulturePtr SystemDefaultCulture = FInternationalization::Get().GetDefaultCulture();
			if (ensure(SystemDefaultCulture))
			{
				const FString& DefaultCultureDisplayName = SystemDefaultCulture->GetDisplayName();
				FText LocalizedSystemDefault = FText::Format(LOCTEXT("SystemDefaultLanguage", "System Default ({0})"), FText::FromString(DefaultCultureDisplayName));

				Options.Add(MoveTemp(LocalizedSystemDefault));
			}
		}
		else
		{
			FCulturePtr Culture = FInternationalization::Get().GetCulture(CultureName);
			if (ensureMsgf(Culture, TEXT("Unable to find Culture '%s'!"), *CultureName))
			{
				const FString CultureDisplayName = Culture->GetDisplayName();
				const FString CultureNativeName = Culture->GetNativeName();

				// 仅在本地名称与当前界面语言下的显示名称不同时同时展示二者，避免重复。
				// Only show both names if they're different (to avoid repetition)
				FString Entry = (!CultureNativeName.Equals(CultureDisplayName, ESearchCase::CaseSensitive))
					? FString::Printf(TEXT("%s (%s)"), *CultureNativeName, *CultureDisplayName)
					: CultureNativeName;

				Options.Add(FText::FromString(Entry));
			}
		}
	}

	return Options;
}

#undef LOCTEXT_NAMESPACE
