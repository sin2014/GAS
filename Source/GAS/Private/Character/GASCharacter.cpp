// ZYZ

#include "Character/GASCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"

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
