// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_LyraContextEffectsLibrary.h"

#include "Feedback/ContextEffects/LyraContextEffectsLibrary.h"

class UClass;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

// 返回 ContextEffectsLibrary 的 UClass，使资产工具把该操作绑定到正确资产类型。
UClass* FAssetTypeActions_LyraContextEffectsLibrary::GetSupportedClass() const
{
	return ULyraContextEffectsLibrary::StaticClass();
}

#undef LOCTEXT_NAMESPACE
