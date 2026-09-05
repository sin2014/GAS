// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraCheatManager.h"
#include "GameFramework/Pawn.h"
#include "LyraPlayerController.h"
#include "LyraDebugCameraController.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Console.h"
#include "GameFramework/HUD.h"
#include "System/LyraAssetManager.h"
#include "System/LyraGameData.h"
#include "LyraGameplayTags.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/LyraHealthComponent.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "System/LyraSystemStatics.h"
#include "Development/LyraDeveloperSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraCheatManager)

// Lyra 作弊命令执行结果和错误使用的日志分类。
DEFINE_LOG_CATEGORY(LogLyraCheat);

namespace LyraCheat
{
	// 固定相机调试模式写入 PlayerCameraManager CameraStyle 的名称。
	static const FName NAME_Fixed = FName(TEXT("Fixed"));
	
	// 是否允许 CycleDebugCameras 在普通、固定和自由调试相机之间循环。
	static bool bEnableDebugCameraCycling = false;
	// 绑定控制调试相机循环能力的 Cheat CVar。
	static FAutoConsoleVariableRef CVarEnableDebugCameraCycling(
		TEXT("LyraCheat.EnableDebugCameraCycling"),
		bEnableDebugCameraCycling,
		TEXT("If true then you can cycle the debug camera while running the game."),
		ECVF_Cheat);

	// 是否在 CheatManager 初始化时自动为所属玩家启用 God 模式。
	static bool bStartInGodMode = false;
	// 绑定自动启用 God 模式的 Cheat CVar。
	static FAutoConsoleVariableRef CVarStartInGodMode(
		TEXT("LyraCheat.StartInGodMode"),
		bStartInGodMode,
		TEXT("If true then the God cheat will be applied on begin play"),
		ECVF_Cheat);
};


// 将自由调试相机 Controller 类配置为 ALyraDebugCameraController。
ULyraCheatManager::ULyraCheatManager()
{
	DebugCameraControllerClass = ALyraDebugCameraController::StaticClass();
}

// 初始化作弊管理器，执行配置为创建阶段运行的命令，并可按 CVar 自动开启 God 模式。
void ULyraCheatManager::InitCheatManager()
{
	Super::InitCheatManager();

#if WITH_EDITOR
	if (GIsEditor)
	{
		APlayerController* PC = GetOuterAPlayerController();
		for (const FLyraCheatToRun& CheatRow : GetDefault<ULyraDeveloperSettings>()->CheatsToRun)
		{
			if (CheatRow.Phase == ECheatExecutionTime::OnCheatManagerCreated)
			{
				PC->ConsoleCommand(CheatRow.Cheat, /*bWriteToLog=*/ true);
			}
		}
	}
#endif

	if (LyraCheat::bStartInGodMode)
	{
		God();	
	}
}

// 同时把作弊反馈写入游戏视口控制台和 LogLyraCheat。
void ULyraCheatManager::CheatOutputText(const FString& TextToOutput)
{
#if USING_CHEAT_MANAGER
	// 将反馈文本写入游戏内控制台。
	// Output to the console.
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportConsole)
	{
		GEngine->GameViewport->ViewportConsole->OutputText(TextToOutput);
	}

	// 同时将反馈文本写入 LogLyraCheat 日志。
	// Output to log.
	UE_LOG(LogLyraCheat, Display, TEXT("%s"), *TextToOutput);
#endif // USING_CHEAT_MANAGER
}

// 通过 LyraPlayerController 在服务器上为所属玩家执行命令。
void ULyraCheatManager::Cheat(const FString& Msg)
{
	if (ALyraPlayerController* LyraPC = Cast<ALyraPlayerController>(GetOuterAPlayerController()))
	{
		LyraPC->ServerCheat(Msg.Left(128));
	}
}

// 通过服务器让所有玩家 Controller 执行同一作弊命令。
void ULyraCheatManager::CheatAll(const FString& Msg)
{
	if (ALyraPlayerController* LyraPC = Cast<ALyraPlayerController>(GetOuterAPlayerController()))
	{
		LyraPC->ServerCheatAll(Msg.Left(128));
	}
}

// 在权威端请求 GameMode 结束当前比赛并进入下一局。
void ULyraCheatManager::PlayNextGame()
{
	ULyraSystemStatics::PlayNextGame(this);
}

// 启用 ULyraCheatManager 的 DebugCamera 模式并同步控制状态。
void ULyraCheatManager::EnableDebugCamera()
{
	Super::EnableDebugCamera();
}

