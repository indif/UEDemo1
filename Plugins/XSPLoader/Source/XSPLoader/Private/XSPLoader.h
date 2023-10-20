// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IXSPLoader.h"



class FXSPLoader : public IXSPLoader
{
public:
	FXSPLoader();
	virtual ~FXSPLoader();

	virtual void SetSourceFiles(const TArray<FString>& FilePathNameArray) override;
	virtual void RequestStaticMeshe(int32 Dbid, float Priority, TWeakObjectPtr<UStaticMeshComponent>& TargetMeshComponent) override;

	void Tick(float DeltaTime);
};
