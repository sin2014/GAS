// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/AimAssistInputModifier.h"
#include "CommonInputTypeEnum.h"
#include "Curves/CurveFloat.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EnhancedPlayerInput.h"
#include "Input/AimAssistTargetManagerComponent.h"
#include "Input/LyraAimSensitivityData.h"
#include "Player/LyraLocalPlayer.h"
#include "Player/LyraPlayerState.h"
#include "SceneView.h"
#include "Settings/LyraSettingsShared.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AimAssistInputModifier)

#if ENABLE_DRAW_DEBUG
#include "Engine/Canvas.h"
#include "Debug/DebugDrawService.h"
#endif	// ENABLE_DRAW_DEBUG

// 定义 Aim Assist 目标收集、配置异常和可见性诊断所使用的日志分类。
DEFINE_LOG_CATEGORY(LogAimAssist);

namespace LyraConsoleVariables
{
	// 非 Shipping 构建中控制是否完全启用 Aim Assist 输入修改。
	static bool bEnableAimAssist = true;
	// 将 Aim Assist 总开关暴露为作弊控制台变量。
	static FAutoConsoleVariableRef CVarEnableAimAssist(
		TEXT("lyra.Weapon.EnableAimAssist"),
		bEnableAimAssist,
		TEXT("Should we enable aim assist while shooting?"),
		ECVF_Cheat);

	// 控制是否绘制准星区域、目标评分和最终输入值等 Aim Assist 调试信息。
	static bool bDrawAimAssistDebug = false;
	// 将 Aim Assist 调试绘制开关暴露为作弊控制台变量。
	static FAutoConsoleVariableRef CVarDrawAimAssistDebug(
		TEXT("lyra.Weapon.DrawAimAssistDebug"),
		bDrawAimAssistDebug,
		TEXT("Should we draw some debug stats about aim assist?"),
		ECVF_Cheat);
}

///////////////////////////////////////////////////////////////////
// FLyraAimAssistTarget

// 清空目标组件、投影、移动、权重、可见性和异步 Trace 状态，使缓存条目恢复为无效目标。
void FLyraAimAssistTarget::ResetTarget()
{
	TargetShapeComponent = nullptr;

	Location = FVector::ZeroVector;
	DeltaMovement = FVector::ZeroVector;
	ScreenBounds.Init();

	ViewDistance = 0.0f;
	SortScore = 0.0f;

	AssistTime = 0.0f;
	AssistWeight = 0.0f;

	VisibilityTraceHandle = FTraceHandle();

	bIsVisible = false;
	bUnderAssistInnerReticle = false;
	bUnderAssistOuterReticle = false;	
}

// 在玩家局部空间比较目标和玩家的帧间位移，计算维持准星相对目标位置所需补偿的 Yaw/Pitch 旋转。
FRotator FLyraAimAssistTarget::GetRotationFromMovement(const FAimAssistOwnerViewData& OwnerInfo) const
{
	ensure(OwnerInfo.IsDataValid());

	// 将目标前后位置统一转换到玩家局部空间，并从新位置扣除玩家自身位移。
	// Convert everything into player space.
	// Account for player movement in new target location.
	const FVector OldLocation = OwnerInfo.PlayerInverseTransform.TransformPositionNoScale(Location - DeltaMovement);
	const FVector NewLocation = OwnerInfo.PlayerInverseTransform.TransformPositionNoScale(Location - OwnerInfo.DeltaMovement);

	FRotator RotationToTarget;
	RotationToTarget.Yaw = CalculateRotationToTarget2D(NewLocation.X, NewLocation.Y, OldLocation.Y);
	RotationToTarget.Pitch = CalculateRotationToTarget2D(NewLocation.X, NewLocation.Z, OldLocation.Z);
	RotationToTarget.Roll = 0.0f;

	return RotationToTarget;
}

// 在单个二维平面内计算从旧横向偏移跟随到目标新位置所需角度；目标位于身后时不施加旋转。
float FLyraAimAssistTarget::CalculateRotationToTarget2D(float TargetX, float TargetY, float OffsetY) const
{
	if (TargetX <= 0.0f)
	{
		return 0.0f;
	}

	const float AngleA = FMath::RadiansToDegrees(FMath::Atan2(TargetY, TargetX));

	if (FMath::IsNearlyZero(OffsetY))
	{
		return AngleA;
	}

	const float Distance = FMath::Sqrt((TargetX * TargetX) + (TargetY * TargetY));
	ensure(Distance > 0.0f);

	const float AngleB = FMath::RadiansToDegrees(FMath::Asin(OffsetY / Distance));

	return FRotator::NormalizeAxis(AngleA - AngleB);
}

///////////////////////////////////////////////////////////////////
// FAimAssistSettings

