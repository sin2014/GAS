// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "CQTest.h"

#if WITH_AUTOMATION_TESTS

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/LyraCharacter.h"
#include "Components/MapTestSpawner.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "Helpers/CQTestAssetHelper.h"
#include "ShooterTestsAnimationTestHelper.h"
#include "ShooterTestsInputTestHelper.h"

/**
 * 单人 Actor 自动化测试基类，负责按名称解析地图、等待地图与 Lyra Experience 完成加载、取得玩家 Pawn 和 ASC，并等待 Spawn GameplayCue 结束。
 * 测试通过 TestCommandBuilder 组织逐帧 Latent Command：Then 在单 Tick 执行，StartWhen/Until 会持续轮询直到成功或超时失败。
 */
/**
 * Implementation of our base class used to share functionality of sharing a Pawn and a level for tests.
 * Inherits from `TTest<Derived, AsserterType>` to provide us our testing functionality.
 *
 * Implements functionality to load a level, specified as parameters in the Constructor, and verifies that the Player is completely spawned within the game world during the `Setup`
 *
 * Makes use of the `TestCommandBuilder` to queue up latent commands to be executed on every Tick of the Engine/Editor
 * `Then` steps will execute within a single tick
 * 'StartWhen` and `Until` steps will keep executing each tick until the predicate has evaluated to true or the timeout period has elapsed. The latter will fail the test.
 */
template<typename Derived, typename AsserterType>
struct ShooterTestsActorBaseTest : public TTest<Derived, AsserterType>
{
	/** 将模板父类的测试运行器、断言器和命令构建器引入当前作用域。 */
	/** Let this object know about our templated parent's variables */
	using TTest<Derived, AsserterType>::TestRunner;
	using TTest<Derived, AsserterType>::Assert;
	using TTest<Derived, AsserterType>::TestCommandBuilder;

	/** 按地图名称查找资源包路径并创建 FMapTestSpawner；测试框架初始化阶段不会实际加载资产。 */
	/**
	 * Construct the Base Actor Test.
	 *
	 * @param MapName - Name of the map.
	 */
	ShooterTestsActorBaseTest(const FString& MapName)
	{
		// CQTest 注册/初始化测试对象时不要加载资产，正式执行测试时再解析地图。
		// Don't load assets during initialization
		if (TestRunner->bInitializing)
		{
			return;
		}
		
		TOptional<FString> PackagePath = CQTestAssetHelper::FindAssetPackagePathByName(MapName);
		ASSERT_THAT(IsTrue(PackagePath.IsSet(), "Could not find the map package."));
		Spawner = MakeUnique<FMapTestSpawner>(PackagePath.GetValue(), MapName);
	}

	/** 检查 Spawner 世界是否已有 GameState、LyraExperienceManagerComponent 且 Experience 已加载；用于 Until 轮询。 */
	/**
	 * Check to make sure that the specified world has fully loaded.
	 *
	 * @return true if the world is fully loaded.
	 * 
	 * @note Method is expected to be used within the `Until` latent command to then wait until the world has loaded.
	 */
	bool HasWorldLoaded()
	{
		UWorld& World = Spawner->GetWorld();
		AGameStateBase* GameState = World.GetGameState();
		if (GameState == nullptr)
		{
			return false;
		}

		ULyraExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyraExperienceManagerComponent>();
		if (ExperienceComponent == nullptr)
		{
			return false;
		}

		return ExperienceComponent->IsExperienceLoaded();
	}

	/** 取得首个玩家 LyraCharacter、ASC 和 Spawn GameplayCueTag，并对所有测试前置条件执行断言。 */
	/** Get our Lyra Player Pawn and all associated systems and functionality needed for our Player. */
	virtual void PreparePlayerPawn()
	{
		Player = Cast<ALyraCharacter>(Spawner->FindFirstPlayerPawn());
		ASSERT_THAT(IsNotNull(Player));

		AbilitySystemComponent = Player->GetLyraAbilitySystemComponent();
		ASSERT_THAT(IsNotNull(AbilitySystemComponent));

		GameplayCueCharacterSpawnTag = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Character.Spawn"), false);
		ASSERT_THAT(IsTrue(GameplayCueCharacterSpawnTag.IsValid()));
	}

