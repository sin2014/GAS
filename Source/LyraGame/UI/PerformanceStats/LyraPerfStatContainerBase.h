// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUserWidget.h"
#include "Performance/LyraPerformanceStatTypes.h"

#include "LyraPerfStatContainerBase.generated.h"

class UObject;
struct FFrame;

// 包含一组性能统计子控件的面板，并按用户设置以及本容器负责的文本或图表模式更新子控件可见性。
/**
 * ULyraPerfStatsContainerBase
 *
 * Panel that contains a set of ULyraPerfStatWidgetBase widgets and manages
 * their visibility based on user settings.
 */
 UCLASS(Abstract)
class ULyraPerfStatContainerBase : public UCommonUserWidget
{
public:
	ULyraPerfStatContainerBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	GENERATED_BODY()

	//~UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~End of UUserWidget interface

	UFUNCTION(BlueprintCallable)
	void UpdateVisibilityOfChildren();

protected:
	// 此容器承载的统计表现类型，用于筛选 TextOnly、GraphOnly 或两者兼有的用户设置。
	// Are we showing text or graph stats?
	UPROPERTY(EditAnywhere, Category=Display)
	ELyraStatDisplayMode StatDisplayModeFilter = ELyraStatDisplayMode::TextAndGraph;
};