// 设置 Aim Assist 的默认准星尺寸、目标距离、腰射/ADS 拉拽与减速强度、插值速率及功能开关。
FAimAssistSettings::FAimAssistSettings()
{
	AssistInnerReticleWidth.SetValue(20.0f);
	AssistInnerReticleHeight.SetValue(20.0f);
	AssistOuterReticleWidth.SetValue(80.0f);
	AssistOuterReticleHeight.SetValue(80.0f);

	TargetingReticleWidth.SetValue(1200.0f);
	TargetingReticleHeight.SetValue(675.0f);
	TargetRange.SetValue(10000.0f);
	TargetWeightCurve = nullptr;

	PullInnerStrengthHip.SetValue(0.6f);
	PullOuterStrengthHip.SetValue(0.5f);
	PullInnerStrengthAds.SetValue(0.7f);
	PullOuterStrengthAds.SetValue(0.4f);
	PullLerpInRate.SetValue(60.0f);
	PullLerpOutRate.SetValue(4.0f);
	PullMaxRotationRate.SetValue(0.0f);

	SlowInnerStrengthHip.SetValue(0.6f);
	SlowOuterStrengthHip.SetValue(0.5f);
	SlowInnerStrengthAds.SetValue(0.7f);
	SlowOuterStrengthAds.SetValue(0.4f);
	SlowLerpInRate.SetValue(60.0f);
	SlowLerpOutRate.SetValue(4.0f);
	SlowMinRotationRate.SetValue(0.0f);

	bEnableAsyncVisibilityTrace = true;
	bRequireInput = true;
	bApplyPull = true;
	bApplySlowing = true;
	bApplyStrafePullScale = true;
	bUseDynamicSlow = true;
	bUseRadialLookRates = true;
}

// 从配置曲线求目标持续受辅助时间对应的权重，并限制在 0 到 1；曲线缺失时返回 0。
float FAimAssistSettings::GetTargetWeightForTime(float Time) const
{
	if (!ensure(TargetWeightCurve != nullptr))
	{
		return 0.0f;
	}

	return FMath::Clamp(TargetWeightCurve->GetFloatValue(Time), 0.0f, 1.0f);
}

// 返回目标权重曲线的最大时间，用于限制 AssistTime 累积；曲线缺失时返回 0。
float FAimAssistSettings::GetTargetWeightMaxTime() const
{
	if (!ensure(TargetWeightCurve != nullptr))
	{
		return 0.0f;
	}

	float MinTime = 0.0f;
	float MaxTime = 0.0f;

	TargetWeightCurve->FloatCurve.GetTimeRange(MinTime, MaxTime);

	return MaxTime;
}

///////////////////////////////////////////////////////////////////
// FAimAssistOwnerViewData