// 停用 ULyraCheatManager 的 DebugCamera 模式并同步控制状态。
void ULyraCheatManager::DisableDebugCamera()
{
	FVector DebugCameraLocation;
	FRotator DebugCameraRotation;

	ADebugCameraController* DebugCC = Cast<ADebugCameraController>(GetOuter());
	APlayerController* OriginalPC = nullptr;

	if (DebugCC)
	{
		OriginalPC = DebugCC->OriginalControllerRef;
		DebugCC->GetPlayerViewPoint(DebugCameraLocation, DebugCameraRotation);
	}

	Super::DisableDebugCamera();

	if (OriginalPC && OriginalPC->PlayerCameraManager && (OriginalPC->PlayerCameraManager->CameraStyle == LyraCheat::NAME_Fixed))
	{
		OriginalPC->SetInitialLocationAndRotation(DebugCameraLocation, DebugCameraRotation);

		OriginalPC->PlayerCameraManager->ViewTarget.POV.Location = DebugCameraLocation;
		OriginalPC->PlayerCameraManager->ViewTarget.POV.Rotation = DebugCameraRotation;
		OriginalPC->PlayerCameraManager->PendingViewTarget.POV.Location = DebugCameraLocation;
		OriginalPC->PlayerCameraManager->PendingViewTarget.POV.Rotation = DebugCameraRotation;
	}
}

// 当前 PlayerController 的控制目标是 DebugCameraController 时返回 true。
bool ULyraCheatManager::InDebugCamera() const
{
	return (Cast<ADebugCameraController>(GetOuter()) ? true : false);
}

// 启用 ULyraCheatManager 的 FixedCamera 模式并同步控制状态。
void ULyraCheatManager::EnableFixedCamera()
{
	const ADebugCameraController* DebugCC = Cast<ADebugCameraController>(GetOuter());
	APlayerController* PC = (DebugCC ? ToRawPtr(DebugCC->OriginalControllerRef) : GetOuterAPlayerController());

	if (PC && PC->PlayerCameraManager)
	{
		PC->SetCameraMode(LyraCheat::NAME_Fixed);
	}
}

// 停用 ULyraCheatManager 的 FixedCamera 模式并同步控制状态。
void ULyraCheatManager::DisableFixedCamera()
{
	const ADebugCameraController* DebugCC = Cast<ADebugCameraController>(GetOuter());
	APlayerController* PC = (DebugCC ? ToRawPtr(DebugCC->OriginalControllerRef) : GetOuterAPlayerController());

	if (PC && PC->PlayerCameraManager)
	{
		PC->SetCameraMode(NAME_Default);
	}
}

// PlayerCameraManager 的 CameraStyle 为 Fixed 时返回 true。
bool ULyraCheatManager::InFixedCamera() const
{
	const ADebugCameraController* DebugCC = Cast<ADebugCameraController>(GetOuter());
	const APlayerController* PC = (DebugCC ? ToRawPtr(DebugCC->OriginalControllerRef) : GetOuterAPlayerController());

	if (PC && PC->PlayerCameraManager)
	{
		return (PC->PlayerCameraManager->CameraStyle == LyraCheat::NAME_Fixed);
	}

	return false;
}

// 切换 ULyraCheatManager 的 ToggleFixedCamera 状态，并应用新的运行时配置。
void ULyraCheatManager::ToggleFixedCamera()
{
	if (InFixedCamera())
	{
		DisableFixedCamera();
	}
	else
	{
		EnableFixedCamera();
	}
}

// 切换 ULyraCheatManager 的 CycleDebugCameras 状态，并应用新的运行时配置。
void ULyraCheatManager::CycleDebugCameras()
{
	if (!LyraCheat::bEnableDebugCameraCycling)
	{
		return;
	}
	
	if (InDebugCamera())
	{
		EnableFixedCamera();
		DisableDebugCamera();
	}
	else if (InFixedCamera())
	{
		DisableFixedCamera();
		DisableDebugCamera();
	}
	else
	{
		EnableDebugCamera();
		DisableFixedCamera();
	}
}

// 切换 ULyraCheatManager 的 CycleAbilitySystemDebug 状态，并应用新的运行时配置。
void ULyraCheatManager::CycleAbilitySystemDebug()
{
	APlayerController* PC = Cast<APlayerController>(GetOuterAPlayerController());

	if (PC && PC->MyHUD)
	{
		if (!PC->MyHUD->bShowDebugInfo || !PC->MyHUD->DebugDisplay.Contains(TEXT("AbilitySystem")))
		{
			PC->MyHUD->ShowDebug(TEXT("AbilitySystem"));
		}

		PC->ConsoleCommand(TEXT("AbilitySystem.Debug.NextCategory"));
	}
}

