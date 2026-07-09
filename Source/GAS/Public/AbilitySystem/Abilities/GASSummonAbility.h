// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/GASGameplayAbility.h"
#include "GASSummonAbility.generated.h"

/**
 * 
 */
UCLASS()
class GAS_API UGASSummonAbility : public UGASGameplayAbility
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	TArray<FVector> GetSpawnLocation();
	
	UFUNCTION(BlueprintPure, Category = "Summoning")
	TSubclassOf<APawn> GetRandomMinionClass() const;
	
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	TArray<TSubclassOf<APawn>> MinionClasses;
	
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	int32 NumMinions = 3;
	
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	float MinionSpawnSpread = 90.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	float MinMinionSpawnDistance = 150.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	float MaxMinionSpawnDistance = 450.f;
};
