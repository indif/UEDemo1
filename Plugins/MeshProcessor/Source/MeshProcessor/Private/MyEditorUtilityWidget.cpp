// Fill out your copyright notice in the Description page of Project Settings.


#include "MyEditorUtilityWidget.h"
#include "MeshProcessor.h"

void UMyEditorUtilityWidget::CombineStaticMeshes()
{
    ::CombineStaticMeshes(this, AssetName, InfoTable);
}