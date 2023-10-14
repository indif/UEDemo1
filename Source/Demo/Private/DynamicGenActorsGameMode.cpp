// Fill out your copyright notice in the Description page of Project Settings.


#include "DynamicGenActorsGameMode.h"

#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"

#include <iostream>
#include <fstream>
#include <vector>

namespace
{
    using namespace std;

    typedef struct body_info
    {
        int dbid;  //结构体的索引就是dbid 从0开始
        int parentdbid;      //parent db id
        short level;    //node 所在的节点层级 从0开始
        std::string name;   //fragment/node name
        std::string property;   //节点属性
        float material[4];  //材质
        float box[6];      //min max
        std::vector<float> vertices;
        std::vector<body_info> fragment;
    } Body_info;

    typedef struct header_info
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
    } Header_info;

    void read_header_info(fstream& file, int nsize, vector<Header_info>* headerList) {
        for (int i = 0; i < nsize; i++) {
            Header_info info = { 0 };
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
            //cout << "startvertices: " << info.startvertices << endl;
            file.read((char*)&info.verticeslength, sizeof(info.verticeslength));
            //cout << "verticeslength: " << info.verticeslength<< endl;
            headerList->push_back(info);
            file.seekg(10, ios::cur);
        }
    }

    void read_body_info(fstream& file, int nsize, vector<Header_info>& headerList, vector<Body_info>* node_v, bool bfragment = false) {
        for (int i = 0; i < nsize; i++) {
            Body_info body = { 0 };
            Header_info info = headerList[i];

            body.dbid = i;
            body.parentdbid = info.parentdbid;
            body.level = info.level;

            //node name
            file.seekg(info.startname, ios::beg);
            char* buffer = new char[info.namelength + 1];
            file.read(buffer, info.namelength);
            buffer[info.namelength] = '\0';
            string name(buffer);
            //cout << "name: " << name.c_str() << endl;
            body.name = name;
            delete[] buffer;

            //node property
            file.seekg(info.startproperty, ios::beg);
            buffer = new char[info.propertylength + 1];
            file.read(buffer, info.propertylength);
            buffer[info.propertylength] = '\0';
            string property(buffer);
            //cout << "property: " << property.c_str() << endl;
            body.property = property;
            delete[] buffer;

            file.seekg(info.startmaterial, ios::beg);
            file.read((char*)&body.material, sizeof(body.material));

            if (!bfragment) {
                file.seekg(info.startbox, ios::beg);
                file.read((char*)&body.box, sizeof(body.box));
            }

            //fragment vertices
            file.seekg(info.startvertices, ios::beg);

            if (bfragment) {
                for (int k = 0; k < info.verticeslength / 4; k++) {
                    float f;
                    file.read((char*)&f, sizeof(f));
                    body.vertices.push_back(f);
                }
            }
            else {
                if (info.verticeslength > 0) {
                    vector<Body_info> fragment_v;
                    vector<Header_info> fragment_headerList;
                    read_header_info(file, info.verticeslength / 50, &fragment_headerList);
                    read_body_info(file, info.verticeslength / 50, fragment_headerList, &body.fragment, true);
                }
            }

            node_v->push_back(body);
        }
    }

    bool load_file(const std::wstring& file_name, vector<Body_info>& node_v) {
        fstream file;
        file.open(file_name, ios::in | ios::binary);
        if (!file.is_open()) {
            cout << "open the file fail" << endl;
            return false;
        }
        file.seekg(0, ios::end);
        int file_size = static_cast<int>(file.tellg());
        cout << "file size: " << file_size << endl;

        //dbid个数 = node个数
        file.seekg(0, ios::beg);
        int nsize;
        file.read((char*)&nsize, sizeof(nsize));
        cout << "node number: " << nsize << endl;

        //每一个node头的大小
        short headlength;
        file.read((char*)&headlength, sizeof(headlength));
        cout << "head length: " << headlength << endl;

        //header
        vector<Header_info> headerList;
        read_header_info(file, nsize, &headerList);

        int file_tellg = static_cast<int>(file.tellg());
        cout << "file tellg: " << file_tellg << endl;

        //body
        if (headerList.size() == nsize) {
            read_body_info(file, nsize, headerList, &node_v);
        }
        cout << "ok" << endl;

        file.close();
        return true;
    }

    UMaterialInstanceDynamic* CreateMaterialInstanceDynamic(UMaterialInterface* SourceMaterial, float material[4])
    {
        FLinearColor Color(material[0], material[1], material[2]);
        UMaterialInstanceDynamic* MaterialInstanceDynamic = UMaterialInstanceDynamic::Create(SourceMaterial, nullptr);
        MaterialInstanceDynamic->SetVectorParameterValue(TEXT("BaseColor"), Color);
        MaterialInstanceDynamic->SetScalarParameterValue(TEXT("Roughness"), material[3]);
        return MaterialInstanceDynamic;
    }

    void AppendRawMesh(const vector<float>& vertices, TArray<FVector>& VertexList)
    {
        int32 Index = VertexList.Num();
        int32 NumMeshVertices = vertices.size() / 3;
        if (NumMeshVertices > 3 && NumMeshVertices % 3 == 0)
        {
            VertexList.AddUninitialized(NumMeshVertices);
            for (size_t j = 0, j_len = vertices.size(); j < j_len; j += 3)
                VertexList[Index++].Set(vertices[j + 1] * 100, vertices[j + 0] * 100, vertices[j + 2] * 100);
        }
    }

    void AppendEllipticalMesh(const vector<float>& vertices, TArray<FVector>& VertexList)
    {
        int32 Index = VertexList.Num();

        //[origin，xVector，yVector，radius]
        //TODO:
    }

    void AppendCylinderMesh(const vector<float>& vertices, TArray<FVector>& VertexList)
    {
        int32 Index = VertexList.Num();

        //[topCenter，bottomCenter，xAxis，yAxis，radius]
        //TODO:
    }

    UStaticMesh* BuildStaticMesh(const TArray<FVector>& VertexList, UObject* Outer, FName Name)
    {
        UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Outer, Name);

        StaticMesh->GetStaticMaterials().Add(FStaticMaterial());//至少添加一个材质

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
            MeshDescBuilder.SetInstanceNormal(VertexInstanceIDs[i], FVector(0,0,1));    //TODO:计算法线和切线
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

        StaticMesh->BuildFromMeshDescriptions(MeshDescPtrs, BuildParams);

        return StaticMesh;
    }

    UStaticMesh* BuildStaticMesh(const vector<float>& vertices, UObject* Outer, FName Name)
    {
        check(vertices.size() > 9 && vertices.size() % 9 == 0);
        int32 NumVertices = vertices.size() / 3;
        TArray<FVector> VertexList;
        VertexList.SetNum(NumVertices);
        for (int32 i = 0; i < NumVertices; i++)
            VertexList[i].Set(vertices[i * 3 + 1] * 100, vertices[i * 3 + 0] * 100, vertices[i * 3 + 2] * 100);

        return BuildStaticMesh(VertexList, Outer, Name);
    }

    UStaticMesh* BuildStaticMesh(const Body_info& Node, UObject* Outer, FName Name)
    {
        TArray<FVector> VertexList;
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

        return BuildStaticMesh(VertexList, Outer, Name);
    }
}

