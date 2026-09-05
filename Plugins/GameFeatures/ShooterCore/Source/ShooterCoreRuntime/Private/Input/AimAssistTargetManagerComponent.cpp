// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/AimAssistTargetManagerComponent.h"
#include "CommonInputTypeEnum.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/Character.h"
#include "GameFramework/InputSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Input/AimAssistInputModifier.h"
#include "Player/LyraPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Input/IAimAssistTargetInterface.h"
#include "ShooterCoreRuntimeSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AimAssistTargetManagerComponent)

namespace LyraConsoleVariables
{
	// 控制是否绘制用于初步 Overlap 候选收集的 Aim Assist 视锥包围盒。
	static bool bDrawDebugViewfinder = false;
	// 将调试包围盒开关暴露为作弊控制台变量。
	static FAutoConsoleVariableRef CVarDrawDebugViewfinder(
		TEXT("lyra.Weapon.AimAssist.DrawDebugViewfinder"),
		bDrawDebugViewfinder,
		TEXT("Should we draw a debug box for the aim assist target viewfinder?"),
		ECVF_Cheat);
}

// 按 ShapeComponent 身份在上一帧缓存中查找同一 Aim Assist 目标，以继承移动、权重和异步 Trace 状态。
const FLyraAimAssistTarget* FindTarget(const TArray<FLyraAimAssistTarget>& Targets, const UShapeComponent* TargetComponent)
{
	const FLyraAimAssistTarget* FoundTarget = Targets.FindByPredicate(
	[&TargetComponent](const FLyraAimAssistTarget& Target)
	{
		return (Target.TargetShapeComponent == TargetComponent);
	});

	return FoundTarget;
}

// 提取目标支持的 Box/Sphere/Capsule 形状和用于屏幕投影的平滑变换；远端角色胶囊改用经过网络平滑的 Mesh 位置，无效或近零形状返回 false。
static bool GatherTargetInfo(const AActor* Actor, const UShapeComponent* ShapeComponent, FTransform& OutTransform, FCollisionShape& OutShape, FVector& OutShapeOrigin)
{
	check(Actor);
	check(ShapeComponent);

	const FCollisionShape TargetShape = ShapeComponent->GetCollisionShape();
	const bool bIsValidShape = (TargetShape.IsBox() || TargetShape.IsSphere() || TargetShape.IsCapsule());

	if (!bIsValidShape || TargetShape.IsNearlyZero())
	{
		return false;
	}

	FTransform TargetTransform;
	FVector TargetShapeOrigin(ForceInitToZero);

	if (const ACharacter* TargetCharacter = Cast<ACharacter>(Actor))
	{
		if (ShapeComponent == TargetCharacter->GetCapsuleComponent())
		{
			// 远端角色的胶囊位置不会平滑移动，因此改用已经执行网络平滑的 SkeletalMesh 变换。
			// Character capsules don't move smoothly for remote players.  Use the mesh location since it's smoothed out.
			const USkeletalMeshComponent* TargetMesh = TargetCharacter->GetMesh();
			check(TargetMesh);

			TargetTransform = TargetMesh->GetComponentTransform();
			TargetShapeOrigin = -TargetCharacter->GetBaseTranslationOffset();
		}
		else
		{
			TargetTransform = ShapeComponent->GetComponentTransform();
		}
	}
	else
	{
		TargetTransform = ShapeComponent->GetComponentTransform();
	}

	OutTransform = TargetTransform;
	OutShape = TargetShape;
	OutShapeOrigin = TargetShapeOrigin;

	return true;
}


