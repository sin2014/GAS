// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPlayerSpawningManagerComponent.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"
#include "Engine/PlayerStartPIE.h"
#include "LyraPlayerStart.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPlayerSpawningManagerComponent)

// 玩家出生点选择与重生管理使用的文件级日志分类。
DEFINE_LOG_CATEGORY_STATIC(LogPlayerSpawning, Log, All);

// 配置不复制的 GameStateComponent，启用初始化和 PrePhysics Tick，但默认关闭 Tick。
ULyraPlayerSpawningManagerComponent::ULyraPlayerSpawningManagerComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(false);
	bAutoRegister = true;
	bAutoActivate = true;
	bWantsInitializeComponent = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// 初始化 ULyraPlayerSpawningManagerComponent，注册其运行期回调并准备所需状态。
void ULyraPlayerSpawningManagerComponent::InitializeComponent()
{
	Super::InitializeComponent();

	FWorldDelegates::LevelAddedToWorld.AddUObject(this, &ThisClass::OnLevelAdded);

	UWorld* World = GetWorld();
	World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleOnActorSpawned));

	for (TActorIterator<ALyraPlayerStart> It(World); It; ++It)
	{
		if (ALyraPlayerStart* PlayerStart = *It)
		{
			CachedPlayerStarts.Add(PlayerStart);
		}
	}
}

// 新关卡加入当前 World 时，把其中所有 LyraPlayerStart 加入弱引用缓存。
void ULyraPlayerSpawningManagerComponent::OnLevelAdded(ULevel* InLevel, UWorld* InWorld)
{
	if (InWorld == GetWorld())
	{
		for (AActor* Actor : InLevel->Actors)
		{
			if (ALyraPlayerStart* PlayerStart = Cast<ALyraPlayerStart>(Actor))
			{
				ensure(!CachedPlayerStarts.Contains(PlayerStart));
				CachedPlayerStarts.Add(PlayerStart);
			}
		}
	}
}

// 当前 World 运行时生成 LyraPlayerStart 时，将其弱引用加入出生点缓存。
void ULyraPlayerSpawningManagerComponent::HandleOnActorSpawned(AActor* SpawnedActor)
{
	if (ALyraPlayerStart* PlayerStart = Cast<ALyraPlayerStart>(SpawnedActor))
	{
		CachedPlayerStarts.Add(PlayerStart);
	}
}

// 以下调用由 ALyraGameMode 代理到本组件，也必须覆盖引擎常规 RestartPlayer 流程。
// ALyraGameMode Proxied Calls - Need to handle when someone chooses
// to restart a player the normal way in the engine.
//======================================================================

// 优先使用 PIE PlayFromHere；观战者随机但不认领，普通玩家走自定义选择并认领 LyraPlayerStart。
AActor* ULyraPlayerSpawningManagerComponent::ChoosePlayerStart(AController* Player)
{
	if (Player)
	{
#if WITH_EDITOR
		if (APlayerStart* PlayerStart = FindPlayFromHereStart(Player))
		{
			return PlayerStart;
		}
#endif

		TArray<ALyraPlayerStart*> StarterPoints;
		for (auto StartIt = CachedPlayerStarts.CreateIterator(); StartIt; ++StartIt)
		{
			if (ALyraPlayerStart* Start = (*StartIt).Get())
			{
				StarterPoints.Add(Start);
			}
			else
			{
				StartIt.RemoveCurrent();
			}
		}

		if (APlayerState* PlayerState = Player->GetPlayerState<APlayerState>())
		{
			// 纯观战者可随机使用任意出生位置，但不会认领该 PlayerStart。
			// start dedicated spectators at any random starting location, but they do not claim it
			if (PlayerState->IsOnlyASpectator())
			{
				if (!StarterPoints.IsEmpty())
				{
					return StarterPoints[FMath::RandRange(0, StarterPoints.Num() - 1)];
				}

				return nullptr;
			}
		}

		AActor* PlayerStart = OnChoosePlayerStart(Player, StarterPoints);

		if (!PlayerStart)
		{
			PlayerStart = GetFirstRandomUnoccupiedPlayerStart(Player, StarterPoints);
		}

		if (ALyraPlayerStart* LyraStart = Cast<ALyraPlayerStart>(PlayerStart))
		{
			LyraStart->TryClaim(Player);
		}

		return PlayerStart;
	}

	return nullptr;
}

#if WITH_EDITOR
// 查找 PlayFromHereStart，未找到符合条件的对象时返回空值。
APlayerStart* ULyraPlayerSpawningManagerComponent::FindPlayFromHereStart(AController* Player)
{
	// “Play From Here”仅适用于玩家 Controller；Bot 等仍从正常出生点生成。
	// Only 'Play From Here' for a player controller, bots etc. should all spawn from normal spawn points.
	if (Player->IsA<APlayerController>())
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<APlayerStart> It(World); It; ++It)
			{
				if (APlayerStart* PlayerStart = *It)
				{
					if (PlayerStart->IsA<APlayerStartPIE>())
					{
						// PIE 中发现 “Play From Here” 出生点时，始终优先使用第一个。
						// Always prefer the first "Play from Here" PlayerStart, if we find one while in PIE mode
						return PlayerStart;
					}
				}
			}
		}
	}

	return nullptr;
}
#endif

// 返回当前默认允许 Controller 重生的结果，预留玩法规则扩展点。
bool ULyraPlayerSpawningManagerComponent::ControllerCanRestart(AController* Player)
{
	bool bCanRestart = true;

	// TODO：根据玩法规则判断该 Controller 当前是否允许重生。
	// TODO Can they restart?

	return bCanRestart;
}

// 重生完成后依次调用 C++ 扩展点和蓝图事件，传递 Controller 与出生旋转。
void ULyraPlayerSpawningManagerComponent::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	OnFinishRestartPlayer(NewPlayer, StartRotation);
	K2_OnFinishRestartPlayer(NewPlayer, StartRotation);
}

//================================================================

// 当前仅调用父类 Tick，保留给 Experience 定制重生管理的扩展点。
void ULyraPlayerSpawningManagerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// 随机选择完全空闲出生点；没有时退而选择可通过传送调整容纳 Pawn 的部分占用点。
APlayerStart* ULyraPlayerSpawningManagerComponent::GetFirstRandomUnoccupiedPlayerStart(AController* Controller, const TArray<ALyraPlayerStart*>& StartPoints) const
{
	if (Controller)
	{
		TArray<ALyraPlayerStart*> UnOccupiedStartPoints;
		TArray<ALyraPlayerStart*> OccupiedStartPoints;

		for (ALyraPlayerStart* StartPoint : StartPoints)
		{
			ELyraPlayerStartLocationOccupancy State = StartPoint->GetLocationOccupancy(Controller);

			switch (State)
			{
				case ELyraPlayerStartLocationOccupancy::Empty:
					UnOccupiedStartPoints.Add(StartPoint);
					break;
				case ELyraPlayerStartLocationOccupancy::Partial:
					OccupiedStartPoints.Add(StartPoint);
					break;

			}
		}

		if (UnOccupiedStartPoints.Num() > 0)
		{
			return UnOccupiedStartPoints[FMath::RandRange(0, UnOccupiedStartPoints.Num() - 1)];
		}
		else if (OccupiedStartPoints.Num() > 0)
		{
			return OccupiedStartPoints[FMath::RandRange(0, OccupiedStartPoints.Num() - 1)];
		}
	}

	return nullptr;
}

