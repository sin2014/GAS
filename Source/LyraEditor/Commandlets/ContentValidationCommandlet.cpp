// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContentValidationCommandlet.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DataValidationModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "ShaderCompiler.h"
#include "SourceControlHelpers.h"
#include "Validation/EditorValidator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ContentValidationCommandlet)

// ContentValidationCommandlet 的变更收集、资产加载和验证结果日志分类。
DEFINE_LOG_CATEGORY_STATIC(LogLyraContentValidation, Log, Log);

class FScopedContentValidationMessageGatherer : public FOutputDevice
{
public:

	FScopedContentValidationMessageGatherer()
		: bAtLeastOneError(false)
	{
		GLog->AddOutputDevice(this);
	}

	~FScopedContentValidationMessageGatherer()
	{
		GLog->RemoveOutputDevice(this);
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if (Verbosity <= ELogVerbosity::Error)
		{
			bAtLeastOneError = true;
		}
	}

	bool bAtLeastOneError;
};

// 配置内容验证 Commandlet 为编辑器模式运行，启用控制台日志并允许渲染相关资产加载。
UContentValidationCommandlet::UContentValidationCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 解析 P4 变更、目录、类型和包上限参数，收集变更包后调用项目设置与资产验证，并以错误数决定进程返回码。
int32 UContentValidationCommandlet::Main(const FString& FullCommandLine)
{
	UE_LOG(LogLyraContentValidation, Display, TEXT("Running ContentValidationCommandlet commandlet..."));
	
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;
	ParseCommandLine(*FullCommandLine, Tokens, Switches, Params);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.SearchAllAssets(true);

	int32 ReturnVal = 0;

	TArray<FString> ChangedPackageNames;
	TArray<FString> DeletedPackageNames;
	TArray<FString> ChangedCode;
	TArray<FString> ChangedOtherFiles;
	FString* P4FilterString = Params.Find(TEXT("P4Filter"));
	if (P4FilterString && !P4FilterString->IsEmpty())
	{
		FString P4CmdString = TEXT("files ") + *P4FilterString;
		if (!GetAllChangedFiles(AssetRegistry, P4CmdString, ChangedPackageNames, DeletedPackageNames, ChangedCode, ChangedOtherFiles))
		{
			UE_LOG(LogLyraContentValidation, Display, TEXT("ContentValidation returning 1. Failed to get changed files."));
			ReturnVal = 1;
		}
	}

	FString* P4ChangelistString = Params.Find(TEXT("P4Changelist"));
	if (P4ChangelistString && !P4ChangelistString->IsEmpty())
	{
		FString P4CmdString = TEXT("opened -c ") + *P4ChangelistString;
		if (!GetAllChangedFiles(AssetRegistry, P4CmdString, ChangedPackageNames, DeletedPackageNames, ChangedCode, ChangedOtherFiles))
		{
			UE_LOG(LogLyraContentValidation, Display, TEXT("ContentValidation returning 1. Failed to get changed files."));
			ReturnVal = 1;
		}
	}

	bool bP4Opened = Switches.Contains(TEXT("P4Opened"));
	if (bP4Opened)
	{
		check(GConfig);

		FString Workspace;
		FString* P4ClientString = Params.Find(TEXT("P4Client"));

		if (P4ClientString && !P4ClientString->IsEmpty())
		{
			Workspace = *P4ClientString;
		}
		else
		{
			const FString& SSCIniFile = SourceControlHelpers::GetSettingsIni();
			GConfig->GetString(TEXT("PerforceSourceControl.PerforceSourceControlSettings"), TEXT("Workspace"), Workspace, SSCIniFile);
		}

		if (!Workspace.IsEmpty())
		{
			FString P4CmdString = FString::Printf(TEXT("-c%s opened"), *Workspace);
			if (!GetAllChangedFiles(AssetRegistry, P4CmdString, ChangedPackageNames, DeletedPackageNames, ChangedCode, ChangedOtherFiles))
			{
				UE_LOG(LogLyraContentValidation, Display, TEXT("ContentValidation returning 1. Failed to get changed files."));
				ReturnVal = 1;
			}
		}
		else
		{
			UE_LOG(LogLyraContentValidation, Error, TEXT("P4 workspace was not found when using P4Opened"));
			UE_LOG(LogLyraContentValidation, Display, TEXT("ContentValidation returning 1. Workspace not found."));
			ReturnVal = 1;
		}
	}

	int32 MaxPackagesToLoad = 2000;

	FString* InPathString = Params.Find(TEXT("InPath"));
	if (InPathString && !InPathString->IsEmpty())
	{
		GetAllPackagesInPath(AssetRegistry, *InPathString, ChangedPackageNames);
	}

	FString* OfTypeString = Params.Find(TEXT("OfType"));
	if (OfTypeString && !OfTypeString->IsEmpty())
	{
		const int32 InitialPackages = ChangedPackageNames.Num();
		GetAllPackagesOfType(*OfTypeString, ChangedPackageNames);
		MaxPackagesToLoad += ChangedPackageNames.Num() - InitialPackages;
	}

	FString* SpecificPackagesString = Params.Find(TEXT("Packages"));
	if (SpecificPackagesString && !SpecificPackagesString->IsEmpty())
	{
		TArray<FString> PackagePaths;
		SpecificPackagesString->ParseIntoArray(PackagePaths, TEXT("+"));
		ChangedPackageNames.Append(PackagePaths);
	}

	// 验证材质加载时会刷新 Shader 编译；先完成已有任务，避免无关 Shader 警告被归因到当前包。
	// We will be flushing shader compile as we load materials, so don't let other shader warnings be attributed incorrectly to the package that is loading.
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}

	FString* InMaxPackagesToLoadString = Params.Find(TEXT("MaxPackagesToLoad"));
	if (InMaxPackagesToLoadString)
	{
		MaxPackagesToLoad = FCString::Atoi(**InMaxPackagesToLoadString);
	}

	TArray<FString> AllWarningsAndErrors;
	UEditorValidator::ValidatePackages(ChangedPackageNames, DeletedPackageNames, MaxPackagesToLoad, AllWarningsAndErrors, EDataValidationUsecase::Commandlet);

	if (!UEditorValidator::ValidateProjectSettings())
	{
		ReturnVal = 1;
	}

	return ReturnVal;
}