	/** 当阻塞玩家输入的 Spawn GameplayCue 不再激活时，判定 Pawn 已完全生成。 */
	/**
	 * Functionality used to check if the Player is fully spawned into the game world.
	 * 
	 * @return true if the GameplayEffect 'GE_SpawnIn' is no longer active as this effect blocks player input.
	 */
	virtual bool IsPlayerPawnFullySpawned()
	{
		bool bIsCurrentlySpawning = AbilitySystemComponent->IsGameplayCueActive(GameplayCueCharacterSpawnTag);
		return !bIsCurrentlySpawning;
	}

	/** 加载指定地图，最多等待 30 秒完成 Experience，再依次等待玩家 Pawn、准备组件并确认生成阶段结束。 */
	/** Setup the test by loading in the specified level and making sure that the Lyra player is fully spawned in before continuing. */
	virtual void Setup() override
	{
		ASSERT_THAT(IsNotNull(Spawner));
		Spawner->AddWaitUntilLoadedCommand(TestRunner);

		const FTimespan LoadingScreenTimeout = FTimespan::FromSeconds(30);
		TestCommandBuilder
			.StartWhen([this]() { return HasWorldLoaded(); }, LoadingScreenTimeout)
			.Until([this]() { return nullptr != Spawner->FindFirstPlayerPawn(); })
			.Then([this]() { PreparePlayerPawn(); })
			.Until([this]() { return IsPlayerPawnFullySpawned(); });
	}

	/** 关卡中的本地被测 LyraCharacter。 */
	/** Reference to our player in the level. */
	ALyraCharacter* Player{ nullptr };

	/** 负责加载目标地图和查找玩家 Pawn 的 MapTestSpawner。 */
	/** Object to handle loading of our desired level. */
	TUniquePtr<FMapTestSpawner> Spawner{ nullptr };

	/** 被测玩家的 LyraAbilitySystemComponent。 */
	/** Reference to the player's ability system component. */
	ULyraAbilitySystemComponent* AbilitySystemComponent{ nullptr };

	/** 用于判断生成过程是否结束的 GameplayCue.Character.Spawn 标签。 */
	/** Reference to the player's spawning gameplay effect. */
	FGameplayTag GameplayCueCharacterSpawnTag;
};

/**
 * 在单人 Actor 测试基类上增加 SkeletalMesh 和输入动作准备，用输入触发动画并通过 Until 等待预期动画开始播放。
 * Do 步骤在单 Tick 注入输入，Until 持续查询全部 AnimInstance，超时则测试失败。
 */
/**
 * Implementation of our base class used to share functionality of sharing a Pawn and a level for tests.
 * Inherits from `ShooterTestsActorBaseTest<Derived, AsserterType>` to provide us our testing functionality and to handle our initial World setup.
 *
 * Apart from loading our level and Player from the base `ShooterTestsActorBaseTest` object, will fetch the Player's SkeletalMesh component and sets up the FShooterTestsPawnTestActions object for input handling.
 * The SkeletalMesh component is what handles animations for the Player and is used to query against for active animations
 * FShooterTestsPawnTestActions is a user-defined input handling object which derives from CQTest's `FInputTestActions` in order to specify Input Actions around what is available to our Player.
 *
 * Makes use of the `TestCommandBuilder` to queue up latent commands to be executed on every Tick of the Engine/Editor
 * `Do` steps will execute within a single tick
 * `Until` steps will keep executing each tick until the predicate has evaluated to true or the timeout period has elapsed. The latter will fail the test.
 */
template<typename Derived, typename AsserterType>
struct ShooterTestsActorAnimationTest : public ShooterTestsActorBaseTest<Derived, AsserterType>
{
	/** 引入模板父类的断言器、命令构建器和玩家引用。 */
	/** Let this object know about our templated parent's variables */
	using ShooterTestsActorBaseTest<Derived, AsserterType>::Assert;
	using ShooterTestsActorBaseTest<Derived, AsserterType>::TestCommandBuilder;
	using ShooterTestsActorBaseTest<Derived, AsserterType>::Player;

