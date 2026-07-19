// ZYZ

#include "Character/GASCharacter.h"
#include "Player/GASPlayerState.h"
#include "AbilitySystemComponent.h"
#include "NiagaraComponent.h"
#include "AbilitySystem/GASAbilitySystemComponent.h"
#include "AbilitySystem/Data/LevelUpInfo.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Player/GASPlayerController.h"
#include "UI/HUD/GASHUD.h" 

AGASCharacter::AGASCharacter()
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>("CameraBoom");
	CameraBoom -> SetupAttachment(GetRootComponent());
	CameraBoom -> SetUsingAbsoluteRotation(true);
	CameraBoom -> bDoCollisionTest = false;
	
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>("TopDownCameraComponent");
	TopDownCameraComponent -> SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent -> bUsePawnControlRotation = false;
	
	LevelUpNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>("LevelUpNiagaraComponent");
	LevelUpNiagaraComponent -> SetupAttachment(GetRootComponent());
	LevelUpNiagaraComponent -> SetAutoActivate(false);
	
	GetCharacterMovement() -> bOrientRotationToMovement = true;
	GetCharacterMovement() -> RotationRate = FRotator(0.f, 400.f, 0.f);
	GetCharacterMovement() -> bConstrainToPlane = true;
	GetCharacterMovement() -> bSnapToPlaneAtStart = true;
	
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	
	CharacterClass = ECharacterClass::Elementalist;
}

void AGASCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	//Init ability actor info for the server
	InitAbilityActorInfo();
	AddCharacterAbilities();
}

void AGASCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	
	//Init ability actor info for the client
	InitAbilityActorInfo();
}

int32 AGASCharacter::FindLevelForXP_Implementation(int32 InXP) const
{
	AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	return GASPlayerState->LevelUpInfo->FindLevelForXP(InXP);
}

int32 AGASCharacter::GetXP_Implementation()
{
	AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	return GASPlayerState->GetPlayerXP();
}

int32 AGASCharacter::GetAttributePointsReward_Implementation(int32 Level) const
{
	AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	return GASPlayerState->LevelUpInfo->LevelUpInformation[Level].AttributePointsAward;
}

int32 AGASCharacter::GetSpellPointsReward_Implementation(int32 Level) const
{
	AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	return GASPlayerState->LevelUpInfo->LevelUpInformation[Level].SpellPointsAward;
}

void AGASCharacter::AddToXP_Implementation(int32 InXP)
{
	AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	GASPlayerState -> AddXP(InXP);
}

void AGASCharacter::AddToLevel_Implementation(int32 InLevel)
{
	AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	GASPlayerState -> AddLevel(InLevel);
}

void AGASCharacter::AddToAttributePoints_Implementation(int32 InAttributePoints)
{
	AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	GASPlayerState -> AddAttributePoints(InAttributePoints);
}

void AGASCharacter::AddToSpellPoints_Implementation(int32 InSpellPoints)
{
	AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	GASPlayerState -> AddSpellPoints(InSpellPoints);
}

int32 AGASCharacter::GetAttributePoints_Implementation() const
{
	AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	return GASPlayerState -> GetAttributePoints();
}

int32 AGASCharacter::GetSpellPoints_Implementation() const
{
	AGASPlayerState* GASPlayerState = GetPlayerState<AGASPlayerState>();
	check(GASPlayerState);
	return GASPlayerState -> GetSpellPoints();
}

void AGASCharacter::LevelUp_Implementation(int32 InLevel)
{
	MulticastLevelUpParticles();
}

void AGASCharacter::MulticastLevelUpParticles_Implementation() const
{
	if (IsValid(LevelUpNiagaraComponent))
	{
		const FVector CameraLocation = TopDownCameraComponent -> GetComponentLocation();
		const FVector NiagaraComponentLocation = LevelUpNiagaraComponent -> GetComponentLocation();
		const FRotator ToCameraRotation = (CameraLocation - NiagaraComponentLocation).Rotation();
		LevelUpNiagaraComponent->SetWorldRotation(ToCameraRotation);
		LevelUpNiagaraComponent->Activate(true);
	}
}

int32 AGASCharacter::GetPlayerLevel_Implementation()
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
