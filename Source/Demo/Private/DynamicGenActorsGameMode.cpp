// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicGenActorsGameMode.h"

#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#include "Math/UnrealMathUtility.h"

#include <iostream>
#include <fstream>
#include <vector>


#define BUILD_MESH_ASYNC 0

#define BATCH_NODES 0

DEFINE_LOG_CATEGORY_STATIC(LogDynamicGenActorsDemo, Log, All);

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
    std::vector<Body_info> fragment;
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

void read_header_info(std::fstream& file, Header_info& info)
{
    file.read((char*)&info.empty_fragment, sizeof(info.empty_fragment));
    file.read((char*)&info.parentdbid, sizeof(info.parentdbid));
    file.read((char*)&info.level, sizeof(info.level));
    file.read((char*)&info.startname, sizeof(info.startname));
    file.read((char*)&info.namelength, sizeof(info.namelength));
    file.read((char*)&info.startproperty, sizeof(info.startproperty));
    file.read((char*)&info.propertylength, sizeof(info.propertylength));
    file.read((char*)&info.startmaterial, sizeof(info.startmaterial));
    file.read((char*)&info.startbox, sizeof(info.startbox));
    file.read((char*)&info.startvertices, sizeof(info.startvertices));
    file.read((char*)&info.verticeslength, sizeof(info.verticeslength));
    file.seekg(10, std::ios::cur);
}

void read_header_info(std::fstream& file, int nsize, std::vector<Header_info>& header_list) {
    header_list.resize(nsize);
    for (int i = 0; i < nsize; i++) {
        read_header_info(file, header_list[i]);
    }
}

void read_body_info(std::fstream& file, const Header_info& header, bool is_fragment, Body_info& body)
{
    body.parentdbid = header.parentdbid;
    body.level = header.level;

    //node name
    file.seekg(header.startname, std::ios::beg);
    char* buffer = new char[header.namelength + 1];
    file.read(buffer, header.namelength);
    buffer[header.namelength] = '\0';
    body.name = buffer;
    delete[] buffer;

    //node property
    file.seekg(header.startproperty, std::ios::beg);
    buffer = new char[header.propertylength + 1];
    file.read(buffer, header.propertylength);
    buffer[header.propertylength] = '\0';
    body.property = buffer;
    delete[] buffer;

    file.seekg(header.startmaterial, std::ios::beg);
    file.read((char*)&body.material, sizeof(body.material));

    if (!is_fragment) {
        file.seekg(header.startbox, std::ios::beg);
        file.read((char*)&body.box, sizeof(body.box));
    }

    //fragment vertices
    file.seekg(header.startvertices, std::ios::beg);

    if (is_fragment) {
        for (int k = 0; k < header.verticeslength / 4; k++) {
            float f;
            file.read((char*)&f, sizeof(f));
            body.vertices.push_back(f);
        }
    }
    else {
        if (header.verticeslength > 0) {
            std::vector<Header_info> fragment_headerList;
            read_header_info(file, header.verticeslength / 50, fragment_headerList);
            int num_fragments = fragment_headerList.size();
            body.fragment.resize(num_fragments);
            for (int k = 0; k < num_fragments; k++) {
                read_body_info(file, fragment_headerList[k], true, body.fragment[k]);
            }
        }
    }
}

bool load_file(std::fstream& file, std::vector<Body_info*>& node_list) {
    file.seekg(0, std::ios::beg);
    int nsize;
    file.read((char*)&nsize, sizeof(nsize));

    //每一个node头的大小
    short headlength;
    file.read((char*)&headlength, sizeof(headlength));

    //header
    std::vector<Header_info> header_list;
    read_header_info(file, nsize, header_list);
    check(header_list.size() == nsize);

    node_list.resize(nsize);
    for (int i = 0; i < nsize; i++)
    {
        node_list[i] = new Body_info;
        node_list[i]->dbid = i;
        read_body_info(file, header_list[i], false, *node_list[i]);
    }

    return true;
}

bool load_file(const std::wstring& filename, std::vector<Body_info*>& node_list) {
    std::fstream file;
    file.open(filename, std::ios::in | std::ios::binary);
    if (!file.is_open())
        return false;
    return load_file(file, node_list);
}

