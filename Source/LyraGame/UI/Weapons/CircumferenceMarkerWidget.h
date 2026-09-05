// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "UI/Weapons/SCircumferenceMarkerWidget.h"

#include "CircumferenceMarkerWidget.generated.h"

class SWidget;
class UObject;
struct FFrame;

UCLASS()
class UCircumferenceMarkerWidget : public UWidget
{
	GENERATED_BODY()

public:
	UCircumferenceMarkerWidget(const FObjectInitializer& ObjectInitializer);

	//~UWidget interface
public:
	virtual void SynchronizeProperties() override;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~End of UWidget interface

	//~UVisual interface
public:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~End of UVisual interface
	
public:
	/** 圆周上每个标记的位置角和图像旋转角。 */
	/** The list of positions/orientations to draw the markers at. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	TArray<FCircumferenceMarkerEntry> MarkerList;

	/** 标记中心所在圆周的半径。 */
	/** The radius of the circle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=(ClampMin=0.0))
	float Radius = 48.0f;

	/** 沿圆周重复绘制的标记图像。 */
	/** The marker image to place around the circle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FSlateBrush MarkerImage;

	/** 是否将标记图像整体放到散布半径外侧，而不是让图像中心落在半径线上。 */
	/** Whether reticle corner images are placed outside the spread radius */
	// TODO：可改为 0 到 1 的对齐参数，以连续表达半径内侧、线上和外侧位置。
	//@TODO: Make this a 0-1 float alignment instead (e.g., inside/on/outside the radius)?
	UPROPERTY(EditAnywhere, Category=Corner)
	uint8 bReticleCornerOutsideSpreadRadius : 1;

public:
	/** 设置圆周半径并使底层 Slate 控件布局失效。 */
	/** Sets the radius of the circle. */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetRadius(float InRadius);

private:
	/** 实际绘制圆周标记的底层 Slate 控件。 */
	/** Internal slate widget representing the actual marker visuals */
	TSharedPtr<SCircumferenceMarkerWidget> MyMarkerWidget;
};
