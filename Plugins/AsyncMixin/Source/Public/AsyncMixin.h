// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Ticker.h"
#include "UObject/SoftObjectPtr.h"

#define UE_API ASYNCMIXIN_API

class FAsyncCondition;
class FName;
class UPrimaryDataAsset;
struct FPrimaryAssetId;
struct FStreamableHandle;
template <class TClass> class TSubclassOf;

DECLARE_DELEGATE_OneParam(FStreamableHandleDelegate, TSharedPtr<FStreamableHandle>)

// TODO：需要考虑引入资源驻留策略。当前预加载资源会一直驻留到显式取消，但通过 AsyncLoad 单独预加载资源时也可能需要控制驻留周期；相比为每次调用增加策略或拆分两套 API，更倾向于统一的驻留策略，尚需决定它应作为会增加实例内存的成员，还是模板参数。
//TODO I think we need to introduce a retention policy, preloads automatically stay in memory until canceled
//     but what if you want to preload individual items just using the AsyncLoad functions?  I don't want to
//     introduce individual policies per call, or introduce a whole set of preload vs asyncloads, so would
//     would rather have a retention policy.  Should it be a member and actually create real memory when
//     you inherit from AsyncMixin, or should it be a template argument?
//enum class EAsyncMixinRetentionPolicy : uint8
//{
//	Default,
//	KeepResidentUntilComplete,
//	KeepResidentUntilCancel
//};

/**
 * FAsyncMixin 用于组织一组异步依赖：资源加载可以立即发起，但完成回调始终按请求加入队列的顺序执行，从而让调用方按线性流程编写初始化代码。
 * 使用方继承该混入类，先取消对象复用前遗留的请求，再依次登记加载、条件或事件步骤，最后调用 StartAsyncLoading；若遗漏启动，系统会在下一帧兜底启动。
 * 所有步骤完成后触发 OnFinishedLoading。取消或宿主析构时会解除完成回调，因此 Lambda 可以安全捕获 this，不会在宿主生命周期结束后回调。
 * 每个实例的加载状态按需保存在静态映射中，混入类本身不增加实例大小；预加载 Bundle 的状态会保留句柄以维持资源驻留。
 * 可通过命令行参数 -LogCmds="LogAsyncMixin Verbose" 查看详细调试日志。
 */
/**
 * The FAsyncMixin allows easier management of async loading requests, to ensure linear request handling, to make 
 * writing code much easier.  The usage pattern is as follows,
 *
 * First - inherit from FAsyncMixin, even if you're a UObject, you can also inherit from FAsyncMixin.
 *
 * Then - you can make your async loads as follows.
 * 
 * CancelAsyncLoading();			// Some objects get reused like in lists, so it's important to cancel anything you had pending doesn't complete.
 * AsyncLoad(ItemOne, CallbackOne);
 * AsyncLoad(ItemTwo, CallbackTwo);
 * StartAsyncLoading();
 * 
 * You can also include the 'this' scope safely, one of the benefits of the mix-in, is that none of the callbacks
 * are ever out of scope of the host AsyncMixin derived object.
 * e.g.
 * AsyncLoad(SomeSoftObjectPtr, [this, ...]() {
 *    
 * });
 * 
 *
 * What will happen is first we cancel any existing one(s), e.g. perhaps we are a widget that just got told to represent
 * some new thing.  What will happen is we'll Load ItemOne and ItemTwo, *THEN* we'll call the callbacks in the order you
 * requested the async loads - even if ItemOne or ItemTwo was already loaded when you request it.
 *
 * When all the async loading requests complete, OnFinishedLoading will be called.
 * 
 * If you forget to call StartAsyncLoading(), we'll call it next frame, but you should remember to call it
 * when you're done with your setup, as maybe everything is already loaded, and it will avoid a single frame
 * of a loading indicator flash, which is annoying.
 * 
 * NOTE: The FAsyncMixin also makes it safe to pass [this] as a captured input into your lambda, because it handles 
 * unhooking everything if either your owner class is destroyed, or you cancel everything.
 *
 * NOTE: FAsyncMixin doesn't add any additional memory to your class.  Several classes currently handling async loading 
 * internally allocate TSharedPtr<FStreamableHandle> members and tend to hold onto SoftObjectPaths temporary state.  The 
 * FAsyncMixin does all of this internally with a static TMap so that all of the async request memory is stored temporarily
 * and sparsely.
 * 
 * NOTE: For debugging and understanding what's going on, you should add -LogCmds="LogAsyncMixin Verbose" to the command line.
 */
