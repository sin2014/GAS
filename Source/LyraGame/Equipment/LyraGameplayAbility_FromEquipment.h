// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "LyraGameplayAbility_FromEquipment.generated.h"

class ULyraEquipmentInstance;
class ULyraInventoryItemInstance;

/**
 * 由装备实例授予并与其绑定的 GameplayAbility；当前 GameplayAbilitySpec 的 SourceObject 即关联装备实例。
 */
/**
 * ULyraGameplayAbility_FromEquipment
 *
 * An ability granted by and associated with an equipment instance
 */
UCLASS()
class ULyraGameplayAbility_FromEquipment : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:

	ULyraGameplayAbility_FromEquipment(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category="Lyra|Ability")
	ULyraEquipmentInstance* GetAssociatedEquipment() const;

	UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
	ULyraInventoryItemInstance* GetAssociatedItem() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

};