// 从玩家前方 Aim Assist 通道收集候选，执行接口筛选、形状屏幕投影和评分排序，仅对最高分的有限目标进行遮挡检测并输出跨帧状态。
void UAimAssistTargetManagerComponent::GetVisibleTargets(const FAimAssistFilter& Filter, const FAimAssistSettings& Settings, const FAimAssistOwnerViewData& OwnerData, const TArray<FLyraAimAssistTarget>& OldTargets, OUT TArray<FLyraAimAssistTarget>& OutNewTargets)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAimAssistTargetManagerComponent::GetVisibleTargets);
	OutNewTargets.Reset();
	const APlayerController* PC = OwnerData.PlayerController;
	
	if (!PC)
	{
		UE_LOG(LogAimAssist, Error, TEXT("Invalid player controller passed to GetVisibleTargets!"));
		return;
	}

	const APawn* OwnerPawn = PC->GetPawn();

	if (!OwnerPawn)
	{
		UE_LOG(LogAimAssist, Error, TEXT("Could not find a valid pawn for aim assist!"));
		return;	
	}
	
	const FVector ViewLocation = OwnerData.ViewTransform.GetTranslation();
	const FVector ViewForward = OwnerData.ViewTransform.GetUnitAxis(EAxis::X);

	const float FOVScale = GetFOVScale(PC, ECommonInputType::Gamepad);
	const float InvFieldOfViewScale = (FOVScale > 0.0f) ? (1.0f / FOVScale) : 1.0f;
	const float TargetRange = (Settings.TargetRange.GetValue() * InvFieldOfViewScale);

	// 按 FOV 缩放准星投影深度，使不同缩放状态下准星保持近似相同的屏幕尺寸。
	// Use the field of view to scale the reticle projection.  This maintains the same reticle size regardless of field of view.
	const float ReticleDepth = (Settings.ReticleDepth * InvFieldOfViewScale);

	// 分别计算辅助内圈、外圈和候选目标准星的屏幕空间边界。
	// Calculate the bounds of this reticle in screen space
	const FBox2D AssistInnerReticleBounds = OwnerData.ProjectReticleToScreen(Settings.AssistInnerReticleWidth.GetValue(), Settings.AssistInnerReticleHeight.GetValue(), ReticleDepth);
	const FBox2D AssistOuterReticleBounds = OwnerData.ProjectReticleToScreen(Settings.AssistOuterReticleWidth.GetValue(), Settings.AssistOuterReticleHeight.GetValue(), ReticleDepth);
	const FBox2D TargetingReticleBounds = OwnerData.ProjectReticleToScreen(Settings.TargetingReticleWidth.GetValue(), Settings.TargetingReticleHeight.GetValue(), ReticleDepth);

	static TArray<FOverlapResult> OverlapResults;
	// 在 Aim Assist 通道用朝向玩家视角的 Box Overlap 粗略收集前方潜在目标。
	// Do a world trace on the Aim Assist channel to get any visible targets
	{
		UWorld* World = GetWorld();
		
		OverlapResults.Reset();

		const FVector PawnLocation = OwnerPawn->GetActorLocation();
		ECollisionChannel AimAssistChannel = GetAimAssistChannel();
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AimAssist_QueryTargetsInRange), true);
		Params.AddIgnoredActor(OwnerPawn);

		// MakeBox 接收半尺寸，因此深度、宽度和高度都乘以 0.5。
		// Need to multiply these by 0.5 because MakeBox takes in half extents
		FCollisionShape BoxShape = FCollisionShape::MakeBox(FVector3f(ReticleDepth * 0.5f, Settings.AssistOuterReticleWidth.GetValue() * 0.5f, Settings.AssistOuterReticleHeight.GetValue() * 0.5f));						
		World->OverlapMultiByChannel(OUT OverlapResults, PawnLocation, OwnerData.PlayerTransform.GetRotation(), AimAssistChannel, BoxShape, Params);

#if ENABLE_DRAW_DEBUG && !UE_BUILD_SHIPPING
		if(LyraConsoleVariables::bDrawDebugViewfinder)
		{
			DrawDebugBox(World, PawnLocation, BoxShape.GetBox(), OwnerData.PlayerTransform.GetRotation(), FColor::Red);	
		}
