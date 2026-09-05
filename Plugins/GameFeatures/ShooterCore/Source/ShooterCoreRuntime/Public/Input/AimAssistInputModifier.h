// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Math/IntRect.h"
#include "ScalableFloat.h"
#include "WorldCollision.h"
#include "Input/LyraInputModifiers.h"
#include "DrawDebugHelpers.h"
#include "AimAssistInputModifier.generated.h"

class APlayerController;
class UInputAction;
class ULocalPlayer;
class UShapeComponent;
class ULyraAimSensitivityData;
class ULyraSettingsShared;

DECLARE_LOG_CATEGORY_EXTERN(LogAimAssist, Log, All);

/** 缓存当前 Pawn、PlayerController 和本地视口的投影数据，供目标发现与屏幕空间计算复用。 */
/** A container for some commonly used viewport data based on the current pawn */
struct FAimAssistOwnerViewData
{
	FAimAssistOwnerViewData() { ResetViewData(); }

	/** 从当前 PlayerController 重建视图矩阵、投影矩阵、视口、玩家变换、帧间位移和队伍信息。 */
	/**
	 * Update the "owner" information based on our current player controller. This calculates and stores things like the view matrix
	 * and current rotation that is used to determine what targets are visible
	 */
	void UpdateViewData(const APlayerController* PC);

	/** 将所有视图缓存恢复为无有效控制器的默认状态。 */
	/** Reset all the properties on this set of data to their defaults */
	void ResetViewData();

	/** 仅当 PlayerController 和 LocalPlayer 都有效时返回 true。 */
	/** Returns true if this owner struct has a valid player controller */
	bool IsDataValid() const { return PlayerController != nullptr && LocalPlayer != nullptr; }

	FBox2D ProjectReticleToScreen(float ReticleWidth, float ReticleHeight, float ReticleDepth) const;
	FBox2D ProjectBoundsToScreen(const FBox& Bounds) const;
	FBox2D ProjectShapeToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const;
	FBox2D ProjectBoxToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const;
	FBox2D ProjectSphereToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const;
	FBox2D ProjectCapsuleToScreen(const FCollisionShape& Shape, const FVector& ShapeOrigin, const FTransform& WorldTransform) const;

	/** 用于取得相机、Pawn、视口和可见目标计算数据的 PlayerController。 */
	/** Pointer to the player controller that can be used to calculate the data we need to check for visible targets */
	const APlayerController* PlayerController = nullptr;

	const ULocalPlayer* LocalPlayer = nullptr;
	
	FMatrix ProjectionMatrix = FMatrix::Identity;
	
	FMatrix ViewProjectionMatrix = FMatrix::Identity;
	
	FIntRect ViewRect = FIntRect(0, 0, 0, 0);
	
	FTransform ViewTransform = FTransform::Identity;
	
	FVector ViewForward = FVector::ZeroVector;
	
	// 玩家变换使用 Pawn 的位置和 Controller 的旋转，以统一移动与视角空间。
	// Player transform is the actor's location and the controller's rotation.
	FTransform PlayerTransform = FTransform::Identity;
	
	FTransform PlayerInverseTransform = FTransform::Identity;

	/** 玩家位置相对上一帧的世界空间位移。 */
	/** The movement delta between the current frame and the last */
	FVector DeltaMovement = FVector::ZeroVector;

	/** 从 ALyraPlayerState 读取的所属队伍 ID；没有 PlayerState 时为 INDEX_NONE。 */
	/** The ID of the team that this owner is from. It is populated from the ALyraPlayerState. If the owner does not have a player state, then it will be INDEX_NONE */
	int32 TeamID = INDEX_NONE;
};

/** 可跨帧缓存的单个 Aim Assist 目标状态，包括屏幕边界、移动、可见性、权重和异步 Trace 句柄。 */
/** A container for keeping the state of targets between frames that can be cached */
USTRUCT(BlueprintType)
struct FLyraAimAssistTarget
{
	GENERATED_BODY()

	FLyraAimAssistTarget() { ResetTarget(); }

	bool IsTargetValid() const { return TargetShapeComponent.IsValid(); }

	void ResetTarget();

