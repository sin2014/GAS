// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameSetting.h"

#include "GameSettingValue.generated.h"

#define UE_API GAMESETTINGS_API

class UObject;

// 值设置抽象定义初始值保存、恢复默认、还原和提交后的基线更新。
//--------------------------------------
// UGameSettingValue
//--------------------------------------

// 所有“值”类设置的基类；此类设置可修改、恢复默认或还原到初始值。
/**
 * The base class for all settings that are conceptually a value, that can be 
 * changed, and thus reset or restored to their initial value.
 */
UCLASS(MinimalAPI, Abstract)
class UGameSettingValue : public UGameSetting
{
	GENERATED_BODY()

public:
	UE_API UGameSettingValue();

	// 保存设置初始值；初始化时调用，应用设置后也应重新保存。
	/** Stores an initial value for the setting.  This will be called on initialize, but should also be called if you 'apply' the setting. */
	virtual void StoreInitial() PURE_VIRTUAL(, );

	// 将属性恢复为默认值。
	/** Resets the property to the default. */
	virtual void ResetToDefault() PURE_VIRTUAL(, );

	// 恢复到打开设置界面、尚未修改时记录的初始值。
	/** Restores the setting to the initial value, this is the value when you open the settings before making any tweaks. */
	virtual void RestoreToInitial() PURE_VIRTUAL(, );

protected:
	UE_API virtual void OnInitialized() override;
};

#undef UE_API
