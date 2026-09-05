// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorValidator_Load.h"

#include "AssetCompilingManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Validation/EditorValidator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorValidator_Load)

#define LOCTEXT_NAMESPACE "EditorValidator"

// 该列表仅忽略“已在内存中的资产被复制到临时包重载”所特有的日志。
// 只能加入由同一资产同时存在于两个包中导致的已知误报。
// This list only ignores log messages that occur while we are reloading an asset that is already in memory
// Should only be used for warnings that occur as a result of having the asset in memory in two different packages
// 临时副本重载内存资产时允许忽略的已知双包误报警告前缀。
TArray<FString> UEditorValidator_Load::InMemoryReloadLogIgnoreList = { TEXT("Enum name collision: '") };

// 启用资产加载验证器，并配置对一般资产执行磁盘重载检查。
UEditorValidator_Load::UEditorValidator_Load()
	: Super()
{
}

// Commandlet 已在运行中加载内容，因此仅在非 Commandlet 环境启用该验证器。
bool UEditorValidator_Load::IsEnabled() const
{
	// Commandlet 运行期间已经从磁盘加载内容，无需再执行内存资产重载验证。
	// Commandlets do not need this validation step as they loaded the content while running.
	return !IsRunningCommandlet() && Super::IsEnabled();
}

// 接受有有效包名且通过基类过滤的资产，World 和 ExternalActor 等特殊包在加载阶段再跳过。
bool UEditorValidator_Load::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return Super::CanValidateAsset_Implementation(InAsset) && InAsset != nullptr;
}

// 对资产所属包执行独立重载并把捕获的加载警告和错误写入 DataValidationContext。
EDataValidationResult UEditorValidator_Load::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	check(InAsset);

	TArray<FString> WarningsAndErrors;
	if (GetLoadWarningsAndErrorsForPackage(InAsset->GetOutermost()->GetName(), WarningsAndErrors))
	{
		for (const FString& WarningOrError : WarningsAndErrors)
		{
			AssetFails(InAsset, FText::FromString(WarningOrError));
		}
	}
	else
	{
		AssetFails(InAsset, LOCTEXT("Load_FailedLoad", "Failed to get package load warnings and errors"));
	}

	if (GetValidationResult() != EDataValidationResult::Invalid)
	{
		AssetPasses(InAsset);
	}

	return GetValidationResult();
}

