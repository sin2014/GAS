// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraCameraMode.h"

#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "GameFramework/Character.h"
#include "LyraCameraComponent.h"
#include "LyraPlayerCameraManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCameraMode)


//////////////////////////////////////////////////////////////////////////
// FLyraCameraModeView
//////////////////////////////////////////////////////////////////////////
// 将相机位置、旋转和控制旋转初始化为零值，并使用项目默认 FOV。
FLyraCameraModeView::FLyraCameraModeView()
	: Location(ForceInit)
	, Rotation(ForceInit)
	, ControlRotation(ForceInit)
	, FieldOfView(LYRA_CAMERA_DEFAULT_FOV)
{
}

// 按 OtherWeight 混合位置、旋转、控制旋转和 FOV，旋转使用归一化最短路径插值。
void FLyraCameraModeView::Blend(const FLyraCameraModeView& Other, float OtherWeight)
{
	if (OtherWeight <= 0.0f)
	{
		return;
	}
	else if (OtherWeight >= 1.0f)
	{
		*this = Other;
		return;
	}

	Location = FMath::Lerp(Location, Other.Location, OtherWeight);

	const FRotator DeltaRotation = (Other.Rotation - Rotation).GetNormalized();
	Rotation = Rotation + (OtherWeight * DeltaRotation);

	const FRotator DeltaControlRotation = (Other.ControlRotation - ControlRotation).GetNormalized();
	ControlRotation = ControlRotation + (OtherWeight * DeltaControlRotation);

	FieldOfView = FMath::Lerp(FieldOfView, Other.FieldOfView, OtherWeight);
}


//////////////////////////////////////////////////////////////////////////
// ULyraCameraMode
//////////////////////////////////////////////////////////////////////////
// 初始化默认 FOV、俯仰限制、线性混合参数以及尚未混入的权重状态。
ULyraCameraMode::ULyraCameraMode()
{
	FieldOfView = LYRA_CAMERA_DEFAULT_FOV;
	ViewPitchMin = LYRA_CAMERA_DEFAULT_PITCH_MIN;
	ViewPitchMax = LYRA_CAMERA_DEFAULT_PITCH_MAX;

	BlendTime = 0.5f;
	BlendFunction = ELyraCameraModeBlendFunction::EaseOut;
	BlendExponent = 4.0f;
	BlendAlpha = 1.0f;
	BlendWeight = 1.0f;
}

// 返回作为该模式 Outer 的 LyraCameraComponent；Outer 类型不匹配时返回 nullptr。
ULyraCameraComponent* ULyraCameraMode::GetLyraCameraComponent() const
{
	return CastChecked<ULyraCameraComponent>(GetOuter());
}

// 通过所属 LyraCameraComponent 返回 World；模式尚未挂接到组件时返回 nullptr。
UWorld* ULyraCameraMode::GetWorld() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? nullptr : GetOuter()->GetWorld();
}

// 通过所属 LyraCameraComponent 返回当前相机目标；组件不存在时返回 nullptr。
AActor* ULyraCameraMode::GetTargetActor() const
{
	const ULyraCameraComponent* LyraCameraComponent = GetLyraCameraComponent();

	return LyraCameraComponent->GetTargetActor();
}

// 根据目标 Pawn 或 Character 计算相机枢轴位置，并补偿蹲伏胶囊高度变化。
FVector ULyraCameraMode::GetPivotLocation() const
{
	const AActor* TargetActor = GetTargetActor();
	check(TargetActor);

	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		// 根据 Character 的蹲伏胶囊高度差修正相机枢轴，避免蹲伏时视点突跳。
		// Height adjustments for characters to account for crouching.
		if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetPawn))
		{
			const ACharacter* TargetCharacterCDO = TargetCharacter->GetClass()->GetDefaultObject<ACharacter>();
			check(TargetCharacterCDO);

			const UCapsuleComponent* CapsuleComp = TargetCharacter->GetCapsuleComponent();
			check(CapsuleComp);

			const UCapsuleComponent* CapsuleCompCDO = TargetCharacterCDO->GetCapsuleComponent();
			check(CapsuleCompCDO);

			const float DefaultHalfHeight = CapsuleCompCDO->GetUnscaledCapsuleHalfHeight();
			const float ActualHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
			const float HeightAdjustment = (DefaultHalfHeight - ActualHalfHeight) + TargetCharacterCDO->BaseEyeHeight;

			return TargetCharacter->GetActorLocation() + (FVector::UpVector * HeightAdjustment);
		}

		return TargetPawn->GetPawnViewLocation();
	}

	return TargetActor->GetActorLocation();
}

