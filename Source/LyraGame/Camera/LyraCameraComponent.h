// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"

#include "LyraCameraComponent.generated.h"

class UCanvas;
class ULyraCameraMode;
class ULyraCameraModeStack;
class UObject;
struct FFrame;
struct FGameplayTag;
struct FMinimalViewInfo;
template <class TClass> class TSubclassOf;

DECLARE_DELEGATE_RetVal(TSubclassOf<ULyraCameraMode>, FLyraCameraModeDelegate);


/**
 * 项目的基础相机组件，通过相机模式栈计算并输出最终视图。
 */
/**
 * ULyraCameraComponent
 *
 *	The base camera component class used by this project.
 */
UCLASS()
class ULyraCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:

	ULyraCameraComponent(const FObjectInitializer& ObjectInitializer);

	// 返回指定 Actor 上的 LyraCameraComponent；不存在时返回 nullptr。
	// Returns the camera component if one exists on the specified actor.
	UFUNCTION(BlueprintPure, Category = "Lyra|Camera")
	static ULyraCameraComponent* FindCameraComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<ULyraCameraComponent>() : nullptr); }

	// 返回相机当前跟随和观察的目标 Actor，默认即组件拥有者。
	// Returns the target actor that the camera is looking at.
	virtual AActor* GetTargetActor() const { return GetOwner(); }

	// 每帧查询当前应压入栈顶的相机模式，通常由 HeroComponent 绑定。
	// Delegate used to query for the best camera mode.
	FLyraCameraModeDelegate DetermineCameraModeDelegate;

	// 为下一次视图计算累加 FOV 偏移；应用后立即清零，因此只影响一帧。
	// Add an offset to the field of view.  The offset is only for one frame, it gets cleared once it is applied.
	void AddFieldOfViewOffset(float FovOffset) { FieldOfViewOffset += FovOffset; }

	virtual void DrawDebug(UCanvas* Canvas) const;

	// 返回栈顶相机模式的类型标签及其当前混合权重。
	// Gets the tag associated with the top layer and the blend weight of it
	void GetBlendInfo(float& OutWeightOfTopLayer, FGameplayTag& OutTagOfTopLayer) const;

protected:

	virtual void OnRegister() override;
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	virtual void UpdateCameraModes();

protected:

	// 持有相机模式实例并按栈顶优先规则计算混合结果的模式栈。
	// Stack used to blend the camera modes.
	UPROPERTY()
	TObjectPtr<ULyraCameraModeStack> CameraModeStack;

	// 待应用到最终视图的单帧 FOV 偏移。
	// Offset applied to the field of view.  The offset is only for one frame, it gets cleared once it is applied.
	float FieldOfViewOffset;

};
