// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSettingValue.h"

#include "GameSettingValueDiscrete.generated.h"

#define UE_API GAMESETTINGS_API

class UObject;
struct FFrame;

UCLASS(MinimalAPI, Abstract)
class UGameSettingValueDiscrete : public UGameSettingValue
{
	GENERATED_BODY()

public:
	UE_API UGameSettingValueDiscrete();

	// 以下纯虚接口定义离散设置的选项写入、当前索引、默认索引和显示文本。
	/** UGameSettingValueDiscrete */
	virtual void SetDiscreteOptionByIndex(int32 Index) PURE_VIRTUAL(,);
	
	UFUNCTION(BlueprintCallable)
	virtual int32 GetDiscreteOptionIndex() const PURE_VIRTUAL(,return INDEX_NONE;);

	// 以下默认索引接口可选；未提供默认值时返回 INDEX_NONE。
	/** Optional */
	UFUNCTION(BlueprintCallable)
	virtual int32 GetDiscreteOptionDefaultIndex() const { return INDEX_NONE; }

	UFUNCTION(BlueprintCallable)
	virtual TArray<FText> GetDiscreteOptions() const PURE_VIRTUAL(,return TArray<FText>(););

	UE_API virtual FString GetAnalyticsValue() const;
};

#undef UE_API
