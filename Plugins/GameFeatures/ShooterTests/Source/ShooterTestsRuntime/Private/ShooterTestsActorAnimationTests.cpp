// Copyright Epic Games, Inc.All Rights Reserved.

#include "Utilities/ShooterTestsActorTest.h"

#if WITH_AUTOMATION_TESTS

#include "Helpers/CQTestAssetFilterBuilder.h"
#include "Helpers/CQTestAssetHelper.h"

#include "Engine/DataAsset.h"
#include "Equipment/LyraEquipmentManagerComponent.h"
#include "Equipment/LyraPickupDefinition.h"
#include "Misc/Paths.h"
#include "ObjectBuilder.h"
#include "Weapons/LyraWeaponInstance.h"
#include "Weapons/LyraWeaponSpawner.h"

/**
 * 声明 InputCrouchAnimationTest：加载基础地图、让玩家进入蹲伏状态，再分别注入蹲伏移动与转身输入并等待对应动画播放。
 * 该测试复用 ShooterTestsActorAnimationTest 的玩家、Mesh、输入和动画查询能力；Do/Then 单 Tick 执行，Until 逐 Tick 等待或超时失败。
 */
/**
 * Creates a standalone test object using the name from the first parameter, in the case `InputCrouchAnimationTest`, which inherits from `ShooterTestsActorAnimationTest<Derived, AsserterType>` to provide us our testing functionality.
 * The second parameter specifies the category and subcategories used for displaying within the UI
 * 
 * Note that this test uses the ACTOR_ANIMATION_TEST macro, which is defined in `/Utilities/ShooterTestsActorTest.h`, to inherit from a base class with user-defined variables and methods. Reference that document for more information.
 *
 * The test object will test animation playback during specific inputs while crouched. All variables are reset after each test iteration.
 * 
 * The test makes use of the `TestCommandBuilder` to queue up latent commands to be executed on every Tick of the Engine/Editor
 * `Do` and `Then` steps will execute within a single tick
 * `Until` steps will keep executing each tick until the predicate has evaluated to true or the timeout period has elapsed. The latter will fail the test.
 *
 * Each TEST_METHOD will register with the `InputCrouchAnimationTest` test object and has the variables and methods from `InputCrouchAnimationTest` available for use.
 */
ACTOR_ANIMATION_TEST(InputCrouchAnimationTest, "Project.Functional Tests.ShooterTests.Actor.Animation")
{
	// 调用动画测试基类构造函数，指定需要加载的基础测试地图名称。
	// Make a call to our base Constructor to set the name of the level to load
	InputCrouchAnimationTest() : ShooterTestsActorAnimationTest(TEXT("L_ShooterTest_Basic"))
	{
	}
	
	/** 每个测试前先执行基类地图/玩家准备，再注入蹲伏并等待蹲伏待机动画，确保后续用例拥有一致前置状态。 */
	/**
	 * Run before each TEST_METHOD to load our level, initialize our Player, and Player components needed for the tests before to execute successfully.
	 * The test also makes sure that the Player is playing a crouching idle animation after toggling the Player's crouched state.
	 * If an ASSERT_THAT fails at any point, the TEST_METHODS will also fail as this means that our test prerequisites were not setup
	 * 
	 * Note that because we're derived from a user-defined base class, that we will also call our base `Setup`. It's importanrt to know that `BEFORE_EACH` is a macro that wraps around `virtual void Setup() override`
	 */
	BEFORE_EACH()
	{
		ShooterTestsActorAnimationTest::Setup();

		TestCommandBuilder
			.Do([this]() { GetExpectedAnimation(FShooterTestsAnimationTestHelper::PistolCrouchIdleAnimationName); })
			.Then([this]() { PawnActions->ToggleCrouch(); })
			.Until([this]() { return AnimationTestHelper.IsAnimationPlaying(PlayerMesh, ExpectedAnimation); });
	}
	
	/** 每个用例仅替换预期动画名称和输入动作，通过基类 TestInputActionAnimation 复用输入注入和动画等待步骤。 */
	/**
	 * Each test is registered with the name of the type of movement being tested and checks both the expected animation and the input action used to trigger the animation
	 * Due to the nature of the test being the same with the exception of the animation name and the input action, we have a helper method implemented in from our base class being used
	 */
	TEST_METHOD(PlayerCrouched_ForwardMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchWalkForwardAnimationName, [this]() { PawnActions->MoveForward(); });
	}
	
	TEST_METHOD(PlayerCrouched_BackwardMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchWalkBackwardAnimationName, [this]() { PawnActions->MoveBackward(); });
	}
	
	TEST_METHOD(PlayerCrouched_StrafeLeftMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchStrafeLeftAnimationName, [this]() { PawnActions->StrafeLeft(); });
	}
	
	TEST_METHOD(PlayerCrouched_StrafeRightMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchStrafeRightAnimationName, [this]() { PawnActions->StrafeRight(); });
	}
	
	TEST_METHOD(PlayerCrouched_RotateLeftMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchRotateLeftAnimationName, [this]() { PawnActions->RotateLeft(); });
	}
	
	TEST_METHOD(PlayerCrouched_RotateRightMovement)
	{
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolCrouchRotateRightAnimationName, [this]() { PawnActions->RotateRight(); });
	}
};

