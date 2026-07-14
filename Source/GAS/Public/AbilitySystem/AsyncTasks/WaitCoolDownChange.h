// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "WaitCoolDownChange.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectSpec;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCoolDownChangeSignature, float, TimeRemaining);

/**
 * 
 */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = "AsyncTask"))
class GAS_API UWaitCoolDownChange : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintAssignable)
	FCoolDownChangeSignature CoolDownStart;
	
	UPROPERTY(BlueprintAssignable)
	FCoolDownChangeSignature CoolDownEnd;
	
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UWaitCoolDownChange* WaitForCoolDownChange(UAbilitySystemComponent* AbilitySystemComponent,const FGameplayTag InCoolDownTag);
	
	UFUNCTION(BlueprintCallable)
	void EndTask();
	
protected:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ASC;
	
	FGameplayTag CoolDownTag;
	
	void CoolDownTagChanged(const FGameplayTag InCoolDownTag, int32 NewCount);
	void OnActiveEffectAdded(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveEffectHandle);
};
