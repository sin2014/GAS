// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Lyra ReplicationGraph 概览
 *
 * 本系统不调用 AActor::IsNetRelevantFor，而由 ReplicationGraphNode 为每条连接生成待复制 Actor 列表。
 * 大部分列表可跨帧复用，使空间相关性收集能够在连接间共享。节点可以是全局、连接专用或由同队连接共享；
 * Actor 也允许同时进入多个节点，最终收集阶段会处理重复项，但配置时仍应尽量减少重复路由。
 *
 * 游戏代码不应直接调用 ULyraReplicationGraph。ReplicationGraph 在 InitGlobalActorClassSettings 中订阅团队变化等
 * 明确的游戏事件，从而集中掌握全部复制路由。主要节点包括二维空间网格、全连接 AlwaysRelevant 列表、
 * 每连接 AlwaysRelevant 列表、PlayerState 限频节点，以及基类创建的每连接 TearOff 节点。
 *
 * 距离相关 Actor 进入 GridSpatialization2D；全局常驻 Actor 进入 ActorList；Owner、Pawn、ViewTarget 等连接专属对象
 * 由 AlwaysRelevant_ForConnection 每帧收集；PlayerStateFrequencyLimiter 每帧滚动复制少量模拟 PlayerState，
 * 所有者自己的 PlayerState 则通过连接专属节点以更高频率复制。
 *
 * 要让 Actor 对所有连接常驻，可设置 bAlwaysRelevant；要仅对特定连接常驻，应扩展连接节点的收集逻辑，
 * 并将该 Class 路由设为 NotRouted，避免同时进入其他节点。调试时可先禁用 ReplicationGraph 排除普通复制问题，
 * 再使用 Net.RepGraph.PrintGraph、PrintAll、PrintAllActorInfo 和 Lyra.RepGraph.PrintRouting 等命令检查节点与路由。
 */
/**
*	
*	===================== LyraReplicationGraph Replication =====================
*
*	Overview
*	
*		This changes the way actor relevancy works. AActor::IsNetRelevantFor is NOT used in this system!
*		
*		Instead, The ULyraReplicationGraph contains UReplicationGraphNodes. These nodes are responsible for generating lists of actors to replicate for each connection.
*		Most of these lists are persistent across frames. This enables most of the gathering work ("which actors should be considered for replication) to be shared/reused.
*		Nodes may be global (used by all connections), connection specific (each connection gets its own node), or shared (e.g, teams: all connections on the same team share).
*		Actors can be in multiple nodes! For example a pawn may be in the spatialization node but also in the always-relevant-for-team node. It will be returned twice for 
*		teammates. This is ok though should be minimized when possible.
*		
*		ULyraReplicationGraph is intended to not be directly used by the game code. That is, you should not have to include LyraReplicationGraph.h anywhere else.
*		Rather, ULyraReplicationGraph depends on the game code and registers for events that the game code broadcasts (e.g., events for players joining/leaving teams).
*		This choice was made because it gives ULyraReplicationGraph a complete holistic view of actor replication. Rather than exposing generic public functions that any
*		place in game code can invoke, all notifications are explicitly registered in ULyraReplicationGraph::InitGlobalActorClassSettings.
*		
*	Lyra Nodes
*	
*		These are the top level nodes currently used:
*		
*		UReplicationGraphNode_GridSpatialization2D: 
*		This is the spatialization node. All "distance based relevant" actors will be routed here. This node divides the map into a 2D grid. Each cell in the grid contains 
*		children nodes that hold lists of actors based on how they update/go dormant. Actors are put in multiple cells. Connections pull from the single cell they are in.
*		
*		UReplicationGraphNode_ActorList
*		This is an actor list node that contains the always relevant actors. These actors are always relevant to every connection.
*		
*		ULyraReplicationGraphNode_AlwaysRelevant_ForConnection
*		This is the node for connection specific always relevant actors. This node does not maintain a persistent list but builds it each frame. This is possible because (currently)
*		these actors are all easily accessed from the PlayerController. A persistent list would require notifications to be broadcast when these actors change, which would be possible
*		but currently not necessary.
*		
*		ULyraReplicationGraphNode_PlayerStateFrequencyLimiter
*		A custom node for handling player state replication. This replicates a small rolling set of player states (currently 2/frame). This is so player states replicate
*		to simulated connections at a low, steady frequency, and to take advantage of serialization sharing. Auto proxy player states are replicated at higher frequency (to the
*		owning connection only) via ULyraReplicationGraphNode_AlwaysRelevant_ForConnection.
*		
*		UReplicationGraphNode_TearOff_ForConnection
*		Connection specific node for handling tear off actors. This is created and managed in the base implementation of Replication Graph.
*	
*	How To Use
*	
*		Making something always relevant: Please avoid if you can :) If you must, just setting AActor::bAlwaysRelevant = true in the class defaults will do it.
*		
*		Making something always relevant to connection: You will need to modify ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::GatherActorListsForConnection. You will also want 
*		to make sure the actor does not get put in one of the other nodes. The safest way to do this is by setting its EClassRepNodeMapping to NotRouted in ULyraReplicationGraph::InitGlobalActorClassSettings.
*
*	How To Debug
*	
*		Its a good idea to just disable rep graph to see if your problem is specific to this system or just general replication/game play problem.
*		
*		If it is replication graph related, there are several useful commands that can be used: see ReplicationGraph_Debugging.cpp. The most useful are below. Use the 'cheat' command to run these on the server from a client.
*	
*		"Net.RepGraph.PrintGraph" - this will print the graph to the log: each node and actor. 
*		"Net.RepGraph.PrintGraph class" - same as above but will group by class.
*		"Net.RepGraph.PrintGraph nclass" - same as above but will group by native classes (hides blueprint noise)
*		
*		Net.RepGraph.PrintAll <Frames> <ConnectionIdx> <"Class"/"Nclass"> -  will print the entire graph, the gathered actors, and how they were prioritized for a given connection for X amount of frames.
*		
*		Net.RepGraph.PrintAllActorInfo <ActorMatchString> - will print the class, global, and connection replication info associated with an actor/class. If MatchString is empty will print everything. Call directly from client.
*		
*		Lyra.RepGraph.PrintRouting - will print the EClassRepNodeMapping for each class. That is, how a given actor class is routed (or not) in the Replication Graph.
*	
*/

#include "LyraReplicationGraph.h"

#include "Net/UnrealNetwork.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"
#include "CoreGlobals.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerCategoryReplicator.h"
#endif

#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/NetConnection.h"
#include "UObject/UObjectIterator.h"

#include "LyraReplicationGraphSettings.h"
#include "Character/LyraCharacter.h"
#include "Player/LyraPlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraReplicationGraph)

// Lyra ReplicationGraph 初始化、路由和连接收集诊断使用的日志分类。
DEFINE_LOG_CATEGORY( LogLyraRepGraph );

namespace Lyra::RepGraph
{
	// 销毁信息忽略距离裁剪前允许复制的最大线性距离。
	float DestructionInfoMaxDist = 30000.f;
	// 将销毁信息最大距离暴露为运行时控制台变量。
	static FAutoConsoleVariableRef CVarLyraRepGraphDestructMaxDist(TEXT("Lyra.RepGraph.DestructInfo.MaxDist"), DestructionInfoMaxDist, TEXT("Max distance (not squared) to rep destruct infos at"), ECVF_Default);

