// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSettingKeyboardInput.h"

#include "EnhancedInputSubsystems.h"
#include "../LyraSettingsLocal.h"
#include "Player/LyraLocalPlayer.h"
#include "PlayerMappableInputConfig.h"
#include "EnhancedInputSubsystems.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSettingKeyboardInput)

class ULocalPlayer;

#define LOCTEXT_NAMESPACE "LyraSettings"

namespace Lyra::ErrorMessages
{
// 找不到增强输入映射元数据时使用的占位显示名称。
static const FText UnknownMappingName = LOCTEXT("LyraErrors_UnknownMappingName", "Unknown Mapping");
}


// 初始化键盘映射设置项的槽位状态。
ULyraSettingKeyboardInput::ULyraSettingKeyboardInput()
{
	bReportAnalytics = false;
}

// 返回当前按键映射行的显示名称；映射不存在时返回空文本。
FText ULyraSettingKeyboardInput::GetSettingDisplayName() const
{
	if (const FKeyMappingRow* Row = FindKeyMappingRow())
	{
		if (Row->HasAnyMappings())
		{
			return Row->Mappings.begin()->GetDisplayName();
		}
	}
	
	return Lyra::ErrorMessages::UnknownMappingName;
}

// 返回当前按键映射行的显示分类；映射不存在时返回空文本。
FText ULyraSettingKeyboardInput::GetSettingDisplayCategory() const
{
	if (const FKeyMappingRow* Row = FindKeyMappingRow())
	{
		if (Row->HasAnyMappings())
		{
			return Row->Mappings.begin()->GetDisplayCategory();
		}
	}

	return Lyra::ErrorMessages::UnknownMappingName;
}

// 在当前可映射按键配置中查找此设置项对应的映射行。
const FKeyMappingRow* ULyraSettingKeyboardInput::FindKeyMappingRow() const
{
	if (const UEnhancedPlayerMappableKeyProfile* Profile = FindMappableKeyProfile())
	{
		return Profile->FindKeyMappingRow(ActionMappingName);
	}

	ensure(false);
	return nullptr;
}

// 返回本地玩家当前的增强输入可映射按键配置；不可用时返回空指针。
UEnhancedPlayerMappableKeyProfile* ULyraSettingKeyboardInput::FindMappableKeyProfile() const
{
	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		return Settings->GetKeyProfileWithId(ProfileIdentifier);
	}
	
	ensure(false);
	return nullptr;
}

// 返回本地玩家的增强输入用户设置；玩家或子系统不可用时返回空指针。
UEnhancedInputUserSettings* ULyraSettingKeyboardInput::GetUserSettings() const
{
	if(ULyraLocalPlayer* LyraLocalPlayer = Cast<ULyraLocalPlayer>(LocalPlayer))
	{
		// 从本地玩家的增强输入子系统取得用户设置，以便读写玩家按键配置。
		// Map the key to the player key profile
		if (UEnhancedInputLocalPlayerSubsystem* System = LyraLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			return System->GetUserSettings();
		}
	}
	
	return nullptr;
}

// 初始化键盘映射设置项，并记录用于恢复的当前绑定。
void ULyraSettingKeyboardInput::OnInitialized()
{
	DynamicDetails = FGetGameSettingsDetails::CreateLambda([this](ULocalPlayer&)
	{
		if (const FKeyMappingRow* Row = FindKeyMappingRow())
		{
			if (Row->HasAnyMappings())
			{
				return FText::Format(LOCTEXT("DynamicDetails_KeyboardInputAction", "Bindings for {0}"), Row->Mappings.begin()->GetDisplayName());
			}
		}
		return FText::GetEmpty();
	});

	Super::OnInitialized();
}

// 从映射配置和查询条件提取映射名称、槽位按键及显示信息。
void ULyraSettingKeyboardInput::InitializeInputData(const UEnhancedPlayerMappableKeyProfile* KeyProfile, const FKeyMappingRow& MappingData, const FPlayerMappableKeyQueryOptions& InQueryOptions)
{
	check(KeyProfile);

	ProfileIdentifier = KeyProfile->GetProfileIdString();
	QueryOptions = InQueryOptions;
	
	for (const FPlayerKeyMapping& Mapping : MappingData.Mappings)
	{
		// 只记录通过初始化查询条件的映射，避免把其他设备或映射类别混入当前设置项。
		// Only add mappings that pass the query filters that have been provided upon creation
		if (!KeyProfile->DoesMappingPassQueryOptions(Mapping, QueryOptions))
		{
			continue;
		}
		
		ActionMappingName = Mapping.GetMappingName();
		InitialKeyMappings.Add(Mapping.GetSlot(), Mapping.GetCurrentKey());
		const FText& MappingDisplayName =Mapping.GetDisplayName();
		
		if (!MappingDisplayName.IsEmpty())
		{
			SetDisplayName(MappingDisplayName);	
		}
	}
	
	const FString NameString = TEXT("KBM_Input_") + ActionMappingName.ToString();
	SetDevName(*NameString);
}

