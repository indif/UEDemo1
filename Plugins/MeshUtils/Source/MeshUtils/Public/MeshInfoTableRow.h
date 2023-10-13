#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MeshInfoTableRow.generated.h"

//记录原始StaticMesh信息的表格数据结构
USTRUCT(BlueprintType)
struct FMeshInfoTableRow : public FTableRowBase
{
    GENERATED_USTRUCT_BODY()

public:
    FMeshInfoTableRow() {};

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mesh Info")
    FName ID;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mesh Info")
    int32 StartFaceIndex;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Mesh Info")
    int32 FaceCount;
};