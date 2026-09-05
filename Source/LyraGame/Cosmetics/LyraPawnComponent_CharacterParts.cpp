// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cosmetics/LyraPawnComponent_CharacterParts.h"

#include "Components/SkeletalMeshComponent.h"
#include "Cosmetics/LyraCharacterPartTypes.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPawnComponent_CharacterParts)

class FLifetimeProperty;
class UPhysicsAsset;
class USkeletalMesh;
class UWorld;

//////////////////////////////////////////////////////////////////////

// 返回部件类与附着 SocketName 组成的调试字符串，部件类为空时使用 None。
FString FLyraAppliedCharacterPartEntry::GetDebugString() const
{
	return FString::Printf(TEXT("(PartClass: %s, Socket: %s, Instance: %s)"), *GetPathNameSafe(Part.PartClass), *Part.SocketName.ToString(), *GetPathNameSafe(SpawnedComponent));
}

//////////////////////////////////////////////////////////////////////

// FastArray 在客户端移除条目前，销毁对应 ChildActorComponent，并在批次结束后广播部件变化。
void FLyraCharacterPartList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	bool bDestroyedAnyActors = false;
	for (int32 Index : RemovedIndices)
	{
		FLyraAppliedCharacterPartEntry& Entry = Entries[Index];
		bDestroyedAnyActors |= DestroyActorForEntry(Entry);
	}

	if (bDestroyedAnyActors && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

// FastArray 在客户端新增条目后为每项生成 ChildActorComponent，并广播部件变化。
void FLyraCharacterPartList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	bool bCreatedAnyActors = false;
	for (int32 Index : AddedIndices)
	{
		FLyraAppliedCharacterPartEntry& Entry = Entries[Index];
		bCreatedAnyActors |= SpawnActorForEntry(Entry);
	}

	if (bCreatedAnyActors && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

// 复制条目内容变化时销毁旧 Actor 并重新生成，因为当前不支持原地传播部件配置变化。
void FLyraCharacterPartList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	bool bChangedAnyActors = false;

	// 不对已生成 Actor 原地传播条目修改；复制变化时直接销毁并重新创建。
	// We don't support dealing with propagating changes, just destroy and recreate
	for (int32 Index : ChangedIndices)
	{
		FLyraAppliedCharacterPartEntry& Entry = Entries[Index];

		bChangedAnyActors |= DestroyActorForEntry(Entry);
		bChangedAnyActors |= SpawnActorForEntry(Entry);
	}

	if (bChangedAnyActors && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

// 仅权威端分配递增 PartHandle、追加 FastArray 条目并标记脏数据，返回后续移除使用的句柄。
FLyraCharacterPartHandle FLyraCharacterPartList::AddEntry(FLyraCharacterPart NewPart)
{
	FLyraCharacterPartHandle Result;
	Result.PartHandle = PartHandleCounter++;

	if (ensure(OwnerComponent && OwnerComponent->GetOwner() && OwnerComponent->GetOwner()->HasAuthority()))
	{
		FLyraAppliedCharacterPartEntry& NewEntry = Entries.AddDefaulted_GetRef();
		NewEntry.Part = NewPart;
		NewEntry.PartHandle = Result.PartHandle;
	
		if (SpawnActorForEntry(NewEntry))
		{
			OwnerComponent->BroadcastChanged();
		}

		MarkItemDirty(NewEntry);
	}

	return Result;
}

// 仅权威端按 PartHandle 查找条目，销毁本地实例、移除数组并标记 FastArray 变更。
void FLyraCharacterPartList::RemoveEntry(FLyraCharacterPartHandle Handle)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FLyraAppliedCharacterPartEntry& Entry = *EntryIt;
		if (Entry.PartHandle == Handle.PartHandle)
		{
			const bool bDestroyedActor = DestroyActorForEntry(Entry);
			EntryIt.RemoveCurrent();
			MarkArrayDirty();

			if (bDestroyedActor && ensure(OwnerComponent))
			{
				OwnerComponent->BroadcastChanged();
			}

			break;
		}
	}
}

// 销毁所有已生成部件并清空 FastArray；按参数决定是否广播一次汇总变化。
void FLyraCharacterPartList::ClearAllEntries(bool bBroadcastChangeDelegate)
{
	bool bDestroyedAnyActors = false;
	for (FLyraAppliedCharacterPartEntry& Entry : Entries)
	{
		bDestroyedAnyActors |= DestroyActorForEntry(Entry);
	}
	Entries.Reset();
	MarkArrayDirty();

	if (bDestroyedAnyActors && bBroadcastChangeDelegate && ensure(OwnerComponent))
	{
		OwnerComponent->BroadcastChanged();
	}
}

// 遍历已生成部件 Actor 的 GameplayTagAssetInterface，合并其全部外观标签。
FGameplayTagContainer FLyraCharacterPartList::CollectCombinedTags() const
{
	FGameplayTagContainer Result;

	for (const FLyraAppliedCharacterPartEntry& Entry : Entries)
	{
		if (Entry.SpawnedComponent != nullptr)
		{
			if (IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Entry.SpawnedComponent->GetChildActor()))
			{
				TagInterface->GetOwnedGameplayTags(/*inout*/ Result);
			}
		}
	}

	return Result;
}

