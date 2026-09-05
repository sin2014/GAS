// Copyright Epic Games, Inc. All Rights Reserved.

#include "AsyncMixin.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Stats/Stats.h"

// 定义 AsyncMixin 的文件内日志分类，用于跟踪队列启动、步骤等待、取消和状态销毁。
DEFINE_LOG_CATEGORY_STATIC(LogAsyncMixin, Log, All);

// 保存每个混入实例按需创建的加载状态，使 FAsyncMixin 本身保持零额外实例内存。
TMap<FAsyncMixin*, TSharedRef<FAsyncMixin::FLoadingState>> FAsyncMixin::Loading;

// 初始化一个尚未分配加载状态的异步混入实例；实际状态会在首次登记步骤时创建。
FAsyncMixin::FAsyncMixin()
{
}

// 在游戏线程销毁宿主时移除对应状态，从而取消未完成请求并阻断后续完成回调。
FAsyncMixin::~FAsyncMixin()
{
	check(IsInGameThread());

	// 从映射中移除状态会连带取消其监控的待处理加载，之后不应再收到完成回调。
	// Removing the loading state will cancel any pending loadings it was 
	// monitoring, and shouldn't receive any future callbacks for completion.
	Loading.Remove(this);
}

// 返回已存在的只读加载状态；调用方必须先确认该实例确实拥有状态。
const FAsyncMixin::FLoadingState& FAsyncMixin::GetLoadingStateConst() const
{
	check(IsInGameThread());
	return Loading.FindChecked(this).Get();
}

// 获取加载状态，并在首次登记异步工作时按需创建并加入静态映射。
FAsyncMixin::FLoadingState& FAsyncMixin::GetLoadingState()
{
	check(IsInGameThread());

	if (TSharedRef<FLoadingState>* LoadingState = Loading.Find(this))
	{
		return (*LoadingState).Get();
	}

	return Loading.Add(this, MakeShared<FLoadingState>(*this)).Get();
}

// 判断当前混入实例是否已经分配外置加载状态。
bool FAsyncMixin::HasLoadingState() const
{
	check(IsInGameThread());

	return Loading.Contains(this);
}

// 仅在状态存在时取消队列并请求延迟销毁，避免一次无任务取消反而分配状态。
void FAsyncMixin::CancelAsyncLoading()
{
	// 没有待处理工作时不要为了执行取消而创建加载状态。
	// Don't create the loading state if we don't have anything pending.
	if (HasLoadingState())
	{
		GetLoadingState().CancelAndDestroy();
	}
}

// 查询已启动队列是否仍在加载；尚未创建状态时直接返回 false。
bool FAsyncMixin::IsAsyncLoadingInProgress() const
{
	// 没有待处理工作时不要为了查询状态而创建加载状态。
	// Don't create the loading state if we don't have anything pending.
	if (HasLoadingState())
	{
		return GetLoadingStateConst().IsLoadingInProgress();
	}

	return false;
}

// 查询当前是否正在处理步骤，或已安排下一帧启动但尚未开始。
bool FAsyncMixin::IsLoadingInProgressOrPending() const
{
	if (HasLoadingState())
	{
		return GetLoadingStateConst().IsLoadingInProgressOrPending();
	}

	return false;
}

// 为单个软对象路径创建流送步骤，并将完成委托交给加载状态按序执行。
void FAsyncMixin::AsyncLoad(FSoftObjectPath SoftObjectPath, const FSimpleDelegate& DelegateToCall)
{
	GetLoadingState().AsyncLoad(SoftObjectPath, DelegateToCall);
}

// 为一组软对象路径创建同一个流送步骤，整组加载完成后再推进队列。
void FAsyncMixin::AsyncLoad(const TArray<FSoftObjectPath>& SoftObjectPaths, const FSimpleDelegate& DelegateToCall)
{
	GetLoadingState().AsyncLoad(SoftObjectPaths, DelegateToCall);
}

// 登记主资产 Bundle 预加载步骤，并保留其句柄以维持资源驻留。
void FAsyncMixin::AsyncPreloadPrimaryAssetsAndBundles(const TArray<FPrimaryAssetId>& AssetIds, const TArray<FName>& LoadBundles, const FSimpleDelegate& DelegateToCall)
{
	GetLoadingState().AsyncPreloadPrimaryAssetsAndBundles(AssetIds, LoadBundles, DelegateToCall);
}

