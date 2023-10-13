// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "MyEditorUtilityWidget.generated.h"

/**
 * 
 */
UCLASS()
class MESHPROCESSOR_API UMyEditorUtilityWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void CombineStaticMeshes();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombineStaticMeshes")
	FString AssetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CombineStaticMeshes")
	class UDataTable* InfoTable;
};