// 从本地 PlayerController 更新视图/投影矩阵、视口、Pawn 位置、控制旋转、帧间移动和 Lyra 队伍 ID；任一关键数据缺失时整体重置。
void FAimAssistOwnerViewData::UpdateViewData(const APlayerController* PC)
{
	FSceneViewProjectionData ProjectionData;
	PlayerController = PC;
	LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;

	if (!IsDataValid() || !PlayerController || !LocalPlayer)
	{
		ResetViewData();
		return;
	}
	
	const APawn* Pawn = Cast<APawn>(PlayerController->GetPawn());
	
	if (!Pawn || !LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
	{
		ResetViewData();
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	ProjectionMatrix = ProjectionData.ProjectionMatrix;
	ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
	ViewRect = ProjectionData.GetConstrainedViewRect();
	ViewTransform = FTransform(ViewRotation, ViewLocation);
	ViewForward = ViewTransform.GetUnitAxis(EAxis::X);

	const FVector OldLocation = PlayerTransform.GetTranslation();
	const FVector NewLocation = Pawn->GetActorLocation();
	const FRotator NewRotation = PC->GetControlRotation();

	PlayerTransform = FTransform(NewRotation, NewLocation);
	PlayerInverseTransform = PlayerTransform.Inverse();

	DeltaMovement = (NewLocation - OldLocation);

	// 从 LyraPlayerState 缓存队伍 ID；PlayerState 尚未建立时使用 INDEX_NONE。
	// Set the Team ID
	if (ALyraPlayerState* LyraPS = PlayerController->GetPlayerState<ALyraPlayerState>())
	{
		TeamID = LyraPS->GetTeamId();
	}
	else
	{
		TeamID = INDEX_NONE;
	}
}

// 清除控制器和本地玩家引用，把所有矩阵、变换、位移和队伍信息恢复为无效默认值。
void FAimAssistOwnerViewData::ResetViewData()
{
	PlayerController = nullptr;
	LocalPlayer = nullptr;
	
	ProjectionMatrix = FMatrix::Identity;
	ViewProjectionMatrix = FMatrix::Identity;
	ViewRect = FIntRect(0, 0, 0, 0);
	ViewTransform = FTransform::Identity;

	PlayerTransform = FTransform::Identity;
	PlayerInverseTransform = FTransform::Identity;
	ViewForward = FVector::ZeroVector;

	DeltaMovement = FVector::ZeroVector;
	TeamID = INDEX_NONE;
}

// 把参考深度处以视线中心对称的世界空间准星尺寸投影成屏幕矩形；投影失败时返回无效边界。
FBox2D FAimAssistOwnerViewData::ProjectReticleToScreen(float ReticleWidth, float ReticleHeight, float ReticleDepth) const
{
	FBox2D ReticleBounds(ForceInitToZero);

	const FVector ReticleExtents((ReticleWidth * 0.5f), -(ReticleHeight * 0.5f), ReticleDepth);

	if (FSceneView::ProjectWorldToScreen(ReticleExtents, ViewRect, ProjectionMatrix, ReticleBounds.Max))
	{
		ReticleBounds.Min.X = ViewRect.Min.X + (ViewRect.Max.X - ReticleBounds.Max.X);
		ReticleBounds.Min.Y = ViewRect.Min.Y + (ViewRect.Max.Y - ReticleBounds.Max.Y);

		ReticleBounds.bIsValid = true;
	}

	return ReticleBounds;
}

// 投影世界空间 AABB 的八个顶点并合并为屏幕空间包围盒。
FBox2D FAimAssistOwnerViewData::ProjectBoundsToScreen(const FBox& Bounds) const
{
	FBox2D Box2D(ForceInitToZero);

	if (Bounds.IsValid)
	{
		const FVector Vertices[] =
		{
			FVector(Bounds.Min),
			FVector(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.Z),
			FVector(Bounds.Min.X, Bounds.Max.Y, Bounds.Min.Z),
			FVector(Bounds.Max.X, Bounds.Min.Y, Bounds.Min.Z),
			FVector(Bounds.Max.X, Bounds.Max.Y, Bounds.Min.Z),
			FVector(Bounds.Max.X, Bounds.Min.Y, Bounds.Max.Z),
			FVector(Bounds.Min.X, Bounds.Max.Y, Bounds.Max.Z),
			FVector(Bounds.Max)
		};

		for (int32 VerticeIndex = 0; VerticeIndex < UE_ARRAY_COUNT(Vertices); ++VerticeIndex)
		{
			FVector2D ScreenPoint;
			if (FSceneView::ProjectWorldToScreen(Vertices[VerticeIndex], ViewRect, ViewProjectionMatrix, ScreenPoint))
			{
				Box2D += ScreenPoint;
			}
		}
	}

	return Box2D;
}

// 按碰撞形状类型分派到 Box、Sphere 或 Capsule 投影；不支持的形状记录警告并返回无效边界。
FBox2D FAimAssistOwnerViewData::ProjectShapeToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const
{
	FBox2D Box2D(ForceInitToZero);

	switch (Shape.ShapeType)
	{
	case ECollisionShape::Box:
		Box2D = ProjectBoxToScreen(Shape, ShapeOrigin, WorldTransform);
		break;
	case ECollisionShape::Sphere:
		Box2D = ProjectSphereToScreen(Shape, ShapeOrigin, WorldTransform);
		break;
	case ECollisionShape::Capsule:
		Box2D = ProjectCapsuleToScreen(Shape, ShapeOrigin, WorldTransform);
		break;
	default:
		UE_LOG(LogAimAssist, Warning, TEXT("FAimAssistOwnerViewData::ProjectShapeToScreen() - Invalid shape type!"));
		break;
	}

	return Box2D;
}

// 将 Box 的八个局部顶点变换到世界空间后投影，得到覆盖目标的屏幕矩形。
FBox2D FAimAssistOwnerViewData::ProjectBoxToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const
{
	check(Shape.IsBox());
	check(!Shape.IsNearlyZero());

	const FVector BoxExtents = Shape.GetBox();

	const FVector Vertices[] =
	{
		FVector(-BoxExtents.X, -BoxExtents.Y, -BoxExtents.Z),
		FVector(-BoxExtents.X, -BoxExtents.Y,  BoxExtents.Z),
		FVector(-BoxExtents.X,  BoxExtents.Y, -BoxExtents.Z),
		FVector(-BoxExtents.X,  BoxExtents.Y,  BoxExtents.Z),
		FVector( BoxExtents.X, -BoxExtents.Y, -BoxExtents.Z),
		FVector( BoxExtents.X, -BoxExtents.Y,  BoxExtents.Z),
		FVector( BoxExtents.X,  BoxExtents.Y, -BoxExtents.Z),
		FVector( BoxExtents.X,  BoxExtents.Y,  BoxExtents.Z)
	};

	FBox2D Box2D(ForceInitToZero);

	for (int32 VerticeIndex = 0; VerticeIndex < UE_ARRAY_COUNT(Vertices); ++VerticeIndex)
	{
		const FVector Vertex = WorldTransform.TransformPositionNoScale(Vertices[VerticeIndex] + ShapeOrigin);

		FVector2D ScreenPoint;
		if (FSceneView::ProjectWorldToScreen(Vertex, ViewRect, ViewProjectionMatrix, ScreenPoint))
		{
			Box2D += ScreenPoint;
		}
	}

	return Box2D;
}

// 沿相机右轴和上轴构造 Sphere 的屏幕对角极值点并投影，近似其屏幕包围盒。
FBox2D FAimAssistOwnerViewData::ProjectSphereToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const
{
	check(Shape.IsSphere());
	check(!Shape.IsNearlyZero());

	const FVector ViewAxisY = ViewTransform.GetUnitAxis(EAxis::Y);
	const FVector ViewAxisZ = ViewTransform.GetUnitAxis(EAxis::Z);

	const float SphereRadius = Shape.GetSphereRadius();
	const FVector SphereLocation = WorldTransform.TransformPositionNoScale(ShapeOrigin);
	const FVector SphereExtent = (ViewAxisY * SphereRadius) + (ViewAxisZ * SphereRadius);

	const FVector Vertices[] =
	{
		FVector(SphereLocation + SphereExtent),
		FVector(SphereLocation - SphereExtent),
	};

	FBox2D Box2D(ForceInitToZero);

	for (int32 VerticeIndex = 0; VerticeIndex < UE_ARRAY_COUNT(Vertices); ++VerticeIndex)
	{
		FVector2D ScreenPoint;
		if (FSceneView::ProjectWorldToScreen(Vertices[VerticeIndex], ViewRect, ViewProjectionMatrix, ScreenPoint))
		{
			Box2D += ScreenPoint;
		}
	}

	return Box2D;
}

// 对 Capsule 顶部和底部球心分别构造面向相机的极值点并投影，形成完整屏幕包围盒。
FBox2D FAimAssistOwnerViewData::ProjectCapsuleToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const
{
	check(Shape.IsCapsule());
	check(!Shape.IsNearlyZero());

	const FVector ViewAxisY = ViewTransform.GetUnitAxis(EAxis::Y);
	const FVector ViewAxisZ = ViewTransform.GetUnitAxis(EAxis::Z);

	const float CapsuleAxisHalfLength = Shape.GetCapsuleAxisHalfLength();
	const float CapsuleRadius = Shape.GetCapsuleRadius();

	const FVector TopSphereLocation = WorldTransform.TransformPositionNoScale(FVector(0.0f, 0.0f, CapsuleAxisHalfLength) + ShapeOrigin);
	const FVector BottomSphereLocation = WorldTransform.TransformPositionNoScale(FVector(0.0f, 0.0f, -CapsuleAxisHalfLength) + ShapeOrigin);
	const FVector SphereExtent = (ViewAxisY * CapsuleRadius) + (ViewAxisZ * CapsuleRadius);

	const FVector Vertices[] =
	{
		FVector(TopSphereLocation + SphereExtent),
		FVector(TopSphereLocation - SphereExtent),
		FVector(BottomSphereLocation + SphereExtent),
		FVector(BottomSphereLocation - SphereExtent),
	};

	FBox2D Box2D(ForceInitToZero);

	for (int32 VerticeIndex = 0; VerticeIndex < UE_ARRAY_COUNT(Vertices); ++VerticeIndex)
	{
		FVector2D ScreenPoint;
		if (FSceneView::ProjectWorldToScreen(Vertices[VerticeIndex], ViewRect, ViewProjectionMatrix, ScreenPoint))
		{
			Box2D += ScreenPoint;
		}
	}

	return Box2D;
}

///////////////////////////////////////////////////////////////////
// UAimAssistInputModifier

// 定义手柄满档 Yaw/Pitch 基础观察速率，Pitch 为 Yaw 的 60%。
static const float GamepadUserOptions_YawLookRateBase = 900.0f;
static const float GamepadUserOptions_PitchLookRateBase = (GamepadUserOptions_YawLookRateBase * 0.6f);

// TODO：将观察速度百分比到旋转速率的换算宏改为 constexpr 函数。
// TODO Make this a constexpr instead of a define
#define YawLookSpeedToRotationRate(_Speed)		((_Speed) / 100.0f * GamepadUserOptions_YawLookRateBase)
#define PitchLookSpeedToRotationRate(_Speed)	((_Speed) / 100.0f * GamepadUserOptions_PitchLookRateBase)

// 把默认手柄灵敏度换算为 Yaw/Pitch 旋转速率，并可按摇杆偏转角统一径向速率以保持对角方向准确。
FRotator UAimAssistInputModifier::GetLookRates(const FVector& LookInput)
{
	FRotator LookRates;

	const float SensitivityHipLevel = 50.0f;
	{
		LookRates.Yaw = YawLookSpeedToRotationRate(SensitivityHipLevel);
		LookRates.Pitch = PitchLookSpeedToRotationRate(SensitivityHipLevel);
		LookRates.Roll = 0.0f;
	}

	LookRates.Yaw = FMath::Clamp(LookRates.Yaw, 0.0f, GamepadUserOptions_YawLookRateBase);
	LookRates.Pitch = FMath::Clamp(LookRates.Pitch, 0.0f, GamepadUserOptions_PitchLookRateBase);

	if (Settings.bUseRadialLookRates)
	{
		// 根据摇杆偏转方向在 Yaw 与 Pitch 速率之间混合，使对角输入的实际旋转方向保持准确。
		// Blend between yaw and pitch based on stick deflection.  This keeps diagonals accurate.
		const float RadialLerp = FMath::Atan2(FMath::Abs(LookInput.Y), FMath::Abs(LookInput.X)) / HALF_PI;
		const float RadialLookRate = FMath::Lerp(LookRates.Yaw, LookRates.Pitch, RadialLerp);

		LookRates.Yaw = RadialLookRate;
		LookRates.Pitch = RadialLookRate;	
	}
	
	return LookRates;
}

// 每次输入更新时刷新玩家视图和目标缓存，把 Aim Assist 计算出的旋转速度还原为归一化观察输入；禁用或上下文无效时原样返回输入。
FInputActionValue UAimAssistInputModifier::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAimAssistInputModifier::ModifyRaw_Implementation);

#if ENABLE_DRAW_DEBUG
	if (LyraConsoleVariables::bDrawAimAssistDebug)
	{
		if (!DebugDrawHandle.IsValid())
		{
			DebugDrawHandle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &UAimAssistInputModifier::AimAssistDebugDraw));
		}
		else
		{
			UDebugDrawService::Unregister(DebugDrawHandle);
			DebugDrawHandle.Reset();
		}
		bRegisteredDebug = true;
	}
