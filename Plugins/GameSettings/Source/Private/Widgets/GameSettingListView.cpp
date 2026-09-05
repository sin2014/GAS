// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GameSettingListView.h"
#include "Widgets/GameSettingListEntry.h"
#include "Widgets/GameSettingVisualData.h"


#include "GameSettingCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingListView)

#if WITH_EDITOR
#include "Editor/WidgetCompilerLog.h"
#endif

#define LOCTEXT_NAMESPACE "GameSetting"

// 创建设置列表；条目类将在生成条目时依据视觉数据动态选择。
UGameSettingListView::UGameSettingListView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

// 在蓝图编译时校验视觉数据和条目类配置，缺失时记录编译错误。
void UGameSettingListView::ValidateCompiledDefaults(IWidgetCompilerLog& InCompileLog) const
{
	Super::ValidateCompiledDefaults(InCompileLog);

	if (!VisualData)
	{
		InCompileLog.Error(FText::Format(FText::FromString("{0} has no VisualData defined."), FText::FromString(GetName())));
	}
}

#endif

// 根据设置视觉数据选择条目类、生成控件并应用可选名称覆盖。
UUserWidget& UGameSettingListView::OnGenerateEntryWidgetInternal(UObject* Item, TSubclassOf<UUserWidget> DesiredEntryClass, const TSharedRef<STableViewBase>& OwnerTable)
{
	UGameSetting* SettingItem = Cast<UGameSetting>(Item);

	TSubclassOf<UGameSettingListEntryBase> SettingEntryClass = TSubclassOf<UGameSettingListEntryBase>(DesiredEntryClass);
	if (VisualData)
	{
		if (const TSubclassOf<UGameSettingListEntryBase> EntryClassSetting = VisualData->GetEntryForSetting(SettingItem))
		{
			SettingEntryClass = EntryClassSetting;
		}
		else
		{
			//UE_LOG(LogGameSettings, Error, TEXT("UGameSettingListView: No Entry Class Found!"));
		}
	}
	else
	{
		//UE_LOG(LogGameSettings, Error, TEXT("UGameSettingListView: No VisualData Defined!"));
	}

	UGameSettingListEntryBase& EntryWidget = GenerateTypedEntry<UGameSettingListEntryBase>(SettingEntryClass, OwnerTable);

	if (!IsDesignTime())
	{
		if (const FText* Override = NameOverrides.Find(SettingItem->GetDevName()))
		{
			EntryWidget.SetDisplayNameOverride(*Override);
		}

		EntryWidget.SetSetting(SettingItem);
	}

	return EntryWidget;
}

// 仅允许可见且启用的设置条目被选择或导航。
bool UGameSettingListView::OnIsSelectableOrNavigableInternal(UObject* SelectedItem)
{
	if (const UGameSettingCollection* CollectionItem = Cast<UGameSettingCollection>(SelectedItem))
	{
		return CollectionItem->IsSelectable();
	}

	return true;
}

// 按设置开发者名称注册列表显示名称覆盖。
void UGameSettingListView::AddNameOverride(const FName& DevName, const FText& OverrideName)
{
	NameOverrides.Add(DevName, OverrideName);
}

#undef LOCTEXT_NAMESPACE
