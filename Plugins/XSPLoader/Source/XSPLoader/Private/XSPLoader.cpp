#include "XSPLoader.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY_STATIC(LogXSPLoader, Log, All);

namespace
{
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

    void read_header_info(std::fstream& file, int nsize, TArray<Header_info>& header_list) {
        header_list.SetNum(nsize);
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
                TArray<Header_info> fragment_headerList;
                read_header_info(file, header.verticeslength / 50, fragment_headerList);
                int num_fragments = fragment_headerList.Num();
                body.fragment.SetNum(num_fragments);
                for (int k = 0; k < num_fragments; k++) {
                    read_body_info(file, fragment_headerList[k], true, body.fragment[k]);
                }
            }
        }
    }

    //网格体
    void AppendRawMesh(const std::vector<float>& vertices, TArray<FVector>& VertexList)
    {
        check(vertices.size() >= 9 && vertices.size() % 9 == 0);

        int32 Index = VertexList.Num();
        int32 NumMeshVertices = vertices.size() / 3;
        if (NumMeshVertices >= 3 && NumMeshVertices % 3 == 0)
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
        for (int32 i = 0, i_len = Node.fragment.Num(); i < i_len; i++)
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

    void GetMaterial(Body_info& Node, FLinearColor& Color, float& Roughness)
    {
        if (IsValidMaterial(Node.material))
        {
            Color = FLinearColor(Node.material[0], Node.material[1], Node.material[2]);
            Roughness = Node.material[3];
            return;
        }

        for (int32 i = 0, len = Node.fragment.Num(); i < len; i++)
        {
            if (IsValidMaterial(Node.fragment[i].material))
            {
                Color = FLinearColor(Node.fragment[i].material[0], Node.fragment[i].material[1], Node.fragment[i].material[2]);
                Roughness = Node.fragment[i].material[3];
                return;
            }
        }

        Color = FLinearColor(0.078125f, 0.078125f, 0.078125f);
        Roughness = 1.f;
    }

    void InheritMaterial(Body_info& Node, Body_info& Parent)
    {
        if (IsValidMaterial(Parent.material))
        {
            Node.material[0] = Parent.material[0];
            Node.material[1] = Parent.material[1];
            Node.material[2] = Parent.material[2];
            Node.material[3] = Parent.material[3];
        }
    }

    UMaterialInstanceDynamic* CreateMaterialInstanceDynamic(UMaterialInterface* SourceMaterial, const FLinearColor& Color, float Roughness)
    {
        UMaterialInstanceDynamic* MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(SourceMaterial, nullptr);
        MaterialInstanceDynamic->SetVectorParameterValue(TEXT("BaseColor"), Color);
        MaterialInstanceDynamic->SetScalarParameterValue(TEXT("Roughness"), Roughness);
        return MaterialInstanceDynamic;
    }

    bool CheckNode(const Body_info& Node)
    {
        bool bValid = false;
        for (int32 j = 0; j < Node.fragment.Num(); j++)
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
                UE_LOG(LogXSPLoader, Display, TEXT("未处理的图元类型: %s"), *FString(Fragment.name.c_str()));
            }
            else
            {
                UE_LOG(LogXSPLoader, Warning, TEXT("未处理的图元类型: %s"), *FString(Fragment.name.c_str()));
            }
        }
        return bValid;
    }

}

void FStaticMeshRequest::Invalidate()
{
    bValid = false;
    TargetComponent.Reset();
    StaticMesh.Reset();
}

void FRequestQueue::Add(TSharedPtr<FStaticMeshRequest>& Request)
{

}

void FRequestQueue::AddNoLock(TSharedPtr<FStaticMeshRequest>& Request)
{

}

void FRequestQueue::TakeFirst(TSharedPtr<FStaticMeshRequest>& Request)
{

}

bool FRequestQueue::IsEmpty()
{
    return true;
}

void FRequestQueue::Swap(FRequestList& OutRequestList)
{

}

