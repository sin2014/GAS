// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "Styling/SlateTypes.h"
#include "SubtitleDisplaySubsystem.h"

#include "SubtitleDisplay.generated.h"

#define UE_API GAMESUBTITLES_API

class USubtitleDisplayOptions;

struct FSubtitleFormat;

UCLASS(MinimalAPI, BlueprintType, Blueprintable, meta = (DisableNativeTick))
class USubtitleDisplay : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Display Info")
	FSubtitleFormat Format;

	UPROPERTY(EditAnywhere, Category = "Display Info")
	TObjectPtr<USubtitleDisplayOptions> Options;

	// 文本超过该宽度时换行；值为零或负数时不按固定宽度换行。
	// Whether text wraps onto a new line when it's length exceeds this width; if this value is zero or negative, no wrapping occurs.
	UPROPERTY(EditAnywhere, Category="Display Info")
	float WrapTextAt;
	
	UFUNCTION(BlueprintCallable, Category = Subtitles, Meta = (Tooltip = "True if there are subtitles currently.  False if the subtitle text is empty."))
	UE_API bool HasSubtitles() const;

	/** 是否强制使用手动字幕模式并显示 PreviewText，运行时也可启用。 */
	/** Preview text to be displayed when designing the widget */
	UPROPERTY(EditAnywhere, Category="Preview")
	bool bPreviewMode;

	/** 设计器或预览模式中显示的示例字幕文本。 */
	/** Preview text to be displayed when designing the widget */
	UPROPERTY(EditAnywhere, Category="Preview")
	FText PreviewText;

public:

	// UWidget 公共接口实现。
	// UWidget Public Interface
	UE_API virtual void SynchronizeProperties() override;
	UE_API virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
	UE_API virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const;
#endif
	// 结束 UWidget 公共接口实现。
	// End UWidget Public Interface

protected:

	// UWidget 受保护接口实现。
	// UWidget Protected Interface
	UE_API virtual TSharedRef<class SWidget> RebuildWidget() override;
	// 结束 UWidget 受保护接口实现。
	// End UWidget Protected Interface

	UE_API void HandleSubtitleDisplayOptionsChanged(const FSubtitleFormat& InDisplayFormat);
	
private:

	void RebuildStyle();

private:

	UPROPERTY(Transient)
	FTextBlockStyle GeneratedStyle;

	UPROPERTY(Transient)
	FSlateBrush GeneratedBackgroundBorder;

	/** 实际负责显示字幕数据的 Slate 子控件。 */
	/** The actual widget for displaying subtitle data */
	TSharedPtr<class SSubtitleDisplay> SubtitleWidget;
};

#undef UE_API
