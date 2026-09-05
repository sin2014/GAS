// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSetting.h"

#include "GameSettingCollection.generated.h"

#define UE_API GAMESETTINGS_API

struct FGameSettingFilterState;

// 设置集合拥有若干子设置，并负责按过滤条件递归收集可显示条目。
//--------------------------------------
// UGameSettingCollection
//--------------------------------------

UCLASS(MinimalAPI)
class UGameSettingCollection : public UGameSetting
{
	GENERATED_BODY()

public:
	UE_API UGameSettingCollection();

	virtual TArray<UGameSetting*> GetChildSettings() override { return Settings; }
	UE_API TArray<UGameSettingCollection*> GetChildCollections() const;

	UE_API void AddSetting(UGameSetting* Setting);
	UE_API virtual void GetSettingsForFilter(const FGameSettingFilterState& FilterState, TArray<UGameSetting*>& InOutSettings) const;

	virtual bool IsSelectable() const { return false; }

protected:
	// 该集合直接拥有的设置。
	/** The settings owned by this collection. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameSetting>> Settings;
};

//--------------------------------------
// 分页集合代表需要通过导航进入的独立子设置页面。
// UGameSettingCollectionPage
//--------------------------------------

UCLASS(MinimalAPI)
class UGameSettingCollectionPage : public UGameSettingCollection
{
	GENERATED_BODY()

public:

	DECLARE_EVENT_OneParam(UGameSettingCollectionPage, FOnExecuteNavigation, UGameSetting* /*Setting*/);
	FOnExecuteNavigation OnExecuteNavigationEvent;

public:
	UE_API UGameSettingCollectionPage();

	FText GetNavigationText() const { return NavigationText; }
	void SetNavigationText(FText Value) { NavigationText = Value; }
#if !UE_BUILD_SHIPPING
	void SetNavigationText(const FString& Value) { SetNavigationText(FText::FromString(Value)); }
#endif
	
	UE_API virtual void OnInitialized() override;
	UE_API virtual void GetSettingsForFilter(const FGameSettingFilterState& FilterState, TArray<UGameSetting*>& InOutSettings) const override;
	virtual bool IsSelectable() const override { return true; }

	/**  */
	UE_API void ExecuteNavigation();

private:
	FText NavigationText;
};

#undef UE_API