void FBuildStaticMeshTask::DoWork()
{
    BuildStaticMesh(Request->StaticMesh.Get(), NodeData);

    MergeRequestQueue.Add(Request);
}

uint32 FXSPFileLoadRunnalbe::Run()
{
    //读节点头信息
    FileStream.seekg(0, std::ios::beg);
    int NumNodes;
    FileStream.read((char*)&NumNodes, sizeof(NumNodes));
    check(Count == NumNodes);
    short headlength;
    FileStream.read((char*)&headlength, sizeof(headlength));
    read_header_info(FileStream, Count, HeaderList);
    check(HeaderList.Num() == Count);

    //循环等待并执行加载请求
    while (!bStopRequested)
    {
        TSharedPtr<FStaticMeshRequest> Request;
        LoadRequestQueue.TakeFirst(Request);
        if (Request.IsValid() && Request->IsValid())
        {
            //计算全局dbid在本文件中的局部dbid
            int32 LocalDbid = Request->Dbid - StartDbid;
            check(LocalDbid >= 0 && LocalDbid < Count);

            //读取Body数据
            bool bHasCache = true;
            if (!BodyMap.Contains(LocalDbid))
            {
                bHasCache = false;
                //读过的节点数据就缓存在内存中
                read_body_info(FileStream, HeaderList[LocalDbid], false, BodyMap.Emplace(LocalDbid));
            }

            Body_info& NodeData = BodyMap[LocalDbid];
            if (CheckNode(NodeData))
            {
                //新读入的节点需要尝试继承上级节点的材质数据
                int32 LocalParentDbid = NodeData.parentdbid < 0 ? -1 : NodeData.parentdbid - StartDbid;
                if (LocalParentDbid >= 0 && LocalParentDbid < Count)
                {
                    if (!BodyMap.Contains(LocalParentDbid))
                    {
                        read_body_info(FileStream, HeaderList[LocalParentDbid], false, BodyMap.Emplace(LocalParentDbid));
                    }
                    
                    InheritMaterial(NodeData, BodyMap[LocalParentDbid]);
                }

                GetMaterial(NodeData, Request->Color, Request->Roughness);

                //分发构建网格体的任务到线程池
                (new FAutoDeleteAsyncTask<FBuildStaticMeshTask>(Request, BodyMap[LocalDbid], MergeRequestQueue))->StartBackgroundTask();
            }
        }

        if (LoadRequestQueue.IsEmpty())
            FPlatformProcess::SleepNoStats(1.0f);
    }

    return 0;
}


FXSPLoader::FXSPLoader()
{

}

FXSPLoader::~FXSPLoader()
{
    Reset();
}

bool FXSPLoader::Init(const TArray<FString>& FilePathNameArray)
{
    if (bInitialized)
        return false;

    int32 NumFiles = FilePathNameArray.Num();
    if (NumFiles < 1)
        return false;

    int32 TotalNumNodes = 0;
    bool bFail = false;
    SourDataList.SetNum(NumFiles);
    for (int32 i = 0; i < NumFiles; ++i)
    {
        SourDataList[i] = new FSourceData;
        std::fstream& FileStream = SourDataList[i]->FileStream;
        FileStream.open(std::wstring(*FilePathNameArray[i]), std::ios::in | std::ios::binary);
        if (!FileStream.is_open())
        {
            bFail = true;
            break;
        }

        //读取源文件的节点数
        FileStream.seekg(0, std::ios::beg);
        int NumNodes;
        FileStream.read((char*)&NumNodes, sizeof(NumNodes));
        
        SourDataList[i]->StartDbid = TotalNumNodes;
        SourDataList[i]->Count = NumNodes;
        TotalNumNodes += NumNodes;
    }
    if (bFail)
    {
        Reset();
        return false;
    }

    //SourceMaterial = TStrongObjectPtr(Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, L"/Game/DynamicGenActors/M_MainOpaque")));

    //为每个源文件创建一个读取线程
    for (int32 i = 0; i < NumFiles; ++i)
    {
        FString ThreadName = FString::Printf(TEXT("XSPFileLoader_%d"), i);
        SourDataList[i]->FileLoadRunnable = new FXSPFileLoadRunnalbe(SourDataList[i]->FileStream, SourDataList[i]->StartDbid, SourDataList[i]->Count, SourDataList[i]->LoadRequestQueue, MergeRequestQueue);
        SourDataList[i]->LoadThread = FRunnableThread::Create(SourDataList[i]->FileLoadRunnable, *ThreadName, 8 * 1024, TPri_Normal);
    }

    bInitialized = true;

    return true;
}