#endif
	}

	// 从 Overlap 命中的 Actor 和 Component 中收集所有实现 IAimAssistTarget 的目标选项。
	// Gather target options from any visibile hit results that implement the IAimAssistTarget interface
	TArray<FAimAssistTargetOptions> NewTargetData;
	{
		for (const FOverlapResult& Overlap : OverlapResults)
		{
			TScriptInterface<IAimAssistTaget> TargetActor(Overlap.GetActor());
			if (TargetActor)
			{
				FAimAssistTargetOptions TargetData;
				TargetActor->GatherTargetOptions(TargetData);
				NewTargetData.Add(TargetData);
			}
			
			TScriptInterface<IAimAssistTaget> TargetComponent(Overlap.GetComponent());
			if (TargetComponent)
			{
				FAimAssistTargetOptions TargetData;
				TargetComponent->GatherTargetOptions(TargetData);
				NewTargetData.Add(TargetData);
			}			
		}
	}
	
	// 筛选玩家前方且屏幕边界与目标准星相交的目标，并构建本帧缓存。
	// Gather targets that are in front of the player
	{
		const FVector PawnLocation = OwnerPawn->GetActorLocation();		
		
		for (FAimAssistTargetOptions& AimAssistTarget : NewTargetData)
		{
			if (!DoesTargetPassFilter(OwnerData, Filter, AimAssistTarget, TargetRange))
			{
				continue;
			}
			
			AActor* OwningActor = AimAssistTarget.TargetShapeComponent->GetOwner();

			FTransform TargetTransform;
			FCollisionShape TargetShape;
			FVector TargetShapeOrigin;

			if (!GatherTargetInfo(OwningActor, AimAssistTarget.TargetShapeComponent.Get(), TargetTransform, TargetShape, TargetShapeOrigin))
			{
				continue;
			}
			
			const FVector TargetViewLocation = TargetTransform.TransformPositionNoScale(TargetShapeOrigin);
			const FVector TargetViewVector = (TargetViewLocation - ViewLocation);

			FVector TargetViewDirection;
			float TargetViewDistance;
			TargetViewVector.ToDirectionAndLength(TargetViewDirection, TargetViewDistance);
			const float TargetViewDot = FVector::DotProduct(TargetViewDirection, ViewForward);
			if (TargetViewDot <= 0.0f)
			{
				continue;
			}
			
			const FLyraAimAssistTarget* OldTarget = FindTarget(OldTargets, AimAssistTarget.TargetShapeComponent.Get());

			// 把目标碰撞形状投影为屏幕包围盒，用于准星相交判断。
			// Calculate the screen bounds for this target
			FBox2D TargetScreenBounds(ForceInitToZero);
			const bool bUpdateTargetProjections = true;
			if (bUpdateTargetProjections)
			{
				TargetScreenBounds = OwnerData.ProjectShapeToScreen(TargetShape, TargetShapeOrigin, TargetTransform);
			}
			else
			{
				// 若关闭本帧投影更新，则存在旧缓存时沿用上一帧屏幕边界。
				// Target projections are not being updated so use the values from the previous frame if the target existed.
				if (OldTarget)
				{
					TargetScreenBounds = OldTarget->ScreenBounds;
				}
			}

			if (!TargetScreenBounds.bIsValid)
			{
				continue;
			}

			if (!TargetingReticleBounds.Intersect(TargetScreenBounds))
			{
				continue;
			}

			FLyraAimAssistTarget NewTarget;

			NewTarget.TargetShapeComponent = AimAssistTarget.TargetShapeComponent;
			NewTarget.Location = TargetTransform.GetTranslation();
			NewTarget.ScreenBounds = TargetScreenBounds;
			NewTarget.ViewDistance = TargetViewDistance;
			NewTarget.bUnderAssistInnerReticle = AssistInnerReticleBounds.Intersect(TargetScreenBounds);
			NewTarget.bUnderAssistOuterReticle = AssistOuterReticleBounds.Intersect(TargetScreenBounds);
			
			// 从上一帧同一形状目标继承位移基准、辅助计时、权重和异步 Trace 句柄。
			// Transfer target data from last frame.
			if (OldTarget)
			{
				NewTarget.DeltaMovement = (NewTarget.Location - OldTarget->Location);
				NewTarget.AssistTime = OldTarget->AssistTime;
				NewTarget.AssistWeight = OldTarget->AssistWeight;
				NewTarget.VisibilityTraceHandle = OldTarget->VisibilityTraceHandle;
			}

			// 综合上一帧辅助权重、视线夹角和视距计算排序分数，优先保留稳定且靠近准星中心的目标。
			// Calculate a score used for sorting based on previous weight, distance from target, and distance from reticle.
			const float AssistWeightScore = (NewTarget.AssistWeight * Settings.TargetScore_AssistWeight);
			const float ViewDotScore = ((TargetViewDot * Settings.TargetScore_ViewDot) - Settings.TargetScore_ViewDotOffset);
			const float ViewDistanceScore = ((1.0f - (TargetViewDistance / TargetRange)) * Settings.TargetScore_ViewDistance);

			NewTarget.SortScore = (AssistWeightScore + ViewDotScore + ViewDistanceScore);

			OutNewTargets.Add(NewTarget);
		}
	}

	// 候选过多时按分数降序截断，限制每帧昂贵的可见性 Trace 数量。
	// Sort the targets by their score so if there are too many so we can limit the amount of visibility traces performed.
	if (OutNewTargets.Num() > Settings.MaxNumberOfTargets)
	{
		OutNewTargets.Sort([](const FLyraAimAssistTarget& TargetA, const FLyraAimAssistTarget& TargetB)
		{
			return (TargetA.SortScore > TargetB.SortScore);
		});
		
		OutNewTargets.SetNum(Settings.MaxNumberOfTargets, EAllowShrinking::No);
	}

	// 对最终候选执行从相机到目标视点的遮挡检测。
	// Do visibliity traces on the targets
	{
		for (FLyraAimAssistTarget& Target : OutNewTargets)
		{
			DetermineTargetVisibility(Target, Settings, Filter, OwnerData);
		}
	}
}

