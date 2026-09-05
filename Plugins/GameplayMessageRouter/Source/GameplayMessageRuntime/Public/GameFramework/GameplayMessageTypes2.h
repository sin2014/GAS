// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "GameplayMessageTypes2.generated.h"

class UGameplayMessageRouter;

// 消息监听器的频道匹配规则。
// Match rule for message listeners
UENUM(BlueprintType)
enum class EGameplayMessageMatch : uint8
{
	// 精确匹配只接收频道标签完全相同的消息。
	// 例如，监听“A.B”可接收 A.B，但不会接收 A.B.C。
	// An exact match will only receive messages with exactly the same channel
	// (e.g., registering for "A.B" will match a broadcast of A.B but not A.B.C)
	ExactMatch,

	// 部分匹配会接收以监听频道为根的消息。
	// 例如，监听“A.B”既可接收 A.B，也可接收 A.B.C。
	// A partial match will receive any messages rooted in the same channel
	// (e.g., registering for "A.B" will match a broadcast of A.B as well as A.B.C)
	PartialMatch
};

/** 注册 Gameplay Message 监听器时用于指定高级行为的参数结构。 */
/**
 * Struct used to specify advanced behavior when registering a listener for gameplay messages
 */
template<typename FMessageStructType>
struct FGameplayMessageListenerParams
{
	/** 指定回调是否接收更具体的子频道广播，或仅接收精确匹配的频道。 */
	/** Whether Callback should be called for broadcasts of more derived channels or if it will only be called for exact matches. */
	EGameplayMessageMatch MatchType = EGameplayMessageMatch::ExactMatch;

	/** 绑定后，当指定频道广播消息时调用此回调。 */
	/** If bound this callback will trigger when a message is broadcast on the specified channel. */
	TFunction<void(FGameplayTag, const FMessageStructType&)> OnMessageReceivedCallback;

	/** 将成员函数以弱对象引用方式绑定到 OnMessageReceivedCallback，避免回调延长对象生命周期。 */
	/** Helper to bind weak member function to OnMessageReceivedCallback */
	template<typename TOwner = UObject>
	void SetMessageReceivedCallback(TOwner* Object, void(TOwner::* Function)(FGameplayTag, const FMessageStructType&))
	{
		TWeakObjectPtr<TOwner> WeakObject(Object);
		OnMessageReceivedCallback = [WeakObject, Function](FGameplayTag Channel, const FMessageStructType& Payload)
		{
			if (TOwner* StrongObject = WeakObject.Get())
			{
				(StrongObject->*Function)(Channel, Payload);
			}
		};
	}
};

