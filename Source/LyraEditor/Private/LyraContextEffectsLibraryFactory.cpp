// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraContextEffectsLibraryFactory.h"

#include "Feedback/ContextEffects/LyraContextEffectsLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraContextEffectsLibraryFactory)

class FFeedbackContext;
class UClass;
class UObject;

// 配置工厂支持新建 ContextEffectsLibrary 资产，并设置 SupportedClass、文本导入和编辑后打开选项。
ULyraContextEffectsLibraryFactory::ULyraContextEffectsLibraryFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = ULyraContextEffectsLibrary::StaticClass();

	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

// 在指定父包中创建带传入名称与标志的 ContextEffectsLibrary 对象。
UObject* ULyraContextEffectsLibraryFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	ULyraContextEffectsLibrary* LyraContextEffectsLibrary = NewObject<ULyraContextEffectsLibrary>(InParent, Name, Flags);

	return LyraContextEffectsLibrary;
}
