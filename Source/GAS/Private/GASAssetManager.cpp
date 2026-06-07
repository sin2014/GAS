// ZYZ

#include "GASAssetManager.h"
#include "AbilitySystemGlobals.h"
#include "GASGameplayTags.h"

UGASAssetManager& UGASAssetManager::Get()
{
	check(GEngine);
	UGASAssetManager* GASAssetManager = Cast<UGASAssetManager>(GEngine->AssetManager);
	return *GASAssetManager;
}

void UGASAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	
	FGASGameplayTags::IntializeNativeGameplayTags();
	
	// This is required to use Target Data!
	UAbilitySystemGlobals::Get().InitGlobalData();
}