bool IsValidMaterial(float material[4])
{
    if (!FMath::IsFinite(material[0]) || !FMath::IsFinite(material[1]) || !FMath::IsFinite(material[2]) || !FMath::IsFinite(material[3]))
        return false;
    if (material[0] < 0 || material[0] > 1 || material[1] < 0 || material[1] > 1 || material[2] < 0 || material[2] > 1)
        return false;
    if (FMath::IsNearlyZero(material[0]) && FMath::IsNearlyZero(material[1]) && FMath::IsNearlyZero(material[2]))
        return false;
    if (material[3] < 0 || material[3] > 1)
        return false;
    return true;
}

void InheritMaterial(std::vector<Body_info*>& node_list)
{
    //只对具有fragment的节点，向上继承一级节点的材质
    int32 NumNodes = node_list.size();
    for (int32 i = NumNodes - 1; i >= 0; i--)
    {
        Body_info* Node = node_list[i];
        if (Node->fragment.size() == 0)
            continue;
        if (Node->parentdbid < 0)
            continue;
        check(Node->parentdbid < NumNodes);

        Body_info* ParentNode = node_list[Node->parentdbid];
        if (IsValidMaterial(ParentNode->material))
        {
            Node->material[0] = ParentNode->material[0];
            Node->material[1] = ParentNode->material[1];
            Node->material[2] = ParentNode->material[2];
            Node->material[3] = ParentNode->material[3];
        }
    }
}

void GetMaterial(Body_info* Node, FLinearColor& Color, float& Roughness)
{
    if (nullptr != Node)
    {
        if (IsValidMaterial(Node->material))
        {
            Color = FLinearColor(Node->material[0], Node->material[1], Node->material[2]);
            Roughness = Node->material[3];
            return;
        }

        for (size_t i = 0, len = Node->fragment.size(); i < len; i++)
        {
            if (IsValidMaterial(Node->fragment[i].material))
            {
                Color = FLinearColor(Node->fragment[i].material[0], Node->fragment[i].material[1], Node->fragment[i].material[2]);
                Roughness = Node->fragment[i].material[3];
                return;
            }
        }
    }

    Color = FLinearColor(0.078125f, 0.078125f, 0.078125f);
    Roughness = 1.f;
}

bool CheckNode(const Body_info& Node)
{
    bool bValid = false;
    for (size_t j = 0; j < Node.fragment.size(); j++)
    {
        const Body_info& Fragment = Node.fragment[j];
        if ((Fragment.name == "Mesh") ||
            Fragment.name == "Elliptical" ||
            Fragment.name == "Cylinder")
        {
            bValid = true;
        }
        else if (Fragment.name == "Line" || Fragment.name == "Point")
        {
            UE_LOG(LogDynamicGenActorsDemo, Display, TEXT("未处理的图元类型: %s"), *FString(Fragment.name.c_str()));
        }
        else
        {
            UE_LOG(LogDynamicGenActorsDemo, Warning, TEXT("未处理的图元类型: %s"), *FString(Fragment.name.c_str()));
        }
    }
    return bValid;
}

//网格体
void AppendRawMesh(const std::vector<float>& vertices, TArray<FVector>& VertexList)
{
    check(vertices.size() >= 9 && vertices.size() % 9 == 0);

    int32 Index = VertexList.Num();
    int32 NumMeshVertices = vertices.size() / 3;
    if (NumMeshVertices >=3 && NumMeshVertices % 3 == 0)
    {
        VertexList.AddUninitialized(NumMeshVertices);
        for (size_t j = 0, j_len = vertices.size(); j < j_len; j += 3)
            VertexList[Index++].Set(vertices[j + 1] * 100, vertices[j + 0] * 100, vertices[j + 2] * 100);
    }
}

