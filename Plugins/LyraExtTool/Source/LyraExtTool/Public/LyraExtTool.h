// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class FLyraExtToolModule : public IModuleInterface
{
public:

	/** IModuleInterface 生命周期实现。 */
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
