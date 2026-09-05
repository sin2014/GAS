# Shooter 测试插件

# Shooter Tests Plugin

**Shooter Tests** 是随 **Lyra** 游戏内容一同提供的插件，包含针对 **Lyra** 的自动化测试。本文档用于说明如何使用该插件，并详细介绍其中已有的测试。

**Shooter Tests** is a plugin which is bundled as part of the **Lyra** game content and contains tests which are run against **Lyra**. This document is meant to serve as a guide on working with the plugin and providing a detailed overview on the provided tests.

本指南分为两个主要部分：第一部分概述插件，第二部分详细说明可用的测试类型和具体测试。

- [插件概述](#plugin-overview)
  - [插件结构](#plugin-structure)
  - [如何运行插件提供的测试](#how-to-run-tests-provided-by-the-plugin)
- [插件内的测试](#tests-within-the-plugin)
  - [CQTests](#cqtests)
    - [CQTest 前置知识](#cqtest-prerequisites)
    - [动画测试前置知识](#animation-test-prerequisites)
    - [InputCrouchAnimationTest](#inputcrouchanimationtest)
    - [WeaponMeleeAnimationTest](#weaponmeleeanimationtest)
    - [复制测试前置知识](#replication-test-prerequisites)
    - [InputAnimationTest](#inputanimationtest)
    - [AbilitySpawnerMapTest](#abilityspawnermaptest)
  - [蓝图功能测试](#blueprint-functional-tests)
    - [B\_Test\_AutoRun](#b_test_autorun)
    - [B\_Test\_FireWeapon](#b_test_fireweapon)
- [Lyra 自动化测试故障排查](#troubleshooting-lyra-automated-tests)
  - [网络测试无法使用新地图](#cannot-use-a-new-map-for-a-network-test)
  - [每次测试随机生成角色导致不稳定](#tests-are-flaky-due-to-each-test-run-spawning-a-random-character)
- [参考资源](#resources)

This guide is split up into two main sections each with their own focus. The first section provides an overview of the plugin while the second section goes into detail about the different test types and tests available
- [Plugin Overview](#plugin-overview)
  - [Plugin Structure](#plugin-structure)
  - [How to run tests provided by the plugin](#how-to-run-tests-provided-by-the-plugin)
- [Tests Within the Plugin](#tests-within-the-plugin)
  - [CQTests](#cqtests)
    - [CQTest Prerequisites](#cqtest-prerequisites)
    - [Animation Test Prerequisites](#animation-test-prerequisites)
    - [InputCrouchAnimationTest](#inputcrouchanimationtest)
    - [WeaponMeleeAnimationTest](#weaponmeleeanimationtest)
    - [Replication Test Prerequisites](#replication-test-prerequisites)
    - [InputAnimationTest](#inputanimationtest)
    - [AbilitySpawnerMapTest](#abilityspawnermaptest)
  - [Blueprint Functional Tests](#blueprint-functional-tests)
    - [B\_Test\_AutoRun](#b_test_autorun)
    - [B\_Test\_FireWeapon](#b_test_fireweapon)
- [Troubleshooting Lyra automated tests](#troubleshooting-lyra-automated-tests)
  - [Cannot use a new map for a network test](#cannot-use-a-new-map-for-a-network-test)
  - [Tests are flaky due to each test run spawning a random character](#tests-are-flaky-due-to-each-test-run-spawning-a-random-character)
- [Resources](#resources)

## 插件概述

## Plugin Overview

本文档只介绍 **Shooter Tests** 插件。关于创建新插件或启用、禁用现有插件等通用内容，请参阅[插件在线文档](https://dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine)。

The scope of this document is to only cover the **Shooter Tests** plugin. For a more generalized guide to plugins, including how to create a new plugin or enabling/disabling existing plugins, please see [the online documentation on plugins](https://dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine) for more information.

本节概述随 **Lyra** Starter Content 提供的插件。插件可通过 [Epic Games Launcher](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine) 获取，位置为 `/LyraStarterGame/Plugins/GameFeatures/ShooterTests`。后续小节将介绍目录结构、如何在 Lyra 中启用插件，以及如何运行插件自带测试。

This section provides an overview of the plugin. The plugin comes bundled as part of the **Lyra** starter content which can be downloaded from [the Epic Games Launcher](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine) and can be found in `/LyraStarterGame/Plugins/GameFeatures/ShooterTests`. The underlying sections below will go into further detail about different aspects of the plugin. Specifics include: the structure of the plugin, how to enable the plugin for use within Lyra, and how to run the tests packaged with the plugin.

### 插件结构

### Plugin Structure

下面是插件的总体目录结构。

Below is the general document structure of the plugin

```
.
└── ShooterTests/
    ├── Binaries/
    │   └── Platform dependant built libraries
    ├── Content/
    │   ├── Blueprint
    │   ├── Input
    │   ├── Maps
    │   ├── System
    │   ├── ShooterTests.uasset
    ├── Intermediate/
    │   └── Platform dependant generated code
    ├── Resources/
    │    └── Icon128.png
    └── Source/
        └── ShooterTestsRuntime/
            └── Private/
```

本文档重点介绍 `Content` 和 `Source` 目录，因为测试及其所需资源位于这些目录中。例如，[蓝图功能测试](#blueprint-functional-tests)的测试蓝图存放在 `Blueprint` 目录，加载该蓝图的地图则位于 `Map` 目录。测试组成看似分散，但这是为了让可复用资产位于适当目录；按职责拆分组件可以避免重复资产和插件体积增长。

The focus and scope of this document will be on the `Content` and `Source` directories as this will be where the tests and resources needed for tests will reside. For example, the [Blueprint Functional Tests](#blueprint-functional-tests) will have a Blueprint class for the test stored within the `Blueprint` directory, but the map that loads the Blueprint asset will be found in the `Map` directory. While it seems that the pieces of the test are scattered within the plugin, they all reside in the appropriate folder as some assets are reused in other tests. It's best practice to split up your components and organize them in a way that makes sense as opposed to bundling all pieces of a test together as the latter can lead to duplication of assets and a larger plugin size.

### 如何运行插件提供的测试

### How to run tests provided by the plugin

本节要求了解 **Automation System**，并知道如何在 **Session Frontend** 的 **Automation** 标签页中查找和执行测试，详情参阅 [Automation System 在线文档](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-user-guide-in-unreal-engine)。打开该标签页后，可通过 `Project -> Functional Tests -> Shooter Tests` 找到插件测试，也可在顶部 **Search** 栏输入 **ShooterTests** 筛选。下一节会详细解释插件包含的测试类型和目标。

This section requires knowledge about the **Automation System** and how to navigate the **Automation** tab of the **Session Frontend** to find and execute tests. Please see [the online documentation on the Automation System](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-user-guide-in-unreal-engine) for more information. With the **Session Frontend** opened to the **Automation** tab, we can navigate to `Project -> Functional Tests -> Shooter Tests` in order to get to the tests currently packaged within the plugin. These tests can also be reached by using the **Search** bar on top and using the keyword **ShooterTests** to filter out and only show the tests in this plugin. See the section immediately below for more detailed explanation on what types of tests are included and what is being tested.

## 插件内的测试

## Tests Within the Plugin

插件展示了使用蓝图或 C++ 代码实现测试的方式，本节说明插件中包含的各类测试。

The plugin demonstrates tests using either Blueprint or code. This section describes various tests packaged with the plugin.

* [CQTests](#cqtests)
* [蓝图功能测试](#blueprint-functional-tests)

* [CQTests](#cqtests)
* [Blueprint Functional Tests](#blueprint-functional-tests)

### CQTests（代码质量测试）

### CQTests

**CQTest** 即 Code Quality Tests，是使用 C++ 创建功能测试的方法。**ShooterTests** 启用了 Unreal Engine 的 **CQTest** 模块。之所以选择该框架，是因为它为每个测试用例提供前置/后置功能，并会自动重置测试状态，确保测试彼此独立，避免对象在用例间泄漏。更深入的说明请参阅 `/Engine/Source/Developer/CQTest` 中的 README。

**CQTest**, or Code Quality Tests, are a method of creating functional tests using C++. The **CQTest** framework is an Unreal Engine module and is enabled within **ShooterTests**. Unreal Engine provides multiple testing frameworks, but the focus on CQTest was decided due to providing before/after functionality that is paired with each test case. Another benefit is that **CQTest** resets the state of each test automatically, making sure that each test is atomic in that there is no worry about leaking objects to or from another test. Please refer to the readme documentation located in `/Engine/Source/Developer/CQTest` for a deeper understanding of **CQTest**.

**Shooter Tests** 使用 **CQTest** 框架，并按功能类别和子类别组织。蓝图功能测试在 **Automation** 标签页中按所在地图分类；点击蓝图测试会加载含 Functional Test Actor 的关卡，而点击 **CQTest** 会打开实现该测试的代码文件。

**Shooter Tests** are tests implemented using the **CQTest** framework within their respective categories. Within the categories, it's possible to have subcategories to help further define the type or functionality expected to be tested. While [Blueprint Functional Tests](#blueprint-functional-tests) are categorized in the **Automation** tab by the map that they reside in. Similar to how clicking on a [Blueprint Functional Tests](#blueprint-functional-tests) will load the level with the Functional Test Actor, clicking on a **CQTest** will open the code file where the test is implemented.

使用 **CQTest** 创建测试时，可以在声明 `TEST_CLASS` 或 `TEST` 时自定义分类，例如：

Tests created using the **CQTest** framework allows for custom categorization when declaring the `TEST_CLASS` or `TEST` itself. For example

```
TEST(SimpleTest, "Project.Functional Tests.ShooterTests.Tests")
{
    ASSERT_THAT(IsTrue(true));
}
```

该代码会在 **Automation** 标签页的 `Project -> Functional Tests -> ShooterTests -> Tests` 下显示名为 `SimpleTest` 的条目。有关 **CQTest** 的更多信息，请参阅前述 README 和下方示例。

will show an item labeled `SimpleTest` under `Project -> Functional Tests -> ShooterTests -> Tests` within the **Automation** tab. More information about the **CQTest** framework is documented in the readme mentioned above and the examples outlined below.

插件当前测试类别和子类别如下：

Here are the current categories and subcategories of the tests within the plugin:

* **Actor（Actor 测试）**
  * **Animation（动画）**
    * [InputCrouchAnimationTest](#inputcrouchanimationtest)
    * [WeaponMeleeAnimationTest](#weaponmeleeanimationtest)
  * **Replication（复制）**
    * [InputAnimationTest](#inputanimationtest)
* **GameplayAbility（Gameplay Ability）**
  * [AbilitySpawnerMapTest](#abilityspawnermaptest)

* **Actor**
  * **Animation**
    * [InputCrouchAnimationTest](#inputcrouchanimationtest)
    * [WeaponMeleeAnimationTest](#weaponmeleeanimationtest)
  * **Replication**
    * [InputAnimationTest](#inputanimationtest)
* **GameplayAbility**
  * [AbilitySpawnerMapTest](#abilityspawnermaptest)

#### CQTest 前置知识

#### CQTest Prerequisites

下文的 [InputCrouchAnimationTest](#inputcrouchanimationtest)、[WeaponMeleeAnimationTest](#weaponmeleeanimationtest) 和 [InputAnimationTest](#inputanimationtest) 使用自定义 `TEST_CLASS` 实现。**CQTest** 创建测试时提供以下宏：

Please note that the [InputCrouchAnimationTest](#inputcrouchanimationtest), [WeaponMeleeAnimationTest](#weaponmeleeanimationtest), and [InputAnimationTest](#inputanimationtest) below follows a custom implementation of the `TEST_CLASS`. **CQTest** provides the following macros when creating tests:

* `TEST`
* `TEST_CLASS`
* `TEST_CLASS_WITH_BASE`
* `TEST_CLASS_WITH_FLAGS`
* `TEST_CLASS_WITH_BASE_AND_FLAGS`
* `TEST_METHOD`

动画测试基于上述宏定义了两个新宏：

The Animation tests use the above to create 2 new macros:

* `ACTOR_ANIMATION_TEST`
  * 封装 `TEST_CLASS_WITH_BASE`，并指定 `ShooterTestsActorAnimationTest` 为基础结构体。
* `ACTOR_ANIMATION_TEST_WITH_FLAGS`
  * 封装 `TEST_CLASS_WITH_BASE_AND_FLAGS`，并指定 `ShooterTestsActorAnimationTest` 为基础结构体。

* `ACTOR_ANIMATION_TEST`
  * Wraps the `TEST_CLASS_WITH_BASE` macro specifying the `ShooterTestsActorAnimationTest` as the base struct.
* `ACTOR_ANIMATION_TEST_WITH_FLAGS`
  * Wraps the `TEST_CLASS_WITH_BASE_AND_FLAGS` macro specifying the `ShooterTestsActorAnimationTest` as the base struct.

复制测试也基于上述宏定义了自己的宏：

The Replication test also uses the above to create its own macro:

* `ACTOR_ANIMATION_NETWORK_TEST`
  * 封装 `TEST_CLASS_WITH_BASE_AND_FLAGS`，指定 `ShooterTestsActorAnimationNetworkTest` 为基础结构体，并使用 `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`，因为这些测试只能在 Editor 中运行。

* `ACTOR_ANIMATION_NETWORK_TEST`
  * Wraps the `TEST_CLASS_WITH_BASE_AND_FLAGS` macro specifying the `ShooterTestsActorAnimationNetworkTest` as the base struct and specifies the flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` as these tests cannot run outside of the Editor.

`ShooterTestsActorBaseTest` 和 `ShooterTestsActorNetworkTest` 都继承自模板类型 `public TTest<Derived, AsserterType>`，后者建立自动化测试的基本结构。**CQTest** Engine 插件根目录的 README 介绍了其核心结构。继承 `TTest` 后，测试既能获得注册和执行能力，也能扩展特定测试共享的功能。

Both the `ShooterTestsActorBaseTest` and `ShooterTestsActorNetworkTest` inherits from `public TTest<Derived, AsserterType>` which is a templated object that setups up the structure of an Automated Test. More information about the structure and information about the core of **CQTest** can be found in the README document within the root directory of the **CQTest** Engine plugin. By inheriting from `TTest` we gain all of the functionality that allows for the tests to be registered and executed, while being able to expand on the functionality that would be shared by the specific tests.

#### 动画测试前置知识

#### Animation Test Prerequisites

一些测试会继续继承上述对象，以扩展动画测试所需功能。`ACTOR_ANIMATION_TEST` 宏使用自定义的 `ShooterTestsActorAnimationTest`，它派生自 `ShooterTestsActorBaseTest`。这种层级与 Actor/Animation 分类一致，可将动画成员与只需要 Actor 基础能力的测试分离。

There are also tests that inherit from the above mentioned objects to further expand the functionality needed for testing. The newly defined `ACTOR_ANIMATION_TEST` macros take in a new user defined class called `ShooterTestsActorAnimationTest` which has been created to help with testing the animation functionality as `ShooterTestsActorAnimationTest` is derived from `ShooterTestsActorBaseTest`. The reason being is similar to how we have a category of tests for an `Actor` with a subcategory of `Animation`, we also have a base class for the Actor called `ShooterTestsActorBaseTest` with `ShooterTestsActorAnimationTest` being derived. This way we can keep the members needed for animations separate from tests that just need or will expand upon the Actor.

为了在新宏中使用 `ShooterTestsActorAnimationTest` 作为基类，基础类型保持 `Derived` 和 `AsserterType` 模板参数，并由 `ShooterTestsActorAnimationTest` 继承 `public ShooterTestsActorBaseTest<Derived, AsserterType>`。因此动画测试同时获得 Actor 基础能力和动画专用能力。

To use our newly defined `ACTOR_ANIMATION_TEST` macros, we need to specify `ShooterTestsActorAnimationTest` as the base class, but if `ShooterTestsActorAnimationTest` is derived from `ShooterTestsActorBaseTest`, how do we exactly create a base class to then be used in our macro? If we focus on either `ShooterTestsActorBaseTest` we can see that these classes inherit from `public TTest<Derived, AsserterType>` which is a templated object. From there we can create the `ShooterTestsActorAnimationTest` object which derives from `public ShooterTestsActorBaseTest<Derived, AsserterType>`, again with the template specified. With this we can now have our `ShooterTestsActorAnimationTest` object which provides access and functionality to the animation tests as well as providing functionality from `ShooterTestsActorBaseTest` for our Actor.

动画测试使用 `ShooterTestsAnimationTestHelper` 和 `ShooterTestsInputTestHelper` 提供的辅助功能。

The animation tests make use of helper functionality provided by `ShooterTestsAnimationTestHelper` and `ShooterTestsInputTestHelper`.

**ShooterTestsAnimationTestHelper**

`ShooterTestsAnimationTestHelper` 用于查找 `UAnimationAsset` 并判断其是否正在播放。相关方法需要 `USkeletalMeshComponent`：`FindAnimationAsset` 查找名称匹配且 Skeleton 兼容的资产；`IsAnimationPlaying` 遍历组件的所有动画实例，检查活动 Montage、同步组和非分组播放器。

**ShooterTestsAnimationTestHelper**
The `ShooterTestsAnimationTestHelper` object that exists in this file aids in finding `UAnimationAsset` objects and checking if the asset is playing. The object has methods which require the `USkeletalMeshComponent` in order to retrieve animation assets and information. `FindAnimationAsset` will search through the `USkeletalMeshComponent` to make sure that an asset with the same name exists within the component. `IsAnimationPlaying` iterates through all of the animation instances of the `USkeletalMeshComponent` before checking any active montages and sync groups for the expected animation to be playing.

**ShooterTestsInputTestHelper**

该文件定义了多个 Lyra 玩家可使用的输入测试对象，每个被测 `Input Action` 都有对应的 `FTestAction`。当前实现如下：

**ShooterTestsInputTestHelper**
Within this file are multiple input objects that the **Lyra** player has access to. We have multiple `FTestAction` objects for each `Input Action` that is tested against. Currently the following `Input Actions` are implemented and being handled:

* `FToggleCrouchTestAction`
* `FMeleeTestAction`
* `FJumpTestAction`
* `FMoveTestAction`
* `FLookTestAction`

除上述动作外，`FMoveTestAction` 和 `FLookTestAction` 还具有处理二维轴不同方向的派生类型：

In addition to the actions mentioned above, both `FMoveTestAction` and `FLookTestAction` have additional derived objects to handle the different directions along the 2D axis. These include:

* `FMoveForwardTestAction`
* `FMoveBackwardTestAction`
* `FStrafeLeftTestAction`
* `FStrafeRightTestAction`
* `FRotateLeftTestAction`
* `FRotateRightTestAction`

虽然 **Lyra** 玩家还有更多 `Input Actions`，当前只实现测试覆盖所需的动作，后续会随新测试继续增加。

While the **Lyra** player has more `Input Actions` available, these are what are currently handled based on the current test coverage and more will be added as more tests are created.

为触发这些 `Input Actions`，`FShooterTestsPawnTestActions` 继承 `FInputTestActions` 并定义各动作接口。底层流程仍是对 `FTestAction` 调用 `PerformAction`，但具名方法让测试意图和预期行为更清晰。

To be able to trigger all these `Input Actions` the `FShooterTestsPawnTestActions` object extends from `FInputTestActions` to specify each action that will be performed. While all the input follows the same flow of calling `PerformAction` on a `FTestAction` object, we expand upon the actions to make the tests more descriptive as to what is tested against and what is expected to happen.

#### InputCrouchAnimationTest（蹲伏输入动画测试）

#### InputCrouchAnimationTest

**继续阅读前，请先查看上方[动画测试前置知识](#animation-test-prerequisites)。本节使用了自定义的基础 **CQTest** 实现，直接阅读可能产生混淆。**

**Please see the section above about [Animation Test Prerequisites](#animation-test-prerequisites) before going deeper into this section as there are custom implementations of base **CQTest** functionality that may cause confusion.**

**InputCrouchAnimationTest** 由 `ACTOR_ANIMATION_TEST` 宏创建，实现位于 `/ShooterTests/Source/ShooterTestsRuntime/Private/ShooterTestsActorAnimationTests.cpp`。测试验证玩家蹲伏时执行不同移动动作会播放预期动画。

The **InputCrouchAnimationTest** is a test object created from the macro `ACTOR_ANIMATION_TEST` and the implementation can be found in `/ShooterTests/Source/ShooterTestsRuntime/Private/ShooterTestsActorAnimationTests.cpp`. These test that the expected animations are played during certain movement actions while crouched. 

实现分解如下：

The breakdown of the implementation is as follows:

测试使用 `ACTOR_ANIMATION_TEST` 宏以及参数 `InputCrouchAnimationTest` 和 `"Project.Functional Tests.ShooterTests.Actor.Animation"` 创建。宏生成 `TTestRunner` 和 `InputCrouchAnimationTest` 实例。由于基础对象是 `ShooterTestsActorAnimationTest`，且其又派生自 `ShooterTestsActorBaseTest`，测试可直接使用这些类提供的成员和方法；`BEFORE_EACH` 会准备 `PlayerController` 等依赖，`TEST_METHOD` 无需重复初始化。

We create our test object with the `ACTOR_ANIMATION_TEST` macro and the parameters `InputCrouchAnimationTest` and `"Project.Functional Tests.ShooterTests.Actor.Animation"`. As mentioned in the [CQTest Prerequisites](#cqtest-prerequisites), the macro creates our test object with a `TTestRunner` and an instance of the `InputCrouchAnimationTest`. Because the macro uses `ShooterTestsActorAnimationTest` as our base object, which is also derived from `ShooterTestsActorBaseTest`, we get access to all of the member variables and methods provided by these objects. This will allow us to create our `TEST_METHOD` without the need to setup the `PlayerController` or any other components as the `BEFORE_EACH` will handle that for us.

`InputCrouchAnimationTest` 构造函数通过初始化列表调用基础对象构造函数，传入待加载地图的目录和名称。`ShooterTestsActorBaseTest` 会创建 `FMapTestSpawner`，并在 `ShooterTestsActorBaseTest::Setup()` 中负责加载地图。

The `InputCrouchAnimationTest` constructor is defined as a way to call the base object's constructor with an initializer list to provide the directory where the map to load is located and the name of the map to load. This is due to the fact that the `ShooterTestsActorBaseTest` will create an instance of a `FMapTestSpawner`. This object handles loading of maps whenever `ShooterTestsActorBaseTest::Setup()` is called.

`BEFORE_EACH` 宏包装了 `virtual void Setup() override`，因此可以调用 `ShooterTestsActorAnimationTest::Setup();`。该方法继续调用 `ShooterTestsActorBaseTest::Setup()`，开始加载构造函数指定的地图并等待关卡就绪。

`BEFORE_EACH` goes through and prepares the level for the test as the macro has defined a `virtual void Setup() override` which is what allows the call to `ShooterTestsActorAnimationTest::Setup();` to be made. `ShooterTestsActorAnimationTest::Setup();` actually calls the base object's `Setup`, which in this case will be `ShooterTestsActorBaseTest`. What the setup will then do is start loading the map that was specified in the Constructor and waits until the level has been loaded.

**请注意，关卡已标记为可用时，其中的实际资产仍可能尚未完全加载。**

**Note that while the level has been loaded and marked for use, the actual assets may not have fully populated within the level yet.**

随后使用 `TestCommandBuilder` 构建测试开始前的关卡准备步骤：

The `TestCommandBuilder` is then used to build the steps needed to ensure that the level is setup prior to the test.

* 等待地图初始化并找到玩家 `Pawn`。默认超时为 10 秒，但为覆盖 Loading Screen 的额外加载，将其提高到 30 秒。
* `Pawn` 加载后调用 `ShooterTestsActorAnimationTest::PreparePlayerPawn()`，该方法也会调用 `ShooterTestsActorBaseTest::PreparePlayerPawn()`。
  * `ShooterTestsActorBaseTest::PreparePlayerPawn()` 准备主玩家及后续判断完全生成所需的玩家功能。
  * `ShooterTestsActorAnimationTest::PreparePlayerPawn()` 准备动画测试所需功能。
* 等待玩家被判定为完全生成。玩家生成时存在会切换输入处理的 `GameplayEffect`，而动画测试需要输入，因此必须等待该效果结束。

* Wait before starting until the map has been initialized and a player `Pawn` has been found for use. This step defaults to a timeout of 10 seconds, but we increase the timeout to 30 seconds to take into account additional loading done via the loading screen.
* Then, with the `Pawn` loaded a call to `ShooterTestsActorAnimationTest::PreparePlayerPawn()` is made, which also calls `ShooterTestsActorBaseTest::PreparePlayerPawn()`
  * `ShooterTestsActorBaseTest::PreparePlayerPawn()` handles setting up the main player and functionality tied to the player which will be used in a later step when determining if the player has fully spawned in.
  * `ShooterTestsActorAnimationTest::PreparePlayerPawn()` handles setting up functionality tied to testing animations.
* With the functionality now setup, we wait until the player is deemed as being fully spawned. This is done because there is a `GameplayEffect` tied to the player which is triggered on spawn. This effect toggles the input handling which could impact tests as we need input to trigger our animations.

此时进入 **InputCrouchAnimationTest** 自身的 `BEFORE_EACH`。玩家和动画功能已经由基础 `Setup` 准备完毕，本层继续使用 `TestCommandBuilder` 让玩家进入蹲伏状态，因为每个 `TEST_METHOD` 都依赖该前置状态。

At this point we are in the `BEFORE_EACH` of the **InputCrouchAnimationTest** which will again use the `TestCommandBuilder` to continue building on our steps from our base `Setup` now that the player and animation functionality is setup and loaded. The steps here will get the player to be crouched since each `TEST_METHOD` needs the player in this state to continue.

* 使用 Do 取得用于比较的 `MM_Pistol_Crouch_Idle` 动画。
* 使用 Then 触发蹲伏 `Input Action`。
* 等待玩家进入蹲伏并播放上一步取得的预期动画。

* Do, a command which will get the animation for `MM_Pistol_Crouch_Idle` to be tested against
* Then, trigger the crouch `Input Action` to be executed
* Wait until the player is crouched and playing the expected animation retrieved in the above step

完成 `BEFORE_EACH` 后，各 `TEST_METHOD` 验证不同 `Input Action` 是否播放正确动画。由于流程相同，仅预期动画和输入不同，`ShooterTestsActorAnimationTest::TestInputActionAnimation` 封装了通用步骤：先按名称取得 `UAnimationAsset`，再继续构建 `TestCommandBuilder`。

With the `BEFORE_EACH` now setup to be performed before each of our `TEST_METHOD`, we can create all the tests which will verify that the correct animation is playing for each `Input Action`. Because there are multiple tests within the object that all perform the same test flow, but differ in the expected animation to be playing and the `Input Action` needed to be triggered, the method `TestInputActionAnimation` has been implemented as part of the `ShooterTestsActorAnimationTest` object to perform the flow needed with the different parameters. The method fetches the `UAnimationAsset` from the name of the animation before continuing building upon the `TestCommandBuilder`

* 使用 Do 执行指定输入命令。
* 使用 Until 等待预期动画开始播放，因为动画状态可能跨多个 Tick 才更新。

* Do, the specified input command
* Wait until the expected animation is playing as this can occur over multiple ticks

#### WeaponMeleeAnimationTest（武器近战动画测试）

#### WeaponMeleeAnimationTest

**继续阅读前，请先查看上方[动画测试前置知识](#animation-test-prerequisites)。本节使用了自定义的基础 **CQTest** 实现，直接阅读可能产生混淆。**

**Please see the section above about [Animation Test Prerequisites](#animation-test-prerequisites) before going deeper into this section as there are custom implementations of base **CQTest** functionality that may cause confusion.**

**WeaponMeleeAnimationTest** 由 `ACTOR_ANIMATION_TEST_WITH_FLAGS` 宏创建，实现位于 `/ShooterTests/Source/ShooterTestsRuntime/Private/ShooterTestsActorAnimationTests.cpp`。测试验证玩家装备不同武器后执行近战输入会播放对应动画。

The **WeaponMeleeAnimationTest** is a test object created from the macro `ACTOR_ANIMATION_TEST_WITH_FLAGS` and the implementation can be found in `/ShooterTests/Source/ShooterTestsRuntime/Private/ShooterTestsActorAnimationTests.cpp`. These test that the expected animations are played during certain movement actions while the player has certain weapons equipped. 

实现分解如下：

The breakdown of the implementation is as follows:

测试使用 `ACTOR_ANIMATION_TEST_WITH_FLAGS` 宏、分类参数以及 `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` 创建，因此只在 Editor 中运行。宏以 `ShooterTestsActorAnimationTest` 为基础，而它又派生自 `ShooterTestsActorBaseTest`，所以测试可复用玩家、地图、输入和动画准备，`BEFORE_EACH` 会处理 `PlayerController` 等公共依赖。

We create our test object with the `ACTOR_ANIMATION_TEST_WITH_FLAGS` macro and the parameters `InputCrouchAnimationTest`, `"Project.Functional Tests.ShooterTests.Actor.Animation"`, and we specify the flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`. As mentioned in the [CQTest Prerequisites](#cqtest-prerequisites), the macro creates our test object with a `TTestRunner` and an instance of the `InputCrouchAnimationTest`. Because the macro uses `ShooterTestsActorAnimationTest` as our base object, which is also derived from `ShooterTestsActorBaseTest`, we get access to all of the member variables and methods provided by these objects. This will allow us to create our `TEST_METHOD` without the need to setup the `PlayerController` or any other components as the `BEFORE_EACH` will handle that for us. Because these tests are also spawning items to be used, we specify the flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` to only run these tests within the Editor context.

**WeaponMeleeAnimationTest** 额外使用以下变量：

The **WeaponMeleeAnimationTest** object has additional variables specified to be use.

* `FCQTestBlueprintHelper` 用于查找创建 `ALyraWeaponSpawner* WeaponSpawnerPad` 所需的 Blueprint。
* `WeaponSpawnerPad` 为 `Pawn` 生成可拾取和装备的武器，随后测试执行近战 `Input Action` 并验证动画。

* `FCQTestBlueprintHelper` will assist with finding Blueprints for the objects which be used to create our `ALyraWeaponSpawner* WeaponSpawnerPad`.
* `WeaponSpawnerPad` will create the weapons for the `Pawn` to pickup and equip for the tests to then perform a melee `Input Action` to trigger an animation to play and be tested against.

`WeaponMeleeAnimationTest` 构造函数通过初始化列表调用基础对象构造函数，传入待加载地图的目录和名称。`ShooterTestsActorBaseTest` 会创建 `FMapTestSpawner`，并在 `ShooterTestsActorBaseTest::Setup()` 中负责加载地图。

The `WeaponMeleeAnimationTest` constructor is defined as a way to call the base object's constructor with an initializer list to provide the directory where the map to load is located and the name of the map to load. This is due to the fact that the `ShooterTestsActorBaseTest` will create an instance of a `FMapTestSpawner`. This object handles loading of maps whenever `ShooterTestsActorBaseTest::Setup()` is called.

即使当前测试没有显式声明自己的 `BEFORE_EACH`，基础对象仍会通过宏生成的 `virtual void Setup() override` 准备关卡。`ShooterTestsActorAnimationTest::Setup()` 会继续调用 `ShooterTestsActorBaseTest::Setup()`，加载构造函数中指定的地图并等待关卡就绪。

Even though we don't specify it, the `BEFORE_EACH` of our base object goes through and prepares the level for the test as the macro has defined a `virtual void Setup() override` which is what allows the call to `ShooterTestsActorAnimationTest::Setup();` to be made. `ShooterTestsActorAnimationTest::Setup();` actually calls the base object's `Setup`, which in this case will be `ShooterTestsActorBaseTest`. What the setup will then do is start loading the map that was specified in the Constructor and waits until the level has been loaded.

**请注意，关卡已标记为可用时，其中的实际资产仍可能尚未完全加载。**

**Note that while the level has been loaded and marked for use, the actual assets may not have fully populated within the level yet.**

随后使用 `TestCommandBuilder` 构建测试开始前的关卡准备步骤：

The `TestCommandBuilder` is then used to build the steps needed to ensure that the level is setup prior to the test.

* 等待地图初始化并找到玩家 `Pawn`。默认超时为 10 秒，但为覆盖 Loading Screen 的额外加载，将其提高到 30 秒。
* `Pawn` 加载后调用 `ShooterTestsActorAnimationTest::PreparePlayerPawn()`，该方法也会调用 `ShooterTestsActorBaseTest::PreparePlayerPawn()`。
  * `ShooterTestsActorBaseTest::PreparePlayerPawn()` 准备主玩家及后续判断完全生成所需的玩家功能。
  * `ShooterTestsActorAnimationTest::PreparePlayerPawn()` 准备动画测试所需功能。
* 等待玩家被判定为完全生成。玩家生成时存在会切换输入处理的 `GameplayEffect`，而动画测试需要输入，因此必须等待该效果结束。

* Wait before starting until the map has been initialized and a player `Pawn` has been found for use. This step defaults to a timeout of 10 seconds, but we increase the timeout to 30 seconds to take into account additional loading done via the loading screen.
* Then, with the `Pawn` loaded a call to `ShooterTestsActorAnimationTest::PreparePlayerPawn()` is made, which also calls `ShooterTestsActorBaseTest::PreparePlayerPawn()`
  * `ShooterTestsActorBaseTest::PreparePlayerPawn()` handles setting up the main player and functionality tied to the player which will be used in a later step when determining if the player has fully spawned in.
  * `ShooterTestsActorAnimationTest::PreparePlayerPawn()` handles setting up functionality tied to testing animations.
* With the functionality now setup, we wait until the player is deemed as being fully spawned. This is done because there is a `GameplayEffect` tied to the player which is triggered on spawn. This effect toggles the input handling which could impact tests as we need input to trigger our animations.

各 `TEST_METHOD` 流程相同，但会装备三种不同武器并验证各自近战动画。测试调用 `EquipSpawnedWeapon`，再由 `SpawnWeaponSpawnerPad` 使用 `FCQTestBlueprintHelper::GetBlueprintClass` 查找 `ALyraWeaponSpawner` 类型，并用 `FCQTestBlueprintHelper::FindDataBlueprint` 取得 `ULyraWeaponPickupDefinition`。最后通过 `TObjectBuilder` 在玩家 `Pawn` 下方生成设置了目标 `WeaponDefinition` 的武器生成器。

Each `TEST_METHOD`, while tests the same flow, tests with different weapons equipped to make sure that the correct animation is playing. We are testing against 3 distinct weapons each with their own distinct melee animation. Each test will call the `EquipSpawnedWeapon` which takes in 3 parameters, the directory where the asset can be found, the name of the asset which stores the data to be loaded, and the name of the weapon once created and equipped. The method will call the `SpawnWeaponSpawnerPad` method, which takes in the first 2 of the supplied parameters, to use the `FCQTestBlueprintHelper::GetBlueprintClass` to find our `UClass` of the `ALyraWeaponSpawner` to be spawned. We also call `FCQTestBlueprintHelper::FindDataBlueprint` to get our `ULyraWeaponPickupDefinition` of the weapon instance that will be supplied as a parameter for the `ALyraWeaponSpawner`. With the objects found from `FCQTestBlueprintHelper`, the `TObjectBuilder` is then used to create an instance of the `ALyraWeaponSpawner` to be spawned right under the player `Pawn` with the `WeaponDefinition` parameter set to spawn the desired weapon specified by the test.

`TestCommandBuilder` 随后等待玩家装备预期武器。检查通过 `ULyraEquipmentManagerComponent` 取得第一个 `ULyraWeaponInstance`，确认实例有效、具有 Instigator 且类名符合预期，避免使用错误武器进行验证。装备正确后，测试注入近战 `Input Action` 并等待对应动画 Montage 播放。

Using the `TestCommandBuilder`, we build our steps to start testing that the expected weapon is equipped by the player `Pawn`. This is done by checking the `ULyraEquipmentManagerComponent` against the first `ULyraWeaponInstance`, which will be our currently equipped weapon. We also make sure that the equipped weapon matches the expected name of the weapon instance so that we're not testing against a weapon that we're not expecting and that the functionality of equipping a newly spawned weapon is expected. Once we have made sure that the expected weapon is equipped, we can then add the steps to the steps to the `TestCommandBuilder` to perform our melee `Input Action` before waiting to make sure that the correct animation montage was played.

#### 复制测试前置知识

#### Replication Test Prerequisites

使用 `ACTOR_ANIMATION_NETWORK_TEST` 宏时，需要指定 `ShooterTestsActorAnimationNetworkTest` 为基类。网络复制测试与单机动画测试采用类似层级：`ShooterTestsActorAnimationNetworkTest` 派生自基础 `ShooterTestsActorNetworkTest`。两者使用 `FShooterTestsNetworkComponent` 和 `FShooterTestsNetworkState` 分别保存服务端与客户端状态。由于状态需要访问 Actor 测试能力，网络组件和状态都以被测 Actor 辅助器类型为模板参数。

To use the newly defined `ACTOR_ANIMATION_NETWORK_TEST` macro, we need to specify `ShooterTestsActorAnimationNetworkTest` as the base class. Note that the network replication tests follow a similar setup to the stand-alone animation tests above where there is a base `ShooterTestsActorNetworkTest` that `ShooterTestsActorAnimationNetworkTest` derives from. The way the macro is used for the network tests is slightly different as we have more than just a single player to work with. Both the `ShooterTestsActorAnimationNetworkTest` and the base `ShooterTestsActorNetworkTest` make use of a `FShooterTestsNetworkComponent` which also handles a `FShooterTestsNetworkState` to keep track of both the server and client states. Because the `FShooterTestsNetworkState` requires knowledge of the Actors, instead of the Actor information being tied to the test, reference the [stand-alone animation tests](#animation-test-prerequisites) above. Both the `FShooterTestsNetworkComponent` and `FShooterTestsNetworkState` are templated to take in the Actor used for the test.

**FShooterTestsNetworkState**

`FShooterTestsNetworkState` 管理某个服务端或客户端 PIE 会话的状态。服务端和客户端即使加载同一地图，也各自拥有不同 `UWorld`。服务端世界为所有玩家持有有效 `APlayerController`；客户端世界只有本地 `ULocalPlayer` 的 Controller，其他 `ALyraCharacter` 是从服务端复制移动和动作的 `APawn`。状态对象直接保存本地玩家和对端网络玩家，并通过 `FShooterTestsActorTestHelper` 访问常用 Actor 功能，避免每次从 `UWorld` 重新遍历。

The `FShooterTestsNetworkState` struct is used to manage both the server and client state in relation to their PIE session. Currently this includes a `UWorld` for the PIE session as both the server and client will have different `UWorld` instances even though it will be the same map. The server `UWorld` will have a valid `APlayerController` for all players, regardless of whether they are local or network connected. On the client `UWorld`, there will only be a single `APlayerController` and that is for the `ULocalPlayer`. The other `ALyraCharacter` instances are `APawn` which get their movement and actions replicated from the server. The `FShooterTestsNetworkState` also keeps a pointer to both the local player and the network connected player for easy access needed for testing, instead of traversing through the `UWorld` of each. The local and network players make use of a `FShooterTestsActorTestHelper` which is an Actor helper object.

**FShooterTestsNetworkComponent**

`FShooterTestsNetworkComponent` 创建并连接 PIE 会话，也负责为服务端和客户端排队 Latent Command。它在 `TestCommandBuilder` 的 `Then`/`Until` 基础上提供 `ThenServer`、`UntilServer`、`ThenClient` 和 `UntilClient`。组件模板参数必须是 `FShooterTestsActorTestHelper` 或其派生类型。使用前必须调用 `Start`，以创建会话、配置网络并等待两端玩家完成生成。

The `FShooterTestsNetworkComponent` is the main component which handles the creation of the PIE sessions and the network between them. The component also handles queuing of latent commands for both the server and client PIE sessions. Similar to how the `TestCommandBuilder` has steps for `Then` and `Until`, the `FShooterTestsNetworkComponent` extends those steps and makes them available for both server, `ThenServer` and `UntilServer`, and the client, `ThenClient` and `UntilClient`. This component is templated and requires a type of `FShooterTestsActorTestHelper` to be provided so that both the component and `FShooterTestsNetworkState` can then use the Actor functionality needed for testing. The final step before being able to use the component is to call `Start` as this will then trigger the creation of the server and client PIE sessions, before configuring the network and making sure that both the server and client players are fully spawned into each session.

**FShooterTestsActorTestHelper**

该辅助器包装 `ALyraCharacter` 并提供网络测试常用功能，包括取得 `USkeletalMeshComponent` 和 `ULyraAbilitySystemComponent`、判断玩家是否完成生成，以及支持动画状态检查。它与单机动画测试使用的逻辑一致，但网络测试同时操作多个玩家，因此将其整理为共享对象。

As mentioned in both the `FShooterTestsNetworkState` and `FShooterTestsNetworkComponent`, this is the helper object which wraps around the `ALyraCharacter` to provide commonly used functionality for testing. The wrapped object handles fetching the `USkeletalMeshComponent` as well as the `ULyraAbilitySystemComponent` which can then be used to check when the player is fully spawned into the level or to determine if an animation is currently playing. The underlying functionality is no different than what is being performed in the [InputCrouchAnimationTest](#inputcrouchanimationtest) above, but it made sense to consolidate this information into a shared object as the networking replication tests work on multiple players.

**FShooterTestsActorInputTestHelper**

该对象派生自 `FShooterTestsActorTestHelper`，既保留 `ALyraCharacter` 基础能力，又通过 `FShooterTestsPawnTestActions` 向玩家注入输入。为了简化测试调用，它使用 `enum class` 映射可执行的输入类型。

Object derived from the above `FShooterTestsActorTestHelper` to get all of the main `ALyraCharacter` functionality, but also makes use of `FShooterTestsPawnTestActions` to perform input actions on the player. To make performing input for the tests easier, there is an `enum class` which maps to the type of input that is performed.

#### InputAnimationTest（输入动画复制测试）

#### InputAnimationTest

**继续阅读前，请先查看上方[复制测试前置知识](#replication-test-prerequisites)。本节使用了自定义的基础 **CQTest** 实现，直接阅读可能产生混淆。**

**Please see the section above about [Replication Test Prerequisites](#replication-test-prerequisites) before going deeper into this section as there are custom implementations of base **CQTest** functionality that may cause confusion.**

**InputAnimationTest** 由 `ACTOR_ANIMATION_NETWORK_TEST` 宏创建，实现位于 `/ShooterTests/Source/ShooterTestsRuntime/Private/ShooterTestsActorNetworkTests.cpp`。测试验证特定移动输入会播放预期动画，并且动画状态会正确复制到网络另一端。

The **InputAnimationTest** is a test object created from the macro `ACTOR_ANIMATION_NETWORK_TEST` and the implementation can be found in `/ShooterTests/Source/ShooterTestsRuntime/Private/ShooterTestsActorNetworkTests.cpp`. These test that the expected animations are played during certain movement actions and replicated properly across the network. 

实现分解如下：

The breakdown of the implementation is as follows:

测试使用 `ACTOR_ANIMATION_NETWORK_TEST` 宏及参数 `InputAnimationTest` 和 `"Project.Functional Tests.ShooterTests.Actor.Replication"` 创建。宏生成 `TTestRunner` 和 `InputAnimationTest` 实例，并以 `ShooterTestsActorAnimationNetworkTest` 为基础对象；后者又派生自 `ShooterTestsActorNetworkTest`。因此 `virtual void Setup() override` 会准备网络世界、Controller 和玩家，`TEST_METHOD` 无需重复初始化。

We create our test object with the `ACTOR_ANIMATION_NETWORK_TEST` macro and the parameters `InputAnimationTest` and `"Project.Functional Tests.ShooterTests.Actor.Replication"`. As mentioned in the [CQTest Prerequisites](#cqtest-prerequisites), the macro creates our test object with a `TTestRunner` and an instance of the `InputAnimationTest`. Because the macro uses `ShooterTestsActorAnimationNetworkTest` as our base object, which is also derived from `ShooterTestsActorNetworkTest`, we get access to all of the member variables and methods provided by these objects. This will allow us to create our `TEST_METHOD` without the need to setup the `PlayerController` or any other components as the `virtual void Setup() override` will handle that for us.

`InputAnimationTest` 构造函数通过初始化列表把地图包路径交给基础对象。网络测试不使用 `FMapTestSpawner` 查找玩家，因为 `FShooterTestsNetworkComponent` 会创建 PIE 会话，并把服务端/客户端世界写入各自的 `FShooterTestsNetworkState`。

The `InputAnimationTest` constructor is defined as a way to call the base object's constructor with an initializer list to provide the package path of the map to be loaded. This is due to the fact that the `ShooterTestsActorNetworkTest` handles loading of the map. This differs from the animation tests as those tests make use of the `FMapTestSpawner` to not only load the map, but also find our player character. The `FMapTestSpawner` has little use here as the `FShooterTestsNetworkComponent` creates our PIE sessions and assigns the server and client worlds to the `FShooterTestsNetworkState`.

`ShooterTestsBaseActorNetworkTest::Setup()` 会初始化 `FShooterTestsNetworkComponent<NetworkActorType>`，并从服务端和客户端 `UWorld` 取得玩家。`ShooterTestsActorAnimationNetworkTest` 把 `FShooterTestsActorInputTestHelper` 作为 `NetworkActorType`，满足其必须派生自 `FShooterTestsActorTestHelper` 的约束。只有两端状态都拥有世界，且服务端与客户端玩家在所有 PIE 实例中完成加载和生成后，测试才可继续。

During the `ShooterTestsBaseActorNetworkTest::Setup()` is where the `FShooterTestsNetworkComponent<NetworkActorType> Network` is initialized and fetches players from both the server and client `UWorld`. As mentioned above, the `NetworkActorType` needs to be of type `FShooterTestsActorTestHelper` which is specified in the `ShooterTestsActorAnimationNetworkTest` definition when we provide the `FShooterTestsActorInputTestHelper` to our `Network` object. The tests will be ready once the `Network` object has initialized both the server and client states with their `UWorld` and both server and client players are loaded and spawned in all running PIE instances.

**该过程需要在多个 PIE 实例中完整加载世界和资产，连接客户端越多耗时越长，因此测试只使用两个会话：一个 Listen Server 和一个独立客户端。**

**Note that this process will take some time as both the world and assets need to be fully loaded across multiple PIE instances. This time increases as more clients connect which is why it was decided to limit clients to just 2.**

上述准备通过 `TestCommandBuilder` 组织以下步骤：

The above is accomplished by using the `TestCommandBuilder` with steps needed to ensure that the level and players are setup and ready prior to the test.

* 创建 `PlaySettings`，设置 PIE 会话数量和网络选项。
* 等待所有 PIE 会话创建完成，并把正确的 `UWorld` 分配给服务端和客户端状态。
  * 每个 `UWorld` 都应拥有 NetDriver。
  * 服务端 `UWorld` 的 NetDriver 应标记为 Server。
  * Lyra 使用 Loading Screen 隐藏 PIE 初始化，因此该步骤使用更长超时。
* 等待服务端和客户端成功建立连接。
  * 服务端按预期客户端数量检查连接数。
  * 客户端需要有效 `ViewTarget`，表明已能查看关卡或 PlayerController 已被确认。
* 将网络组件标记为运行中。
* 添加 TearDown 步骤，在测试结束时关闭 PIE 会话。

* Create the `PlaySettings` which will set the number of clients as well as our settings for the PIE sessions.
* Wait until all PIE sessions have been created and we are able to set both the server and client states with the proper `UWorld`.
  * All `UWorld` objects should have been created with a network driver.
  * The server `UWorld` will have its network driver marked as being the server.
  * We go through this step with an increased timeout duration as Lyra uses a loading screen to hide all of the PIE initialization.
* Wait until the server and clients have successfully connected to each other.
  * The server will have information about the number of clients currently connected which is checked against our expected client count.
  * The client will have a valid `ViewTarget` which tells us that the client is able to view the loaded level or that a PlayerController has been acknowledged.
* Then, mark the network component as actively running.
* Add a tear down step to end the PIE sessions when the test is done

完成 `Network.Start` 提供的步骤后，服务端和客户端 PIE 会话及各自关卡已经初始化。`ShooterTestsBaseActorNetworkTest::Setup()` 仍需取得两端玩家并确认其在 `UWorld` 中完全生成，因为每个 `TEST_METHOD` 都依赖两名玩家就绪。

At this point we have finished the steps that were provided `Start` of the `Network` object. At this point the server and client PIE sessions have been initialized and the level for each should be loaded. We still need to continue the `ShooterTestsBaseActorNetworkTest::Setup()` and fetch our players for the server and client and make sure that they are fully spawned in the `UWorld`. The steps here will get the players since each `TEST_METHOD` requires both the server and client players fully spawned to continue.

* 在服务端等待找到拥有者 PlayerController。
* 将该 Controller 对应玩家写入服务端状态的 LocalPlayer。
* 等待服务端玩家结束生成效果并标记为就绪。
* 在客户端等待找到拥有者 PlayerController。
* 将该 Controller 对应玩家写入客户端状态的 LocalPlayer。
* 等待客户端玩家结束生成效果并标记为就绪。

* On the server, wait until an owning PlayerController has been found
* Then, set the owning player controller as the local player in the server state
* Wait until the server player has finished playing their spawning effect to mark them as being fully spawned and ready
* On the client, wait until an owning PlayerController has been found
* Then, set the owning player controller as the local player in the client state
* Wait until the client player has finished playing their spawning effect to mark them as being fully spawned and ready

上述步骤使服务端和客户端本地玩家在各自 PIE 实例中就绪。接下来还需查找每个实例中的对端复制玩家，确认两端彼此连接且玩家均已生成，然后才能开始测试。

The above steps get both of the server and client player loaded and spawned within their PIE instance. Next we want to repeat the above steps but for each instances connecting player. This way we know that both the server and client are connected to each other and fully spawned so that we can begin our test.

* 在服务端遍历 `UWorld` 玩家，找到连接到本会话的客户端玩家。
* 在客户端重复同样步骤，找到服务端玩家。

* On the server, iterate through all of our `UWorld` players and find the player connected to us
* Repeat the above step, but on the client

完成 `ShooterTestsBaseActorNetworkTest::Setup()` 后即可执行 `InputAnimationTest` 的 `TEST_METHOD`。每个用例都通过 `FShooterTestsNetworkComponent` 在 `TestCommandBuilder` 中建立 Latent Command，并明确使用服务端或客户端状态执行。

With the `ShooterTestsBaseActorNetworkTest::Setup()` completed, the `TEST_METHOD` for the `InputAnimationTest` can be performed. Each `TEST_METHOD` will follow a similar process of using the `FShooterTestsNetworkComponent` to setup latent commands via the `TestCommandBuilder` to be executed using either the server or client state.

#### AbilitySpawnerMapTest（Gameplay Ability 生成器地图测试）

#### AbilitySpawnerMapTest

**AbilitySpawnerMapTest** 由 `TEST_CLASS_WITH_FLAGS` 宏创建，实现位于 `/ShooterTests/Source/ShooterTestsRuntime/Private/ShooterTestsMapTests.cpp`。测试验证 GameplayEffect 会按预期作用于玩家 `Pawn`。

The **AbilitySpawnerMapTest** is a test object created from the macro `TEST_CLASS_WITH_FLAGS` and the implementation can be found in `/ShooterTests/Source/ShooterTestsRuntime/Private/ShooterTestsMapTests.cpp`. These test gameplay behavior is appropriately applied to the player `Pawn`.

实现分解如下：

The breakdown of the implementation is as follows:

测试使用 `TEST_CLASS_WITH_FLAGS` 宏以及 `AbilitySpawnerMapTest`、`"Project.Functional Tests.ShooterTests.GameplayAbility"` 和 `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` 参数创建。由于测试会生成依赖本地资产的对象，因此限制在 EditorContext 中运行。

We create our test object with the `TEST_CLASS_WITH_FLAGS` macro and the parameters `AbilitySpawnerMapTest`, `"Project.Functional Tests.ShooterTests.GameplayAbility"`, and we specify the flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`. Because these tests are also spawning items to be used, we specify the flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` to only run these tests within the Editor context.

**AbilitySpawnerMapTest** 使用以下附加变量：

The **AbilitySpawnerMapTest** object has additional variables specified to be use.

* `FMapTestSpawner` 创建玩家 `Pawn` 和其他测试对象所在的测试世界。
* `FCQTestBlueprintHelper` 帮助查找创建 `AActor* GameplayEffectPad` 所需的 Blueprint。
* `ALyraCharacter* Player` 是承受 `GameplayEffect` 的世界内玩家 `Pawn`。
* `ULyraAbilitySystemComponent* AbilitySystemComponent` 保存并处理玩家的 Ability System 状态。
* `const ULyraHealthSet* HealthSet` 保存玩家生命属性。

* `FMapTestSpawner` will spawn the test world for the player `Pawn` and other objects to be spawned into.
* `FCQTestBlueprintHelper` will assist with finding Blueprints for the objects which be used to create our `AActor* GameplayEffectPad`.
* `ALyraCharacter* Player` is the player `Pawn` in the world that the `GameplayEffect` will be applied to.
* `ULyraAbilitySystemComponent* AbilitySystemComponent` is where the information about the `Player` is stored and handled by.
* `const ULyraHealthSet* HealthSet` is where the information about the `Player` health is stored.

除变量外，测试还实现了一些辅助方法。`DoDamageToPlayer` 接受 `double` 伤害值，通过 `ULyraAssetManager` 取得项目 GameData 中的伤害 GameplayEffect，验证 SpecHandle 后由 `AbilitySystemComponent` 写入伤害数值并应用效果。

In addition to variables, there are also some methods implemented to help with our tests. `DoDamageToPlayer` is a method that takes in a `double` parameter used to apply as our `Player` damage. The method uses the `ULyraAssetManager` to then get the game data for the damage GameplayEffect and makes sure that the handle to the effect is valid. With a valid handle the call can then be made by the `AbilitySystemComponent` to specify the effect and the amount of damage to be applied.

`SpawnGameplayPad` 接受 GameplayEffect 名称，使用 `FCQTestBlueprintHelper::GetBlueprintClass` 分别查找目标 Effect 和 GameplayEffect Pad 类型，再通过 `TObjectBuilder` 生成配置了目标效果的 Pad Actor。

The `SpawnGameplayPad` method takes a parameter for the name of the GameplayEffect. The method calls into `FCQTestBlueprintHelper::GetBlueprintClass` to look at the GameplayDirectory for the specified effect name and returns a `UClass` of the desired effect. Another call to `FCQTestBlueprintHelper::GetBlueprintClass` is done, but this time we are specifically looking for the `UClass` of the GameplayEffect pad. Using the `TObjectBuilder`, we spawn an `AActor` of the GameplayEffect pad with the class of the desired effect to spawn with.

最后，`IsPlayerDamaged` 仅检查当前生命是否低于最大生命。

The final method, `IsPlayerDamaged`, just checks to see if the current health is less than the max allowed health.

有了这些变量和方法后，`BEFORE_EACH` 使用 `FMapTestSpawner` 指定并加载测试关卡。关卡加载后，`TestCommandBuilder` 继续组织开始测试前的准备步骤。

With the variables and methods supplied by this object, we can now start going into the methods which will describe the flow of the tests. First is the `BEFORE_EACH` which goes through and prepares the level for the test by using the `FMapTestSpawner` to specify our level location and name before waiting until the level has been loaded. Once the level is loaded the `TestCommandBuilder` is then used to build the steps needed to ensure that the level is setup prior to the test.

* 等待找到有效玩家 `Pawn`。默认超时为 10 秒，但 **Lyra** Loading Screen 还会加载其他对象，因此提高到 30 秒。
* 使用 Do 保存 `FMapTestSpawner::FindFirstPlayerPawn` 返回的 `Player`，并验证 `AbilitySystemComponent` 有效且 `HealthSet` 处于满生命。

* Start when we are able to find a valid player `Pawn`. The default timeout is 10 seconds, but **Lyra** uses a loading screen to load additional objects. Because of this we increase the timeout to 30 seconds.
* Do, set the `Player` found from `FMapTestSpawner::FindFirstPlayerPawn`. We also make sure that the `AbilitySystemComponent` is valid and that `HealthSet` is set to the max possible health.

完成 `BEFORE_EACH` 后，可运行以下 `TEST_METHOD`：

After the `BEFORE_EACH`, we then have `TEST_METHOD` that can be run:

**PlayerOnDamageSpawner_Eventually_LosesHealth**

使用 `TestCommandBuilder` 验证生成伤害 GameplayEffect 后玩家生命下降。

Uses the `TestCommandBuilder` to build steps that will check that the `Player` loses health when the damage GameplayEffect is spawned.

* 等待 `AbilitySystemComponent` 不再具有 `TAG_Gameplay_DamageImmunity`。
* 调用 `SpawnGameplayPad` 生成带伤害 GameplayEffect 的 GameplayPad。
* 持续运行直到 `IsPlayerDamaged` 表明玩家已受到伤害。

* Start when the `AbilitySystemComponent` can find the gameplay tag for `TAG_Gameplay_DamageImmunity`
* Then, call the method `SpawnGameplayPad` to spawn our GameplayPad with the damage GameplayEffect.
* Run until we see that the player has been damaged by the effect, `IsPlayerDamaged`.

**PlayerMissingHealth_OnHealSpawner_RestoresHealth**

使用 `TestCommandBuilder` 验证玩家先受伤，再由治疗 GameplayEffect 恢复生命。

Uses the `TestCommandBuilder` to build steps that will check that the `Player` loses health when the heal GameplayEffect is spawned.

* 等待 `AbilitySystemComponent` 不再具有 `TAG_Gameplay_DamageImmunity`。
* 调用 `DoDamageToPlayer` 直接对 `Player` 造成 10 点伤害。
* 持续运行直到 `IsPlayerDamaged` 表明玩家已受到伤害。
* 调用 `SpawnGameplayPad` 生成带治疗 GameplayEffect 的 GameplayPad。
* 持续运行直到 `!IsPlayerDamaged` 表明玩家已恢复。

* Start when the `AbilitySystemComponent` can find the gameplay tag for `TAG_Gameplay_DamageImmunity`
* Then, call the method `DoDamageToPlayer` to apply 10 points of damage directly to our `Player`.
* Run until we see that the player has been damaged by the effect, `IsPlayerDamaged`.
* Then, call the method `SpawnGameplayPad` to spawn our GameplayPad with the healing GameplayEffect.
* Run until we see that the player has been healed by the effect by checking that our player has not been damaged, `!IsPlayerDamaged`.

### 蓝图功能测试

### Blueprint Functional Tests

**Shooter Tests** 插件包含若干蓝图功能测试，位于 `/GameFeatures/ShooterTests/Content/Blueprint`。可在 Blueprint Editor 中查看测试结构和所用节点。在 **Session Frontend** 的 **Automation** 标签页中，测试按关卡名称显示；例如 [B_Test_AutoRun](#b_test_autorun) 位于 `L_ShooterTest_Autorun` 下，点击测试会打开该关卡，除非 Editor 已经打开它。当前蓝图功能测试包括：

The **Shooter Tests** plugin has a few Blueprint functional tests which can be found in `/GameFeatures/ShooterTests/Content/Blueprint`. These tests can be viewed within the Blueprint Editor to help get a better understanding of how the tests are setup and what nodes they are using to accomplish testing the functionality. Please note that when viewing these tests from the **Automation** tab of the **Session Frontend**, the Blueprint Functional Test will reside under the name of the Level. For example, the test [B_Test_AutoRun](#b_test_autorun) will be located under the level name of `L_ShooterTest_Autorun` and clicking on the test itself will open the level unless the Editor already has the level opened. Some of the tests implemented using a Blueprint Functional Test Actor:

* [B_Test_AutoRun](#b_test_autorun)
* [B_Test_FireWeapon](#b_test_fireweapon)

关于 Blueprint Functional Test Actor 和测试创建入门，请参阅[蓝图功能测试在线文档](https://dev.epicgames.com/documentation/en-us/unreal-engine/functional-testing-in-unreal-engine)。

Please see [the online documentation on Blueprint Functional Tests](https://dev.epicgames.com/documentation/en-us/unreal-engine/functional-testing-in-unreal-engine) for more information about how to get get started with creating tests of what a Blueprint Functional Test Actor is.

#### B_Test_AutoRun（自动奔跑测试）

#### B_Test_AutoRun

**B_Test_AutoRun** 蓝图功能测试验证 **Hero** 的 `Lyra Player Controller` 能启用自动奔跑 `Input Action`。在 Editor 中打开该蓝图可查看以下做法：

The **B_Test_AutoRun** Blueprint Functional Test is setup to check that the **Hero** `Lyra Player Controller` can have the `Input Action` for auto run enabled. Opening up and viewing this Blueprint within the Editor will help show the following:

* 将 `Event Start Test` 设置为主要测试入口。
* 使用断言节点验证条件和行为。
* 创建并触发自定义事件。
* 使用 Delay 让其他 Latent Command 执行。
* 设置和读取私有 Blueprint Function 变量。
* 使用 Blueprint Component（**Target Point**）。
* 通过本地 PlayerController 注入输入。
* 在每个 `Event Tick` 检查测试成功条件。
* 检查 Actor 之间的重叠。

* How to setup the `Event Start Test` event as the main test entry point
* Using assertion nodes to validate conditions and behaviors
* Creating and triggering of custom events
* Use of Delays to allow other latent commands to execute
* Setting and getting of private Blueprint Function Variables
* Working with Blueprint Components (**Target Point**)
* Working with the local PlayerController to inject input
* Performing checks on every `Event Tick` to validate a successful test condition
* Using functionality to check overlaps between actors

#### B_Test_FireWeapon（武器射击测试）

#### B_Test_FireWeapon

**B_Test_FireWeapon** 蓝图功能测试先确认 **Hero** 的初始武器拥有弹药，再执行射击，并通过弹药数量下降验证武器确实开火。在 Editor 中打开该蓝图可查看以下做法：

The **B_Test_FireWeapon** Blueprint Functional Test is setup to check that the **Hero** has ammo for their starting weapon and upon firing, validates that the weapon has successfully fired by checking if the ammo count has decreased. Opening up and viewing this Blueprint within the Editor will help show the following:

* 将 `Event Start Test` 设置为主要测试入口。
* 使用 Blueprint Macro 复用逻辑。
* 使用断言节点验证条件和行为。
* 创建并触发自定义事件。
* 使用 Delay 让其他 Latent Command 执行。
* 通过本地 PlayerController 注入输入。

* How to setup the `Event Start Test` event as the main test entry point
* Using Blueprint Macros for reusability
* Using assertion nodes to validate conditions and behaviors
* Creating and triggering of custom events
* Use of Delays to allow other latent commands to execute
* Working with the local PlayerController to inject input

## Lyra 自动化测试故障排查

## Troubleshooting Lyra automated tests

本节列出在 **Lyra** 项目中创建自动化测试时发现的一些常见问题。

This section mentions some common pitfalls discovered when creating automated tests within the **Lyra** project.

### 网络测试无法使用新地图

### Cannot use a new map for a network test

默认情况下，**Lyra** 要求通过 Front End 创建网络并指定待加载 Experience。多数测试希望直接使用目标关卡建立网络，而不模拟正常前端菜单流程。解决方法是在 Editor 中加载地图，在 `Outliner` 中右键 `UWorld` 并打开 `World Settings`。在面板中找到 `PIE` 类别，禁用 `Force Standalone Net Mode` 后保存 `UWorld`，即可绕过 **Lyra** Front End 创建网络。

By default **Lyra** requires a network to be created by going through the front-end and providing an experience to be loaded. For most testing scenarios we will want to have the network created with the level we want without the need to mimic going through the typical **Lyra** front-end menus. To adjust this, load the map within the Editor and right-click the `UWorld` within the `Outliner`. A context menu will appear where one of the options will be `World Settings`. Clicking on the `World Settings` option will open a `World Settings` panel. Within the panel search for the `PIE` category where there will be a checkbox enabled for `Force Standalone Net Mode`. Disabling this and saving the `UWorld` will allow for networks to be created without the need to go through the **Lyra** front-end.

### 每次测试随机生成角色导致不稳定

### Tests are flaky due to each test run spawning a random character

**Lyra** 会生成 Manny 和 Quinn 两种角色模型，默认随机选择。两者使用不同动画层，因此动画等测试可能不稳定。可按以下步骤强制加载特定角色，或采用确定性的角色选择：

There are 2 different character models that are spawned in a **Lyra** session, Manny and Quinn. By default these characters are spawned randomly and testing for certain functionality, such as animations, could cause flakiness as each character has a different animation layer associated with them. The following steps should be done in order to forcefully load a specific character, or to have character loading handled in a deterministic manner:

* 在 Content Browser 中导航到 `/Content/Characters/Cosmetics`。
  * 该目录除两种角色模型外，还包含 `B_PickRandomCharacter` 和 `B_CharacterSelection` 两个 Blueprint Class。
  * 默认由 `ALyraPlayerController` 调用 `B_PickRandomCharacter`，在生成时随机选择模型。
  * `B_CharacterSelection` 使用轮询方式确定加载顺序：先 `B_Manny`，再 `B_Quinn`，然后回到 `B_Manny`。
* 复制 `B_PickRandomCharacter` 或 `B_CharacterSelection`，或者以 `Lyra Controller Component Character Parts` 为父类创建新的 Blueprint Class。
* 修改新 Blueprint 的 `Event Graph` 以加载测试所需角色，完成后编译并保存。
* 在 Content Browser 中导航到 `/Plugins/ShooterTests Content/System/Experiences`。
  * 该目录包含 `B_BasicShooterTest` 和 `B_AutomatedShooterTest` 两个 Blueprint Class。
* 复制 `B_BasicShooterTest` 或 `B_AutomatedShooterTest`，或者以 `Lyra Experience Definition` 为父类创建新的 Blueprint Class。
* 在 `Class Defaults` 中导航到 `Actions` 类别，并展开 `Actions` 类别和其中的 `Actions` 项。
* 添加或展开 `Add Components` 元素。
* 使用上方创建的 Blueprint 添加或替换 `Engine.Controller` 脚本，然后编译并保存。
* 返回 Editor，打开该世界的 `World Settings`。
  * 可按上一节步骤访问 `World Settings` 面板。
* 在 `World Settings` 的 `Game Mode` 类别中找到 `Default Gameplay Experience`，选择新创建的 Experience Blueprint。

* In the Content Browser, navigate to `/Content/Characters/Cosmetics`
  * This directory not only has the 2 character models, but there you will find 2 Blueprint Classes, `B_PickRandomCharacter` and `B_CharacterSelection`
  * By default, `B_PickRandomCharacter` is invoked on the `ALyraPlayerController` to select a random model when spawning
  * `B_CharacterSelection` is more deterministic in that it loads the model using a round robin approach starting with `B_Manny`, then `B_Quinn`, before going back to `B_Manny`
* Either duplicate the `B_PickRandomCharacter` or `B_CharacterSelection` Blueprint or create a new Blueprint Class with the `Lyra Controller Component Character Parts` as the parent class
* Modify the `Event Graph` of the new blueprint to load the character you wish to test with and make sure to compile and save when done.
* Back in the Content Browser, navigate to `/Plugins/ShooterTests Content/System/Experiences`
  * This directory has 2 Blueprint Classes, `B_BasicShooterTest` and `B_AutomatedShooterTest`
* * Either duplicate the `B_BasicShooterTest` or `B_AutomatedShooterTest` Blueprint or create a new Blueprint Class with the `Lyra Experience Definition` as the parent class
* Under the `Class Defaults` section navigate to the `Actions` category and expand both the `Actions` category and the `Actions` item.
* Add or expand the `Add Components` element
* Add or replace the `Engine.Controller` script with the Blueprint that was created above before compiling and saving the Blueprint
* Go back to the Editor and open up the `World Settings` of the world
  * Follow the steps above on how to access the `World Settings` panel
* In the `World Settings` panel, search for the `Default Gameplay Experience` under the `Game Mode` category and select the newly created experience Blueprint

## 参考资源

## Resources

* [Unreal Engine 插件](https://dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine)
* [Lyra 示例](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)
* [Unreal Automation System 用户指南](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-user-guide-in-unreal-engine)
* [蓝图功能测试](https://dev.epicgames.com/documentation/en-us/unreal-engine/functional-testing-in-unreal-engine)
* [Blueprint Macro](https://dev.epicgames.com/documentation/en-us/unreal-engine/macros-in-unreal-engine)

* [Plugins in Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/plugins-in-unreal-engine)
* [Lyra Sample](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine)
* [Unreal Automation System User guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-system-user-guide-in-unreal-engine)
* [Blueprint Functional Tests](https://dev.epicgames.com/documentation/en-us/unreal-engine/functional-testing-in-unreal-engine)
* [Blueprint Macro](https://dev.epicgames.com/documentation/en-us/unreal-engine/macros-in-unreal-engine)
