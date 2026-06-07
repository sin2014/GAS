// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GASProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class GAS_API AGASProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	AGASProjectile();
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

protected:
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, 
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Sphere;
};
