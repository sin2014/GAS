// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraEquipmentInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "LyraEquipmentDefinition.h"
#include "Net/UnrealNetwork.h"

#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraEquipmentInstance)

class FLifetimeProperty;
class UClass;
class USceneComponent;

// 构造可复制的运行时装备实例，Instigator 与附属 Actor 初始为空。
ULyraEquipmentInstance::ULyraEquipmentInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 通过 Outer Actor 返回装备实例所属 World；CDO 或无效 Outer 返回 nullptr。
UWorld* ULyraEquipmentInstance::GetWorld() const
{
	if (APawn* OwningPawn = GetPawn())
	{
		return OwningPawn->GetWorld();
	}
	else
	{
		return nullptr;
	}
}

// 注册 Instigator 与 SpawnedActors 的传统网络复制属性。
void ULyraEquipmentInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, Instigator);
	DOREPLIFETIME(ThisClass, SpawnedActors);
}

// 为装备 UObject 创建并注册 Iris 属性复制 Fragment。
void ULyraEquipmentInstance::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	using namespace UE::Net;

	// 根据该装备实例的复制属性构建描述符，并创建、注册对应的 PropertyReplicationFragment。
	// Build descriptors and allocate PropertyReplicationFragments for this object
	FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags);
}

// 将装备实例的 Outer Actor 转换为所应用的 Pawn。
APawn* ULyraEquipmentInstance::GetPawn() const
{
	return Cast<APawn>(GetOuter());
}

// 仅当所属 Pawn 是请求类型或其派生类时返回该 Pawn。
APawn* ULyraEquipmentInstance::GetTypedPawn(TSubclassOf<APawn> PawnType) const
{
	APawn* Result = nullptr;
	if (UClass* ActualPawnType = PawnType)
	{
		if (GetOuter()->IsA(ActualPawnType))
		{
			Result = Cast<APawn>(GetOuter());
		}
	}
	return Result;
}

// 仅在权威 Pawn 上生成配置 Actor，附着到指定 Socket，并记录用于复制和卸装销毁。
void ULyraEquipmentInstance::SpawnEquipmentActors(const TArray<FLyraEquipmentActorToSpawn>& ActorsToSpawn)
{
	if (APawn* OwningPawn = GetPawn())
	{
		USceneComponent* AttachTarget = OwningPawn->GetRootComponent();
		if (ACharacter* Char = Cast<ACharacter>(OwningPawn))
		{
			AttachTarget = Char->GetMesh();
		}

		for (const FLyraEquipmentActorToSpawn& SpawnInfo : ActorsToSpawn)
		{
			AActor* NewActor = GetWorld()->SpawnActorDeferred<AActor>(SpawnInfo.ActorToSpawn, FTransform::Identity, OwningPawn);
			NewActor->FinishSpawning(FTransform::Identity, /*bIsDefaultTransform=*/ true);
			NewActor->SetActorRelativeTransform(SpawnInfo.AttachTransform);
			NewActor->AttachToComponent(AttachTarget, FAttachmentTransformRules::KeepRelativeTransform, SpawnInfo.AttachSocket);

			SpawnedActors.Add(NewActor);
		}
	}
}

// 销毁并清空该装备实例生成的全部附属 Actor。
void ULyraEquipmentInstance::DestroyEquipmentActors()
{
	for (AActor* Actor : SpawnedActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
}

// 装备生效时触发蓝图 OnEquipped 回调。
void ULyraEquipmentInstance::OnEquipped()
{
	K2_OnEquipped();
}

// 装备卸下时触发蓝图 OnUnequipped 回调。
void ULyraEquipmentInstance::OnUnequipped()
{
	K2_OnUnequipped();
}

// Instigator 复制更新后的扩展回调，当前不执行额外逻辑。
void ULyraEquipmentInstance::OnRep_Instigator()
{
}

