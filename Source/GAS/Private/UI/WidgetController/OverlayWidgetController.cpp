// ZYZ

#include "UI/WidgetController/OverlayWidgetController.h"
#include "AbilitySystem/GASAbilitySystemComponent.h"
#include "AbilitySystem/GASAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"

//广播初始值
void UOverlayWidgetController::BroadcastInitialValues()
{
	const UGASAttributeSet* GASAttributeSet = CastChecked<UGASAttributeSet>(AttributeSet);
	
	OnHealthChanged.Broadcast(GASAttributeSet->GetHealth());
	OnMaxHealthChanged.Broadcast(GASAttributeSet->GetMaxHealth());
	OnManaChanged.Broadcast(GASAttributeSet->GetMana());
	OnMaxManaChanged.Broadcast(GASAttributeSet->GetMaxMana());
}

void UOverlayWidgetController::BindCallbacksToDependencies()
{
	const UGASAttributeSet* GASAttributeSet = CastChecked<UGASAttributeSet>(AttributeSet);
	
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GASAttributeSet->GetHealthAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnHealthChanged.Broadcast(Data.NewValue);
		}
	);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GASAttributeSet->GetMaxHealthAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnMaxHealthChanged.Broadcast(Data.NewValue);
		}
	);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GASAttributeSet->GetManaAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnManaChanged.Broadcast(Data.NewValue);
		}
	);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(GASAttributeSet->GetMaxManaAttribute()).AddLambda(
		[this](const FOnAttributeChangeData& Data)
		{
			OnMaxManaChanged.Broadcast(Data.NewValue);
		}
	);
	
	if (UGASAbilitySystemComponent* GASASC = Cast<UGASAbilitySystemComponent>(AbilitySystemComponent))
	{
		if (GASASC->bStartupAbilitiesGiven)
		{
			OnInitializeStartupAbilities(GASASC);
		}
		else
		{
			GASASC->AbilitiesGivenDelegate.AddUObject(this, &UOverlayWidgetController::OnInitializeStartupAbilities);
		}
        
        GASASC->EffectAssetTags.AddLambda(
        	[this](const FGameplayTagContainer& AssetTags)
        	{
        		for (const FGameplayTag& Tag : AssetTags)
        		{
        			FGameplayTag MessageTag = FGameplayTag::RequestGameplayTag(FName("Message"));
        			if (Tag.MatchesTag(MessageTag))
        			{
        				const FUIWidgetRow* Row = GetDataTableRowByTag<FUIWidgetRow>(MessageWidgetDataTable, Tag);
        				MessageWidgetRowDelegate.Broadcast(*Row);
        			}
        		}
        	}
        );
	}
}

void UOverlayWidgetController::OnInitializeStartupAbilities(UGASAbilitySystemComponent* GASASC)
{
	if (!GASASC->bStartupAbilitiesGiven) return;
	
	FForEachAbility BroadcastDelegate;
	BroadcastDelegate.BindLambda([this, GASASC](const FGameplayAbilitySpec& AbilitySpec)
	{
		FGASAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(GASASC->GetAbilityTagFromSpec(AbilitySpec));
		Info.InputTag = GASASC->GetInputTagFromSpec(AbilitySpec);
		AbilityInfoDelegate.Broadcast(Info);
	});
	GASASC->ForEachAbility(BroadcastDelegate);
}
