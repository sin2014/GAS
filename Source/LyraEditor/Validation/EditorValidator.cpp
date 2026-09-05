// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorValidator.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "Engine/BlueprintCore.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Logging/MessageLog.h"
#include "LyraEditor.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Settings/ProjectPackagingSettings.h"
#include "ShaderCompiler.h"
#include "SourceCodeNavigation.h"
#include "SourceControlOperations.h"
#include "Stats/StatsMisc.h"
#include "StudioTelemetry.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorValidator)

#define LOCTEXT_NAMESPACE "EditorValidator"

// 单个头文件变化最多扩展验证的派生资产数量，防止依赖扫描失控。
int32 GMaxAssetsChangedByAHeader = 200;
// 将 EditorValidator.MaxAssetsChangedByAHeader CVar 绑定到派生资产扫描上限。
static FAutoConsoleVariableRef CVarMaxAssetsChangedByAHeader(TEXT("EditorValidator.MaxAssetsChangedByAHeader"), GMaxAssetsChangedByAHeader, TEXT("The maximum number of assets to check for content validation based on a single header change."), ECVF_Default);

// 当前编辑器验证流程是否允许加载引用者和执行高开销完整验证。
bool UEditorValidator::bAllowFullValidationInEditor = false;
// 验证期间日志收集器共享的忽略模式列表。
TArray<FString> FLyraValidationMessageGatherer::IgnorePatterns;

// 构造抽象 Lyra 验证器基类，并保持 EditorValidatorBase 默认行为。
UEditorValidator::UEditorValidator()
	: Super()
{
}

// 刷新 SourceControl 已打开文件，分类资产与头文件变更，扩展受代码影响蓝图，并以可交互慢任务运行项目和包验证。
void UEditorValidator::ValidateCheckedOutContent(bool bInteractive, const EDataValidationUsecase InValidationUsecase)
{
	if (FStudioTelemetry::IsAvailable())
	{
		FStudioTelemetry::Get().RecordEvent(TEXT("ValidateContent"));
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		if (bInteractive)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DiscoveringAssets", "Still discovering assets. Try again once it is complete."));
		}
		else
		{
			UE_LOG(LogLyraEditor, Display, TEXT("Could not run ValidateCheckedOutContent because asset discovery was still being done."));
		}
		return;
	}

	TArray<FString> ChangedPackageNames;
	TArray<FString> DeletedPackageNames;

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if (ISourceControlModule::Get().IsEnabled())
	{
		// 构建过滤器时主动刷新已打开文件状态，确保检出内容的 SourceControl 状态准确。
		// Request the opened files at filter construction time to make sure checked out files have the correct state for the filter
		TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
		UpdateStatusOperation->SetGetOpenedOnly(true);
		SourceControlProvider.Execute(UpdateStatusOperation, EConcurrency::Synchronous);

		TArray<FSourceControlStateRef> CheckedOutFiles = SourceControlProvider.GetCachedStateByPredicate(
			[](const FSourceControlStateRef& State) { return State->IsCheckedOut() || State->IsAdded() || State->IsDeleted(); }
		);

		for (const FSourceControlStateRef& FileState : CheckedOutFiles)
		{
			FString Filename = FileState->GetFilename();
			if (FPackageName::IsPackageFilename(Filename))
			{
				// 将已检出资产文件转换为包名并加入验证集合。
				// Assets
				FString PackageName;
				if (FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageName))
				{
					if (FileState->IsDeleted())
					{
						DeletedPackageNames.Add(PackageName);
					}
					else
					{
						ChangedPackageNames.Add(PackageName);
					}
				}
			}
			else if (Filename.EndsWith(TEXT(".h")))
			{
				// 原生类头文件变化可能破坏其派生蓝图，因此把受影响蓝图包也加入验证集合。
				// Source code header changes for classes may cause issues in assets based on those classes
				UEditorValidator::GetChangedAssetsForCode(AssetRegistryModule.Get(), Filename, ChangedPackageNames);
			}
		}
	}

	bool bAnyIssuesFound = false;
	TArray<FString> AllWarningsAndErrors;
	{
		if (bInteractive)
		{
			bAllowFullValidationInEditor = true;

			// 加载材质时会刷新 Shader 编译；先完成已有任务，避免无关警告错误归因到当前包。
			// We will be flushing shader compile as we load materials, so dont let other shader warnings be attributed incorrectly to the package that is loading.
			if (GShaderCompilingManager)
			{
				FScopedSlowTask SlowTask(0.f, LOCTEXT("CompilingShadersBeforeCheckingContentTask", "Finishing shader compiles before checking content..."));
				SlowTask.MakeDialog();
				GShaderCompilingManager->FinishAllCompilation();
			}
		}
		{
			FScopedSlowTask SlowTask(0.f, LOCTEXT("CheckingContentTask", "Checking content..."));
			SlowTask.MakeDialog();
			if (!ValidatePackages(ChangedPackageNames, DeletedPackageNames, 2000, AllWarningsAndErrors, InValidationUsecase))
			{
				bAnyIssuesFound = true;
			}
		}
		if (bInteractive)
		{
			bAllowFullValidationInEditor = false;
		}
	}

	{
		FLyraValidationMessageGatherer ScopedMessageGatherer;
		if (!ValidateProjectSettings())
		{
			bAnyIssuesFound = true;
		}
		AllWarningsAndErrors.Append(ScopedMessageGatherer.GetAllWarningsAndErrors());
	}

	if (bInteractive)
	{
		const bool bAtLeastOneMessage = (AllWarningsAndErrors.Num() != 0);
		if (bAtLeastOneMessage)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ContentValidationFailed", "!!!!!!! Your checked out content has issues. Don't submit until they are fixed !!!!!!!\r\n\r\nSee the MessageLog and OutputLog for details"));
		}
		else if (bAnyIssuesFound)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ContentValidationFailedWithNoMessages", "No errors or warnings were found, but there was an error return code. Look in the OutputLog and log file for details. You may need engineering help."));
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ContentValidationPassed", "All checked out content passed. Nice job."));
		}
	}
}