#endif

#if !UE_BUILD_SHIPPING
	if (!LyraConsoleVariables::bEnableAimAssist)
	{
		return CurrentValue;
	}
#endif //UE_BUILD_SHIPPING

	APlayerController* PC = PlayerInput ? Cast<APlayerController>(PlayerInput->GetOuter()) : nullptr;
	if (!PC)
	{
		return CurrentValue;
	}

	// 根据当前 PlayerController 更新玩家视图信息，包括用于目标可见性判断的视图矩阵和当前旋转。
	// Update the "owner" information based on our current player controller. This calculates and stores things like the view matrix
	// and current rotation that is used to determine what targets are visible
	OwnerViewData.UpdateViewData(PC);

	if (!OwnerViewData.IsDataValid())
	{
		return CurrentValue;
	}
	
	// 交换双缓冲目标缓存，更新当前可见目标和评分，供后续计算每个目标的拉拽与减速贡献。
	// Swaps the target cache's and determines what targets are currently visible. Updates the score of each target to determine
	// how much pull/slow effect should be applied to each
	UpdateTargetData(DeltaTime);

	FVector BaselineInput = CurrentValue.Get<FVector>();
	
	FVector OutAssistedInput = BaselineInput;
	FVector CurrentMoveInput = MoveInputAction ? PlayerInput->GetActionValue(MoveInputAction).Get<FVector>() : FVector::ZeroVector;	

	// TODO：当前观察速率换算仍存在不准确之处，需要与实际灵敏度管线进一步统一。
	// Something about the look rates is incorrect
	FRotator LookRates = GetLookRates(BaselineInput);
	
	const FRotator RotationalVelocity = UpdateRotationalVelocity(PC, DeltaTime, BaselineInput, CurrentMoveInput);
	
	if (LookRates.Yaw > 0.0f)
	{
		OutAssistedInput.X = (RotationalVelocity.Yaw / LookRates.Yaw);
		OutAssistedInput.X = FMath::Clamp(OutAssistedInput.X, -1.0f, 1.0f);
	}
	
	if (LookRates.Pitch > 0.0f)
	{
		OutAssistedInput.Y = (RotationalVelocity.Pitch / LookRates.Pitch);
		OutAssistedInput.Y = FMath::Clamp(OutAssistedInput.Y, -1.0f, 1.0f);
	}

