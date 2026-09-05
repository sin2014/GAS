// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Components/SlateWrapperTypes.h"
#include "GameSettingFilterState.h"
#include "GameplayTagContainer.h"

#include "GameSetting.generated.h"

#define UE_API GAMESETTINGS_API

class ULocalPlayer;
class UGameSettingRegistry;

// 设置基类统一管理生命周期、父子关系、编辑条件、搜索文本和变化通知。
//--------------------------------------
// UGameSetting
//--------------------------------------

DECLARE_DELEGATE_RetVal_OneParam(FText, FGetGameSettingsDetails, ULocalPlayer& /*InLocalPlayer*/);

/**
 * 
 */
UCLASS(MinimalAPI, Abstract, BlueprintType)
class UGameSetting : public UObject
{
	GENERATED_BODY()

public:
	UGameSetting() { }

public:
	DECLARE_EVENT_TwoParams(UGameSetting, FOnSettingChanged, UGameSetting* /*InSetting*/, EGameSettingChangeReason /*InChangeReason*/);
	DECLARE_EVENT_OneParam(UGameSetting, FOnSettingApplied, UGameSetting* /*InSetting*/);
	DECLARE_EVENT_OneParam(UGameSetting, FOnSettingEditConditionChanged, UGameSetting* /*InSetting*/);

	FOnSettingChanged OnSettingChangedEvent;
	FOnSettingApplied OnSettingAppliedEvent;
	FOnSettingEditConditionChanged OnSettingEditConditionChangedEvent;

public:

	// 返回设置的非本地化开发者名称；该名称应保持不变，并在当前注册表内唯一标识此设置。
	/**
	 * Gets the non-localized developer name for this setting.  This should remain constant, and represent a 
	 * unique identifier for this setting inside this settings registry.
	 */
	UFUNCTION(BlueprintCallable)
	FName GetDevName() const { return DevName; }
	void SetDevName(const FName& Value) { DevName = Value; }

	bool GetAdjustListViewPostRefresh() const { return bAdjustListViewPostRefresh; }
	void SetAdjustListViewPostRefresh(const bool Value) { bAdjustListViewPostRefresh = Value; }

	UFUNCTION(BlueprintCallable)
	FText GetDisplayName() const { return DisplayName; }
	void SetDisplayName(const FText& Value) { DisplayName = Value; }
#if !UE_BUILD_SHIPPING
	void SetDisplayName(const FString& Value) { SetDisplayName(FText::FromString(Value)); }
#endif
	UFUNCTION(BlueprintCallable)
	ESlateVisibility GetDisplayNameVisibility() { return DisplayNameVisibility; }
	void SetNameDisplayVisibility(ESlateVisibility InVisibility) { DisplayNameVisibility = InVisibility; }

	UFUNCTION(BlueprintCallable)
	FText GetDescriptionRichText() const { return DescriptionRichText; }
	void SetDescriptionRichText(const FText& Value) { DescriptionRichText = Value; InvalidateSearchableText(); }
#if !UE_BUILD_SHIPPING
	// 此重载仅供作弊项等非 Shipping 内容使用；Shipping 构建禁止使用，以防引入未本地化文本。
	/** This version is for cheats and other non-shipping items, that don't need to localize their text.  We don't permit this in shipping to prevent unlocalized text being introduced. */
	void SetDescriptionRichText(const FString& Value) { SetDescriptionRichText(FText::FromString(Value)); }
#endif

	UFUNCTION(BlueprintCallable)
	const FGameplayTagContainer& GetTags() const { return Tags; }
	void AddTag(const FGameplayTag& TagToAdd) { Tags.AddTag(TagToAdd); }

	void SetRegistry(UGameSettingRegistry* InOwningRegistry) { OwningRegistry = InOwningRegistry; }

	// 返回用于搜索的说明纯文本。
	/** Gets the searchable plain text for the description. */
	UE_API const FString& GetDescriptionPlainText() const;

	// 使用所属本地玩家初始化设置；容器会自动初始化加入其中的设置。
	/** Initializes the setting, giving it the owning local player.  Containers automatically initialize settings added to them. */
	UE_API void Initialize(ULocalPlayer* InLocalPlayer);

	// 返回设置所属的本地玩家；所有已初始化设置都应持有该对象。
	/** Gets the owning local player for this setting - which all initialized settings will have. */
	ULocalPlayer* GetOwningLocalPlayer() const { return LocalPlayer; }
	
