// ZYZ

#include "AbilitySystem/Abilities/GASMeleeAttack.h"

bool UGASMeleeAttack::PickRandomTaggedMontage(const TArray<FTaggedMontage>& TaggedMontages, FTaggedMontage& OutTaggedMontage) const
{
	OutTaggedMontage = FTaggedMontage();

	TArray<FTaggedMontage> ValidTaggedMontages = TaggedMontages;
	ValidTaggedMontages.RemoveAll([](const FTaggedMontage& TaggedMontage)
	{
		return !IsValid(TaggedMontage.Montage);
	});

	if (ValidTaggedMontages.Num() == 0)
	{
		return false;
	}

	OutTaggedMontage = ValidTaggedMontages[FMath::RandRange(0, ValidTaggedMontages.Num() - 1)];
	return true;
}