// 从所属玩家 ASC 取消所有由输入激活且当前处于活动状态的技能。
void ULyraCheatManager::CancelActivatedAbilities()
{
	if (ULyraAbilitySystemComponent* LyraASC = GetPlayerAbilitySystemComponent())
	{
		const bool bReplicateCancelAbility = true;
		LyraASC->CancelInputActivatedAbilities(bReplicateCancelAbility);
	}
}

// 解析 TagName 并向所属玩家 ASC 添加一个动态 GameplayTag；标签或 ASC 无效时输出错误。
void ULyraCheatManager::AddTagToSelf(FString TagName)
{
	FGameplayTag Tag = LyraGameplayTags::FindTagByString(TagName, true);
	if (Tag.IsValid())
	{
		if (ULyraAbilitySystemComponent* LyraASC = GetPlayerAbilitySystemComponent())
		{
			LyraASC->AddDynamicTagGameplayEffect(Tag);
		}
	}
	else
	{
		UE_LOG(LogLyraCheat, Display, TEXT("AddTagToSelf: Could not find any tag matching [%s]."), *TagName);
	}
}

// 解析 TagName 并从所属玩家 ASC 移除一个动态 GameplayTag；标签或 ASC 无效时输出错误。
void ULyraCheatManager::RemoveTagFromSelf(FString TagName)
{
	FGameplayTag Tag = LyraGameplayTags::FindTagByString(TagName, true);
	if (Tag.IsValid())
	{
		if (ULyraAbilitySystemComponent* LyraASC = GetPlayerAbilitySystemComponent())
		{
			LyraASC->RemoveDynamicTagGameplayEffect(Tag);
		}
	}
	else
	{
		UE_LOG(LogLyraCheat, Display, TEXT("RemoveTagFromSelf: Could not find any tag matching [%s]."), *TagName);
	}
}

// 对所属玩家 ASC 应用带 SetByCaller_Damage 数值的伤害 GameplayEffect；没有 ASC 时不处理。
void ULyraCheatManager::DamageSelf(float DamageAmount)
{
	if (ULyraAbilitySystemComponent* LyraASC = GetPlayerAbilitySystemComponent())
	{
		ApplySetByCallerDamage(LyraASC, DamageAmount);
	}
}

// 客户端先把命令转发到服务器；权威端射线选择准星目标，并向其 Lyra ASC 应用指定伤害。
void ULyraCheatManager::DamageTarget(float DamageAmount)
{
	if (ALyraPlayerController* LyraPC = Cast<ALyraPlayerController>(GetOuterAPlayerController()))
	{
		if (LyraPC->GetNetMode() == NM_Client)
		{
			// 客户端调用时自动将伤害目标命令转发到服务器执行。
			// Automatically send cheat to server for convenience.
			LyraPC->ServerCheat(FString::Printf(TEXT("DamageTarget %.2f"), DamageAmount));
			return;
		}

		FHitResult TargetHitResult;
		AActor* TargetActor = GetTarget(LyraPC, TargetHitResult);

		if (ULyraAbilitySystemComponent* LyraTargetASC = Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor)))
		{
			ApplySetByCallerDamage(LyraTargetASC, DamageAmount);
		}
	}
}

// 创建项目配置的 SetByCaller 伤害 GameplayEffectSpec，写入 DamageMagnitude 后应用到目标 ASC 自身。
void ULyraCheatManager::ApplySetByCallerDamage(ULyraAbilitySystemComponent* LyraASC, float DamageAmount)
{
	check(LyraASC);

	TSubclassOf<UGameplayEffect> DamageGE = ULyraAssetManager::GetSubclass(ULyraGameData::Get().DamageGameplayEffect_SetByCaller);
	FGameplayEffectSpecHandle SpecHandle = LyraASC->MakeOutgoingSpec(DamageGE, 1.0f, LyraASC->MakeEffectContext());

	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Damage, DamageAmount);
		LyraASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

// 对所属玩家 ASC 应用带 SetByCaller_Heal 数值的治疗 GameplayEffect；没有 ASC 时不处理。
void ULyraCheatManager::HealSelf(float HealAmount)
{
	if (ULyraAbilitySystemComponent* LyraASC = GetPlayerAbilitySystemComponent())
	{
		ApplySetByCallerHeal(LyraASC, HealAmount);
	}
}

