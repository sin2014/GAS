// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ReplicationGraphTypes.h"
#include "LyraReplicationGraphTypes.generated.h"

// Actor 类到 ReplicationGraph 节点的主路由策略枚举；每个类映射到其中一种策略。
// This is the main enum we use to route actors to the right replication node. Each class maps to one enum.
UENUM()
enum class EClassRepNodeMapping : uint32
{
	NotRouted,						/* 不进入通用节点，由 PlayerState 限频节点等专用逻辑处理。 */ // Doesn't map to any node. Used for special case actors that handled by special case nodes (ULyraReplicationGraphNode_PlayerStateFrequencyLimiter)
	RelevantAllConnections,			/* 路由到全局或流送关卡的 AlwaysRelevant 节点。 */ // Routes to an AlwaysRelevantNode or AlwaysRelevantStreamingLevelNode node

	// 从此项往下只能添加空间化策略；判断逻辑见 ULyraReplicationGraph::IsSpatialized。
	// ONLY SPATIALIZED Enums below here! See ULyraReplicationGraph::IsSpatialized

	Spatialize_Static,				/* 路由到 GridNode；Actor 不移动，无需每帧更新空间位置。 */ // Routes to GridNode: these actors don't move and don't need to be updated every frame.
	Spatialize_Dynamic,				/* 路由到 GridNode；Actor 经常移动，每帧更新空间位置。 */ // Routes to GridNode: these actors mode frequently and are updated once per frame.
	Spatialize_Dormancy,			/* 路由到 GridNode；休眠时按静态处理，唤醒后按动态处理，适用于非休眠时会移动的 Actor。 */ // Routes to GridNode: While dormant we treat as static. When flushed/not dormant dynamic. Note this is for things that "move while not dormant".
};

// 可直接应用到 Actor Class 的复制图设置，也可由 FRepGraphActorTemplateSettings 模板映射生成。
// Actor Class Settings that can be assigned directly to a Class.  Can also be mapped to a FRepGraphActorTemplateSettings 
USTRUCT()
struct FRepGraphActorClassSettings
{
	GENERATED_BODY()

	FRepGraphActorClassSettings() = default;

	// 要应用该设置的 Actor Class 名称。
	// Name of the Class the settings will be applied to
	UPROPERTY(EditAnywhere)
	FSoftClassPath ActorClass;

	// 是否把该 Class 的节点路由策略加入 ClassRepNodePolicies。
	// If we should add this Class' RepInfo to the ClassRepNodePolicies Map
	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
	bool bAddClassRepInfoToMap  = true;

	// 写入 ClassRepNodePolicies 时为该 Class 使用的路由策略。
	// What ClassNodeMapping we should use when adding Class to ClassRepNodePolicies Map
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bAddClassRepInfoToMap"))
	EClassRepNodeMapping ClassNodeMapping = EClassRepNodeMapping::NotRouted;

	// 是否为该 Class 配置 Multicast RPC 在无现有 ActorChannel 时的行为。
	// Should we add this to the RPC_Multicast_OpenChannelForClass map
	UPROPERTY(EditAnywhere, meta = (InlineEditConditionToggle))
	bool bAddToRPC_Multicast_OpenChannelForClassMap = false;

	// Multicast RPC 命中该配置时，是否允许为 Actor 主动打开复制 Channel。
	// If this is added to RPC_Multicast_OpenChannelForClass map then should we actually open a channel or not
	UPROPERTY(EditAnywhere, meta = (EditCondition = "bAddToRPC_Multicast_OpenChannelForClassMap"))
	bool bRPC_Multicast_OpenChannelForClass = true;

	UClass* GetStaticActorClass() const
	{
		UClass* StaticActorClass = nullptr;
		const FString ActorClassNameString = ActorClass.ToString();

		if (FPackageName::IsScriptPackage(ActorClassNameString))
		{
			StaticActorClass = FindObject<UClass>(nullptr, *ActorClassNameString, EFindObjectFlags::ExactClass);

			if (!StaticActorClass)
			{
				UE_LOG(LogTemp, Error, TEXT("FRepGraphActorClassSettings: Cannot Find Static Class for %s"), *ActorClassNameString);
			}
		}
		else
		{
			// 通过类路径同步加载，允许蓝图 Actor Class 使用自定义复制图设置。
			// Allow blueprints to be used for custom class settings
			StaticActorClass = (UClass*)StaticLoadObject(UClass::StaticClass(), nullptr, *ActorClassNameString);
			if (!StaticActorClass)
			{
				UE_LOG(LogTemp, Error, TEXT("FRepGraphActorClassSettings: Cannot Load Static Class for %s"), *ActorClassNameString);
			}
		}

		return StaticActorClass;
	}
};
