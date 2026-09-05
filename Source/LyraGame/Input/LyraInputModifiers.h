// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputModifiers.h"

#include "UObject/UnrealType.h"
#include "LyraInputModifiers.generated.h"

struct FInputActionValue;

class FProperty;
class UEnhancedPlayerInput;
class ULyraAimSensitivityData;
class UObject;

/** 根据 SharedUserSettings 中指定的 double 属性，对各轴输入进行缩放并钳制到配置范围。 */
/** 
*  Scales input basedon a double property in the SharedUserSettings
*/
UCLASS(NotBlueprintable, MinimalAPI, meta = (DisplayName = "Setting Based Scalar"))
class ULyraSettingBasedScalar : public UInputModifier
{
	GENERATED_BODY()

public:

	/** 用作 X 轴缩放系数的 SharedUserSettings 属性名。 */
	/** Name of the property that will be used to clamp the X Axis of this value */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	FName XAxisScalarSettingName = NAME_None;

	/** 用作 Y 轴缩放系数的 SharedUserSettings 属性名。 */
	/** Name of the property that will be used to clamp the Y Axis of this value */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	FName YAxisScalarSettingName = NAME_None;

	/** 用作 Z 轴缩放系数的 SharedUserSettings 属性名。 */
	/** Name of the property that will be used to clamp the Z Axis of this value */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	FName ZAxisScalarSettingName = NAME_None;
	
	/** 各轴设置缩放系数允许的最大值。 */
	/** Set the maximium value of this setting on each axis. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	FVector MaxValueClamp = FVector(10.0, 10.0, 10.0);
	
	/** 各轴设置缩放系数允许的最小值。 */
	/** Set the minimum value of this setting on each axis. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings)
	FVector MinValueClamp = FVector::ZeroVector;

protected:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;

	/** 缓存从设置类反射得到的 FProperty，避免每帧重复按名称查找。 */
	/** FProperty Cache that will be populated with any found FProperty's on the settings class so that we don't need to look them up each frame */
	TArray<const FProperty*> PropertyCache;
};

/** 指定死区应用于移动摇杆还是视角摇杆。 */
/** Represents which stick that this deadzone is for, either the move or the look stick */
UENUM()
enum class EDeadzoneStick : uint8
{
	/** 使用移动摇杆的用户死区设置。 */
	/** Deadzone for the movement stick */
	MoveStick = 0,

	/** 使用视角摇杆的用户死区设置。 */
	/** Deadzone for the looking stick */
	LookStick = 1,
};

/**
 * 死区上下限由 Lyra SharedSettings 中对应摇杆设置驱动的 Enhanced Input Modifier。
 */
/**
 * This is a deadzone input modifier that will have it's thresholds driven by what is in the Lyra Shared game settings. 
 */
UCLASS(NotBlueprintable, MinimalAPI, meta = (DisplayName = "Lyra Settings Driven Dead Zone"))
class ULyraInputModifierDeadZone : public UInputModifier
{
	GENERATED_BODY()

public:

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings, Config)
	EDeadZoneType Type = EDeadZoneType::Radial;
	
	// 输入绝对值达到该上限后钳制为 1。
	// Threshold above which input is clamped to 1
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings, Config)
	float UpperThreshold = 1.0f;

	/** 选择移动或视角摇杆，从而决定读取哪组用户死区设置。 */
	/** Which stick this deadzone is for. This controls which setting will be used when calculating the deadzone */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings, Config)
	EDeadzoneStick DeadzoneStick = EDeadzoneStick::MoveStick;

protected:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;

	// 调试可视化中，未修改输入显示为黑色，被死区阻断时显示红色，并用强度区分轴向。
	// Visualize as black when unmodified. Red when blocked (with differing intensities to indicate axes)
	// 对应的摇杆死区可视化方法参考以下文章。
	// Mirrors visualization in https://www.gamasutra.com/blogs/JoshSutphin/20130416/190541/Doing_Thumbstick_Dead_Zones_Right.php.
	virtual FLinearColor GetVisualizationColor_Implementation(FInputActionValue SampleValue, FInputActionValue FinalValue) const override;
};

/** 选择普通观察或开镜瞄准所使用的灵敏度类型。 */
/** The type of targeting sensitity that should be considered */
UENUM()
enum class ELyraTargetingType : uint8
{
	/** 普通观察视角时使用的灵敏度。 */
	/** Sensitivity to be applied why normally looking around */
	Normal = 0,

	/** 开镜瞄准时使用的灵敏度。 */
	/** The sensitivity that should be applied while Aiming Down Sights */
	ADS = 1,
};

/** 根据 Lyra SharedSettings 的手柄灵敏度档位，对视角输入应用标量。 */
/** Applies a scalar modifier based on the current gamepad settings in Lyra Shared game settings.  */
UCLASS(NotBlueprintable, MinimalAPI, meta = (DisplayName = "Lyra Gamepad Sensitivity"))
class ULyraInputModifierGamepadSensitivity : public UInputModifier
{
	GENERATED_BODY()
public:
	
	/** 本 Modifier 读取普通观察还是 ADS 灵敏度设置。 */
	/** The type of targeting to use for this Sensitivity */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=Settings, Config)
	ELyraTargetingType TargetingType = ELyraTargetingType::Normal;

	/** 将用户灵敏度枚举档位映射为实际 float 标量的数据资产。 */
	/** Asset that gives us access to the float scalar value being used for sensitivty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(AssetBundles="Client,Server"))
	TObjectPtr<const ULyraAimSensitivityData> SensitivityLevelTable;

protected:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
};

/** 根据 Lyra SharedSettings 的反转选项，对瞄准输入的对应轴取反。 */
/** Applies an inversion of axis values based on a setting in the Lyra Shared game settings */
UCLASS(NotBlueprintable, MinimalAPI, meta = (DisplayName = "Lyra Aim Inversion Setting"))
class ULyraInputModifierAimInversion : public UInputModifier
{
	GENERATED_BODY()
	
protected:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;	
};
