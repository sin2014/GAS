// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "InteractionOption.generated.h"

class IInteractableTarget;
class UUserWidget;

/** 描述一个可呈现并执行的交互选项，包括目标、UI 文本和两种技能执行方式。 */
/**  */
USTRUCT(BlueprintType)
struct FInteractionOption
{
	GENERATED_BODY()

public:
	/** 提供该选项并接收事件数据定制的交互目标。 */
	/** The interactable target */
	UPROPERTY(BlueprintReadWrite)
	TScriptInterface<IInteractableTarget> InteractableTarget;

	/** 交互提示的主文本。 */
	/** Simple text the interaction might return */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Text;

	/** 交互提示的辅助文本。 */
	/** Simple sub-text the interaction might return */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText SubText;

	// 支持两种交互技能执行模型：
	// METHODS OF INTERACTION
	//--------------------------------------------------------------

	// 1）接近目标时把技能临时授予交互者 Avatar，执行交互时由交互者 ASC 激活。
	// 1) Place an ability on the avatar that they can activate when they perform interaction.

	/** 接近可交互对象时需要授予交互者 Avatar 的技能类型。 */
	/** The ability to grant the avatar when they get near interactable objects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> InteractionAbilityToGrant;

	// 或者：
	// - OR -

	// 2）目标拥有自己的 ASC 与交互技能，交互者直接触发目标上的 GameplayAbilitySpec。
	// 2) Allow the object we're interacting with to have its own ability system and interaction ability, that we can activate instead.

	/** 目标侧 ASC，用于查找 TargetInteractionAbilityHandle 并发送 GameplayEvent。 */
	/** The ability system on the target that can be used for the TargetInteractionHandle and sending the event, if needed. */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UAbilitySystemComponent> TargetAbilitySystem = nullptr;

	/** 此选项要在目标 ASC 上激活的 GameplayAbilitySpec 句柄。 */
	/** The ability spec to activate on the object for this option. */
	UPROPERTY(BlueprintReadOnly)
	FGameplayAbilitySpecHandle TargetInteractionAbilityHandle;

	// UI
	//--------------------------------------------------------------

	/** 呈现该类交互提示时使用的 Widget 类。 */
	/** The widget to show for this kind of interaction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<UUserWidget> InteractionWidgetClass;

	//--------------------------------------------------------------

public:
	FORCEINLINE bool operator==(const FInteractionOption& Other) const
	{
		return InteractableTarget == Other.InteractableTarget &&
			InteractionAbilityToGrant == Other.InteractionAbilityToGrant&&
			TargetAbilitySystem == Other.TargetAbilitySystem &&
			TargetInteractionAbilityHandle == Other.TargetInteractionAbilityHandle &&
			InteractionWidgetClass == Other.InteractionWidgetClass &&
			Text.IdenticalTo(Other.Text) &&
			SubText.IdenticalTo(Other.SubText);
	}

	FORCEINLINE bool operator!=(const FInteractionOption& Other) const
	{
		return !operator==(Other);
	}

	FORCEINLINE bool operator<(const FInteractionOption& Other) const
	{
		return InteractableTarget.GetInterface() < Other.InteractableTarget.GetInterface();
	}
};