	FRotator GetRotationFromMovement(const FAimAssistOwnerViewData& OwnerInfo) const;
	
	TWeakObjectPtr<UShapeComponent> TargetShapeComponent;
	
	FVector Location = FVector::ZeroVector;
	FVector DeltaMovement = FVector::ZeroVector;
	FBox2D ScreenBounds;

	float ViewDistance = 0.0f;
	float SortScore = 0.0f;

	float AssistTime = 0.0f;
	float AssistWeight = 0.0f;

	FTraceHandle VisibilityTraceHandle;
	
	uint8 bIsVisible : 1;
	
	uint8 bUnderAssistInnerReticle : 1;
	
	uint8 bUnderAssistOuterReticle : 1;
	
protected:

	float CalculateRotationToTarget2D(float TargetX, float TargetY, float OffsetY) const;
};

/** 用于排除特定 Aim Assist 目标的筛选选项。 */
/** Options for filtering out certain aim assist targets */
USTRUCT(BlueprintType)
struct FAimAssistFilter
{
	GENERATED_BODY()

	FAimAssistFilter()
		: bIncludeSameFriendlyTargets(false)
		, bExcludeInstigator(true)
		, bExcludeAllAttachedToInstigator(false)
		, bExcludeRequester(true)
		, bExcludeAllAttachedToRequester(false)
		, bTraceComplexCollision(false)
		, bExcludeDeadOrDying(true)
	{}

	/** 为 true 时允许同队目标进入候选列表。 */
	/** If true, then we should include any targets even if they are on our team */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	uint8 bIncludeSameFriendlyTargets : 1;
	
	/** 从 Trace 中排除 RequestedBy 的 Instigator Actor。 */
	/** Exclude 'RequestedBy->Instigator' Actor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint8 bExcludeInstigator : 1;
	
	/** 从 Trace 中排除附加到 RequestedBy 的 Instigator 上的全部 Actor。 */
	/** Exclude all actors attached to 'RequestedBy->Instigator' Actor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint32 bExcludeAllAttachedToInstigator : 1;

	/** 从 Trace 中排除发起查询的 RequestedBy Actor。 */
	/** Exclude 'RequestedBy Actor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint8 bExcludeRequester : 1;
	
	/** 从 Trace 中排除附加到 RequestedBy 上的全部 Actor。 */
	/** Exclude all actors attached to 'RequestedBy Actor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint8 bExcludeAllAttachedToRequester : 1;
	
	/** 是否使用复杂碰撞执行目标可见性 Trace。 */
	/** Trace against complex collision. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint8 bTraceComplexCollision : 1;
	
	/** 是否排除带有死亡或濒死状态的目标。 */
	/** Exclude all dead or dying targets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TargetSelection)
	uint8 bExcludeDeadOrDying : 1;

	/** 排除所属 Actor 类型命中集合中任一类的目标。 */
	/** Any target whose owning actor is of this type will be excluded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSet<TObjectPtr<UClass>> ExcludedClasses;

	/** 排除携带任一指定 GameplayTag 的目标。 */
	/** Targets with any of these tags will be excluded. */
	FGameplayTagContainer ExclusionGameplayTags;

	/** 排除超出该世界空间距离的目标。 */
	/** Any target outside of this range will be excluded */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	double TargetRange = 10000.0;
};

/** 控制存在有效目标时 Aim Assist 的候选准星、拉拽、减速、评分和插值行为。 */
/** Settings for how aim assist should behave when there are active targets */
USTRUCT(BlueprintType)
struct FAimAssistSettings
{
	GENERATED_BODY()

	FAimAssistSettings();

	float GetTargetWeightForTime(float Time) const;
	float GetTargetWeightMaxTime() const;
	
	// Aim Assist 内圈准星在参考深度处的世界空间宽度。
	// Width of aim assist inner reticle in world space.
	UPROPERTY(EditAnywhere)
	FScalableFloat AssistInnerReticleWidth;

	// Aim Assist 内圈准星在参考深度处的世界空间高度。
	// Height of aim assist inner reticle in world space.
	UPROPERTY(EditAnywhere)
	FScalableFloat AssistInnerReticleHeight;