#if ENABLE_DRAW_DEBUG
	LastBaselineValue = BaselineInput;
	LastLookRatePitch = LookRates.Pitch;
	LastLookRateYaw = LookRates.Yaw;
	LastOutValue = OutAssistedInput;
#endif
	return OutAssistedInput;
}

// 从 GameState 的目标管理器重建本帧缓存，按目标在可见外圈内停留时间增减 AssistTime，并归一化所有目标权重。
void UAimAssistInputModifier::UpdateTargetData(float DeltaTime)
{
	if(!ensure(OwnerViewData.PlayerController))
	{
		UE_LOG(LogAimAssist, Error, TEXT("[UAimAssistInputModifier::UpdateTargetData] Invalid player controller in owner view data!"));
		return;
	}
	
	UAimAssistTargetManagerComponent* TargetManager = nullptr;

	if (UWorld* World = OwnerViewData.PlayerController->GetWorld())
	{
		if (AGameStateBase* GameState = World->GetGameState())
		{
			TargetManager = GameState->FindComponentByClass<UAimAssistTargetManagerComponent>();	
		}
	}
	
	if (!TargetManager)
	{
		return;
	}

	// 交换缓存后根据当前视图和上一帧状态收集本帧可见目标。
	// Update the targets based on what is visible
	SwapTargetCaches();
	const TArray<FLyraAimAssistTarget>& OldTargetCache = GetPreviousTargetCache();
	TArray<FLyraAimAssistTarget>& NewTargetCache = GetCurrentTargetCache();
	
	TargetManager->GetVisibleTargets(Filter, Settings, OwnerViewData, OldTargetCache, NewTargetCache);

	//
	// 根据目标持续处于可见辅助区域的时间更新权重。
	// Update target weights.
	//
	float TotalAssistWeight = 0.0f;

	for (FLyraAimAssistTarget& Target : NewTargetCache)
	{
		if (Target.bUnderAssistOuterReticle && Target.bIsVisible)
		{
			const float MaxAssistTime = Settings.GetTargetWeightMaxTime();
			Target.AssistTime = FMath::Min((Target.AssistTime + DeltaTime), MaxAssistTime);
		}
		else
		{
			Target.AssistTime = FMath::Max((Target.AssistTime - DeltaTime), 0.0f);
		}

		// 通过权重曲线查询目标在辅助准星下停留指定时长后的贡献。
		// Look up assist weight based on how long the target has been under the assist reticle.
		Target.AssistWeight = Settings.GetTargetWeightForTime(Target.AssistTime);

		TotalAssistWeight += Target.AssistWeight;
	}

	// 将全部目标权重归一化，使多目标贡献总和为 1。
	// Normalize the weights.
	if (TotalAssistWeight > 0.0f)
	{
		for (FLyraAimAssistTarget& Target : NewTargetCache)
		{
			Target.AssistWeight = (Target.AssistWeight / TotalAssistWeight);
		}
	}
}

// 根据普通观察或 ADS 类型读取玩家灵敏度档位并通过数据表换算；配置缺失时返回对应默认缩放。
const float UAimAssistInputModifier::GetSensitivtyScalar(const ULyraSettingsShared* SharedSettings) const
{
	if (SharedSettings && SensitivityLevelTable)
	{
		const ELyraGamepadSensitivity Sens = TargetingType == ELyraTargetingType::Normal ? SharedSettings->GetGamepadLookSensitivityPreset() : SharedSettings->GetGamepadTargetingSensitivityPreset();
		return SensitivityLevelTable->SensitivtyEnumToFloat(Sens);
	}
	
	UE_LOG(LogAimAssist, Warning, TEXT("SensitivityLevelTable is null, using default value!"));
	return (TargetingType == ELyraTargetingType::Normal) ? 1.0f : 0.5f;	
}

