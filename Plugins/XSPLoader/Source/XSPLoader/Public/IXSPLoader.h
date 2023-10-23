// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IXSPLoader
{
public:
	virtual ~IXSPLoader() {};

	/**
	 *	初始化
	 *	@param	FilePathNameArray [in]	数据源文件数组，按dbid顺序排列
	 */
	virtual bool Init(const TArray<FString>& FilePathNameArray) = 0;

	/**
	 *	重置
	 */
	virtual void Reset() = 0;

	/**
	 *	请求静态网格数据（数据加载完毕后会自动设置到目标组件上）
	 *	@param	Dbid				[in]	请求的节点
	 *  @param	Priority			[in]	优先级
	 *  @param	TargetMeshComponent	[in]	目标组件
	 */
	virtual void RequestStaticMeshe(int32 Dbid, float Priority, UStaticMeshComponent* TargetMeshComponent) = 0;
};
