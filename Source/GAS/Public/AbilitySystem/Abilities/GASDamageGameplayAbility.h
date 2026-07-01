// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "Interaction/CombatInterface.h"
#include "AbilitySystem/Abilities/GASGameplayAbility.h"
#include "GASDamageGameplayAbility.generated.h"

/**
 * 
 */
UCLASS()
class GAS_API UGASDamageGameplayAbility : public UGASGameplayAbility
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void CauseDamage(AActor* TargetActor);
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	TMap<FGameplayTag, FScalableFloat> DamageTypes;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	FScalableFloat DamageMultiplier;
	
	UFUNCTION(BlueprintPure)
	FTaggedMontage GetRandomTaggedMontageFromArray(const TArray<FTaggedMontage>& TaggedMontages) const;
};