// 汇总可见目标的加权跟随旋转、拉拽和减速强度，结合死区、灵敏度、FOV 与玩家输入输出每秒旋转速度。
FRotator UAimAssistInputModifier::UpdateRotationalVelocity(APlayerController* PC, float DeltaTime, FVector CurrentLookInputValue, FVector CurrentMoveInputValue)
{
	FRotator RotationalVelocity(ForceInitToZero);
	FRotator RotationNeeded(ForceInitToZero);
	
	float PullStrength = 0.0f;
	float SlowStrength = 0.0f;
	
	const TArray<FLyraAimAssistTarget>& TargetCache = GetCurrentTargetCache();

	float LookStickDeadzone = 0.25f;
	float MoveStickDeadzone = 0.25f;
	float SettingStrengthScalar = (TargetingType == ELyraTargetingType::Normal) ? 1.0f : 0.5f;

	if (ULyraLocalPlayer* LP = Cast<ULyraLocalPlayer>(PC->GetLocalPlayer()))
	{
		ULyraSettingsShared* SharedSettings = LP->GetSharedSettings();
		LookStickDeadzone = SharedSettings->GetGamepadLookStickDeadZone();
		MoveStickDeadzone = SharedSettings->GetGamepadMoveStickDeadZone();
		SettingStrengthScalar = GetSensitivtyScalar(SharedSettings);
	}
	
	for (const FLyraAimAssistTarget& Target : TargetCache)
	{
		if (Target.bUnderAssistOuterReticle && Target.bIsVisible)
		{
			// 按目标权重累加补偿目标移动与玩家移动所需的跟随旋转。
			// Add up total rotation needed to follow weighted targets based on target and player movement.
			RotationNeeded += (Target.GetRotationFromMovement(OwnerViewData) * Target.AssistWeight);

			float TargetPullStrength = 0.0f;
			float TargetSlowStrength = 0.0f;
			CalculateTargetStrengths(Target, TargetPullStrength, TargetSlowStrength);

			// 累加目标已经乘以 AssistWeight 的拉拽与减速贡献。
			// Add up total amount of weighted pull and slow from the targets.
			PullStrength += TargetPullStrength;
			SlowStrength += TargetSlowStrength;
		}
	}

	// 此处还可继续按当前武器、玩家移动状态或其他玩法因素缩放辅助强度。
	// You could also apply some scalars based on the current weapon that is equipped, the player's movement state,
	// or any other factors you want here
	PullStrength *= Settings.StrengthScale * SettingStrengthScalar;
	SlowStrength *= Settings.StrengthScale * SettingStrengthScalar;

	const float PullLerpRate = (PullStrength > LastPullStrength) ? Settings.PullLerpInRate.GetValue() : Settings.PullLerpOutRate.GetValue();
	if (PullLerpRate > 0.0f)
	{
		PullStrength = FMath::FInterpConstantTo(LastPullStrength, PullStrength, DeltaTime, PullLerpRate);
	}

	const float SlowLerpRate = (SlowStrength > LastSlowStrength) ? Settings.SlowLerpInRate.GetValue() : Settings.SlowLerpOutRate.GetValue();
	if (SlowLerpRate > 0.0f)
	{
		SlowStrength = FMath::FInterpConstantTo(LastSlowStrength, SlowStrength, DeltaTime, SlowLerpRate);
	}

	LastPullStrength = PullStrength;
	LastSlowStrength = SlowStrength;

	const bool bIsLookInputActive =  (CurrentLookInputValue.SizeSquared() > FMath::Square(LookStickDeadzone));
	const bool bIsMoveInputActive = (CurrentMoveInputValue.SizeSquared() > FMath::Square(MoveStickDeadzone));
	
	const bool bIsApplyingLookInput = (bIsLookInputActive || !Settings.bRequireInput);
	const bool bIsApplyingMoveInput = (bIsMoveInputActive || !Settings.bRequireInput);
	const bool bIsApplyingAnyInput = (bIsApplyingLookInput || bIsApplyingMoveInput);

	// 在允许拉拽且存在有效输入时，施加跟随目标移动所需的旋转补偿。
	// Apply pulling towards the target
	if (Settings.bApplyPull && bIsApplyingAnyInput && !FMath::IsNearlyZero(PullStrength))
	{
		// 拉拽量是维持准星跟随目标所需旋转的一定比例。
		// The amount of pull is a percentage of the rotation needed to stay on target.
		FRotator PullRotation = (RotationNeeded * PullStrength);

		if (!bIsApplyingLookInput && Settings.bApplyStrafePullScale)
		{
			// 玩家没有主动观察时按横移输入缩放拉拽，避免向前跑过目标时视角被突然带走。
			// Scale pull strength by amount of player strafe if the player isn't actively looking around.
			// This helps prevent view yanks when running forward past targets.
			float StrafePullScale = FMath::Abs(CurrentMoveInputValue.Y);
		
			PullRotation.Yaw *= StrafePullScale;
			PullRotation.Pitch *= StrafePullScale;
		}

		// 限制单帧最大拉拽旋转，避免强行扯动视角；上限按 FOV 缩放以保持缩放前后的体感一致。
		// Clamp the maximum amount of pull rotation to prevent it from yanking the player's view too much.
		// The clamped rate is scaled so it feels the same regardless of field of view.
		const float FOVScale = UAimAssistTargetManagerComponent::GetFOVScale(PC, ECommonInputType::Gamepad);
		const float PullMaxRotationRate = (Settings.PullMaxRotationRate.GetValue() * FOVScale);
		if (PullMaxRotationRate > 0.0f)
		{
			const float PullMaxRotation = (PullMaxRotationRate * DeltaTime);

			PullRotation.Yaw = FMath::Clamp(PullRotation.Yaw, -PullMaxRotation, PullMaxRotation);
			PullRotation.Pitch = FMath::Clamp(PullRotation.Pitch, -PullMaxRotation, PullMaxRotation);
		}

		RotationNeeded -= PullRotation;
		RotationalVelocity += (PullRotation * (1.0f / DeltaTime));
	}

	FRotator LookRates = GetLookRates(CurrentLookInputValue);

	// 玩家主动观察且目标有效时，降低基础观察旋转速率。
	// Apply slowing
	if (Settings.bApplySlowing && bIsApplyingLookInput && !FMath::IsNearlyZero(SlowStrength))
	{
		// 减速后的旋转速率按正常观察速率的一定比例计算。
		// The slowed rotation rate is a percentage of the normal look rotation rates.
		FRotator SlowRates = (LookRates * (1.0f - SlowStrength));

		const bool bUseDynamicSlow = true;

		if (Settings.bUseDynamicSlow)
		{
			const FRotator BoostRotation = (RotationNeeded * (1.0f / DeltaTime));

			const float YawDynamicBoost = (BoostRotation.Yaw * FMath::Sign(CurrentLookInputValue.X));
			if (YawDynamicBoost > 0.0f)
			{
				SlowRates.Yaw += YawDynamicBoost;
			}

			const float PitchDynamicBoost = (BoostRotation.Pitch * FMath::Sign(CurrentLookInputValue.Y));
			if (PitchDynamicBoost > 0.0f)
			{
				SlowRates.Pitch += PitchDynamicBoost;
			}
		}

		// 为减速后的速率设置下限，避免低灵敏度下过于迟滞；下限按 FOV 缩放以保持一致体感。
		// Clamp the minimum amount of slow to prevent it from feeling sluggish on low sensitivity settings.
		// The clamped rate is scaled so it feels the same regardless of field of view.
		const float FOVScale = UAimAssistTargetManagerComponent::GetFOVScale(PC, ECommonInputType::Gamepad);
		const float SlowMinRotationRate = (Settings.SlowMinRotationRate.GetValue() * FOVScale);
		if (SlowMinRotationRate > 0.0f)
		{
			SlowRates.Yaw = FMath::Max(SlowRates.Yaw, SlowMinRotationRate);
			SlowRates.Pitch = FMath::Max(SlowRates.Pitch, SlowMinRotationRate);
		}

		// 减速分支不能反而超过原始观察速率。
		// Make sure the slow rate isn't faster then our default.
		SlowRates.Yaw = FMath::Min(SlowRates.Yaw, LookRates.Yaw);
		SlowRates.Pitch = FMath::Min(SlowRates.Pitch, LookRates.Pitch);

		RotationalVelocity.Yaw += (CurrentLookInputValue.X * SlowRates.Yaw);
		RotationalVelocity.Pitch += (CurrentLookInputValue.Y * SlowRates.Pitch);
		RotationalVelocity.Roll = 0.0f;
	}
	else
	{
		RotationalVelocity.Yaw += (CurrentLookInputValue.X * LookRates.Yaw);
		RotationalVelocity.Pitch += (CurrentLookInputValue.Y * LookRates.Pitch);
		RotationalVelocity.Roll = 0.0f;
	}

	return RotationalVelocity;
}

