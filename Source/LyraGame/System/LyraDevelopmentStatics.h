// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"

#include "LyraDevelopmentStatics.generated.h"

class UClass;
class UObject;
class UWorld;
struct FAssetData;
struct FFrame;

UCLASS()
class ULyraDevelopmentStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 是否在 PIE 中跳过热身、等待玩家等完整比赛流程，直接进入 Gameplay。
	// 仅编辑器运行且 Lyra Developer Settings 的 bTestFullGameFlowInPIE 为 false 时返回 true。
	// Should game logic skip directly to gameplay (skipping any match warmup / waiting for players / etc... aspects)
	// Will always return false except when playing in the editor and bTestFullGameFlowInPIE (in Lyra Developer Settings) is false
	UFUNCTION(BlueprintCallable, Category="Lyra")
	static bool ShouldSkipDirectlyToGameplay();

	// 编辑器中是否加载纯装饰背景资源。
	// 除非 PIE 且 bSkipLoadingCosmeticBackgroundsInPIE 为 true，否则始终返回 true。
	// Should game logic load cosmetic backgrounds in the editor?
	// Will always return true except when playing in the editor and bSkipLoadingCosmeticBackgroundsInPIE (in Lyra Developer Settings) is true
	UFUNCTION(BlueprintCallable, Category = "Lyra", meta=(ExpandBoolAsExecs="ReturnValue"))
	static bool ShouldLoadCosmeticBackgrounds();

	// 返回与 ShouldLoadCosmeticBackgrounds 相同的装饰背景加载策略，供非执行引脚场景查询。
	// Should game logic load cosmetic backgrounds in the editor?
	// Will always return true except when playing in the editor and bSkipLoadingCosmeticBackgroundsInPIE (in Lyra Developer Settings) is true
	UFUNCTION(BlueprintCallable, Category = "Lyra")
	static bool CanPlayerBotsAttack();

	// 查找最适合执行服务器作弊命令的 PIE World：可能是唯一的单机 World、监听服务器或专用服务器。
	// Finds the most appropriate play-in-editor world to run 'server' cheats on
	//   This might be the only world if running standalone, the listen server, or the dedicated server
	static UWorld* FindPlayInEditorAuthorityWorld();

	// 使用短名称及若干启发式规则查找指定基类的派生 Class，便于作弊控制台输入。
	// Tries to find a class by a short name (with some heuristics to improve the usability when done via a cheat console)
	static UClass* FindClassByShortName(const FString& SearchToken, UClass* DesiredBaseClass, bool bLogFailures = true);

	template <typename DesiredClass>
	static TSubclassOf<DesiredClass> FindClassByShortName(const FString& SearchToken, bool bLogFailures = true)
	{
		return FindClassByShortName(SearchToken, DesiredClass::StaticClass(), bLogFailures);
	}

private:
	static TArray<FAssetData> GetAllBlueprints();
	static UClass* FindBlueprintClass(const FString& TargetNameRaw, UClass* DesiredBaseClass);
};