	// Aim Assist 外圈准星在参考深度处的世界空间宽度。
	// Width of aim assist outer reticle in world space.
	UPROPERTY(EditAnywhere)
	FScalableFloat AssistOuterReticleWidth;

	// Aim Assist 外圈准星在参考深度处的世界空间高度。
	// Height of aim assist outer reticle in world space.
	UPROPERTY(EditAnywhere)
	FScalableFloat AssistOuterReticleHeight;

	// 用于初始收集候选目标的目标准星世界空间宽度。
	// Width of targeting reticle in world space.
	UPROPERTY(EditAnywhere)
	FScalableFloat TargetingReticleWidth;

	// 用于初始收集候选目标的目标准星世界空间高度。
	// Height of targeting reticle in world space.
	UPROPERTY(EditAnywhere)
	FScalableFloat TargetingReticleHeight;

	// 从玩家相机向前收集潜在目标的距离；会按 FOV 缩放，以保持不同视野角下相近的屏幕覆盖范围。
	// Range from player's camera used to gather potential targets.
	// Note: This is scaled using the field of view in order to limit targets by their screen size.
	UPROPERTY(EditAnywhere)
	FScalableFloat TargetRange;

	// 目标持续处于辅助区域的时间到权重映射，0 表示无贡献，1 表示最大贡献。
	// How much weight the target has based on the time it has been targeted.  (0 = None, 1 = Max)
	UPROPERTY(EditAnywhere)
	TObjectPtr<const UCurveFloat> TargetWeightCurve = nullptr;

	// 腰射时目标位于内圈准星内，目标与玩家移动对拉拽旋转的贡献比例。
	// How much target and player movement contributes to the aim assist pull when target is under the inner reticle. (0 = None, 1 = Max)
	UPROPERTY(EditAnywhere)
	FScalableFloat PullInnerStrengthHip;

	// 腰射时目标仅位于外圈准星内，目标与玩家移动对拉拽旋转的贡献比例。
	// How much target and player movement contributes to the aim assist pull when target is under the outer reticle. (0 = None, 1 = Max)
	UPROPERTY(EditAnywhere)
	FScalableFloat PullOuterStrengthHip;

	// ADS 时目标位于内圈准星内，目标与玩家移动对拉拽旋转的贡献比例。
	// How much target and player movement contributes to the aim assist pull when target is under the inner reticle. (0 = None, 1 = Max)
	UPROPERTY(EditAnywhere)
	FScalableFloat PullInnerStrengthAds;

	// ADS 时目标仅位于外圈准星内，目标与玩家移动对拉拽旋转的贡献比例。
	// How much target and player movement contributes to the aim assist pull when target is under the outer reticle. (0 = None, 1 = Max)
	UPROPERTY(EditAnywhere)
	FScalableFloat PullOuterStrengthAds;

	// 拉拽强度增大时使用的指数插值速率；设为 0 时不执行平滑渐入。
	// Exponential interpolation rate used to ramp up the pull strength.  Set to '0' to disable.
	UPROPERTY(EditAnywhere)
	FScalableFloat PullLerpInRate;

	// 拉拽强度减小时使用的指数插值速率；设为 0 时不执行平滑渐出。
	// Exponential interpolation rate used to ramp down the pull strength.  Set to '0' to disable.
	UPROPERTY(EditAnywhere)
	FScalableFloat PullLerpOutRate;

	// Aim Assist 拉拽允许增加的最大旋转速率；设为 0 时不限制，并按 FOV 缩放以维持缩放前后的手感。
	// Rotation rate maximum cap on amount of aim assist pull.  Set to '0' to disable.
	// Note: This is scaled based on the field of view so it feels the same regardless of zoom.
	UPROPERTY(EditAnywhere)
	FScalableFloat PullMaxRotationRate;

	// 腰射时目标位于内圈准星内，对期望转向速率施加的减速比例。
	// Amount of aim assist slow applied to desired turn rate when target is under the inner reticle. (0 = None, 1 = Max)
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowInnerStrengthHip;

	// 腰射时目标仅位于外圈准星内，对期望转向速率施加的减速比例。
	// Amount of aim assist slow applied to desired turn rate when target is under the outer reticle. (0 = None, 1 = Max)
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowOuterStrengthHip;

