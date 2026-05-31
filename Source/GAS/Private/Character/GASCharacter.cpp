// ZYZ

#include "Character/GASCharacter.h"
#include "Player/GASPlayerState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/GASAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/GASPlayerController.h"
#include "UI/HUD/GASHUD.h" 

AGASCharacter::AGASCharacter()
{
	GetCharacterMovement() -> bOrientRotationToMovement = true;
	GetCharacterMovement() -> RotationRate = FRotator(0.f, 400.f, 0.f);
	GetCharacterMovement() -> bConstrainToPlane = true;
	GetCharacterMovement() -> bSnapToPlaneAtStart = true;
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
}

void AGASCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	//Init ability actor info for the server
	InitAbilityActorInfo();
}

void AGASCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	//Init ability actor info for the client
	InitAbilityActorInfo();
}

int32 AGASCharacter::GetPlayerLevel()
{
	const AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	return GASPlayerState -> GetPlayerLevel();
}

void AGASCharacter::InitAbilityActorInfo()
{
	AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	GASPlayerState -> GetAbilitySystemComponent() -> InitAbilityActorInfo(GASPlayerState, this);
	Cast<UGASAbilitySystemComponent>(GASPlayerState -> GetAbilitySystemComponent()) -> AbilityActorInfoSet();
	AbilitySystemComponent = GASPlayerState -> GetAbilitySystemComponent();
	AttributeSet = GASPlayerState -> GetAttributeSet();
	
	AGASPlayerController* GASPlayerController = Cast<AGASPlayerController>(GetController());
	if (GASPlayerController)
	{
		AGASHUD* GASHUD = Cast<AGASHUD>(GASPlayerController -> GetHUD());
		if (GASHUD)
		{
			GASHUD -> InitOverlay(GASPlayerController, GASPlayerState, AbilitySystemComponent, AttributeSet);
		}
	}
	InitializeDefaultAttributes();
}
