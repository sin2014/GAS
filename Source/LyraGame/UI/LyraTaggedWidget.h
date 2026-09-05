// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "GameplayTagContainer.h"

#include "LyraTaggedWidget.generated.h"

class UObject;

// 可根据所属玩家 GameplayTag 自动抑制显示的布局控件，同时保留调用方原本请求的可见性状态。
/**
 * An widget in a layout that has been tagged (can be hidden or shown via tags on the owning player)
 */
UCLASS(Abstract, Blueprintable)
class ULyraTaggedWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	ULyraTaggedWidget(const FObjectInitializer& ObjectInitializer);

	//~UWidget interface
	virtual void SetVisibility(ESlateVisibility InVisibility) override;
	//~End of UWidget interface

	//~UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~End of UUserWidget interface

protected:
	/** 所属玩家拥有任一标签时，控件使用 HiddenVisibility。 */
	/** If the owning player has any of these tags, this widget will be hidden (using HiddenVisibility) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FGameplayTagContainer HiddenByTags;

	/** 未被 GameplayTag 抑制且调用方希望显示时采用的可见性。 */
	/** The visibility to use when this widget is shown (not hidden by gameplay tags). */
	UPROPERTY(EditAnywhere, Category = "HUD")
	ESlateVisibility ShownVisibility = ESlateVisibility::Visible;

	/** 被 GameplayTag 抑制时采用的可见性。 */
	/** The visibility to use when this widget is hidden by gameplay tags. */
	UPROPERTY(EditAnywhere, Category = "HUD")
	ESlateVisibility HiddenVisibility = ESlateVisibility::Collapsed;

	/** 忽略标签抑制时，调用方最后请求的状态是否为可见。 */
	/** Do we want to be visible (ignoring tags)? */
	bool bWantsToBeVisible = true;

private:
	void OnWatchedTagsChanged();
};
