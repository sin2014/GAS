// ZYZ

#include "AbilitySystem/Data/AttributeInfo.h"
#include "GAS/GASLogChannels.h"

FGASAttributeInfo UAttributeInfo::FindAttributeInfoForTag(const FGameplayTag& AttributeTag, bool bLogNotFound) const
{
	for (const FGASAttributeInfo& Info : AttributeInfomation)
	{
		if (Info.AttributeTag.MatchesTagExact(AttributeTag))
		{
			return Info;
		}
	}
	
	if (bLogNotFound)
	{
		UE_LOG(LogGAS, Error, TEXT("Can't find info for AttributeTag [%s] on AttributeInfo [%s]."), *AttributeTag.ToString(), *GetNameSafe(this));
	}
	
	return FGASAttributeInfo();
}