// 执行 P4 命令并把仓库变更分类为现有/删除资产包、代码和其他文件，同时将插件及 `/Game/` 路径转换为包名。
bool UContentValidationCommandlet::GetAllChangedFiles(IAssetRegistry& AssetRegistry, const FString& P4CmdString, TArray<FString>& OutChangedPackageNames, TArray<FString>& DeletedPackageNames, TArray<FString>& OutChangedCode, TArray<FString>& OutChangedOtherFiles) const
{
	TArray<FString> Results;
	int32 ReturnCode = 0;
	if (LaunchP4(P4CmdString, Results, ReturnCode))
	{
		if (ReturnCode == 0)
		{
			for (const FString& Result : Results)
			{
				FString DepotPathName;
				FString ExtraInfoAfterPound;
				if (Result.Split(TEXT("#"), &DepotPathName, &ExtraInfoAfterPound))
				{
					if (DepotPathName.EndsWith(TEXT(".uasset")) || DepotPathName.EndsWith(TEXT(".umap")))
					{
						FString FullPackageName;
						{
							// 将主项目 Content 仓库路径转换为 `/Game/` 包名。
							// Check for /Game/ assets
							FString PostContentPath;
							if (DepotPathName.Split(TEXT("LyraGame/Content/"), nullptr, &PostContentPath)) /* TODO：项目或模块重命名后该硬编码路径可能失效。 */ //@TODO: RENAME: Potential issue when modules are renamed
							{
								if (!PostContentPath.IsEmpty())
								{
									const FString PostContentPathWithoutExtension = FPaths::GetBaseFilename(PostContentPath, false);
									FString PackageNameToTest = TEXT("/Game/") + PostContentPathWithoutExtension;
									if (!UEditorValidator::IsInUncookedFolder(PackageNameToTest))
									{
										FullPackageName = PackageNameToTest;
									}
								}
							}
						}
						
						if (FullPackageName.IsEmpty())
						{
							// 将 Plugins 下的仓库路径转换为对应插件挂载点包名。
							// Check for plugin assets
							FString PostPluginsPath;
							if (DepotPathName.Split(TEXT("LyraGame/Plugins/"), nullptr, &PostPluginsPath))
							{
								const int32 ContentFolderIdx = PostPluginsPath.Find(TEXT("/Content/"));
								if (ContentFolderIdx != INDEX_NONE)
								{
									int32 PluginFolderIdx = PostPluginsPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, ContentFolderIdx - 1);
									if (PluginFolderIdx == INDEX_NONE)
									{
									// 路径直接位于 `/Plugins/` 下，没有前导子目录。
									// No leading /. Directly in the /Plugins/ folder
										PluginFolderIdx = 0;
									}
									else
									{
									// 跳过前导 `/`，处理 `/Plugins/` 下的嵌套插件目录。
									// Skip the leading /. A subfolder in the /Plugins/ folder
										PluginFolderIdx++;
									}
									
									const int32 PostContentFolderIdx = ContentFolderIdx + FCString::Strlen(TEXT("/Content/"));
									const FString PostContentPath = PostPluginsPath.RightChop(PostContentFolderIdx);
									const FString PluginName = PostPluginsPath.Mid(PluginFolderIdx, ContentFolderIdx - PluginFolderIdx);
									if (!PostContentPath.IsEmpty() && !PluginName.IsEmpty())
									{
										TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
										if (Plugin.IsValid() && Plugin->IsEnabled())
										{
											const FString PostContentPathWithoutExtension = FPaths::GetBaseFilename(PostContentPath, false);
											FullPackageName = FString::Printf(TEXT("/%s/%s"), *PluginName, *PostContentPathWithoutExtension);
										}
									}
								}
							}
						}

						if (!FullPackageName.IsEmpty())
						{
							if (ExtraInfoAfterPound.Contains(TEXT("delete")))
							{
								DeletedPackageNames.AddUnique(FullPackageName);
							}
							else
							{
								OutChangedPackageNames.AddUnique(FullPackageName);
							}
						}
					}
					else
					{
						FString PostLyraGamePath;
						if (DepotPathName.Split(TEXT("/LyraGame/"), nullptr, &PostLyraGamePath))
						{
							if (DepotPathName.EndsWith(TEXT(".cpp")))
							{
								OutChangedCode.Add(PostLyraGamePath);
							}
							else if (DepotPathName.EndsWith(TEXT(".h")))
							{
								OutChangedCode.Add(PostLyraGamePath);

								FString ChangedHeaderLocalFilename = GetLocalPathFromDepotPath(DepotPathName);
								if (!ChangedHeaderLocalFilename.IsEmpty())
								{
									UEditorValidator::GetChangedAssetsForCode(AssetRegistry, ChangedHeaderLocalFilename, OutChangedPackageNames);
								}
							}
							else
							{
								OutChangedOtherFiles.Add(PostLyraGamePath);
							}
						}
					}
				}
			}

			return true;
		}
		else
		{
			UE_LOG(LogLyraContentValidation, Error, TEXT("p4 returned non-zero return code %d"), ReturnCode);
		}
	}

	return false;
}

