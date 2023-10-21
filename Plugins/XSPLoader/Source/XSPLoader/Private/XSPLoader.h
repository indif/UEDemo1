// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IXSPLoader.h"
#include <fstream>

struct Body_info
{
	int dbid;  //结构体的索引就是dbid 从0开始
	int parentdbid;      //parent db id
	short level;    //node 所在的节点层级 从0开始
	std::string name;   //fragment/node name
	std::string property;   //节点属性
	float material[4];  //材质
	float box[6];      //min max
	std::vector<float> vertices;
	TArray<Body_info> fragment;
};

struct Header_info
{
	//int dbid;  //结构体的索引就是dbid 从0开始
	short empty_fragment;  //1为有fragment 2为没有fragment
	int parentdbid;      //parent db id
	short level;    //node 所在的节点层级 从0开始
	int startname;   //节点名称开始索引
	int namelength;   //节点名称字符大小
	int startproperty;  //节点属性开始索引
	int propertylength;  //节点属性字符大小
	int startmaterial;  //材质属性开始索引，固定16个字符，一次是R(4) G(4) B(4) roughnessFactor(4)
	int startbox;   //box开始索引
	int startvertices;  //vertices开始索引
	int verticeslength;  //vertices头文件大小
};

struct FStaticMeshRequest
{
	int32 Dbid;
	float Priority;
	uint64 LastUpdateFrameNumber;
	bool bValid;
	FLinearColor Color;
	float Roughness;
	TWeakObjectPtr<UStaticMeshComponent> TargetComponent;
	TStrongObjectPtr<UStaticMesh> StaticMesh;

	FStaticMeshRequest(int32 InDbid, float InPriority, TWeakObjectPtr<UStaticMeshComponent>& InTargetComponent)
		: Dbid(InDbid)
		, Priority(InPriority)
		, LastUpdateFrameNumber(-1)
		, bValid(true)
		, TargetComponent(InTargetComponent)
	{}

	void Invalidate();

	bool IsValid() { return bValid; }

	bool IsRequestCurrent(uint64 FrameNumber)
	{
		return bValid && (FrameNumber - LastUpdateFrameNumber <= 1);
	}
};

struct FSortRequestFunctor
{
	bool operator() (const TSharedPtr<FStaticMeshRequest>& Lhs, const TSharedPtr<FStaticMeshRequest>& Rhs) const
	{
		if (Lhs->LastUpdateFrameNumber > Rhs->LastUpdateFrameNumber) return true;
		else if (Lhs->LastUpdateFrameNumber < Rhs->LastUpdateFrameNumber) return false;
		else return (Lhs->Priority > Rhs->Priority);
	}
};

struct FRequestQueue
{
public:
	void Add(TSharedPtr<FStaticMeshRequest>& Request);

	void AddNoLock(TSharedPtr<FStaticMeshRequest>& Request);

	void TakeFirst(TSharedPtr<FStaticMeshRequest>& Request);

	bool IsEmpty();

	typedef TArray<TSharedPtr<FStaticMeshRequest>> FRequestList;
	void Swap(FRequestList& RequestList);

	FRequestList RequestList;
	FCriticalSection RequestListCS;

	class FXSPLoader* Loader;
};

class FBuildStaticMeshTask : public FNonAbandonableTask
{
public:
	FBuildStaticMeshTask(TSharedPtr<FStaticMeshRequest> InRequest, Body_info& InNodeData, FRequestQueue& MergeQueue)
		: Request(InRequest)
		, NodeData(InNodeData)
		, MergeRequestQueue(MergeQueue)
	{
	}

	void DoWork();

	TStatId GetStatId() const
	{
		return TStatId();
	}

private:
	TSharedPtr<FStaticMeshRequest> Request;
	Body_info& NodeData;
	FRequestQueue& MergeRequestQueue;
};

class FXSPFileLoadRunnalbe : public FRunnable
{
public:
	FXSPFileLoadRunnalbe(std::fstream& InFileStream, int32 InStartDbid, int32 InCount, FRequestQueue& LoadQueue, FRequestQueue& MergeQueue)
		: FileStream(InFileStream)
		, StartDbid(InStartDbid)
		, Count(InCount)
		, LoadRequestQueue(LoadQueue)
		, MergeRequestQueue(MergeQueue)
	{}

	virtual bool Init() override
	{
		bIsRunning = true;
		return true;
	}
	virtual uint32 Run() override;
	virtual void Stop() override
	{
		bStopRequested = true;
	}
	virtual void Exit() override
	{
		bIsRunning = false;
	}

private:
	TAtomic<bool> bIsRunning;
	TAtomic<bool> bStopRequested;

	std::fstream& FileStream;
	int32 StartDbid;
	int32 Count;
	FRequestQueue& LoadRequestQueue;
	FRequestQueue& MergeRequestQueue;
	TArray<Header_info> HeaderList;
	TMap<int32, Body_info> BodyMap;
};

class FXSPLoader : public IXSPLoader
{
public:
	FXSPLoader();
	virtual ~FXSPLoader();

	virtual bool Init(const TArray<FString>& FilePathNameArray) override;
	virtual void RequestStaticMeshe(int32 Dbid, float Priority, TWeakObjectPtr<UStaticMeshComponent>& TargetMeshComponent) override;

	void Tick(float DeltaTime);

	void Reset();

private:
	void DispatchNewRequests(uint64 InFrameNumber);
	void ProcessMergeRequests();

private:
	bool bInitialized = false;
	std::atomic<uint64> FrameNumber;

	//每个源文件对应一个读取线程和一个请求队列
	struct FSourceData
	{
		int32 StartDbid;
		int32 Count;
		std::fstream FileStream;
		FXSPFileLoadRunnalbe* FileLoadRunnable = nullptr;
		FRunnableThread* LoadThread = nullptr;
		FRequestQueue LoadRequestQueue;
	};
	TArray<FSourceData*> SourDataList;

	// 材质模板
	TStrongObjectPtr<UMaterialInterface> SourceMaterial;

	FRequestQueue MergeRequestQueue;

	/**
	Request的生命周期:
	1.在FXSPLoader::Tick中被创建,根据dbid投入到相应文件对应的LoadRequestQueue	--Game线程
	2.在FXSPFileLoadRunnalbe::Run中被从LoadRequestQueue中取出,(读取节点数据后)填充材质数据,与节点数据一起被封装为一个构建任务分发到线程池	--每个文件对应一个工作线程
	3.在FBuildStaticMeshTask::DoWork中完成网格体构建后,被投入到全局的MergeRequestQueue	--线程池任意线程
	4.在FXSPLoader::Tick中被从MergeRequestQueue中取出,将静态网格设置给组件对象,之后Request被销毁	--Game线程
	在整个声明周期中,无论Request如何流转,AllRequestMap一直持有Request,用于Request状态的更新(线程同步?)
	*/

	//全部请求的Map,只在Game线程访问
	TMap<int32, TSharedPtr<FStaticMeshRequest>> AllRequestMap;

	//收集新请求的缓存数组,由外部调用线程和Game线程访问
	TArray<TSharedPtr<FStaticMeshRequest>> CachedRequestArray;
	FCriticalSection CachedRequestArrayCS;

	FCriticalSection RequestCS;
};