//椭圆形
void AppendEllipticalMesh(const std::vector<float>& vertices, TArray<FVector>& VertexList)
{
    check(vertices.size() == 10);

    static const int32 NumSegments = 18;
    float DeltaAngle = UE_TWO_PI / NumSegments;

    //[origin，xVector，yVector，radius]
    FVector Origin(vertices[1] * 100, vertices[0] * 100, vertices[2] * 100);
    FVector XVector(vertices[4], vertices[3], vertices[5]); //单位方向向量?
    FVector YVector(vertices[7], vertices[6], vertices[8]);
    float Radius = vertices[9] * 100;

    //沿径向的一圈向量
    TArray<FVector> RadialVectors;
    RadialVectors.SetNumUninitialized(NumSegments + 1);
    for (int32 i = 0; i <= NumSegments; i++)
    {
        RadialVectors[i] = XVector * Radius * FMath::Sin(DeltaAngle * i) + YVector * Radius * FMath::Cos(DeltaAngle * i);
    }

    //椭圆面
    TArray<FVector> EllipticalMeshVertices;
    EllipticalMeshVertices.SetNumUninitialized(NumSegments * 3);
    int32 Index = 0;
    for (int32 i = 0; i < NumSegments; i++)
    {
        EllipticalMeshVertices[Index++] = Origin;
        EllipticalMeshVertices[Index++] = Origin + RadialVectors[i + 1];
        EllipticalMeshVertices[Index++] = Origin + RadialVectors[i];
    }

    VertexList.Append(EllipticalMeshVertices);
}

//圆柱体
void AppendCylinderMesh(const std::vector<float>& vertices, TArray<FVector>& VertexList)
{
    check(vertices.size() == 13);

    static const int32 NumSegments = 18;
    float DeltaAngle = UE_TWO_PI / NumSegments;

    //[topCenter，bottomCenter，xAxis，yAxis，radius]
    FVector TopCenter(vertices[1] * 100, vertices[0] * 100, vertices[2] * 100);
    FVector BottomCenter(vertices[4] * 100, vertices[3] * 100, vertices[5] * 100);
    //FVector DirX(vertices[7] * 100, vertices[6] * 100, vertices[8] * 100);
    //FVector DirY(vertices[10] * 100, vertices[9] * 100, vertices[11] * 100);
    float Radius = vertices[12] * 100;

    //轴向
    FVector UpDir = TopCenter - BottomCenter;
    float Height = UpDir.Length();
    UpDir.Normalize();
    
    //计算径向
    FVector RightDir;
    if (FMath::Abs(UpDir.Z) > UE_SQRT_3 / 3)
        RightDir.Set(1, 0, 0);
    else
        RightDir.Set(0, 0, 1);
    RightDir.Normalize();
    FVector RadialDir = FVector::CrossProduct(RightDir, UpDir);
    RadialDir.Normalize();

    //沿径向的一圈向量
    TArray<FVector> RadialVectors;
    RadialVectors.SetNumUninitialized(NumSegments + 1);
    for (int32 i = 0; i <= NumSegments; i++)
    {
        RadialVectors[i] = RadialDir.RotateAngleAxisRad(DeltaAngle * i, UpDir) * Radius;
    }

    TArray<FVector> CylinderMeshVertices;
    CylinderMeshVertices.SetNumUninitialized(NumSegments * 6);
    int32 Index = 0;
    ////顶面
    //for (int32 i = 0; i < NumSegments; i++)
    //{
    //    CylinderMeshVertices[Index++] = TopCenter;
    //    CylinderMeshVertices[Index++] = TopCenter + RadialVectors[i + 1];
    //    CylinderMeshVertices[Index++] = TopCenter + RadialVectors[i];
    //}
    ////底面
    //for (int32 i = 0; i < NumSegments; i++)
    //{
    //    CylinderMeshVertices[Index++] = BottomCenter;
    //    CylinderMeshVertices[Index++] = BottomCenter + RadialVectors[i];
    //    CylinderMeshVertices[Index++] = BottomCenter + RadialVectors[i + 1];
    //}
    //侧面
    for (int32 i = 0; i < NumSegments; i++)
    {
        CylinderMeshVertices[Index++] = BottomCenter + RadialVectors[i];
        CylinderMeshVertices[Index++] = TopCenter + RadialVectors[i];
        CylinderMeshVertices[Index++] = BottomCenter + RadialVectors[i + 1];
        CylinderMeshVertices[Index++] = BottomCenter + RadialVectors[i + 1];
        CylinderMeshVertices[Index++] = TopCenter + RadialVectors[i];
        CylinderMeshVertices[Index++] = TopCenter + RadialVectors[i + 1];
    }

    VertexList.Append(CylinderMeshVertices);
}

