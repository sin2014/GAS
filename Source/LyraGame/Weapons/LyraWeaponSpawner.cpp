// Copyright Epic Games, Inc. All Rights Reserved.


#include "LyraWeaponSpawner.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Equipment/LyraPickupDefinition.h"
#include "GameFramework/Pawn.h"
#include "Inventory/InventoryFragment_SetStats.h"
#include "Kismet/GameplayStatics.h"
#include "LyraLogChannels.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWeaponSpawner)

class FLifetimeProperty;
class UNiagaraSystem;
class USoundBase;
struct FHitResult;

// 创建武器刷新器的默认组件、碰撞范围、冷却参数和网络复制状态。
// Sets default values
ALyraWeaponSpawner::ALyraWeaponSpawner()
{
	// 启用每帧 Tick，用于旋转展示网格和更新重生进度；不需要这些表现时可关闭以节省性能。
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CollisionVolume = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CollisionVolume"));
	CollisionVolume->InitCapsuleSize(80.f, 80.f);
	CollisionVolume->OnComponentBeginOverlap.AddDynamic(this, &ALyraWeaponSpawner::OnOverlapBegin);

	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(RootComponent);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);

	WeaponMeshRotationSpeed = 40.0f;
	CoolDownTime = 30.0f;
	CheckExistingOverlapDelay = 0.25f;
	bIsWeaponAvailable = true;
	bReplicates = true;
}

// 开始游戏时从 WeaponDefinition 读取实际刷新冷却时间，并报告缺失配置。
// Called when the game starts or when spawned
void ALyraWeaponSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponDefinition && WeaponDefinition->InventoryItemDefinition)
	{
		CoolDownTime = WeaponDefinition->SpawnCoolDownSeconds;	
	}
	else if (const UWorld* World = GetWorld())
	{
		if (!World->IsPlayingReplay())
		{
			UE_LOG(LogLyra, Error, TEXT("'%s' does not have a valid weapon definition! Make sure to set this data on the instance!"), *GetNameSafe(this));	
		}
	}
}

// 结束游戏时清除重生和重叠延迟计时器，再调用基类清理。
void ALyraWeaponSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CoolDownTimerHandle);
		World->GetTimerManager().ClearTimer(CheckOverlapsDelayTimerHandle);
	}
	
	Super::EndPlay(EndPlayReason);
}

// 每帧更新重生进度并旋转展示中的武器网格。
// Called every frame
void ALyraWeaponSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 根据计时器剩余时间更新 0 到 1 的 CoolDownPercentage，驱动重生指示器。
	//Update the CoolDownPercentage property to drive respawn time indicators
	UWorld* World = GetWorld();
	if (World->GetTimerManager().IsTimerActive(CoolDownTimerHandle))
	{
		CoolDownPercentage = 1.0f - World->GetTimerManager().GetTimerRemaining(CoolDownTimerHandle)/CoolDownTime;
	}

	WeaponMesh->AddRelativeRotation(FRotator(0.0f, World->GetDeltaSeconds() * WeaponMeshRotationSpeed, 0.0f));
}

// 编辑器构造或属性变化时把武器定义的网格、偏移和缩放同步到展示组件。
void ALyraWeaponSpawner::OnConstruction(const FTransform& Transform)
{
	if (WeaponDefinition != nullptr && WeaponDefinition->DisplayMesh != nullptr)
	{
		WeaponMesh->SetStaticMesh(WeaponDefinition->DisplayMesh);
		WeaponMesh->SetRelativeLocation(WeaponDefinition->WeaponMeshOffset);
		WeaponMesh->SetRelativeScale3D(WeaponDefinition->WeaponMeshScale);
	}	
}

// 服务器上可用武器刷新点与 Pawn 开始重叠时尝试发放武器。
void ALyraWeaponSpawner::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepHitResult)
{
	APawn* OverlappingPawn = Cast<APawn>(OtherActor);
	if (GetLocalRole() == ROLE_Authority && bIsWeaponAvailable && OverlappingPawn != nullptr)
	{
		AttemptPickUpWeapon(OverlappingPawn);
	}
}

// 重生后枚举仍位于碰撞范围内的 Pawn，并逐个重新尝试拾取。
void ALyraWeaponSpawner::CheckForExistingOverlaps()
{
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors, APawn::StaticClass());

	for (AActor* OverlappingActor : OverlappingActors)
	{
		AttemptPickUpWeapon(Cast<APawn>(OverlappingActor));
	}
}

