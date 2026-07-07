// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Interaction/CombatInterface.h"
#include "GASCharacterBase.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayEffect;
class UGameplayAbility;
class UAnimMontage;

UCLASS(Abstract)
class GAS_API AGASCharacterBase : public ACharacter, public IAbilitySystemInterface, public ICombatInterface
{
	GENERATED_BODY()

public:
	AGASCharacterBase();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const;
	
	/**
	 * Combat Interface
	 */
	virtual UAnimMontage* GetHitReactMontage_Implementation() override;
	virtual FVector GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag) override;
	virtual AActor* GetAvatar_Implementation() override;
	virtual bool IsDead_Implementation() const override;
	virtual void Die() override;
	virtual TArray<FTaggedMontage> GetAttackMontages_Implementation() override;
	FTaggedMontage GetTaggedMontageByTag_Implementation(const FGameplayTag& MontageTag) override;
	virtual UNiagaraSystem* GetBloodEffect_Implementation() override;
	
	UFUNCTION(NetMulticast, Reliable)
	virtual void MuticastHandleDeath();
	
	UPROPERTY(EditAnywhere, Category = "Combat")
	TArray<FTaggedMontage> AttackMontages;

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USkeletalMeshComponent> Weapon;
	
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName WeaponTipSocketName;
	
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName LeftHandSocketName;
	
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName RightHandSocketName;
	
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName LeftFootSocketName;
	
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName RightFootSocketName;
	
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName TailSocketName;
	
	bool bDead = false;
	
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;
	
	virtual void InitAbilityActorInfo();
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttributes;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultSecondaryAttributes;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultVitalAttributes;
	
	void ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const;
	virtual void InitializeDefaultAttributes() const;
	
	void AddCharacterAbilities();
	
	/* Dissolve Effects */
	void Dissolve();
	
	UFUNCTION(BlueprintImplementableEvent)
	void StartDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);
	
	UFUNCTION(BlueprintImplementableEvent)
	void StartWeaponDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dissolve")
	TObjectPtr<UMaterialInstance> DissolveMaterialInstance;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dissolve")
	TObjectPtr<UMaterialInstance> WeaponDissolveMaterialInstance;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ComBat")
	UNiagaraSystem* BloodEffect;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ComBat")
	USoundBase* DeathSound;
	
private:
	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;
	
	UPROPERTY(EditAnywhere, Category = "ComBat")
	TObjectPtr<UAnimMontage> HitReactMontage;
};