// 在 OwnerComponent 选定的附着组件上创建 ChildActorComponent，应用 Socket、碰撞策略和 Tick 依赖；缺少类或附着目标时返回 false。
bool FLyraCharacterPartList::SpawnActorForEntry(FLyraAppliedCharacterPartEntry& Entry)
{
	bool bCreatedAnyActors = false;

	if (ensure(OwnerComponent) && !OwnerComponent->IsNetMode(NM_DedicatedServer))
	{
		if (Entry.Part.PartClass != nullptr)
		{
			UWorld* World = OwnerComponent->GetWorld();

			if (USceneComponent* ComponentToAttachTo = OwnerComponent->GetSceneComponentToAttachTo())
			{
				const FTransform SpawnTransform = ComponentToAttachTo->GetSocketTransform(Entry.Part.SocketName);

				UChildActorComponent* PartComponent = NewObject<UChildActorComponent>(OwnerComponent->GetOwner());

				PartComponent->SetupAttachment(ComponentToAttachTo, Entry.Part.SocketName);
				PartComponent->SetChildActorClass(Entry.Part.PartClass);
				PartComponent->RegisterComponent();

				if (AActor* SpawnedActor = PartComponent->GetChildActor())
				{
					switch (Entry.Part.CollisionMode)
					{
					case ECharacterCustomizationCollisionMode::UseCollisionFromCharacterPart:
						// 保留部件类自身的碰撞设置。
						// Do nothing
						break;

					case ECharacterCustomizationCollisionMode::NoCollision:
						SpawnedActor->SetActorEnableCollision(false);
						break;
					}

					// 显式建立 Tick 前置关系，弥补 ChildActorComponent 未自动提供依赖的问题。
					// Set up a direct tick dependency to work around the child actor component not providing one
					if (USceneComponent* SpawnedRootComponent = SpawnedActor->GetRootComponent())
					{
						SpawnedRootComponent->AddTickPrerequisiteComponent(ComponentToAttachTo);
					}
				}

				Entry.SpawnedComponent = PartComponent;
				bCreatedAnyActors = true;
			}
		}
	}

	return bCreatedAnyActors;
}

// 注销并销毁条目持有的 ChildActorComponent；没有已生成组件时返回 false。
bool FLyraCharacterPartList::DestroyActorForEntry(FLyraAppliedCharacterPartEntry& Entry)
{
	bool bDestroyedAnyActors = false;

	if (Entry.SpawnedComponent != nullptr)
	{
		Entry.SpawnedComponent->DestroyComponent();
		Entry.SpawnedComponent = nullptr;
		bDestroyedAnyActors = true;
	}

	return bDestroyedAnyActors;
}

//////////////////////////////////////////////////////////////////////

// 构造可复制且默认关闭 Tick 的 Pawn 外观部件组件，并把 FastArray OwnerComponent 指向自身。
ULyraPawnComponent_CharacterParts::ULyraPawnComponent_CharacterParts(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

// 登记 CharacterPartList 的网络复制，使所有客户端收到权威端装配条目。
void ULyraPawnComponent_CharacterParts::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, CharacterPartList);
}

// 把部件请求交给 FastArray 添加并返回服务器句柄。
FLyraCharacterPartHandle ULyraPawnComponent_CharacterParts::AddCharacterPart(const FLyraCharacterPart& NewPart)
{
	return CharacterPartList.AddEntry(NewPart);
}

// 按服务器句柄从 FastArray 移除一个角色部件。
void ULyraPawnComponent_CharacterParts::RemoveCharacterPart(FLyraCharacterPartHandle Handle)
{
	CharacterPartList.RemoveEntry(Handle);
}

// 清空全部角色部件，并广播一次最终变化。
void ULyraPawnComponent_CharacterParts::RemoveAllCharacterParts()
{
	CharacterPartList.ClearAllEntries(/*bBroadcastChangeDelegate=*/ true);
}

