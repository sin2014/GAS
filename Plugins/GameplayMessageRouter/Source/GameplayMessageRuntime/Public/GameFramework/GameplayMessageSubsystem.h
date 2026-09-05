// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayMessageTypes2.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/WeakObjectPtr.h"

#include "GameplayMessageSubsystem.generated.h"

#define UE_API GAMEPLAYMESSAGERUNTIME_API

class UGameplayMessageSubsystem;
struct FFrame;

GAMEPLAYMESSAGERUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogGameplayMessageSubsystem, Log, All);

class UAsyncAction_ListenForGameplayMessage;

/**
 * 用于撤销已注册消息监听器的不透明句柄。
 * @see UGameplayMessageSubsystem::RegisterListener 和 UGameplayMessageSubsystem::UnregisterListener
 */
/**
 * An opaque handle that can be used to remove a previously registered message listener
 * @see UGameplayMessageSubsystem::RegisterListener and UGameplayMessageSubsystem::UnregisterListener
 */
USTRUCT(BlueprintType)
struct FGameplayMessageListenerHandle
{
public:
	GENERATED_BODY()

	FGameplayMessageListenerHandle() {}

	UE_API void Unregister();

	bool IsValid() const { return ID != 0; }

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UGameplayMessageSubsystem> Subsystem;

	UPROPERTY(Transient)
	FGameplayTag Channel;

	UPROPERTY(Transient)
	int32 ID = 0;

	FDelegateHandle StateClearedHandle;

	friend UGameplayMessageSubsystem;

	FGameplayMessageListenerHandle(UGameplayMessageSubsystem* InSubsystem, FGameplayTag InChannel, int32 InID) : Subsystem(InSubsystem), Channel(InChannel), ID(InID) {}
};

/** 单个已注册监听器的内部记录。 */
/** 
 * Entry information for a single registered listener
 */
USTRUCT()
struct FGameplayMessageListenerData
{
	GENERATED_BODY()

	// 接收到匹配消息时调用的类型擦除回调。
	// Callback for when a message has been received
	TFunction<void(FGameplayTag, const UScriptStruct*, const void*)> ReceivedCallback;

	int32 HandleID;
	EGameplayMessageMatch MatchType;

	// 保留监听时的结构体类型及其曾经有效的状态，用于诊断类型失效和不匹配问题。
	// Adding some logging and extra variables around some potential problems with this
	TWeakObjectPtr<const UScriptStruct> ListenerStructType = nullptr;
	bool bHadValidType = false;
};

/**
 * 该系统允许消息发送方与监听方在无需直接引用彼此的情况下通信，但双方必须约定相同的 USTRUCT 消息格式。
 * 可从 GameInstance 获取消息路由器：
 *    UGameInstance::GetSubsystem<UGameplayMessageSubsystem>(GameInstance)
 * 也可通过能够解析到 World 的对象直接获取：
 *    UGameplayMessageSubsystem::Get(WorldContextObject)
 * 同一频道存在多个监听器时，不保证回调顺序，并且顺序可能随时间变化。
 */
/**
 * This system allows event raisers and listeners to register for messages without
 * having to know about each other directly, though they must agree on the format
 * of the message (as a USTRUCT() type).
 *
 *
 * You can get to the message router from the game instance:
 *    UGameInstance::GetSubsystem<UGameplayMessageSubsystem>(GameInstance)
 * or directly from anything that has a route to a world:
 *    UGameplayMessageSubsystem::Get(WorldContextObject)
 *
 * Note that call order when there are multiple listeners for the same channel is
 * not guaranteed and can change over time!
 */
UCLASS(MinimalAPI)
class UGameplayMessageSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	friend UAsyncAction_ListenForGameplayMessage;