void AppendNodeMesh(const Body_info& Node, TArray<FVector>& VertexList)
{
    for (size_t i = 0, i_len = Node.fragment.size(); i < i_len; i++)
    {
        if (Node.fragment[i].name == "Mesh")
        {
            AppendRawMesh(Node.fragment[i].vertices, VertexList);
        }
        else if (Node.fragment[i].name == "Elliptical")
        {
            AppendEllipticalMesh(Node.fragment[i].vertices, VertexList);
        }
        else if (Node.fragment[i].name == "Cylinder")
        {
            AppendCylinderMesh(Node.fragment[i].vertices, VertexList);
        }
    }
}

void BuildStaticMesh(UStaticMesh* StaticMesh, const TArray<FVector>& VertexList)
{
    StaticMesh->GetStaticMaterials().Add(FStaticMaterial());

    FMeshDescription MeshDesc;
    FStaticMeshAttributes Attributes(MeshDesc);
    Attributes.Register();

    FMeshDescriptionBuilder MeshDescBuilder;
    MeshDescBuilder.SetMeshDescription(&MeshDesc);
    MeshDescBuilder.EnablePolyGroups();
    MeshDescBuilder.SetNumUVLayers(1);

    int32 NumVertices = VertexList.Num();
    TArray<FVertexInstanceID> VertexInstanceIDs;
    VertexInstanceIDs.SetNum(NumVertices);
    for (int32 i = 0; i < NumVertices; i++)
    {
        FVertexID VertexID = MeshDescBuilder.AppendVertex(VertexList[i]);
        VertexInstanceIDs[i] = MeshDescBuilder.AppendInstance(VertexID);
        MeshDescBuilder.SetInstanceColor(VertexInstanceIDs[i], FVector4f(1, 1, 1, 1));
        MeshDescBuilder.SetInstanceUV(VertexInstanceIDs[i], FVector2D(0, 0));
        MeshDescBuilder.SetInstanceNormal(VertexInstanceIDs[i], FVector(0, 0, 1));    //TODO:计算法线和切线
        //MeshDescBuilder.SetInstanceTangentSpace(VertexInstanceIDs[i], FVector(), FVector(), true);
    }

    FPolygonGroupID PolygonGroup = MeshDescBuilder.AppendPolygonGroup();
    int32 NumTriangles = NumVertices / 3;
    for (int32 i = 0; i < NumTriangles; i++)
    {
        MeshDescBuilder.AppendTriangle(VertexInstanceIDs[i * 3 + 0], VertexInstanceIDs[i * 3 + 1], VertexInstanceIDs[i * 3 + 2], PolygonGroup);
    }

    UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
    BuildParams.bMarkPackageDirty = false;
    BuildParams.bBuildSimpleCollision = false;
    BuildParams.bFastBuild = true;

    TArray<const FMeshDescription*> MeshDescPtrs;
    MeshDescPtrs.Emplace(&MeshDesc);

    //StaticMesh->SetNumSourceModels(3);
    StaticMesh->BuildFromMeshDescriptions(MeshDescPtrs, BuildParams);
    //StaticMesh->Build();
}

void BuildStaticMesh(UStaticMesh* StaticMesh, const Body_info& Node)
{
    TArray<FVector> VertexList;
    AppendNodeMesh(Node, VertexList);
    if (VertexList.Num() < 3)
    {
        checkNoEntry();
        return;
    }
    BuildStaticMesh(StaticMesh, VertexList);
}

void BuildStaticMesh(UStaticMesh* StaticMesh, std::vector<Body_info*>& NodeList)
{
    TArray<FVector> VertexList;
    for (auto NodePtr : NodeList)
    {
        AppendNodeMesh(*NodePtr, VertexList);
    }
    BuildStaticMesh(StaticMesh, VertexList);
}

