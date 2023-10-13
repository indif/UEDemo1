// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FMeshProcessorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

//合并选中的静态网格资源
void CombineStaticMeshes(UObject* Outer, FString OutAssetName, class UDataTable* OutInfoTable);