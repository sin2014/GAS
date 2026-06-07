// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/GASGameplayAbility.h"
#include "GASProjectileSpell.generated.h"

class AGASProjectile;

/**
 * 
 */
UCLASS()
class GAS_API UGASProjectileSpell : public UGASGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void SpawnProjectile(const FVector& ProjectileTargetLocation);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AGASProjectile> ProjectileClass;
};
