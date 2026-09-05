// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "LyraCharacterPartTypes.generated.h"

class UChildActorComponent;
class ULyraPawnComponent_CharacterParts;
struct FLyraCharacterPartList;

//////////////////////////////////////////////////////////////////////

// 定义生成角色部件 Actor 后如何处理其碰撞设置。
// How should collision be configured on the spawned part actor
UENUM()
enum class ECharacterCustomizationCollisionMode : uint8
{
	// 禁用所有生成角色部件的碰撞。
	// Disable collision on spawned character parts
	NoCollision,

	// 保留角色部件类自身配置的碰撞设置。
	// Leave the collision settings on character parts alone
	UseCollisionFromCharacterPart
};

//////////////////////////////////////////////////////////////////////

// 添加角色部件后返回的服务器句柄，可用于精确移除对应条目。
// A handle created by adding a character part entry, can be used to remove it later
USTRUCT(BlueprintType)
struct FLyraCharacterPartHandle
{
	GENERATED_BODY()

	void Reset()
	{
		PartHandle = INDEX_NONE;
	}

	bool IsValid() const
	{
		return PartHandle != INDEX_NONE;
	}

private:
	UPROPERTY()
	int32 PartHandle = INDEX_NONE;

	friend FLyraCharacterPartList;
};

//////////////////////////////////////////////////////////////////////
// 描述需要生成并附着到 Pawn 的一个角色部件请求。
// A character part request

USTRUCT(BlueprintType)
struct FLyraCharacterPart
{
	GENERATED_BODY()

	// 要实例化为 ChildActor 的部件 Actor 类。
	// The part to spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AActor> PartClass;

	// 部件附着到父组件的 SocketName；为空时直接附着到组件。
	// The socket to attach the part to (if any)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SocketName;

	// 生成后如何处理部件内 PrimitiveComponent 的碰撞。
	// How to handle collision for the primitive components in the part
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ECharacterCustomizationCollisionMode CollisionMode = ECharacterCustomizationCollisionMode::NoCollision;

	// 比较部件类与 SocketName 是否相同，忽略碰撞模式差异。
	// Compares against another part, ignoring the collision mode
	static bool AreEquivalentParts(const FLyraCharacterPart& A, const FLyraCharacterPart& B)
	{
		return (A.PartClass == B.PartClass) && (A.SocketName == B.SocketName);
	}
};
