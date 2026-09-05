// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/GameSettingVisualData.h"

#include "GameSetting.h"
#include "Widgets/GameSettingListEntry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingVisualData)

// 依次按自定义选择器、开发者名称和类继承链查找最匹配的设置条目控件。
TSubclassOf<UGameSettingListEntryBase> UGameSettingVisualData::GetEntryForSetting(UGameSetting* InSetting)
{
	if (InSetting == nullptr)
	{
		return TSubclassOf<UGameSettingListEntryBase>();
	}

	// 先检查是否为该设置配置了自定义视觉元素选择器。
	// Check if there's a custom logic for finding this setting's visual element
	TSubclassOf<UGameSettingListEntryBase> CustomEntry = GetCustomEntryForSetting(InSetting);
	if (CustomEntry)
	{
		return CustomEntry;
	}

	// 再按设置开发者名称查找专用条目控件；这种特例应尽量少用。
	// Check if there's a specific entry widget for a setting by name.  Hopefully this is super rare.
	{
		TSubclassOf<UGameSettingListEntryBase> EntryWidgetClassPtr = EntryWidgetForName.FindRef(InSetting->GetDevName());
		if (EntryWidgetClassPtr)
		{
			return EntryWidgetClassPtr;
		}
	}

	// 最后沿设置类继承链查找类级条目映射，选择与该设置类型最匹配的控件。
	// Finally check to see if there's an entry for this setting following the classes we have entries for.
	// we use the super chain of the setting classes to find the most applicable entry widget for this class
	// of setting.
	for (UClass* Class = InSetting->GetClass(); Class; Class = Class->GetSuperClass())
	{
		if (TSubclassOf<UGameSetting> SettingClass = TSubclassOf<UGameSetting>(Class))
		{
			TSubclassOf<UGameSettingListEntryBase> EntryWidgetClassPtr = EntryWidgetForClass.FindRef(SettingClass);
			if (EntryWidgetClassPtr)
			{
				return EntryWidgetClassPtr;
			}
		}
	}

	return TSubclassOf<UGameSettingListEntryBase>();
}

// 合并按开发者名称与类继承链配置的详情扩展，返回当前设置适用的全部扩展类。
TArray<TSoftClassPtr<UGameSettingDetailExtension>> UGameSettingVisualData::GatherDetailExtensions(UGameSetting* InSetting)
{
	TArray<TSoftClassPtr<UGameSettingDetailExtension>> Extensions;

	// 先按设置开发者名称查找详情扩展。
	// Find extensions by setting name
	FGameSettingNameExtensions* ExtensionsWithName = ExtensionsForName.Find(InSetting->GetDevName());
	if (ExtensionsWithName)
	{
		Extensions.Append(ExtensionsWithName->Extensions);
		if (!ExtensionsWithName->bIncludeClassDefaultExtensions)
		{
			return Extensions;
		}
	}

	// 再沿设置类继承链收集适用于该设置的类级详情扩展。
	// Find extensions for it using the super chain of the setting so that we get any
	// class based extensions for this setting.
	for (UClass* Class = InSetting->GetClass(); Class; Class = Class->GetSuperClass())
	{
		if (TSubclassOf<UGameSetting> SettingClass = TSubclassOf<UGameSetting>(Class))
		{
			FGameSettingClassExtensions* ExtensionForClass = ExtensionsForClasses.Find(SettingClass);
			if (ExtensionForClass)
			{
				Extensions.Append(ExtensionForClass->Extensions);
			}
		}
	}

	return Extensions;
}

// 调用可选自定义选择器；未配置时返回空类并继续采用默认映射。
TSubclassOf<UGameSettingListEntryBase> UGameSettingVisualData::GetCustomEntryForSetting(UGameSetting* InSetting)
{
	return TSubclassOf<UGameSettingListEntryBase>();
}
