// Copyright Epic Games, Inc.All Rights Reserved.

#include "Tests/LyraTestControllerBootTest.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraTestControllerBootTest)

// 首次调用记录启动时间，等待固定延迟后才认为 BootTest 完成，避免 Gauntlet 尚未恢复焦点就结束。
bool ULyraTestControllerBootTest::IsBootProcessComplete() const
{
	static double StartTime = FPlatformTime::Seconds();
	const double TimeSinceStart = FPlatformTime::Seconds() - StartTime;

	if (TimeSinceStart >= TestDelay)
	{
		return true;
//@TODO：补充有实际覆盖价值的启动状态验证。
//@TODO: actually do some useful testing here
// 		if (const UWorld* World = GetWorld())
// 		{
// 			if (const ULyraGameInstance* GameInstance = Cast<ULyraGameInstance>(GetWorld()->GetGameInstance()))
// 			{
// 				if (GameInstance->GetCurrentState() == ShooterGameInstanceState::WelcomeScreen ||
// 					GameInstance->GetCurrentState() == ShooterGameInstanceState::MainMenu)
// 				{
// 					return true;
// 				}
// 			}
// 		}
	}

	return false;
}
