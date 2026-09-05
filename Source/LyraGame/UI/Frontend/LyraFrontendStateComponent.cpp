// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraFrontendStateComponent.h"

#include "CommonGameInstance.h"
#include "CommonSessionSubsystem.h"
#include "CommonUserSubsystem.h"
#include "ControlFlowManager.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NativeGameplayTags.h"
#include "PrimaryGameLayout.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraFrontendStateComponent)

namespace FrontendTags
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PLATFORM_TRAIT_SINGLEONLINEUSER, "Platform.Trait.SingleOnlineUser");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_UI_LAYER_MENU, "UI.Layer.Menu");
}

// 构造负责驱动登录、会话恢复和前端页面切换的状态组件。
ULyraFrontendStateComponent::ULyraFrontendStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 开始游戏时以高优先级等待 Experience 加载完成，再启动前端控制流。
void ULyraFrontendStateComponent::BeginPlay()
{
	Super::BeginPlay();

	// 前端控制流必须等 Experience 加载完成后才能决定登录、会话和主界面步骤。
	// Listen for the experience load to complete
	AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>();
	ULyraExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyraExperienceManagerComponent>();
	check(ExperienceComponent);

	// Experience 组件与本组件生命周期相同，因此无需在 EndPlay 中单独解除此委托。
	// This delegate is on a component with the same lifetime as this one, so no need to unhook it in 
	ExperienceComponent->CallOrRegister_OnExperienceLoaded_HighPriority(FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

// 仅转发基类 EndPlay；Experience 委托与同生命周期组件绑定，当前不在此额外撤销前端控制流。
void ULyraFrontendStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// 前端控制流未完成时请求显示加载界面，并优先用当前步骤名称说明阻塞原因。
bool ULyraFrontendStateComponent::ShouldShowLoadingScreen(FString& OutReason) const
{
	if (bShouldShowLoadingScreen)
	{
		OutReason = TEXT("Frontend Flow Pending...");
		
		if (FrontEndFlow.IsValid())
		{
			const TOptional<FString> StepDebugName = FrontEndFlow->GetCurrentStepDebugName();
			if (StepDebugName.IsSet())
			{
				OutReason = StepDebugName.GetValue();
			}
		}
		
		return true;
	}

	return false;
}

// Experience 就绪后创建并执行用户初始化、Press Start、请求会话和主界面的顺序控制流。
void ULyraFrontendStateComponent::OnExperienceLoaded(const ULyraExperienceDefinition* Experience)
{
	FControlFlow& Flow = FControlFlowStatics::Create(this, TEXT("FrontendFlow"))
		.QueueStep(TEXT("Wait For User Initialization"), this, &ThisClass::FlowStep_WaitForUserInitialization)
		.QueueStep(TEXT("Try Show Press Start Screen"), this, &ThisClass::FlowStep_TryShowPressStartScreen)
		.QueueStep(TEXT("Try Join Requested Session"), this, &ThisClass::FlowStep_TryJoinRequestedSession)
		.QueueStep(TEXT("Try Show Main Screen"), this, &ThisClass::FlowStep_TryShowMainScreen);

	Flow.ExecuteFlow();

	FrontEndFlow = Flow.AsShared();
}

// 硬断线时重置用户状态，始终清理旧会话，然后继续前端流程。
void ULyraFrontendStateComponent::FlowStep_WaitForUserInitialization(FControlFlowNodeRef SubFlow)
{
	// URL 带 closed 选项时视为硬断线，显式清除用户和会话状态。
	// TODO：应重构引擎断线流程，直接传递断线原因而不是通过 URL 选项推断。
	// If this was a hard disconnect, explicitly destroy all user and session state
	// TODO: Refactor the engine disconnect flow so it is more explicit about why it happened
	bool bWasHardDisconnect = false;
	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AGameModeBase>();
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);

	if (ensure(GameMode) && UGameplayStatics::HasOption(GameMode->OptionsString, TEXT("closed")))
	{
		bWasHardDisconnect = true;
	}

	// 仅硬断线重置用户登录状态，普通返回前端时保留用户。
	// Only reset users on hard disconnect
	UCommonUserSubsystem* UserSubsystem = GameInstance->GetSubsystem<UCommonUserSubsystem>();
	if (ensure(UserSubsystem) && bWasHardDisconnect)
	{
		UserSubsystem->ResetUserState();
	}

	// 无论断线类型如何都清理现有在线会话状态。
	// Always reset sessions
	UCommonSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UCommonSessionSubsystem>();
	if (ensure(SessionSubsystem))
	{
		SessionSubsystem->CleanUpSessions();
	}

	SubFlow->ContinueFlow();
}

// 已登录时直接继续；无需 Press Start 的平台异步自动登录，否则异步压入页面并在停用或取消后继续。
void ULyraFrontendStateComponent::FlowStep_TryShowPressStartScreen(FControlFlowNodeRef SubFlow)
{
	const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	UCommonUserSubsystem* UserSubsystem = GameInstance->GetSubsystem<UCommonUserSubsystem>();

	// 第一位本地玩家已完成本地或在线登录时，跳过 Press Start 步骤。
	// Check to see if the first player is already logged in, if they are, we can skip the press start screen.
	if (const UCommonUserInfo* FirstUser = UserSubsystem->GetUserInfoForLocalPlayerIndex(0))
	{
		if (FirstUser->InitializationState == ECommonUserInitializationState::LoggedInLocalOnly ||
			FirstUser->InitializationState == ECommonUserInitializationState::LoggedInOnline)
		{
			SubFlow->ContinueFlow();
			return;
		}
	}

	// 只有多在线用户平台需要通过按下 Start 的控制器确定登录用户时才显示 Press Start；其他平台直接使用默认输入设备自动登录。
	// Check to see if the platform actually requires a 'Press Start' screen.  This is only
	// required on platforms where there can be multiple online users where depending on what player's
	// controller presses 'Start' establishes the player to actually login to the game with.
	if (!UserSubsystem->ShouldWaitForStartInput())
	{
		// 启动使用默认输入设备 ID 的自动本地登录，并保存流程节点直到异步初始化回调。
		// Start the auto login process, this should finish quickly and will use the default input device id
		InProgressPressStartScreen = SubFlow;
		UserSubsystem->OnUserInitializeComplete.AddDynamic(this, &ULyraFrontendStateComponent::OnUserInitialized);
		UserSubsystem->TryToInitializeForLocalPlay(0, FInputDeviceId(), false);

		return;
	}

	// 异步压入 Press Start 页面；页面停用或压入取消后关闭加载界面并继续控制流。
	// Add the Press Start screen, move to the next flow when it deactivates.
	if (UPrimaryGameLayout* RootLayout = UPrimaryGameLayout::GetPrimaryGameLayoutForPrimaryPlayer(this))
	{
		constexpr bool bSuspendInputUntilComplete = true;
		TWeakObjectPtr<ULyraFrontendStateComponent> WeakThis = this;
		RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(FrontendTags::TAG_UI_LAYER_MENU, bSuspendInputUntilComplete, PressStartScreenClass,
			[this, WeakThis, SubFlow](EAsyncWidgetLayerState State, UCommonActivatableWidget* Screen) 
			{
				if (WeakThis.IsValid())
				{
					switch (State)
					{
					case EAsyncWidgetLayerState::AfterPush:
						bShouldShowLoadingScreen = false;
						Screen->OnDeactivated().AddWeakLambda(this, [this, SubFlow]() {
							SubFlow->ContinueFlow();
						});
						break;
					case EAsyncWidgetLayerState::Canceled:
						bShouldShowLoadingScreen = false;
						SubFlow->ContinueFlow();
						return;
					}
				}
			}
		);
	}
}

// 解除一次性用户初始化委托并清空待处理节点；当前无论成功或失败都继续前端流程。
void ULyraFrontendStateComponent::OnUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext)
{
	FControlFlowNodePtr FlowToContinue = InProgressPressStartScreen;
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(this);
	UCommonUserSubsystem* UserSubsystem = GameInstance->GetSubsystem<UCommonUserSubsystem>();

	if (ensure(FlowToContinue.IsValid() && UserSubsystem))
	{
		UserSubsystem->OnUserInitializeComplete.RemoveDynamic(this, &ULyraFrontendStateComponent::OnUserInitialized);
		InProgressPressStartScreen.Reset();

		if (bSuccess)
		{
			// 用户初始化成功后正常继续前端流程。
			// On success continue flow normally
			FlowToContinue->ContinueFlow();
		}
		else
		{
			// TODO：当前初始化失败仍继续流程，后续应改为进入登录错误页面。
			// TODO: Just continue for now, could go to some sort of error screen
			FlowToContinue->ContinueFlow();
		}
	}
}

