// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "LyraVerbMessage.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "LyraVerbMessageReplication.generated.h"

class UObject;
struct FLyraVerbMessageReplication;
struct FNetDeltaSerializeInfo;

// Fast Array 中单条待复制的 Verb 消息；序列化状态由 FFastArraySerializerItem 维护。
/**
 * Represents one verb message
 */
USTRUCT(BlueprintType)
struct FLyraVerbMessageReplicationEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FLyraVerbMessageReplicationEntry()
	{}

	FLyraVerbMessageReplicationEntry(const FLyraVerbMessage& InMessage)
		: Message(InMessage)
	{
	}

	FString GetDebugString() const;

private:
	friend FLyraVerbMessageReplication;

	UPROPERTY()
	FLyraVerbMessage Message;
};

/** 通过 Fast Array 从服务器复制到客户端的 Verb 消息容器。 */
/** Container of verb messages to replicate */
USTRUCT(BlueprintType)
struct FLyraVerbMessageReplication : public FFastArraySerializer
{
	GENERATED_BODY()

	FLyraVerbMessageReplication()
	{
	}

public:
	void SetOwner(UObject* InOwner) { Owner = InOwner; }

	// 在服务器端追加消息并标记条目为脏，使其进入下一次 Fast Array 增量复制。
	// Broadcasts a message from server to clients
	void AddMessage(const FLyraVerbMessage& Message);

	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FLyraVerbMessageReplicationEntry, FLyraVerbMessageReplication>(CurrentMessages, DeltaParms, *this);
	}

private:
	void RebroadcastMessage(const FLyraVerbMessage& Message);

private:
	// 已复制的 Verb 消息条目；客户端收到新增或变更后会重新广播到本地消息子系统。
	// Replicated list of gameplay tag stacks
	UPROPERTY()
	TArray<FLyraVerbMessageReplicationEntry> CurrentMessages;
	
	// 持有该容器的对象，用于定位 World 及其 GameplayMessageSubsystem。
	// Owner (for a route to a world)
	UPROPERTY()
	TObjectPtr<UObject> Owner = nullptr;
};

template<>
struct TStructOpsTypeTraits<FLyraVerbMessageReplication> : public TStructOpsTypeTraitsBase2<FLyraVerbMessageReplication>
{
	enum
	{
		WithNetDeltaSerializer = true,
	};
};