// 验证目标激活状态、形状、前向距离、阵营、生命状态、排除标签和排除类；任一条件不满足即返回 false。
bool UAimAssistTargetManagerComponent::DoesTargetPassFilter(const FAimAssistOwnerViewData& OwnerData, const FAimAssistFilter& Filter, const FAimAssistTargetOptions& Target, const float AcceptableRange) const
{
	const APawn* OwnerPawn = OwnerData.PlayerController ? OwnerData.PlayerController->GetPawn() : nullptr;
	
	if (!Target.bIsActive || !OwnerPawn || !Target.TargetShapeComponent.IsValid())
	{
		return false;
	}
	
	const AActor* TargetOwningActor = Target.TargetShapeComponent->GetOwner();
	check(TargetOwningActor);
	if (TargetOwningActor == OwnerPawn || TargetOwningActor == OwnerPawn->GetInstigator())
	{
		return false;
	}
	
	const FVector PawnLocation = OwnerPawn->GetActorLocation();
	
	// 使用目标在视线前向轴上的投影距离，同时排除身后目标和超过可接受范围的目标。
	// Do a distance check on the given actor
	const FVector TargetVector = TargetOwningActor->GetActorLocation() - PawnLocation;
	const float TargetViewDistanceCheck = FVector::DotProduct(OwnerData.ViewForward, TargetVector);

	if ((TargetViewDistanceCheck < 0.0f) || (TargetViewDistanceCheck > AcceptableRange))
	{
		return false;
	}
	
	if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetOwningActor))
	{
		// 未允许友军目标时，排除与 Owner 队伍 ID 相同的角色。
		// If the given target is on the same team as the owner, then exclude it from the search	
		if (!Filter.bIncludeSameFriendlyTargets)
		{
			if (const ALyraPlayerState* PS = TargetCharacter->GetPlayerState<ALyraPlayerState>())
			{
				if (PS->GetTeamId() == OwnerData.TeamID)
				{
					return false;
				}
			}
		}

		// 配置要求时通过 LyraHealthComponent 排除已经死亡或正在死亡的角色。
		// Exclude dead or dying characters
		if (Filter.bExcludeDeadOrDying)
		{
			if (const ULyraHealthComponent* HealthComponent = ULyraHealthComponent::FindHealthComponent(TargetCharacter))
			{
				if (HealthComponent->IsDeadOrDying())
				{
					return false;
				}
			}	
		}
	}

	// 目标命中任一排除 GameplayTag 时忽略。
	// If this target has any tags that the filter wants to exlclude, then ignore it
	if (Target.AssociatedTags.HasAny(Filter.ExclusionGameplayTags))
	{
		return false;
	}

	if (Filter.ExcludedClasses.Contains(TargetOwningActor->GetClass()))
	{
		return false;
	}

	return true;
}

