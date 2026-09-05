// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraTextHotfixConfig.h"
#include "Internationalization/PolyglotTextData.h"
#include "Internationalization/TextLocalizationManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTextHotfixConfig)

// 构造用于注册 Polyglot 文本替换数据的配置对象。
ULyraTextHotfixConfig::ULyraTextHotfixConfig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 把当前 TextReplacements 注册到文本本地化管理器，使热修复文本参与查找。
void ULyraTextHotfixConfig::ApplyTextReplacements() const
{
	FTextLocalizationManager::Get().RegisterPolyglotTextData(TextReplacements);
}

// 属性初始化完成后立即注册当前文本替换。
void ULyraTextHotfixConfig::PostInitProperties()
{
	Super::PostInitProperties();
	ApplyTextReplacements();
}

// 配置重新加载后重新注册文本替换，使热修复值立即生效。
void ULyraTextHotfixConfig::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);
	ApplyTextReplacements();
}

#if WITH_EDITOR
// 编辑器属性变化后重新注册文本替换，实时更新预览结果。
void ULyraTextHotfixConfig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyTextReplacements();
}
#endif

