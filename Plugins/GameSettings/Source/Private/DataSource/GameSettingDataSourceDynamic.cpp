// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataSource/GameSettingDataSourceDynamic.h"

#include "Engine/LocalPlayer.h"

//--------------------------------------
// 动态设置数据源通过运行时路径解析玩家对象上的属性或函数。
// FGameSettingDataSourceDynamic
//--------------------------------------

// 保存由函数名和属性名组成的动态路径，等待针对具体本地玩家解析。
FGameSettingDataSourceDynamic::FGameSettingDataSourceDynamic(const TArray<FString>& InDynamicPath)
	: DynamicPath(InDynamicPath)
{
}

// 针对指定本地玩家解析动态属性路径，并缓存后续读写所需的对象与属性链。
bool FGameSettingDataSourceDynamic::Resolve(ULocalPlayer* InLocalPlayer)
{
	return DynamicPath.Resolve(InLocalPlayer);
}

// 通过已解析的动态路径读取当前属性，并将结果序列化为设置系统使用的字符串。
FString FGameSettingDataSourceDynamic::GetValueAsString(ULocalPlayer* InLocalPlayer) const
{
	FString OutStringValue;

	const bool bSuccess = PropertyPathHelpers::GetPropertyValueAsString(InLocalPlayer, DynamicPath, OutStringValue);
	ensure(bSuccess);

	return OutStringValue;
}

// 把字符串反序列化后写入动态属性路径指向的目标，并触发相应属性更新。
void FGameSettingDataSourceDynamic::SetValue(ULocalPlayer* InLocalPlayer, const FString& InStringValue)
{
	const bool bSuccess = PropertyPathHelpers::SetPropertyValueFromString(InLocalPlayer, DynamicPath, InStringValue);
	ensure(bSuccess);
}

// 将动态属性路径拼接成可读文本，供诊断和错误信息使用。
FString FGameSettingDataSourceDynamic::ToString() const
{
	return DynamicPath.ToString();
}
