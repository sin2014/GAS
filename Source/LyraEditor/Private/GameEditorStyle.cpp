// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameEditorStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

// GameEditorStyle 的进程级 Slate 样式实例，在模块启动和关闭时注册与释放。
TSharedPtr< FSlateStyleSet > FGameEditorStyle::StyleInstance = nullptr;

// StyleInstance 尚未创建时构建并注册 GameEditorStyle；重复调用不重复注册。
void FGameEditorStyle::Initialize()
{
	if ( !StyleInstance.IsValid() )
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle( *StyleInstance );
	}
}

// 从 SlateStyleRegistry 注销样式并释放 StyleInstance。
void FGameEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle( *StyleInstance );
	ensure( StyleInstance.IsUnique() );
	StyleInstance.Reset();
}

// 返回缓存的 GameEditorStyle 名称，供图标与工具栏查找样式集合。
FName FGameEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("GameEditorStyle"));
	return StyleSetName;
}

#define IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( FPaths::EngineContentDir() / "Editor/Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define BOX_BRUSH( RelativePath, ... ) FSlateBoxBrush( FPaths::EngineContentDir() / "Editor/Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush( FPaths::EngineContentDir() / "Editor/Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )

#define GAME_IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush( FPaths::ProjectContentDir() / "Editor/Slate"/ RelativePath + TEXT(".png"), __VA_ARGS__ )
#define GAME_IMAGE_BRUSH_SVG( RelativePath, ... ) FSlateVectorImageBrush( FPaths::ProjectContentDir() / "Editor/Slate"/ RelativePath + TEXT(".svg"), __VA_ARGS__ )

// 创建 SlateStyleSet，设置插件 Resources 目录并注册 CheckContent 工具栏图标。
TSharedRef< FSlateStyleSet > FGameEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> StyleRef = MakeShareable(new FSlateStyleSet(FGameEditorStyle::GetStyleSetName()));
	StyleRef->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleRef->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	FSlateStyleSet& Style = StyleRef.Get();

	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon64x64(64.0f, 64.0f);

	// 注册“检查内容”等编辑器工具栏图标资源。
	// Toolbar
	{
		Style.Set("GameEditor.CheckContent", new GAME_IMAGE_BRUSH_SVG("Icons/CheckContent", Icon20x20));
	}

	return StyleRef;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH

// 返回已初始化的 StyleInstance；未初始化时触发检查失败。
const ISlateStyle& FGameEditorStyle::Get()
{
	return *StyleInstance;
}