public:

	/** @return 与指定对象所在 World 的 GameInstance 关联的消息路由器。 */
	/**
	 * @return the message router for the game instance associated with the world of the specified object
	 */
	static UE_API UGameplayMessageSubsystem& Get(const UObject* WorldContextObject);

	/** @return 指定对象所在 World 中是否存在有效的 Gameplay Message 子系统。 */
	/**
	 * @return true if a valid GameplayMessageRouter subsystem if active in the provided world
	 */
	static UE_API bool HasInstance(const UObject* WorldContextObject);

	//~USubsystem 接口
	//~USubsystem interface
	UE_API virtual void Deinitialize() override;
	//~USubsystem 接口结束
	//~End of USubsystem interface

	/**
	 * 在指定频道广播消息。
	 * @param Channel 广播使用的消息频道。
	 * @param Message 要发送的消息；其 UScriptStruct 类型必须与该频道监听器所期望的类型兼容，否则会记录错误。
	 */
	/**
	 * Broadcast a message on the specified channel
	 *
	 * @param Channel			The message channel to broadcast on
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 */
	template <typename FMessageStructType>
	void BroadcastMessage(FGameplayTag Channel, const FMessageStructType& Message)
	{
		const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
		BroadcastMessageInternal(Channel, StructType, &Message);
	}

	/**
	 * 注册指定频道的消息监听器。
	 * @param Channel 要监听的消息频道。
	 * @param Callback 收到广播时调用的函数；参数结构体类型必须与发送方提供的类型兼容。
	 * @return 可通过句柄自身的 Unregister() 或路由器的 UnregisterListener 撤销监听的句柄。
	 */
	/**
	 * Register to receive messages on a specified channel
	 *
	 * @param Channel			The message channel to listen to
	 * @param Callback			Function to call with the message when someone broadcasts it (must be the same type of UScriptStruct provided by broadcasters for this channel, otherwise an error will be logged)
	 *
	 * @return a handle that can be used to unregister this listener (either by calling Unregister() on the handle or calling UnregisterListener on the router)
	 */
	template <typename FMessageStructType>
	FGameplayMessageListenerHandle RegisterListener(FGameplayTag Channel, TFunction<void(FGameplayTag, const FMessageStructType&)>&& Callback, EGameplayMessageMatch MatchType = EGameplayMessageMatch::ExactMatch)
	{
		auto ThunkCallback = [InnerCallback = MoveTemp(Callback)](FGameplayTag ActualTag, const UScriptStruct* SenderStructType, const void* SenderPayload)
		{
			InnerCallback(ActualTag, *reinterpret_cast<const FMessageStructType*>(SenderPayload));
		};

		const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
		return RegisterListenerInternal(Channel, ThunkCallback, StructType, MatchType);
	}

	/**
	 * 使用指定对象的成员函数监听频道，并在触发回调前通过弱引用确认对象仍然存活。
	 * @param Channel 要监听的消息频道。
	 * @param Object 接收回调的对象实例。
	 * @param Function 收到兼容结构体消息时调用的成员函数。
	 * @return 可用于撤销此监听器的句柄。
	 */
	/**
	 * Register to receive messages on a specified channel and handle it with a specified member function
	 * Executes a weak object validity check to ensure the object registering the function still exists before triggering the callback
	 *
	 * @param Channel			The message channel to listen to
	 * @param Object			The object instance to call the function on
	 * @param Function			Member function to call with the message when someone broadcasts it (must be the same type of UScriptStruct provided by broadcasters for this channel, otherwise an error will be logged)
	 *
	 * @return a handle that can be used to unregister this listener (either by calling Unregister() on the handle or calling UnregisterListener on the router)
	 */
	template <typename FMessageStructType, typename TOwner = UObject>
	FGameplayMessageListenerHandle RegisterListener(FGameplayTag Channel, TOwner* Object, void(TOwner::* Function)(FGameplayTag, const FMessageStructType&))
	{
		TWeakObjectPtr<TOwner> WeakObject(Object);
		return RegisterListener<FMessageStructType>(Channel,
			[WeakObject, Function](FGameplayTag Channel, const FMessageStructType& Payload)
			{
				if (TOwner* StrongObject = WeakObject.Get())
				{
					(StrongObject->*Function)(Channel, Payload);
				}
			});
	}

	/**
	 * 使用高级参数注册频道监听器；参数可控制标签匹配规则和消息回调。
	 * 此接口中带状态的部分后续宜拆分到独立系统。
	 * @param Channel 要监听的消息频道。
	 * @param Params 描述高级监听行为的参数结构。
	 * @return 可用于撤销此监听器的句柄；未提供回调时返回无效句柄。
	 */
	/**
	 * Register to receive messages on a specified channel with extra parameters to support advanced behavior
	 * The stateful part of this logic should probably be separated out to a separate system
	 *
	 * @param Channel			The message channel to listen to
	 * @param Params			Structure containing details for advanced behavior
	 *
	 * @return a handle that can be used to unregister this listener (either by calling Unregister() on the handle or calling UnregisterListener on the router)
	 */
	template <typename FMessageStructType>
	FGameplayMessageListenerHandle RegisterListener(FGameplayTag Channel, FGameplayMessageListenerParams<FMessageStructType>& Params)
	{
		FGameplayMessageListenerHandle Handle;

		// 注册后接收该频道未来广播的消息。
		// Register to receive any future messages broadcast on this channel
		if (Params.OnMessageReceivedCallback)
		{
			auto ThunkCallback = [InnerCallback = Params.OnMessageReceivedCallback](FGameplayTag ActualTag, const UScriptStruct* SenderStructType, const void* SenderPayload)
			{
				InnerCallback(ActualTag, *reinterpret_cast<const FMessageStructType*>(SenderPayload));
			};

			const UScriptStruct* StructType = TBaseStructure<FMessageStructType>::Get();
			Handle = RegisterListenerInternal(Channel, ThunkCallback, StructType, Params.MatchType);
		}

		return Handle;
	}

	/** 使用 RegisterListener 返回的句柄移除先前注册的消息监听器。 */
	/**
	 * Remove a message listener previously registered by RegisterListener
	 *
	 * @param Handle	The handle returned by RegisterListener
	 */
	UE_API void UnregisterListener(FGameplayMessageListenerHandle Handle);

