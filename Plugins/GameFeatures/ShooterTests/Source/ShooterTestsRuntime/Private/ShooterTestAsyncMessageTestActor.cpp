// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShooterTestAsyncMessageTestActor.h"

#include "AudioThread.h"
#include "Async/Async.h"
#include "AsyncMessageBindingOptions.h"
#include "AsyncGameplayMessageSystem.h"
#include "AsyncMessageWorldSubsystem.h"
#include "AsyncMessageSystemLogs.h"
#include "Components/StaticMeshComponent.h"
#include "NativeGameplayTags.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ShooterTestAsyncMessageTestActor)

// 定义颜色、通用异步测试和高频压力测试使用的消息标签层级。
UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Red, "Change.Color.Red")
UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Green, "Change.Color.Green")
UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Blue, "Change.Color.Blue")
// 定义指定线程投递验证使用的通用异步测试标签。
UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Async_Test, "Async.Message.Test")
// 定义高频压力测试的父消息和子消息标签。
UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Test_Heavy, "Tests.HeavyMessage")
UE_DEFINE_GAMEPLAY_TAG(Message_Tag_Test_HeavySubmessage, "Tests.HeavyMessage.SubMessage")

// 将颜色 GameplayTag 包装为异步消息 ID，供广播器与监听器共享。
static FAsyncMessageId Message_ChangeColor_Red = { Message_Tag_Red };
static FAsyncMessageId Message_ChangeColor_Green = { Message_Tag_Green};
static FAsyncMessageId Message_ChangeColor_Blue = { Message_Tag_Blue };

// 指定线程投递测试使用的异步消息 ID。
static FAsyncMessageId Message_Async_Test = { Message_Tag_Async_Test };

///////////////////////////////////////////////////////////////
// AAsyncColorChangeBroadcastActor

// 构造颜色消息广播器，默认从红色消息 ID 开始广播。
AAsyncColorChangeBroadcastActor::AAsyncColorChangeBroadcastActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ColorMessageToBroadcast = Message_ChangeColor_Red;
}

// Actor 开始游戏时同时启动后台线程颜色消息和游戏线程 GameplayTag 消息定时器。
void AAsyncColorChangeBroadcastActor::BeginPlay()
{
	Super::BeginPlay();
	
	SpawnBackgroundTask();

	SpawnGameplayTagBroadcaster();
}

// 在线程池普通任务中用弱引用排队颜色 Payload，完成后递增计数，并按 ColorMessageBroadcastFrequency 安排下一次任务。
void AAsyncColorChangeBroadcastActor::SpawnBackgroundTask()
{	
	TWeakObjectPtr<AAsyncColorChangeBroadcastActor> WeakThis(this);

	// 从非游戏线程排队消息，本例使用任务池中的普通优先级线程。
	// Queue a message from a non-game thread, a normal thread from the task pool in this case
	AsyncTask(ENamedThreads::AnyNormalThreadNormalTask, [WeakThis]()
	{
		if (!WeakThis.IsValid())
		{ 
			return;
		}

		if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(WeakThis->GetWorld()))
		{
			// 将当前颜色结构体 Payload 排入共享消息系统等待广播。
			// Queue a color change message
			Sys.Get()->QueueMessageForBroadcast(WeakThis->ColorMessageToBroadcast, TStructView<FColorChangingMessage>(WeakThis->ColorChangeData));
		}

		WeakThis->NumBroadcasts++;
	});

	// 使用一次性 Timer 按配置频率再次生成后台任务。
	// Set up a timer to spawn a new background task every second
	GetWorld()->GetTimerManager().SetTimer(StartBackgroundTaskTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				SpawnBackgroundTask();
			}), 1.0f, false, ColorMessageBroadcastFrequency);
}