UMaterialInstanceDynamic* CreateMaterialInstanceDynamic(UMaterialInterface* SourceMaterial, const FLinearColor& Color, float Roughness)
{
    UMaterialInstanceDynamic* MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(SourceMaterial, nullptr);
    MaterialInstanceDynamic->SetVectorParameterValue(TEXT("BaseColor"), Color);
    MaterialInstanceDynamic->SetScalarParameterValue(TEXT("Roughness"), Roughness);
    return MaterialInstanceDynamic;
}

void ADynamicGenActorsGameMode::FLoadFileTask::DoWork()
{
    bSucceed = load_file(std::wstring(*FilePathName), NodeDataList);
    if (bSucceed)
        InheritMaterial(NodeDataList);
}

// 异步构建静态网格数据的任务类
class FBuildStaticMeshTask : public FNonAbandonableTask
{
public:
    FBuildStaticMeshTask(ADynamicGenActorsGameMode* InGameMode, UStaticMesh* InStaticMesh, Body_info* InNode)
        : GameMode(InGameMode)
        , StaticMesh(InStaticMesh)
        , Node(InNode)
    {
    }
    ~FBuildStaticMeshTask()
    {
        delete Node;
    }

    void DoWork()
    {
        // 构建静态网格
        BuildStaticMesh(StaticMesh, *Node);

        // 完成构建的数据推送到待合并队列，在Game线程合并进场景
        ADynamicGenActorsGameMode::FLoadedData LoadedData;
        LoadedData.Name = FName(FString::FromInt(Node->dbid));
        LoadedData.StaticMesh = StaticMesh;
        GetMaterial(Node, LoadedData.Color, LoadedData.Roughness);
        GameMode->LoadedNodes.Enqueue(LoadedData);
    }

    TStatId GetStatId() const
    {
        return TStatId();
    }

private:
    ADynamicGenActorsGameMode* GameMode;
    UStaticMesh* StaticMesh;
    Body_info* Node;
};

ADynamicGenActorsGameMode::ADynamicGenActorsGameMode()
{
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
}

void ADynamicGenActorsGameMode::BeginPlay()
{
    Super::BeginPlay();

    //创建Actor
    {
        FActorSpawnParameters ActorSpawnParameters;
        ActorSpawnParameters.Name = TEXT("DataActor");
        DataActor = GetWorld()->SpawnActor<AActor>();
#if WITH_EDITOR
        DataActor->SetActorLabel(TEXT("DataActor"));
#endif
        USceneComponent* SceneComponent = NewObject<USceneComponent>(DataActor, TEXT("RootComponent"));
        DataActor->SetRootComponent(SceneComponent);
    }

    //加载材质模板
    SourceMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, L"/Game/DynamicGenActors/M_MainOpaque"));
    if (!SourceMaterial)
    {
        UE_LOG(LogDynamicGenActorsDemo, Error, TEXT("加载材质模板失败"));
    }

    //后台线程读取文件
    FString DataFilePathName = FPaths::Combine(FPaths::ProjectDir(), TEXT("data.xsp"));
    AsyncLoadFileTask = new FAsyncTask<FLoadFileTask>(DataFilePathName, NodeDataList);
    AsyncLoadFileTask->StartBackgroundTask();
    CurrentLoadPhase = ELoadPhase::LP_LoadingFile;
    
    FString Message = FString::Printf(TEXT("正在读取数据文件"));
    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, Message, true);
}

void ADynamicGenActorsGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (CurrentLoadPhase == ELoadPhase::LP_LoadingFile)
    {
        if (AsyncLoadFileTask)
        {
            if (AsyncLoadFileTask->IsDone())
            {
                if (AsyncLoadFileTask->GetTask().bSucceed)
                {
                    LoadScene();

                    CurrentLoadPhase = ELoadPhase::LP_LoadingScene;
                }
                else
                {
                    CurrentLoadPhase = ELoadPhase::LP_Finished;

                    FString Message = FString::Printf(TEXT("读取数据文件失败"));
                    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, Message, true);
                    UE_LOG(LogDynamicGenActorsDemo, Error, TEXT("%s"), *Message);
                }

                delete AsyncLoadFileTask;
                AsyncLoadFileTask = nullptr;
            }
        }
    }
    else if (CurrentLoadPhase == ELoadPhase::LP_LoadingScene)
    {
        if (NumLoadedNodes < NumValidNodes)
        {
            int64 BeginTicks = FDateTime::Now().GetTicks();

            // 从完成队列取出静态网格加入场景
            FLoadedData LoadedData;
            while (LoadedNodes.Dequeue(LoadedData))
            {
                AddToScene(&LoadedData);
                NumLoadedNodes++;

                FString Message = FString::Printf(TEXT("图元加载中 (%d / %d) ..."), NumLoadedNodes, NumValidNodes);
                GEngine->AddOnScreenDebugMessage(0, 5.0f, FColor::Red, Message, true);

                // 每帧最多给0.07秒用于合并新网格到场景中，以保证一定的帧率
                if ((float)(FDateTime::Now().GetTicks() - BeginTicks) / ETimespan::TicksPerSecond >= 0.07f)
                    break;
            }
        }
        else
        {
            CurrentLoadPhase = ELoadPhase::LP_Finished;

            FString Message = FString::Printf(TEXT("加载完成 (%d)"), NumLoadedNodes);
            GEngine->AddOnScreenDebugMessage(0, 10.0f, FColor::Green, Message, true);
        }
    }
}

void ADynamicGenActorsGameMode::LoadScene()
{
    NumLoadedNodes = 0;
    NumValidNodes = 0;
    int32 NumNodes = NodeDataList.size();

#if BATCH_NODES
    NumValidNodes = 1;
    StaticMeshList.Add(NewObject<UStaticMesh>());
#else
    StaticMeshList.AddUninitialized(NumNodes);
    for (int32 i = 0; i < NumNodes; i++)
    {
        if (CheckNode(*NodeDataList[i]))
        {
            NumValidNodes++;
            // 必须在Game线程创建UObject派生对象
            StaticMeshList[i] = NewObject<UStaticMesh>();
        }
        else
        {
            // 释放空的Node对象
            delete NodeDataList[i];
            NodeDataList[i] = nullptr;
            StaticMeshList[i] = nullptr;
        }
    }
#endif

#if BUILD_MESH_ASYNC //异步多线程构建静态网格对象
    for (int32 i = 0; i < NumNodes; i++)
    {
        if (StaticMeshList[i])
        {
            (new FAutoDeleteAsyncTask<FBuildStaticMeshTask>(this, StaticMeshList[i], NodeDataList[i]))->StartBackgroundTask();
        }
    }
#else //Game线程构建静态网格对象

#if BATCH_NODES
    BuildStaticMesh(StaticMeshList[0], NodeDataList);
    FLoadedData LoadedData;
    LoadedData.Name = FName(FString::FromInt(0));
    LoadedData.StaticMesh = StaticMeshList[0];
    GetMaterial(nullptr, LoadedData.Color, LoadedData.Roughness);
    LoadedNodes.Enqueue(LoadedData);
#else
    for (int32 i = 0; i < NumNodes; i++)
    {
        UStaticMesh* StaticMesh = StaticMeshList[i];
        if (StaticMesh)
        {
            Body_info* Node = NodeDataList[i];

            BuildStaticMesh(StaticMesh, *Node);

            FLoadedData LoadedData;
            LoadedData.Name = FName(FString::FromInt(Node->dbid));
            LoadedData.StaticMesh = StaticMesh;
            GetMaterial(Node, LoadedData.Color, LoadedData.Roughness);
            LoadedNodes.Enqueue(LoadedData);
        }
    }
#endif
#endif
}

void ADynamicGenActorsGameMode::AddToScene(FLoadedData* LoadedData)
{
    UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(DataActor, LoadedData->Name);
    StaticMeshComponent->SetStaticMesh(LoadedData->StaticMesh);
    //StaticMeshComponent->SetMaterial(0, CreateMaterialInstanceDynamic(SourceMaterial, LoadedData->Color, LoadedData->Roughness));
    StaticMeshComponent->AttachToComponent(DataActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    StaticMeshComponent->RegisterComponent();
}