	// 是否输出客户端流送关卡 AlwaysRelevant 列表变化的详细日志。
	int32 DisplayClientLevelStreaming = 0;
	// 将客户端流送关卡诊断开关暴露为控制台变量。
	static FAutoConsoleVariableRef CVarLyraRepGraphDisplayClientLevelStreaming(TEXT("Lyra.RepGraph.DisplayClientLevelStreaming"), DisplayClientLevelStreaming, TEXT(""), ECVF_Default);

	// 二维空间复制网格中每个单元格的边长。
	float CellSize = 10000.f;
	// 将空间网格单元尺寸暴露为控制台变量。
	static FAutoConsoleVariableRef CVarLyraRepGraphCellSize(TEXT("Lyra.RepGraph.CellSize"), CellSize, TEXT(""), ECVF_Default);

	// 空间复制网格的初始最小 X 偏移；Actor 超出范围时系统会自动重建。
	// Essentially "Min X" for replication. This is just an initial value. The system will reset itself if actors appears outside of this.
	float SpatialBiasX = -150000.f;
	// 将空间网格初始 X 偏移暴露为控制台变量。
	static FAutoConsoleVariableRef CVarLyraRepGraphSpatialBiasX(TEXT("Lyra.RepGraph.SpatialBiasX"), SpatialBiasX, TEXT(""), ECVF_Default);

	// 空间复制网格的初始最小 Y 偏移；Actor 超出范围时系统会自动重建。
	// Essentially "Min Y" for replication. This is just an initial value. The system will reset itself if actors appears outside of this.
	float SpatialBiasY = -200000.f;
	// 将空间网格初始 Y 偏移暴露为控制台变量。
	static FAutoConsoleVariableRef CVarLyraRepSpatialBiasY(TEXT("Lyra.RepGraph.SpatialBiasY"), SpatialBiasY, TEXT(""), ECVF_Default);

	// 动态空间化 Actor 的频率桶数量；桶越多，有效收集频率越低，并且先于 Actor 自身 NetUpdateFrequency 检查。
	// How many buckets to spread dynamic, spatialized actors across. High number = more buckets = smaller effective replication frequency. This happens before individual actors do their own NetUpdateFrequency check.
	int32 DynamicActorFrequencyBuckets = 3;
	// 将动态空间 Actor 的频率桶数量暴露为控制台变量。
	static FAutoConsoleVariableRef CVarLyraRepDynamicActorFrequencyBuckets(TEXT("Lyra.RepGraph.DynamicActorFrequencyBuckets"), DynamicActorFrequencyBuckets, TEXT(""), ECVF_Default);

	// 非零时禁止 Actor 超出初始边界后触发空间网格重建。
	int32 DisableSpatialRebuilds = 1;
	// 将空间网格重建禁用开关暴露为控制台变量。
	static FAutoConsoleVariableRef CVarLyraRepDisableSpatialRebuilds(TEXT("Lyra.RepGraph.DisableSpatialRebuilds"), DisableSpatialRebuilds, TEXT(""), ECVF_Default);

	// 是否记录运行期首次遇到的 Class 延迟初始化过程。
	int32 LogLazyInitClasses = 0;
	// 将 Class 延迟初始化诊断开关暴露为控制台变量。
	static FAutoConsoleVariableRef CVarLyraRepLogLazyInitClasses(TEXT("Lyra.RepGraph.LogLazyInitClasses"), LogLazyInitClasses, TEXT(""), ECVF_Default);

	// FastShared 移动更新的目标带宽，独立于 NetDriver 常规目标带宽。
	// How much bandwidth to use for FastShared movement updates. This is counted independently of the NetDriver's target bandwidth.
	int32 TargetKBytesSecFastSharedPath = 10;
	// 将 FastShared 路径目标带宽暴露为控制台变量。
	static FAutoConsoleVariableRef CVarLyraRepTargetKBytesSecFastSharedPath(TEXT("Lyra.RepGraph.TargetKBytesSecFastSharedPath"), TargetKBytesSecFastSharedPath, TEXT(""), ECVF_Default);

	// FastShared 路径相对于常规 CullDistance 的距离比例要求。
	float FastSharedPathCullDistPct = 0.80f;
	// 将 FastShared 距离比例暴露为控制台变量。
	static FAutoConsoleVariableRef CVarLyraRepFastSharedPathCullDistPct(TEXT("Lyra.RepGraph.FastSharedPathCullDistPct"), FastSharedPathCullDistPct, TEXT(""), ECVF_Default);

	// 是否启用 ActorListFrequencyBuckets 的 FastShared 更新路径。
	int32 EnableFastSharedPath = 1;
	// 将 FastShared 路径启用开关暴露为控制台变量。
	static FAutoConsoleVariableRef CVarLyraRepEnableFastSharedPath(TEXT("Lyra.RepGraph.EnableFastSharedPath"), EnableFastSharedPath, TEXT(""), ECVF_Default);

	// 仅为主 GameNetDriver 按项目设置创建复制图实例；禁用或其他 NetDriver 返回空。
	UReplicationDriver* ConditionalCreateReplicationDriver(UNetDriver* ForNetDriver, UWorld* World)
	{
		// 仅为主 GameNetDriver 创建 Lyra ReplicationGraph，忽略其他用途的 NetDriver。
		// Only create for GameNetDriver
		if (World && ForNetDriver && ForNetDriver->NetDriverName == NAME_GameNetDriver)
		{
			const ULyraReplicationGraphSettings* LyraRepGraphSettings = GetDefault<ULyraReplicationGraphSettings>();

			// 允许开发者设置显式禁用 ReplicationGraph。
			// Enable/Disable via developer settings
			if (LyraRepGraphSettings && LyraRepGraphSettings->bDisableReplicationGraph)
			{
				UE_LOG(LogLyraRepGraph, Display, TEXT("Replication graph is disabled via LyraReplicationGraphSettings."));
				return nullptr;
			}

			UE_LOG(LogLyraRepGraph, Display, TEXT("Replication graph is enabled for %s in world %s."), *GetNameSafe(ForNetDriver), *GetPathNameSafe(World));

			TSubclassOf<ULyraReplicationGraph> GraphClass;
			if (LyraRepGraphSettings)
			{
				GraphClass = LyraRepGraphSettings->DefaultReplicationGraphClass.TryLoadClass<ULyraReplicationGraph>();
			}

			if (GraphClass.Get() == nullptr)
			{
				GraphClass = ULyraReplicationGraph::StaticClass();
			}

			ULyraReplicationGraph* LyraReplicationGraph = NewObject<ULyraReplicationGraph>(GetTransientPackage(), GraphClass.Get());
			return LyraReplicationGraph;
		}

		return nullptr;
	}
};

// ----------------------------------------------------------------------------------------------------------

