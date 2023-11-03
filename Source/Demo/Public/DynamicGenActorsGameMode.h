// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DynamicGenActorsGameMode.generated.h"

/**
 * 
 */
UCLASS()
class DEMO_API ADynamicGenActorsGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ADynamicGenActorsGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float deltaSeconds) override;

private:
	// 材质模板
	UPROPERTY()
	class UMaterialInterface* SourceMaterial;

	// 动态加载的Actor
	UPROPERTY()
	AActor* DataActor;

	// 确保异步构建过程中不被GC
	UPROPERTY()
	TArray<UStaticMesh*> StaticMeshList;

	//加载阶段
	enum class ELoadPhase
	{
		LP_NotStart,
		LP_LoadingFile,
		LP_LoadingScene,
		LP_Finished
	};
	ELoadPhase CurrentLoadPhase = ELoadPhase::LP_NotStart;

	//节点数据
	std::vector<struct Body_info*> NodeDataList;

	friend class FLoadFileTask;
	class FLoadFileTask : public FNonAbandonableTask
	{
	public:
		FLoadFileTask(const FString& InFilePathName, std::vector<struct Body_info*>& InNodeDataList)
			: FilePathName(InFilePathName)
			, NodeDataList(InNodeDataList)
		{}

		void DoWork();

		TStatId GetStatId() const
		{
			return TStatId();
		}

		bool bSucceed = false;
	private:
		FString FilePathName;
		std::vector<struct Body_info*>& NodeDataList;
	};
	FAsyncTask<class FLoadFileTask>* AsyncLoadFileTask = nullptr;


	int32 NumValidNodes = 0;
	int32 NumLoadedNodes = 0;
	int32 NumTotoalTriangles = 0;

	// 存放构建完成的静态网格对象及相关数据
	struct FLoadedData
	{
		FName Name;
		UStaticMesh* StaticMesh;
		FLinearColor Color;
		float Roughness;
		int32 NumTriangles = 0;
	};

	// 多生产者单消费者的无锁队列
	TQueue<FLoadedData, EQueueMode::Mpsc> LoadedNodes;
	friend class FBuildStaticMeshTask;

private:
	void LoadScene();
	void AddToScene(FLoadedData* LoadedData);
};