// 若任一配置标签有效，则在游戏线程按固定频率循环排队两个无 Payload 的 GameplayTag 消息。
void AAsyncColorChangeBroadcastActor::SpawnGameplayTagBroadcaster()
{
	if (!GameplayTagToBroadcastA.IsValid() && !GameplayTagToBroadcastB.IsValid())
	{
		return;
	}
	
	TWeakObjectPtr<UWorld> WeakWorld = GetWorld();
	TWeakObjectPtr<AAsyncColorChangeBroadcastActor> WeakThis = this;
	
	// 设置循环 Timer；其回调运行在游戏线程并直接排队 GameplayTag 消息。
	// Set up a timer to spawn a new background task. This will queue the message on the game thread
	GetWorld()->GetTimerManager().SetTimer(GameplayTagSpawner,
		FTimerDelegate::CreateWeakLambda(this, [WeakWorld, WeakThis]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			
			// 将所有有效的配置 GameplayTag 分别排入广播队列。
			// Queue the gameplay tag message a for broadcast
			if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(WeakWorld.Get()))
			{
				if (WeakThis->GameplayTagToBroadcastA.IsValid())
				{
					Sys->QueueMessageForBroadcast(WeakThis->GameplayTagToBroadcastA);	
				}

				if (WeakThis->GameplayTagToBroadcastB.IsValid())
				{
					Sys->QueueMessageForBroadcast(WeakThis->GameplayTagToBroadcastB);
				}
			}
		}), 1.0f, true, GameplayTagBroadcastFrequency);
}

///////////////////////////////////////////////////////////////
// AColorChangingTestListener

// 创建作为根组件的静态网格，并为每个材质槽创建动态实例，以便消息回调修改颜色参数。
AColorChangingTestListener::AColorChangingTestListener(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ColorChangingMesh"));
	SetRootComponent(MeshComp);
	
	// 颜色消息会覆盖网格材质参数，因此先为所有材质槽创建动态材质实例。
	// We will be overriding the materials on this mesh to change their color, so make some dynamic instances of the material
	for (int32 MatIdx = 0; MatIdx < MeshComp->GetNumMaterials(); ++MatIdx)
	{
		MeshComp->CreateDynamicMaterialInstance(MatIdx);
	}
}

// 绑定颜色父消息、可选的指定线程测试监听器，并随机选择首个具体颜色消息开始轮换监听。
void AColorChangingTestListener::BeginPlay()
{
	Super::BeginPlay();
	
	if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld()))
	{
		// 绑定 Change.Color 父消息 ID，从而接收所有颜色子消息。
		// Listen to ALL color messages
		BoundHandle_ColorParent = Sys->BindListener(
			Message_ChangeColor_Red.GetParentMessageId(),
			TWeakObjectPtr<AColorChangingTestListener>(this),
			&ThisClass::HandleAnyColorChange);
	}

	if (bShouldSpawnTaskOnOtherThread)
	{
		SetupNamedThreadListener();
	}
	
	PreviousColorBoundTo = FMath::RandRange(0, 2);
	
	SetupColorListener();
}

// Actor 结束游戏时解除具体颜色监听器和父消息监听器，避免消息系统保留失效回调。
void AColorChangingTestListener::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	ResetListenerToColor();

	if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld()))
	{
		Sys->UnbindListener(BoundHandle_ColorParent);
	}
}

// 选择覆盖 ID 或下一个轮换颜色 ID，并把 HandleColorChange 绑定到该具体消息。
void AColorChangingTestListener::SetupColorListener()
{
	if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld()))
	{
		const FAsyncMessageId MessageToListenFor = GetColorChangeToListenTo();

		FAsyncMessageBindingOptions Opts = {};

		// 绑定当前选择的具体颜色消息。
		// Listen to the color messages
		BoundHandle_Color = Sys->BindListener(
			MessageToListenFor,
			TWeakObjectPtr<AColorChangingTestListener>(this),
			&ThisClass::HandleColorChange,
			Opts);
	}
}

// 从共享消息系统解除当前具体颜色监听器，并把句柄重置为 Invalid。
void AColorChangingTestListener::ResetListenerToColor()
{
	if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld()))
	{
		Sys->UnbindListener(BoundHandle_Color);
		BoundHandle_Color = FAsyncMessageHandle::Invalid;
	}
}

