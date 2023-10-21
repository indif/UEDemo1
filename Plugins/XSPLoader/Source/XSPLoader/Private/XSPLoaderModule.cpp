// Copyright Epic Games, Inc. All Rights Reserved.
#include "XSPLoaderModule.h"
#include "XSPLoader.h"

#define LOCTEXT_NAMESPACE "FXSPLoaderModule"


void FXSPLoaderModule::StartupModule()
{
	FXSPLoader* TempXSPLoader = new FXSPLoader;
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("XSPLoader"), 0.0f, [TempXSPLoader](float DeltaTime)
		{
			TempXSPLoader->Tick(DeltaTime);
			return true;
		});
	XSPLoader = TempXSPLoader;
}

void FXSPLoaderModule::ShutdownModule()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	delete XSPLoader;
	XSPLoader = nullptr;
}

IXSPLoader& FXSPLoaderModule::Get() const
{
	return *XSPLoader;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FXSPLoaderModule, XSPLoader)