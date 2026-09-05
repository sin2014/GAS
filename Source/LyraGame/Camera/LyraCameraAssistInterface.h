// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "LyraCameraAssistInterface.generated.h"

/** */
UINTERFACE(BlueprintType)
class ULyraCameraAssistInterface : public UInterface
{
	GENERATED_BODY()
};

class ILyraCameraAssistInterface
{
	GENERATED_BODY()

public:
	/**
	 * 返回允许相机射线穿过并忽略碰撞的 Actor，例如多个观察目标、Pawn 或载具。
	 */
	/**
	 * Get the list of actors that we're allowing the camera to penetrate. Useful in 3rd person cameras
	 * when you need the following camera to ignore things like the a collection of view targets, the pawn,
	 * a vehicle..etc.
	 */
	virtual void GetIgnoredActorsForCameraPentration(TArray<const AActor*>& OutActorsAllowPenetration) const { }

	/**
	 * 返回相机必须避免穿透、需要保持在画面中的焦点 Actor；未实现时默认使用当前 ViewTarget。
	 */
	/**
	 * The target actor to prevent penetration on.  Normally, this is almost always the view target, which if
	 * unimplemented will remain true.  However, sometimes the view target, isn't the same as the root actor 
	 * you need to keep in frame.
	 */
	virtual TOptional<AActor*> GetCameraPreventPenetrationTarget() const
	{
		return TOptional<AActor*>();
	}

	/** 相机进入焦点 Actor 内部时调用，可用于临时隐藏被相机重叠的目标。 */
	/** Called if the camera penetrates the focal target.  Useful if you want to hide the target actor when being overlapped. */
	virtual void OnCameraPenetratingTarget() { }
};
