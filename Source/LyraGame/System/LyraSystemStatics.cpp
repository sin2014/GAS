// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraSystemStatics.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "LyraLogChannels.h"
#include "Components/MeshComponent.h"
#include "GameModes/LyraUserFacingExperienceDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSystemStatics)

// 对非 BlueprintClass 的 PrimaryAsset，要求已加载基类派生自非空期望类型，验证通过后返回软对象路径。
TSoftObjectPtr<UObject> ULyraSystemStatics::GetTypedSoftObjectReferenceFromPrimaryAssetId(FPrimaryAssetId PrimaryAssetId, TSubclassOf<UObject> ExpectedAssetType)
{
	if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
	{
		FPrimaryAssetTypeInfo Info;
		if (Manager->GetPrimaryAssetTypeInfo(PrimaryAssetId.PrimaryAssetType, Info) && !Info.bHasBlueprintClasses)
		{
			if (UClass* AssetClass = Info.AssetBaseClassLoaded)
			{
				if ((ExpectedAssetType == nullptr) || !AssetClass->IsChildOf(ExpectedAssetType))
				{
					return nullptr;
				}
			}
			else
			{
				UE_LOG(LogLyra, Warning, TEXT("GetTypedSoftObjectReferenceFromPrimaryAssetId(%s, %s) - AssetBaseClassLoaded was unset so we couldn't validate it, returning null"),
					*PrimaryAssetId.ToString(),
					*GetPathNameSafe(*ExpectedAssetType));
			}

			return TSoftObjectPtr<UObject>(Manager->GetPrimaryAssetPath(PrimaryAssetId));
		}
	}
	return nullptr;
}

// 将会话公布的 Experience 名称组合为 UserFacingExperienceDefinition 类型的 PrimaryAssetId。
FPrimaryAssetId ULyraSystemStatics::GetPrimaryAssetIdFromUserFacingExperienceName(const FString& AdvertisedExperienceID)
{
	const FPrimaryAssetType Type(ULyraUserFacingExperienceDefinition::StaticClass()->GetFName());
	return FPrimaryAssetId(Type, FName(*AdvertisedExperienceID));
}

// 复用当前 WorldContext 的 LastURL，清理 PIE 前缀和 Host/Port 后发起保持连接的相对 ServerTravel。
void ULyraSystemStatics::PlayNextGame(const UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World == nullptr)
	{
		return;
	}

	const FWorldContext& WorldContext = GEngine->GetWorldContextFromWorldChecked(World);
	FURL LastURL = WorldContext.LastURL;

#if WITH_EDITOR
	// PIE Travel 前移除地图包名的 PIE 前缀，恢复可用于服务器 Travel 的真实路径。
	// To transition during PIE we need to strip the PIE prefix from maps.
	LastURL.Map = UWorld::StripPIEPrefixFromPackageName(LastURL.Map, WorldContext.World()->StreamingLevelsPrefix);
#endif

	// 添加 SeamlessTravel 选项以保持客户端连接；未启用无缝 Travel 时会回退为普通 Travel。
	// Add seamless travel option as we want to keep clients connected. This will fall back to hard travel if seamless is disabled
	LastURL.AddOption(TEXT("SeamlessTravel"));

	FString URL = LastURL.ToString();
	// ServerTravel URL 必须移除现有 Host/Port，否则 Travel 会失败。
	// If we don't remove the host/port info the server travel will fail.
	URL.RemoveFromStart(LastURL.GetHostPortString());
	
	const bool bAbsolute = false; /* 此处需要使用 TRAVEL_Relative。 */ // we want to use TRAVEL_Relative
	const bool bShouldSkipGameNotify = false;
	World->ServerTravel(URL, bAbsolute, bShouldSkipGameNotify);
}

// 遍历目标 Actor 可选包含子 Actor 的全部 MeshComponent，统一设置材质标量参数。
void ULyraSystemStatics::SetScalarParameterValueOnAllMeshComponents(AActor* TargetActor, const FName ParameterName, const float ParameterValue, bool bIncludeChildActors)
{
	if (TargetActor != nullptr)
	{
		TargetActor->ForEachComponent<UMeshComponent>(bIncludeChildActors, [=](UMeshComponent* InComponent)
		{
			InComponent->SetScalarParameterValueOnMaterials(ParameterName, ParameterValue);
		});
	}
}

// 遍历目标 Actor 可选包含子 Actor 的全部 MeshComponent，统一设置材质向量参数。
void ULyraSystemStatics::SetVectorParameterValueOnAllMeshComponents(AActor* TargetActor, const FName ParameterName, const FVector ParameterValue, bool bIncludeChildActors)
{
	if (TargetActor != nullptr)
	{
		TargetActor->ForEachComponent<UMeshComponent>(bIncludeChildActors, [=](UMeshComponent* InComponent)
		{
			InComponent->SetVectorParameterValueOnMaterials(ParameterName, ParameterValue);
		});
	}
}

// 将线性颜色转换为向量，并复用统一 MeshComponent 材质向量参数写入路径。
void ULyraSystemStatics::SetColorParameterValueOnAllMeshComponents(AActor* TargetActor, const FName ParameterName, const FLinearColor ParameterValue, bool bIncludeChildActors)
{
	SetVectorParameterValueOnAllMeshComponents(TargetActor, ParameterName, FVector(ParameterValue), bIncludeChildActors);
}

// 收集目标 Actor 上指定类型的组件，并按参数决定是否包含子 Actor 组件。
TArray<UActorComponent*> ULyraSystemStatics::FindComponentsByClass(AActor* TargetActor, TSubclassOf<UActorComponent> ComponentClass, bool bIncludeChildActors)
{
	TArray<UActorComponent*> Components;
	if (TargetActor != nullptr)
	{
		TargetActor->GetComponents(ComponentClass, /*out*/ Components, bIncludeChildActors);

	}
	return MoveTemp(Components);
}