	// 设置动态详情回调；构建说明面板时调用，其返回文本不参与搜索。
	/** Set the dynamic details callback, we query this when building the description panel.  This text is not searchable.*/
	void SetDynamicDetails(const FGetGameSettingsDetails& InDynamicDetails) { DynamicDetails = InDynamicDetails; }

	// 返回设置的动态详情，例如账户剩余退款次数或账户编号。
	/**
	 * Gets the dynamic details about this setting.  This may be information like, how many refunds are remaining 
	 * on their account, or the account number.
	 */
	UFUNCTION(BlueprintCallable)
	UE_API FText GetDynamicDetails() const;

	UFUNCTION(BlueprintCallable)
	FText GetWarningRichText() const { return WarningRichText; }
	void SetWarningRichText(const FText& Value) { WarningRichText = Value; InvalidateSearchableText(); }
#if !UE_BUILD_SHIPPING
	// 此重载仅供作弊项等非 Shipping 内容使用；Shipping 构建禁止使用，以防引入未本地化文本。
	/** This version is for cheats and other non-shipping items, that don't need to localize their text.  We don't permit this in shipping to prevent unlocalized text being introduced. */
	void SetWarningRichText(const FString& Value) { SetWarningRichText(FText::FromString(Value)); }
#endif

	// 综合当前编辑条件和附加过滤状态，返回该设置的可见性与可编辑状态。
	/**
	 * Gets the edit state of this property based on the current state of its edit conditions as well as any additional
	 * filter state.
	 */
	const FGameSettingEditableState& GetEditState() const { return EditableStateCache; }

	// 为设置添加编辑条件，用于控制其可见性和可编辑性。
	/** Adds a new edit condition to this setting, allowing you to control the visibility and edit-ability of this setting. */
	UE_API void AddEditCondition(const TSharedRef<FGameSettingEditCondition>& InEditCondition);

	// 添加设置依赖；依赖项变化时重新评估当前设置的编辑条件。
	/** Add setting dependency, if these settings change, we'll re-evaluate edit conditions for this setting. */
	UE_API void AddEditDependency(UGameSetting* DependencySetting);

	// 设置的父级拥有者通常是集合，顶层设置则由注册表拥有。
	/** The parent object that owns the setting, in most cases the collection, but for top level settings the registry. */
	UE_API void SetSettingParent(UGameSetting* InSettingParent);
	UGameSetting* GetSettingParent() const { return SettingParent; }

	// 该设置是否应上报到分析系统。
	/** Should this setting be reported to analytics. */
	bool GetIsReportedToAnalytics() const { return bReportAnalytics; }
	void SetIsReportedToAnalytics(bool bReport) { bReportAnalytics = bReport; }

	// 返回该设置用于分析上报的值。
	/** Gets the analytics value for this setting. */
	virtual FString GetAnalyticsValue() const { return TEXT(""); }

	// 某些设置需要异步初始化；设置系统会等待所有设置就绪后再显示界面。
	/**
	 * Some settings may take an async amount of time to finish initializing.  The settings system will wait
	 * for all settings to be ready before showing the setting.
	 */
	bool IsReady() const { return bReady; }

	// 任何设置都可以包含子设置，以支持不会直接列在面板中的集合或动作；这些内部设置仍可在其他界面被修改，并需要保存初始值及支持恢复。
	/**
	 * Any setting can have children, this is so we can allow for the possibility of "collections" or "actions" that
	 * are not directly visible to the user, but are set by some means and need to have initial and restored values.
	 * In that case, you would likely have internal settings inside an action subclass that is set on another screen,
	 * but never directly listed on the settings panel.
	 */
	virtual TArray<UGameSetting*> GetChildSettings() { return TArray<UGameSetting*>(); }

	// 重新计算设置的可编辑状态并广播变化，使正在显示该设置的 UI 刷新选项和状态。
	/**
	 * Refresh the editable state of the setting and notify that the state has changed so that any UI currently
	 * examining this setting is updated with the new options, or whatever.
	 */
	UE_API void RefreshEditableState(bool bNotifyEditConditionsChanged = true);

	// 设置通常会立即修改实时值；少数设置会先写入临时状态，待稍后应用，例如新的屏幕分辨率。
	/**
	 * We expect settings to change the live value immediately, but occasionally there are special settings
	 * that go are immediately stored to a temporary location but we don't actually apply them until later
	 * like selecting a new resolution.
	 */
	UE_API void Apply();