// 首个实例构造时安装全局 ReplicationDriver 工厂委托，使主 GameNetDriver 可创建 Lyra 复制图。
ULyraReplicationGraph::ULyraReplicationGraph()
{
	if (!UReplicationDriver::CreateReplicationDriverDelegate().IsBound())
	{
		UReplicationDriver::CreateReplicationDriverDelegate().BindLambda(
			[](UNetDriver* ForNetDriver, const FURL& URL, UWorld* World) -> UReplicationDriver*
			{
				return Lyra::RepGraph::ConditionalCreateReplicationDriver(ForNetDriver, World);
			});
	}
}

// World 状态重置时清空流送关卡常驻列表，并重置现有和待连接的每连接 AlwaysRelevant 节点缓存。
void ULyraReplicationGraph::ResetGameWorldState()
{
	Super::ResetGameWorldState();

	AlwaysRelevantStreamingLevelActors.Empty();

	for (UNetReplicationGraphConnection* ConnManager : Connections)
	{
		for (UReplicationGraphNode* ConnectionNode : ConnManager->GetConnectionGraphNodes())
		{
			if (ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = Cast<ULyraReplicationGraphNode_AlwaysRelevant_ForConnection>(ConnectionNode))
			{
				AlwaysRelevantConnectionNode->ResetGameWorldState();
			}
		}
	}

	for (UNetReplicationGraphConnection* ConnManager : PendingConnections)
	{
		for (UReplicationGraphNode* ConnectionNode : ConnManager->GetConnectionGraphNodes())
		{
			if (ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = Cast<ULyraReplicationGraphNode_AlwaysRelevant_ForConnection>(ConnectionNode))
			{
				AlwaysRelevantConnectionNode->ResetGameWorldState();
			}
		}
	}
}

// 优先读取显式 Class 策略，否则根据复制、AlwaysRelevant 和 Owner relevancy 标志及父类差异推导路由。
EClassRepNodeMapping ULyraReplicationGraph::GetClassNodeMapping(UClass* Class) const
{
	if (!Class)
	{
		return EClassRepNodeMapping::NotRouted;
	}
	
	if (const EClassRepNodeMapping* Ptr = ClassRepNodePolicies.FindWithoutClassRecursion(Class))
	{
		return *Ptr;
	}
	
	AActor* ActorCDO = Cast<AActor>(Class->GetDefaultObject());
	if (!ActorCDO || !ActorCDO->GetIsReplicated())
	{
		return EClassRepNodeMapping::NotRouted;
	}
		
	auto ShouldSpatialize = [](const AActor* CDO)
	{
		return CDO->GetIsReplicated() && (!(CDO->bAlwaysRelevant || CDO->bOnlyRelevantToOwner || CDO->bNetUseOwnerRelevancy));
	};

	auto GetLegacyDebugStr = [](const AActor* CDO)
	{
		return FString::Printf(TEXT("%s [%d/%d/%d]"), *CDO->GetClass()->GetName(), CDO->bAlwaysRelevant, CDO->bOnlyRelevantToOwner, CDO->bNetUseOwnerRelevancy);
	};

	// 仅当该 Class 的策略不同于父类时写入映射，避免为所有子类重复保存相同路由。
	// Only handle this class if it differs from its super. There is no need to put every child class explicitly in the graph class mapping
	UClass* SuperClass = Class->GetSuperClass();
	if (AActor* SuperCDO = Cast<AActor>(SuperClass->GetDefaultObject()))
	{
		if (SuperCDO->GetIsReplicated() == ActorCDO->GetIsReplicated()
			&& SuperCDO->bAlwaysRelevant == ActorCDO->bAlwaysRelevant
			&& SuperCDO->bOnlyRelevantToOwner == ActorCDO->bOnlyRelevantToOwner
			&& SuperCDO->bNetUseOwnerRelevancy == ActorCDO->bNetUseOwnerRelevancy
			)
		{
			return GetClassNodeMapping(SuperClass);
		}
	}

	if (ShouldSpatialize(ActorCDO))
	{
		return EClassRepNodeMapping::Spatialize_Dynamic;
	}
	else if (ActorCDO->bAlwaysRelevant && !ActorCDO->bOnlyRelevantToOwner)
	{
		return EClassRepNodeMapping::RelevantAllConnections;
	}

	return EClassRepNodeMapping::NotRouted;
}

// 计算指定 Actor Class 的节点策略并写入支持类继承查询的路由映射表。
void ULyraReplicationGraph::RegisterClassRepNodeMapping(UClass* Class)
{
	EClassRepNodeMapping Mapping = GetClassNodeMapping(Class);
	ClassRepNodePolicies.Set(Class, Mapping);
}

// 从 Actor CDO 提取 CullDistance 和 NetUpdateFrequency，初始化该 Class 的距离裁剪与复制帧周期。
void ULyraReplicationGraph::InitClassReplicationInfo(FClassReplicationInfo& Info, UClass* Class, bool Spatialize) const
{
	AActor* CDO = Class->GetDefaultObject<AActor>();
	if (Spatialize)
	{
		Info.SetCullDistanceSquared(CDO->GetNetCullDistanceSquared());
		UE_LOG(LogLyraRepGraph, Log, TEXT("Setting cull distance for %s to %f (%f)"), *Class->GetName(), Info.GetCullDistanceSquared(), Info.GetCullDistance());
	}

	Info.ReplicationPeriodFrame = GetReplicationPeriodFrameForFrequency(CDO->GetNetUpdateFrequency());

	UClass* NativeClass = Class;
	while (!NativeClass->IsNative() && NativeClass->GetSuperClass() && NativeClass->GetSuperClass() != AActor::StaticClass())
	{
		NativeClass = NativeClass->GetSuperClass();
	}

	UE_LOG(LogLyraRepGraph, Log, TEXT("Setting replication period for %s (%s) to %d frames (%.2f)"), *Class->GetName(), *NativeClass->GetName(), Info.ReplicationPeriodFrame, CDO->GetNetUpdateFrequency());
}

// 显式配置类及其子类沿用已有信息；其余 Class 按当前路由是否空间化推导复制参数。
bool ULyraReplicationGraph::ConditionalInitClassReplicationInfo(UClass* ReplicatedClass, FClassReplicationInfo& ClassInfo)
{
	if (ExplicitlySetClasses.FindByPredicate([&](const UClass* SetClass) { return ReplicatedClass->IsChildOf(SetClass); }) != nullptr)
	{
		return false;
	}

	bool ClassIsSpatialized = IsSpatialized(ClassRepNodePolicies.GetChecked(ReplicatedClass));
	InitClassReplicationInfo(ClassInfo, ReplicatedClass, ClassIsSpatialized);
	return true;
}

// 写入项目显式 Class 路由，并警告 AlwaysRelevant Class 被错误配置到空间节点的情况。
void ULyraReplicationGraph::AddClassRepInfo(UClass* Class, EClassRepNodeMapping Mapping)
{
	if (IsSpatialized(Mapping))
	{
		if (Class->GetDefaultObject<AActor>()->bAlwaysRelevant)
		{
			UE_LOG(LogLyraRepGraph, Warning, TEXT("Replicated Class %s is AlwaysRelevant but is initialized into a spatialized node (%s)"), *Class->GetName(), *StaticEnum<EClassRepNodeMapping>()->GetNameStringByValue((int64)Mapping));
		}
	}

	ClassRepNodePolicies.Set(Class, Mapping);
}

