// ZYZ

#include "UI/WidgetController/AttributeMenuWidgetController.h"
#include "AbilitySystem/GASAttributeSet.h"
#include "AbilitySystem/Data/AttributeInfo.h"
#include "GASGameplayTags.h"

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
}

void UAttributeMenuWidgetController::BroadcastAttributeInfo(const FGameplayTag& AttributeTag, const FGameplayAttribute& Attribute) const
{
	FGASAttributeInfo Info = AttributeInfo->FindAttributeInfoForTag(AttributeTag);
	Info.AttributeValue = Attribute.GetNumericValue(AttributeSet);
	AttributeInfoDelegate.Broadcast(Info);
}