// 登记一个需轮询至完成的条件步骤，并在条件满足后执行回调。
void FAsyncMixin::AsyncCondition(TSharedRef<FAsyncCondition> Condition, const FSimpleDelegate& Callback)
{
	GetLoadingState().AsyncCondition(Condition, Callback);
}

// 将不依赖资源的事件委托作为立即完成步骤插入有序队列。
void FAsyncMixin::AsyncEvent(const FSimpleDelegate& Callback)
{
	GetLoadingState().AsyncEvent(Callback);
}

// 启动待处理队列；若没有登记任何步骤，也保持生命周期契约，依次触发开始和结束通知。
void FAsyncMixin::StartAsyncLoading()
{
	// 未登记任务时没有加载状态，直接成对触发通知即可，无需为立即释放的状态分配内存。
	// If we don't actually have any loading state because they've not queued anything to load,
	// just immediately start and finish the operation by calling the callbacks, no point in allocating
	// the memory just to de-allocate it.
	if (IsLoadingInProgressOrPending())
	{
		GetLoadingState().Start();
	}
	else
	{
		OnStartedLoading();
		OnFinishedLoading();
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// 将按需创建的队列状态绑定到宿主混入对象，用于通知和映射移除。
FAsyncMixin::FLoadingState::FLoadingState(FAsyncMixin& InOwner)
	: OwnerRef(InOwner)
{
}

// 销毁状态时同步取消步骤和所有延迟计时器，确保不再有 Ticker 回调访问该对象。
FAsyncMixin::FLoadingState::~FLoadingState()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAsyncMixin_FLoadingState_DestroyThisMemoryDelegate);
	UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] Destroy LoadingState (Done)"), this);

	// 状态已进入析构，必须取消当前工作和原先安排的延迟销毁，防止重复清理。
	// If we get destroyed, need to cancel whatever we're doing and cancel any
	// pending destruction - as we're already on the way out.
	CancelOnly(/*bDestroying*/true);
	CancelDestroyThisMemory(/*bDestroying*/true);
}

// 停止兜底启动并取消每个步骤；把步骤移到延迟析构数组后重置队列游标，以规避回调重入时数组 Reset 导致的对象损坏。
void FAsyncMixin::FLoadingState::CancelOnly(bool bDestroying)
{
	if (!bDestroying)
	{
		UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] Cancel"), this);
	}

	CancelStartTimer();

	for (TUniquePtr<FAsyncStep>& Step : AsyncSteps)
	{
		Step->Cancel();
	}

	// 将步骤所有权移入另一个数组，避免当前回调栈仍引用步骤时立即析构造成崩溃。
	// Moving the memory to another array so we don't crash.
	// There was an issue where the Step would get corrupted because we were calling Reset() on the array.
	AsyncStepsPendingDestruction = MoveTemp(AsyncSteps);

	bPreloadedBundles = false;
	bHasStarted = false;
	CurrentAsyncStep = 0;
}

// 取消队列并在后续 Ticker 中从宿主映射移除状态，避免当前成员函数执行期间自毁。
void FAsyncMixin::FLoadingState::CancelAndDestroy()
{
	CancelOnly(/*bDestroying*/false);
	RequestDestroyThisMemory();
}

// 撤销尚未执行的延迟销毁请求；状态被重新使用或已经析构时不应再由旧 Ticker 删除。
void FAsyncMixin::FLoadingState::CancelDestroyThisMemory(bool bDestroying)
{
	// 如果已经安排释放该状态，需要先撤销对应 Ticker。
	// If we've schedule the memory to be deleted we need to abort that.
	if (IsPendingDestroy())
	{
		if (!bDestroying)
		{
			UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] Destroy LoadingState (Canceled)"), this);
		}

		FTSTicker::GetCoreTicker().RemoveTicker(DestroyMemoryDelegate);
		DestroyMemoryDelegate.Reset();
	}
}

