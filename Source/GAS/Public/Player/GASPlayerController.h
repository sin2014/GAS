// ZYZ

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "GASPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UGASInputConfig;
class UGASAbilitySystemComponent;
class USplineComponent;
struct FInputActionValue;
class IEnemyInterface;
class UDamageTextComponent;

/**
 * 
 */
UCLASS()
class GAS_API AGASPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	AGASPlayerController();
	virtual void PlayerTick(float DeltaTime) override;
	
	UFUNCTION(Client, Reliable)
	void ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter);
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	
private:
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> GasContext;
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;
	
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ShiftAction;
	
	void Move(const FInputActionValue& InputActionValue);
	void ShiftPressed(){ bShiftKeyDown = true; };
	void ShiftReleased() { bShiftKeyDown = false; };
	bool bShiftKeyDown = false;
	
	void CursorTrace();
	FHitResult CursorHit;
	IEnemyInterface* LastActor;
	IEnemyInterface* ThisActor;
	
	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);
	void AbilityInputTagHeld(FGameplayTag InputTag);
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UGASInputConfig> InputConfig;
	
	UPROPERTY()
	TObjectPtr<UGASAbilitySystemComponent> GASAbilitySystemComponent;
	
	UGASAbilitySystemComponent* GetASC();
	
	FVector CahedDestination = FVector::ZeroVector;
	float FollowTime = 0.f;
	float ShortPressThreshold = 0.5f;
	bool bAutoRunning = false;
	bool bTargeting = false;
	
	UPROPERTY(EditDefaultsOnly)
	float AutoRunAcceptanceRadius = 50.f;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USplineComponent> Spline;
	
	void AutoRun();
	
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent> DamageTextComponentClass;
};
