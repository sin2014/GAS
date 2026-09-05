// Copyright Epic Games, Inc.All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS && WITH_AUTOMATION_DRIVER

#include "Misc/AutomationTest.h"
#include "AutomationDriverTypeDefs.h"
#include "IAutomationDriver.h"
#include "IAutomationDriverModule.h"
#include "IDriverElement.h"
#include "LocateBy.h"

BEGIN_DEFINE_SPEC(FMenuStartEliminationSpec, "Lyra.MenuStartEliminationSpec",
                  EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
	FAutomationDriverPtr Driver;

	void ClickButton(const FString& Text) const;
END_DEFINE_SPEC(FMenuStartEliminationSpec)

// 通过 AutomationDriver 查找指定文本按钮，等待可交互后点击。
void FMenuStartEliminationSpec::ClickButton(const FString& Text) const
{
	const auto Button = Driver->FindElement(By::TextFilter::Contains(By::Path("<SCommonButton>"), Text));
	Driver->Wait(Until::ElementIsInteractable(Button, FWaitTimeout::InSeconds(60)));
	Button->Click();
}

// 定义从主菜单启动 Elimination 的自动化步骤，并在测试前后重置 AutomationDriver。
void FMenuStartEliminationSpec::Define()
{
	BeforeEach([this]()
	{
		if (IAutomationDriverModule::Get().IsEnabled())
		{
			// 若 AutomationDriver 被前一个测试遗留为启用状态，则先禁用以强制重置。
			// Check if the Driver was left enabled by a previous test
			// Disable it in this case to force a reset
			IAutomationDriverModule::Get().Disable();
		}
		IAutomationDriverModule::Get().Enable();

		Driver = IAutomationDriverModule::Get().CreateDriver();
	});

	AfterEach([this]()
	{
		Driver.Reset(); /* 释放引用，使 AutomationDriver 可以销毁。 */ /* remove reference to allow destruction */
		IAutomationDriverModule::Get().Disable();
	});

	Describe("Menu Start Elimination", [this]()
	{
		It("Should click buttons to Start Elimination", EAsyncExecution::ThreadPool, [this]()
		{
			ClickButton("Play Lyra");

			ClickButton("Start a Game");

			ClickButton("Elimination");
		});
	});
}

#endif
