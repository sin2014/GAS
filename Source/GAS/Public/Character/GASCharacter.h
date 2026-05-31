// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "Character/GASCharacterBase.h"
#include "GASCharacter.generated.h"

/**
 * 
 */
UCLASS()
class GAS_API AGASCharacter : public AGASCharacterBase
{
	GENERATED_BODY()
	
public:
	AGASCharacter();
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	
	/** Combat Interface */
	virtual int32 GetPlayerLevel() override;
	
private:
	virtual void InitAbilityActorInfo() override;
};