protected:
	/** 蓝图自定义 Thunk 入口，在指定频道广播任意 USTRUCT 类型的消息。 */
	/**
	 * Broadcast a message on the specified channel
	 *
	 * @param Channel			The message channel to broadcast on
	 * @param Message			The message to send (must be the same type of UScriptStruct expected by the listeners for this channel, otherwise an error will be logged)
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category=Messaging, meta=(CustomStructureParam="Message", AllowAbstract="false", DisplayName="Broadcast Message"))
	UE_API void K2_BroadcastMessage(FGameplayTag Channel, const int32& Message);

	DECLARE_FUNCTION(execK2_BroadcastMessage);

private:
	// 执行类型擦除后的消息广播、标签父链匹配和监听器类型校验。
	// Internal helper for broadcasting a message
	UE_API void BroadcastMessageInternal(FGameplayTag Channel, const UScriptStruct* StructType, const void* MessageBytes);

	// 注册类型擦除后的内部消息监听器。
	// Internal helper for registering a message listener
	UE_API FGameplayMessageListenerHandle RegisterListenerInternal(
		FGameplayTag Channel, 
		TFunction<void(FGameplayTag, const UScriptStruct*, const void*)>&& Callback,
		const UScriptStruct* StructType,
		EGameplayMessageMatch MatchType);

	UE_API void UnregisterListenerInternal(FGameplayTag Channel, int32 HandleID);

private:
	// 保存单个频道的全部监听器及递增句柄编号。
	// List of all entries for a given channel
	struct FChannelListenerList
	{
		TArray<FGameplayMessageListenerData> Listeners;
		int32 HandleID = 0;
	};

private:
	TMap<FGameplayTag, FChannelListenerList> ListenerMap;
};

#undef UE_API
