// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPawn.h"

#include "GameFramework/Controller.h"
#include "LyraLogChannels.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ScriptInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPawn)

class FLifetimeProperty;
class UObject;

// 构造模块化 Pawn，创建 PawnExtensionComponent，并初始化为无队伍状态。
ALyraPawn::ALyraPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 登记 ALyraPawn 需要通过网络复制的属性及复制条件。
void ALyraPawn::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, MyTeamID);
}

// 在组件初始化前完成 ALyraPawn 的早期注册和依赖准备。
void ALyraPawn::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

// 在 EndPlay 阶段解除 ALyraPawn 的委托、状态注册和外部引用。
void ALyraPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// 服务器接管 Pawn 后通知 PawnExtension 刷新初始化状态，并从 Controller 取得及监听 TeamId。
void ALyraPawn::PossessedBy(AController* NewController)
{
	const FGenericTeamId OldTeamID = MyTeamID;

	Super::PossessedBy(NewController);

	// 从新 Controller 取得当前 TeamId，并订阅其后续队伍变化。
	// Grab the current team ID and listen for future changes
	if (ILyraTeamAgentInterface* ControllerAsTeamProvider = Cast<ILyraTeamAgentInterface>(NewController))
	{
		MyTeamID = ControllerAsTeamProvider->GetGenericTeamId();
		ControllerAsTeamProvider->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnControllerChangedTeam);
	}
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

// Pawn 失去 Controller 时解除旧队伍监听，并按策略重新确定失去控制后的 TeamId。
void ALyraPawn::UnPossessed()
{
	AController* const OldController = GetController();

	// 停止监听旧 Controller 的队伍变化。
	// Stop listening for changes from the old controller
	const FGenericTeamId OldTeamID = MyTeamID;
	if (ILyraTeamAgentInterface* ControllerAsTeamProvider = Cast<ILyraTeamAgentInterface>(OldController))
	{
		ControllerAsTeamProvider->GetTeamChangedDelegateChecked().RemoveAll(this);
	}

	Super::UnPossessed();

	// 根据失去控制权后的策略重新确定 Pawn 的 TeamId。
	// Determine what the new team ID should be afterwards
	MyTeamID = DetermineNewTeamAfterPossessionEnds(OldTeamID);
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

// 仅允许权威端在 Pawn 未被接管时直接设置 TeamId；被接管时队伍必须由 Controller 驱动。
void ALyraPawn::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	if (GetController() == nullptr)
	{
		if (HasAuthority())
		{
			const FGenericTeamId OldTeamID = MyTeamID;
			MyTeamID = NewTeamID;
			ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
		}
		else
		{
			UE_LOG(LogLyraTeams, Error, TEXT("You can't set the team ID on a pawn (%s) except on the authority"), *GetPathNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogLyraTeams, Error, TEXT("You can't set the team ID on a possessed pawn (%s); it's driven by the associated controller"), *GetPathNameSafe(this));
	}
}

// 返回 Pawn 当前缓存的 MyTeamID；尚未从 Controller 获得队伍时为 NoTeam。
FGenericTeamId ALyraPawn::GetGenericTeamId() const
{
	return MyTeamID;
}

// 返回 Pawn 自身的队伍变化多播委托地址。
FOnLyraTeamIndexChangedDelegate* ALyraPawn::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

// Controller 队伍变化时更新 Pawn 缓存 TeamId，并向 Pawn 监听者转发变化。
void ALyraPawn::OnControllerChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	const FGenericTeamId MyOldTeamID = MyTeamID;
	MyTeamID = IntegerToGenericTeamId(NewTeam);
	ConditionalBroadcastTeamChanged(this, MyOldTeamID, MyTeamID);
}

// MyTeamID 复制变化时向 Pawn 的队伍监听者广播旧值和新值。
void ALyraPawn::OnRep_MyTeamID(FGenericTeamId OldTeamID)
{
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}

