// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicLoadGameMode.h"
#include "XSPLoaderModule.h"
#include <fstream>

DEFINE_LOG_CATEGORY_STATIC(LogDynamicLoadDemo, Log, All);

ADynamicLoadGameMode::ADynamicLoadGameMode()
{
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
}

void ADynamicLoadGameMode::BeginPlay()
{
    FString DataFilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("XSP"));
    TArray<FString> FileArray;
    IFileManager::Get().FindFiles(FileArray, *DataFilePath, TEXT(".xsp"));
    if (FileArray.Num() < 1)
    {
        UE_LOG(LogDynamicLoadDemo, Error, TEXT("查找XSP文件失败"));
        return;
    }

    bool bFail = false;
    for (FString& FileName : FileArray)
    {
        FileName = FPaths::Combine(DataFilePath, FileName);
        std::fstream FileStream;
        FileStream.open(std::wstring(*FileName), std::ios::in | std::ios::binary);
        if (!FileStream.is_open())
        {
            bFail = true;
            break;
        }
        FileStream.seekg(0, std::ios::beg);
        int NumNodes;
        FileStream.read((char*)&NumNodes, sizeof(NumNodes));
        NodesNumArray.Add(NumNodes);
        FileStream.close();
    }
    if (bFail)
    {
        UE_LOG(LogDynamicLoadDemo, Error, TEXT("读取XSP文件失败"));
        return;
    }

    IXSPLoader& Loader = FModuleManager::LoadModuleChecked<FXSPLoaderModule>("XSPLoader").Get();
    //FModuleManager::GetModuleChecked<FXSPLoaderModule>("XSPLoader");
    if (!Loader.Init(FileArray))
    {
        UE_LOG(LogDynamicLoadDemo, Error, TEXT("初始化XSPLoader失败"));
        return;
    }
    
    DataActor = GetWorld()->SpawnActor<AActor>();
    USceneComponent* SceneComponent = NewObject<USceneComponent>(DataActor, TEXT("RootComponent"));
    DataActor->SetRootComponent(SceneComponent);

    int32 Index = 0;
    for (int32 i = 0; i < NodesNumArray.Num(); ++i)
    {
        int32 Num = NodesNumArray[i];
        for (int32 j = 0; j < Num; ++j)
        {
            FString Name = FString::FromInt(Index);
            UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(DataActor, *Name);
            StaticMeshComponent->SetMobility(EComponentMobility::Movable);
            StaticMeshComponent->AttachToComponent(DataActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            StaticMeshComponents.Emplace(Index, StaticMeshComponent);

            Index += 1;
        }
    }
    UE_LOG(LogDynamicLoadDemo, Display, TEXT("总共%d节点"), Index);
}

void ADynamicLoadGameMode::Tick(float deltaSeconds)
{
    //模拟每帧剔除操作,所有节点都通过,对未加载的节点发起加载请求
    int32 Count = 0;
    for (auto& Pair : StaticMeshComponents)
    {
        if (Pair.Value->GetStaticMesh() == nullptr)
        {
            FModuleManager::GetModuleChecked<FXSPLoaderModule>("XSPLoader").Get().RequestStaticMeshe(Pair.Key, 0, Pair.Value);
            Count += 1;
        }
        if (Count > 1000)
            return;
    }
}
