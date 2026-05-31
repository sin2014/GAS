// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "MMCharacterBase.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UMMCharacterAttributeSet;
class USpringArmComponent;
class UStaticMeshComponent;

// 最小可运行人物角色；负责挂载 GAS 组件、角色属性和临时可见模型。
UCLASS()
class MMM_API AMMCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// 创建角色组件、GAS 组件、属性集合、临时模型和俯视相机。
	AMMCharacterBase();

	// 返回该角色持有的 GAS AbilitySystemComponent。
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 返回该角色持有的人物属性集合。
	UFUNCTION(BlueprintPure, Category = "MM|角色|GAS")
	UMMCharacterAttributeSet* GetCharacterAttributeSet() const;

protected:
	// 初始化 GAS ActorInfo，并输出一次属性调试信息。
	virtual void BeginPlay() override;

private:
	// GAS 能力系统组件；角色的 AttributeSet 必须通过它进入 GAS 流程。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MM|角色|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// 人物属性集合；保存等级、HP、核心属性、攻防三率和六元素防御。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MM|角色|GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMMCharacterAttributeSet> CharacterAttributeSet;

	// 临时可见模型；当前只用于让玩家在默认地图中能看到角色。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MM|角色|显示", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> TemporaryVisualMesh;

	// 固定俯视相机臂；用于首个可运行版本的四向移动观察。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MM|角色|相机", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	// 固定俯视相机；首个版本不支持玩家旋转相机。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MM|角色|相机", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;
};
