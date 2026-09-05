// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSettingRegistry.h"

#include "GameSettingCollection.h"
#include "GameSettingAction.h"
#include "UObject/WeakObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingRegistry)

#define LOCTEXT_NAMESPACE "GameSetting"

// 设置注册表实现设置树重建、名称查找、初始化检查和事件聚合。
//--------------------------------------
// UGameSettingRegistry
//--------------------------------------

// 创建空设置注册表；顶层设置和名称映射由 Initialize/Regenerate 构建。
UGameSettingRegistry::UGameSettingRegistry()
{
}

// 保存本地玩家并重新生成顶层设置；随后启动所有注册项的初始化流程。
void UGameSettingRegistry::Initialize(ULocalPlayer* InLocalPlayer)
{
	OwningLocalPlayer = InLocalPlayer;
	OnInitialize(InLocalPlayer);

	//UGameFeaturesSubsystem
}

// 清空旧注册项和映射，由派生注册表重新构建设置树并统一初始化。
void UGameSettingRegistry::Regenerate()
{
	for (UGameSetting* Setting : RegisteredSettings)
	{
		Setting->MarkAsGarbage();
	}
	RegisteredSettings.Reset();
	TopLevelSettings.Reset();

	OnInitialize(OwningLocalPlayer);
}

// 检查所有顶层设置是否完成启动，供界面决定何时可以展示。
bool UGameSettingRegistry::IsFinishedInitializing() const
{
	bool bReady = true;
	for (UGameSetting* Setting : RegisteredSettings)
	{
		if (!Setting->IsReady())
		{
			bReady = false;
			break;
		}
	}

	return bReady;
}

// 请求注册表持久化当前设置；基类保留扩展入口。
void UGameSettingRegistry::SaveChanges()
{

}

// 遍历顶层设置并按过滤状态收集当前页面可展示的条目。
void UGameSettingRegistry::GetSettingsForFilter(const FGameSettingFilterState& FilterState, TArray<UGameSetting*>& InOutSettings)
{
	TArray<UGameSetting*> RootSettings;
	if (FilterState.GetSettingRootList().Num() > 0)
	{
		RootSettings.Append(FilterState.GetSettingRootList());
	}
	else
	{
		RootSettings.Append(TopLevelSettings);
	}

	for (UGameSetting* TopLevelSetting : RootSettings)
	{
		if (const UGameSettingCollection* TopLevelCollection = Cast<UGameSettingCollection>(TopLevelSetting))
		{
			TopLevelCollection->GetSettingsForFilter(FilterState, InOutSettings);
		}
		else
		{
			if (FilterState.DoesSettingPassFilter(*TopLevelSetting))
			{
				InOutSettings.Add(TopLevelSetting);
			}
		}
	}
}

// 按稳定的开发者名称查询设置映射；未注册时返回空指针。
UGameSetting* UGameSettingRegistry::FindSettingByDevName(const FName& SettingDevName)
{
	for (UGameSetting* Setting : RegisteredSettings)
	{
		if (Setting->GetDevName() == SettingDevName)
		{
			return Setting;
		}
	}

	return nullptr;
}

// 注册顶层设置、订阅其事件，并递归登记内部子设置的名称映射。
void UGameSettingRegistry::RegisterSetting(UGameSetting* InSetting)
{
	if (InSetting)
	{
		TopLevelSettings.Add(InSetting);
		InSetting->SetRegistry(this);
		RegisterInnerSettings(InSetting);
	}
}

// 递归登记设置及其后代，校验开发者名称唯一，并绑定事件转发。
void UGameSettingRegistry::RegisterInnerSettings(UGameSetting* InSetting)
{
	InSetting->OnSettingChangedEvent.AddUObject(this, &ThisClass::HandleSettingChanged);
	InSetting->OnSettingAppliedEvent.AddUObject(this, &ThisClass::HandleSettingApplied);
	InSetting->OnSettingEditConditionChangedEvent.AddUObject(this, &ThisClass::HandleSettingEditConditionsChanged);

	// 这里集中转发命名动作事件，虽然增加了一层聚合，但能简化注册表使用方的绑定。
	// Not a fan of this, but it makes sense to aggregate action events for simplicity.
	if (UGameSettingAction* ActionSetting = Cast<UGameSettingAction>(InSetting))
	{
		ActionSetting->OnExecuteNamedActionEvent.AddUObject(this, &ThisClass::HandleSettingNamedAction);
	}
	// 这里集中转发导航事件，虽然增加了一层聚合，但能简化注册表使用方的绑定。
	// Not a fan of this, but it makes sense to aggregate navigation events for simplicity.
	else if (UGameSettingCollectionPage* NewPageCollection = Cast<UGameSettingCollectionPage>(InSetting))
	{
		NewPageCollection->OnExecuteNavigationEvent.AddUObject(this, &ThisClass::HandleSettingNavigation);
	}

#if !UE_BUILD_SHIPPING
	ensureAlwaysMsgf(!RegisteredSettings.Contains(InSetting), TEXT("This setting has already been registered!"));
	ensureAlwaysMsgf(nullptr == RegisteredSettings.FindByPredicate([&InSetting](UGameSetting* ExistingSetting) { return InSetting->GetDevName() == ExistingSetting->GetDevName(); }), TEXT("A setting with this DevName has already been registered!  DevNames must be unique within a registry."));
#endif

	RegisteredSettings.Add(InSetting);

	for (UGameSetting* ChildSetting : InSetting->GetChildSettings())
	{
		RegisterInnerSettings(ChildSetting);
	}
}

// 把单项应用事件转发为注册表级事件。
void UGameSettingRegistry::HandleSettingApplied(UGameSetting* Setting)
{
	OnSettingApplied(Setting);
}

// 把设置值及变化原因转发给注册表监听者。
void UGameSettingRegistry::HandleSettingChanged(UGameSetting* Setting, EGameSettingChangeReason Reason)
{
	OnSettingChangedEvent.Broadcast(Setting, Reason);
}

// 转发设置可见性、可编辑性或选项变化事件。
void UGameSettingRegistry::HandleSettingEditConditionsChanged(UGameSetting* Setting)
{
	OnSettingEditConditionChangedEvent.Broadcast(Setting);
}

// 转发设置触发的命名动作及其 GameplayTag。
void UGameSettingRegistry::HandleSettingNamedAction(UGameSetting* Setting, FGameplayTag GameSettings_Action_Tag)
{
	OnSettingNamedActionEvent.Broadcast(Setting, GameSettings_Action_Tag);
}

// 转发进入子设置页面的导航请求。
void UGameSettingRegistry::HandleSettingNavigation(UGameSetting* Setting)
{
	OnExecuteNavigationEvent.Broadcast(Setting);
}

#undef LOCTEXT_NAMESPACE