void FXSPLoader::RequestStaticMeshe(int32 Dbid, float Priority, TWeakObjectPtr<UStaticMeshComponent>& TargetMeshComponent)
{
    FScopeLock Lock(&CachedRequestArrayCS);
    CachedRequestArray.Emplace(new FStaticMeshRequest(Dbid, Priority, TargetMeshComponent));
}

void FXSPLoader::Tick(float DeltaTime)
{
    if (!bInitialized)
        return;

    uint64 CurrentFrameNumber = FrameNumber.fetch_add(1);

    DispatchNewRequests(CurrentFrameNumber);

    ProcessMergeRequests();
}

void FXSPLoader::Reset()
{
    for (auto SourceDataPtr : SourDataList)
    {
        if (nullptr != SourceDataPtr->LoadThread)
        {
            SourceDataPtr->LoadThread->Kill(true);
            delete SourceDataPtr->LoadThread;
        }
        if (nullptr != SourceDataPtr->FileLoadRunnable)
        {
            delete SourceDataPtr->FileLoadRunnable;
        }
        if (SourceDataPtr->FileStream.is_open())
        {
            SourceDataPtr->FileStream.close();
        }
        delete SourceDataPtr;
    }
    SourDataList.Empty();
}

void FXSPLoader::DispatchNewRequests(uint64 InFrameNumber)
{
    TArray<TSharedPtr<FStaticMeshRequest>> NewRequestArray;
    {
        FScopeLock Lock(&CachedRequestArrayCS);
        NewRequestArray = MoveTemp(CachedRequestArray);
    }

    for (auto& TempRequest : NewRequestArray)
    {
        int32 Dbid = TempRequest->Dbid;
        if (AllRequestMap.Contains(Dbid))
        {
            //更新已有请求
            FScopeLock Lock(&RequestCS);
            TSharedPtr<FStaticMeshRequest>& Request = AllRequestMap[Dbid];
            Request->Priority = TempRequest->Priority;
            Request->LastUpdateFrameNumber = InFrameNumber;
            Request->TargetComponent = TempRequest->TargetComponent;
        }
        else
        {
            //新请求,为其创建静态网格对象,并加入Map
            TempRequest->StaticMesh = TStrongObjectPtr<UStaticMesh>(NewObject<UStaticMesh>());
            TempRequest->LastUpdateFrameNumber = InFrameNumber;
            AllRequestMap.Emplace(Dbid, TempRequest);
            //根据Dbid分发到相应的请求队列
            for (auto SourceDataPtr : SourDataList)
            {
                if (Dbid >= SourceDataPtr->StartDbid && Dbid < SourceDataPtr->StartDbid + SourceDataPtr->Count)
                {
                    SourceDataPtr->LoadRequestQueue.Add(TempRequest);
                    break;
                }
            }
        }
    }
}

void FXSPLoader::ProcessMergeRequests()
{
    while (!MergeRequestQueue.IsEmpty())
    {
        TSharedPtr<FStaticMeshRequest> Request;
        MergeRequestQueue.TakeFirst(Request);
        if (Request.IsValid() && Request->IsValid())
        {
            Request->TargetComponent->SetMaterial(0, CreateMaterialInstanceDynamic(SourceMaterial.Get(), Request->Color, Request->Roughness));
            Request->TargetComponent->SetStaticMesh(Request->StaticMesh.Get());
            //Request->TargetComponent->RegisterComponent();
        }

        AllRequestMap.Remove(Request->Dbid);
    }
}