// 存在可加入请求会话时监听一次性加入结果：成功取消本地主菜单流程，失败继续；无请求时直接继续。
void ULyraFrontendStateComponent::FlowStep_TryJoinRequestedSession(FControlFlowNodeRef SubFlow)
{
	UCommonGameInstance* GameInstance = Cast<UCommonGameInstance>(UGameplayStatics::GetGameInstance(this));
	if (GameInstance->GetRequestedSession() != nullptr && GameInstance->CanJoinRequestedSession())
	{
		UCommonSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<UCommonSessionSubsystem>();
		if (ensure(SessionSubsystem))
		{
			// 监听请求会话加入结果：成功时取消进入主菜单的流程，失败时继续显示主菜单。
			// TODO：还需确保会话加入成功后服务器 Travel 确实完成，再结束过渡状态。
			// Bind to session join completion to continue or cancel the flow
			// TODO:  Need to ensure that after session join completes, the server travel completes.
			OnJoinSessionCompleteEventHandle = SessionSubsystem->OnJoinSessionCompleteEvent.AddWeakLambda(this, [this, SubFlow, SessionSubsystem](const FOnlineResultInformation& Result)
			{
				// 回调由 SessionSubsystem 自身触发，此时对象必然有效，可立即解除一次性委托。
				// Unbind delegate. SessionSubsystem is the object triggering this event, so it must still be valid.
				SessionSubsystem->OnJoinSessionCompleteEvent.Remove(OnJoinSessionCompleteEventHandle);
				OnJoinSessionCompleteEventHandle.Reset();

				if (Result.bWasSuccessful)
				{
					// 会话加入成功后将发生联网 Travel，不再进入本地主菜单，因此取消剩余流程。
					// No longer transitioning to the main menu
					SubFlow->CancelFlow();
				}
				else
				{
					// 会话加入失败，继续进入本地主菜单。
					// Proceed to the main menu
					SubFlow->ContinueFlow();
					return;
				}
			});
			GameInstance->JoinRequestedSession();
			return;
		}
	}
	// 没有可加入的请求会话时直接跳过此步骤。
	// Skip this step if we didn't start requesting a session join
	SubFlow->ContinueFlow();
}