// 为非显式继承类生成 FClassReplicationInfo 并登记到全局 Actor 复制信息表。
void ULyraReplicationGraph::RegisterClassReplicationInfo(UClass* ReplicatedClass)
{
	FClassReplicationInfo ClassInfo;
	if (ConditionalInitClassReplicationInfo(ReplicatedClass, ClassInfo))
	{
		GlobalActorReplicationInfoMap.SetClassInfo(ReplicatedClass, ClassInfo);
		UE_LOG(LogLyraRepGraph, Log, TEXT("Setting %s - %.2f"), *GetNameSafe(ReplicatedClass), ClassInfo.GetCullDistance());
	}
}

// 构建全部复制 Class 的路由和参数、角色 FastShared 策略、RPC 开 Channel 规则及延迟初始化回调。
void ULyraReplicationGraph::InitGlobalActorClassSettings()
{
	// 为当前未加载的 Class 注册延迟初始化回调，在首次出现时生成其复制配置。
	// Setup our lazy init function for classes that are not currently loaded.
	GlobalActorReplicationInfoMap.SetInitClassInfoFunc(
		[this](UClass* Class, FClassReplicationInfo& ClassInfo)
		{
			RegisterClassRepNodeMapping(Class); /* 节点路由必须先于 Class 复制信息注册。 */ // This needs to run before RegisterClassReplicationInfo.

			const bool bHandled = ConditionalInitClassReplicationInfo(Class, ClassInfo);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (Lyra::RepGraph::LogLazyInitClasses != 0)
			{
				if (bHandled)
				{
					EClassRepNodeMapping Mapping = ClassRepNodePolicies.GetChecked(Class);
					UE_LOG(LogLyraRepGraph, Warning, TEXT("%s was Lazy Initialized. (Parent: %s) %d."), *GetNameSafe(Class), *GetNameSafe(Class->GetSuperClass()), (int32)Mapping);

					FClassReplicationInfo& ParentRepInfo = GlobalActorReplicationInfoMap.GetClassInfo(Class->GetSuperClass());
					if (ClassInfo.BuildDebugStringDelta() != ParentRepInfo.BuildDebugStringDelta())
					{
						UE_LOG(LogLyraRepGraph, Warning, TEXT("Differences Found!"));
						FString DebugStr = ParentRepInfo.BuildDebugStringDelta();
						UE_LOG(LogLyraRepGraph, Warning, TEXT("  Parent: %s"), *DebugStr);

						DebugStr = ClassInfo.BuildDebugStringDelta();
						UE_LOG(LogLyraRepGraph, Warning, TEXT("  Class : %s"), *DebugStr);
					}
				}
				else
				{
					UE_LOG(LogLyraRepGraph, Warning, TEXT("%s skipped Lazy Initialization because it does not differ from its parent. (Parent: %s)"), *GetNameSafe(Class), *GetNameSafe(Class->GetSuperClass()));

				}
			}
#endif

			return bHandled;
		});

	ClassRepNodePolicies.InitNewElement = [this](UClass* Class, EClassRepNodeMapping& NodeMapping)
	{
		NodeMapping = GetClassNodeMapping(Class);
		return true;
	};

	const ULyraReplicationGraphSettings* LyraRepGraphSettings = GetDefault<ULyraReplicationGraphSettings>();
	check(LyraRepGraphSettings);

	// 应用项目设置中针对特定 Class 的节点路由覆盖。
	// Set Classes Node Mappings
	for (const FRepGraphActorClassSettings& ActorClassSettings : LyraRepGraphSettings->ClassSettings)
	{
		if (ActorClassSettings.bAddClassRepInfoToMap)
		{
			if (UClass* StaticActorClass = ActorClassSettings.GetStaticActorClass())
			{
				UE_LOG(LogLyraRepGraph, Log, TEXT("ActorClassSettings -- AddClassRepInfo - %s :: %i"), *StaticActorClass->GetName(), int(ActorClassSettings.ClassNodeMapping));
				AddClassRepInfo(StaticActorClass, ActorClassSettings.ClassNodeMapping);
			}
		}
	}

#if WITH_GAMEPLAY_DEBUGGER
	AddClassRepInfo(AGameplayDebuggerCategoryReplicator::StaticClass(), EClassRepNodeMapping::NotRouted);				/* 由 ULyraReplicationGraphNode_AlwaysRelevant_ForConnection 负责复制。 */ // Replicated via ULyraReplicationGraphNode_AlwaysRelevant_ForConnection
#endif

	TArray<UClass*> AllReplicatedClasses;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		AActor* ActorCDO = Cast<AActor>(Class->GetDefaultObject());
		if (!ActorCDO || !ActorCDO->GetIsReplicated())
		{
			continue;
		}

		// 跳过蓝图编译产生的 SKEL_ 与 REINST_ 临时 Class，避免污染复制配置。
		// Skip SKEL and REINST classes. I don't know a better way to do this.
		if (Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}

		// --------------------------------------------------------------------
		// 记录启用复制的 Class，第二遍再统一构建 FClassReplicationInfo。
		// This is a replicated class. Save this off for the second pass below
		// --------------------------------------------------------------------

		AllReplicatedClasses.Add(Class);

		RegisterClassRepNodeMapping(Class);
	}

	// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// 构建每个 Class 的 FClassReplicationInfo；部分由 Lyra 显式设置，其余从 AActor 传统复制属性推导。
	// Setup FClassReplicationInfo. This is essentially the per class replication settings. Some we set explicitly, the rest we are setting via looking at the legacy settings on AActor.
	// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	auto SetClassInfo = [&](UClass* Class, const FClassReplicationInfo& Info) { GlobalActorReplicationInfoMap.SetClassInfo(Class, Info); ExplicitlySetClasses.Add(Class); };
	ExplicitlySetClasses.Reset();

	FClassReplicationInfo CharacterClassRepInfo;
	CharacterClassRepInfo.DistancePriorityScale = 1.f;
	CharacterClassRepInfo.StarvationPriorityScale = 1.f;
	CharacterClassRepInfo.ActorChannelFrameTimeout = 4;
	CharacterClassRepInfo.SetCullDistanceSquared(ALyraCharacter::StaticClass()->GetDefaultObject<ALyraCharacter>()->GetNetCullDistanceSquared());

	SetClassInfo(ACharacter::StaticClass(), CharacterClassRepInfo);

	{
		// 校验 FSharedRepMovement 的量化设置与默认 LyraCharacter 的 RepMovement 一致。
		// Sanity check our FSharedRepMovement type has the same quantization settings as the default character.
		FRepMovement DefaultRepMovement = ALyraCharacter::StaticClass()->GetDefaultObject<ALyraCharacter>()->GetReplicatedMovement(); /* 使用默认角色 RepMovement 的相同量化设置。 */ // Use the same quantization settings as our default replicatedmovement
		FSharedRepMovement SharedRepMovement;
		ensureMsgf(SharedRepMovement.RepMovement.LocationQuantizationLevel == DefaultRepMovement.LocationQuantizationLevel, TEXT("LocationQuantizationLevel mismatch. %d != %d"), (uint8)SharedRepMovement.RepMovement.LocationQuantizationLevel, (uint8)DefaultRepMovement.LocationQuantizationLevel);
		ensureMsgf(SharedRepMovement.RepMovement.VelocityQuantizationLevel == DefaultRepMovement.VelocityQuantizationLevel, TEXT("VelocityQuantizationLevel mismatch. %d != %d"), (uint8)SharedRepMovement.RepMovement.VelocityQuantizationLevel, (uint8)DefaultRepMovement.VelocityQuantizationLevel);
		ensureMsgf(SharedRepMovement.RepMovement.RotationQuantizationLevel == DefaultRepMovement.RotationQuantizationLevel, TEXT("RotationQuantizationLevel mismatch. %d != %d"), (uint8)SharedRepMovement.RepMovement.RotationQuantizationLevel, (uint8)DefaultRepMovement.RotationQuantizationLevel);
	}

	// ------------------------------------------------------------------------------------------------------
	// 为 Pawn 配置 FastShared 复制函数；每个 Pawn 每帧最多调用一次，决定是否向全部相关连接共享移动更新。
	//	Setup FastShared replication for pawns. This is called up to once per frame per pawn to see if it wants
	//	to send a FastShared update to all relevant connections.
	// ------------------------------------------------------------------------------------------------------
	CharacterClassRepInfo.FastSharedReplicationFunc = [](AActor* Actor)
	{
		bool bSuccess = false;
		if (ALyraCharacter* Character = Cast<ALyraCharacter>(Actor))
		{
			bSuccess = Character->UpdateSharedReplication();
		}
		return bSuccess;
	};

	CharacterClassRepInfo.FastSharedReplicationFuncName = FName(TEXT("FastSharedReplication"));

	FastSharedPathConstants.MaxBitsPerFrame = (int32)((float)(Lyra::RepGraph::TargetKBytesSecFastSharedPath * 1024 * 8) / NetDriver->GetNetServerMaxTickRate());
	FastSharedPathConstants.DistanceRequirementPct = Lyra::RepGraph::FastSharedPathCullDistPct;

	SetClassInfo(ALyraCharacter::StaticClass(), CharacterClassRepInfo);

	// ---------------------------------------------------------------------
	UReplicationGraphNode_ActorListFrequencyBuckets::DefaultSettings.ListSize = 12;
	UReplicationGraphNode_ActorListFrequencyBuckets::DefaultSettings.NumBuckets = Lyra::RepGraph::DynamicActorFrequencyBuckets;
	UReplicationGraphNode_ActorListFrequencyBuckets::DefaultSettings.BucketThresholds.Reset();
	UReplicationGraphNode_ActorListFrequencyBuckets::DefaultSettings.EnableFastPath = (Lyra::RepGraph::EnableFastSharedPath > 0);
	UReplicationGraphNode_ActorListFrequencyBuckets::DefaultSettings.FastPathFrameModulo = 1;

	RPCSendPolicyMap.Reset();

	// 为所有复制 Class 根据传统 AActor 设置补全 FClassReplicationInfo。
	// Set FClassReplicationInfo based on legacy settings from all replicated classes
	for (UClass* ReplicatedClass : AllReplicatedClasses)
	{
		RegisterClassReplicationInfo(ReplicatedClass);
	}

	// 输出最终推导出的 Class 路由与复制配置，便于启动诊断。
	// Print out what we came up with
	UE_LOG(LogLyraRepGraph, Log, TEXT(""));
	UE_LOG(LogLyraRepGraph, Log, TEXT("Class Routing Map: "));
	for (auto ClassMapIt = ClassRepNodePolicies.CreateIterator(); ClassMapIt; ++ClassMapIt)
	{
		UClass* Class = CastChecked<UClass>(ClassMapIt.Key().ResolveObjectPtr());
		EClassRepNodeMapping Mapping = ClassMapIt.Value();

		// 蓝图 Class 仅在配置不同于其原生父类时单独输出。
		// Only print if different than native class
		UClass* ParentNativeClass = GetParentNativeClass(Class);

		EClassRepNodeMapping* ParentMapping = ClassRepNodePolicies.Get(ParentNativeClass);
		if (ParentMapping && Class != ParentNativeClass && Mapping == *ParentMapping)
		{
			continue;
		}

		UE_LOG(LogLyraRepGraph, Log, TEXT("  %s (%s) -> %s"), *Class->GetName(), *GetNameSafe(ParentNativeClass), *StaticEnum<EClassRepNodeMapping>()->GetNameStringByValue((int64)Mapping));
	}

	UE_LOG(LogLyraRepGraph, Log, TEXT(""));
	UE_LOG(LogLyraRepGraph, Log, TEXT("Class Settings Map: "));
	FClassReplicationInfo DefaultValues;
	for (auto ClassRepInfoIt = GlobalActorReplicationInfoMap.CreateClassMapIterator(); ClassRepInfoIt; ++ClassRepInfoIt)
	{
		UClass* Class = CastChecked<UClass>(ClassRepInfoIt.Key().ResolveObjectPtr());
		const FClassReplicationInfo& ClassInfo = ClassRepInfoIt.Value();
		UE_LOG(LogLyraRepGraph, Log, TEXT("  %s (%s) -> %s"), *Class->GetName(), *GetNameSafe(GetParentNativeClass(Class)), *ClassInfo.BuildDebugStringDelta());
	}


	// 根据 CVar 配置复制销毁信息的最大距离平方。
	// Rep destruct infos based on CVar value
	DestructInfoMaxDistanceSquared = Lyra::RepGraph::DestructionInfoMaxDist * Lyra::RepGraph::DestructionInfoMaxDist;