class FAsyncMixin : public FNoncopyable
{
protected:
	UE_API FAsyncMixin();

public:
	UE_API virtual ~FAsyncMixin();

protected:
	/** 队列首次开始处理时调用。 */
	/** Called when loading starts. */
	virtual void OnStartedLoading() { }
	/** 当前队列中的所有步骤均已完成时调用；回调中仍可追加并启动新步骤。 */
	/** Called when all loading has finished. */
	virtual void OnFinishedLoading() { }

protected:
	/** 异步加载软类引用，并在该步骤按队列顺序轮到时调用无参回调。 */
	/** Async load a TSoftClassPtr<T>, call the Callback when complete. */
	template<typename T = UObject>
	void AsyncLoad(TSoftClassPtr<T> SoftClass, TFunction<void()>&& Callback)
	{
		AsyncLoad(SoftClass.ToSoftObjectPath(), FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 异步加载软类引用，并将解析出的类传给回调；加载失败时传入的类可能为空。 */
	/** Async load a TSoftClassPtr<T>, call the Callback when complete. */
	template<typename T = UObject>
	void AsyncLoad(TSoftClassPtr<T> SoftClass, TFunction<void(TSubclassOf<T>)>&& Callback)
	{
		AsyncLoad(SoftClass.ToSoftObjectPath(),
			FSimpleDelegate::CreateLambda([SoftClass, UserCallback = MoveTemp(Callback)]() mutable {
				UserCallback(SoftClass.Get());
			})
		);
	}

	/** 异步加载软类引用，并在完成后执行委托。 */
	/** Async load a TSoftClassPtr<T>, call the Callback when complete. */
	template<typename T = UObject>
	void AsyncLoad(TSoftClassPtr<T> SoftClass, const FSimpleDelegate& Callback = FSimpleDelegate())
	{
		AsyncLoad(SoftClass.ToSoftObjectPath(), Callback);
	}

	/** 异步加载软对象引用，并在该步骤按队列顺序轮到时调用无参回调。 */
	/** Async load a TSoftObjectPtr<T>, call the Callback when complete. */
	template<typename T = UObject>
	void AsyncLoad(TSoftObjectPtr<T> SoftObject, TFunction<void()>&& Callback)
	{
		AsyncLoad(SoftObject.ToSoftObjectPath(), FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 异步加载软对象引用，并将解析出的对象传给回调；加载失败时对象指针可能为空。 */
	/** Async load a TSoftObjectPtr<T>, call the Callback when complete. */
	template<typename T = UObject>
	void AsyncLoad(TSoftObjectPtr<T> SoftObject, TFunction<void(T*)>&& Callback)
	{
		AsyncLoad(SoftObject.ToSoftObjectPath(),
			FSimpleDelegate::CreateLambda([SoftObject, UserCallback = MoveTemp(Callback)]() mutable {
				UserCallback(SoftObject.Get());
			})
		);
	}

	/** 异步加载软对象引用，并在完成后执行委托。 */
	/** Async load a TSoftObjectPtr<T>, call the Callback when complete. */
	template<typename T = UObject>
	void AsyncLoad(TSoftObjectPtr<T> SoftObject, const FSimpleDelegate& Callback = FSimpleDelegate())
	{
		AsyncLoad(SoftObject.ToSoftObjectPath(), Callback);
	}

	/** 异步加载单个软对象路径，并把完成委托作为一个有序步骤加入队列。 */
	/** Async load a FSoftObjectPath, call the Callback when complete. */
	UE_API void AsyncLoad(FSoftObjectPath SoftObjectPath, const FSimpleDelegate& Callback = FSimpleDelegate());

	/** 将一组软对象路径作为同一个异步步骤加载，整组完成后调用无参回调。 */
	/** Async load an array of FSoftObjectPath, call the Callback when complete. */
	void AsyncLoad(const TArray<FSoftObjectPath>& SoftObjectPaths, TFunction<void()>&& Callback)
	{
		AsyncLoad(SoftObjectPaths, FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 将一组软对象路径作为同一个异步步骤加载，整组完成后执行委托。 */
	/** Async load an array of FSoftObjectPath, call the Callback when complete. */
	UE_API void AsyncLoad(const TArray<FSoftObjectPath>& SoftObjectPaths, const FSimpleDelegate& Callback = FSimpleDelegate());

	/** 从主数据资产提取 PrimaryAssetId，并递归预加载 LoadBundles 指定的资产 Bundle。 */
	/** Given an array of primary assets, it loads all of the bundles referenced by properties of these assets specified in the LoadBundles array. */
	template<typename T = UPrimaryDataAsset>
	void AsyncPreloadPrimaryAssetsAndBundles(const TArray<T*>& Assets, const TArray<FName>& LoadBundles, const FSimpleDelegate& Callback = FSimpleDelegate())
	{
		TArray<FPrimaryAssetId> PrimaryAssetIds;
		for (const T* Item : Assets)
		{
			PrimaryAssetIds.Add(Item);
		}

		AsyncPreloadPrimaryAssetsAndBundles(PrimaryAssetIds, LoadBundles, Callback);
	}

	/** 递归预加载指定主资产 ID 的 Bundle，并在资源驻留后调用无参回调。 */
	/** Given an array of primary asset ids, it loads all of the bundles referenced by properties of these assets specified in the LoadBundles array. */
	void AsyncPreloadPrimaryAssetsAndBundles(const TArray<FPrimaryAssetId>& AssetIds, const TArray<FName>& LoadBundles, TFunction<void()>&& Callback)
	{
		AsyncPreloadPrimaryAssetsAndBundles(AssetIds, LoadBundles, FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 递归预加载指定主资产 ID 的 Bundle；加载状态会保留流送句柄，使 Bundle 持续驻留到取消。 */
	/** Given an array of primary asset ids, it loads all of the bundles referenced by properties of these assets specified in the LoadBundles array. */
	UE_API void AsyncPreloadPrimaryAssetsAndBundles(const TArray<FPrimaryAssetId>& AssetIds, const TArray<FName>& LoadBundles, const FSimpleDelegate& Callback = FSimpleDelegate());

	/** 向队列加入一个异步条件；条件返回 Complete 前不会推进到后续步骤。 */
	/** Add a future condition that must be true before we move forward. */
	UE_API void AsyncCondition(TSharedRef<FAsyncCondition> Condition, const FSimpleDelegate& Callback = FSimpleDelegate());

	/** 将纯事件回调插入有序步骤序列，不绑定任何具体资产；适合可选资产存在与否都必须执行的流程节点。 */
	/**
	 * Rather than load anything, this callback is just inserted into the callback sequence so that when async loading 
	 * completes this event will be called at the same point in the sequence.  Super useful if you don't want a step to be
	 * tied to a particular asset in case some of the assets are optional.
	 */
	void AsyncEvent(TFunction<void()>&& Callback)
	{
		AsyncEvent(FSimpleDelegate::CreateLambda(MoveTemp(Callback)));
	}

	/** 将委托作为纯事件步骤插入序列，轮到该步骤时直接执行。 */
	/**
	 * Rather than load anything, this callback is just inserted into the callback sequence so that when async loading
	 * completes this event will be called at the same point in the sequence.  Super useful if you don't want a step to be
	 * tied to a particular asset in case some of the assets are optional.
	 */
	UE_API void AsyncEvent(const FSimpleDelegate& Callback);

	/** 启动并推进当前已登记的异步步骤；没有步骤时仍同步触发开始和结束通知。 */
	/** Flushes any async loading requests. */
	UE_API void StartAsyncLoading();

	/** 取消所有待处理步骤并安排释放该实例对应的外置加载状态。 */
	/** Cancels any pending async loads. */
	UE_API void CancelAsyncLoading();

	/** 返回当前是否仍有已启动但尚未完成的步骤；仅等待下一帧启动的队列不计为进行中。 */
	/** Is async loading current in progress? */
	UE_API bool IsAsyncLoadingInProgress() const;

private:
	/** FLoadingState 是按需分配的实际队列状态，存放在 FAsyncMixin 的静态映射中，因此未使用异步功能的宿主不会承担实例内存开销。 */
	/**
	 * The FLoadingState is what actually is allocated for the FAsyncMixin in a big map so that the FAsyncMixin itself holds no
	 * no memory, and we dynamically create the FLoadingState only if needed, and destroy it when it's unneeded.
	 */
	class FLoadingState : public TSharedFromThis<FLoadingState>
	{
	public:
		FLoadingState(FAsyncMixin& InOwner);
		virtual ~FLoadingState();

		/** 取消兜底启动计时器，发出开始通知并推进异步序列。 */
		/** Starts the async sequence. */
		void Start();

		/** 取消队列中的请求并延迟移除状态，避免在当前回调栈内销毁自身。 */
		/** Cancels the async sequence. */
		void CancelAndDestroy();

		void AsyncLoad(FSoftObjectPath SoftObject, const FSimpleDelegate& DelegateToCall);
		void AsyncLoad(const TArray<FSoftObjectPath>& SoftObjectPaths, const FSimpleDelegate& DelegateToCall);
		void AsyncPreloadPrimaryAssetsAndBundles(const TArray<FPrimaryAssetId>& PrimaryAssetIds, const TArray<FName>& LoadBundles, const FSimpleDelegate& DelegateToCall);
		void AsyncCondition(TSharedRef<FAsyncCondition> Condition, const FSimpleDelegate& Callback);
		void AsyncEvent(const FSimpleDelegate& Callback);

		bool IsLoadingComplete() const { return !IsLoadingInProgress(); }
		bool IsLoadingInProgress() const;
		bool IsLoadingInProgressOrPending() const;
		bool IsPendingDestroy() const;

	private:
		void CancelOnly(bool bDestroying);
		void CancelStartTimer();
		void TryScheduleStart();
		void TryCompleteAsyncLoading();
		void CompleteAsyncLoading();

	private:
		void RequestDestroyThisMemory();
		void CancelDestroyThisMemory(bool bDestroying);

		/** 加载状态所属的混入对象，用于触发宿主的开始和结束通知，并作为静态映射键。 */
		/** Who owns the loading state?  We need this to call back into the owning mix-in object. */
		FAsyncMixin& OwnerRef;

		/** 标记队列是否预加载过 Bundle；预加载句柄必须保持存活，因此完成后不能像普通加载那样立即释放状态。 */
		/**
		 * Did we need to pre-load bundles?  If we didn't pre-load bundles (which require you keep the streaming handle 
		 * around or they will be destroyed), then we can safely destroy the FLoadingState when everything is done loading.
		 */
		bool bPreloadedBundles = false;

		class FAsyncStep
		{
		public:
			FAsyncStep(const FSimpleDelegate& InUserCallback);
			FAsyncStep(const FSimpleDelegate& InUserCallback, const TSharedPtr<FStreamableHandle>& InStreamingHandle);
			FAsyncStep(const FSimpleDelegate& InUserCallback, const TSharedPtr<FAsyncCondition>& InCondition);

			~FAsyncStep();

			void ExecuteUserCallback();

			bool IsLoadingInProgress() const
			{
				return !IsComplete();
			}

			bool IsComplete() const;
			void Cancel();

			bool BindCompleteDelegate(const FSimpleDelegate& NewDelegate);
			bool IsCompleteDelegateBound() const;

		private:
			FSimpleDelegate UserCallback;
			bool bIsCompletionDelegateBound = false;

			// 当前步骤至多等待一种异步来源：流送句柄或轮询条件；两者都为空时该步骤是立即完成的事件。
			// Possible Async 'thing'
			TSharedPtr<FStreamableHandle> StreamingHandle;
			TSharedPtr<FAsyncCondition> Condition;
		};

		bool bHasStarted = false;

		int32 CurrentAsyncStep = 0;
		TArray<TUniquePtr<FAsyncStep>> AsyncSteps;
		TArray<TUniquePtr<FAsyncStep>> AsyncStepsPendingDestruction;

		FTSTicker::FDelegateHandle StartTimerDelegate;
		FTSTicker::FDelegateHandle DestroyMemoryDelegate;
	};

	UE_API const FLoadingState& GetLoadingStateConst() const;
	
	UE_API FLoadingState& GetLoadingState();

	UE_API bool HasLoadingState() const;

	UE_API bool IsLoadingInProgressOrPending() const;

private:
	static UE_API TMap<FAsyncMixin*, TSharedRef<FLoadingState>> Loading;
};

/**
 * 当一个对象需要并行管理多条彼此独立的异步依赖链时，可创建多个 FAsyncScope，而不必让对象本身继承多份混入状态。
 * 每个 Scope 都提供与 FAsyncMixin 相同的有序加载、条件、事件、启动和取消接口，并拥有独立生命周期。
 */
/**
 * Sometimes a mix-in just doesn't make sense.  Perhaps the object has to manage many different jobs
 * that each have their own async dependency chain/scope.  For those situations you can use the FAsyncScope.
 * 
 * This class is a standalone Async dependency handler so that you can fire off several load jobs and always handle them
 * in the proper order, just like with combining FAsyncMixin with your class.
 */
class FAsyncScope : public FAsyncMixin
{
public:
	using FAsyncMixin::AsyncLoad;

	using FAsyncMixin::AsyncPreloadPrimaryAssetsAndBundles;

	using FAsyncMixin::AsyncCondition;

	using FAsyncMixin::AsyncEvent;

	using FAsyncMixin::CancelAsyncLoading;

	using FAsyncMixin::StartAsyncLoading;

	using FAsyncMixin::IsAsyncLoadingInProgress;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

enum class EAsyncConditionResult : uint8
{
	TryAgain,
	Complete
};

DECLARE_DELEGATE_RetVal(EAsyncConditionResult, FAsyncConditionDelegate);

/** 在异步步骤之间插入可重复检查的自定义条件；条件返回 Complete 前，Ticker 会周期性重试并阻止队列继续推进。 */
/**
 * The async condition allows you to have custom reasons to hault the async loading until some condition is met.
 */
class FAsyncCondition : public TSharedFromThis<FAsyncCondition>
{
public:
	FAsyncCondition(const FAsyncConditionDelegate& Condition);
	FAsyncCondition(TFunction<EAsyncConditionResult()>&& Condition);
	virtual ~FAsyncCondition();

protected:
	bool IsComplete() const;
	bool BindCompleteDelegate(const FSimpleDelegate& NewDelegate);

private:
	bool TryToContinue(float DeltaTime);

	FTSTicker::FDelegateHandle RepeatHandle;
	FAsyncConditionDelegate UserCondition;
	FSimpleDelegate CompletionDelegate;

	friend FAsyncMixin;
};

#undef UE_API
