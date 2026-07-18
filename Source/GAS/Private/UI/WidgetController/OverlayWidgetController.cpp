// ZYZ

#include "UI/WidgetController/OverlayWidgetController.h"
#include "AbilitySystem/GASAbilitySystemComponent.h"
#include "AbilitySystem/GASAttributeSet.h"
#include "AbilitySystem/Data/AbilityInfo.h"
#include "AbilitySystem/Data/LevelUpInfo.h"
#include "Player/GASPlayerState.h"

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
	AGASPlayerState* GASPlayerState = CastChecked<AGASPlayerState>(PlayerState);
	GASPlayerState->OnLevelChangedDelegate.AddLambda(
		[this](int32 NewLevel)
		{
			OnPlayerLevelChangedDelegate.Broadcast(NewLevel);
		}
	);
	GASPlayerState->OnXPChangedDelegate.AddUObject(this, &UOverlayWidgetController::OnXPChanged);
	
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

void UOverlayWidgetController::OnLevelChanged(int32 NewLevel) const
{
}

void UOverlayWidgetController::OnXPChanged(int32 NewXP) const
{
	const AGASPlayerState* GASPlayerState = CastChecked<AGASPlayerState>(PlayerState);
	const ULevelUpInfo* LevelUpInfo = GASPlayerState->LevelUpInfo;
	checkf(LevelUpInfo, TEXT("Unabled to find LevelUpInfo. Please fill out GASPlayerState Blueprint."))
	
	const int32 Level = LevelUpInfo->FindLevelForXP(NewXP);
	const int32 MaxLevel = LevelUpInfo->LevelUpInformation.Num();
	
	if (Level <= MaxLevel && Level > 0)
	{
		const int32 CurrentLevelUpRequirement = LevelUpInfo->LevelUpInformation[Level].LevelUpRequirement;
		const int32 PreviousLevelUpRequirement = LevelUpInfo->LevelUpInformation[Level - 1].LevelUpRequirement;
		
		const int32 DeltaLevelUpRequirement = CurrentLevelUpRequirement - PreviousLevelUpRequirement;
		const int32 XPForThisLevel = NewXP - PreviousLevelUpRequirement;
		
		const float XPBarPercent = static_cast<float>(XPForThisLevel) / static_cast<float>(DeltaLevelUpRequirement);
		OnXPPercentChangedDelegate.Broadcast(XPBarPercent);
	}
}
