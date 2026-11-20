// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

WCWSNAPJUMP_API DECLARE_LOG_CATEGORY_EXTERN(LogWcwSnapJump, Log, All);

class FWcwSnapJumpModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
