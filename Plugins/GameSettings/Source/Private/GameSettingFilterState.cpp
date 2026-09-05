// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSettingFilterState.h"
#include "GameSetting.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingFilterState)

#define LOCTEXT_NAMESPACE "GameSetting"

class FSettingFilterExpressionContext : public ITextFilterExpressionContext
{
public:
	// 保存待过滤设置的只读引用，供后续文本表达式匹配使用。
	explicit FSettingFilterExpressionContext(const UGameSetting& InSetting) : Setting(InSetting) {}

	// 将基础搜索表达式与设置的纯文本说明进行匹配，并遵循调用方指定的文本比较模式。
	virtual bool TestBasicStringExpression(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode) const override
	{
		return TextFilterUtils::TestBasicStringExpression(Setting.GetDescriptionPlainText(), InValue, InTextComparisonMode);
	}

	// 当前过滤器不支持“键-值”复杂表达式，因此始终返回不匹配。
	virtual bool TestComplexExpression(const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode) const override
	{
		return false;
	}

private:
	// 当前正在接受过滤的设置。
	/** Setting being filtered. */
	const UGameSetting& Setting;
};

//--------------------------------------
// 设置过滤状态组合根列表、允许列表、可见性与文本搜索条件。
// FGameSettingFilterState
//--------------------------------------

// 创建仅支持基础字符串表达式的搜索求值器，复杂键值表达式不在当前过滤范围内。
FGameSettingFilterState::FGameSettingFilterState()
	: SearchTextEvaluator(ETextFilterExpressionEvaluatorMode::BasicString)
{
}

// 把设置加入当前页面根列表，用于限制过滤遍历的起点。
void FGameSettingFilterState::AddSettingToRootList(UGameSetting* InSetting)
{
	SettingAllowList.Add(InSetting);
	SettingRootList.Add(InSetting);
}

// 把设置加入允许列表；列表非空时仅这些设置可以通过过滤。
void FGameSettingFilterState::AddSettingToAllowList(UGameSetting* InSetting)
{
	SettingAllowList.Add(InSetting);
}

// 更新搜索关键字，并记录是否需要执行文本匹配。
void FGameSettingFilterState::SetSearchText(const FString& InSearchText)
{
	SearchTextEvaluator.SetFilterText(FText::FromString(InSearchText));
}

// 依次检查根列表、允许列表、可见性和搜索文本；任一约束不满足时即拒绝该设置。
bool FGameSettingFilterState::DoesSettingPassFilter(const UGameSetting& InSetting) const
{
	const FGameSettingEditableState& EditableState = InSetting.GetEditState();

	if (!bIncludeHidden && !EditableState.IsVisible())
	{
		return false;
	}

	if (!bIncludeDisabled && !EditableState.IsEnabled())
	{
		return false;
	}

	if (!bIncludeResetable && !EditableState.IsResetable())
	{
		return false;
	}

	// 判断当前是否启用了任一设置过滤条件。
	// Are we filtering settings?
	if (SettingAllowList.Num() > 0)
	{
		if (!SettingAllowList.Contains(&InSetting))
		{
			bool bAllowed = false;
			const UGameSetting* NextSetting = &InSetting;
			while (const UGameSetting* Parent = NextSetting->GetSettingParent())
			{
				if (SettingAllowList.Contains(Parent))
				{
					bAllowed = true;
					break;
				}

				NextSetting = Parent;
			}

			if (!bAllowed)
			{
				return false;
			}
		}
	}

	// TODO：后续可扩展更多过滤条件。
	// TODO more filters...

	// 文本搜索通常代价最高，因此始终放在其他过滤条件之后执行。
	// Always search text last, it's generally the most expensive filter.
	if (!SearchTextEvaluator.TestTextFilter(FSettingFilterExpressionContext(InSetting)))
	{
		return false;
	}

	return true;
}

//--------------------------------------
// 可编辑状态记录设置是否可见、可交互、可重置以及禁用原因。
// FGameSettingsEditableState
//--------------------------------------

// 把设置标记为不可见，并记录仅供开发者诊断的隐藏原因。
void FGameSettingEditableState::Hide(const FString& DevReason)
{
#if !UE_BUILD_SHIPPING
	ensureAlwaysMsgf(!DevReason.IsEmpty(), TEXT("To hide a setting, you must provide a developer reason."));
#endif

	bVisible = false;

#if !UE_BUILD_SHIPPING
	HiddenReasons.Add(DevReason);
#endif
}

// 把设置标记为不可交互，并累积面向用户展示的禁用原因。
void FGameSettingEditableState::Disable(const FText& Reason)
{
#if !UE_BUILD_SHIPPING
	ensureAlwaysMsgf(!Reason.IsEmpty(), TEXT("To disable a setting, you must provide a reason that we can show players."));
#endif

	bEnabled = false;
	DisabledReasons.Add(Reason);
}

// 将指定离散选项加入隐藏集合，使其不再作为可选值呈现。
void FGameSettingEditableState::DisableOption(const FString& Option)
{
#if !UE_BUILD_SHIPPING
	ensureAlwaysMsgf(!DisabledOptions.Contains(Option), TEXT("You've already disabled this option."));
#endif

	DisabledOptions.Add(Option);
}

// 把设置标记为不可恢复默认值，供重置操作过滤。
void FGameSettingEditableState::UnableToReset()
{
	bResetable = false;
}

#undef LOCTEXT_NAMESPACE

