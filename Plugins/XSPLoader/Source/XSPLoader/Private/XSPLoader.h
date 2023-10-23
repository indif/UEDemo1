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
	UStaticMeshComponent* TargetComponent;
	TStrongObjectPtr<UStaticMesh> StaticMesh;
	std::atomic_bool bReleasable;

	FStaticMeshRequest(int32 InDbid, float InPriority, UStaticMeshComponent* InTargetComponent)
		: Dbid(InDbid)
		, Priority(InPriority)
		, LastUpdateFrameNumber(-1)
		, bValid(true)
		, Color(1,1,1)
		, Roughness(1)
		, TargetComponent(InTargetComponent)
		, bReleasable(false)
	{}

	void Invalidate();

	bool IsValid() { return bValid; }

	void SetReleasable() { bReleasable.store(true); }

	void ResetReleasable() { bReleasable.store(false); }

	bool IsReleasable() { return bReleasable; }

	bool IsRequestCurrent(uint64 FrameNumber);
};

struct FSortRequestFunctor
{
	bool operator() (FStaticMeshRequest* Lhs, FStaticMeshRequest* Rhs) const
	{
		if (Lhs->LastUpdateFrameNumber > Rhs->LastUpdateFrameNumber) return true;
		else if (Lhs->LastUpdateFrameNumber < Rhs->LastUpdateFrameNumber) return false;
		else return (Lhs->Priority > Rhs->Priority);
	}
};

struct FRequestQueue
{
public:
	void Add(FStaticMeshRequest* Request);

	void AddNoLock(FStaticMeshRequest* Request);

	void TakeFirst(FStaticMeshRequest*& Request);

	bool IsEmpty();

	typedef TArray<FStaticMeshRequest*> FRequestList;
	void Swap(FRequestList& RequestList);

	FRequestList RequestList;
	FCriticalSection RequestListCS;

	class FXSPLoader* Loader;
};

class FBuildStaticMeshTask : public FNonAbandonableTask
{
public:
	FBuildStaticMeshTask(FStaticMeshRequest* InRequest, Body_info* InNodeData, FRequestQueue& MergeQueue)
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
	FStaticMeshRequest* Request;
	Body_info* NodeData;
	FRequestQueue& MergeRequestQueue;
};

class FXSPFileLoadRunnalbe : public FRunnable
{
public:
	FXSPFileLoadRunnalbe(class FXSPLoader* Owner, std::fstream& InFileStream, int32 InStartDbid, int32 InCount, FRequestQueue& LoadQueue, FRequestQueue& MergeQueue)
		: Loader(Owner)
		, FileStream(InFileStream)
		, StartDbid(InStartDbid)
		, Count(InCount)
		, LoadRequestQueue(LoadQueue)
		, MergeRequestQueue(MergeQueue)
	{}
	~FXSPFileLoadRunnalbe();

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

	class FXSPLoader* Loader;

	std::fstream& FileStream;
	int32 StartDbid;
	int32 Count;
	FRequestQueue& LoadRequestQueue;
	FRequestQueue& MergeRequestQueue;
	TArray<Header_info> HeaderList;
	TMap<int32, Body_info*> BodyMap;
};

class FXSPLoader : public IXSPLoader
{
public:
	FXSPLoader();
	virtual ~FXSPLoader();

	virtual bool Init(const TArray<FString>& FilePathNameArray) override;
	virtual void RequestStaticMeshe(int32 Dbid, float Priority, UStaticMeshComponent* TargetMeshComponent) override;

	void Tick(float DeltaTime);

	void Reset();

	void AddToBlacklist(int32 Dbid);

private:
	void DispatchNewRequests(uint64 InFrameNumber);
	void ProcessMergeRequests(float AvailableTime);
	void ReleaseRequests();

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
	TArray<FSourceData*> SourceDataList;

	// 材质模板
	TStrongObjectPtr<UMaterialInterface> SourceMaterial;

	FRequestQueue MergeRequestQueue;

	/**
	Request的生命周期:
	1.在FXSPLoader::Tick中被创建,根据dbid投入到相应文件对应的LoadRequestQueue	--Game线程
	2.在FXSPFileLoadRunnalbe::Run中被从LoadRequestQueue中取出,(读取节点数据后)填充材质数据,与节点数据一起被封装为一个构建任务分发到线程池	--每个文件对应一个工作线程
	3.在FBuildStaticMeshTask::DoWork中完成网格体构建后,被投入到全局的MergeRequestQueue	--线程池任意线程
	4.在FXSPLoader::Tick中被从MergeRequestQueue中取出,将静态网格设置给组件对象,之后Request被销毁	--Game线程
	在整个声明周期中,无论Request如何流转,AllRequestMap一直持有Request,最终必须确保Request在Game线程释放
	*/

	//全部请求的Map,只在Game线程访问
	TMap<int32, FStaticMeshRequest*> AllRequestMap;

	//没有网格体的节点
	TArray<int32> Blacklist;
	FCriticalSection BlacklistCS;

	//收集新请求的缓存数组,由外部调用线程和Game线程访问
	TArray<FStaticMeshRequest*> CachedRequestArray;
	FCriticalSection CachedRequestArrayCS;

	//所有Request的可更新属性共享同一把锁
	FCriticalSection RequestCS;

	friend struct FRequestQueue;
	friend class FXSPFileLoadRunnalbe;
};