// 把 Async.Message.Test 监听器指定到 RHI Thread，并定期在同一命名线程排队消息以验证线程路由。
void AColorChangingTestListener::SetupNamedThreadListener()
{
	check(bShouldSpawnTaskOnOtherThread);

	// 要求该测试消息的回调在 RHI Thread 上执行。
	// We want to receive our event on the RHI thread
	FAsyncMessageBindingOptions Opts = {};
	Opts.SetNamedThreads(ENamedThreads::RHIThread);

	if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld()))
	{
		Sys->BindListener(Message_Async_Test, [](const FAsyncMessage& Message)
		{
			// 验证消息确实在请求的命名线程上接收。
			// Ensure that we are actually geting our message on the thread we have requested to
			const uint32 CurrentThread = FPlatformTLS::GetCurrentThreadId();
			ensure(CurrentThread == Message.GetThreadQueuedFromThreadId());
			
			UE_LOG(LogAsyncMessageSystem, Verbose, TEXT("Successfully received message on RHI thread"));
		}, Opts);
	}

	// 首次等待五秒，再生成异步任务排队测试消息。
	// Wait 5 seconds, and then spawn an async task on the background thread to queue the message
	const float NextDelay = 5.0f;

	TWeakObjectPtr<UWorld> WeakWorld = GetWorld();

	// 设置循环 Timer，每秒调度一次向 RHI Thread 投递的消息任务。
	// Set up a timer to spawn a new background task every second
	GetWorld()->GetTimerManager().SetTimer(BackgroundQueueMessageHandle,
		FTimerDelegate::CreateWeakLambda(this, [WeakWorld]()
			{
				// 在指定后台命名线程上生成任务并排队消息。
				// Spawn an async task to queue an message, on a background thread
				AsyncTask(ENamedThreads::RHIThread, [WeakWorld]()
				{
					if (TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(WeakWorld.Get()))
					{
						Sys->QueueMessageForBroadcast(Message_Async_Test);
					}
				});
			
				// Timer 以一秒间隔循环执行。
				// loop every 1 second
			}), 1.0f, true, NextDelay);
}

// 解析颜色 Payload 并更新动态材质参数；未固定 Override ID 时解除当前监听并轮换到下一个颜色消息。
void AColorChangingTestListener::HandleColorChange(const FAsyncMessage& Message)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AColorChangingTestListener::HandleColorChange);

	ensure(IsInGameThread() || IsInParallelGameThread());
	
	// 从具体颜色消息中取得期望颜色 Payload。
	// Some specific color change here
	const FColorChangingMessage* Data = Message.GetPayloadData<const FColorChangingMessage>();
	if (!Data)
	{
		ensure(false);
		return;
	}

	// 更新网格全部动态材质上的颜色参数。
	// Change the dynamic material param colors
	FLinearColor Col = Data->DesiredColorToChange;
	MeshComp->SetColorParameterValueOnMaterials(MaterialColorParamName, Col);	
	
	// 配置了固定 Override 消息时保持现有绑定，不参与颜色轮换。
	// If we are listening for a specific message already, don't attempt to rebind
	if (OverrideMessageBinding.IsValid())
	{
		return;
	}
	
	// 解除刚刚收到的具体颜色消息监听。
	// Stop listening for the color that we just got
	ResetListenerToColor();

	// 继续绑定轮换序列中的下一个颜色消息。
	// And start listening to a new fancy color message
	SetupColorListener();
}

// 处理颜色父消息监听器收到的任意子消息，验证 Payload 后轻微旋转 Actor 作为可观察副作用。
void AColorChangingTestListener::HandleAnyColorChange(const FAsyncMessage& Message)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AColorChangingTestListener::HandleAnyColorChange);
	
	ensure(IsInGameThread() || IsInParallelGameThread());

	const FColorChangingMessage* Data = Message.GetPayloadData<const FColorChangingMessage>();
	if (!Data)
	{
		ensure(false);
		return;
	}

	const uint32 MyThread = FPlatformTLS::GetCurrentThreadId();

	// 每次收到任意颜色变化时轻微旋转 Actor，以表明父消息监听器已执行。
	// Any time there is a color change, rotate the actor a bit	
	AddActorLocalRotation(FRotator(0.0, 0.0, 10.0));
}

// 优先返回显式 Override 消息，否则在红、绿、蓝三个具体消息 ID 之间循环选择。
const FAsyncMessageId AColorChangingTestListener::GetColorChangeToListenTo()
{
	// 配置了具体消息 ID 时始终监听该消息。
	// If you have specified a specific color to listen for let you do so here
	if (OverrideMessageBinding.IsValid())
	{
		return OverrideMessageBinding;
	}
	
	// 将上次颜色索引循环推进到下一个红、绿或蓝消息。
	// Increment to the next color
	PreviousColorBoundTo = FMath::WrapExclusive(++PreviousColorBoundTo, 0, 3);

	int32 IndexToBindTo = PreviousColorBoundTo;
	
	if (IndexToBindTo == 0)
	{
		return Message_ChangeColor_Red;
	}
	else if (IndexToBindTo == 1)
	{
		return Message_ChangeColor_Green;
	}
	else if (IndexToBindTo == 2)
	{
		return Message_ChangeColor_Blue;
	}
	
	return Message_ChangeColor_Blue;
}