// 按数量上限过滤包，预加载资产并单独收集加载警告，再运行 DataValidationSubsystem 验证并汇总错误。
bool UEditorValidator::ValidatePackages(const TArray<FString>& ExistingPackageNames, const TArray<FString>& DeletedPackageNames, int32 MaxPackagesToLoad, TArray<FString>& OutAllWarningsAndErrors, const EDataValidationUsecase InValidationUsecase)
{
	bool bAnyIssuesFound = false;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FString> AllPackagesToValidate = ExistingPackageNames;
	for (const FString& DeletedPackageName : DeletedPackageNames)
	{
		UE_LOG(LogLyraEditor, Display, TEXT("Adding referencers for deleted package %s to be verified"), *DeletedPackageName);
		TArray<FName> PackageReferencers;
		AssetRegistry.GetReferencers(FName(*DeletedPackageName), PackageReferencers, UE::AssetRegistry::EDependencyCategory::Package);
		for (const FName& Referencer : PackageReferencers)
		{
			const FString ReferencerString = Referencer.ToString();
			if (!DeletedPackageNames.Contains(ReferencerString) && !IsInUncookedFolder(ReferencerString))
			{
				UE_LOG(LogLyraEditor, Display, TEXT("    Deleted package referencer %s was added to the queue to be verified"), *ReferencerString);
				AllPackagesToValidate.Add(ReferencerString);
			}
		}
	}

	const FText ValidationPageName = LOCTEXT("ValidatePackages", "Validate Packages");

	FMessageLog DataValidationLog("AssetCheck");
	DataValidationLog.NewPage(ValidationPageName);

	if (AllPackagesToValidate.Num() > MaxPackagesToLoad)
	{
		// 变更包数超过上限时跳过现有包验证并记录警告，避免一次验证加载过多内容。
		// Too much changed to verify, just pass it.
		FString WarningMessage = FString::Printf(TEXT("Assets to validate (%d) exceeded -MaxPackagesToLoad=(%d). Skipping existing package validation."), AllPackagesToValidate.Num(), MaxPackagesToLoad);
		UE_LOG(LogLyraEditor, Warning, TEXT("%s"), *WarningMessage);
		OutAllWarningsAndErrors.Add(WarningMessage);
		DataValidationLog.Warning(FText::FromString(WarningMessage));
	}
	else
	{
		// 根据包过滤结果收集实际存在且包含资产的待验证 AssetData。
		// Load all packages that match the file filter string
		TArray<FAssetData> AssetsToCheck;
		for (const FString& PackageName : AllPackagesToValidate)
		{
			if (FPackageName::IsValidLongPackageName(PackageName) && !IsInUncookedFolder(PackageName))
			{
				int32 OldNumAssets = AssetsToCheck.Num();
				AssetRegistry.GetAssetsByPackageName(FName(*PackageName), AssetsToCheck, true);
				if (AssetsToCheck.Num() == OldNumAssets)
				{
					FString WarningMessage;
					// 仅处理磁盘上仍存在的包；不存在时通常是已删除或不含资产的包。
					// See if the file exists at all. Otherwise, the package contains no assets.
					if (FPackageName::DoesPackageExist(PackageName))
					{
						WarningMessage = FString::Printf(TEXT("Found no assets in package '%s'"), *PackageName);
					}
					else
					{
						if (ISourceControlModule::Get().IsEnabled())
						{
							ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
							FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
							TSharedPtr<ISourceControlState, ESPMode::ThreadSafe> FileState = SourceControlProvider.GetState(PackageFilename, EStateCacheUsage::ForceUpdate);
							if (FileState->IsAdded())
							{
								WarningMessage = FString::Printf(TEXT("Package '%s' is missing from disk. It is marked for add in perforce but missing from your hard drive."), *PackageName);
							}

							if (FileState->IsCheckedOut())
							{
								WarningMessage = FString::Printf(TEXT("Package '%s' is missing from disk. It is checked out in perforce but missing from your hard drive."), *PackageName);
							}
						}

						if (WarningMessage.IsEmpty())
						{
							WarningMessage = FString::Printf(TEXT("Package '%s' is missing from disk."), *PackageName);
						}
					}
					ensure(!WarningMessage.IsEmpty());
					UE_LOG(LogLyraEditor, Warning, TEXT("%s"), *WarningMessage);
					OutAllWarningsAndErrors.Add(WarningMessage);
					DataValidationLog.Warning(FText::FromString(WarningMessage));
					bAnyIssuesFound = true;
				}
			}
		}

		if (AssetsToCheck.Num() > 0)
		{
			// 先单独预加载所有资产，使加载警告与验证器产生的警告可以分别归类。
			// Preload all assets to check, so load warnings can be handled separately from validation warnings
			{
				for (const FAssetData& AssetToCheck : AssetsToCheck)
				{
					if (!AssetToCheck.IsAssetLoaded())
					{
						UE_LOG(LogLyraEditor, Display, TEXT("Preloading %s..."), *AssetToCheck.GetObjectPathString());

						// 在加载单个资产期间临时收集日志警告和错误。
						// Start listening for load warnings
						FLyraValidationMessageGatherer ScopedPreloadMessageGatherer;
						
						// 强制加载资产以暴露序列化、依赖和编译问题。
						// Load the asset
						AssetToCheck.GetAsset();

						if (ScopedPreloadMessageGatherer.GetAllWarningsAndErrors().Num() > 0)
						{
							// 将异常加载警告重新报告为错误，使 CI/Build Health 创建并分派问题。
							// Repeat all errant load warnings as errors, so other CIS systems can treat them more severely (i.e. Build health will create an issue and assign it to a developer)
							for (const FString& LoadWarning : ScopedPreloadMessageGatherer.GetAllWarnings())
							{
								UE_LOG(LogLyraEditor, Error, TEXT("%s"), *LoadWarning);
							}

							OutAllWarningsAndErrors.Append(ScopedPreloadMessageGatherer.GetAllWarningsAndErrors());
							bAnyIssuesFound = true;
						}
					}
				}
			}

			// 资产全部预加载后运行 DataValidationSubsystem 中的所有适用验证器。
			// Run all validators now.
			FLyraValidationMessageGatherer ScopedMessageGatherer;
			FValidateAssetsSettings Settings;
			FValidateAssetsResults Results;

			Settings.bSkipExcludedDirectories = true;
			Settings.bShowIfNoFailures = true;
			Settings.ValidationUsecase = InValidationUsecase;
			Settings.MessageLogPageTitle = ValidationPageName;

			const bool bHasInvalidFiles = GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>()->ValidateAssetsWithSettings(AssetsToCheck, Settings, Results) > 0;

			if (bHasInvalidFiles || ScopedMessageGatherer.GetAllWarningsAndErrors().Num() > 0)
			{
				OutAllWarningsAndErrors.Append(ScopedMessageGatherer.GetAllWarningsAndErrors());
				bAnyIssuesFound = true;
			}
		}
	}

	return !bAnyIssuesFound;
}