// 安排一次性 Ticker，在当前调用栈退出后从静态映射移除本状态；重复请求会被合并。
void FAsyncMixin::FLoadingState::RequestDestroyThisMemory()
{
	// 已有销毁请求时忽略重复调度。
	// If we're already pending to destroy this memory, just ignore.
	if (!IsPendingDestroy())
	{
		UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] Destroy LoadingState (Requested)"), this);

		DestroyMemoryDelegate = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float DeltaTime) {
			// 从映射移除宿主对应条目，并释放队列状态占用的临时内存。
			// Remove any memory we were using.
			FAsyncMixin::Loading.Remove(&OwnerRef);
			return false;
		}));
	}
}

// 撤销因调用方遗漏 StartAsyncLoading 而安排的下一帧兜底启动。
void FAsyncMixin::FLoadingState::CancelStartTimer()
{
	if (StartTimerDelegate.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(StartTimerDelegate);
		StartTimerDelegate.Reset();
	}
}

// 开始或继续处理队列：撤销兜底计时器，只在首次启动时通知宿主，然后尝试推进所有已完成步骤。
void FAsyncMixin::FLoadingState::Start()
{
	UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] Start (Current Progress %d/%d)"), this, CurrentAsyncStep + 1, AsyncSteps.Num());

	// 显式启动后撤销待执行的兜底启动请求。
	// Cancel any pending kickoff load requests.
	CancelStartTimer();

	bool bStartingStepFound = false;

	if (!bHasStarted)
	{
		bHasStarted = true;
		OwnerRef.OnStartedLoading();
	}
	
	TryCompleteAsyncLoading();
}

// 立即向 StreamableManager 发起单资源高优先级加载，把句柄和用户回调封装为队列步骤，并安排兜底启动。
void FAsyncMixin::FLoadingState::AsyncLoad(FSoftObjectPath SoftObjectPath, const FSimpleDelegate& DelegateToCall)
{
	UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] AsyncLoad '%s'"), this, *SoftObjectPath.ToString());

	AsyncSteps.Add(
		MakeUnique<FAsyncStep>(
			DelegateToCall,
			UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftObjectPath, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority, false, false, TEXT("AsyncMixin"))
			)
	);

	TryScheduleStart();
}

// 以一个流送句柄加载整组路径，使用户回调只在所有路径完成且轮到该步骤时执行。
void FAsyncMixin::FLoadingState::AsyncLoad(const TArray<FSoftObjectPath>& SoftObjectPaths, const FSimpleDelegate& DelegateToCall)
{
	{
		const FString& Paths = FString::JoinBy(SoftObjectPaths, TEXT(", "), [](const FSoftObjectPath& SoftObjectPath) { return FString::Printf(TEXT("'%s'"), *SoftObjectPath.ToString()); });
		UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] AsyncLoad [%s]"), this, *Paths);
	}

	AsyncSteps.Add(
		MakeUnique<FAsyncStep>(
			DelegateToCall,
			UAssetManager::GetStreamableManager().RequestAsyncLoad(SoftObjectPaths, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority, false, false, TEXT("AsyncMixin"))
			)
	);

	TryScheduleStart();
}

// 递归预加载主资产 Bundle；非空资产列表会设置驻留标记，阻止完成后自动释放持有流送句柄的状态。
void FAsyncMixin::FLoadingState::AsyncPreloadPrimaryAssetsAndBundles(const TArray<FPrimaryAssetId>& AssetIds, const TArray<FName>& LoadBundles, const FSimpleDelegate& DelegateToCall)
{
	{		
		const FString& Assets = FString::JoinBy(AssetIds, TEXT(", "), [](const FPrimaryAssetId& AssetId) { return AssetId.ToString(); });
		const FString& Bundles = FString::JoinBy(LoadBundles, TEXT(", "), [](const FName& LoadBundle) { return LoadBundle.ToString(); });
		UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX]  AsyncPreload Assets [%s], Bundles[%s]"), this, *Assets, *Bundles);
	}

	TSharedPtr<FStreamableHandle> StreamingHandle;

	if (AssetIds.Num() > 0)
	{
		bPreloadedBundles = true;

		const bool bLoadRecursive = true;
		StreamingHandle = UAssetManager::Get().PreloadPrimaryAssets(AssetIds, LoadBundles, bLoadRecursive);
	}

	AsyncSteps.Add(MakeUnique<FAsyncStep>(DelegateToCall, StreamingHandle));

	TryScheduleStart();
}

