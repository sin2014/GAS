// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraEditor.h"

#include "AbilitySystemGlobals.h"
#include "DataValidationModule.h"
#include "Development/LyraDeveloperSettings.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameEditorStyle.h"
#include "GameModes/LyraExperienceManager.h"
#include "GameplayAbilitiesEditorModule.h"
#include "GameplayCueInterface.h"
#include "GameplayCueNotify_Burst.h"
#include "GameplayCueNotify_BurstLatent.h"
#include "GameplayCueNotify_Looping.h"
#include "Private/AssetTypeActions_LyraContextEffectsLibrary.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "UObject/UObjectIterator.h"
#include "UnrealEdGlobals.h"
#include "Validation/EditorValidator.h"

class SWidget;

#define LOCTEXT_NAMESPACE "LyraEditor"

// Lyra 编辑器模块、工具栏和验证入口使用的日志分类。
DEFINE_LOG_CATEGORY(LogLyraEditor);

// 向 GameplayCue 编辑器提供新建 Notify 时允许选择的 Lyra 基类。
// This function tells the GameplayCue editor what classes to expose when creating new notifies.
static void GetGameplayCueDefaultClasses(TArray<UClass*>& Classes)
{
	Classes.Empty();
	Classes.Add(UGameplayCueNotify_Burst::StaticClass());
	Classes.Add(AGameplayCueNotify_BurstLatent::StaticClass());
	Classes.Add(AGameplayCueNotify_Looping::StaticClass());
}

// 告诉 GameplayCue 编辑器应在哪些类中扫描 GameplayCue 事件实现。
// This function tells the GameplayCue editor what classes to search for GameplayCue events.
static void GetGameplayCueInterfaceClasses(TArray<UClass*>& Classes)
{
	Classes.Empty();

	for (UClass* Class : TObjectRange<UClass>())
	{
		if (Class->IsChildOf<AActor>() && Class->ImplementsInterface(UGameplayCueInterface::StaticClass()))
		{
			Classes.Add(Class);
		}
	}
}

// 根据 GameplayCueTag 决定新 Notify 资产的创建目录。
// This function tells the GameplayCue editor where to create the GameplayCue notifies based on their tag.
static FString GetGameplayCuePath(FString GameplayCueTag)
{
	FString Path = FString(TEXT("/Game"));

	//@TODO：根据 Tag 根节点尝试定位插件目录，使插件 GameplayCue 创建在所属插件内容中。
	//@TODO: Try plugins (e.g., GameplayCue.ShooterGame.Foo should be in ShooterCore or something)

	// 无插件映射时使用 UAbilitySystemGlobals::GameplayCueNotifyPaths 的第一个目录。
	// Default path to the first entry in the UAbilitySystemGlobals::GameplayCueNotifyPaths.
	if (IGameplayAbilitiesModule::IsAvailable())
	{
		IGameplayAbilitiesModule& GameplayAbilitiesModule = IGameplayAbilitiesModule::Get();

		if (GameplayAbilitiesModule.IsAbilitySystemGlobalsAvailable())
		{
			UAbilitySystemGlobals* AbilitySystemGlobals = GameplayAbilitiesModule.GetAbilitySystemGlobals();
			check(AbilitySystemGlobals);

			TArray<FString> GetGameplayCueNotifyPaths = AbilitySystemGlobals->GetGameplayCueNotifyPaths();

			if (GetGameplayCueNotifyPaths.Num() > 0)
			{
				Path = GetGameplayCueNotifyPaths[0];
			}
		}
	}

	GameplayCueTag.RemoveFromStart(TEXT("GameplayCue."));

	FString NewDefaultPathName = FString::Printf(TEXT("%s/GCN_%s"), *Path, *GameplayCueTag);

	return NewDefaultPathName;
}

// 当前编辑器存在活动 PIE/SIE PlayWorld 时返回 true。
static bool HasPlayWorld()
{
	return GEditor->PlayWorld != nullptr;
}

// 当前没有活动 PlayWorld 时返回 true，用于限制只能在非 PIE 状态执行的菜单操作。
static bool HasNoPlayWorld()
{
	return !HasPlayWorld();
}

// PlayWorld 存在且未因蓝图调试暂停时返回 true。
static bool HasPlayWorldAndRunning()
{
	return HasPlayWorld() && !GUnrealEd->PlayWorld->bDebugPauseExecution;
}

// 校验地图软路径非空后，通过 AssetEditorSubsystem 在编辑器中打开该地图。
static void OpenCommonMap_Clicked(const FString MapPath)
{
	if (ensure(MapPath.Len()))
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(MapPath);
	}
}

// 仅在未运行 PIE 且开发设置配置了 CommonEditorMaps 时显示常用地图菜单。
static bool CanShowCommonMaps()
{
	return HasNoPlayWorld() && !GetDefault<ULyraDeveloperSettings>()->CommonEditorMaps.IsEmpty();
}