// 检查 Python 开发模式、Packaging 测试地图和项目配置等必须满足的编辑器设置，并返回整体是否有效。
bool UEditorValidator::ValidateProjectSettings()
{
	bool bSuccess = true;

	FMessageLog ValidationLog("AssetCheck");

	{
		bool bDeveloperMode = false;
		GConfig->GetBool(TEXT("/Script/PythonScriptPlugin.PythonScriptPluginSettings"), TEXT("bDeveloperMode"), /*out*/ bDeveloperMode, GEngineIni);

		if (bDeveloperMode)
		{
			const FString ErrorMessage(TEXT("The project setting version of Python's bDeveloperMode should not be checked in. Use the editor preference version instead!"));
			UE_LOG(LogLyraEditor, Error, TEXT("%s"), *ErrorMessage);
			ValidationLog.Error(FText::AsCultureInvariant(ErrorMessage));
			bSuccess = false;
		}
	}

	return bSuccess;
}

// 判断包路径是否位于 PackagingSettings 的 DirectoriesToNeverCook 下，并可输出命中的目录名。
bool UEditorValidator::IsInUncookedFolder(const FString& PackageName, FString* OutUncookedFolderName)
{
	const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
	check(PackagingSettings);
	for (const FDirectoryPath& DirectoryToNeverCook : PackagingSettings->DirectoriesToNeverCook)
	{
		const FString& UncookedFolder = DirectoryToNeverCook.Path;
		if (PackageName.StartsWith(UncookedFolder))
		{
			if (OutUncookedFolderName)
			{
				FString FolderToReport = UncookedFolder.StartsWith(TEXT("/Game/")) ? UncookedFolder.RightChop(6) : UncookedFolder;
				if (FolderToReport.EndsWith(TEXT("/")))
				{
					*OutUncookedFolderName = FolderToReport.LeftChop(1);
				}
				else
				{
					*OutUncookedFolderName = FolderToReport;
				}
			}
			return true;
		}
	}

	return false;
}

