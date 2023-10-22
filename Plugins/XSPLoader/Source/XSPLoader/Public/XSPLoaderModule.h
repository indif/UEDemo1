// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "IXSPLoader.h"

class FXSPLoaderModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	XSPLOADER_API IXSPLoader& Get() const;

private:
	FTSTicker::FDelegateHandle TickerHandle;
	IXSPLoader* XSPLoader = nullptr;
};