// 根据 CommonEditorMaps 构建下拉菜单，忽略无效路径，并为每个地图创建打开命令。
static TSharedRef<SWidget> GetCommonMapsDropdown()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	
	for (const FSoftObjectPath& Path : GetDefault<ULyraDeveloperSettings>()->CommonEditorMaps)
	{
		if (!Path.IsValid())
		{
			continue;
		}
		
		const FText DisplayName = FText::FromString(Path.GetAssetName());
		MenuBuilder.AddMenuEntry(
			DisplayName,
			LOCTEXT("CommonPathDescription", "Opens this map in the editor"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(&OpenCommonMap_Clicked, Path.ToString()),
				FCanExecuteAction::CreateStatic(&HasNoPlayWorld),
				FIsActionChecked(),
				FIsActionButtonVisible::CreateStatic(&HasNoPlayWorld)
			)
		);
	}

	return MenuBuilder.MakeWidget();
}

// 从工具栏以手动、可交互模式验证全部已检出内容。
static void CheckGameContent_Clicked()
{
	UEditorValidator::ValidateCheckedOutContent(/*bInteractive=*/true, EDataValidationUsecase::Manual);
}

// 扩展 LevelEditor Play 工具栏，注册内容验证按钮和常用地图下拉菜单。
static void RegisterGameEditorMenus()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	FToolMenuSection& Section = Menu->AddSection("PlayGameExtensions", TAttribute<FText>(), FToolMenuInsert("Play", EToolMenuInsertType::After));

	// Uncomment this to add a custom toolbar that is displayed during PIE
	// Useful for making easy access to changing game state artificially, adding cheats, etc
	// FToolMenuEntry BlueprintEntry = FToolMenuEntry::InitComboButton(
	// 	"OpenGameMenu",
	// 	FUIAction(
	// 		FExecuteAction(),
	// 		FCanExecuteAction::CreateStatic(&HasPlayWorld),
	// 		FIsActionChecked(),
	// 		FIsActionButtonVisible::CreateStatic(&HasPlayWorld)),
	// 	FOnGetContent::CreateStatic(&YourCustomMenu),
	// 	LOCTEXT("GameOptions_Label", "Game Options"),
	// 	LOCTEXT("GameOptions_ToolTip", "Game Options"),
	// 	FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.OpenLevelBlueprint")
	// );
	// BlueprintEntry.StyleNameOverride = "CalloutToolbar";
	// Section.AddEntry(BlueprintEntry);

	FToolMenuEntry CheckContentEntry = FToolMenuEntry::InitToolBarButton(
		"CheckContent",
		FUIAction(
			FExecuteAction::CreateStatic(&CheckGameContent_Clicked),
			FCanExecuteAction::CreateStatic(&HasNoPlayWorld),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateStatic(&HasNoPlayWorld)),
		LOCTEXT("CheckContentButton", "Check Content"),
		LOCTEXT("CheckContentDescription", "Runs the Content Validation job on all checked out assets to look for warnings and errors"),
		FSlateIcon(FGameEditorStyle::GetStyleSetName(), "GameEditor.CheckContent")
	);
	CheckContentEntry.StyleNameOverride = "CalloutToolbar";
	Section.AddEntry(CheckContentEntry);

	FToolMenuEntry CommonMapEntry = FToolMenuEntry::InitComboButton(
		"CommonMapOptions",
		FUIAction(
			FExecuteAction(),
			FCanExecuteAction::CreateStatic(&HasNoPlayWorld),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateStatic(&CanShowCommonMaps)),
		FOnGetContent::CreateStatic(&GetCommonMapsDropdown),
		LOCTEXT("CommonMaps_Label", "Common Maps"),
		LOCTEXT("CommonMaps_ToolTip", "Some commonly desired maps while using the editor"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Level")
	);
	CommonMapEntry.StyleNameOverride = "CalloutToolbar";
	Section.AddEntry(CommonMapEntry);
}

/**
 * LyraEditor 模块，注册编辑器样式、资产类型、GameplayCue 扩展、工具栏验证入口及启动/关闭清理。
 */
/**
 * FLyraEditorModule
 */
class FLyraEditorModule : public FDefaultGameModuleImpl
{
	typedef FLyraEditorModule ThisClass;

	// 初始化编辑器样式、GameplayAbilities 委托、工具栏、PIE 回调和 ContextEffectsLibrary 资产操作。
	virtual void StartupModule() override
	{
		FGameEditorStyle::Initialize();

		if (!IsRunningGame())
		{
			FModuleManager::Get().OnModulesChanged().AddRaw(this, &FLyraEditorModule::ModulesChangedCallback);

			BindGameplayAbilitiesEditorDelegates();

			if (FSlateApplication::IsInitialized())
			{
				ToolMenusHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterGameEditorMenus));
			}

