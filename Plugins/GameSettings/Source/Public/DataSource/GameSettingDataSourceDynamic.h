// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSettingDataSource.h"
#include "PropertyPathHelpers.h"

#define UE_API GAMESETTINGS_API

class ULocalPlayer;

//--------------------------------------
// 动态数据源沿配置路径解析对象、函数和属性，供设置值运行时读写。
// FGameSettingDataSourceDynamic
//--------------------------------------

class FGameSettingDataSourceDynamic : public FGameSettingDataSource
{
public:
	UE_API FGameSettingDataSourceDynamic(const TArray<FString>& InDynamicPath);

	UE_API virtual bool Resolve(ULocalPlayer* InLocalPlayer) override;

	UE_API virtual FString GetValueAsString(ULocalPlayer* InLocalPlayer) const override;

	UE_API virtual void SetValue(ULocalPlayer* InLocalPlayer, const FString& Value) override;

	UE_API virtual FString ToString() const override;

private:
	FCachedPropertyPath DynamicPath;
};

#undef UE_API