// 开始运行时保持 FastArray 与组件关联，并等待复制或权威端添加部件。
void ULyraPawnComponent_CharacterParts::BeginPlay()
{
	Super::BeginPlay();
}

// 组件结束时销毁全部 ChildActor，但不再广播外观变化，然后调用父类清理。
void ULyraPawnComponent_CharacterParts::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CharacterPartList.ClearAllEntries(/*bBroadcastChangeDelegate=*/ false);

	Super::EndPlay(EndPlayReason);
}

// 注册时确认 Owner 是 Pawn，并重新设置 FastArray OwnerComponent。
void ULyraPawnComponent_CharacterParts::OnRegister()
{
	Super::OnRegister();
	
	if (!IsTemplate())
	{
		CharacterPartList.SetOwnerComponent(this);
	}
}

// 收集所有有效 SpawnedComponent 的 ChildActor；尚未生成或已销毁的条目被跳过。
TArray<AActor*> ULyraPawnComponent_CharacterParts::GetCharacterPartActors() const
{
	TArray<AActor*> Result;
	Result.Reserve(CharacterPartList.Entries.Num());

	for (const FLyraAppliedCharacterPartEntry& Entry : CharacterPartList.Entries)
	{
		if (UChildActorComponent* PartComponent = Entry.SpawnedComponent)
		{
			if (AActor* SpawnedActor = PartComponent->GetChildActor())
			{
				Result.Add(SpawnedActor);
			}
		}
	}

	return Result;
}

// Owner 为 ACharacter 时返回其 MeshComponent，否则返回 nullptr。
USkeletalMeshComponent* ULyraPawnComponent_CharacterParts::GetParentMeshComponent() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (ACharacter* OwningCharacter = Cast<ACharacter>(OwnerActor))
		{
			if (USkeletalMeshComponent* MeshComponent = OwningCharacter->GetMesh())
			{
				return MeshComponent;
			}
		}
	}

	return nullptr;
}

// Character 优先附着到 MeshComponent，其他 Pawn 附着到 RootComponent；Owner 无有效场景组件时返回 nullptr。
USceneComponent* ULyraPawnComponent_CharacterParts::GetSceneComponentToAttachTo() const
{
	if (USkeletalMeshComponent* MeshComponent = GetParentMeshComponent())
	{
		return MeshComponent;
	}
	else if (AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetRootComponent();
	}
	else
	{
		return nullptr;
	}
}

// 取得全部部件合并标签；RequiredPrefix 有效时只返回其匹配子标签。
FGameplayTagContainer ULyraPawnComponent_CharacterParts::GetCombinedTags(FGameplayTag RequiredPrefix) const
{
	FGameplayTagContainer Result = CharacterPartList.CollectCombinedTags();
	if (RequiredPrefix.IsValid())
	{
		return Result.Filter(FGameplayTagContainer(RequiredPrefix));
	}
	else
	{
		return Result;
	}
}

// 按合并外观标签重新选择主体 SkeletalMesh 和强制 PhysicsAsset，然后广播 OnCharacterPartsChanged。
void ULyraPawnComponent_CharacterParts::BroadcastChanged()
{
	const bool bReinitPose = true;

	// 部件变化后重新评估角色主体网格是否需要切换。
	// Check to see if the body type has changed
	if (USkeletalMeshComponent* MeshComponent = GetParentMeshComponent())
	{
		// 合并部件标签，并按 BodyMeshes 规则选择目标 SkeletalMesh。
		// Determine the mesh to use based on cosmetic part tags
		const FGameplayTagContainer MergedTags = GetCombinedTags(FGameplayTag());
		USkeletalMesh* DesiredMesh = BodyMeshes.SelectBestBodyStyle(MergedTags);

		// 应用目标网格；未变化时 SetSkeletalMesh 不执行实质操作。
		// Apply the desired mesh (this call is a no-op if the mesh hasn't changed)
		MeshComponent->SetSkeletalMesh(DesiredMesh, /*bReinitPose=*/ bReinitPose);

		// 若配置了强制 PhysicsAsset，则覆盖网格自带物理资产。
		// Apply the desired physics asset if there's a forced override independent of the one from the mesh
		if (UPhysicsAsset* PhysicsAsset = BodyMeshes.ForcedPhysicsAsset)
		{
			MeshComponent->SetPhysicsAsset(PhysicsAsset, /*bForceReInit=*/ bReinitPose);
		}
	}

	// 通知观察者重新应用队伍着色、动画层等依赖外观部件的表现。
	// Let observers know, e.g., if they need to apply team coloring or similar
	OnCharacterPartsChanged.Broadcast(this);
}


