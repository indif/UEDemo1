// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshProcessor.h"

#include "EditorUtilityLibrary.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MeshInfoTableRow.h"
#include "RawMesh.h"

#define LOCTEXT_NAMESPACE "FMeshProcessorModule"

void FMeshProcessorModule::StartupModule()
{
}

void FMeshProcessorModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMeshProcessorModule, MeshProcessor)

void CombineStaticMeshes(UObject* Outer, FString OutAssetName, class UDataTable* OutInfoTable)
{
    if (!Outer)
        return;
    if (OutAssetName.IsEmpty())
        return;
    if (!OutInfoTable)
        return;

    //获取选中的静态网格资产
    FString AssetPath;
    TArray<FAssetData> StaticMesheAssetArray;
    TArray<FAssetData> SelectedAssetDataArray = UEditorUtilityLibrary::GetSelectedAssetData();
    for (FAssetData& AssetData : SelectedAssetDataArray)
    {
        if (AssetData.GetClass() == UStaticMesh::StaticClass())
            StaticMesheAssetArray.Emplace(AssetData);
        if (AssetPath.IsEmpty())
            AssetPath = AssetData.PackagePath.ToString() + TEXT("/") + OutAssetName;
    }
    if (StaticMesheAssetArray.Num() == 0)
        return;

    //按资产名称排序，在DataTable中便于浏览（没有其他实际意义）
    StaticMesheAssetArray.StableSort(
        [](const FAssetData& A, const FAssetData& B) {return A.AssetName.ToString() < B.AssetName.ToString(); }
    );

    //遍历所有待合并的静态网格资产，取其RawMesh，合并为目标RawMesh，并记录各个源RawMesh的信息
    FRawMesh DestRawMesh;
    int32 TotalFaceCount = 0;
    int32 TotalWedgeCount = 0;
    int32 TotalVertexCount = 0;
    for (FAssetData& AssetData : StaticMesheAssetArray)
    {
        UStaticMesh* StaticMesh = Cast<UStaticMesh>(AssetData.GetAsset());
        const TArray<FStaticMeshSourceModel>& SrcModelArray = StaticMesh->GetSourceModels();
        check(SrcModelArray.Num() == 1);
        const FStaticMeshSourceModel& SrcModel = SrcModelArray[0];
        FRawMesh RawMesh;
        SrcModel.LoadRawMesh(RawMesh);

        int32 NumWedges = RawMesh.WedgeIndices.Num();
        int32 NumFaces = NumWedges / 3;
        int32 NumVertices = RawMesh.VertexPositions.Num();

        //置为0
        DestRawMesh.FaceMaterialIndices.AddZeroed(NumFaces);
        DestRawMesh.FaceSmoothingMasks.AddZeroed(NumFaces);

        //角点信息直接追加
        DestRawMesh.WedgeTangentX.Append(RawMesh.WedgeTangentX);
        DestRawMesh.WedgeTangentY.Append(RawMesh.WedgeTangentY);
        DestRawMesh.WedgeTangentZ.Append(RawMesh.WedgeTangentZ);
        DestRawMesh.WedgeTexCoords[0].Append(RawMesh.WedgeTexCoords[0]);
        DestRawMesh.WedgeColors.Append(RawMesh.WedgeColors);

        //顶点坐标直接追加
        DestRawMesh.VertexPositions.Append(RawMesh.VertexPositions);

        //角点的顶点坐标索引增加偏移量
        DestRawMesh.WedgeIndices.AddUninitialized(NumWedges);
        for (int32 i = 0; i < NumWedges; ++i)
        {
            DestRawMesh.WedgeIndices[TotalWedgeCount + i] = TotalVertexCount + RawMesh.WedgeIndices[i];
        }

        //记录到数据表
        FMeshInfoTableRow RowData;
        RowData.ID = AssetData.AssetName;
        RowData.StartFaceIndex = TotalFaceCount;
        RowData.FaceCount = NumFaces;
        OutInfoTable->AddRow(AssetData.AssetName, RowData);

        TotalFaceCount += NumFaces;
        TotalWedgeCount += NumWedges;
        TotalVertexCount += NumVertices;
    }

    if (!DestRawMesh.IsValid())
        return;

    //创建静态网格资产
    UPackage* NewMeshPack = CreatePackage(Outer, *AssetPath);
    UStaticMesh* NewStaticMesh = NewObject<UStaticMesh>(NewMeshPack, FName(*OutAssetName), RF_Public | RF_Standalone);
    NewStaticMesh->InitResources();
    NewStaticMesh->PreEditChange(nullptr);

    //合并的RawMesh设置给新建的静态网格资产，只设置LOD0
    FStaticMeshSourceModel& AddedSrcModel = NewStaticMesh->AddSourceModel();
    AddedSrcModel.BuildSettings.bRecomputeNormals = false;
    AddedSrcModel.BuildSettings.bRecomputeTangents = false;
    AddedSrcModel.BuildSettings.bRemoveDegenerates = false;
    AddedSrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
    AddedSrcModel.BuildSettings.bUseFullPrecisionUVs = false;
    AddedSrcModel.BuildSettings.bGenerateLightmapUVs = false;
    AddedSrcModel.BuildSettings.SrcLightmapIndex = 0;
    AddedSrcModel.BuildSettings.DstLightmapIndex = 0;
    AddedSrcModel.SaveRawMesh(DestRawMesh);

    //交给编辑器构件静态网格资产
    TArray<FText> BuildErrors;
    NewStaticMesh->Build(true, &BuildErrors); 
    
    //通知编辑器
    NewStaticMesh->PostEditChange();
    FAssetRegistryModule::AssetCreated(NewStaticMesh);
    NewMeshPack->MarkPackageDirty();

    /* 问题： 
    * 参考 UE::Private::StaticMeshBuilder::BuildVertexBuffer
    * Build的过程中，输入RawMesh的顶点被重排（合并重合顶点），导致最终渲染（和构建碰撞体）的RenderData的FaceIndex与RawMesh的不同
    */

    /* 解决方案A:
    * 修改引擎将BuildVertexBuffer中合并重合顶点的逻辑屏蔽掉，即可使构造的RenderData与输入RawMesh的FaceIndex一致
    * 拾取查询仍按原方案使用FaceIndex查询DataTable
    */
    //引擎StaticMeshBuilder.cpp文件中，BuildVertexBuffer函数，注释掉以下一段代码：
    /*
            for (int32 k = 0; k < DupVerts.Num(); k++)
            {
                if (DupVerts[k] >= WedgeIndex)
                {
                    break;
                }
                int32 Location = RemapVerts.IsValidIndex(DupVerts[k]) ? RemapVerts[DupVerts[k]] : INDEX_NONE;
                if (Location != INDEX_NONE && AreVerticesEqual(StaticMeshVertex, StaticMeshBuildVertices[Location], VertexComparisonThreshold))
                {
                    Index = Location;
                    break;
                }
            }
    */

    /* 解决方案B:
    * WedgeMap记录了RawMesh三角形角点序号到RenderData顶点索引的对应关系
    * 要得到RawMesh三角形对应到RenderData的FaceIndex，除了WedgeMap外还需要RenderData的索引数组(SectionIndices)
    * 需要做如下处理：
    * 1. 修改引擎将SectionIndices存到FStaticMeshSection中
    * 2. 在Build后，利用WedgeMap和SectionIndices计算出RenderData的每一个Face对应到哪个输入的RawMesh（FaceObjectIndexArray）
    * 3. 在拾取查询时，用拾取到的FaceIndex查FaceObjectIndexArray表获取到对应的ObjectIndex，再到DataTable中查其ID
    */
}