// 返回当前验证流程是否允许加载引用资产和执行高开销完整检查。
bool UEditorValidator::ShouldAllowFullValidation()
{
	return IsRunningCommandlet() || bAllowFullValidationInEditor;
}

// 排除 nullptr、临时包、Uncooked 目录和测试地图之外不应由通用 Lyra 验证器处理的资产。
bool UEditorValidator::CanValidateAsset_Implementation(UObject* InAsset) const
{
	if (InAsset)
	{
		FString PackageName = InAsset->GetOutermost()->GetName();
		if (!IsInUncookedFolder(PackageName))
		{
			return true;
		}
	}
	
	return false;
}

// 解析变更头文件所属模块与相对路径，查找其中原生类及其非 DataOnly 派生蓝图，并加入受影响包集合。
void UEditorValidator::GetChangedAssetsForCode(IAssetRegistry& AssetRegistry, const FString& ChangedHeaderLocalFilename, TArray<FString>& OutChangedPackageNames)
{
	// 首次调用时构建原生 UClass 到“模块名+头文件相对路径”的缓存，后续头文件变更查询复用。
	static struct FCachedNativeClasses
	{
	public:
		// 遍历所有原生 UClass，从 AssetData 标签提取所属模块和头文件路径并建立多值索引。
		FCachedNativeClasses()
		{
			static const FName ModuleNameFName = "ModuleName";
			static const FName ModuleRelativePathFName = "ModuleRelativePath";

			for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
			{
				UClass* TestClass = *ClassIt;
				if (TestClass->HasAnyClassFlags(CLASS_Native))
				{
					FAssetData ClassAssetData(TestClass);

					FString ModuleName, ModuleRelativePath;
					ClassAssetData.GetTagValue(ModuleNameFName, ModuleName);
					ClassAssetData.GetTagValue(ModuleRelativePathFName, ModuleRelativePath);

					Classes.Add(ModuleName + TEXT("+") + ModuleRelativePath, TestClass);
				}
			}
		}

		// 返回指定模块与相对头文件中声明的全部原生类弱引用。
		TArray<TWeakObjectPtr<UClass>> GetClassesInHeader(const FString& ModuleName, const FString& ModuleRelativePath)
		{
			TArray<TWeakObjectPtr<UClass>> ClassesInHeader;
			Classes.MultiFind(ModuleName + TEXT("+") + ModuleRelativePath, ClassesInHeader);

			return ClassesInHeader;
		}

	private:
		TMultiMap<FString, TWeakObjectPtr<UClass>> Classes;
	} NativeClassCache;

	const TArray<FString>& ModuleNames = FSourceCodeNavigation::GetSourceFileDatabase().GetModuleNames();
	const FString* Module = ModuleNames.FindByPredicate([ChangedHeaderLocalFilename](const FString& ModuleBuildPath) {
		const FString ModuleFullPath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(ModuleBuildPath));
		if (ChangedHeaderLocalFilename.StartsWith(ModuleFullPath))
		{
			return true;
		}
		return false;
		});

	if (Module)
	{
		SCOPE_LOG_TIME_IN_SECONDS(TEXT("Looking for blueprints affected by code changes"), nullptr);

		const FString FoundModulePath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(*Module));
		const FString FoundModulePathWithSlash = FoundModulePath / TEXT("");
		FString ChangedHeaderReleativeToModule = ChangedHeaderLocalFilename;
		FPaths::MakePathRelativeTo(ChangedHeaderReleativeToModule, *FoundModulePathWithSlash);
		FString ChangedHeaderModule = FPaths::GetBaseFilename(FoundModulePath);

		// 第一步：从发生变化的头文件中找出所有原生 UClass。
		// STEP 1 - Find all the native classes inside the header that changed.
		TArray<TWeakObjectPtr<UClass>> ClassList = NativeClassCache.GetClassesInHeader(ChangedHeaderModule, ChangedHeaderReleativeToModule);

		// 第二步：通过继承关系和 AssetRegistry 找出这些原生类派生的非数据蓝图。
		// STEP 2 - We now need to convert the set of native classes into actual derived blueprints.
		bool bTooManyFiles = false;
		TArray<FAssetData> BlueprintsDerivedFromNativeModifiedClasses;
		for (TWeakObjectPtr<UClass> ModifiedClassPtr : ClassList)
		{
			// 单个头文件影响资产数达到上限后停止继续扩展依赖集合。
			// If we capped out on maximum number of modified files for a single header change, don't try to keep looking for more stuff.
			if (bTooManyFiles)
			{
				break;
			}

			if (UClass* ModifiedClass = ModifiedClassPtr.Get())
			{
				// 收集直接或间接继承这些原生类的全部派生类路径。
				// This finds all native derived blueprints, both direct subclasses, or subclasses of subclasses.
				TSet<FTopLevelAssetPath> DerivedClassNames;
				TArray<FTopLevelAssetPath> ClassNames;
				ClassNames.Add(ModifiedClass->GetClassPathName());
				AssetRegistry.GetDerivedClassNames(ClassNames, TSet<FTopLevelAssetPath>(), DerivedClassNames);

				UE_LOG(LogLyraEditor, Display, TEXT("Validating Subclasses of %s in %s + %s"), *ModifiedClass->GetName(), *ChangedHeaderModule, *ChangedHeaderReleativeToModule);

				FARFilter Filter;
				Filter.bRecursiveClasses = true;
				Filter.ClassPaths.Add(UBlueprintCore::StaticClass()->GetClassPathName());

				// 枚举蓝图资产，查找直接继承原生类或通过其他蓝图间接继承的资产。
				// We enumerate all assets to find any blueprints who inherit from native classes directly - or
				// from other blueprints.
				AssetRegistry.EnumerateAssets(Filter, [&BlueprintsDerivedFromNativeModifiedClasses, &bTooManyFiles, &DerivedClassNames, ChangedHeaderModule, ChangedHeaderReleativeToModule](const FAssetData& AssetData)
					{
						FString PackageName = AssetData.PackageName.ToString();
						// 跳过 DataOnly 蓝图和 Uncooked 目录，避免依赖扩展规模失控。
						// Don't check data-only blueprints, we'll be here all day.
						if (!AssetData.GetTagValueRef<bool>(FBlueprintTags::IsDataOnly) && !UEditorValidator::IsInUncookedFolder(PackageName))
						{
							// 读取 GeneratedClassPath，确认蓝图生成类是否属于目标派生类集合。
							// Need to get the generated class here to see if it's one in the derived set we care about.
							const FString ClassFromData = AssetData.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);
							if (!ClassFromData.IsEmpty())
							{
								const FTopLevelAssetPath ClassObjectPath(FPackageName::ExportTextPathToObjectPath(ClassFromData));
								if (DerivedClassNames.Contains(ClassObjectPath))
								{
									UE_LOG(LogLyraEditor, Display, TEXT("\tAdding %s To Validate"), *PackageName);

									BlueprintsDerivedFromNativeModifiedClasses.Emplace(AssetData);

									if (BlueprintsDerivedFromNativeModifiedClasses.Num() >= GMaxAssetsChangedByAHeader)
									{
										bTooManyFiles = true;
										UE_LOG(LogLyraEditor, Display, TEXT("Too many assets invalidated (Max %d) by change to, %s + %s"), GMaxAssetsChangedByAHeader, *ChangedHeaderModule, *ChangedHeaderReleativeToModule);
										return false; /* 已达到受影响文件上限，停止枚举。 */ // Stop enumerating.
									}
								}
							}
						}
						return true;
					});
			}
		}

		// 第三步：把可能受头文件变化影响的蓝图包报告为需重新验证的变更资产。
		// STEP 3 - Report the possibly changed blueprints as affected modified packages that need
		// to be proved out.
		for (const FAssetData& BlueprintsDerivedFromNativeModifiedClass : BlueprintsDerivedFromNativeModifiedClasses)
		{
			OutChangedPackageNames.Add(BlueprintsDerivedFromNativeModifiedClass.PackageName.ToString());
		}
	}
}

#undef LOCTEXT_NAMESPACE
