// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "LyraTextHotfixConfig.generated.h"

struct FPolyglotTextData;
struct FPropertyChangedEvent;

// 通过配置中的 FPolyglotTextData 条目替换任意已注册 FText，使文本内容可随配置热修复更新。
/**
 * This class allows hotfixing individual FText values anywhere
 */

UCLASS(config=Game, defaultconfig)
class ULyraTextHotfixConfig : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULyraTextHotfixConfig(const FObjectInitializer& ObjectInitializer);

	// UObject interface
	virtual void PostInitProperties() override;
	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// End of UObject interface

private:
	void ApplyTextReplacements() const;

private:
	// 需要注册并应用的多语言文本替换列表。
	// The list of FText values to hotfix
	UPROPERTY(Config, EditAnywhere)
	TArray<FPolyglotTextData> TextReplacements;
};