// 对内存包复制临时文件后以独立包名重载，或直接加载未驻留包，收集日志并清理 Loader 与临时包。
bool UEditorValidator_Load::GetLoadWarningsAndErrorsForPackage(const FString& PackageName, TArray<FString>& OutWarningsAndErrors)
{
	check(!PackageName.IsEmpty());
	check(GEngine);

	UPackage* const ExistingPackage = FindPackage(nullptr, *PackageName);

	if (ExistingPackage == GetTransientPackage())
	{
		return true;
	}

	// 跳过 World 和 External Actor 包，避免临时复制加载破坏关卡资源关系。
	// Skip World or External Actor packages
	if (ExistingPackage && UWorld::IsWorldOrWorldExternalPackage(ExistingPackage))
	{
		return true;
	}

	// Commandlet 不加载临时副本，因为过程需要 GC，可能销毁上层调用栈仍使用的对象；首次正常加载已足够。
	// Commandlet 对象也不使用 RF_Standalone，反复加载同一资产会显著增加执行时间。
	// Commandlets shouldnt load the temporary packages since it involves collecting garbage and may destroy objects higher in the callstack. Loading it the one time is probably good enough
	// Also since commandlets dont use RF_Standalone, this could greatly increase commandlet execution time when loading the same assets over and over
	if (ExistingPackage && !IsRunningCommandlet() && UEditorValidator::ShouldAllowFullValidation() && !ExistingPackage->ContainsMap() && !PackageName.EndsWith(TEXT("_BuiltData")))
	{
		// 将磁盘资产复制到临时目录，以独立包名重载并捕获真实加载问题。
		// Copy the asset file to the temp directory and load it
		const FString& SrcPackageName = PackageName;
		FString SrcFilename;
		const bool bSourceFileExists = FPackageName::DoesPackageExist(SrcPackageName, &SrcFilename);
		if (bSourceFileExists)
		{
			static int32 PackageIdentifier = 0;
			FString DestPackageName = FString::Printf(TEXT("/Temp/%s_%d"), *FPackageName::GetLongPackageAssetName(ExistingPackage->GetName()), PackageIdentifier++);
			FString DestFilename = FPackageName::LongPackageNameToFilename(DestPackageName, FPaths::GetExtension(SrcFilename, true));
			uint32 CopyResult = IFileManager::Get().Copy(*DestFilename, *SrcFilename);
			if (ensure(CopyResult == COPY_OK))
			{
				// 收集整个临时重载过程的警告和错误，用于决定验证结果。
				// Gather all warnings and errors during the process to determine return value
				UPackage* LoadedPackage = nullptr;
				{
					FLyraValidationMessageGatherer::AddIgnorePatterns(InMemoryReloadLogIgnoreList);
					FLyraValidationMessageGatherer ScopedMessageGatherer;
					// 蓝图先编译原资产，再以 DisableCompileOnLoad 加载副本；旁路副本遇到涉及自身的循环引用时无法可靠编译。
					// If we are loading a blueprint, compile the original and load the duplicate with DisableCompileOnLoad, since BPs loaded on the side may not compile if there are circular references involving self
					int32 LoadFlags = LOAD_ForDiff;
					{
						TArray<UObject*> AllExistingObjects;
						GetObjectsWithPackage(ExistingPackage, AllExistingObjects, EGetObjectsFlags::None);
						TArray<UBlueprint*> AllNonDOBPs;
						for (UObject* Obj : AllExistingObjects)
						{
							UBlueprint* BP = Cast<UBlueprint>(Obj);
							if (BP && !FBlueprintEditorUtils::IsDataOnlyBlueprint(BP))
							{
								AllNonDOBPs.Add(BP);
							}
						}
						if (AllNonDOBPs.Num() > 0)
						{
							LoadFlags |= LOAD_DisableCompileOnLoad;
							for (UBlueprint* BP : AllNonDOBPs)
							{
								check(BP);
								FKismetEditorUtilities::CompileBlueprint(BP);
							}
						}
					}
					LoadedPackage = LoadPackage(NULL, *DestPackageName, LoadFlags);
				
					// 等待新加载资产完成异步编译，否则无法安全重置包 Loader，也无法确认是否产生错误。
					// Make sure what we just loaded has finish compiling otherwise we won't be able
					// to reset loaders for the package or verify if errors have been emitted.
					FAssetCompilingManager::Get().FinishAllCompilation();

					for (const FString& LoadWarningOrError : ScopedMessageGatherer.GetAllWarningsAndErrors())
					{
						FString SanitizedMessage = LoadWarningOrError.Replace(*DestFilename, *SrcFilename);
						SanitizedMessage = SanitizedMessage.Replace(*DestPackageName, *SrcPackageName);
						OutWarningsAndErrors.Add(SanitizedMessage);
					}
					FLyraValidationMessageGatherer::RemoveIgnorePatterns(InMemoryReloadLogIgnoreList);
				}
				if (LoadedPackage)
				{
					ResetLoaders(LoadedPackage);
					IFileManager::Get().Delete(*DestFilename);
					TArray<UObject*> AllLoadedObjects;
					GetObjectsWithPackage(LoadedPackage, AllLoadedObjects, EGetObjectsFlags::IncludeNestedObjects);
					for (UObject* Obj : AllLoadedObjects)
					{
						if (Obj->IsRooted())
						{
							continue;
						}
						Obj->ClearFlags(RF_Public | RF_Standalone);
						Obj->SetFlags(RF_Transient);
						if (UWorld* WorldToDestroy = Cast<UWorld>(Obj))
						{
							WorldToDestroy->DestroyWorld(true);
						}
						Obj->MarkAsGarbage();
					}
					GEngine->ForceGarbageCollection(true);
				}
			}
			else
			{
				// 无法复制到临时目录，不能执行独立重载验证。
				// Failed to copy the file to the temp folder
				return false;
			}
		}
		else
		{
			// 资产只存在于内存且尚未保存，没有可复制的源文件。
			// It was in memory but not yet saved probably (no source file)
			return false;
		}
	}
	else
	{
		// 资产尚未在内存中，直接从原包加载并收集日志即可。
		// Not in memory, just load it
		FLyraValidationMessageGatherer ScopedMessageGatherer;
		LoadPackage(nullptr, *PackageName, LOAD_None);
		OutWarningsAndErrors = ScopedMessageGatherer.GetAllWarningsAndErrors();
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

