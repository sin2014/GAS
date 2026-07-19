// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LevelUpInfo.generated.h"

USTRUCT(BlueprintType)
struct FGASLevelUpInfo
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly)
	int32 LevelUpRequirement = 0;
	
	UPROPERTY(EditDefaultsOnly)
	int32 AttributePointsAward = 1;
	
	UPROPERTY(EditDefaultsOnly)
	int32 SpellPointsAward = 1;
};

/**
 * 
 */
UCLASS()
class GAS_API ULevelUpInfo : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly)
	TArray<FGASLevelUpInfo> LevelUpInformation;
	
	int32 FindLevelForXP(int32 XP) const;
};
