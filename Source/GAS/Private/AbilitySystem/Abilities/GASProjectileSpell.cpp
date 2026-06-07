// ZYZ

#include "AbilitySystem/Abilities/GASProjectileSpell.h"
#include "Actor/GASProjectile.h"
#include "Interaction/CombatInterface.h"

void UGASProjectileSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                          const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                          const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	
}

void UGASProjectileSpell::SpawnProjectile()
{
	const bool bIsServer = GetAvatarActorFromActorInfo()->HasAuthority();
	if (!bIsServer) return;
	
	ICombatInterface* CombatInterface = Cast<ICombatInterface>(GetAvatarActorFromActorInfo());
	if (CombatInterface)
	{
		const FVector& SocketLocation = CombatInterface->GetCombatSocketLocation();
		
		FTransform SpawnTransform;
		SpawnTransform.SetLocation(SocketLocation);
		//TODO: Set the Projectile's rotation.
		
		AGASProjectile* Projectile = GetWorld()->SpawnActorDeferred<AGASProjectile>(
			ProjectileClass, SpawnTransform,
			GetOwningActorFromActorInfo(),
			Cast<APawn>(GetOwningActorFromActorInfo()),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		);
		
		//TODO: Give the Projectile a Gameplay Effect Spec for causing Damage.
		
		Projectile->FinishSpawning(SpawnTransform);
	}
}