// 按目标位于内圈或外圈以及当前腰射/ADS 状态选取强度，并乘以该目标归一化后的 AssistWeight。
void UAimAssistInputModifier::CalculateTargetStrengths(const FLyraAimAssistTarget& Target, float& OutPullStrength, float& OutSlowStrength) const
{
	const bool bIsADS = (TargetingType == ELyraTargetingType::ADS);
	
	if (Target.bUnderAssistInnerReticle)
	{
		if (bIsADS)
		{
			OutPullStrength = Settings.PullInnerStrengthAds.GetValue();
			OutSlowStrength = Settings.SlowInnerStrengthAds.GetValue();
		}
		else
		{
			OutPullStrength = Settings.PullInnerStrengthHip.GetValue();
			OutSlowStrength = Settings.SlowInnerStrengthHip.GetValue();
		}
	}
	else if (Target.bUnderAssistOuterReticle)
	{
		if (bIsADS)
		{
			OutPullStrength = Settings.PullOuterStrengthAds.GetValue();
			OutSlowStrength = Settings.SlowOuterStrengthAds.GetValue();
		}
		else
		{
			OutPullStrength = Settings.PullOuterStrengthHip.GetValue();
			OutSlowStrength = Settings.SlowOuterStrengthHip.GetValue();
		}
	}
	else
	{
		OutPullStrength = 0.0f;
		OutSlowStrength = 0.0f;
	}

	OutPullStrength *= Target.AssistWeight;
	OutSlowStrength *= Target.AssistWeight;
}