/**
 * 声明仅在 EditorContext 运行的 WeaponMeleeAnimationTest。测试生成指定武器拾取 Pad，等待玩家装备相应武器，再触发近战并验证武器专属 Montage。
 * 该测试依赖本地 Blueprint/DataAsset，因此非 Editor 运行前需要随游戏 Cook 相关资产。
 */
/**
 * Creates a standalone test object using the name from the first parameter, in the case `WeaponMeleeAnimationTest`, that inherits from `ShooterTestsActorAnimationTest<Derived, AsserterType>` to provide us our testing functionality.
 * The second parameter specifies the category and subcategories used for displaying within the UI
 * The third parameter specifies the flags as to what context the test will run in and the filter to be applied for the test to appear in the UI
 *
 * Note that this test uses the ACTOR_ANIMATION_TEST macro, which is defined in `/Utilities/ShooterTestsActorTest.h`, to inherit from a base class with user-defined variables and methods. Reference that document for more information.
 * This is a test which will be run in the Editor context as this test spawns objects based on Blueprints found on the local filesystem. Assets require to be cooked along the game to run in the game/client
 *
 * The test object will test spawning weapons for the Player to equip. Test will also verify that the proper melee animation is triggered for the respective input and equipped weapon. All variables are reset after each test iteration.
 * The test makes use of the `TestCommandBuilder` to queue up latent commands to be executed on every Tick of the Engine/Editor
 * `StartWhen` steps will keep executing each tick until the predicate has evaluated to true or the timeout period has elapsed. The latter will fail the test.
 *
 * Each TEST_METHOD will register with the `WeaponMeleeAnimationTest` test object and has the variables and methods from `WeaponMeleeAnimationTest` available for use.
 */
