#include "UERingEditorModule.h"

#include "ContentBrowserMenuContexts.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "UERingBundleBuilder.h"
#include "UERingCppIndexer.h"
#include "UERingExportManager.h"
#include "UERingIndexManager.h"
#include "UERingLifecycleManager.h"
#include "UERingMcpIntegration.h"
#include "UERingSettings.h"
#include "UERingValidator.h"
#include "UERingVersion.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FUERingEditorModule"

DEFINE_LOG_CATEGORY_STATIC(LogUERingEditor, Log, All);
static const FName UERingStatusTabName(TEXT("UERingStatus"));

FUERingEditorModule::FUERingEditorModule() = default;
FUERingEditorModule::~FUERingEditorModule() = default;

void FUERingEditorModule::StartupModule()
{
    FUERingExportManager::Get().Initialize();
    FUERingLifecycleManager::Get().Initialize();
    McpIntegration = MakeUnique<FUERingMcpIntegration>();
    McpIntegration->Initialize();
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        UERingStatusTabName,
        FOnSpawnTab::CreateRaw(this, &FUERingEditorModule::SpawnStatusTab))
        .SetDisplayName(LOCTEXT("StatusTabTitle", "UE Ring Status"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUERingEditorModule::RegisterMenus));
    UE_LOG(LogUERingEditor, Log, TEXT("%s editor module started."), UE_RING_PLUGIN_NAME);
}

void FUERingEditorModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(UERingStatusTabName);
    if (McpIntegration)
    {
        McpIntegration->Shutdown();
        McpIntegration.Reset();
    }
    FUERingLifecycleManager::Get().Shutdown();
    FUERingExportManager::Get().Shutdown();
    UE_LOG(LogUERingEditor, Log, TEXT("%s editor module stopped."), UE_RING_PLUGIN_NAME);
}

void FUERingEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu");
    FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

    Section.AddDynamicEntry(
        "UERing_ExportBlueprintSemantic",
        FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
        {
            FToolUIAction Action;
            Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InContext)
            {
                const UContentBrowserAssetContextMenuContext* Context =
                    UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
                if (Context == nullptr)
                {
                    return;
                }

                int32 ExportedCount = 0;
                int32 UnchangedCount = 0;
                int32 UnsupportedCount = 0;
                TArray<FString> Errors;

                for (const FUERingExportResult& Result :
                    FUERingExportManager::Get().ExportAssets(Context->SelectedAssets))
                {
                    switch (Result.Status)
                    {
                    case EUERingExportStatus::Exported:
                        ++ExportedCount;
                        break;
                    case EUERingExportStatus::Unchanged:
                        ++UnchangedCount;
                        break;
                    case EUERingExportStatus::Unsupported:
                        ++UnsupportedCount;
                        break;
                    case EUERingExportStatus::Failed:
                        Errors.Add(Result.Error);
                        break;
                    }
                }

                TArray<FName> PackageNames;
                for (const FAssetData& Asset : Context->SelectedAssets) PackageNames.Add(Asset.PackageName);
                FString IndexError;
                if (!FUERingIndexManager::UpdatePackages(PackageNames, IndexError))
                {
                    Errors.Add(IndexError);
                }
                FString CppIndexError;
                if (ExportedCount > 0
                    && GetDefault<UUERingSettings>()->bIncludeCppIndex
                    && !FUERingCppIndexer::UpdatePackages(PackageNames, CppIndexError))
                {
                    Errors.Add(CppIndexError);
                }

                FNotificationInfo Notification(Errors.IsEmpty()
                    ? FText::Format(
                        LOCTEXT("ExportSucceeded", "UE Ring exported {0}; {1} unchanged; {2} unsupported."),
                        FText::AsNumber(ExportedCount),
                        FText::AsNumber(UnchangedCount),
                        FText::AsNumber(UnsupportedCount))
                    : FText::Format(
                        LOCTEXT("ExportFailed", "UE Ring exported {0}, with {1} failure(s). See Output Log."),
                        FText::AsNumber(ExportedCount),
                        FText::AsNumber(Errors.Num())));
                Notification.ExpireDuration = 6.0f;

                if (Errors.IsEmpty())
                {
                    Notification.Image = FAppStyle::GetBrush("Icons.SuccessWithColor");
                }
                else
                {
                    Notification.Image = FAppStyle::GetBrush("Icons.ErrorWithColor");
                    for (const FString& Error : Errors)
                    {
                        UE_LOG(LogUERingEditor, Error, TEXT("%s"), *Error);
                    }
                }

                FSlateNotificationManager::Get().AddNotification(Notification);
            });

            InSection.AddMenuEntry(
                "UERing_ExportAssetSemantic",
                LOCTEXT("ExportBlueprintSemantic", "Export AI Semantic"),
                LOCTEXT("ExportBlueprintSemanticTooltip", "Export supported selected assets as deterministic USEM semantic JSON."),
                FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Export"),
                Action);
        }));

    Section.AddDynamicEntry(
        "UERing_CreateAIBundle",
        FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
        {
            FToolUIAction Action;
            Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InContext)
            {
                const UContentBrowserAssetContextMenuContext* Context =
                    UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
                if (Context == nullptr)
                {
                    return;
                }
                int32 SupportedCount = 0;
                int32 PreviewFileCount = 0;
                FString PreviewText = TEXT("The following files will be included in the offline AI bundle(s):\n\n");
                for (const FAssetData& Asset : Context->SelectedAssets)
                {
                    if (!FUERingExportManager::Get().CanExport(Asset))
                    {
                        continue;
                    }
                    TArray<FString> PreviewFiles;
                    FString PreviewError;
                    if (!FUERingBundleBuilder::Preview(Asset.PackageName.ToString(), PreviewFiles, PreviewError))
                    {
                        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(PreviewError));
                        return;
                    }
                    ++SupportedCount;
                    PreviewFileCount += PreviewFiles.Num();
                    PreviewText += Asset.PackageName.ToString() + TEXT("\n");
                    for (const FString& File : PreviewFiles)
                    {
                        PreviewText += TEXT("  ") + File + TEXT("\n");
                    }
                    PreviewText += TEXT("\n");
                }
                PreviewText += FString::Printf(
                    TEXT("Create %d bundle(s) containing %d file(s)? Sanitized configuration copies are used. Review the generated files again before sharing."),
                    SupportedCount,
                    PreviewFileCount);
                if (SupportedCount == 0
                    || FMessageDialog::Open(
                        EAppMsgType::YesNo,
                        FText::FromString(PreviewText)) != EAppReturnType::Yes)
                {
                    return;
                }
                int32 Built = 0;
                TArray<FString> Errors;
                for (const FAssetData& Asset : Context->SelectedAssets)
                {
                    if (!FUERingExportManager::Get().CanExport(Asset))
                    {
                        continue;
                    }
                    FString BundleDirectory;
                    FString Error;
                    if (FUERingBundleBuilder::Build(Asset.PackageName.ToString(), BundleDirectory, Error))
                    {
                        ++Built;
                    }
                    else
                    {
                        Errors.Add(Asset.PackageName.ToString() + TEXT(": ") + Error);
                    }
                }
                FNotificationInfo Notification(Errors.IsEmpty()
                    ? FText::Format(LOCTEXT("BundleSucceeded", "UE Ring created {0} AI bundle(s)."), FText::AsNumber(Built))
                    : FText::Format(LOCTEXT("BundleFailed", "UE Ring created {0} bundle(s), with {1} failure(s)."),
                        FText::AsNumber(Built), FText::AsNumber(Errors.Num())));
                Notification.ExpireDuration = 6.0f;
                Notification.Image = FAppStyle::GetBrush(Errors.IsEmpty() ? "Icons.SuccessWithColor" : "Icons.ErrorWithColor");
                for (const FString& Error : Errors)
                {
                    UE_LOG(LogUERingEditor, Error, TEXT("%s"), *Error);
                }
                FSlateNotificationManager::Get().AddNotification(Notification);
            });
            InSection.AddMenuEntry(
                "UERing_CreateAIBundle",
                LOCTEXT("CreateAIBundle", "Create AI Bundle"),
                LOCTEXT("CreateAIBundleTooltip", "Package exported semantics and related offline context for the selected assets."),
                FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderOpen"),
                Action);
        }));

    Section.AddDynamicEntry(
        "UERing_ExportWithDependencies",
        FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
        {
            FToolUIAction Action;
            Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InContext)
            {
                const UContentBrowserAssetContextMenuContext* Context =
                    UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
                if (Context == nullptr)
                {
                    return;
                }
                IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
                TMap<FName, FAssetData> UniqueAssets;
                for (const FAssetData& Asset : Context->SelectedAssets)
                {
                    UniqueAssets.Add(Asset.PackageName, Asset);
                    TArray<FName> Dependencies;
                    Registry.GetDependencies(
                        Asset.PackageName, Dependencies,
                        UE::AssetRegistry::EDependencyCategory::Package,
                        UE::AssetRegistry::EDependencyQuery::Hard);
                    for (const FName Dependency : Dependencies)
                    {
                        TArray<FAssetData> DependencyAssets;
                        Registry.GetAssetsByPackageName(Dependency, DependencyAssets, true);
                        for (const FAssetData& DependencyAsset : DependencyAssets)
                        {
                            if (FUERingExportManager::Get().CanExport(DependencyAsset))
                            {
                                UniqueAssets.Add(DependencyAsset.PackageName, DependencyAsset);
                            }
                        }
                    }
                }
                TArray<FAssetData> Assets;
                UniqueAssets.GenerateValueArray(Assets);
                int32 Failed = 0;
                for (const FUERingExportResult& Result : FUERingExportManager::Get().ExportAssets(Assets))
                {
                    Failed += Result.Status == EUERingExportStatus::Failed ? 1 : 0;
                }
                TArray<FName> PackageNames;
                for (const FAssetData& Asset : Assets) PackageNames.Add(Asset.PackageName);
                FString Error;
                FUERingIndexManager::UpdatePackages(PackageNames, Error);
                FNotificationInfo Notification(FText::Format(
                    LOCTEXT("ExportDepsComplete", "UE Ring processed {0} asset(s), with {1} failure(s)."),
                    FText::AsNumber(Assets.Num()), FText::AsNumber(Failed)));
                Notification.ExpireDuration = 6.0f;
                Notification.Image = FAppStyle::GetBrush(Failed == 0 ? "Icons.SuccessWithColor" : "Icons.ErrorWithColor");
                FSlateNotificationManager::Get().AddNotification(Notification);
            });
            InSection.AddMenuEntry(
                "UERing_ExportWithDependencies",
                LOCTEXT("ExportWithDependencies", "Export AI Semantic with Hard Dependencies"),
                LOCTEXT("ExportWithDependenciesTooltip", "Export selected supported assets and their direct hard dependencies."),
                FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Export"),
                Action);
        }));

    UToolMenu* FolderMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.FolderContextMenu");
    FToolMenuSection& FolderSection = FolderMenu->FindOrAddSection("PathContextBulkOperations");
    FolderSection.AddDynamicEntry(
        "UERing_ExportFolderSemantic",
        FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
        {
            FToolUIAction Action;
            Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InContext)
            {
                const UContentBrowserFolderContext* Context = InContext.FindContext<UContentBrowserFolderContext>();
                if (Context == nullptr)
                {
                    return;
                }
                FARFilter Filter;
                Filter.bRecursivePaths = true;
                for (const FString& Path : Context->GetSelectedPackagePaths())
                {
                    Filter.PackagePaths.Add(*Path);
                }
                TArray<FAssetData> Assets;
                IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
                Registry.GetAssets(Filter, Assets);
                Assets.RemoveAll([](const FAssetData& Asset)
                {
                    return !FUERingExportManager::Get().CanExport(Asset);
                });
                FUERingExportManager::Get().CanonicalizeAssetsByPackage(Assets);
                int32 Failed = 0;
                for (const FUERingExportResult& Result : FUERingExportManager::Get().ExportAssets(Assets))
                {
                    Failed += Result.Status == EUERingExportStatus::Failed ? 1 : 0;
                }
                TArray<FName> PackageNames;
                for (const FAssetData& Asset : Assets) PackageNames.Add(Asset.PackageName);
                FString Error;
                FUERingIndexManager::UpdatePackages(PackageNames, Error);
                FNotificationInfo Notification(FText::Format(
                    LOCTEXT("ExportFolderComplete", "UE Ring processed {0} asset(s), with {1} failure(s)."),
                    FText::AsNumber(Assets.Num()), FText::AsNumber(Failed)));
                Notification.ExpireDuration = 6.0f;
                Notification.Image = FAppStyle::GetBrush(Failed == 0 ? "Icons.SuccessWithColor" : "Icons.ErrorWithColor");
                FSlateNotificationManager::Get().AddNotification(Notification);
            });
            InSection.AddMenuEntry(
                "UERing_ExportFolderSemantic",
                LOCTEXT("ExportFolderSemantic", "Export Folder AI Semantics"),
                LOCTEXT("ExportFolderSemanticTooltip", "Recursively export all supported assets in the selected folders."),
                FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Export"),
                Action);
        }));

    UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
    FToolMenuSection& ToolsSection = ToolsMenu->FindOrAddSection("UERing");
    ToolsSection.AddMenuEntry(
        "UERing_OpenStatus",
        LOCTEXT("OpenStatus", "UE Ring Status"),
        LOCTEXT("OpenStatusTooltip", "Open semantic export status, validation, and project export controls."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Info"),
        FUIAction(FExecuteAction::CreateLambda([]
        {
            FGlobalTabmanager::Get()->TryInvokeTab(UERingStatusTabName);
        })));
}

