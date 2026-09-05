// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"

#include "ContentValidationCommandlet.generated.h"

class IAssetRegistry;
class UObject;

UCLASS()
class UContentValidationCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	// UCommandlet 入口：解析参数、收集变更并执行内容验证及可选自动导出步骤。
	// Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	// End UCommandlet Interface

private:
	/** 内容验证之后可按命令行参数执行的自动导出和持久化步骤。 */
	/** Validate steps */
	bool AutoExportMCPTemplates(const TArray<FString>& ChangedPackageNames, const TArray<FString>& DeletedPackageNames, const TArray<FString>& ChangedCode, const TArray<FString>& ChangedOtherFiles, const FString& SyncedCL, const FString& Robomerge, bool& bOutDidExport);
	bool AutoExportDadContent(const FString& BuildCL, const FString& AccessToken);
	bool AutoPersistDadContent(const FString& AccessToken);

private:
	/** 从 Perforce、AssetRegistry 和包路径中解析本次需要验证的变更集合。 */
	/** Helper functions */
	bool GetAllChangedFiles(IAssetRegistry& AssetRegistry, const FString& P4CmdString, TArray<FString>& OutChangedPackageNames, TArray<FString>& DeletedPackageNames, TArray<FString>& OutChangedCode, TArray<FString>& OutChangedOtherFiles) const;
	void GetAllPackagesInPath(IAssetRegistry& AssetRegistry, const FString& InPathString, TArray<FString>& OutPackageNames) const;
	void GetAllPackagesOfType(const FString& OfTypeString, TArray<FString>& OutPackageNames) const;
	bool LaunchP4(const FString& Args, TArray<FString>& Output, int32& OutReturnCode) const;
	FString GetLocalPathFromDepotPath(const FString& DepotPathName) const;
};
