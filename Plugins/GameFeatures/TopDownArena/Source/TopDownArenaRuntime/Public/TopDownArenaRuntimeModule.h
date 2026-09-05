// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class FTopDownArenaRuntimeModule : public IModuleInterface
{
public:

	/** IModuleInterface 生命周期接口。 */
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
