// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameSession.h"

#include "LyraGameSession.generated.h"

class UObject;


UCLASS(Config = Game)
class ALyraGameSession : public AGameSession
{
	GENERATED_BODY()

public:

	ALyraGameSession(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:

	/** 覆盖并禁用 AGameSession 默认自动登录流程，改由 LyraGameMode 处理专用服务器登录。 */
	/** Override to disable the default behavior */
	virtual bool ProcessAutoLogin() override;

	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
};
