// Fill out your copyright notice in the Description page of Project Settings.


#include "MyAssetActionUtility.h"
#include "MeshProcessor.h"

void UMyAssetActionUtility::CombineStaticMeshes(FString OutAssetName, class UDataTable* OutInfoTable)
{
    ::CombineStaticMeshes(this, OutAssetName, OutInfoTable);
}