// ZYZ

#include "AbilitySystem/Abilities/GASSummonAbility.h"

TArray<FVector> UGASSummonAbility::GetSpawnLocation()
{
	const FVector Forward = GetAvatarActorFromActorInfo()->GetActorForwardVector();
	const FVector Location = GetAvatarActorFromActorInfo()->GetActorLocation();
	
	const FVector LSpread = Forward.RotateAngleAxis(-MinionSpawnSpread / 2.f, FVector::UpVector);
	const float DeltaSpread = NumMinions > 1 ? MinionSpawnSpread / (NumMinions - 1) : 0.f;
	
	TArray<FVector> SpawnLocations;
	for (int i = 0; i < NumMinions; i++)
	{
		const FVector Direction = LSpread.RotateAngleAxis(DeltaSpread * i, FVector::UpVector);
		FVector SpawnLocation = Location + Direction * FMath::FRandRange(MinMinionSpawnDistance, MaxMinionSpawnDistance);
		
		FHitResult Hit;
		GetWorld()->LineTraceSingleByChannel(Hit, SpawnLocation + FVector(0.f, 0.f, 500.f), SpawnLocation - FVector(0.f, 0.f, 500.f), ECollisionChannel::ECC_Visibility);
		if (Hit.bBlockingHit)
		{
			SpawnLocation = Hit.ImpactPoint;
		}
		SpawnLocations.Add(SpawnLocation);
	}
	return SpawnLocations;
}

TSubclassOf<APawn> UGASSummonAbility::GetRandomMinionClass() const
{
	const int32 Selection = FMath::RandRange(0, MinionClasses.Num() - 1);
	return MinionClasses[Selection];
}