	/** 使用指定地图名称构造动画测试，并复用基础类的地图/玩家准备流程。 */
	/**
	 * Construct the Actor Animation Test.
	 *
	 * @param MapName - Name of the map.
	 */
	ShooterTestsActorAnimationTest(const FString& MapName) : ShooterTestsActorBaseTest<Derived, AsserterType>(MapName) { }

	/** 在基础玩家准备后创建输入代理，并查找、验证玩家 SkeletalMeshComponent。 */
	/**
	 * Calls the parent method to get our Lyra Player Pawn and all associated systems and functionality needed for our Player before setting up functionality needed for input handling and animations.
	 * 
	 * @see ShooterTestsActorBaseTest<Derived, AsserterType>::PreparePlayerPawn()
	 */
	void PreparePlayerPawn() override
	{
		ShooterTestsActorBaseTest<Derived, AsserterType>::PreparePlayerPawn();
		ASSERT_THAT(IsTrue(IsValid(Player), TEXT("Player Pawn has not been set")));

		PawnActions = MakeUnique<FShooterTestsPawnTestActions>(Player);

		UActorComponent* ActorComponent = Player->GetComponentByClass(USkeletalMeshComponent::StaticClass());
		ASSERT_THAT(IsTrue(IsValid(ActorComponent), TEXT("Cannot find SkeletalMeshComponent from Player")));

		PlayerMesh = Cast<USkeletalMeshComponent>(ActorComponent);
		ASSERT_THAT(IsTrue(IsValid(PlayerMesh), TEXT("Cannot cast component to SkeletalMeshComponent")));
	}

	/** 按名称查找与玩家 Skeleton 兼容的预期动画；找不到时立即断言失败。 */
	/**
	 * Get our expected animation to test against.
	 * @note Will assert if the animation cannot be found within the SkeletalMesh
	 */
	void GetExpectedAnimation(const FString& AnimationName)
	{
		ExpectedAnimation = AnimationTestHelper.FindAnimationAsset(PlayerMesh, AnimationName);
		ASSERT_THAT(IsTrue(IsValid(ExpectedAnimation), FString::Format(TEXT("Cannot find animation '{0}'"), { AnimationName })));
	}

	/** 查找预期动画，执行给定输入动作，并逐帧等待该动画在玩家 Mesh 上开始播放。 */
	/**
	 * Tests to see if the expected animation is playing after performing our InputAction.
	 *
	 * @param AnimationName - Name of the animation asset to test against.
	 * @param InputAction - Function of the Input actions to run which will trigger the animation.
	 */
	void TestInputActionAnimation(const FString& AnimationName, TFunction<void()> InputAction)
	{
		GetExpectedAnimation(AnimationName);
		TestCommandBuilder
			.Do(InputAction)
			.Until([this]() { return AnimationTestHelper.IsAnimationPlaying(PlayerMesh, ExpectedAnimation); });
	}

	/** 提供动画资产查找与播放状态检查的辅助对象。 */
	/** Animation helper object. */
	FShooterTestsAnimationTestHelper AnimationTestHelper;

	/** 被测玩家的 SkeletalMeshComponent。 */
	/** Reference to the player's skeletal mesh component. */
	USkeletalMeshComponent* PlayerMesh{ nullptr };

	/** 当前测试期望播放的动画资产。 */
	/** Reference to our animation asset. */
	UAnimationAsset* ExpectedAnimation{ nullptr };

	/** 向被测 Pawn 注入 Lyra 输入动作的代理对象。 */
	/** Object which performs input actions. */
	TUniquePtr<FShooterTestsPawnTestActions> PawnActions{ nullptr };
};

/** 基于上述动画测试基类快速声明默认标志的测试。 */
/** Macro to quickly create tests based on the above test object. */
#define ACTOR_ANIMATION_TEST(_ClassName, _TestDir) TEST_CLASS_WITH_BASE(_ClassName, _TestDir, ShooterTestsActorAnimationTest)

/** 基于上述动画测试基类快速声明带自定义 Automation 标志的测试。 */
/** Macro to quickly create tests based on the above test object with custom flags. */
#define ACTOR_ANIMATION_TEST_WITH_FLAGS(_ClassName, _TestDir, _Flags) TEST_CLASS_WITH_BASE_AND_FLAGS(_ClassName, _TestDir, ShooterTestsActorAnimationTest, _Flags)

#endif // WITH_AUTOMATION_TESTS