TSharedRef<SDockTab> FUERingEditorModule::SpawnStatusTab(const FSpawnTabArgs& Args)
{
    const FUERingValidationReport Report = FUERingValidator::Validate();
    const FString OutputRoot = FUERingExportManager::Get().GetOutputRoot();
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(16.0f, 16.0f, 16.0f, 4.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("StatusHeading", "UE Ring Semantic Export"))
                    .TextStyle(FAppStyle::Get(), "HeadingExtraSmall")
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(16.0f, 4.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::Format(LOCTEXT("OutputRootLabel", "Output: {0}"), FText::FromString(OutputRoot)))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(16.0f, 4.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::Format(
                        LOCTEXT("ValidationSummary", "Checked {0} | Missing {1} | Stale {2} | Orphan {3} | Invalid {4}"),
                        FText::AsNumber(Report.Checked), FText::AsNumber(Report.Missing),
                        FText::AsNumber(Report.Stale), FText::AsNumber(Report.Orphan), FText::AsNumber(Report.Invalid)))
                    .ColorAndOpacity(Report.IsValid() ? FSlateColor::UseForeground() : FAppStyle::GetSlateColor("Colors.Warning"))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(16.0f, 12.0f, 16.0f, 4.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("RefreshValidation", "Refresh Validation"))
                        .OnClicked_Lambda([]
                        {
                            if (const TSharedPtr<SDockTab> ExistingTab = FGlobalTabmanager::Get()->FindExistingLiveTab(UERingStatusTabName))
                            {
                                ExistingTab->RequestCloseTab();
                            }
                            FGlobalTabmanager::Get()->TryInvokeTab(UERingStatusTabName);
                            return FReply::Handled();
                        })
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("ExportProject", "Export Project"))
                        .OnClicked_Lambda([]
                        {
                            IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
                            TArray<FAssetData> Assets;
                            Registry.GetAllAssets(Assets, true);
                            Assets.RemoveAll([](const FAssetData& Asset)
                            {
                                return !FUERingExportManager::Get().CanExport(Asset);
                            });
                            FUERingExportManager::Get().CanonicalizeAssetsByPackage(Assets);
                            FString Error;
                            if (!FUERingExportManager::Get().PrepareFullExport(Error))
                            {
                                FNotificationInfo Notification(FText::FromString(Error));
                                Notification.ExpireDuration = 8.0f;
                                Notification.Image = FAppStyle::GetBrush("Icons.ErrorWithColor");
                                FSlateNotificationManager::Get().AddNotification(Notification);
                                return FReply::Handled();
                            }
                            FUERingExportManager::Get().ExportAssets(Assets);
                            FUERingExportManager::Get().FinalizeFullExport(Assets, Error);
                            FUERingIndexManager::Rebuild(Error);
                            if (GetDefault<UUERingSettings>()->bIncludeCppIndex)
                            {
                                FUERingCppIndexer::Rebuild(Error);
                            }
                            return FReply::Handled();
                        })
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("OpenOutput", "Open Output Folder"))
                        .OnClicked_Lambda([OutputRoot]
                        {
                            IFileManager::Get().MakeDirectory(*OutputRoot, true);
                            FPlatformProcess::ExploreFolder(*OutputRoot);
                            return FReply::Handled();
                        })
                    ]
                ]
            ]
        ];
}

IMPLEMENT_MODULE(FUERingEditorModule, UERingEditor)

#undef LOCTEXT_NAMESPACE

