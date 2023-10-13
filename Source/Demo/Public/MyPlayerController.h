// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MyPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class DEMO_API AMyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	//封装这个拾取函数以便能够在HitResult中返回FaceIndex
	UFUNCTION(BlueprintCallable, Category = "Game|Player")
	bool GetHitResultWithFaceIndexUnderCursorByChannel(ETraceTypeQuery TraceChannel, FHitResult& HitResult) const;
};
