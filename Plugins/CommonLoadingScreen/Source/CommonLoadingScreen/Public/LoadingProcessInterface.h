// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "LoadingProcessInterface.generated.h"

#define UE_API COMMONLOADINGSCREEN_API

/** 供可能触发或延长加载流程的对象实现，用于请求显示加载画面。 */
/** Interface for things that might cause loading to happen which requires a loading screen to be displayed */
UINTERFACE(MinimalAPI, BlueprintType)
class ULoadingProcessInterface : public UInterface
{
	GENERATED_BODY()
};

class ILoadingProcessInterface
{
	GENERATED_BODY()

public:
	// 检查对象是否实现此接口；若实现，则询问它当前是否要求显示加载画面并返回原因。
	// Checks to see if this object implements the interface, and if so asks whether or not we should
	// be currently showing a loading screen
	static UE_API bool ShouldShowLoadingScreen(UObject* TestObject, FString& OutReason);

	virtual bool ShouldShowLoadingScreen(FString& OutReason) const
	{
		return false;
	}
};

#undef UE_API
