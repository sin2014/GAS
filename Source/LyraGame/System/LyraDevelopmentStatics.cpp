// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraDevelopmentStatics.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "Development/LyraDeveloperSettings.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraDevelopmentStatics)

// PIE 中根据开发者设置决定是否跳过完整前端流程，非编辑器运行始终不跳过。
bool ULyraDevelopmentStatics::ShouldSkipDirectlyToGameplay()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		return !GetDefault<ULyraDeveloperSettings>()->bTestFullGameFlowInPIE;
	}
#endif
	return false;
}

// PIE 中允许开发者跳过装饰背景加载，非编辑器运行默认加载。
bool ULyraDevelopmentStatics::ShouldLoadCosmeticBackgrounds()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		return !GetDefault<ULyraDeveloperSettings>()->bSkipLoadingCosmeticBackgroundsInPIE;
	}
#endif
	return true;
}

// PIE 中读取玩家 Bot 攻击开关，非编辑器运行默认允许攻击。
bool ULyraDevelopmentStatics::CanPlayerBotsAttack()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		return GetDefault<ULyraDeveloperSettings>()->bAllowPlayerBotsToAttack;
	}
#endif
	return true;
}

//@TODO：多数调用实际需要在每个权威 PIE World 执行一段 Lambda，而不只是返回单个 World。
//@TODO: Actually want to take a lambda and run on every authority world most of the time...
// 扫描 PIE WorldContext 并优先返回专用服务器；没有专服时按当前选择逻辑返回一个 PIE World 候选。
UWorld* ULyraDevelopmentStatics::FindPlayInEditorAuthorityWorld()
{
	check(GEngine);

	// 查找服务器 World；任意 PIE World 都可作为回退，但专用服务器 World 优先级最高。
	// Find the server world (any PIE world will do, in case they are running without a dedicated server, but the ded. server world is ideal)
	UWorld* ServerWorld = nullptr;
#if WITH_EDITOR
	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::PIE)
		{
			if (UWorld* TestWorld = WorldContext.World())
			{
				if (WorldContext.RunAsDedicated)
				{
					// 专用服务器是最理想目标，找到后可立即结束搜索。
					// Ideal case
					ServerWorld = TestWorld;
					break;
				}
				else if (ServerWorld == nullptr)
				{
					ServerWorld = TestWorld;
				}
				else
				{
					// 已有候选时，选择 NetMode 更偏向服务器权威的一项。
					// We already have a candidate, see if this one is 'better'
					if (TestWorld->GetNetMode() < ServerWorld->GetNetMode())
					{
						return ServerWorld;
					}
				}
			}
		}
	}
#endif

	return ServerWorld;
}

// 通过 AssetRegistry 查询项目可见的全部 Blueprint 资产元数据，不立即加载资产对象。
TArray<FAssetData> ULyraDevelopmentStatics::GetAllBlueprints()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FName PluginAssetPath;

	TArray<FAssetData> BlueprintList;
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	AssetRegistryModule.Get().GetAssets(Filter, BlueprintList);

	return BlueprintList;
}

// 按短资产名或完整对象路径查找 Blueprint，加载命中资产并返回符合指定基类的 GeneratedClass。
UClass* ULyraDevelopmentStatics::FindBlueprintClass(const FString& TargetNameRaw, UClass* DesiredBaseClass)
{
	FString TargetName = TargetNameRaw;
	TargetName.RemoveFromEnd(TEXT("_C"), ESearchCase::CaseSensitive);

	TArray<FAssetData> BlueprintList = ULyraDevelopmentStatics::GetAllBlueprints();
	for (const FAssetData& AssetData : BlueprintList)
	{
		if ((AssetData.AssetName.ToString() == TargetName) || (AssetData.GetObjectPathString() == TargetName))
		{
			if (UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset()))
			{
				if (UClass* GeneratedClass = BP->GeneratedClass)
				{
					if (GeneratedClass->IsChildOf(DesiredBaseClass))
					{
						return GeneratedClass;
					}
				}
			}
		}
	}

	return nullptr;
}

// 规范化类名或对象路径，先查已加载原生类型、再查 Blueprint 资产，并验证候选类继承关系。
UClass* ULyraDevelopmentStatics::FindClassByShortName(const FString& SearchToken, UClass* DesiredBaseClass, bool bLogFailures)
{
	check(DesiredBaseClass);

	FString TargetName = SearchToken;

	// 先搜索原生 Class 与已加载资产，未命中时再查询 AssetRegistry。
	// Check native classes and loaded assets first before resorting to the asset registry
	bool bIsValidClassName = true;
	if (TargetName.IsEmpty() || TargetName.Contains(TEXT(" ")))
	{
		bIsValidClassName = false;
	}
	else if (!FPackageName::IsShortPackageName(TargetName))
	{
		if (TargetName.Contains(TEXT(".")))
		{
			// 将 type'path' 导出文本格式转换为纯对象路径；不含引号时原样返回。
			// Convert type'path' to just path (will return the full string if it doesn't have ' in it)
			TargetName = FPackageName::ExportTextPathToObjectPath(TargetName);

			FString PackageName;
			FString ObjectName;
			TargetName.Split(TEXT("."), &PackageName, &ObjectName);

			const bool bIncludeReadOnlyRoots = true;
			FText Reason;
			if (!FPackageName::IsValidLongPackageName(PackageName, bIncludeReadOnlyRoots, &Reason))
			{
				bIsValidClassName = false;
			}
		}
		else
		{
			bIsValidClassName = false;
		}
	}

	UClass* ResultClass = nullptr;
	if (bIsValidClassName)
	{
		ResultClass = UClass::TryFindTypeSlow<UClass>(TargetName);
	}

	// 仍未找到时，通过 AssetRegistry 搜索满足基类要求的蓝图资产。
	// If we still haven't found anything yet, try the asset registry for blueprints that match the requirements
	if (ResultClass == nullptr)
	{
		ResultClass = FindBlueprintClass(TargetName, DesiredBaseClass);
	}

	// 找到候选 Class 后验证其是否派生自 DesiredBaseClass。
	// Now validate the class (if we have one)
	if (ResultClass != nullptr)
	{
		if (!ResultClass->IsChildOf(DesiredBaseClass))
		{
			if (bLogFailures)
			{
				UE_LOG(LogConsoleResponse, Warning, TEXT("Found an asset %s but it wasn't of type %s"), *ResultClass->GetPathName(), *DesiredBaseClass->GetName());
			}
			ResultClass = nullptr;
		}
	}
	else
	{
		if (bLogFailures)
		{
			UE_LOG(LogConsoleResponse, Warning, TEXT("Failed to find class of type %s named %s"), *DesiredBaseClass->GetName(), *SearchToken);
		}
	}

	return ResultClass;
}