// 将共享条件对象加入队列，条件完成前由其自身的 Ticker 周期性检查。
void FAsyncMixin::FLoadingState::AsyncCondition(TSharedRef<FAsyncCondition> Condition, const FSimpleDelegate& DelegateToCall)
{
	UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] AsyncCondition '0x%llX'"), this, &Condition.Get());

	AsyncSteps.Add(MakeUnique<FAsyncStep>(DelegateToCall, Condition));

	TryScheduleStart();
}

// 添加不持有流送句柄或条件的步骤，使委托在序列推进到此处时立即执行。
void FAsyncMixin::FLoadingState::AsyncEvent(const FSimpleDelegate& DelegateToCall)
{
	UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] AsyncEvent"), this);

	AsyncSteps.Add(MakeUnique<FAsyncStep>(DelegateToCall));

	TryScheduleStart();
}

// 取消待销毁状态，并在尚未安排时注册下一帧一次性启动，保证遗漏显式启动的队列仍会运行。
void FAsyncMixin::FLoadingState::TryScheduleStart()
{
	CancelDestroyThisMemory(/*bDestroying*/false);

	// 调用方若忘记显式启动，队列会在下一帧自动开始。
	// In the event the user forgets to start async loading, we'll begin doing it next frame.
	if (!StartTimerDelegate.IsValid())
	{
		StartTimerDelegate = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float DeltaTime) {
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FAsyncMixin_FLoadingState_TryScheduleStartDelegate);
			Start();
			return false;
		}));
	}
}

// 根据当前步骤游标判断队列是否仍在加载；只要游标尚未到最后一步，后续步骤即使已完成也仍算进行中。
bool FAsyncMixin::FLoadingState::IsLoadingInProgress() const
{
	if (AsyncSteps.Num() > 0)
	{
		if (CurrentAsyncStep < AsyncSteps.Num())
		{
			if (CurrentAsyncStep == (AsyncSteps.Num() - 1))
			{
				return AsyncSteps[CurrentAsyncStep]->IsLoadingInProgress();
			}

			// 游标指向有效且非末尾步骤时，队列必然尚未结束；游标越界则表示没有任务或已经处理完毕。
			// If we know it's a valid index, but not the last one, then we know we're still loading,
			// if it's not a valid index, we know there's no loading, or we're beyond any loading.
			return true;
		}
	}

	return false;
}

// 同时考虑正在处理的步骤和等待下一帧启动的队列。
bool FAsyncMixin::FLoadingState::IsLoadingInProgressOrPending() const
{
	return StartTimerDelegate.IsValid() || IsLoadingInProgress();
}

// 通过一次性销毁 Ticker 的句柄判断该状态是否正在等待从映射移除。
bool FAsyncMixin::FLoadingState::IsPendingDestroy() const
{
	return DestroyMemoryDelegate.IsValid();
}

// 顺序推进所有已完成步骤；遇到首个未完成步骤时绑定唯一完成通知，队列清空后进入收尾流程。
void FAsyncMixin::FLoadingState::TryCompleteAsyncLoading()
{
	// 未处于启动状态却收到完成回调，说明队列已在同一帧或同一调用栈完成，应等待延迟销毁而不再操作状态。
	// If we haven't started when we get this callback it means we've already completed
	// and this is some other callback finishing on the same frame/stack that we need to avoid
	// doing anything with until the memory is finished being deleted.
	if (!bHasStarted)
	{
		return;
	}

	UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] TryCompleteAsyncLoading - (Current Progress %d/%d)"), this, CurrentAsyncStep + 1, AsyncSteps.Num());

	while (CurrentAsyncStep < AsyncSteps.Num())
	{
		FAsyncStep* Step = AsyncSteps[CurrentAsyncStep].Get();
		if (Step->IsLoadingInProgress())
		{
			if (!Step->IsCompleteDelegateBound())
			{
				UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] Step %d - Still Loading (Listening)"), this, CurrentAsyncStep + 1);
				const bool bBound = Step->BindCompleteDelegate(FSimpleDelegate::CreateSP(this, &FLoadingState::TryCompleteAsyncLoading));
				ensureMsgf(bBound, TEXT("This is not intended to return false.  We're checking if it's loaded above, this should definitely return true."));
			}
			else
			{
				UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] Step %d - Still Loading (Waiting)"), this, CurrentAsyncStep + 1);
			}

			break;
		}
		else
		{
			UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] Step %d - Completed (Calling User)"), this, CurrentAsyncStep + 1);

			// 必须先推进游标再执行用户回调，因为回调可能追加新任务并立即重启队列。
			// Always advance the CurrentAsyncStep, before calling the user callback, it's possible they might
			// add new work, and try and start again, so we need to be ready for the next bit.
			CurrentAsyncStep++;

			Step->ExecuteUserCallback();
		}
	}
	
	// 仅当队列确已完成且仍标记为已启动时收尾，避免用户回调重入 Start/TryComplete 后沿旧调用栈重复完成 N 次。
	// If we're done loading, and bHasStarted is still true (meaning this is the first time we're encountering a request to complete)
	// try and complete.  It's entirely possible that a user callback might append new work, which they immediately start, which
	// immediately tries to complete, which might create a case where we're now inside of TryCompleteAsyncLoading, which then
	// calls Start, which then calls TryCompleteAsyncLoading, so when we come back out of the stack, we need to avoid trying to
	// complete the async loading N+ times.
	if (IsLoadingComplete() && bHasStarted)
	{
		CompleteAsyncLoading();
	}
}

