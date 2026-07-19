// ZYZ

#include "UI/WidgetController/AttributeMenuWidgetController.h"
#include "AbilitySystem/GASAttributeSet.h"
#include "AbilitySystem/Data/AttributeInfo.h"
#include "Player/GASPlayerState.h"
#include "GASGameplayTags.h"
#include "AbilitySystem/GASAbilitySystemComponent.h"


void UAttributeMenuWidgetController::BindCallbacksToDependencies()
{
	UGASAttributeSet* AS = CastChecked<UGASAttributeSet>(AttributeSet);
	check(AttributeInfo);
	for (auto& Pair : AS->TagsToAttributes)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Pair.Value()).AddLambda(
		[this, Pair, AS](const FOnAttributeChangeData& Data)
		{
			BroadcastAttributeInfo(Pair.Key, Pair.Value());
		}
	);
	}
	AGASPlayerState* GASPlayerState = CastChecked<AGASPlayerState>(PlayerState);
	GASPlayerState->OnAttributePointsChangedDelegate.AddLambda(
		[this](int32 Points)
		{
			AttributePointsChangedDelegate.Broadcast(Points);
		}	
	);
}

void UAttributeMenuWidgetController::BroadcastInitialValues()
{
	UGASAttributeSet* AS = Cast<UGASAttributeSet>(AttributeSet);
	check(AttributeInfo);
	for (auto& Pair : AS->TagsToAttributes)
	{
		BroadcastAttributeInfo(Pair.Key, Pair.Value());
	}
	
	// Example: 按标签从AttributeSet中获取属性值并广播给UI
	// FGASAttributeInfo StrengthInfo = AttributeInfo->FindAttributeInfoForTag(FGASGameplayTags::Get().Attributes_Primary_Strength);
	// StrengthInfo.AttributeValue = AS->GetStrength();
	// AttributeInfoDelegate.Broadcast(StrengthInfo);
	
	AGASPlayerState* GASPlayerState = CastChecked<AGASPlayerState>(PlayerState);
	AttributePointsChangedDelegate.Broadcast(GASPlayerState->GetAttributePoints());
}

void UAttributeMenuWidgetController::UpgradeAttribute(const FGameplayTag& AttributeTag)
{
	UGASAbilitySystemComponent* GASASC = CastChecked<UGASAbilitySystemComponent>(AbilitySystemComponent);
	GASASC->UpgradeAttribute(AttributeTag);
}

void UAttributeMenuWidgetController::BroadcastAttributeInfo(const FGameplayTag& AttributeTag, const FGameplayAttribute& Attribute) const
{
	FGASAttributeInfo Info = AttributeInfo->FindAttributeInfoForTag(AttributeTag);
	Info.AttributeValue = Attribute.GetNumericValue(AttributeSet);
	AttributeInfoDelegate.Broadcast(Info);
}