// 从目标 Pawn 的 ViewRotation 获取相机枢轴旋转，并规范化俯仰角范围。
FRotator ULyraCameraMode::GetPivotRotation() const
{
	const AActor* TargetActor = GetTargetActor();
	check(TargetActor);

	if (const APawn* TargetPawn = Cast<APawn>(TargetActor))
	{
		return TargetPawn->GetViewRotation();
	}

	return TargetActor->GetActorRotation();
}

// 依次更新模式视图和混合进度，生成该模式本帧输出。
void ULyraCameraMode::UpdateCameraMode(float DeltaTime)
{
	UpdateView(DeltaTime);
	UpdateBlending(DeltaTime);
}

// 使用目标枢轴位置和旋转生成基础 View，并应用 FOV 与俯仰角限制。
void ULyraCameraMode::UpdateView(float DeltaTime)
{
	FVector PivotLocation = GetPivotLocation();
	FRotator PivotRotation = GetPivotRotation();

	PivotRotation.Pitch = FMath::ClampAngle(PivotRotation.Pitch, ViewPitchMin, ViewPitchMax);

	View.Location = PivotLocation;
	View.Rotation = PivotRotation;
	View.ControlRotation = View.Rotation;
	View.FieldOfView = FieldOfView;
}

// 直接设置模式最终混合权重，并按缓动函数反推出对应线性 BlendAlpha。
void ULyraCameraMode::SetBlendWeight(float Weight)
{
	BlendWeight = FMath::Clamp(Weight, 0.0f, 1.0f);

	// 外部直接设置的是最终权重，因此需按当前缓动函数反推线性 BlendAlpha。
	// Since we're setting the blend weight directly, we need to calculate the blend alpha to account for the blend function.
	const float InvExponent = (BlendExponent > 0.0f) ? (1.0f / BlendExponent) : 1.0f;

	switch (BlendFunction)
	{
	case ELyraCameraModeBlendFunction::Linear:
		BlendAlpha = BlendWeight;
		break;

	case ELyraCameraModeBlendFunction::EaseIn:
		BlendAlpha = FMath::InterpEaseIn(0.0f, 1.0f, BlendWeight, InvExponent);
		break;

	case ELyraCameraModeBlendFunction::EaseOut:
		BlendAlpha = FMath::InterpEaseOut(0.0f, 1.0f, BlendWeight, InvExponent);
		break;

	case ELyraCameraModeBlendFunction::EaseInOut:
		BlendAlpha = FMath::InterpEaseInOut(0.0f, 1.0f, BlendWeight, InvExponent);
		break;

	default:
		checkf(false, TEXT("SetBlendWeight: Invalid BlendFunction [%d]\n"), (uint8)BlendFunction);
		break;
	}
}

// 按 BlendTime 推进 BlendAlpha，再依据配置的缓动函数和指数计算 BlendWeight。
void ULyraCameraMode::UpdateBlending(float DeltaTime)
{
	if (BlendTime > 0.0f)
	{
		BlendAlpha += (DeltaTime / BlendTime);
		BlendAlpha = FMath::Min(BlendAlpha, 1.0f);
	}
	else
	{
		BlendAlpha = 1.0f;
	}

	const float Exponent = (BlendExponent > 0.0f) ? BlendExponent : 1.0f;

	switch (BlendFunction)
	{
	case ELyraCameraModeBlendFunction::Linear:
		BlendWeight = BlendAlpha;
		break;

	case ELyraCameraModeBlendFunction::EaseIn:
		BlendWeight = FMath::InterpEaseIn(0.0f, 1.0f, BlendAlpha, Exponent);
		break;

	case ELyraCameraModeBlendFunction::EaseOut:
		BlendWeight = FMath::InterpEaseOut(0.0f, 1.0f, BlendAlpha, Exponent);
		break;

	case ELyraCameraModeBlendFunction::EaseInOut:
		BlendWeight = FMath::InterpEaseInOut(0.0f, 1.0f, BlendAlpha, Exponent);
		break;

	default:
		checkf(false, TEXT("UpdateBlending: Invalid BlendFunction [%d]\n"), (uint8)BlendFunction);
		break;
	}
}

// 在 Canvas 上输出模式类、类型标签、混合参数和当前 View 数据。
void ULyraCameraMode::DrawDebug(UCanvas* Canvas) const
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetDrawColor(FColor::White);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("      LyraCameraMode: %s (%f)"), *GetName(), BlendWeight));
}


