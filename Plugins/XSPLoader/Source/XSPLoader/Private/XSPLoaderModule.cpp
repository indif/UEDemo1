// Copyright Epic Games, Inc. All Rights Reserved.
#include "XSPLoaderModule.h"
#include "XSPLoader.h"

#define LOCTEXT_NAMESPACE "FXSPLoaderModule"

static FXSPLoader GXSPLoader;

void FXSPLoaderModule::StartupModule()
{
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("XSPLoader"), 0.0f, [this](float DeltaTime)
		{
			GXSPLoader.Tick(DeltaTime);
			return true;
		});
}

void FXSPLoaderModule::ShutdownModule()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
}

IXSPLoader& FXSPLoaderModule::Get() const
{
	return GXSPLoader;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FXSPLoaderModule, XSPLoader)