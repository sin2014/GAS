// ZYZ

#include "AbilitySystem/Data/AbilityInfo.h"
#include "GAS/GASLogChannels.h"

FGASAbilityInfo UAbilityInfo::FindAbilityInfoForTag(const FGameplayTag& AbilityTag, bool bLogNotFound) const
{
	for (const FGASAbilityInfo& Info : AbilityInfomation)
	{
		if (Info.AbilityTag.MatchesTagExact(AbilityTag))
		{
			return Info;
		}
	}
	
	if (bLogNotFound)
	{
		UE_LOG(LogGAS, Error, TEXT("Can't find info for AbilityTag [%s] on AbilityInfo [%s]."), *AbilityTag.ToString(), *GetNameSafe(this));
	}
	
	return FGASAbilityInfo();
}