#if WITH_GAMEPLAY_DEBUGGER
	AGameplayDebuggerCategoryReplicator::NotifyDebuggerOwnerChange.AddUObject(this, &ThisClass::OnGameplayDebuggerOwnerChange);
#endif

	// 构建各 Class 的 Multicast RPC 是否可主动打开 ActorChannel 的策略表。
	// Add to RPC_Multicast_OpenChannelForClass map
	RPC_Multicast_OpenChannelForClass.Reset();
	RPC_Multicast_OpenChannelForClass.Set(AActor::StaticClass(), true); /* 默认允许 Multicast RPC 主动打开 ActorChannel。 */ // Open channels for multicast RPCs by default
	RPC_Multicast_OpenChannelForClass.Set(AController::StaticClass(), false); /* Controller 禁止由 Multicast 打开 Channel；非 Owner 连接打开 Channel 会破坏其复制。 */ // multicasts should never open channels on Controllers since opening a channel on a non-owner breaks the Controller's replication.
	RPC_Multicast_OpenChannelForClass.Set(AServerStatReplicator::StaticClass(), false);

	for (const FRepGraphActorClassSettings& ActorClassSettings : LyraRepGraphSettings->ClassSettings)
	{
		if (ActorClassSettings.bAddToRPC_Multicast_OpenChannelForClassMap)
		{
			if (UClass* StaticActorClass = ActorClassSettings.GetStaticActorClass())
			{
				UE_LOG(LogLyraRepGraph, Log, TEXT("ActorClassSettings -- RPC_Multicast_OpenChannelForClass - %s"), *StaticActorClass->GetName());
				RPC_Multicast_OpenChannelForClass.Set(StaticActorClass, ActorClassSettings.bRPC_Multicast_OpenChannelForClass);
			}
		}
	}
}

