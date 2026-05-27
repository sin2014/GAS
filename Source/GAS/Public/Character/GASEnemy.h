// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "Character/GASCharacterBase.h"
#include "Interaction/EnemyInterface.h"
#include "GASEnemy.generated.h"

/**
 * 
 */
UCLASS()
class GAS_API AGASEnemy : public AGASCharacterBase, public IEnemyInterface
{
	GENERATED_BODY()

public:
	AGASEnemy();
	virtual void HighlightActor() override;
	virtual void UnHighlightActor() override;
	
protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;
};