// 通过 AssetRegistry 递归枚举指定虚拟路径下的包名，并排除不参与 Cook 的目录。
void UContentValidationCommandlet::GetAllPackagesInPath(IAssetRegistry& AssetRegistry, const FString& InPathString, TArray<FString>& OutPackageNames) const
{
	TArray<FString> Paths;
	InPathString.ParseIntoArray(Paths, TEXT("+"));

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bIncludeOnlyOnDiskAssets = true;

	for (const FString& Path : Paths)
	{
		Filter.PackagePaths.Add(FName(*Path));
	}

	TArray<FAssetData> AssetsInPaths;
	if (AssetRegistry.GetAssets(Filter, AssetsInPaths))
	{
		for (const FAssetData& AssetData : AssetsInPaths)
		{
			OutPackageNames.Add(AssetData.PackageName.ToString());
		}
	}
}

// 解析类名并查询该类及派生类资产，把匹配资产包名加入验证集合。
void UContentValidationCommandlet::GetAllPackagesOfType(const FString& OfTypeString, TArray<FString>& OutPackageNames) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FString> Types;
	OfTypeString.ParseIntoArray(Types, TEXT("+"));

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bIncludeOnlyOnDiskAssets = true;

	for (const FString& Type : Types)
	{
		FTopLevelAssetPath TypePathName = UClass::TryConvertShortTypeNameToPathName<UStruct>(Type, ELogVerbosity::Error, TEXT("UContentValidationCommandlet"));
		if (TypePathName.IsNull())
		{
			UE_LOG(LogLyraContentValidation, Error, TEXT("Failed to convert short class name \"%s\" to path name. Please use class path names."), *Type);
		}
		else
		{
			Filter.ClassPaths.Add(TypePathName);
		}
	}

	TArray<FAssetData> AssetsOfType;
	if (AssetRegistry.GetAssets(Filter, AssetsOfType))
	{
		for (const FAssetData& AssetData : AssetsOfType)
		{
			OutPackageNames.Add(AssetData.PackageName.ToString());
		}
	}
}

