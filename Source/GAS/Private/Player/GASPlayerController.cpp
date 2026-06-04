// ZYZ

#include "Player/GASPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/GASAbilitySystemComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Input/GASInputComponent.h"
#include "Interaction/EnemyInterface.h"

AGASPlayerController::AGASPlayerController()
{
	bReplicates = true;    //标记为可复制实体
}

void AGASPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	
	CursorTrace();
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
}

void AGASPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagReleased(InputTag);
}

void AGASPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagPressed(InputTag);
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
