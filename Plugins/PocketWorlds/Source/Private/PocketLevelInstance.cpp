// Copyright Epic Games, Inc. All Rights Reserved.

#include "PocketLevelInstance.h"

#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "PocketLevel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PocketLevelInstance)

// 构造尚未绑定玩家和流送关卡的 Pocket 实例，实际配置由子系统调用 Initialize 完成。
UPocketLevelInstance::UPocketLevelInstance()
{

}

// 绑定本地玩家与 Pocket 数据资产，在指定位置创建动态流送实例并监听 Loaded/Shown 事件；资产无效或加载请求失败时返回 false。
bool UPocketLevelInstance::Initialize(ULocalPlayer* InLocalPlayer, UPocketLevel* InPocketLevel, FVector InSpawnPoint)
{
	LocalPlayer = InLocalPlayer;
	World = LocalPlayer->GetWorld();
	PocketLevel = InPocketLevel;
	Bounds = FBoxSphereBounds(FSphere(InSpawnPoint, PocketLevel->Bounds.GetAbsMax()));

	if (ensure(StreamingPocketLevel == nullptr))
	{
		if (ensure(!PocketLevel->Level.IsNull()))
		{
			bool bSuccess = false;
			StreamingPocketLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(LocalPlayer, PocketLevel->Level, Bounds.Origin, FRotator::ZeroRotator, bSuccess);

			if (ensure(bSuccess && StreamingPocketLevel))
			{
				StreamingPocketLevel->OnLevelLoaded.AddUniqueDynamic(this, &ThisClass::HandlePocketLevelLoaded);
				StreamingPocketLevel->OnLevelShown.AddUniqueDynamic(this, &ThisClass::HandlePocketLevelShown);
			}

			return bSuccess;
		}
	}

	return false;
}

// 请求加载并显示已经创建的动态关卡实例；尚未初始化时不执行任何操作。
void UPocketLevelInstance::StreamIn()
{
	if (StreamingPocketLevel)
	{
		StreamingPocketLevel->SetShouldBeVisible(true);
		StreamingPocketLevel->SetShouldBeLoaded(true);
	}
}

// 请求隐藏并卸载动态关卡实例，释放其世界内容但保留 Pocket 实例对象供后续复用。
void UPocketLevelInstance::StreamOut()
{
	if (StreamingPocketLevel)
	{
		StreamingPocketLevel->SetShouldBeVisible(false);
		StreamingPocketLevel->SetShouldBeLoaded(false);
	}
}

// 注册关卡可见后的就绪回调；若当前已是 LoadedVisible，则先立即通知一次，再保留委托供后续显示事件使用。
FDelegateHandle UPocketLevelInstance::AddReadyCallback(FPocketLevelInstanceEvent::FDelegate Callback)
{
	if (StreamingPocketLevel->GetLevelStreamingState() == ELevelStreamingState::LoadedVisible)
	{
		Callback.ExecuteIfBound(this);
	}
	
	return OnReadyEvent.Add(Callback);
}

// 使用委托句柄移除先前注册的 Pocket 就绪回调。
void UPocketLevelInstance::RemoveReadyCallback(FDelegateHandle CallbackToRemove)
{
	OnReadyEvent.Remove(CallbackToRemove);
}

// UObject 销毁时发起非阻塞卸载、解除流送事件绑定并丢弃关卡引用，防止异步回调进入已销毁实例。
void UPocketLevelInstance::BeginDestroy()
{
	Super::BeginDestroy();

	if (StreamingPocketLevel)
	{
		StreamingPocketLevel->bShouldBlockOnUnload = false;
		StreamingPocketLevel->SetShouldBeLoaded(false);
		StreamingPocketLevel->OnLevelShown.RemoveAll(this);
		StreamingPocketLevel->OnLevelLoaded.RemoveAll(this);
		StreamingPocketLevel = nullptr;
	}
}

// 动态关卡加载完成后把内容标记为客户端本地可见，并临时修正 Actor 角色与所有权，避免网络层等待服务端复制这些本地对象。
void UPocketLevelInstance::HandlePocketLevelLoaded()
{
	if (StreamingPocketLevel)
	{
		// 将关卡内容配置为纯客户端本地生成；通过角色交换标记避免把这些 Actor 当作等待服务端后续复制的远端生成对象。
		// Make everything in the level setup so that it's setup on the client, and we treat
		// everything as locally spawned, rather than bExchangedRoles = true, where it's spawned
		// on the client, but the expectation is the server said do it, and the server is going to 
		// be telling us about them later.
		if (ULevel* LoadedLevel = StreamingPocketLevel->GetLoadedLevel())
		{
			LoadedLevel->bClientOnlyVisible = true;

			for (AActor* Actor : LoadedLevel->Actors)
			{
				if (Actor)
				{
					// 临时方案：当 bClientOnlyVisible 足以表达本地语义后应移除。
					Actor->bExchangedRoles = true;  // HACK, Remove when bClientOnlyVisible is all we need.
				}
			}

			// TODO：共享 Pocket 空间不应统一归属于某个本地 PlayerController，需要单独处理所有权。
			// TODO: Don't put ownership over shared pocket spaces.
			if (LocalPlayer)
			{
				if (APlayerController* PC = LocalPlayer->GetPlayerController(GetWorld()))
				{
					for (AActor* Actor : LoadedLevel->Actors)
					{
						if (Actor)
						{
							Actor->SetOwner(PC);
						}
					}
				}
			}
		}
	}
}

// 关卡完成显示后广播就绪事件，使等待方可以安全访问已经进入世界的 Pocket 内容。
void UPocketLevelInstance::HandlePocketLevelShown()
{
	OnReadyEvent.Broadcast(this);
}

