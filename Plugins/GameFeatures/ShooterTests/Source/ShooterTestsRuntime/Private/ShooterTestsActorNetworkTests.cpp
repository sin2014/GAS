// Copyright Epic Games, Inc.All Rights Reserved.

#include "Utilities/ShooterTestsActorNetworkTest.h"

#if ENABLE_SHOOTERTESTS_NETWORK_TEST

/**
 * 声明 InputAnimationTest 网络测试，在 Listen Server 与独立 Client PIE 会话中注入跳跃、移动和蹲伏输入，并验证本地玩家动画已复制到另一会话的对应玩家。
 * FShooterTestsNetworkComponent 将 ThenServer/ThenClient 和 UntilServer/UntilClient 分别路由到正确世界；超时会使测试失败。
 */
/**
 * Creates a standalone test object using the name from the first parameter, in the case `InputAnimationTest`, which inherits from `ShooterTestsActorAnimationNetworkTest<Derived, AsserterType, FShooterTestsActorInputTestHelper>` to provide us our testing functionality.
 * The second parameter specifies the category and subcategories used for displaying within the UI
 *
 * Note that this test uses the ACTOR_ANIMATION_NETWORK_TEST macro, which is defined in `/Utilities/ShooterTestsNetworkTest.h`, to inherit from a base class with user-defined variables and methods. Reference that document for more information.
 *
 * The test object will test animation playback during specific inputs while in a networked session. All variables are reset after each test iteration.
 *
 * The test makes use of the `TestCommandBuilder` to queue up latent commands to be executed on every Tick of the Engine/Editor
 * Because of the nature of a network session having a server and client, the `FShooterTestsNetworkComponent` handles splitting up the commands between the Server and Client PIE sessions.
 * `ThenServer` and `ThenClient` steps will execute within a single tick on the server or client
 * `UntilServer` and `UntilClient` steps will keep executing each tick on the server or client until the predicate has evaluated to true or the timeout period has elapsed. The latter will fail the test.
 *
 * Each TEST_METHOD will register with the `InputAnimationTest` test object and has the variables and methods from `InputAnimationTest` available for use.
 */