			FEditorDelegates::BeginPIE.AddRaw(this, &ThisClass::OnBeginPIE);
			FEditorDelegates::EndPIE.AddRaw(this, &ThisClass::OnEndPIE);
		}

		// 注册 ContextEffectsLibrary 的资产类型操作，使其显示在 Gameplay 分类并支持专用创建流程。
		// Register the Context Effects Library asset type actions.
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			TSharedRef<FAssetTypeActions_LyraContextEffectsLibrary> AssetAction = MakeShared<FAssetTypeActions_LyraContextEffectsLibrary>();
			LyraContextEffectsLibraryAssetAction = AssetAction;
			AssetTools.RegisterAssetTypeActions(AssetAction);
		}
	}

	// PIE 开始时通知 LyraExperienceManager 重置并准备编辑器内 Experience 生命周期。
	void OnBeginPIE(bool bIsSimulating)
	{
		ULyraExperienceManager* ExperienceManager = GEngine->GetEngineSubsystem<ULyraExperienceManager>();
		check(ExperienceManager);
		ExperienceManager->OnPlayInEditorBegun();
	}

	// PIE 结束回调当前不执行附加操作，保留为模块扩展点。
	void OnEndPIE(bool bIsSimulating)
	{
	}

	// 注销资产操作、PIE/模块委托和工具栏回调，并释放 GameplayAbilities 绑定与 Slate 样式。
	virtual void ShutdownModule() override
	{
		// 模块关闭时注销 ContextEffectsLibrary 资产类型操作，避免残留编辑器回调。
		// Unregister the Context Effects Library asset type actions.
		{
			FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");
			TSharedPtr<IAssetTypeActions> AssetAction = LyraContextEffectsLibraryAssetAction.Pin();
			if (AssetToolsModule && AssetAction)
			{
				AssetToolsModule->Get().UnregisterAssetTypeActions(AssetAction.ToSharedRef());
			}
		}

		FEditorDelegates::BeginPIE.RemoveAll(this);
		FEditorDelegates::EndPIE.RemoveAll(this);

		// 撤销本模块对 UToolMenus 注册的所有菜单和工具栏扩展。
		// Undo UToolMenus
		if (UObjectInitialized() && ToolMenusHandle.IsValid())
		{
			UToolMenus::UnRegisterStartupCallback(ToolMenusHandle);
		}

		UnbindGameplayAbilitiesEditorDelegates();
		FModuleManager::Get().OnModulesChanged().RemoveAll(this);
		FGameEditorStyle::Shutdown();
	}

protected:

	// 将 GameplayCue 默认类、接口扫描类和创建路径解析函数绑定到 GameplayAbilitiesEditorModule。
	static void BindGameplayAbilitiesEditorDelegates()
	{
		IGameplayAbilitiesEditorModule& GameplayAbilitiesEditorModule = IGameplayAbilitiesEditorModule::Get();

		GameplayAbilitiesEditorModule.GetGameplayCueNotifyClassesDelegate().BindStatic(&GetGameplayCueDefaultClasses);
		GameplayAbilitiesEditorModule.GetGameplayCueInterfaceClassesDelegate().BindStatic(&GetGameplayCueInterfaceClasses);
		GameplayAbilitiesEditorModule.GetGameplayCueNotifyPathDelegate().BindStatic(&GetGameplayCuePath);
	}

	// GameplayAbilitiesEditorModule 可用时解除本模块注册的三个 GameplayCue 委托。
	static void UnbindGameplayAbilitiesEditorDelegates()
	{
		if (IGameplayAbilitiesEditorModule::IsAvailable())
		{
			IGameplayAbilitiesEditorModule& GameplayAbilitiesEditorModule = IGameplayAbilitiesEditorModule::Get();
			GameplayAbilitiesEditorModule.GetGameplayCueNotifyClassesDelegate().Unbind();
			GameplayAbilitiesEditorModule.GetGameplayCueInterfaceClassesDelegate().Unbind();
			GameplayAbilitiesEditorModule.GetGameplayCueNotifyPathDelegate().Unbind();
		}
	}

	// GameplayAbilitiesEditor 晚于 LyraEditor 加载时补做 GameplayCue 委托绑定。
	void ModulesChangedCallback(FName ModuleThatChanged, EModuleChangeReason ReasonForChange)
	{
		if ((ReasonForChange == EModuleChangeReason::ModuleLoaded) && (ModuleThatChanged.ToString() == TEXT("GameplayAbilitiesEditor")))
		{
			BindGameplayAbilitiesEditorDelegates();
		}
	}

private:
	TWeakPtr<IAssetTypeActions> LyraContextEffectsLibraryAssetAction;
	FDelegateHandle ToolMenusHandle;
};

IMPLEMENT_MODULE(FLyraEditorModule, LyraEditor);

#undef LOCTEXT_NAMESPACE