// 创建二维空间网格、全连接 AlwaysRelevant 列表和 PlayerState 限频三个全局复制节点。
void ULyraReplicationGraph::InitGlobalGraphNodes()
{
	// -----------------------------------------------
	// 创建二维空间化网格节点，接收距离相关的静态、动态和休眠 Actor。
	//	Spatial Actors
	// -----------------------------------------------

	GridNode = CreateNewNode<UReplicationGraphNode_GridSpatialization2D>();
	GridNode->CellSize = Lyra::RepGraph::CellSize;
	GridNode->SpatialBias = FVector2D(Lyra::RepGraph::SpatialBiasX, Lyra::RepGraph::SpatialBiasY);

	if (Lyra::RepGraph::DisableSpatialRebuilds)
	{
		GridNode->AddToClassRebuildDenyList(AActor::StaticClass()); /* 禁止所有 Class 触发空间网格重建。 */ // Disable All spatial rebuilding
	}
	
	AddGlobalGraphNode(GridNode);

	// -----------------------------------------------
	// 创建对所有连接始终相关的全局 ActorList 节点。
	//	Always Relevant (to everyone) Actors
	// -----------------------------------------------
	AlwaysRelevantNode = CreateNewNode<UReplicationGraphNode_ActorList>();
	AddGlobalGraphNode(AlwaysRelevantNode);

	// -----------------------------------------------
	// 创建 PlayerState 限频节点，每帧滚动返回一部分 PlayerState。
	//	Player State specialization. This will return a rolling subset of the player states to replicate
	// -----------------------------------------------
	ULyraReplicationGraphNode_PlayerStateFrequencyLimiter* PlayerStateNode = CreateNewNode<ULyraReplicationGraphNode_PlayerStateFrequencyLimiter>();
	AddGlobalGraphNode(PlayerStateNode);
}

// 为单条网络连接创建专属 AlwaysRelevant 节点，并绑定其客户端流送关卡可见性事件。
void ULyraReplicationGraph::InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection)
{
	Super::InitConnectionGraphNodes(RepGraphConnection);

	ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = CreateNewNode<ULyraReplicationGraphNode_AlwaysRelevant_ForConnection>();

	// 连接专属节点需要监听客户端流送关卡可见性变化，以维护对应关卡的 AlwaysRelevant Actor。
	// This node needs to know when client levels go in and out of visibility
	RepGraphConnection->OnClientVisibleLevelNameAdd.AddUObject(AlwaysRelevantConnectionNode, &ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityAdd);
	RepGraphConnection->OnClientVisibleLevelNameRemove.AddUObject(AlwaysRelevantConnectionNode, &ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityRemove);

	AddConnectionGraphNode(AlwaysRelevantConnectionNode, RepGraphConnection);
}

// 查询 Class 的继承感知路由策略，未登记时回退为 NotRouted。
EClassRepNodeMapping ULyraReplicationGraph::GetMappingPolicy(UClass* Class)
{
	EClassRepNodeMapping* PolicyPtr = ClassRepNodePolicies.Get(Class);
	EClassRepNodeMapping Policy = PolicyPtr ? *PolicyPtr : EClassRepNodeMapping::NotRouted;
	return Policy;
}

// 按 Class 策略将新 Actor 加入全局常驻列表、流送关卡列表或对应静态、动态、休眠空间网格路径。
void ULyraReplicationGraph::RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo)
{
	EClassRepNodeMapping Policy = GetMappingPolicy(ActorInfo.Class);
	switch(Policy)
	{
		case EClassRepNodeMapping::NotRouted:
		{
			break;
		}
		
		case EClassRepNodeMapping::RelevantAllConnections:
		{
			if (ActorInfo.StreamingLevelName == NAME_None)
			{
				AlwaysRelevantNode->NotifyAddNetworkActor(ActorInfo);
			}
			else
			{
				FActorRepListRefView& RepList = AlwaysRelevantStreamingLevelActors.FindOrAdd(ActorInfo.StreamingLevelName);
				RepList.ConditionalAdd(ActorInfo.Actor);
			}
			break;
		}

		case EClassRepNodeMapping::Spatialize_Static:
		{
			GridNode->AddActor_Static(ActorInfo, GlobalInfo);
			break;
		}
		
		case EClassRepNodeMapping::Spatialize_Dynamic:
		{
			GridNode->AddActor_Dynamic(ActorInfo, GlobalInfo);
			break;
		}
		
		case EClassRepNodeMapping::Spatialize_Dormancy:
		{
			GridNode->AddActor_Dormancy(ActorInfo, GlobalInfo);
			break;
		}
	};
}

// 从此前路由的节点移除 Actor，并让全连接常驻 Actor 的销毁信息绕过距离裁剪。
void ULyraReplicationGraph::RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo)
{
	EClassRepNodeMapping Policy = GetMappingPolicy(ActorInfo.Class);
	switch(Policy)
	{
		case EClassRepNodeMapping::NotRouted:
		{
			break;
		}
		
		case EClassRepNodeMapping::RelevantAllConnections:
		{
			if (ActorInfo.StreamingLevelName == NAME_None)
			{
				AlwaysRelevantNode->NotifyRemoveNetworkActor(ActorInfo);
			}
			else
			{
				FActorRepListRefView& RepList = AlwaysRelevantStreamingLevelActors.FindChecked(ActorInfo.StreamingLevelName);
				if (RepList.RemoveFast(ActorInfo.Actor) == false)
				{
					UE_LOG(LogLyraRepGraph, Warning, TEXT("Actor %s was not found in AlwaysRelevantStreamingLevelActors list. LevelName: %s"), *GetActorRepListTypeDebugString(ActorInfo.Actor), *ActorInfo.StreamingLevelName.ToString());
				}				
			}

			SetActorDestructionInfoToIgnoreDistanceCulling(ActorInfo.GetActor());

			break;
		}

		case EClassRepNodeMapping::Spatialize_Static:
		{
			GridNode->RemoveActor_Static(ActorInfo);
			break;
		}
		
		case EClassRepNodeMapping::Spatialize_Dynamic:
		{
			GridNode->RemoveActor_Dynamic(ActorInfo);
			break;
		}
		
		case EClassRepNodeMapping::Spatialize_Dormancy:
		{
			GridNode->RemoveActor_Dormancy(ActorInfo);
			break;
		}
	};
}

// 由于监听全局静态事件，PIE 中必须过滤其他 World 的广播，避免跨 World 修改路由。
// Since we listen to global (static) events, we need to watch out for cross world broadcasts (PIE)
#if WITH_EDITOR
#define CHECK_WORLDS(X) if(X->GetWorld() != GetWorld()) return;
#else
#define CHECK_WORLDS(X)
#endif

