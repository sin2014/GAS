// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/UObjectIterator.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Engine/World.h"

#include "LyraLogChannels.h"

//////////////////////////////////////////////////////////////////////////

#if ALLOW_DEBUG_FILES
#include "HAL/IConsoleManager.h"

// 将给定条目列表写成指定名称的静态 Collection 文件，并返回绝对路径。
// 这里直接生成文件而不依赖 Collection Manager，使该命令可在运行时使用且无需链接开发者模块。
// Writes a collection of the specified name containing a list of items (returning the absolute file path to the collection)
// This is manual rather than relying on the collection manager so it can be used at runtime without depending on a developer module
FString WriteCollectionFile(const FString& CollectionName, const TArray<FString>& Items)
{
	// 编辑器中写入 CST_Local 使用的 Saved/Collections；运行时写入性能分析目录，便于后续收集。
	// If in the editor, create it in the directory that CST_Local would have used, otherwise write it to the profiling dir for later harvesting
	const FString OutputDir = WITH_EDITOR ? (FPaths::ProjectSavedDir() / TEXT("Collections")) : (FPaths::ProfilingDir() / TEXT("AssetSnapshots"));

	IFileManager::Get().MakeDirectory(*OutputDir, true);

	const FString LogFilename = OutputDir / (CollectionName + TEXT(".collection"));

	if (FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter(*LogFilename))
	{
		const FGuid CollectionGUID = FGuid::NewGuid();

		OutputFile->Logf(TEXT("FileVersion:2"));
		OutputFile->Logf(TEXT("Type:Static"));
		OutputFile->Logf(TEXT("Guid:%s"), *CollectionGUID.ToString(EGuidFormats::DigitsWithHyphens));
		OutputFile->Logf(TEXT(""));

		for (const FString& Item : Items)
		{
			OutputFile->Logf(TEXT("%s"), *Item);
		}

		// 删除 FArchive 会刷新缓冲区并关闭文件句柄。
		// Flush, close and delete.
		delete OutputFile;

		const FString AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*LogFilename);
		return AbsolutePath;
	}

	return FString();
}

// 注册 Lyra.ObjListToCollection 命令，将当前已加载资产快照写入 Collection 文件。
FAutoConsoleCommandWithWorldAndArgs GObjListToCollectionCmd(
	TEXT("Lyra.ObjListToCollection"),
	TEXT("Spits out a collection that contains the current object list"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
		[](const TArray<FString>& Params, UWorld* World)
{
	// 枚举当前已加载资产；蓝图生成类转换回不带 _C 后缀的蓝图资产路径。
	// Get the list of loaded assets
	TArray<FString> AssetPaths;
	for (TObjectIterator<UObject> It; It; ++It)
	{
		UObject* Obj = *It;
		if (Obj->IsAsset())
		{
			AssetPaths.Add(Obj->GetPathName());
		}
		else if (UBlueprintGeneratedClass* Class = Cast<UBlueprintGeneratedClass>(Obj))
		{
			FString BlueprintName = Class->GetPathName();
			BlueprintName.RemoveFromEnd(TEXT("_C"));
			AssetPaths.Add(BlueprintName);
		}
	}
	AssetPaths.Sort();

	// 用当前时间、地图名和可选命令参数组成快照文件名。
	// Determine the filename
	FString CollectionNameSuffix;
	if (Params.Num() > 0)
	{
		CollectionNameSuffix = TEXT("_") + Params[0];
	}
	const FString CollectionName = FString::Printf(TEXT("_LoadedAssets_%s_%s%s"), *FDateTime::Now().ToString(TEXT("%H%M%S")), *GWorld->GetMapName(), *CollectionNameSuffix);

	// 将排序后的已加载资产路径写入 Collection 文件。
	// Write the collection out
	const FString CollectionFilePath = WriteCollectionFile(CollectionName, AssetPaths);
	UE_LOG(LogLyra, Warning, TEXT("Wrote collection of loaded assets to %s"), *CollectionFilePath);
}));

#endif

//////////////////////////////////////////////////////////////////////////

// 比较一组对象与共同父类 CDO 的属性值，找出所有实例都固定为同一非默认值的字段，辅助判断能否移回父类并减少内存占用。
// This can be used in a command to compare assets to a parent class (BP or C++ default) to determine if any fields are actually 'fixed' and can be removed to save memory
void AnalyzeObjectListForDifferences(TArrayView<UObject*> ObjectList, UClass* CommonClass, const TSet<FName>& PropertiesToIgnore, bool bLogAllMatchedDefault=false)
{
	check(CommonClass);
	UObject* CommonClassCDO = CommonClass->GetDefaultObject();

	UE_LOG(LogLyra, Log, TEXT("  Field\tDifferentToBase\tNumValues\tValues"));

	for (TFieldIterator<FProperty> PropIt(CommonClass); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		if (PropertiesToIgnore.Contains(Prop->GetFName()))
		{
			continue;
		}

		// TODO：当前只支持标量属性，尚未处理固定长度数组。
		//@TODO: Handle fixed length arrays
		ensure(Prop->ArrayDim <= 1);

		FString DefaultValueStr;
		Prop->ExportText_InContainer(0, /*out*/ DefaultValueStr, CommonClassCDO, CommonClassCDO, nullptr, 0);

		bool bAnyMatchedDefaultValue = false;
		bool bAllMatchedDefaultValue = true;

		TSet<FString> ValuesObserved;
		for (UObject* Object : ObjectList)
		{
			FString ValueStr;
			if (Prop->ExportText_InContainer(0, /*out*/ ValueStr, Object, CommonClassCDO, nullptr, 0))
			{
				ValuesObserved.Add(ValueStr);
				bAllMatchedDefaultValue = false;
			}
			else
			{
				bAnyMatchedDefaultValue = true;
			}
		}

		if (bAnyMatchedDefaultValue)
		{
			ValuesObserved.Add(DefaultValueStr);
		}

		if (!bAllMatchedDefaultValue)
		{
			const FString ValueList = FString::Join(ValuesObserved, TEXT(","));

			UE_LOG(LogLyra, Log, TEXT("  %s::%s\t%s\t%d\t%s"),
				*CommonClass->GetName(),
				*Prop->GetName(),
				(ValuesObserved.Num() == 1) ? TEXT("FixedDifferent") : TEXT("Varies"),
				ValuesObserved.Num(),
				*ValueList);
		}
		else if (bLogAllMatchedDefault)
		{
			UE_LOG(LogLyra, Log, TEXT("  %s::%s\t%s"), *CommonClass->GetName(), *Prop->GetName(), TEXT("Default"));
		}
	}
}

//////////////////////////////////////////////////////////////////////////