//////////////////////////////////////////////////////////////////////////
// ULyraCameraModeStack
//////////////////////////////////////////////////////////////////////////
// 构造默认激活的相机模式栈，并保持实例池和活动栈为空。
ULyraCameraModeStack::ULyraCameraModeStack()
{
	bIsActive = true;
}

// 激活模式栈，并对当前栈内每个模式调用 OnActivation。
void ULyraCameraModeStack::ActivateStack()
{
	if (!bIsActive)
	{
		bIsActive = true;

		// 激活模式栈时，通知当前栈内所有模式进入激活状态。
		// Notify camera modes that they are being activated.
		for (ULyraCameraMode* CameraMode : CameraModeStack)
		{
			check(CameraMode);
			CameraMode->OnActivation();
		}
	}
}

// 停用模式栈，并对当前栈内每个模式调用 OnDeactivation。
void ULyraCameraModeStack::DeactivateStack()
{
	if (bIsActive)
	{
		bIsActive = false;

		// 停用模式栈时，通知当前栈内所有模式退出激活状态。
		// Notify camera modes that they are being deactivated.
		for (ULyraCameraMode* CameraMode : CameraModeStack)
		{
			check(CameraMode);
			CameraMode->OnDeactivation();
		}
	}
}

// 复用或创建目标模式实例，保留其旧贡献权重后移到栈顶，并确保栈底保持完整权重。
void ULyraCameraModeStack::PushCameraMode(TSubclassOf<ULyraCameraMode> CameraModeClass)
{
	if (!CameraModeClass)
	{
		return;
	}

	ULyraCameraMode* CameraMode = GetCameraModeInstance(CameraModeClass);
	check(CameraMode);

	int32 StackSize = CameraModeStack.Num();

	if ((StackSize > 0) && (CameraModeStack[0] == CameraMode))
	{
		// 目标模式已位于栈顶，无需重新插入或重置混合。
		// Already top of stack.
		return;
	}

	// 若模式已在栈中，先移除旧位置，并计算它此前对最终视图的实际贡献。
	// 该贡献将作为重新压到栈顶时的初始权重，避免画面跳变。
	// See if it's already in the stack and remove it.
	// Figure out how much it was contributing to the stack.
	int32 ExistingStackIndex = INDEX_NONE;
	float ExistingStackContribution = 1.0f;

	for (int32 StackIndex = 0; StackIndex < StackSize; ++StackIndex)
	{
		if (CameraModeStack[StackIndex] == CameraMode)
		{
			ExistingStackIndex = StackIndex;
			ExistingStackContribution *= CameraMode->GetBlendWeight();
			break;
		}
		else
		{
			ExistingStackContribution *= (1.0f - CameraModeStack[StackIndex]->GetBlendWeight());
		}
	}

	if (ExistingStackIndex != INDEX_NONE)
	{
		CameraModeStack.RemoveAt(ExistingStackIndex);
		StackSize--;
	}
	else
	{
		ExistingStackContribution = 0.0f;
	}

	// 有混合时间且栈非空时从旧贡献权重开始，否则直接以完整权重启用。
	// Decide what initial weight to start with.
	const bool bShouldBlend = ((CameraMode->GetBlendTime() > 0.0f) && (StackSize > 0));
	const float BlendWeight = (bShouldBlend ? ExistingStackContribution : 1.0f);

	CameraMode->SetBlendWeight(BlendWeight);

	// 将目标模式插入栈顶，使其成为优先视图层。
	// Add new entry to top of stack.
	CameraModeStack.Insert(CameraMode, 0);

	// 栈底模式始终保持 100% 权重，作为所有上层模式的基础视图。
	// Make sure stack bottom is always weighted 100%.
	CameraModeStack.Last()->SetBlendWeight(1.0f);

	// 只有首次加入栈时才触发 OnActivation；栈内重排不重复激活。
	// Let the camera mode know if it's being added to the stack.
	if (ExistingStackIndex == INDEX_NONE)
	{
		CameraMode->OnActivation();
	}
}

// 栈激活时先更新各层混合权重和裁剪无效底层，再从栈底向上合成最终视图。
bool ULyraCameraModeStack::EvaluateStack(float DeltaTime, FLyraCameraModeView& OutCameraModeView)
{
	if (!bIsActive)
	{
		return false;
	}

	UpdateStack(DeltaTime);
	BlendStack(OutCameraModeView);

	return true;
}