#if WITH_GAMEPLAY_DEBUGGER
// GameplayDebugger 更换复制所有者时，将其从旧连接节点移除并挂到新所有者的专属常驻节点。
void ULyraReplicationGraph::OnGameplayDebuggerOwnerChange(AGameplayDebuggerCategoryReplicator* Debugger, APlayerController* OldOwner)
{
	CHECK_WORLDS(Debugger);

	auto GetAlwaysRelevantForConnectionNode = [this](APlayerController* Controller) -> ULyraReplicationGraphNode_AlwaysRelevant_ForConnection*
	{
		if (Controller)
		{
			if (UNetConnection* NetConnection = Controller->GetNetConnection())
			{
				if (NetConnection->GetDriver() == NetDriver)
				{
					if (UNetReplicationGraphConnection* GraphConnection = FindOrAddConnectionManager(NetConnection))
					{
						for (UReplicationGraphNode* ConnectionNode : GraphConnection->GetConnectionGraphNodes())
						{
							if (ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = Cast<ULyraReplicationGraphNode_AlwaysRelevant_ForConnection>(ConnectionNode))
							{
								return AlwaysRelevantConnectionNode;
							}
						}
					}
				}
			}
		}

		return nullptr;
	};

	if (ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = GetAlwaysRelevantForConnectionNode(OldOwner))
	{
		AlwaysRelevantConnectionNode->GameplayDebugger = nullptr;
	}

	if (ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = GetAlwaysRelevantForConnectionNode(Debugger->GetReplicationOwner()))
	{
		AlwaysRelevantConnectionNode->GameplayDebugger = Debugger;
	}
}
#endif

#undef CHECK_WORLDS

// ------------------------------------------------------------------------------

// 清空该连接逐帧构建的 Actor 列表及仍需复制的可见流送关卡缓存。
void ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::ResetGameWorldState()
{
	ReplicationActorList.Reset();
	AlwaysRelevantStreamingLevelsNeedingReplication.Empty();
}

// 为连接收集 Viewer、ViewTarget、自有 Pawn/PlayerState、可见流送关卡常驻 Actor 及可选 GameplayDebugger。
void ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	ULyraReplicationGraph* LyraGraph = CastChecked<ULyraReplicationGraph>(GetOuter());

	ReplicationActorList.Reset();

	for (const FNetViewer& CurViewer : Params.Viewers)
	{
		ReplicationActorList.ConditionalAdd(CurViewer.InViewer);
		ReplicationActorList.ConditionalAdd(CurViewer.ViewTarget);

		if (ALyraPlayerController* PC = Cast<ALyraPlayerController>(CurViewer.InViewer))
		{
			// 按连接序号与复制帧奇偶性将 PlayerState 更新节流到约 50%。
			// 50% throttling of PlayerStates.
			const bool bReplicatePS = (Params.ConnectionManager.ConnectionOrderNum % 2) == (Params.ReplicationFrameNum % 2);
			if (bReplicatePS)
			{
				// 所有者自己的 PlayerState 始终通过连接节点返回；其他模拟 PlayerState 由限频节点处理。
				// Always return the player state to the owning player. Simulated proxy player states are handled by ULyraReplicationGraphNode_PlayerStateFrequencyLimiter
				if (APlayerState* PS = PC->PlayerState)
				{
					if (!bInitializedPlayerState)
					{
						bInitializedPlayerState = true;
						FConnectionReplicationActorInfo& ConnectionActorInfo = Params.ConnectionManager.ActorInfoMap.FindOrAdd(PS);
						ConnectionActorInfo.ReplicationPeriodFrame = 1;
					}

					ReplicationActorList.ConditionalAdd(PS);
				}
			}

			FCachedAlwaysRelevantActorInfo& LastData = PastRelevantActorMap.FindOrAdd(CurViewer.Connection);

			if (ALyraCharacter* Pawn = Cast<ALyraCharacter>(PC->GetPawn()))
			{
				UpdateCachedRelevantActor(Params, Pawn, LastData.LastViewer);

				if (Pawn != CurViewer.ViewTarget)
				{
					ReplicationActorList.ConditionalAdd(Pawn);
				}
			}

			if (ALyraCharacter* ViewTargetPawn = Cast<ALyraCharacter>(CurViewer.ViewTarget))
			{
				UpdateCachedRelevantActor(Params, ViewTargetPawn, LastData.LastViewTarget);
			}
		}
	}

	CleanupCachedRelevantActors(PastRelevantActorMap);

	// 收集客户端当前可见流送关卡中的 AlwaysRelevant Actor。
	// Always relevant streaming level actors.
	FPerConnectionActorInfoMap& ConnectionActorInfoMap = Params.ConnectionManager.ActorInfoMap;
	
	TMap<FName, FActorRepListRefView>& AlwaysRelevantStreamingLevelActors = LyraGraph->AlwaysRelevantStreamingLevelActors;

	for (int32 Idx=AlwaysRelevantStreamingLevelsNeedingReplication.Num()-1; Idx >= 0; --Idx)
	{
		const FName& StreamingLevel = AlwaysRelevantStreamingLevelsNeedingReplication[Idx];

		FActorRepListRefView* Ptr = AlwaysRelevantStreamingLevelActors.Find(StreamingLevel);
		if (Ptr == nullptr)
		{
			// 该关卡已无 AlwaysRelevant 列表，从连接可见关卡缓存中移除。
			// No always relevant lists for that level
			UE_CLOG(Lyra::RepGraph::DisplayClientLevelStreaming > 0, LogLyraRepGraph, Display, TEXT("CLIENTSTREAMING Removing %s from AlwaysRelevantStreamingLevelActors because FActorRepListRefView is null. %s "), *StreamingLevel.ToString(),  *Params.ConnectionManager.GetName());
			AlwaysRelevantStreamingLevelsNeedingReplication.RemoveAtSwap(Idx, EAllowShrinking::No);
			continue;
		}

		FActorRepListRefView& RepList = *Ptr;

		if (RepList.Num() > 0)
		{
			bool bAllDormant = true;
			for (FActorRepListType Actor : RepList)
			{
				FConnectionReplicationActorInfo& ConnectionActorInfo = ConnectionActorInfoMap.FindOrAdd(Actor);
				if (ConnectionActorInfo.bDormantOnConnection == false)
				{
					bAllDormant = false;
					break;
				}
			}

			if (bAllDormant)
			{
				UE_CLOG(Lyra::RepGraph::DisplayClientLevelStreaming > 0, LogLyraRepGraph, Display, TEXT("CLIENTSTREAMING All AlwaysRelevant Actors Dormant on StreamingLevel %s for %s. Removing list."), *StreamingLevel.ToString(), *Params.ConnectionManager.GetName());
				AlwaysRelevantStreamingLevelsNeedingReplication.RemoveAtSwap(Idx, EAllowShrinking::No);
			}
			else
			{
				UE_CLOG(Lyra::RepGraph::DisplayClientLevelStreaming > 0, LogLyraRepGraph, Display, TEXT("CLIENTSTREAMING Adding always Actors on StreamingLevel %s for %s because it has at least one non dormant actor"), *StreamingLevel.ToString(), *Params.ConnectionManager.GetName());
				Params.OutGatheredReplicationLists.AddReplicationActorList(RepList);
			}
		}
		else
		{
			UE_LOG(LogLyraRepGraph, Warning, TEXT("ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::GatherActorListsForConnection - empty RepList %s"), *Params.ConnectionManager.GetName());
		}

	}