	// ADS 时目标位于内圈准星内，对期望转向速率施加的减速比例。
	// Amount of aim assist slow applied to desired turn rate when target is under the inner reticle. (0 = None, 1 = Max)
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowInnerStrengthAds;

	// ADS 时目标仅位于外圈准星内，对期望转向速率施加的减速比例。
	// Amount of aim assist slow applied to desired turn rate when target is under the outer reticle. (0 = None, 1 = Max)
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowOuterStrengthAds;

	// 减速强度增大时使用的指数插值速率；设为 0 时不执行平滑渐入。
	// Exponential interpolation rate used to ramp up the slow strength.  Set to '0' to disable.
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowLerpInRate;

	// 减速强度减小时使用的指数插值速率；设为 0 时不执行平滑渐出。
	// Exponential interpolation rate used to ramp down the slow strength.  Set to '0' to disable.
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowLerpOutRate;

	// Aim Assist 减速后仍允许保留的最小旋转速率；设为 0 时不限制，并按 FOV 缩放以维持缩放前后的手感。
	// Rotation rate minimum cap on amount to aim assist slow.  Set to '0' to disable.
	// Note: This is scaled based on the field of view so it feels the same regardless of zoom.
	UPROPERTY(EditAnywhere)
	FScalableFloat SlowMinRotationRate;
	
	/** 每帧最多进行精确可见性检测并参与辅助计算的目标数量。 */
	/** The maximum number of targets that can be considered during a given frame. */
	UPROPERTY(EditAnywhere)
	int32 MaxNumberOfTargets = 6;

	/** 将世界空间准星投影到屏幕时使用的参考深度。 */
	/**  */
	UPROPERTY(EditAnywhere)
	float ReticleDepth = 3000.0f;

	UPROPERTY(EditAnywhere)
	float TargetScore_AssistWeight = 10.0f;

	UPROPERTY(EditAnywhere)
	float TargetScore_ViewDot = 50.0f;

	UPROPERTY(EditAnywhere)
	float TargetScore_ViewDotOffset = 40.0f;

	UPROPERTY(EditAnywhere)
	float TargetScore_ViewDistance = 0.25f;

	UPROPERTY(EditAnywhere)
	float StrengthScale = 1.0f;

	/** 是否启用跨帧异步可见性 Trace；关闭时使用同步 Trace。 */
	/** Enabled/Disable asynchronous visibility traces. */
	UPROPERTY(EditAnywhere)
	uint8 bEnableAsyncVisibilityTrace : 1;

	/** 是否要求玩家当前存在观察输入才应用 Aim Assist。 */
	/** Whether or not we require input for aim assist to be applied */
	UPROPERTY(EditAnywhere)
	uint8 bRequireInput : 1;

	/** 是否应用朝目标移动方向补偿的拉拽旋转。 */
	/** Whether or not pull should be applied to aim assist */
	UPROPERTY(EditAnywhere)
	uint8 bApplyPull : 1;

	/** 玩家未主动观察时，是否按横移输入缩放拉拽，以减少跑过目标时的视角突跳。 */
	/** Whether or not to apply a strafe pull based off of movement input */
	UPROPERTY(EditAnywhere)
	uint8 bApplyStrafePullScale : 1;
	
	/** 是否在准星经过目标时降低玩家观察旋转速率。 */
	/** Whether or not to apply a slowing effect during aim assist */
	UPROPERTY(EditAnywhere)
	uint8 bApplySlowing : 1;

	/** 是否根据观察输入方向和强度动态调整减速效果。 */
	/** Whether or not to apply a dynamic slow effect based off of look input */
	UPROPERTY(EditAnywhere)
	uint8 bUseDynamicSlow : 1;

	/** 是否按摇杆径向偏转在 Yaw 与 Pitch 观察速率之间混合，以保持对角输入方向准确。 */
	/** Whether or not look rates should blend between yaw and pitch based on stick deflection using radial look rates */
	UPROPERTY(EditAnywhere)
	uint8 bUseRadialLookRates : 1;
};

/** 使用目标拉拽和旋转减速改善手柄玩家瞄准体验的 Enhanced Input Modifier。 */
/**
 * An input modifier to help gamepad players have better targeting.
 */