// 用当前进程环境启动 p4 子进程，捕获标准输出、返回码和启动失败状态。
bool UContentValidationCommandlet::LaunchP4(const FString& Args, TArray<FString>& Output, int32& OutReturnCode) const
{
	void* PipeRead = nullptr;
	void* PipeWrite = nullptr;

	verify(FPlatformProcess::CreatePipe(PipeRead, PipeWrite));

	bool bInvoked = false;
	OutReturnCode = -1;
	FString StringOutput;
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(TEXT("p4.exe"), *Args, false, true, true, nullptr, 0, nullptr, PipeWrite);
	if (ProcHandle.IsValid())
	{
		while (FPlatformProcess::IsProcRunning(ProcHandle))
		{
			StringOutput += FPlatformProcess::ReadPipe(PipeRead);
			FPlatformProcess::Sleep(0.1f);
		}

		StringOutput += FPlatformProcess::ReadPipe(PipeRead);
		FPlatformProcess::GetProcReturnCode(ProcHandle, &OutReturnCode);
		bInvoked = true;
	}
	else
	{
		UE_LOG(LogLyraContentValidation, Error, TEXT("Failed to launch p4."));
	}

	FPlatformProcess::ClosePipe(PipeRead, PipeWrite);

	StringOutput.ParseIntoArrayLines(Output);

	return bInvoked;
}

// 调用 `p4 where` 把 Depot 路径转换为本地文件路径；命令失败或无输出时返回空字符串。
FString UContentValidationCommandlet::GetLocalPathFromDepotPath(const FString& DepotPathName) const
{
	FString ReturnString;

	const FString& SSCIniFile = SourceControlHelpers::GetSettingsIni();
	FString Workspace;
	GConfig->GetString(TEXT("PerforceSourceControl.PerforceSourceControlSettings"), TEXT("Workspace"), Workspace, SSCIniFile);

	if (Workspace.IsEmpty())
	{
		FString ParameterValue;
		if (FParse::Value(FCommandLine::Get(), TEXT("P4Client="), ParameterValue))
		{
			Workspace = ParameterValue;
		}
	}

	if (!Workspace.IsEmpty())
	{
		TArray<FString> WhereResults;
		int32 ReturnCode = 0;
		FString P4WhereCommand = FString::Printf(TEXT("-ztag -c%s where %s"), *Workspace, *DepotPathName);
		if (LaunchP4(P4WhereCommand, WhereResults, ReturnCode))
		{
			if (WhereResults.Num() >= 2)
			{
				ReturnString = WhereResults[2];
				ReturnString.RemoveFromStart(TEXT("... path "));
				FPaths::NormalizeFilename(ReturnString);
			}
			else
			{
				UE_LOG(LogLyraContentValidation, Warning, TEXT("GetAllChangedFiles failed to run p4 'where'. WhereResults[0] = '%s'. Not adding any validation for %s"), WhereResults.Num() > 0 ? *WhereResults[0] : TEXT("Invalid"), *DepotPathName);
			}
		}
	}

	return ReturnString;
}