ACTOR_ANIMATION_NETWORK_TEST(InputAnimationTest, "Project.Functional Tests.ShooterTests.Actor.Replication")
{
	// 调用网络动画测试基类构造函数，指定完整地图包路径。
	// Make a call to our base Constructor to set level to load
	InputAnimationTest() : ShooterTestsActorAnimationNetworkTest(TEXT("/ShooterTests/Maps/L_ShooterTest_Basic"))
	{
	}

	/** 测试名称以前缀区分服务端、客户端或双端玩家；每项同时指定输入动作和预期动画，并通过基类辅助方法跨会话验证。 */
	/**
	 * Each test is registered with the name of the type of player being tested and checks both the expected animation and the input action used to trigger the animation
	 * If a test is focusing on the Server Player, the test name will be prefixed with `ServerPlayer`. Similarly, the Client Player tests are prefixed as `ClientPlayer`
	 * If a test is set to be performed on both the Server and CLient Players, then the prefix will be `NetworkPlayers`
	 * Helper methods are provided within `ShooterTestsActorNetworkTest` to help streamline with the repeated functionality of performing inputs and fetching and testing animations on either the Server or Clients
	 */
	TEST_METHOD(NetworkPlayers_Jump)
	{
		FShooterTestsActorInputTestHelper::InputActionType Input = FShooterTestsActorInputTestHelper::InputActionType::Jump;

		// 在客户端本地玩家执行跳跃，并在客户端与服务端世界验证动画。
		// Test animation and input for the client instance
		FetchAnimationAssetForClientPlayer(FShooterTestsAnimationTestHelper::PistolJumpAnimationName);
		PerformInputOnClientPlayer(Input);
		IsClientPlayerAnimationPlayingOnAllClients();

		// 在服务端本地玩家执行跳跃，并在服务端与客户端世界验证动画。
		// Test animation and input for the server instance
		FetchAnimationAssetForServerPlayer(FShooterTestsAnimationTestHelper::PistolJumpAnimationName);
		PerformInputOnServerPlayer(Input);
		IsServerPlayerAnimationPlayingOnAllClients();
	}

	TEST_METHOD(ServerPlayer_MannyMovement)
	{
		// 映射 Manny 的移动动画及对应输入；考虑出生点布局，先向左再向右测试以留出避免碰撞和进入动画状态的空间。
		// Map of the animations and their corresponding inputs to be tested against in the loop below.
		// Note that due to how the level is laid out, the server player spawns at the first found player location, while the client spawns to the next of them to avoid immediate collisions.
		// We take the spawn into account and have the server player test their directional movements by going left before going to the right to allow for some additional buffer before the animation starts playing.
		TMap<FString, FShooterTestsActorInputTestHelper::InputActionType> MovementAnimations
		{
			{ FShooterTestsAnimationTestHelper::MannyPistolJogForwardAnimationName, FShooterTestsActorInputTestHelper::InputActionType::MoveForward },
			{ FShooterTestsAnimationTestHelper::MannyPistolJogBackwardAnimationName, FShooterTestsActorInputTestHelper::InputActionType::MoveBackward },
			{ FShooterTestsAnimationTestHelper::MannyPistolStrafeLeftAnimationName, FShooterTestsActorInputTestHelper::InputActionType::StrafeLeft },
			{ FShooterTestsAnimationTestHelper::MannyPistolStrafeRightAnimationName, FShooterTestsActorInputTestHelper::InputActionType::StrafeRight },
		};

		for (const TPair<FString, FShooterTestsActorInputTestHelper::InputActionType>& MovementAnimation : MovementAnimations)
		{
			const FString& AnimationName = MovementAnimation.Key;
			FShooterTestsActorInputTestHelper::InputActionType Input = MovementAnimation.Value;

			FetchAnimationAssetForServerPlayer(AnimationName);
			PerformInputOnServerPlayer(Input);
			IsServerPlayerAnimationPlayingOnAllClients();
			StopAllInputsOnServerPlayer();
		}
	}

	TEST_METHOD(ClientPlayer_QuinnMovement)
	{
		// 映射 Quinn 的移动动画及对应输入；考虑出生点布局，先向右再向左测试以留出避免碰撞和进入动画状态的空间。
		// Map of the animations and their corresponding inputs to be tested against in the loop below.
		// Note that due to how the level is laid out, the server player spawns at the first found player location, while the client spawns to the next of them to avoid immediate collisions.
		// We take the spawn into account and have the client player test their directional movements by going right before going to the left to allow for some additional buffer before the animation starts playing.
		TMap<FString, FShooterTestsActorInputTestHelper::InputActionType> MovementAnimations
		{
			{ FShooterTestsAnimationTestHelper::QuinnPistolJogForwardAnimationName, FShooterTestsActorInputTestHelper::InputActionType::MoveForward },
			{ FShooterTestsAnimationTestHelper::QuinnPistolJogBackwardAnimationName, FShooterTestsActorInputTestHelper::InputActionType::MoveBackward },
			{ FShooterTestsAnimationTestHelper::QuinnPistolStrafeRightAnimationName, FShooterTestsActorInputTestHelper::InputActionType::StrafeRight },
			{ FShooterTestsAnimationTestHelper::QuinnPistolStrafeLeftAnimationName, FShooterTestsActorInputTestHelper::InputActionType::StrafeLeft },
		};

		for (const TPair<FString, FShooterTestsActorInputTestHelper::InputActionType>& MovementAnimation : MovementAnimations)
		{
			const FString& AnimationName = MovementAnimation.Key;
			FShooterTestsActorInputTestHelper::InputActionType Input = MovementAnimation.Value;

			FetchAnimationAssetForClientPlayer(AnimationName);
			PerformInputOnClientPlayer(Input);
			IsClientPlayerAnimationPlayingOnAllClients();
			StopAllInputsOnClientPlayer();
		}
	}

	TEST_METHOD(NetworkPlayers_Crouch)
	{
		FShooterTestsActorInputTestHelper::InputActionType Input = FShooterTestsActorInputTestHelper::InputActionType::Crouch;

		// 在客户端本地玩家切换蹲伏，并在两端验证蹲伏待机动画。
		// Test animation and input for the client instance
		FetchAnimationAssetForClientPlayer(FShooterTestsAnimationTestHelper::PistolCrouchIdleAnimationName);
		PerformInputOnClientPlayer(Input);
		IsClientPlayerAnimationPlayingOnAllClients();
		
		// 在服务端本地玩家切换蹲伏，并在两端验证蹲伏待机动画。
		// Test animation and input for the server instance
		FetchAnimationAssetForServerPlayer(FShooterTestsAnimationTestHelper::PistolCrouchIdleAnimationName);
		PerformInputOnServerPlayer(Input);
		IsServerPlayerAnimationPlayingOnAllClients();
	}
};

#endif // ENABLE_SHOOTERTESTS_NETWORK_TEST
