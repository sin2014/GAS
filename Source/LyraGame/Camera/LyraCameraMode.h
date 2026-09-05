// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "GameplayTagContainer.h"

#include "LyraCameraMode.generated.h"

#define UE_API LYRAGAME_API

class AActor;
class UCanvas;
class ULyraCameraComponent;

/**
 * 定义相机模式切换时由 BlendAlpha 计算 BlendWeight 的曲线类型。
 */
/**
 * ELyraCameraModeBlendFunction
 *
 *	Blend function used for transitioning between camera modes.
 */
UENUM(BlueprintType)
enum class ELyraCameraModeBlendFunction : uint8
{
	// 使用线性插值。
	// Does a simple linear interpolation.
	Linear,

	// 立即加速并在接近目标时平滑减速，缓动程度由指数控制。
	// Immediately accelerates, but smoothly decelerates into the target.  Ease amount controlled by the exponent.
	EaseIn,

	// 平滑加速，但到达目标前不减速，缓动程度由指数控制。
	// Smoothly accelerates, but does not decelerate into the target.  Ease amount controlled by the exponent.
	EaseOut,

	// 平滑加速并平滑减速，缓动程度由指数控制。
	// Smoothly accelerates and decelerates.  Ease amount controlled by the exponent.
	EaseInOut,

	COUNT	UMETA(Hidden)
};


/**
 * 单个相机模式生成的完整视图数据，也是模式栈逐层混合的基本单位。
 */
/**
 * FLyraCameraModeView
 *
 *	View data produced by the camera mode that is used to blend camera modes.
 */
struct FLyraCameraModeView
{
public:

	FLyraCameraModeView();

	void Blend(const FLyraCameraModeView& Other, float OtherWeight);

public:

	FVector Location;
	FRotator Rotation;
	FRotator ControlRotation;
	float FieldOfView;
};


/**
 * 所有相机模式的抽象基类，负责生成视图并维护自身的混合进度。
 */
/**
 * ULyraCameraMode
 *
 *	Base class for all camera modes.
 */
UCLASS(MinimalAPI, Abstract, NotBlueprintable)
class ULyraCameraMode : public UObject
{
	GENERATED_BODY()

public:

	UE_API ULyraCameraMode();

	UE_API ULyraCameraComponent* GetLyraCameraComponent() const;

	UE_API virtual UWorld* GetWorld() const override;

	UE_API AActor* GetTargetActor() const;

	const FLyraCameraModeView& GetCameraModeView() const { return View; }

	// 该模式首次加入激活的相机模式栈时调用。
	// Called when this camera mode is activated on the camera mode stack.
	virtual void OnActivation() {};

	// 该模式从栈中移除或整个模式栈停用时调用。
	// Called when this camera mode is deactivated on the camera mode stack.
	virtual void OnDeactivation() {};

	UE_API void UpdateCameraMode(float DeltaTime);

	float GetBlendTime() const { return BlendTime; }
	float GetBlendWeight() const { return BlendWeight; }
	UE_API void SetBlendWeight(float Weight);

	FGameplayTag GetCameraTypeTag() const
	{
		return CameraTypeTag;
	}

	UE_API virtual void DrawDebug(UCanvas* Canvas) const;

protected:

	UE_API virtual FVector GetPivotLocation() const;
	UE_API virtual FRotator GetPivotRotation() const;

	UE_API virtual void UpdateView(float DeltaTime);
	UE_API virtual void UpdateBlending(float DeltaTime);

protected:
	// 供玩法代码查询当前相机类型的标签，无需依赖具体模式类；
	// 例如可用它判断是否处于瞄准镜模式并调整精度。
	// A tag that can be queried by gameplay code that cares when a kind of camera mode is active
	// without having to ask about a specific mode (e.g., when aiming downsights to get more accuracy)
	UPROPERTY(EditDefaultsOnly, Category = "Blending")
	FGameplayTag CameraTypeTag;

	// 该模式本帧生成的视图输出。
	// View output produced by the camera mode.
	FLyraCameraModeView View;

	// 水平视场角，单位为度。
	// The horizontal field of view (in degrees).
	UPROPERTY(EditDefaultsOnly, Category = "View", Meta = (UIMin = "5.0", UIMax = "170", ClampMin = "5.0", ClampMax = "170.0"))
	float FieldOfView;

	// 视角俯仰的最小角度，单位为度。
	// Minimum view pitch (in degrees).
	UPROPERTY(EditDefaultsOnly, Category = "View", Meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMin;

	// 视角俯仰的最大角度，单位为度。
	// Maximum view pitch (in degrees).
	UPROPERTY(EditDefaultsOnly, Category = "View", Meta = (UIMin = "-89.9", UIMax = "89.9", ClampMin = "-89.9", ClampMax = "89.9"))
	float ViewPitchMax;

	// 该模式从权重 0 混合到 1 所需的时间。
	// How long it takes to blend in this mode.
	UPROPERTY(EditDefaultsOnly, Category = "Blending")
	float BlendTime;

	// 由 BlendAlpha 计算 BlendWeight 时使用的缓动函数。
	// Function used for blending.
	UPROPERTY(EditDefaultsOnly, Category = "Blending")
	ELyraCameraModeBlendFunction BlendFunction;

	// 控制缓动曲线形状的指数。
	// Exponent used by blend functions to control the shape of the curve.
	UPROPERTY(EditDefaultsOnly, Category = "Blending")
	float BlendExponent;

	// 随时间线性推进的混合 Alpha，用于计算最终权重。
	// Linear blend alpha used to determine the blend weight.
	float BlendAlpha;

	// 由 BlendAlpha 和 BlendFunction 计算出的实际混合权重。
	// Blend weight calculated using the blend alpha and function.
	float BlendWeight;

protected:
	/** 为 true 时跳过插值并直接将相机放到理想位置；下一帧自动复位为 false。 */
	/** If true, skips all interpolation and puts camera in ideal location.  Automatically set to false next frame. */
	UPROPERTY(transient)
	uint32 bResetInterpolation:1;
};


/**
 * 管理相机模式实例，并按栈顶优先、从栈底向上合成的规则输出最终视图。
 */
/**
 * ULyraCameraModeStack
 *
 *	Stack used for blending camera modes.
 */
UCLASS()
class ULyraCameraModeStack : public UObject
{
	GENERATED_BODY()

public:

	ULyraCameraModeStack();

	void ActivateStack();
	void DeactivateStack();

	bool IsStackActivate() const { return bIsActive; }

	void PushCameraMode(TSubclassOf<ULyraCameraMode> CameraModeClass);

	bool EvaluateStack(float DeltaTime, FLyraCameraModeView& OutCameraModeView);

	void DrawDebug(UCanvas* Canvas) const;

	// 返回栈顶模式的类型标签及其当前混合权重。
	// Gets the tag associated with the top layer and the blend weight of it
	void GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const;

protected:

	ULyraCameraMode* GetCameraModeInstance(TSubclassOf<ULyraCameraMode> CameraModeClass);

	void UpdateStack(float DeltaTime);
	void BlendStack(FLyraCameraModeView& OutCameraModeView) const;

protected:

	bool bIsActive;

	UPROPERTY()
	TArray<TObjectPtr<ULyraCameraMode>> CameraModeInstances;

	UPROPERTY()
	TArray<TObjectPtr<ULyraCameraMode>> CameraModeStack;
};

#undef UE_API
