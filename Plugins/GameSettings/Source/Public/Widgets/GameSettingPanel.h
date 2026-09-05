// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "Containers/Ticker.h"
#include "GameSettingFilterState.h"
#include "GameplayTagContainer.h"
#include "Misc/ExpressionParserTypesFwd.h"

#include "GameSettingPanel.generated.h"

#define UE_API GAMESETTINGS_API

class UGameSetting;
class UGameSettingDetailView;
class UGameSettingListView;
class UGameSettingRegistry;
class UObject;
struct FFocusEvent;
struct FGeometry;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFocusedSettingChanged, UGameSetting*)

UCLASS(MinimalAPI, Abstract)
class UGameSettingPanel : public UCommonUserWidget
{
	GENERATED_BODY()

public:

	UE_API UGameSettingPanel();
	UE_API virtual void NativeOnInitialized() override;
	UE_API virtual void NativeConstruct() override;
	UE_API virtual void NativeDestruct() override;

	// 处理手柄焦点向子控件转移。
	// Focus transitioning to subwidgets for the gamepad
	UE_API virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

	/**  */
	UE_API void SetRegistry(UGameSettingRegistry* InRegistry);

	// 设置面板过滤条件，限制当前可用的设置。
	/** Sets the filter for this panel, restricting which settings are available currently. */
	UE_API void SetFilterState(const FGameSettingFilterState& InFilterState, bool bClearNavigationStack = true);

	// 根据过滤状态返回当前可见且可用的设置。
	/** Gets the currently visible and available settings based on the filter state. */
	TArray<UGameSetting*> GetVisibleSettings() const { return VisibleSettings; }

	// 返回当前导航栈是否可以返回上一层。
	/** Can we pop the current navigation stack */
	UE_API bool CanPopNavigationStack() const;

	// 弹出当前导航栈顶层。
	/** Pop the navigation stack */
	UE_API void PopNavigationStack();

	// 返回当前页面可能可用的设置集合；结果可能包含不可见设置，且不包含嵌套页面中的设置。
	/**
	 * Gets the set of settings that are potentially available on this screen.
	 * MAY CONTAIN INVISIBLE SETTINGS.
	 * DOES NOT INCLUDED NESTED PAGES.
	 */
	UE_API TArray<UGameSetting*> GetSettingsWeCanResetToDefault() const;

	UE_API void SelectSetting(const FName& SettingDevName);
	UE_API UGameSetting* GetSelectedSetting() const;

	UE_API void RefreshSettingsList();

	FOnFocusedSettingChanged OnFocusedSettingChanged;

protected:
	UE_API void RegisterRegistryEvents();
	UE_API void UnregisterRegistryEvents();
	
	UE_API void HandleSettingItemHoveredChanged(UObject* Item, bool bHovered);
	UE_API void HandleSettingItemSelectionChanged(UObject* Item);
	UE_API void FillSettingDetails(UGameSetting* InSetting);
	UE_API void HandleSettingNamedAction(UGameSetting* Setting, FGameplayTag GameSettings_Action_Tag);
	UE_API void HandleSettingNavigation(UGameSetting* Setting);
	UE_API void HandleSettingEditConditionsChanged(UGameSetting* Setting);

private:

	UPROPERTY(Transient)
	TObjectPtr<UGameSettingRegistry> Registry;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameSetting>> VisibleSettings;

	UPROPERTY(Transient)
	TObjectPtr<UGameSetting> LastHoveredOrSelectedSetting;

	UPROPERTY(Transient)
	FGameSettingFilterState FilterState;

	UPROPERTY(Transient)
	TArray<FGameSettingFilterState> FilterNavigationStack;

	FName DesiredSelectionPostRefresh;

	bool bAdjustListViewPostRefresh = true;

	// 以下为蓝图绑定的设置列表及可选详情视图。
private:	// Bound Widgets
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UGameSettingListView> ListView_Settings;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, BlueprintProtected = true, AllowPrivateAccess = true))
	TObjectPtr<UGameSettingDetailView> Details_Settings;

private:
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnExecuteNamedActionBP, UGameSetting*, Setting, FGameplayTag, Action);
	UPROPERTY(BlueprintAssignable, Category = Events, meta = (DisplayName = "On Execute Named Action"))
	FOnExecuteNamedActionBP BP_OnExecuteNamedAction;

private:
	FTSTicker::FDelegateHandle RefreshHandle;
};

#undef UE_API
