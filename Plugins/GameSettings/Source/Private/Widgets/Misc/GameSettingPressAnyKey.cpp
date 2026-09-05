// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Misc/GameSettingPressAnyKey.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameSettingPressAnyKey)

class ICursor;

class FSettingsPressAnyKeyInputPreProcessor : public IInputProcessor
{
public:
	// 构造按键捕获输入预处理器；事件委托由激活界面随后绑定。
	FSettingsPressAnyKeyInputPreProcessor()
	{

	}

	// 输入捕获不需要逐帧状态更新，保留空 Tick 以满足输入处理器接口。
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override { }

	// 在按键释放时确认选择并吞掉事件，避免该按键继续传给下层界面。
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		HandleKey(InKeyEvent.GetKey());
		return true;
	}

	// 吞掉按键按下事件，实际选择延迟到释放阶段，避免触发重复输入。
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		return true;
	}

	// 将鼠标双击使用的按钮作为候选按键处理，并阻止事件继续传播。
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		HandleKey(MouseEvent.GetEffectingButton());
		return true;
	}

	// 吞掉鼠标按下事件，等待释放后再确认按钮选择。
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		return true;
	}

	// 在鼠标按钮释放时确认选择，并阻止本次点击作用于下层控件。
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		HandleKey(MouseEvent.GetEffectingButton());
		return true;
	}

	// 将有效滚轮增量转换为向上或向下的虚拟按键，并吞掉滚轮或手势事件。
	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override
	{
		if (InWheelEvent.GetWheelDelta() != 0)
		{
			const FKey Key = InWheelEvent.GetWheelDelta() < 0 ? EKeys::MouseScrollDown : EKeys::MouseScrollUp;
			HandleKey(Key);
		}
		return true;
	}

	DECLARE_MULTICAST_DELEGATE(FSettingsPressAnyKeyInputPreProcessorCanceled);
	FSettingsPressAnyKeyInputPreProcessorCanceled OnKeySelectionCanceled;

	DECLARE_MULTICAST_DELEGATE_OneParam(FSettingsPressAnyKeyInputPreProcessorKeySelected, FKey);
	FSettingsPressAnyKeyInputPreProcessorKeySelected OnKeySelected;

private:
	// 根据按键类型广播选择或取消；Command 修饰键本身被忽略，避免误绑定系统组合键前缀。
	void HandleKey(const FKey& Key)
	{
		// Escape、触摸或手柄按键用于取消本次按键捕获。
		// Cancel this process if it's Escape, Touch, or a gamepad key.
		if (Key == EKeys::LeftCommand || Key == EKeys::RightCommand)
		{
			// 忽略 Command 修饰键本身。
			// Ignore
		}
		else if (Key == EKeys::Escape || Key.IsTouch() || Key.IsGamepadKey())
		{
			OnKeySelectionCanceled.Broadcast();
		}
		else
		{
			OnKeySelected.Broadcast(Key);
		}
	}
};

// 创建按键捕获界面；输入预处理器仅在界面激活期间按需注册。
UGameSettingPressAnyKey::UGameSettingPressAnyKey(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

// 激活按键捕获界面，注册输入预处理器并把确认回调指向按键选择处理。
void UGameSettingPressAnyKey::NativeOnActivated()
{
	Super::NativeOnActivated();

	bKeySelected = false;

	InputProcessor = MakeShared<FSettingsPressAnyKeyInputPreProcessor>();
	InputProcessor->OnKeySelected.AddUObject(this, &ThisClass::HandleKeySelected);
	InputProcessor->OnKeySelectionCanceled.AddUObject(this, &ThisClass::HandleKeySelectionCanceled);
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
}

// 停用界面时注销输入预处理器，防止关闭后继续截获输入。
void UGameSettingPressAnyKey::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
}

// 注销输入捕获并广播选中的按键，然后延迟一帧关闭界面。
void UGameSettingPressAnyKey::HandleKeySelected(FKey InKey)
{
	if (!bKeySelected)
	{
		bKeySelected = true;
		Dismiss([this, InKey]() {
			OnKeySelected.Broadcast(InKey);
		});
	}
}

// 注销输入捕获、广播取消事件，并在当前输入处理结束后关闭界面。
void UGameSettingPressAnyKey::HandleKeySelectionCanceled()
{
	if (!bKeySelected)
	{
		bKeySelected = true;
		Dismiss([this]() {
			OnKeySelectionCanceled.Broadcast();
		});
	}
}

// 延迟一个 Tick 再停用界面，随后执行可选回调，避免在输入分发中同步销毁。
void UGameSettingPressAnyKey::Dismiss(TFunction<void()> PostDismissCallback)
{
	// 延迟到下一 Tick 再关闭，确保当前输入事件已经处理完毕。
	// We delay a tick so that we're done processing input.
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, PostDismissCallback](float DeltaTime)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameSettingPressAnyKey_Dismiss);

		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);

		DeactivateWidget();

		PostDismissCallback();

		return false;
	}));
}