// 服务器验证可用状态和 Ability System，成功发放物品后隐藏刷新点、播放效果并启动冷却。
void ALyraWeaponSpawner::AttemptPickUpWeapon_Implementation(APawn* Pawn)
{
	if (GetLocalRole() == ROLE_Authority && bIsWeaponAvailable && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn))
	{
		TSubclassOf<ULyraInventoryItemDefinition> WeaponItemDefinition = WeaponDefinition ? WeaponDefinition->InventoryItemDefinition : nullptr;
		if (WeaponItemDefinition != nullptr)
		{
			// 通过可覆写的 GiveWeapon 向 Pawn 发放物品；只有成功后才消耗刷新点。
			//Attempt to grant the weapon
			if (GiveWeapon(WeaponItemDefinition, Pawn))
			{
				// 拾取成功后在服务器标记不可用、隐藏展示、播放拾取表现并启动重生冷却。
				//Weapon picked up by pawn
				bIsWeaponAvailable = false;
				SetWeaponPickupVisibility(false);
				PlayPickupEffects();
				StartCoolDown();
			}
		}		
	}
}

// 按配置冷却时间启动一次性重生计时器。
void ALyraWeaponSpawner::StartCoolDown()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(CoolDownTimerHandle, this, &ALyraWeaponSpawner::OnCoolDownTimerComplete, CoolDownTime);
	}
}

// 清除冷却；服务器恢复可用状态、播放重生表现并延迟检查已有重叠，最后重置进度。
void ALyraWeaponSpawner::ResetCoolDown()
{
	UWorld* World = GetWorld();

	if (World)
	{
		World->GetTimerManager().ClearTimer(CoolDownTimerHandle);
	}

	if (GetLocalRole() == ROLE_Authority)
	{
		bIsWeaponAvailable = true;
		PlayRespawnEffects();
		SetWeaponPickupVisibility(true);

		if (World)
		{
			World->GetTimerManager().SetTimer(CheckOverlapsDelayTimerHandle, this, &ALyraWeaponSpawner::CheckForExistingOverlaps, CheckExistingOverlapDelay);
		}
	}

	CoolDownPercentage = 0.0f;
}

// 冷却计时完成后执行刷新点重置流程。
void ALyraWeaponSpawner::OnCoolDownTimerComplete()
{
	ResetCoolDown();
}

// 递归设置展示武器网格及其子项可见性。
void ALyraWeaponSpawner::SetWeaponPickupVisibility(bool bShouldBeVisible)
{
	WeaponMesh->SetVisibility(bShouldBeVisible, true);
}

// 按武器定义在刷新点播放拾取音效，并在武器网格位置生成 Niagara 效果。
void ALyraWeaponSpawner::PlayPickupEffects_Implementation()
{
	if (WeaponDefinition != nullptr)
	{
		USoundBase* PickupSound = WeaponDefinition->PickedUpSound;
		if (PickupSound != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
		}

		UNiagaraSystem* PickupEffect = WeaponDefinition->PickedUpEffect;
		if (PickupEffect != nullptr)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupEffect, WeaponMesh->GetComponentLocation());
		}
	}
}

// 按武器定义在刷新点播放重生音效，并在武器网格位置生成 Niagara 效果。
void ALyraWeaponSpawner::PlayRespawnEffects_Implementation()
{
	if (WeaponDefinition != nullptr)
	{
		USoundBase* RespawnSound = WeaponDefinition->RespawnedSound;
		if (RespawnSound != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(this, RespawnSound, GetActorLocation());
		}

		UNiagaraSystem* RespawnEffect = WeaponDefinition->RespawnedEffect;
		if (RespawnEffect != nullptr)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, RespawnEffect, WeaponMesh->GetComponentLocation());
		}
	}
}

// 客户端收到可用状态变化后同步网格、冷却计时和拾取或重生表现。
void ALyraWeaponSpawner::OnRep_WeaponAvailability()
{
	if (bIsWeaponAvailable)
	{
		PlayRespawnEffects();
		SetWeaponPickupVisibility(true);
	}
	else
	{
		SetWeaponPickupVisibility(false);
		StartCoolDown();
		PlayPickupEffects();
	}	
}

// 把 bIsWeaponAvailable 注册为网络复制属性。
void ALyraWeaponSpawner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALyraWeaponSpawner, bIsWeaponAvailable);
}

// 从物品定义 CDO 的 SetStats Fragment 读取指定标签默认值；缺失任一环节时返回 0。
int32 ALyraWeaponSpawner::GetDefaultStatFromItemDef(const TSubclassOf<ULyraInventoryItemDefinition> WeaponItemClass, FGameplayTag StatTag)
{
	if (WeaponItemClass != nullptr)
	{
		if (ULyraInventoryItemDefinition* WeaponItemCDO = WeaponItemClass->GetDefaultObject<ULyraInventoryItemDefinition>())
		{
			if (const UInventoryFragment_SetStats* ItemStatsFragment = Cast<UInventoryFragment_SetStats>( WeaponItemCDO->FindFragmentByClass(UInventoryFragment_SetStats::StaticClass()) ))
			{
				return ItemStatsFragment->GetItemStatByTag(StatTag);
			}
		}
	}

	return 0;
}
