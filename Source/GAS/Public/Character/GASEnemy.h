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
	
	/** Enemy Interface */
	virtual void HighlightActor() override;
	virtual void UnHighlightActor() override;
	
	/** Combat Interface */
	virtual int32 GetPlayerLevel() override;
	
protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults", meta = (AllowPrivateAccess = true))
	int32 Level = 1;
};
