// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/GASDamageGameplayAbility.h"
#include "GASProjectileSpell.generated.h"

class AGASProjectile;
class UGameplayEffect;
struct FGameplayTag;

/**
 * 
 */
UCLASS()
class GAS_API UGASProjectileSpell : public UGASDamageGameplayAbility
{
	GENERATED_BODY()

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void SpawnProjectile(const FVector& ProjectileTargetLocation, const FGameplayTag& SocketTag);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AGASProjectile> ProjectileClass;
};