// 根据相机 FOV 和输入设备返回旋转/投影缩放：手柄与触摸使用 tan(半 FOV) 比值，鼠标保持引擎旧有线性规则以避免改变既有灵敏度。
float UAimAssistTargetManagerComponent::GetFOVScale(const APlayerController* PC, ECommonInputType InputType)
{
	float FovScale = 1.0f;
	const UInputSettings* DefaultInputSettings = GetDefault<UInputSettings>();
	check(DefaultInputSettings && PC);

	if (PC->PlayerCameraManager && DefaultInputSettings->bEnableFOVScaling)
	{
		const float FOVAngle = PC->PlayerCameraManager->GetFOVAngle();
		switch (InputType)
		{
		case ECommonInputType::Gamepad:
		case ECommonInputType::Touch:
		{
			static const float PlayerInput_BaseFOV = 80.0f;
			// 手柄和触摸使用基于 tan(半 FOV) 的正确投影缩放；鼠标若切换到此算法会改变现有玩家灵敏度，因此继续使用兼容路径。
			// This is the proper way to scale based off FOV changes.
			// Ideally mouse would use this too but changing it now will cause sensitivity to change for existing players.
			const float BaseHalfFOV = PlayerInput_BaseFOV * 0.5f;
			const float HalfFOV = FOVAngle * 0.5f;
			const float BaseTanHalfFOV = FMath::Tan(FMath::DegreesToRadians(BaseHalfFOV));
			const float TanHalfFOV = FMath::Tan(FMath::DegreesToRadians(HalfFOV));

			check(BaseTanHalfFOV > 0.0f);
			FovScale = (TanHalfFOV / BaseTanHalfFOV);
			break;
		}
		case ECommonInputType::MouseAndKeyboard:
			FovScale = (DefaultInputSettings->FOVScale * FOVAngle);
			break;
		default:
			ensure(false);
			break;
		}
	}
	return FovScale;
}