ACTOR_ANIMATION_TEST_WITH_FLAGS(WeaponMeleeAnimationTest, "Project.Functional Tests.ShooterTests.Actor.Animation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
	ALyraWeaponSpawner* WeaponSpawnerPad{ nullptr };

	// 调用动画测试基类构造函数，指定需要加载的基础测试地图名称。
	// Make a call to our base Constructor to set the name of the level to load
	WeaponMeleeAnimationTest() : ShooterTestsActorAnimationTest(TEXT("L_ShooterTest_Basic"))
	{
	}
	
	// 从 EquipmentManager 获取首个 LyraWeaponInstance，并按实例类名确认指定武器已装备且 Instigator 有效。
	// Checks to see if the specified Weapon is currently equipped by the player
	bool IsCurrentlyEquippedWeapon(const FString& WeaponName)
	{
		if (ULyraEquipmentManagerComponent* EquipmentManager = Player->FindComponentByClass<ULyraEquipmentManagerComponent>())
		{
			if (ULyraWeaponInstance* WeaponInstance = EquipmentManager->GetFirstInstanceOfType<ULyraWeaponInstance>())
			{
				const bool bIsWeaponEquipped = IsValid(WeaponInstance) && IsValid(WeaponInstance->GetInstigator());
				return bIsWeaponEquipped && WeaponInstance->GetClass()->GetName().Equals(WeaponName);
			}
		}
	
		return false;
	}
	
	// 查找武器拾取 DataAsset 与 B_WeaponSpawner Blueprint，在玩家位置生成配置了该 WeaponDefinition 的拾取 Pad。
	// Spawns the weapon pad with the appropriate weapon to be picked up by the Player
	void SpawnWeaponSpawnerPad(const FString& WeaponDataAsset)
	{
		// 构造递归包含 UDataAsset 派生类的资产筛选器，用于定位武器拾取定义。
		// Generate our DataAsset filter used to find our weapon assets
		FARFilter DataAssetFilter = CQTestAssetHelper::FAssetFilterBuilder()
			.WithClassPath(UDataAsset::StaticClass()->GetClassPathName())
			.IncludeRecursiveClasses()
			.Build();

		UClass* WeaponSpawnerPadBp = CQTestAssetHelper::GetBlueprintClass(TEXT("B_WeaponSpawner"));
		ASSERT_THAT(IsNotNull(WeaponSpawnerPadBp));

		UObject* WeaponData = CQTestAssetHelper::FindDataBlueprint(DataAssetFilter, WeaponDataAsset);
		ASSERT_THAT(IsNotNull(WeaponData));
		ULyraWeaponPickupDefinition* WeaponDefinition = Cast<ULyraWeaponPickupDefinition>(WeaponData);
		ASSERT_THAT(IsNotNull(WeaponDefinition));
	
		WeaponSpawnerPad = &TObjectBuilder<ALyraWeaponSpawner>(*Spawner, WeaponSpawnerPadBp)
			.SetParam("WeaponDefinition", WeaponDefinition)
			.Spawn(Player->GetTransform());
	
		ASSERT_THAT(IsNotNull(WeaponSpawnerPad));
	}

	// 生成指定武器的拾取 Pad，并持续等待玩家装备对应 WeaponInstance 类。
	// Waits until the weapon is spawned and equipped
	void EquipSpawnedWeapon(const FString& WeaponName)
	{
		FString WeaponDataAsset = FString::Printf(TEXT("WeaponPickupData_%s"), *WeaponName);
		FString EquippedWeaponInstanceName = FString::Printf(TEXT("B_WeaponInstance_%s_C"), *WeaponName);
		SpawnWeaponSpawnerPad(WeaponDataAsset);

		TestCommandBuilder
			.StartWhen([this, EquippedWeaponInstanceName = MoveTemp(EquippedWeaponInstanceName)]() { return IsCurrentlyEquippedWeapon(EquippedWeaponInstanceName); });
	}
	
	/** 各用例仅替换武器数据名称和预期近战动画，复用装备等待与输入动画验证步骤。 */
	/**
	 * Each test is registered with the name of the type of movement being tested and checks both the expected animation and the input action used to trigger the animation
	 * Due to the nature of the test being the same with the exception of the animation name and the input action, we use helper methods to implement the steps
	 */
	TEST_METHOD(WeaponMelee_Pistol)
	{
		EquipSpawnedWeapon(TEXT("Pistol"));
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::PistolMeleeAnimationName, [this]() { PawnActions->PerformMelee(); });
	}
	
	TEST_METHOD(WeaponMelee_Rifle)
	{
		EquipSpawnedWeapon(TEXT("Rifle"));
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::RifleMeleeAnimationName, [this]() { PawnActions->PerformMelee(); });
	}
	
	TEST_METHOD(WeaponMelee_Shotgun)
	{
		EquipSpawnedWeapon(TEXT("Shotgun"));
		TestInputActionAnimation(FShooterTestsAnimationTestHelper::ShotgunMeleeAnimationName, [this]() { PawnActions->PerformMelee(); });
	}
};

#endif // WITH_AUTOMATION_TESTS
