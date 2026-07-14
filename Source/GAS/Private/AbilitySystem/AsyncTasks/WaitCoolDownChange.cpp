// ZYZ

#include "AbilitySystem/AsyncTasks/WaitCoolDownChange.h"
#include "AbilitySystemComponent.h"

UWaitCoolDownChange* UWaitCoolDownChange::WaitForCoolDownChange(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTag InCoolDownTag)
{
	UWaitCoolDownChange* WaitCoolDownChange = NewObject<UWaitCoolDownChange>();
	WaitCoolDownChange->ASC = AbilitySystemComponent;
	WaitCoolDownChange->CoolDownTag = InCoolDownTag;
	
	if (!IsValid(AbilitySystemComponent) || !InCoolDownTag.IsValid())
	{
		WaitCoolDownChange->EndTask();
		return nullptr;
	}
	
	// To Know When CoolDown Tag has been removed
	AbilitySystemComponent->RegisterGameplayTagEvent(InCoolDownTag, EGameplayTagEventType::NewOrRemoved).AddUObject(
		WaitCoolDownChange, 
		&UWaitCoolDownChange::CoolDownTagChanged
	);
	
	// To Know When CoolDown Tag has been added
	AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		WaitCoolDownChange,
		&UWaitCoolDownChange::OnActiveEffectAdded
	);
	
	return WaitCoolDownChange;
}

void UWaitCoolDownChange::EndTask()
{
	if (!IsValid(ASC)) return;
	ASC->RegisterGameplayTagEvent(CoolDownTag, EGameplayTagEventType::NewOrRemoved).RemoveAll(this);
	
	SetReadyToDestroy();
	MarkAsGarbage();
}

void UWaitCoolDownChange::CoolDownTagChanged(const FGameplayTag InCoolDownTag, int32 NewCount)
{
	if (NewCount == 0)
	{
		CoolDownEnd.Broadcast(0.f);
	}
}

void UWaitCoolDownChange::OnActiveEffectAdded(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	FGameplayTagContainer AssetTags;
	SpecApplied.GetAllAssetTags(AssetTags);
	
	FGameplayTagContainer GrantedTags;
	SpecApplied.GetAllGrantedTags(GrantedTags);
	
	if (AssetTags.HasTagExact(CoolDownTag) || GrantedTags.HasTagExact(CoolDownTag))
	{
		FGameplayEffectQuery GameplayEffectQuery = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CoolDownTag.GetSingleTagContainer());
		TArray<float> TimesRemaining = ASC->GetActiveEffectsTimeRemaining(GameplayEffectQuery);
		if (TimesRemaining.Num() > 0)
		{
			// Get the highest time
			float TimeRemaining = 0.f;
			for (float Time : TimesRemaining)
			{
				if (Time > TimeRemaining)
				{
					TimeRemaining = Time;
				}
			}
			CoolDownStart.Broadcast(TimeRemaining);
		}
	}
}