// 结束本轮队列、通知宿主，并依据是否需要保持预加载 Bundle 驻留决定释放或保留外置状态。
void FAsyncMixin::FLoadingState::CompleteAsyncLoading()
{
	UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] CompleteAsyncLoading"), this);

	// 清除启动标记并且只向宿主发送一次完成通知。
	// Mark that we've completed loading.
	if (bHasStarted)
	{
		bHasStarted = false;
		OwnerRef.OnFinishedLoading();
	}

	// OnFinishedLoading 可能追加并启动更多工作，因此通知返回后必须重新确认队列仍已完成。
	// It's unlikely but possible they started loading more stuff in the OnFinishedLoading callback,
	// so double check that we're still actually done.
	//
	// 注意：状态仍被使用时不能删除；预加载 Bundle 依赖流送句柄持续存活，所以此类状态需要保留。
	// NOTE: We don't delete ourselves from memory in use.  Doing things like
	// pre-loading a bundle requires keeping the streaming handle alive.  So we're keeping
	// things alive.
	// 
	// 即使保留状态，也必须清理可能捕获外部作用域的完成处理器，避免延长无关对象生命周期。
	// We won't destroy the memory but we need to cleanup anything that may be hanging on to
	// captured scope, like completion handlers.
	if (IsLoadingComplete())
	{
		if (!bPreloadedBundles && !IsLoadingInProgressOrPending())
		{
			// 普通加载已经完成且无待启动工作时，释放宿主为本轮队列分配的外置状态。
			// If we're all done loading or pending loading, we should clean up the memory we're using.
			// go ahead and remove this loading state the owner mix-in allocated.
			RequestDestroyThisMemory();
			return;
		}
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// 创建立即完成的事件步骤，仅保存按序执行的用户回调。
FAsyncMixin::FLoadingState::FAsyncStep::FAsyncStep(const FSimpleDelegate& InUserCallback)
	: UserCallback(InUserCallback)
{
}

// 创建资源加载步骤，持有流送句柄直至加载完成或取消。
FAsyncMixin::FLoadingState::FAsyncStep::FAsyncStep(const FSimpleDelegate& InUserCallback, const TSharedPtr<FStreamableHandle>& InStreamingHandle)
	: UserCallback(InUserCallback)
	, StreamingHandle(InStreamingHandle)
{
}

// 创建条件步骤，持有条件对象直到其满足或队列取消。
FAsyncMixin::FLoadingState::FAsyncStep::FAsyncStep(const FSimpleDelegate& InUserCallback, const TSharedPtr<FAsyncCondition>& InCondition)
	: UserCallback(InUserCallback)
	, Condition(InCondition)
{
}

// 释放步骤持有的委托、流送句柄或条件引用。
FAsyncMixin::FLoadingState::FAsyncStep::~FAsyncStep()
{

}

// 至多执行一次用户回调，并立即解绑以释放其捕获对象。
void FAsyncMixin::FLoadingState::FAsyncStep::ExecuteUserCallback()
{
	UserCallback.ExecuteIfBound();
	UserCallback.Unbind();
}

// 根据流送句柄或异步条件判断步骤完成状态；纯事件步骤始终可立即执行。
bool FAsyncMixin::FLoadingState::FAsyncStep::IsComplete() const
{
	if (StreamingHandle.IsValid())
	{
		return StreamingHandle->HasLoadCompleted();
	}
	else if (Condition.IsValid())
	{
		return Condition->IsComplete();
	}

	return true;
}

// 解除底层完成回调并释放等待对象，使取消后的步骤不再唤醒队列。
void FAsyncMixin::FLoadingState::FAsyncStep::Cancel()
{
	if (StreamingHandle.IsValid())
	{
		StreamingHandle->BindCompleteDelegate(FSimpleDelegate());
		StreamingHandle.Reset();
	}
	else if (Condition.IsValid())
	{
		Condition.Reset();
	}

	bIsCompletionDelegateBound = false;
}

// 为尚未完成的加载或条件绑定队列推进回调；若步骤已完成则返回 false，交由调用方立即继续检查。
bool FAsyncMixin::FLoadingState::FAsyncStep::BindCompleteDelegate(const FSimpleDelegate& NewDelegate)
{
	if (IsComplete())
	{
		// 步骤已经完成，此时再绑定通知为时已晚。
		// Too Late!
		return false;
	}

	if (StreamingHandle.IsValid())
	{
		StreamingHandle->BindCompleteDelegate(NewDelegate);
	}
	else if (Condition)
	{
		Condition->BindCompleteDelegate(NewDelegate);
	}

	bIsCompletionDelegateBound = true;

	return true;
}

// 返回当前步骤是否已经注册过完成通知，防止重复绑定。
bool FAsyncMixin::FLoadingState::FAsyncStep::IsCompleteDelegateBound() const
{
	return bIsCompletionDelegateBound;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// 保存调用方提供的条件委托，条件会在查询或 Ticker 轮询时执行。
FAsyncCondition::FAsyncCondition(const FAsyncConditionDelegate& Condition)
	: UserCondition(Condition)
{
}

// 将可移动函数封装成 Unreal 委托，以统一条件执行和生命周期管理。
FAsyncCondition::FAsyncCondition(TFunction<EAsyncConditionResult()>&& Condition)
	: UserCondition(FAsyncConditionDelegate::CreateLambda([UserFunction = MoveTemp(Condition)]() mutable { return UserFunction(); }))
{
}

// 销毁条件时注销周期性 Ticker，阻止后续轮询访问已释放对象。
FAsyncCondition::~FAsyncCondition()
{
	FTSTicker::GetCoreTicker().RemoveTicker(RepeatHandle);
}

// 立即求值用户条件；未绑定条件视为无需等待，直接完成。
bool FAsyncCondition::IsComplete() const
{
	if (UserCondition.IsBound())
	{
		const EAsyncConditionResult Result = UserCondition.Execute();
		return Result == EAsyncConditionResult::Complete;
	}

	return true;
}

// 为未满足的条件保存完成通知，并以 0.16 秒间隔启动轮询；已满足时返回 false。
bool FAsyncCondition::BindCompleteDelegate(const FSimpleDelegate& NewDelegate)
{
	if (IsComplete())
	{
		// 条件已经满足，不需要再注册完成通知。
		// Already Complete
		return false;
	}

	CompletionDelegate = NewDelegate;

	if (!RepeatHandle.IsValid())
	{
		RepeatHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FAsyncCondition::TryToContinue), 0.16f);
	}

	return true;
}

// 在 Ticker 中重新求值条件；满足后停止轮询、释放用户条件并通知等待该步骤的队列。
bool FAsyncCondition::TryToContinue(float)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAsyncCondition_TryToContinue);

	UE_LOG(LogAsyncMixin, Verbose, TEXT("[0x%llX] AsyncCondition::TryToContinue"), this);

	if (UserCondition.IsBound())
	{
		const EAsyncConditionResult Result = UserCondition.Execute();

		switch (Result)
		{
		case EAsyncConditionResult::TryAgain:
			return true;
		case EAsyncConditionResult::Complete:
			RepeatHandle.Reset();
			UserCondition.Unbind();

			CompletionDelegate.ExecuteIfBound();
			CompletionDelegate.Unbind();
			break;
		}
	}

	return false;
}