// 返回指定映射槽位当前绑定按键的显示文本；无按键时返回未绑定提示。
FText ULyraSettingKeyboardInput::GetKeyTextFromSlot(const EPlayerMappableKeySlot InSlot) const
{
	if (const UEnhancedPlayerMappableKeyProfile* Profile = FindMappableKeyProfile())
	{
		FPlayerMappableKeyQueryOptions QueryOptionsForSlot = QueryOptions;
		QueryOptionsForSlot.SlotToMatch = InSlot;
		
		if (const FKeyMappingRow* Row = FindKeyMappingRow())
		{
			for (const FPlayerKeyMapping& Mapping : Row->Mappings)
			{
				if (Profile->DoesMappingPassQueryOptions(Mapping, QueryOptionsForSlot))
				{
					return Mapping.GetCurrentKey().GetDisplayName();
				}
			}
		}
	}
	
	return EKeys::Invalid.GetDisplayName();
}

// 将此映射行恢复为配置文件默认绑定，并通知设置系统。
void ULyraSettingKeyboardInput::ResetToDefault()
{	
	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		FMapPlayerKeyArgs Args = {};
		Args.MappingName = ActionMappingName;

		FGameplayTagContainer FailureReason;
		Settings->ResetAllPlayerKeysInRow(Args, FailureReason);

		NotifySettingChanged(EGameSettingChangeReason::Change);
	}
}

// 记录当前两个按键槽位，供取消修改时恢复。
void ULyraSettingKeyboardInput::StoreInitial()
{
	if (const UEnhancedPlayerMappableKeyProfile* Profile = FindMappableKeyProfile())
	{
		if(const FKeyMappingRow* Row = FindKeyMappingRow())
		{
			for (const FPlayerKeyMapping& Mapping : Row->Mappings)
			{
				if (Profile->DoesMappingPassQueryOptions(Mapping, QueryOptions))
				{
					ActionMappingName = Mapping.GetMappingName();
					InitialKeyMappings.Add(Mapping.GetSlot(), Mapping.GetCurrentKey());	
				}
			}
		}
	}
}

// 恢复初始化时记录的按键绑定并通知设置系统。
void ULyraSettingKeyboardInput::RestoreToInitial()
{
	for (TPair<EPlayerMappableKeySlot, FKey> Pair : InitialKeyMappings)
	{
		ChangeBinding((int32)Pair.Key, Pair.Value);	
	}
}

// 尝试修改指定槽位的按键绑定，成功时应用并返回 true。
bool ULyraSettingKeyboardInput::ChangeBinding(int32 InKeyBindSlot, FKey NewKey)
{
	if (!NewKey.IsGamepadKey())
	{
		FMapPlayerKeyArgs Args = {};
		Args.MappingName = ActionMappingName;
		Args.Slot = (EPlayerMappableKeySlot) (static_cast<uint8>(InKeyBindSlot));
		Args.NewKey = NewKey;
		// 如需进一步限定作用范围，可指定本次映射只应用到特定硬件设备或按键配置。
		// If you want to, you can additionally specify this mapping to only be applied to a certain hardware device or key profile
		//Args.ProfileId =
		//Args.HardwareDeviceId =
		
		if (UEnhancedInputUserSettings* Settings = GetUserSettings())
		{
			FGameplayTagContainer FailureReason;
			Settings->MapPlayerKey(Args, FailureReason);
			NotifySettingChanged(EGameSettingChangeReason::Change);
		}

		return true;
	}

	return false;
}

// 收集指定按键在目标槽位上已绑定的其他动作名称。
void ULyraSettingKeyboardInput::GetAllMappedActionsFromKey(int32 InKeyBindSlot, FKey Key, TArray<FName>& OutActionNames) const
{	
	if (const UEnhancedPlayerMappableKeyProfile* Profile = FindMappableKeyProfile())
	{		
		Profile->GetMappingNamesForKey(Key, OutActionNames);
	}
}

// 返回此映射行是否已偏离配置文件默认绑定。
bool ULyraSettingKeyboardInput::IsMappingCustomized() const
{
	bool bResult = false;

	if (const UEnhancedPlayerMappableKeyProfile* Profile = FindMappableKeyProfile())
	{
		FPlayerMappableKeyQueryOptions QueryOptionsForSlot = QueryOptions;

		if (const FKeyMappingRow* Row = FindKeyMappingRow())
		{
			for (const FPlayerKeyMapping& Mapping : Row->Mappings)
			{
				if (Profile->DoesMappingPassQueryOptions(Mapping, QueryOptionsForSlot))
				{
					bResult |= Mapping.IsCustomized();
				}
			}
		}
	}

	return bResult;
}

#undef LOCTEXT_NAMESPACE