	// 返回设置所属本地玩家当前所在的世界。
	/** Gets the current world of the local player that owns these settings. */
	UE_API virtual UWorld* GetWorld() const override;

protected:
	/**  */
	UE_API virtual void Startup();
	UE_API void StartupComplete();

	UE_API virtual void OnInitialized();
	UE_API virtual void OnApply();
	UE_API virtual void OnGatherEditState(FGameSettingEditableState& InOutEditState) const;
	UE_API virtual void OnDependencyChanged();

	/**  */
	UE_API virtual FText GetDynamicDetailsInternal() const;

	/** */
	UE_API void HandleEditDependencyChanged(UGameSetting* DependencySetting, EGameSettingChangeReason Reason);
	UE_API void HandleEditDependencyChanged(UGameSetting* DependencySetting);

	// 可搜索纯文本被标记为脏时重新生成缓存。
	/** Regenerates the plain searchable text if it has been dirtied. */
	UE_API void RefreshPlainText() const;
	void InvalidateSearchableText() { bRefreshPlainSearchableText = true; }

	// 通知设置值已经变化。
	/** Notify that the setting changed */
	UE_API void NotifySettingChanged(EGameSettingChangeReason Reason);
	UE_API virtual void OnSettingChanged(EGameSettingChangeReason Reason);

	// 通知设置编辑条件已经变化；设置可能因此隐藏、禁用或更新可选项。
	/** Notify that the settings edit conditions changed.  This may mean it's now invisible, or disabled, or possibly that the options have changed in some meaningful way. */
	UE_API void NotifyEditConditionsChanged();
	UE_API virtual void OnEditConditionsChanged();

	/**  */
	UE_API FGameSettingEditableState ComputeEditableState() const;

protected:

	UPROPERTY(Transient)
	TObjectPtr<ULocalPlayer> LocalPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UGameSetting> SettingParent;

	UPROPERTY(Transient)
	TObjectPtr<UGameSettingRegistry> OwningRegistry;

	FName DevName;
	FText DisplayName;
	ESlateVisibility DisplayNameVisibility = ESlateVisibility::SelfHitTestInvisible;
	FText DescriptionRichText;
	FText WarningRichText;

	// 设置标签集合，可作为 UI 采用不同显示或交互方式的任意标志。
	/** A collection of tags for the settings.  These can just be arbitrary flags used by the UI to do different things. */
	FGameplayTagContainer Tags;

	FGetGameSettingsDetails DynamicDetails;

	// 附加到该设置的全部编辑条件。
	/** Any edit conditions for this setting. */
	TArray<TSharedRef<FGameSettingEditCondition>> EditConditions;

	class FStringCultureCache
	{
		FStringCultureCache(TFunction<FString()> InStringGetter);

		void Invalidate();

		FString Get() const;

	private:
		mutable FString StringCache;
		mutable FCultureRef Culture;
		TFunction<FString()> StringGetter;
	};

	// 显示文本变化时使可搜索文本缓存失效。
	/** When the text changes, we invalidate the searchable text. */
	mutable bool bRefreshPlainSearchableText = true;
	// 设置富文本说明时自动生成对应纯文本。
	/** When we set the rich text for a setting, we automatically generate the plain text. */
	mutable FString AutoGenerated_DescriptionPlainText;

	// 是否参与分析上报；默认仅 GameSettingValue 类型上报。
	/** Report as part of analytics, by default no setting reports, except GameSettingValues. */
	bool bReportAnalytics = false;

private:

	// 大多数设置立即就绪，少数设置需要完成启动过程后才能安全调用。
	/** Most settings are immediately ready, but some may require startup time before it's safe to call their functions. */
	bool bReady = false;

	// 防止广播设置值变化时发生重入。
	/** Prevent re-entrancy problems when announcing a setting has changed. */
	bool bOnSettingChangedEventGuard = false;

	// 防止广播编辑条件变化时发生重入。
	/** Prevent re-entrancy problems when announcing a setting has changed edit conditions. */
	bool bOnEditConditionsChangedEventGuard = false;

	/**  */
	bool bAdjustListViewPostRefresh = true;

	// 设置状态变化时缓存可编辑状态，避免每次查询都重新计算。
	/** We cache the editable state of a setting when it changes rather than reprocessing it any time it's needed.  */
	FGameSettingEditableState EditableStateCache;
};

#undef UE_API
