// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class ISlateStyle;

/** Lyra 编辑器工具栏和菜单使用的 Slate 样式集合。 */
/** Slate style used by the Game Editor */
class FGameEditorStyle
{
public:

	static void Initialize();

	static void Shutdown();
	
	static const ISlateStyle& Get();

	static FName GetStyleSetName();

private:

	static TSharedRef< class FSlateStyleSet > Create();

private:

	static TSharedPtr< class FSlateStyleSet > StyleInstance;
};
