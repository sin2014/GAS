// ZYZ

#include "Player/GASPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/GASAbilitySystemComponent.h"
#include "Components/SplineComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GASGameplayTags.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Input/GASInputComponent.h"
#include "Interaction/EnemyInterface.h"

AGASPlayerController::AGASPlayerController()
{
	bReplicates = true;    //标记为可复制实体
	
	Spline = CreateDefaultSubobject<USplineComponent>(TEXT("Spline"));
}

void AGASPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	
	CursorTrace();
	
	AutoRun();
}

void AGASPlayerController::AutoRun()
{
	if (!bAutoRunning) return;
	if (APawn* ControlledPawn = GetPawn())
	{
		const FVector& LocationOnSpline = Spline->FindLocationClosestToWorldLocation(ControlledPawn->GetActorLocation(), ESplineCoordinateSpace::World);
		const FVector& Direction = Spline->FindDirectionClosestToWorldLocation(LocationOnSpline, ESplineCoordinateSpace::World);
		ControlledPawn->AddMovementInput(Direction);
		
		const float DistanceToDestination = (LocationOnSpline - CahedDestination).Length();
		if (DistanceToDestination < AutoRunAcceptanceRadius)
		{
			bAutoRunning = false;
		}
	}
}

void AGASPlayerController::CursorTrace()
{
	FHitResult CursorHit;
	GetHitResultUnderCursor(ECC_Visibility, false, CursorHit);
	if (!CursorHit.bBlockingHit)
	{
		return;
	}
	
	LastActor = ThisActor;
	ThisActor = Cast<IEnemyInterface>(CursorHit.GetActor());
	if (LastActor == nullptr)
	{
		if (ThisActor != nullptr)
		{
			ThisActor->HighlightActor();
		}
		else
		{
			//both null, do nothing.
		}
	}
	else
	{
		if (ThisActor == nullptr)
		{
			LastActor->UnHighlightActor();
		}
		else
		{
			if (LastActor != ThisActor)
			{
				LastActor->UnHighlightActor();
				ThisActor->HighlightActor();
			}
			else
			{
				//both valid, do nothing.
			}
		}
	}
}

void AGASPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (InputTag.MatchesTagExact(FGASGameplayTags::Get().InputTag_LMB))
	{
		bTargeting = ThisActor ? true : false;
		bAutoRunning = false;
	}
}

void AGASPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (!InputTag.MatchesTagExact(FGASGameplayTags::Get().InputTag_LMB))
	{
		if (GetASC())
		{
			GetASC()->AbilityInputTagReleased(InputTag);
		}
		return;
	}
	
	if (bTargeting)
	{
		if (GetASC())
		{
			GetASC()->AbilityInputTagReleased(InputTag);
		}
	}
	else
	{
	    APawn* ControlledPawn = GetPawn();
		if (FollowTime <= ShortPressThreshold && ControlledPawn)
		{
			if (UNavigationPath* NavigationPath = UNavigationSystemV1::FindPathToLocationSynchronously(this, ControlledPawn->GetActorLocation(), CahedDestination))
			{
				Spline->ClearSplinePoints();
				for (const FVector& PointLoc : NavigationPath->PathPoints)
				{
					Spline->AddSplinePoint(PointLoc, ESplineCoordinateSpace::World);
					DrawDebugSphere(GetWorld(), PointLoc, 8.f, 8, FColor::Red, false, 5.f);
				}
				CahedDestination = NavigationPath->PathPoints[NavigationPath->PathPoints.Num() - 1];
				bAutoRunning = true;
			}
		}
		FollowTime = 0.f;
		bTargeting = false;
	}
}

void AGASPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	if (!InputTag.MatchesTagExact(FGASGameplayTags::Get().InputTag_LMB))
	{
		if (GetASC())
		{
			GetASC()->AbilityInputTagHeld(InputTag);
		}
		return;
	}
	
	if (bTargeting)
	{
		if (GetASC())
		{
			GetASC()->AbilityInputTagHeld(InputTag);
		}
	}
	else
	{ 
		FollowTime += GetWorld()->GetDeltaSeconds();
		
		FHitResult Hit;
		if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			CahedDestination = Hit.ImpactPoint;
		}
		
		if (APawn* ControllerPawn = GetPawn())
		{
			const FVector WorldDirection = (CahedDestination - ControllerPawn->GetActorLocation()).GetSafeNormal();
			ControllerPawn->AddMovementInput(WorldDirection);
		}
	}
}

UGASAbilitySystemComponent* AGASPlayerController::GetASC()
{
	if (GASAbilitySystemComponent == nullptr)
	{
		GASAbilitySystemComponent  = Cast<UGASAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
	}
	return GASAbilitySystemComponent;
}

void AGASPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	check(GasContext);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(GasContext, 0);
	}
	
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	
	FInputModeGameAndUI InputModeData;
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputModeData.SetHideCursorDuringCapture(false);
	SetInputMode(InputModeData);
}

void AGASPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	UGASInputComponent* GASInputComponent = CastChecked<UGASInputComponent>(InputComponent);
	GASInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGASPlayerController::Move);
	GASInputComponent->BindAbilityActions(InputConfig, this, &AGASPlayerController::AbilityInputTagPressed, &AGASPlayerController::AbilityInputTagReleased, &AGASPlayerController::AbilityInputTagHeld);
}

void AGASPlayerController::Move(const FInputActionValue& InputActionValue)
{
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	const FRotator Rotation =GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		ControlledPawn ->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn ->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}