#if WITH_GAMEPLAY_DEBUGGER
	if (GameplayDebugger)
	{
		ReplicationActorList.ConditionalAdd(GameplayDebugger);
	}
#endif

	Params.OutGatheredReplicationLists.AddReplicationActorList(ReplicationActorList);
}

// 客户端显示流送关卡时，将其加入该连接需要检查的 AlwaysRelevant 关卡集合。
void ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityAdd(FName LevelName, UWorld* StreamingWorld)
{
	UE_CLOG(Lyra::RepGraph::DisplayClientLevelStreaming > 0, LogLyraRepGraph, Display, TEXT("CLIENTSTREAMING ::OnClientLevelVisibilityAdd - %s"), *LevelName.ToString());
	AlwaysRelevantStreamingLevelsNeedingReplication.Add(LevelName);
}

// 客户端隐藏流送关卡时，从该连接的 AlwaysRelevant 关卡集合移除名称。
void ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityRemove(FName LevelName)
{
	UE_CLOG(Lyra::RepGraph::DisplayClientLevelStreaming > 0, LogLyraRepGraph, Display, TEXT("CLIENTSTREAMING ::OnClientLevelVisibilityRemove - %s"), *LevelName.ToString());
	AlwaysRelevantStreamingLevelsNeedingReplication.Remove(LevelName);
}

// 输出连接专属 Actor 列表及各可见流送关卡对应的全局常驻列表。
void ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const
{
	DebugInfo.Log(NodeName);
	DebugInfo.PushIndent();
	LogActorRepList(DebugInfo, NodeName, ReplicationActorList);

	for (const FName& LevelName : AlwaysRelevantStreamingLevelsNeedingReplication)
	{
		ULyraReplicationGraph* LyraGraph = CastChecked<ULyraReplicationGraph>(GetOuter());
		if (FActorRepListRefView* RepList = LyraGraph->AlwaysRelevantStreamingLevelActors.Find(LevelName))
		{
			LogActorRepList(DebugInfo, FString::Printf(TEXT("AlwaysRelevant StreamingLevel List: %s"), *LevelName.ToString()), *RepList);
		}
	}

	DebugInfo.PopIndent();
}

// ------------------------------------------------------------------------------

// 要求复制图每帧先调用 PrepareForReplication，以重建 PlayerState 频率桶。
ULyraReplicationGraphNode_PlayerStateFrequencyLimiter::ULyraReplicationGraphNode_PlayerStateFrequencyLimiter()
{
	bRequiresPrepareForReplicationCall = true;
}

// 遍历当前有效 PlayerState，按每帧目标数量重建紧凑桶列表，并清空强制更新列表。
void ULyraReplicationGraphNode_PlayerStateFrequencyLimiter::PrepareForReplication()
{
	ReplicationActorLists.Reset();
	ForceNetUpdateReplicationActorList.Reset();

	ReplicationActorLists.AddDefaulted();
	FActorRepListRefView* CurrentList = &ReplicationActorLists[0];

	// 每帧重建 PlayerState 列表虽然不是最高效，但能简单处理玩家断开并保持数组紧凑；持久列表则需要离场时整理碎片。
	// We rebuild our lists of player states each frame. This is not as efficient as it could be but its the simplest way
	// to handle players disconnecting and keeping the lists compact. If the lists were persistent we would need to defrag them as players left.

	for (TActorIterator<APlayerState> It(GetWorld()); It; ++It)
	{
		APlayerState* PS = *It;
		if (IsActorValidForReplicationGather(PS) == false)
		{
			continue;
		}

		if (CurrentList->Num() >= TargetActorsPerFrame)
		{
			ReplicationActorLists.AddDefaulted();
			CurrentList = &ReplicationActorLists.Last(); 
		}
		
		CurrentList->Add(PS);
	}	
}

// 按复制帧号轮转选择一个 PlayerState 桶，并附加强制网络更新列表供当前连接复制。
void ULyraReplicationGraphNode_PlayerStateFrequencyLimiter::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	const int32 ListIdx = Params.ReplicationFrameNum % ReplicationActorLists.Num();
	Params.OutGatheredReplicationLists.AddReplicationActorList(ReplicationActorLists[ListIdx]);

	if (ForceNetUpdateReplicationActorList.Num() > 0)
	{
		Params.OutGatheredReplicationLists.AddReplicationActorList(ForceNetUpdateReplicationActorList);
	}	
}

// 按桶编号输出 PlayerState 限频节点当前保存的全部 Actor 列表。
void ULyraReplicationGraphNode_PlayerStateFrequencyLimiter::LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const
{
	DebugInfo.Log(NodeName);
	DebugInfo.PushIndent();	

	int32 i=0;
	for (const FActorRepListRefView& List : ReplicationActorLists)
	{
		LogActorRepList(DebugInfo, FString::Printf(TEXT("Bucket[%d]"), i++), List);
	}

	DebugInfo.PopIndent();
}

// ------------------------------------------------------------------------------

// 将当前所有 Actor Class 到 RepGraph 节点策略的映射输出到全局日志设备。
void ULyraReplicationGraph::PrintRepNodePolicies()
{
	UEnum* Enum = StaticEnum<EClassRepNodeMapping>();
	if (!Enum)
	{
		return;
	}

	GLog->Logf(TEXT("===================================="));
	GLog->Logf(TEXT("Lyra Replication Routing Policies"));
	GLog->Logf(TEXT("===================================="));

	for (auto It = ClassRepNodePolicies.CreateIterator(); It; ++It)
	{
		FObjectKey ObjKey = It.Key();
		
		EClassRepNodeMapping Mapping = It.Value();

		GLog->Logf(TEXT("%-40s --> %s"), *GetNameSafe(ObjKey.ResolveObjectPtr()), *Enum->GetNameStringByValue(static_cast<uint32>(Mapping)));
	}
}

// 注册路由诊断命令，遍历现有 LyraReplicationGraph 实例并输出 Class 策略。
FAutoConsoleCommandWithWorldAndArgs LyraPrintRepNodePoliciesCmd(TEXT("Lyra.RepGraph.PrintRouting"),TEXT("Prints how actor classes are routed to RepGraph nodes"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (TObjectIterator<ULyraReplicationGraph> It; It; ++It)
		{
			It->PrintRepNodePolicies();
		}
	})
);

// ------------------------------------------------------------------------------

// 注册频率桶调整命令，将给定数量应用到当前所有 ActorListFrequencyBuckets 节点。
FAutoConsoleCommandWithWorldAndArgs ChangeFrequencyBucketsCmd(TEXT("Lyra.RepGraph.FrequencyBuckets"), TEXT("Resets frequency bucket count."), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray< FString >& Args, UWorld* World) 
{
	int32 Buckets = 1;
	if (Args.Num() > 0)
	{
		LexTryParseString<int32>(Buckets, *Args[0]);
	}

	UE_LOG(LogLyraRepGraph, Display, TEXT("Setting Frequency Buckets to %d"), Buckets);
	for (TObjectIterator<UReplicationGraphNode_ActorListFrequencyBuckets> It; It; ++It)
	{
		UReplicationGraphNode_ActorListFrequencyBuckets* Node = *It;
		Node->SetNonStreamingCollectionSize(Buckets);
	}
}));
