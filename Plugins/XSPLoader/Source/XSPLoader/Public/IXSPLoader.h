// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IXSPLoader
{
public:
	/**
	 *	设置数据源文件列表
	 *	@param	FilePathNameArray [in]	xsp文件数组，按dbid顺序排列
	 */
	virtual void SetSourceFiles(const TArray<FString>& FilePathNameArray) = 0;

	/**
	 *	请求静态网格数据（数据加载完毕后会自动设置到目标组件上）
	 *	@param	Dbid				[in]	请求的节点
	 *  @param	Priority			[in]	优先级
	 *  @param	TargetMeshComponent	[in]	目标组件
	 */
	virtual void RequestStaticMeshe(int32 Dbid, float Priority, TWeakObjectPtr<UStaticMeshComponent>& TargetMeshComponent) = 0;
};