// 从实例池复用指定类的相机模式；不存在时以 CameraComponent 为 Outer 创建并缓存新实例。
ULyraCameraMode* ULyraCameraModeStack::GetCameraModeInstance(TSubclassOf<ULyraCameraMode> CameraModeClass)
{
	check(CameraModeClass);

	// 优先复用该模式类已有的实例，保留其运行时状态并避免重复分配。
	// First see if we already created one.
	for (ULyraCameraMode* CameraMode : CameraModeInstances)
	{
		if ((CameraMode != nullptr) && (CameraMode->GetClass() == CameraModeClass))
		{
			return CameraMode;
		}
	}

	// 实例池中不存在时，以 CameraComponent 为外部对象创建新模式实例。
	// Not found, so we need to create it.
	ULyraCameraMode* NewCameraMode = NewObject<ULyraCameraMode>(GetOuter(), CameraModeClass, NAME_None, RF_NoFlags);
	check(NewCameraMode);

	CameraModeInstances.Add(NewCameraMode);

	return NewCameraMode;
}

// 更新栈内模式，首个达到完整权重的模式会遮蔽并移除其下方全部模式。
void ULyraCameraModeStack::UpdateStack(float DeltaTime)
{
	const int32 StackSize = CameraModeStack.Num();
	if (StackSize <= 0)
	{
		return;
	}

	int32 RemoveCount = 0;
	int32 RemoveIndex = INDEX_NONE;

	for (int32 StackIndex = 0; StackIndex < StackSize; ++StackIndex)
	{
		ULyraCameraMode* CameraMode = CameraModeStack[StackIndex];
		check(CameraMode);

		CameraMode->UpdateCameraMode(DeltaTime);

		if (CameraMode->GetBlendWeight() >= 1.0f)
		{
			// 当某层权重达到 100% 时，更低层已不会影响最终视图，可以从栈中移除。
			// Everything below this mode is now irrelevant and can be removed.
			RemoveIndex = (StackIndex + 1);
			RemoveCount = (StackSize - RemoveIndex);
			break;
		}
	}

	if (RemoveCount > 0)
	{
		// 对即将从栈中移除的模式触发 OnDeactivation。
		// Let the camera modes know they being removed from the stack.
		for (int32 StackIndex = RemoveIndex; StackIndex < StackSize; ++StackIndex)
		{
			ULyraCameraMode* CameraMode = CameraModeStack[StackIndex];
			check(CameraMode);

			CameraMode->OnDeactivation();
		}

		CameraModeStack.RemoveAt(RemoveIndex, RemoveCount);
	}
}

// 以完整权重栈底为基础，逐层向栈顶混合相机视图。
void ULyraCameraModeStack::BlendStack(FLyraCameraModeView& OutCameraModeView) const
{
	const int32 StackSize = CameraModeStack.Num();
	if (StackSize <= 0)
	{
		return;
	}

	// 从完整权重的栈底视图开始，逐层向栈顶混合得到最终结果。
	// Start at the bottom and blend up the stack
	const ULyraCameraMode* CameraMode = CameraModeStack[StackSize - 1];
	check(CameraMode);

	OutCameraModeView = CameraMode->GetCameraModeView();

	for (int32 StackIndex = (StackSize - 2); StackIndex >= 0; --StackIndex)
	{
		CameraMode = CameraModeStack[StackIndex];
		check(CameraMode);

		OutCameraModeView.Blend(CameraMode->GetCameraModeView(), CameraMode->GetBlendWeight());
	}
}

// 按栈顺序输出每个相机模式的类名、标签和混合权重，并调用模式自身调试绘制。
void ULyraCameraModeStack::DrawDebug(UCanvas* Canvas) const
{
	check(Canvas);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;

	DisplayDebugManager.SetDrawColor(FColor::Green);
	DisplayDebugManager.DrawString(FString(TEXT("   --- Camera Modes (Begin) ---")));

	for (const ULyraCameraMode* CameraMode : CameraModeStack)
	{
		check(CameraMode);
		CameraMode->DrawDebug(Canvas);
	}

	DisplayDebugManager.SetDrawColor(FColor::Green);
	DisplayDebugManager.DrawString(FString::Printf(TEXT("   --- Camera Modes (End) ---")));
}

// 输出栈顶模式的 BlendWeight 与 CameraTypeTag；栈为空时输出 1.0 和空标签。
void ULyraCameraModeStack::GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const
{
	if (CameraModeStack.Num() == 0)
	{
		OutWeightOfTopLayer = 1.0f;
		OutTagOfTopLayer = FGameplayTag();
		return;
	}
	else
	{
		ULyraCameraMode* TopEntry = CameraModeStack.Last();
		check(TopEntry);
		OutWeightOfTopLayer = TopEntry->GetBlendWeight();
		OutTagOfTopLayer = TopEntry->GetCameraTypeTag();
	}
}