UCLASS()
class UAimAssistInputModifier : public UInputModifier
{
	GENERATED_BODY()
	
public:
		
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings, Config)
	FAimAssistSettings Settings {};

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings, Config)
	FAimAssistFilter Filter {};

	/** 表示玩家实际移动输入的 InputAction，用于计算横移拉拽缩放。 */
	/** The input action that represents the actual movement of the player */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	TObjectPtr<const UInputAction> MoveInputAction = nullptr;
	
	/** 从共享设置查询灵敏度时使用的瞄准类型，例如普通观察或 ADS。 */
	/** The type of targeting to use for this Sensitivity */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings, Config)
	ELyraTargetingType TargetingType = ELyraTargetingType::Normal;

	/** 把玩家灵敏度档位映射到浮点缩放值的数据资产。 */
	/** Asset that gives us access to the float scalar value being used for sensitivty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AssetBundles="Client,Server"))
	TObjectPtr<const ULyraAimSensitivityData> SensitivityLevelTable = nullptr;
	
protected:
	
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;

	/** 交换双缓冲目标缓存，重新收集当前可见目标，并计算各目标用于拉拽和减速的时间权重。 */
	/**
	* Swaps the target cache's and determines what targets are currently visible.
	* Updates the score of each target to determine
	* how much pull/slow effect should be applied to each
	*/
	void UpdateTargetData(float DeltaTime);

	FRotator UpdateRotationalVelocity(APlayerController* PC, float DeltaTime, FVector CurrentLookInputValue, FVector CurrentMoveInputValue);

	/** 根据目标位于内外准星的状态以及腰射/ADS 参数，计算该目标的拉拽和减速强度。 */
	/** Calcualte the pull and slow strengh of a given target */
	void CalculateTargetStrengths(const FLyraAimAssistTarget& Target, float& OutPullStrength, float& OutSlowStrength) const;

	FRotator GetLookRates(const FVector& LookInput);
	
	void SwapTargetCaches() { TargetCacheIndex ^= 1; }
	const TArray<FLyraAimAssistTarget>& GetPreviousTargetCache() const	{ return ((TargetCacheIndex == 0) ? TargetCache1 : TargetCache0); }
	TArray<FLyraAimAssistTarget>& GetPreviousTargetCache()				{ return ((TargetCacheIndex == 0) ? TargetCache1 : TargetCache0); }

	const TArray<FLyraAimAssistTarget>& GetCurrentTargetCache() const	{ return ((TargetCacheIndex == 0) ? TargetCache0 : TargetCache1); }
	TArray<FLyraAimAssistTarget>& GetCurrentTargetCache()				{ return ((TargetCacheIndex == 0) ? TargetCache0 : TargetCache1); }

	bool HasAnyCurrentTargets() const { return !GetCurrentTargetCache().IsEmpty(); }

	const float GetSensitivtyScalar(const ULyraSettingsShared* SharedSettings) const;
	
	// 双缓冲保存当前帧与上一帧目标状态，以继承移动、权重和异步可见性 Trace。
	// Tracking of the current and previous frame's targets
	UPROPERTY()
	TArray<FLyraAimAssistTarget> TargetCache0;

	UPROPERTY()
	TArray<FLyraAimAssistTarget> TargetCache1;

	/** 当前写入目标缓存的索引；每次更新前在 0 和 1 之间切换。 */
	/** The current in use target cache */
	uint32 TargetCacheIndex;

	FAimAssistOwnerViewData OwnerViewData;

	float LastPullStrength = 0.0f;
	float LastSlowStrength = 0.0f;
	
#if ENABLE_DRAW_DEBUG
	float LastLookRateYaw;
	float LastLookRatePitch;

	FVector LastOutValue;
	FVector LastBaselineValue;

	// TODO：移除此状态，并把 Aim Assist 调试可视化从输入修改器中拆分出去。
	// TODO: Remove this variable and move debug visualization out of this 
	bool bRegisteredDebug = false;

	void AimAssistDebugDraw(class UCanvas* Canvas, APlayerController* PC);
	FDelegateHandle	DebugDrawHandle;
#endif
};