// 异步压入主界面，并在推入成功或取消时关闭加载状态并继续控制流。
void ULyraFrontendStateComponent::FlowStep_TryShowMainScreen(FControlFlowNodeRef SubFlow)
{
	if (UPrimaryGameLayout* RootLayout = UPrimaryGameLayout::GetPrimaryGameLayoutForPrimaryPlayer(this))
	{
		constexpr bool bSuspendInputUntilComplete = true;
		TWeakObjectPtr<ULyraFrontendStateComponent> WeakThis = this;
		RootLayout->PushWidgetToLayerStackAsync<UCommonActivatableWidget>(FrontendTags::TAG_UI_LAYER_MENU, bSuspendInputUntilComplete, MainScreenClass,
		    [this, WeakThis, SubFlow](EAsyncWidgetLayerState State, UCommonActivatableWidget* Screen)
		    {
			    if (WeakThis.IsValid())
			    {
				    switch (State)
				    {
				    case EAsyncWidgetLayerState::AfterPush:
					    bShouldShowLoadingScreen = false;
					    SubFlow->ContinueFlow();
					    return;
				    case EAsyncWidgetLayerState::Canceled:
					    bShouldShowLoadingScreen = false;
					    SubFlow->ContinueFlow();
					    return;
				    }
			    }
		    }
		);
	}
}

