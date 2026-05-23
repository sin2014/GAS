// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GASEffectActor.generated.h"

class UGameplayEffect;

UCLASS()
class GAS_API AGASEffectActor : public AActor
{
	GENERATED_BODY()
	
public:	
	AGASEffectActor();

protected:
	virtual void BeginPlay() override;
	
	UFUNCTION(BlueprintCallable)
	void ApplyEffectToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> GameplayEffectClass);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effect")
	TSubclassOf<UGameplayEffect> InstantGameEffectClass;
};
