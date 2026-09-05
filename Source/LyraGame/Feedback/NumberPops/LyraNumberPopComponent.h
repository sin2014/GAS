// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ControllerComponent.h"
#include "GameplayTagContainer.h"

#include "LyraNumberPopComponent.generated.h"

class UObject;
struct FFrame;

USTRUCT(BlueprintType)
struct FLyraNumberPopRequest
{
	GENERATED_BODY()

	// 数字跳字在世界空间中的生成位置。
	// The world location to create the number pop at
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lyra|Number Pops")
	FVector WorldLocation;

	// 与伤害来源或成因相关的标签，用于选择表现样式。
	// Tags related to the source/cause of the number pop (for determining a style)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lyra|Number Pops")
	FGameplayTagContainer SourceTags;

	// 与受击目标相关的标签，用于选择表现样式。
	// Tags related to the target of the number pop (for determining a style)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lyra|Number Pops")
	FGameplayTagContainer TargetTags;

	// 要显示的数值。
	// The number to display
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lyra|Number Pops")
	int32 NumberToDisplay = 0;

	// 是否为暴击数字；后续应改由标签表达该语义。
	// Whether the number is 'critical' or not (@TODO: move to a tag)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lyra|Number Pops")
	bool bIsCriticalDamage = false;

	FLyraNumberPopRequest()
		: WorldLocation(ForceInitToZero)
	{
	}
};


UCLASS(Abstract)
class ULyraNumberPopComponent : public UControllerComponent
{
	GENERATED_BODY()

public:

	ULyraNumberPopComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 提交一条伤害数字请求，由具体实现加入对应的可视化队列。 */
	/** Adds a damage number to the damage number list for visualization */
	UFUNCTION(BlueprintCallable, Category = Foo)
	virtual void AddNumberPop(const FLyraNumberPopRequest& NewRequest) {}
};
