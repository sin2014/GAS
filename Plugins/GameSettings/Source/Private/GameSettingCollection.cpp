// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSettingCollection.h"
#include "Templates/Casts.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingCollection)

#define LOCTEXT_NAMESPACE "GameSetting"

// 设置集合负责维护子设置并递归生成当前过滤条件下的列表内容。
//--------------------------------------
// UGameSettingCollection
//--------------------------------------

// 创建空设置集合；子设置由 AddSetting 建立父子关系并按需初始化。
UGameSettingCollection::UGameSettingCollection()
{

}

// 把子设置纳入集合、建立父子关系，并在集合已初始化时立即初始化新成员。
void UGameSettingCollection::AddSetting(UGameSetting* Setting)
{
#if !UE_BUILD_SHIPPING
	ensureAlwaysMsgf(Setting->GetSettingParent() == nullptr, TEXT("This setting already has a parent!"));
	ensureAlwaysMsgf(!Settings.Contains(Setting), TEXT("This collection already includes this setting!"));
#endif

	Settings.Add(Setting);
	Setting->SetSettingParent(this);

	if (LocalPlayer)
	{
		Setting->Initialize(LocalPlayer);
	}
}

// 从直接子设置中筛出集合类型并返回，不递归展开后代。
TArray<UGameSettingCollection*> UGameSettingCollection::GetChildCollections() const
{
	TArray<UGameSettingCollection*> CollectionSettings;

	for (UGameSetting* ChildSetting : Settings)
	{
		if (UGameSettingCollection* ChildCollection = Cast<UGameSettingCollection>(ChildSetting))
		{
			CollectionSettings.Add(ChildCollection);
		}
	}

	return CollectionSettings;
}

// 递归收集通过过滤的可见子设置；集合仅在包含可显示后代时进入结果。
void UGameSettingCollection::GetSettingsForFilter(const FGameSettingFilterState& FilterState, TArray<UGameSetting*>& InOutSettings) const
{
	for (UGameSetting* ChildSetting : Settings)
	{
		// 如果子设置是集合，仅当其中存在可见子项时才把该集合加入结果。
		// If the child setting is a collection, only add it to the set if it has any visible children.
		if (Cast<UGameSettingCollectionPage>(ChildSetting))
		{
			if (FilterState.DoesSettingPassFilter(*ChildSetting))
			{
				InOutSettings.Add(ChildSetting);
			}
		}
		else if (UGameSettingCollection* ChildCollection = Cast<UGameSettingCollection>(ChildSetting))
		{
			TArray<UGameSetting*> ChildSettings;
			ChildCollection->GetSettingsForFilter(FilterState, ChildSettings);

			if (ChildSettings.Num() > 0)
			{
				// 不要把根设置本身加入返回结果；它只是当前实际显示的若干设置和子容器的外层容器。
				// Don't add the root setting to the returned items, it's the container of N-possible 
				// other settings and containers we're actually displaying right now.
				if (!FilterState.IsSettingInRootList(ChildSetting))
				{
					InOutSettings.Add(ChildSetting);
				}

				InOutSettings.Append(ChildSettings);
			}
		}
		else
		{
			if (FilterState.DoesSettingPassFilter(*ChildSetting))
			{
				InOutSettings.Add(ChildSetting);
			}
		}
	}
}

// 分页集合把子设置隔离到独立页面，并通过导航事件请求面板切换。
//--------------------------------------
// UGameSettingCollectionPage
//--------------------------------------

// 创建分页集合；导航文本和页面说明将在初始化阶段进行完整性校验。
UGameSettingCollectionPage::UGameSettingCollectionPage()
{
}

// 初始化分页集合并校验导航文本及页面说明，避免生成无法识别的子设置入口。
void UGameSettingCollectionPage::OnInitialized()
{
	Super::OnInitialized();

#if !UE_BUILD_SHIPPING
	ensureAlwaysMsgf(!NavigationText.IsEmpty(), TEXT("You must provide a NavigationText for this setting."));
	ensureAlwaysMsgf(!DescriptionRichText.IsEmpty(), TEXT("You must provide a description for new page collections."));
#endif
}

// 按过滤器是否允许嵌套页面决定递归展开本页，避免把另一页面的设置混入当前列表。
void UGameSettingCollectionPage::GetSettingsForFilter(const FGameSettingFilterState& FilterState, TArray<UGameSetting*>& InOutSettings) const
{
	// 需要包含嵌套页面时调用基类收集全部子项；否则过滤时视为没有子项，因为这些设置会在另一页面显示。
	// If we're including nested pages, call the super and dump them all, otherwise, we pretend we have none for the filtering.
	// because our settings are displayed on another page.
	if (FilterState.bIncludeNestedPages || FilterState.IsSettingInRootList(this))
	{
		Super::GetSettingsForFilter(FilterState, InOutSettings);
	}
}

// 广播导航请求，让设置面板进入该页面包含的子设置。
void UGameSettingCollectionPage::ExecuteNavigation()
{
	OnExecuteNavigationEvent.Broadcast(this);
}

#undef LOCTEXT_NAMESPACE