#if ENABLE_DRAW_DEBUG
// 在调试 Canvas 上绘制强度、输入、三层准星边界以及每个目标的可见性、权重、距离、评分和计时。
void UAimAssistInputModifier::AimAssistDebugDraw(UCanvas* Canvas, APlayerController* PC)
{
	if (!Canvas || !OwnerViewData.IsDataValid() || !LyraConsoleVariables::bDrawAimAssistDebug)
	{
		return;
	}

	const bool bIsADS = (TargetingType == ELyraTargetingType::ADS);
	
	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.Initialize(Canvas, GEngine->GetSmallFont(), FVector2D((bIsADS ? 4.0f : 170.0f), 150.0f));
	DisplayDebugManager.SetDrawColor(FColor::Yellow);

	DisplayDebugManager.DrawString(FString(TEXT("------------------------------")));
	DisplayDebugManager.DrawString(FString(TEXT("Aim Assist Debug Draw")));
	DisplayDebugManager.DrawString(FString(TEXT("------------------------------")));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("Strength Scale: (%.4f)"), Settings.StrengthScale));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("Pull Strength: (%.4f)"), LastPullStrength));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("Slow Strength: (%.4f)"), LastSlowStrength));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("Look Rate Yaw: (%.4f)"), LastLookRateYaw));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("Look Rate Pitch: (%.4f)"), LastLookRatePitch));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("Baseline Value: (%.4f, %.4f, %.4f)"), LastBaselineValue.X, LastBaselineValue.Y, LastBaselineValue.Z));
	DisplayDebugManager.DrawString(FString::Printf(TEXT("Assisted Value: (%.4f, %.4f, %.4f)"), LastOutValue.X, LastOutValue.Y, LastOutValue.Z));

	
	UWorld* World = OwnerViewData.PlayerController->GetWorld();
	check(World);

	const FBox2D AssistInnerReticleBounds = OwnerViewData.ProjectReticleToScreen(Settings.AssistInnerReticleWidth.GetValue(), Settings.AssistInnerReticleHeight.GetValue(), Settings.ReticleDepth);
	const FBox2D AssistOuterReticleBounds = OwnerViewData.ProjectReticleToScreen(Settings.AssistOuterReticleWidth.GetValue(), Settings.AssistOuterReticleHeight.GetValue(), Settings.ReticleDepth);
	const FBox2D TargetingReticleBounds = OwnerViewData.ProjectReticleToScreen(Settings.TargetingReticleWidth.GetValue(), Settings.TargetingReticleHeight.GetValue(), Settings.ReticleDepth);

	if (TargetingReticleBounds.bIsValid)
	{
		FLinearColor ReticleColor(0.25f, 0.25f, 0.25f, 1.0f);
		DrawDebugCanvas2DBox(Canvas, TargetingReticleBounds, ReticleColor, 1.0f);	
	}

	if (AssistInnerReticleBounds.bIsValid)
	{
		FLinearColor ReticleColor(0.0f, 0.0f, 1.0f, 0.2f);

		FCanvasTileItem ReticleTileItem(AssistInnerReticleBounds.Min, AssistInnerReticleBounds.GetSize(), ReticleColor);
		ReticleTileItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(ReticleTileItem);

		ReticleColor.A = 1.0f;
		DrawDebugCanvas2DBox(Canvas, AssistInnerReticleBounds, ReticleColor, 1.0f);
	}

	if (AssistOuterReticleBounds.bIsValid)
	{
		FLinearColor ReticleColor(0.25f, 0.25f, 1.0f, 0.2f);

		FCanvasTileItem ReticleTileItem(AssistOuterReticleBounds.Min, AssistOuterReticleBounds.GetSize(), ReticleColor);
		ReticleTileItem.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(ReticleTileItem);

		ReticleColor.A = 1.0f;
		DrawDebugCanvas2DBox(Canvas, AssistOuterReticleBounds, ReticleColor, 1.0f);
	}

	const TArray<FLyraAimAssistTarget>& TargetCache = GetCurrentTargetCache();
	for (const FLyraAimAssistTarget& Target : TargetCache)
	{
		if (Target.ScreenBounds.bIsValid)
		{
			FLinearColor TargetColor = ((Target.AssistWeight > 0.0f) ? FLinearColor::LerpUsingHSV(FLinearColor::Yellow, FLinearColor::Green, Target.AssistWeight) : FLinearColor::Black);
			TargetColor.A = 0.2f;

			FCanvasTileItem TargetTileItem(Target.ScreenBounds.Min, Target.ScreenBounds.GetSize(), TargetColor);
			TargetTileItem.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(TargetTileItem);

			if (Target.bIsVisible)
			{
				TargetColor.A = 1.0f;
				DrawDebugCanvas2DBox(Canvas, Target.ScreenBounds, TargetColor, 1.0f);
			}

			FCanvasTextItem TargetTextItem(FVector2D::ZeroVector, FText::FromString(FString::Printf(TEXT("Weight: %.2f\nDist: %.2f\nScore: %.2f\nTime: %.2f"), Target.AssistWeight, Target.ViewDistance, Target.SortScore, Target.AssistTime)), GEngine->GetSmallFont(), FLinearColor::White);
			TargetTextItem.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(TargetTextItem, FVector2D(FMath::CeilToFloat(Target.ScreenBounds.Min.X), FMath::CeilToFloat(Target.ScreenBounds.Min.Y)));
		}
	}
}
#endif	// ENABLE_DRAW_DEBUG