void ADynamicGenActorsGameMode::BeginPlay()
{
    Super::BeginPlay();

    std::vector<Body_info> NodeList;

    FString DataFilePathName = FPaths::Combine(FPaths::ProjectDir(), TEXT("data.xsp"));
    if (!load_file(*DataFilePathName, NodeList))
    {
        FString Message = FString::Printf(TEXT("加载数据文件失败：%s"), *DataFilePathName);
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, Message, true);
        return;
    }

    FString Message = FString::Printf(TEXT("加载数据文件完成：节点数=%d"), NodeList.size());
    GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, Message, true);

    UMaterialInterface* SourceMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, L"/Game/DynamicGenActors/M_MainOpaque"));
    if (!SourceMaterial)
        return;

#if 0
    for (size_t i = 0; i < NodeList.size(); i++)
    {
        Body_info& Node = NodeList[i];
        if (Node.fragment.size() > 0)
        {
            bool bHasComponent = false;
            for (size_t j = 0; j < Node.fragment.size(); j++)
            {
                Body_info& Fragment = Node.fragment[j];
                if (Fragment.name == "Mesh" && Fragment.vertices.size() > 9 && Fragment.vertices.size() % 9 == 0)
                {
                    bHasComponent = true;
                    break;
                }
            }
            if (!bHasComponent)
                continue;

            FString Name = FString::FromInt(Node.dbid);
            FActorSpawnParameters ActorSpawnParameters;
            ActorSpawnParameters.Name = *Name;
            AActor* NodeActor = GetWorld()->SpawnActor<AActor>(ActorSpawnParameters);
#if WITH_EDITOR
            NodeActor->SetActorLabel(Name);
#endif
            {
                USceneComponent* Component = NewObject<USceneComponent>(NodeActor, *Name);
                NodeActor->SetRootComponent(Component);
            }

            for (size_t j = 0; j < Node.fragment.size(); j++)
            {
                Body_info& Fragment = Node.fragment[j];
                if (Fragment.name == "Mesh" && Fragment.vertices.size() > 9 && Fragment.vertices.size() % 9 == 0)
                {
                    Name = FString::FromInt(Fragment.dbid);
                    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(NodeActor, *Name);
                    UStaticMesh* StaticMesh = BuildStaticMesh(Fragment.vertices, Component, *Name);
                    Component->SetStaticMesh(StaticMesh);
                    Component->SetMaterial(0, CreateMaterialInstanceDynamic(SourceMaterial, Fragment.material));
                    Component->AttachToComponent(NodeActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
                }
            }

            NodeActor->RegisterAllComponents();
        }
    }
#else
    FString ActorName = FPaths::GetBaseFilename(DataFilePathName);
    FActorSpawnParameters ActorSpawnParameters;
    ActorSpawnParameters.Name = *ActorName;
    AActor* Actor = GetWorld()->SpawnActor<AActor>(ActorSpawnParameters);
#if WITH_EDITOR
    Actor->SetActorLabel(ActorName);
#endif
    USceneComponent* SceneComponent = NewObject<USceneComponent>(Actor, TEXT("RootComponent"));
    Actor->SetRootComponent(SceneComponent);

    for (size_t i = 0; i < NodeList.size(); i++)
    {
        Body_info& Node = NodeList[i];
        if (Node.fragment.size() > 0)
        {
            bool bValidComponent = false;
            for (size_t j = 0; j < Node.fragment.size(); j++)
            {
                Body_info& Fragment = Node.fragment[j];
                if ((Fragment.name == "Mesh" && Fragment.vertices.size() > 9 && Fragment.vertices.size() % 9 == 0) ||
                    Fragment.name == "Elliptical" || 
                    Fragment.name == "Cylinder")
                {
                    bValidComponent = true;
                    break;
                }
            }
            if (!bValidComponent)
                continue;

            FName ComponentName(FString::FromInt(Node.dbid));
            UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(Actor, ComponentName);
            UStaticMesh* StaticMesh = BuildStaticMesh(Node, StaticMeshComponent, ComponentName);
            if (StaticMesh)
            {
                StaticMeshComponent->SetStaticMesh(StaticMesh);
                StaticMeshComponent->SetMaterial(0, CreateMaterialInstanceDynamic(SourceMaterial, Node.material));
                StaticMeshComponent->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            }
        }
    }

    Actor->RegisterAllComponents();
#endif
}