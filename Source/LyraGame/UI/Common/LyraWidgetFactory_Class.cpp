// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWidgetFactory_Class.h"

#include "Blueprint/UserWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWidgetFactory_Class)

// 从数据对象实际类沿父类链查找首个控件映射；没有规则时返回空类。
TSubclassOf<UUserWidget> ULyraWidgetFactory_Class::FindWidgetClassForData_Implementation(const UObject* Data) const
{
	// 从对象当前类沿继承链向父类查找，使用首个匹配的控件构造规则。
	// Starting with the current class, work backwards to see if there are any construction rules for this class.
	for (UClass* Class = Data->GetClass(); Class; Class = Class->GetSuperClass())
	{
		TSoftClassPtr<UObject> ClassPtr(Class);
		if (const TSubclassOf<UUserWidget> EntryWidgetClassPtr = EntryWidgetForClass.FindRef(ClassPtr))
		{
			return EntryWidgetClassPtr;
		}
	}

	return TSubclassOf<UUserWidget>();
}