// 射线选择准星目标，并向目标 Actor 的 Lyra ASC 应用指定治疗量；目标没有 ASC 时不处理。
void ULyraCheatManager::HealTarget(float HealAmount)
{
	if (ALyraPlayerController* LyraPC = Cast<ALyraPlayerController>(GetOuterAPlayerController()))
	{
		FHitResult TargetHitResult;
		AActor* TargetActor = GetTarget(LyraPC, TargetHitResult);

		if (ULyraAbilitySystemComponent* LyraTargetASC = Cast<ULyraAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor)))
		{
			ApplySetByCallerHeal(LyraTargetASC, HealAmount);
		}
	}
}

// 创建项目配置的 SetByCaller 治疗 GameplayEffectSpec，写入 HealMagnitude 后应用到目标 ASC 自身。
void ULyraCheatManager::ApplySetByCallerHeal(ULyraAbilitySystemComponent* LyraASC, float HealAmount)
{
	check(LyraASC);

	TSubclassOf<UGameplayEffect> HealGE = ULyraAssetManager::GetSubclass(ULyraGameData::Get().HealGameplayEffect_SetByCaller);
	FGameplayEffectSpecHandle SpecHandle = LyraASC->MakeOutgoingSpec(HealGE, 1.0f, LyraASC->MakeEffectContext());

	if (SpecHandle.IsValid())
	{
		SpecHandle.Data->SetSetByCallerMagnitude(LyraGameplayTags::SetByCaller_Heal, HealAmount);
		LyraASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

// 从所属 PlayerController 的 PlayerState 获取 Lyra ASC；缺少 Controller 或 ASC 时返回 nullptr。
ULyraAbilitySystemComponent* ULyraCheatManager::GetPlayerAbilitySystemComponent() const
{
	if (ALyraPlayerController* LyraPC = Cast<ALyraPlayerController>(GetOuterAPlayerController()))
	{
		return LyraPC->GetLyraAbilitySystemComponent();
	}
	return nullptr;
}

// 仅当所属 Pawn 已达到 GameplayReady 且拥有 HealthComponent 时触发自毁致死流程。
void ULyraCheatManager::DamageSelfDestruct()
{
	if (ALyraPlayerController* LyraPC = Cast<ALyraPlayerController>(GetOuterAPlayerController()))
	{
 		if (const ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(LyraPC->GetPawn()))
		{
			if (PawnExtComp->HasReachedInitState(LyraGameplayTags::InitState_GameplayReady))
			{
				if (ULyraHealthComponent* HealthComponent = ULyraHealthComponent::FindHealthComponent(LyraPC->GetPawn()))
				{
					HealthComponent->DamageSelfDestruct();
				}
			}
		}
	}
}

// 客户端自动转发到服务器；权威端通过动态标签切换所属玩家的 DamageImmunity 状态。
void ULyraCheatManager::God()
{
	if (ALyraPlayerController* LyraPC = Cast<ALyraPlayerController>(GetOuterAPlayerController()))
	{
		if (LyraPC->GetNetMode() == NM_Client)
		{
			// 客户端调用时自动将无敌命令转发到服务器执行。
			// Automatically send cheat to server for convenience.
			LyraPC->ServerCheat(FString::Printf(TEXT("God")));
			return;
		}

		if (ULyraAbilitySystemComponent* LyraASC = LyraPC->GetLyraAbilitySystemComponent())
		{
			const FGameplayTag Tag = LyraGameplayTags::Cheat_GodMode;
			const bool bHasTag = LyraASC->HasMatchingGameplayTag(Tag);

			if (bHasTag)
			{
				LyraASC->RemoveDynamicTagGameplayEffect(Tag);
			}
			else
			{
				LyraASC->AddDynamicTagGameplayEffect(Tag);
			}
		}
	}
}

// 按 Enabled 参数显式设置或切换 Cheat.UnlimitedHealth 标签，使生命值最低保持为 1。
void ULyraCheatManager::UnlimitedHealth(int32 Enabled)
{
	if (ULyraAbilitySystemComponent* LyraASC = GetPlayerAbilitySystemComponent())
	{
		const FGameplayTag Tag = LyraGameplayTags::Cheat_UnlimitedHealth;
		const bool bHasTag = LyraASC->HasMatchingGameplayTag(Tag);

		if ((Enabled == -1) || ((Enabled > 0) && !bHasTag) || ((Enabled == 0) && bHasTag))
		{
			if (bHasTag)
			{
				LyraASC->RemoveDynamicTagGameplayEffect(Tag);
			}
			else
			{
				LyraASC->AddDynamicTagGameplayEffect(Tag);
			}
		}
	}
}

