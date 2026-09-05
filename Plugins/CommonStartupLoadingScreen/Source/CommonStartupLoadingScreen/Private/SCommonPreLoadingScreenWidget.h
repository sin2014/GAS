// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/GCObject.h"
#include "Widgets/Accessibility/SlateWidgetAccessibleTypes.h"
#include "Widgets/SCompoundWidget.h"

class FReferenceCollector;

class SCommonPreLoadingScreenWidget : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SCommonPreLoadingScreenWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

	//~ FGCObject 接口开始
	//~ Begin FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	//~ FGCObject 接口结束
	//~ End FGCObject interface

private:

};