// 从玩家视点到目标眼睛执行遮挡检测；可见目标可查询上一帧异步结果并为下一帧续发 Trace，否则同步检测并清除旧句柄。
void UAimAssistTargetManagerComponent::DetermineTargetVisibility(FLyraAimAssistTarget& Target, const FAimAssistSettings& Settings, const FAimAssistFilter& Filter, const FAimAssistOwnerViewData& OwnerData)
{
	UWorld* World = GetWorld();
	check(World);

	const AActor* Actor = Target.TargetShapeComponent->GetOwner();
	if (!Actor)
	{
		ensure(false);
		return;
	}

	FVector TargetEyeLocation;
	FRotator TargetEyeRotation;
	Actor->GetActorEyesViewPoint(TargetEyeLocation, TargetEyeRotation);
	
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AimAssist_DetermineTargetVisibility), true);
	InitTargetSelectionCollisionParams(QueryParams, *Actor, Filter);
	QueryParams.AddIgnoredActor(Actor);

	const UShooterCoreRuntimeSettings* ShooterSettings = GetDefault<UShooterCoreRuntimeSettings>();
	const ECollisionChannel AimAssistChannel = ShooterSettings->GetAimAssistCollisionChannel();
		
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);	
	ResponseParams.CollisionResponse.SetResponse(AimAssistChannel, ECR_Ignore);

	if (Target.bIsVisible && Settings.bEnableAsyncVisibilityTrace)
	{
		// 查询上一帧发起的异步可见性 Trace 结果；结果缺失时保守地标记为不可见。
		// Query for previous asynchronous trace result.
		if (Target.VisibilityTraceHandle.IsValid())
		{
			FTraceDatum TraceDatum;
			if (World->QueryTraceData(Target.VisibilityTraceHandle, TraceDatum))
			{
				Target.bIsVisible = (FHitResult::GetFirstBlockingHit(TraceDatum.OutHits) == nullptr);
			}
			else
			{
				UE_LOG(LogAimAssist, Warning, TEXT("UAimAssistTargetManagerComponent::DetermineTargetVisibility() - Failed to find async visibility trace data!"));
				Target.bIsVisible = false;
			}

			// 消费结果后立即清除旧异步 Trace 句柄。
			// Invalidate the async trace handle.
			Target.VisibilityTraceHandle = FTraceHandle();
		}

		// 仅在目标仍可见时为下一帧续发异步 Trace，已经遮挡的目标改由后续同步路径重新确认。
		// Only start a new asynchronous trace for next frame if the target is still visible.
		if (Target.bIsVisible)
		{
			Target.VisibilityTraceHandle = World->AsyncLineTraceByChannel(EAsyncTraceType::Test, OwnerData.ViewTransform.GetTranslation(), TargetEyeLocation, ECC_Visibility, QueryParams, ResponseParams);
		}
	}
	else
	{
		Target.bIsVisible = !World->LineTraceTestByChannel(OwnerData.ViewTransform.GetTranslation(), TargetEyeLocation, ECC_Visibility, QueryParams, ResponseParams);

		// 同步 Trace 不需要保留跨帧句柄，清除可能继承的旧值。
		// Invalidate the async trace handle.
		Target.VisibilityTraceHandle = FTraceHandle();		
	}
}

// 按 Filter 将请求者、Instigator 及其附加层级加入 Trace 忽略集合，并设置是否检测复杂碰撞。
void UAimAssistTargetManagerComponent::InitTargetSelectionCollisionParams(FCollisionQueryParams& OutParams, const AActor& RequestedBy, const FAimAssistFilter& Filter) const
{
	// 按配置排除发起查询的 Actor 本身。
	// Exclude Requester
	if (Filter.bExcludeRequester)
	{
		OutParams.AddIgnoredActor(&RequestedBy);
	}

	// 按配置排除所有附加到请求者的 Actor。
	// Exclude attached to Requester
	if (Filter.bExcludeAllAttachedToRequester)
	{
		TArray<AActor*> ActorsAttachedToRequester;
		RequestedBy.GetAttachedActors(ActorsAttachedToRequester);

		OutParams.AddIgnoredActors(ActorsAttachedToRequester);
	}

	if (Filter.bExcludeInstigator)
	{
		OutParams.AddIgnoredActor(RequestedBy.GetInstigator());
	}

	// 按配置排除所有附加到 Instigator 的 Actor。
	// Exclude attached to Instigator
	if (Filter.bExcludeAllAttachedToInstigator && RequestedBy.GetInstigator())
	{
		TArray<AActor*> ActorsAttachedToInstigator;
		RequestedBy.GetInstigator()->GetAttachedActors(ActorsAttachedToInstigator);

		OutParams.AddIgnoredActors(ActorsAttachedToInstigator);
	}

	OutParams.bTraceComplex = Filter.bTraceComplexCollision;
}

// 从 ShooterCoreRuntimeSettings 返回目标粗筛使用的通道，并在未配置的 ECC_MAX 情况下触发诊断。
ECollisionChannel UAimAssistTargetManagerComponent::GetAimAssistChannel() const
{
	const UShooterCoreRuntimeSettings* ShooterSettings = GetDefault<UShooterCoreRuntimeSettings>();
	const ECollisionChannel AimAssistChannel = ShooterSettings->GetAimAssistCollisionChannel();

	ensureMsgf(AimAssistChannel != ECollisionChannel::ECC_MAX, TEXT("The aim assist collision channel has not been set! Do this in the ShooterCoreRuntime plugin settings"));
	
	return AimAssistChannel;
}
