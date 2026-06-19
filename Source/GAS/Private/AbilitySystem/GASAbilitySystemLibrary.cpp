// ZYZ

#include "AbilitySystem/GASAbilitySystemLibrary.h"

#include "AbilitySystemComponent.h"
#include "Game/GASGameModeBase.h"
#include "GASGameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "UI/WidgetController/GASWidgetController.h"
#include "Player/GASPlayerState.h"
#include "UI/HUD/GASHUD.h"

UOverlayWidgetController* UGASAbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		if (AGASHUD* GASHUD = Cast<AGASHUD>(PC->GetHUD()))
		{
			AGASPlayerState* PS = PC->GetPlayerState<AGASPlayerState>();
			UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
			UAttributeSet* AS = PS->GetAttributeSet();
			const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
			return GASHUD->GetOverlayWidgetController(WidgetControllerParams);
		}
	}
	return nullptr;
}

UAttributeMenuWidgetController* UGASAbilitySystemLibrary::GetAttributeMenuWidgetController(const UObject* WorldContextObject)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		if (AGASHUD* GASHUD = Cast<AGASHUD>(PC->GetHUD()))
		{
			AGASPlayerState* PS = PC->GetPlayerState<AGASPlayerState>();
			UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
			UAttributeSet* AS = PS->GetAttributeSet();
			const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
			return GASHUD->GetAttributeMenuWidgetController(WidgetControllerParams);
		}
	}
	return nullptr;
}

void UGASAbilitySystemLibrary::InitializeDefaultAttributes(const UObject* WorldContextObject, ECharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC)
{
	AActor* AvatarActor = ASC->GetAvatarActor();
	
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	FCharacterClassDefaultInfo ClassDefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);
	
	FGameplayEffectContextHandle PrimaryAttributesContextHandle = ASC->MakeEffectContext();
	PrimaryAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle PrimaryAttributesSpecHandle = ASC->MakeOutgoingSpec(ClassDefaultInfo.PrimaryAttributes, Level, PrimaryAttributesContextHandle);
	ASC->ApplyGameplayEffectSpecToSelf(*PrimaryAttributesSpecHandle.Data.Get());
	
	FGameplayEffectContextHandle SecondaryAttributesContextHandle = ASC->MakeEffectContext();
	SecondaryAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle SecondaryAttributesSpecHandle = ASC->MakeOutgoingSpec(CharacterClassInfo->SecondaryAttributes, Level, SecondaryAttributesContextHandle);
	ASC->ApplyGameplayEffectSpecToSelf(*SecondaryAttributesSpecHandle.Data.Get());
	
	FGameplayEffectContextHandle VitalAttributesContextHandle = ASC->MakeEffectContext();
	VitalAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle VitalAttributesSpecHandle = ASC->MakeOutgoingSpec(CharacterClassInfo->VitalAttributes, Level, VitalAttributesContextHandle);
	ASC->ApplyGameplayEffectSpecToSelf(*VitalAttributesSpecHandle.Data.Get());
}

void UGASAbilitySystemLibrary::GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC)
{
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	for (auto AbilityClass : CharacterClassInfo->CommonAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		ASC->GiveAbility(AbilitySpec);
	}
}

UCharacterClassInfo* UGASAbilitySystemLibrary::GetCharacterClassInfo(const UObject* WorldContextObject)
{
	AGASGameModeBase* GASGameMode = Cast<AGASGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (GASGameMode == nullptr) return nullptr;
	return GASGameMode->CharacterClassInfo;
}


bool UGASAbilitySystemLibrary::IsBlockedHit(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FGASGameplayEffectContext* GASEffectContext = static_cast<const FGASGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return GASEffectContext ? GASEffectContext->IsBlockedHit() : false;
	}
	return false;
}

bool UGASAbilitySystemLibrary::IsCriticalHit(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FGASGameplayEffectContext* GASEffectContext = static_cast<const FGASGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return GASEffectContext ? GASEffectContext->IsCriticalHit() : false;
	}
	return false;
}

void UGASAbilitySystemLibrary::SetIsBlockedHit(FGameplayEffectContextHandle& EffectContextHandle, bool bInIsBlockedHit)
{
	if (FGASGameplayEffectContext* GASEffectContext = static_cast<FGASGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		GASEffectContext->SetIsBlockedHit(bInIsBlockedHit);
	}
}

void UGASAbilitySystemLibrary::SetIsCriticalHit(FGameplayEffectContextHandle& EffectContextHandle, bool bInIsCriticalHit)
{
	if (FGASGameplayEffectContext* GASEffectContext = static_cast<FGASGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		GASEffectContext->SetIsCriticalHit(bInIsCriticalHit);
	}
}

float UGASAbilitySystemLibrary::ApplyValueVariance(const float Value, const float VariancePercent)
{
	if (Value <= 0.f || VariancePercent <= 0.f)
	{
		return Value;
	}

	const float ClampedVariance = FMath::Clamp(VariancePercent, 0.f, 1.f);
	const float MinMultiplier = 1.f - ClampedVariance;
	const float MaxMultiplier = 1.f + ClampedVariance;

	return Value * FMath::FRandRange(MinMultiplier, MaxMultiplier);
}
