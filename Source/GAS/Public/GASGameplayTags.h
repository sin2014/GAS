// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * GASGameplayTags
 * 
 * Singleton containing native gameplay tags
 */

struct FGASGameplayTags
{
public:
	static const FGASGameplayTags& Get() { return GameplayTags; }
	static  void IntializeNativeGameplayTags();
	
	FGameplayTag Attributes_Primary_Strength;
	FGameplayTag Attributes_Primary_Intelligence;
	FGameplayTag Attributes_Primary_Resilience;
	FGameplayTag Attributes_Primary_Vigor;
	
	FGameplayTag Attributes_Secondary_MaxHealth;
	FGameplayTag Attributes_Secondary_MaxMana;
	FGameplayTag Attributes_Secondary_HealthRegeneration;
	FGameplayTag Attributes_Secondary_ManaRegeneration;
	
	FGameplayTag Attributes_Secondary_Armor;
	FGameplayTag Attributes_Secondary_ArmorPenetration;
	FGameplayTag Attributes_Secondary_BlockChance;
	FGameplayTag Attributes_Secondary_CriticalHitChance;
	FGameplayTag Attributes_Secondary_CriticalHitDamage;
	FGameplayTag Attributes_Secondary_CriticalHitResistance;
	
	FGameplayTag Attributes_Resistance_Fire;
	FGameplayTag Attributes_Resistance_Arcane;
	FGameplayTag Attributes_Resistance_Lightning;
	FGameplayTag Attributes_Resistance_Physical;
	
	FGameplayTag InputTag_LMB;
	FGameplayTag InputTag_RMB;
	FGameplayTag InputTag_1;
	FGameplayTag InputTag_2;
	FGameplayTag InputTag_3;
	FGameplayTag InputTag_4;

	FGameplayTag Damage;
	FGameplayTag Damage_Multiplier;
	
	TMap<FGameplayTag, FGameplayTag> DamageTypesToResistances;
	FGameplayTag Damage_Fire;
	FGameplayTag Damage_Arcane;
	FGameplayTag Damage_Lightning;
	FGameplayTag Damage_Physical;
	
	FGameplayTag Abilities_Attack;
	
	FGameplayTag Effects_HitReact;

private:
	static FGASGameplayTags GameplayTags;
};
