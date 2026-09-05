// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/PawnComponent.h"
#include "Cosmetics/LyraCosmeticAnimationTypes.h"
#include "LyraCharacterPartTypes.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "LyraPawnComponent_CharacterParts.generated.h"

class ULyraPawnComponent_CharacterParts;
namespace EEndPlayReason { enum Type : int; }
struct FGameplayTag;
struct FLyraCharacterPartList;

class AActor;
class UChildActorComponent;
class UObject;
class USceneComponent;
class USkeletalMeshComponent;
struct FFrame;
struct FNetDeltaSerializeInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLyraSpawnedCharacterPartsChanged, ULyraPawnComponent_CharacterParts*, ComponentWithChangedParts);

//////////////////////////////////////////////////////////////////////

// FastArray 中一个已应用的角色部件条目。
// A single applied character part
USTRUCT()
struct FLyraAppliedCharacterPartEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FLyraAppliedCharacterPartEntry()
	{}

	FString GetDebugString() const;

private:
	friend FLyraCharacterPartList;
	friend ULyraPawnComponent_CharacterParts;

private:
	// 该复制条目描述的角色部件请求。
	// The character part being represented
	UPROPERTY()
	FLyraCharacterPart Part;

	// 添加时返回给调用方的句柄索引，仅服务器使用且不复制。
	// Handle index we returned to the user (server only)
	UPROPERTY(NotReplicated)
	int32 PartHandle = INDEX_NONE;

	// 本机为该条目创建的 ChildActorComponent，不参与复制。
	// The spawned actor instance (client only)
	UPROPERTY(NotReplicated)
	TObjectPtr<UChildActorComponent> SpawnedComponent = nullptr;
};

//////////////////////////////////////////////////////////////////////

// 使用 FastArray 增量复制的已应用角色部件列表，并在客户端增删实际 ChildActor。
// Replicated list of applied character parts
USTRUCT(BlueprintType)
struct FLyraCharacterPartList : public FFastArraySerializer
{
	GENERATED_BODY()

	FLyraCharacterPartList()
		: OwnerComponent(nullptr)
	{
	}

public:
	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FLyraAppliedCharacterPartEntry, FLyraCharacterPartList>(Entries, DeltaParms, *this);
	}

	FLyraCharacterPartHandle AddEntry(FLyraCharacterPart NewPart);
	void RemoveEntry(FLyraCharacterPartHandle Handle);
	void ClearAllEntries(bool bBroadcastChangeDelegate);

	FGameplayTagContainer CollectCombinedTags() const;

	void SetOwnerComponent(ULyraPawnComponent_CharacterParts* InOwnerComponent)
	{
		OwnerComponent = InOwnerComponent;
	}
	
private:
	friend ULyraPawnComponent_CharacterParts;

	bool SpawnActorForEntry(FLyraAppliedCharacterPartEntry& Entry);
	bool DestroyActorForEntry(FLyraAppliedCharacterPartEntry& Entry);

private:
	// 通过 FastArray 复制的角色部件条目。
	// Replicated list of equipment entries
	UPROPERTY()
	TArray<FLyraAppliedCharacterPartEntry> Entries;

	// 拥有该列表的 PawnComponent，用于生成部件和广播变化。
	// The component that contains this list
	UPROPERTY(NotReplicated)
	TObjectPtr<ULyraPawnComponent_CharacterParts> OwnerComponent;

	// 服务器递增句柄计数器，确保每次添加返回唯一句柄。
	// Upcounter for handles
	int32 PartHandleCounter = 0;
};

template<>
struct TStructOpsTypeTraits<FLyraCharacterPartList> : public TStructOpsTypeTraitsBase2<FLyraCharacterPartList>
{
	enum { WithNetDeltaSerializer = true };
};

//////////////////////////////////////////////////////////////////////

// 在服务器复制部件清单，并在所有客户端为所属 Pawn 生成和附着外观 Actor。
// A component that handles spawning cosmetic actors attached to the owner pawn on all clients
UCLASS(meta=(BlueprintSpawnableComponent))
class ULyraPawnComponent_CharacterParts : public UPawnComponent
{
	GENERATED_BODY()

public:
	ULyraPawnComponent_CharacterParts(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnRegister() override;
	//~End of UActorComponent interface

	// 权威端添加复制条目并返回可用于移除的唯一句柄。
	// Adds a character part to the actor that owns this customization component, should be called on the authority only
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Cosmetics)
	FLyraCharacterPartHandle AddCharacterPart(const FLyraCharacterPart& NewPart);

	// 权威端按句柄移除条目，使各客户端销毁对应外观 Actor。
	// Removes a previously added character part from the actor that owns this customization component, should be called on the authority only
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Cosmetics)
	void RemoveCharacterPart(FLyraCharacterPartHandle Handle);

	// 权威端清空全部复制条目和已生成外观 Actor。
	// Removes all added character parts, should be called on the authority only
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Cosmetics)
	void RemoveAllCharacterParts();

	// 返回本机由该组件生成的所有有效角色部件 Actor。
	// Gets the list of all spawned character parts from this component
	UFUNCTION(BlueprintCallable, BlueprintPure=false, BlueprintCosmetic, Category=Cosmetics)
	TArray<AActor*> GetCharacterPartActors() const;

	// 所属 Pawn 为 ACharacter 时返回其 MeshComponent，否则返回 nullptr。
	// If the parent actor is derived from ACharacter, returns the Mesh component, otherwise nullptr
	USkeletalMeshComponent* GetParentMeshComponent() const;

	// 返回部件附着目标：Character 使用 MeshComponent，其他 Pawn 使用 RootComponent。
	// Returns the scene component to attach the spawned actors to
	// If the parent actor is derived from ACharacter, we'll use the Mesh component, otherwise the root component
	USceneComponent* GetSceneComponentToAttachTo() const;

	// 合并所有已生成部件提供的 GameplayTag，并可按 RequiredPrefix 过滤。
	// Returns the set of combined gameplay tags from attached character parts, optionally filtered to only tags that start with the specified root
	UFUNCTION(BlueprintCallable, BlueprintPure=false, BlueprintCosmetic, Category=Cosmetics)
	FGameplayTagContainer GetCombinedTags(FGameplayTag RequiredPrefix) const;

	void BroadcastChanged();

public:
	// 已生成部件列表或由标签选择出的主体网格发生变化后广播。
	// Delegate that will be called when the list of spawned character parts has changed
	UPROPERTY(BlueprintAssignable, Category=Cosmetics, BlueprintCallable)
	FLyraSpawnedCharacterPartsChanged OnCharacterPartsChanged;

private:
	// 服务器维护并增量复制到客户端的角色部件列表。
	// List of character parts
	UPROPERTY(Replicated, Transient)
	FLyraCharacterPartList CharacterPartList;

	// 根据全部角色部件外观标签选择动画主体 SkeletalMesh 的规则。
	// Rules for how to pick a body style mesh for animation to play on, based on character part cosmetics tags
	UPROPERTY(EditAnywhere, Category=Cosmetics)
	FLyraAnimBodyStyleSelectionSet BodyMeshes;
};
