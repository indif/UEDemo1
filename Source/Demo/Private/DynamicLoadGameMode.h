// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DynamicLoadGameMode.generated.h"

/**
 * 
 */
UCLASS()
class ADynamicLoadGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ADynamicLoadGameMode();

	virtual void BeginPlay() override;
	virtual void Logout(AController* Exiting) override;
	virtual void Tick(float deltaSeconds) override;

private:
	UPROPERTY()
	AActor* DataActor;

	UPROPERTY()
	TMap<int32, UStaticMeshComponent*> StaticMeshComponents;

	TArray<int32> NodesNumArray;
};
