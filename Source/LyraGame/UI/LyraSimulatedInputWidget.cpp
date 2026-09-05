// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/LyraSimulatedInputWidget.h"
#include "EnhancedInputSubsystems.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "LyraLogChannels.h"
#include "InputKeyEventArgs.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraSimulatedInputWidget)

#define LOCTEXT_NAMESPACE "LyraSimulatedInputWidget"

// 构造会消费指针输入的增强输入模拟控件。
ULyraSimulatedInputWidget::ULyraSimulatedInputWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetConsumePointerInput(true);
}

#if WITH_EDITOR
// 在 UMG 设计器面板中把控件归入 Input 分类。
const FText ULyraSimulatedInputWidget::GetPaletteCategory()
{
	return LOCTEXT("PalleteCategory", "Input");
}
#endif // WITH_EDITOR

// 构造时解析当前模拟按键，并监听增强输入映射重建以跟随重绑定。
void ULyraSimulatedInputWidget::NativeConstruct()
{
	// 构造时解析初始模拟按键，并监听控制映射重建以跟随用户重绑定。
	// Find initial key, then listen for any changes to control mappings
	QueryKeyToSimulate();

	if (UEnhancedInputLocalPlayerSubsystem* System = GetEnhancedInputSubsystem())
	{
		System->ControlMappingsRebuiltDelegate.AddUniqueDynamic(this, &ULyraSimulatedInputWidget::OnControlMappingsRebuilt);
	}
	
	Super::NativeConstruct();
}

// 销毁时解除控制映射重建委托，防止子系统回调失效控件。
void ULyraSimulatedInputWidget::NativeDestruct()
{
	if (UEnhancedInputLocalPlayerSubsystem* System = GetEnhancedInputSubsystem())
	{
		System->ControlMappingsRebuiltDelegate.RemoveAll(this);
	}

	Super::NativeDestruct();
}

// 触摸结束时清空 Player Input 中的按下状态，再交由基类处理事件。
FReply ULyraSimulatedInputWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	FlushSimulatedInput();
	
	return Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
}

// 返回所属本地玩家的增强输入子系统；玩家上下文不完整时返回空指针。
UEnhancedInputLocalPlayerSubsystem* ULyraSimulatedInputWidget::GetEnhancedInputSubsystem() const
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (ULocalPlayer* LP = GetOwningLocalPlayer())
		{
			return LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		}
	}
	return nullptr;
}

// 从增强输入子系统取得当前 UEnhancedPlayerInput；子系统不可用时返回空指针。
UEnhancedPlayerInput* ULyraSimulatedInputWidget::GetPlayerInput() const
{
	if (UEnhancedInputLocalPlayerSubsystem* System = GetEnhancedInputSubsystem())
	{
		return System->GetPlayerInput();
	}
	return nullptr;
}

// 有关联 Action 时直接注入向量值，否则向解析出的后备键或配对轴键发送模拟输入事件。
void ULyraSimulatedInputWidget::InputKeyValue(const FVector& Value)
{
	const APlayerController* PC = GetOwningPlayer();
	const FPlatformUserId UserId = PC ? PC->GetPlatformUserId() : PLATFORMUSERID_NONE;
	// 关联 Action 存在时优先通过 Enhanced Input 子系统直接注入 Action 值。
	// If we have an associated input action then we can use it
	if (AssociatedAction)
	{
		if (UEnhancedInputLocalPlayerSubsystem* System = GetEnhancedInputSubsystem())
		{
			// 本控件提供的值已是最终模拟输入，因此传入空 Modifier 和 Trigger 数组以满足函数签名但不重复处理。
			// We don't want to apply any modifiers or triggers to this action, but they are required for the function signature
			TArray<UInputModifier*> Modifiers;
			TArray<UInputTrigger*> Triggers;
			System->InjectInputVectorForAction(AssociatedAction, Value, Modifiers, Triggers);
		}
	}
	// 没有关联 Action 时，改为通过当前解析出的后备按键向 Player Input 注入。
	// In case there is no associated input action, we can attempt to simulate input on the fallback key
	else if (UEnhancedPlayerInput* Input = GetPlayerInput())
	{
		const FInputDeviceId DeviceToSimulate = IPlatformInputDeviceMapper::Get().GetPrimaryInputDeviceForUser(UserId);
		if(KeyToSimulate.IsValid())
		{
			const float DeltaTime = GetWorld()->GetDeltaSeconds();
			auto SimulateKeyPress = [Input, DeltaTime, DeviceToSimulate](const FKey& KeyToSim, const float Value, const EInputEvent Event)
			{
				FInputKeyEventArgs Args = FInputKeyEventArgs::CreateSimulated(
					KeyToSim,
					Event,
					Value,
					KeyToSim.IsAnalog() ? 1 : 0,
					DeviceToSimulate);

				Args.DeltaTime = DeltaTime;
			
				Input->InputKey(Args);
			};
			
			// 对 Mouse2D 这类由 MouseX 和 MouseY 组成的配对根按键，必须分别注入各分量按键，
			// 才能像消息处理器和 Viewport Client 一样在 UPlayerInput 键状态表中正确累加事件。
			// For keys which are the "root" of the key pair (such as Mouse2D
			// being made up of the MouseX and MouseY keys) we should call InputKey for each key in the pair,
			// not the paired key itself. This is so that the events accumulate correctly in
			// the key state map of UPlayerInput. All input events
			// from the message handler and viewport client work this way, so when we simulate key inputs, we should
			// do so as well.
			if (const EKeys::FPairedKeyDetails* PairDetails = EKeys::GetPairedKeyDetails(KeyToSimulate))
			{
				SimulateKeyPress(PairDetails->XKeyDetails->GetKey(), Value.X, IE_Axis);
				SimulateKeyPress(PairDetails->YKeyDetails->GetKey(), Value.Y, IE_Axis);
			}
			else
			{
				SimulateKeyPress(KeyToSimulate, Value.X, IE_Pressed);
			}
		}
	}
	else
	{
		UE_LOG(LogLyra, Error, TEXT("'%s' is attempting to simulate input but has no player input!"), *GetNameSafe(this));
	}
}

// 将二维输入扩展为三维向量并复用统一模拟输入路径。
void ULyraSimulatedInputWidget::InputKeyValue2D(const FVector2D& Value)
{
	InputKeyValue(FVector(Value.X, Value.Y, 0.0));
}

// 清空增强 Player Input 中全部按下键状态，避免触摸结束后保持输入。
void ULyraSimulatedInputWidget::FlushSimulatedInput()
{
	if (UEnhancedPlayerInput* Input = GetPlayerInput())
	{
		Input->FlushPressedKeys();
	}
}

// 取关联 Action 当前映射的首个有效按键；没有映射时使用配置的后备键。
void ULyraSimulatedInputWidget::QueryKeyToSimulate()
{
	if (UEnhancedInputLocalPlayerSubsystem* System = GetEnhancedInputSubsystem())
	{
		TArray<FKey> Keys = System->QueryKeysMappedToAction(AssociatedAction);
		if(!Keys.IsEmpty() && Keys[0].IsValid())
		{
			KeyToSimulate = Keys[0];
		}
		else
		{
			KeyToSimulate = FallbackBindingKey;
		}
	}
}

// 控制映射重建后重新解析需要模拟的按键。
void ULyraSimulatedInputWidget::OnControlMappingsRebuilt()
{
	QueryKeyToSimulate();
}

#undef LOCTEXT_NAMESPACE