// 创建压力测试网格和默认重型消息 Payload，并按配置 TickGroup 启用逐帧 Tick。
AHeavyPerformanceBroadcastor::AHeavyPerformanceBroadcastor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RootMeshComp"));
	SetRootComponent(MeshComp);
	
	MessagePayloadToQueue = FInstancedStruct::Make(FTestHeavyMessage{});

	// Tick 默认位于 TG_PrePhysics，并可通过 CustomTickGroup 覆盖。
	// The tick function is TG_PrePhysics by default
	// Tick this actor every time pre-physics
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = CustomTickGroup;
}

// 每个配置 TickGroup 的 Tick 调用 QueueMessage，持续生成压力测试流量。
void AHeavyPerformanceBroadcastor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	QueueMessage();	
}

// 每帧把配置 Payload 按 NumberOfTimesToQueue 次数排入共享消息系统；系统不可用时直接返回。
void AHeavyPerformanceBroadcastor::QueueMessage()
{
	TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld());
	if (!Sys)
	{
		return;
	}

	for (int32 i = 0; i < NumberOfTimesToQueue; ++i)
	{
		// 排队一次配置的高频测试消息及其实例化结构 Payload。
		// Queue a color change message
		Sys->QueueMessageForBroadcast(MessageToQueue, MessagePayloadToQueue);	
	}
}

// 创建可移动根网格，供高频消息回调执行轻量旋转工作。
AHeavyPerformanceListener::AHeavyPerformanceListener(const FObjectInitializer& ObjectInitializer)
{
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ListenerRootMeshComp"));
	SetRootComponent(MeshComp);
	MeshComp->SetMobility(EComponentMobility::Movable);
}

// Actor 开始游戏时按指定消息 ID 和 TickGroup 注册压力测试监听器。
void AHeavyPerformanceListener::BeginPlay()
{
	Super::BeginPlay();

	SetupListener();
}

// Actor 结束游戏时解除压力测试监听器。
void AHeavyPerformanceListener::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	RemoveListener();
}

// 解析 FTestHeavyMessage 并按 Payload 旋转 Actor，以模拟每条消息触发的轻量玩法工作；Payload 类型错误时返回。
void AHeavyPerformanceListener::HandleMessage(const FAsyncMessage& Message)
{
	const FTestHeavyMessage* Data = Message.GetPayloadData<const FTestHeavyMessage>();
	
	if (!Data)
	{
		ensure(false);
		return;
	}

	// 轻微旋转 Actor，用于模拟消息触发的一小段真实玩法计算。
	// Rotate the actor a little itsy bitsy, just to replicate doing some kind of simple gameplay work.
	AddActorLocalRotation(Data->AmountToRotate);
}

// 按配置 TickGroup 绑定高频消息监听器，并验证返回句柄有效。
void AHeavyPerformanceListener::SetupListener()
{
	check(!ListenerHandle.IsValid());
	TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld());
	if (!Sys)
	{
		return;
	}

	FAsyncMessageBindingOptions BindingOpts = {};
	BindingOpts.SetTickGroup(GroupToListenFor);
	
	ListenerHandle = Sys->BindListener(
		MessageToListenFor,
		TWeakObjectPtr<AHeavyPerformanceListener>(this),
		&AHeavyPerformanceListener::HandleMessage,
		BindingOpts);

	ensure(ListenerHandle.IsValid());
}

// 若监听句柄和消息系统均有效，则解除高频消息监听。
void AHeavyPerformanceListener::RemoveListener()
{
	if (!ListenerHandle.IsValid())
	{
		return;
	}
	
	TSharedPtr<FAsyncGameplayMessageSystem> Sys = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld());
	if (!Sys)
	{
		return;
	}

	Sys->UnbindListener(ListenerHandle);
}
