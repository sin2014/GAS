// Copyright Epic Games, Inc. All Rights Reserved.

#include "Player/MMPlayerController.h"

#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	// 创建按轴取反的输入修饰器，用于 A/S/Left/Down。
	UInputModifierNegate* CreateNegateModifier(UObject* Outer)
	{
		UInputModifierNegate* Modifier = NewObject<UInputModifierNegate>(Outer);
		Modifier->bX = true;
		Modifier->bY = true;
		Modifier->bZ = true;
		return Modifier;
	}

	// 创建 X/Y 轴交换修饰器，用于把一维按键输入映射到 2D Move 的 Y 轴。
	UInputModifierSwizzleAxis* CreateSwizzleToYAxisModifier(UObject* Outer)
	{
		UInputModifierSwizzleAxis* Modifier = NewObject<UInputModifierSwizzleAxis>(Outer);
		Modifier->Order = EInputAxisSwizzle::YXZ;
		return Modifier;
	}

	// 向映射上下文添加一个移动按键，并按需附加取反和轴交换修饰器。
	void MapMoveKey(UInputMappingContext* MappingContext, const UInputAction* Action, const FKey Key, const bool bNegate, const bool bSwizzleToY)
	{
		FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(Action, Key);
		if (bNegate)
		{
			Mapping.Modifiers.Add(CreateNegateModifier(MappingContext));
		}
		if (bSwizzleToY)
		{
			Mapping.Modifiers.Add(CreateSwizzleToYAxisModifier(MappingContext));
		}
	}
}

AMMPlayerController::AMMPlayerController()
{
	// 首个可运行版本不需要鼠标光标。
	bShowMouseCursor = false;
}

void AMMPlayerController::BeginPlay()
{
	Super::BeginPlay();

	EnsureDefaultInputAssets();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			InputSubsystem->AddMappingContext(MovementMappingContext, 0);
		}
	}
}

// 绑定 Enhanced Input 回调；输入资产可能尚未创建，因此先确保默认资产存在。
void AMMPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	EnsureDefaultInputAssets();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMMPlayerController::HandleMoveInput);
	}
}

// 运行时创建最小输入资产，避免当前阶段必须先手工制作 uasset。
void AMMPlayerController::EnsureDefaultInputAssets()
{
	if (MoveAction && MovementMappingContext)
	{
		return;
	}

	MoveAction = NewObject<UInputAction>(this, TEXT("IA_MM_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	MoveAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;

	MovementMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_MM_KeyboardMovement"));

	MapMoveKey(MovementMappingContext, MoveAction, EKeys::D, false, false);
	MapMoveKey(MovementMappingContext, MoveAction, EKeys::Right, false, false);
	MapMoveKey(MovementMappingContext, MoveAction, EKeys::A, true, false);
	MapMoveKey(MovementMappingContext, MoveAction, EKeys::Left, true, false);
	MapMoveKey(MovementMappingContext, MoveAction, EKeys::W, false, true);
	MapMoveKey(MovementMappingContext, MoveAction, EKeys::Up, false, true);
	MapMoveKey(MovementMappingContext, MoveAction, EKeys::S, true, true);
	MapMoveKey(MovementMappingContext, MoveAction, EKeys::Down, true, true);
}

// 处理 2D Move 输入，并把它压成固定俯视视角下的世界四向移动。
void AMMPlayerController::HandleMoveInput(const FInputActionValue& Value)
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	const FVector MovementDirection = GetFourWayMovementDirection(Value.Get<FVector2D>());
	if (!MovementDirection.IsNearlyZero())
	{
		// 使用世界坐标方向推动角色，保证固定俯视视角下的四向移动稳定。
		ControlledPawn->AddMovementInput(MovementDirection, 1.0f);
	}
}

// 保留原先的“纵向优先”规则，避免同时按两个方向时走斜线。
FVector AMMPlayerController::GetFourWayMovementDirection(const FVector2D& MovementValue) const
{
	const int32 ForwardAxis = (MovementValue.Y > 0.5f ? 1 : 0) - (MovementValue.Y < -0.5f ? 1 : 0);
	const int32 RightAxis = (MovementValue.X > 0.5f ? 1 : 0) - (MovementValue.X < -0.5f ? 1 : 0);


	if (ForwardAxis != 0)
	{
		// UE 默认 X 轴作为世界前后方向。
		return FVector(static_cast<float>(ForwardAxis), 0.0f, 0.0f);
	}

	if (RightAxis != 0)
	{
		// UE 默认 Y 轴作为世界左右方向。
		return FVector(0.0f, static_cast<float>(RightAxis), 0.0f);
	}

	return FVector::ZeroVector;
}
