#if WITH_DEV_AUTOMATION_TESTS

#include "UERingExportManager.h"
#include "Tests/UERingTestSaveScope.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraphSchema_K2.h"
#include "Engine/UserDefinedEnum.h"
#include "HAL/FileManager.h"
#include "Kismet2/StructureEditorUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "UObject/SavePackage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUERingDefinitionExporterTest,
    "UERing.Exporter.Definitions.StructAndEnum",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    bool SaveDefinition(UObject* Asset, FString& OutFilename)
    {
        FUERingTestSaveScope SaveScope;
        OutFilename = FPackageName::LongPackageNameToFilename(
            Asset->GetOutermost()->GetName(),
            FPackageName::GetAssetPackageExtension());
        FAssetRegistryModule::AssetCreated(Asset);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Asset->GetOutermost(), Asset, *OutFilename, Args);
    }
}

bool FUERingDefinitionExporterTest::RunTest(const FString& Parameters)
{
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TArray<FString> SourceFiles;
    TArray<FString> SemanticFiles;

    const FString StructPackageName = TEXT("/Game/UERingTests/ST_Semantic_") + Suffix;
    UPackage* StructPackage = CreatePackage(*StructPackageName);
    UUserDefinedStruct* Struct = FStructureEditorUtils::CreateUserDefinedStruct(
        StructPackage,
        *FPackageName::GetLongPackageAssetName(StructPackageName),
        RF_Public | RF_Standalone);
    if (TestNotNull(TEXT("User-defined struct creates"), Struct))
    {
        FEdGraphTerminalType MapValue;
        MapValue.TerminalCategory = UEdGraphSchema_K2::PC_Int;
        const FEdGraphPinType MapType(
            UEdGraphSchema_K2::PC_Name,
            NAME_None,
            nullptr,
            EPinContainerType::Map,
            false,
            MapValue);
        TestTrue(TEXT("Struct map field adds"), FStructureEditorUtils::AddVariable(Struct, MapType));
        const TArray<FStructVariableDescription>& Variables = FStructureEditorUtils::GetVarDesc(Struct);
        if (TestTrue(TEXT("Struct field description exists"), !Variables.IsEmpty()))
        {
            const FGuid FieldGuid = Variables.Last().VarGuid;
            TestTrue(
                TEXT("Struct field renames"),
                FStructureEditorUtils::RenameVariable(Struct, FieldGuid, TEXT("ScoresByName")));
        }
        FStructureEditorUtils::CompileStructure(Struct);

        FString SourceFile;
        if (TestTrue(TEXT("User-defined struct saves"), SaveDefinition(Struct, SourceFile)))
        {
            SourceFiles.Add(SourceFile);
            const FUERingExportResult Result = FUERingExportManager::Get().ExportAsset(FAssetData(Struct));
            TestTrue(TEXT("User-defined struct exports"), Result.IsSuccess());
            SemanticFiles.Add(Result.OutputFile);
            FString Json;
            TestTrue(TEXT("Struct semantic reads"), FFileHelper::LoadFileToString(Json, *Result.OutputFile));
            TestTrue(TEXT("Struct uses Definition exporter"), Json.Contains(TEXT("\"exporter\":\"Definition\"")));
            TestTrue(TEXT("Struct has specialized kind"), Json.Contains(TEXT("\"kind\":\"UserDefinedStruct\"")));
            TestTrue(
                TEXT("Struct map field preserves value type"),
                Json.Contains(TEXT("\"container\":\"map\"")) && Json.Contains(TEXT("\"valueType\"")));
        }
    }

    const FString EnumPackageName = TEXT("/Game/UERingTests/E_Semantic_") + Suffix;
    UPackage* EnumPackage = CreatePackage(*EnumPackageName);
    UUserDefinedEnum* Enum = NewObject<UUserDefinedEnum>(
        EnumPackage,
        *FPackageName::GetLongPackageAssetName(EnumPackageName),
        RF_Public | RF_Standalone);
    TArray<TPair<FName, int64>> EmptyEnumerators;
    TestTrue(
        TEXT("User-defined enum initializes"),
        Enum->SetEnums(
            EmptyEnumerators,
            UEnum::ECppForm::Namespaced,
            UEnum::EUnderlyingType::uint8,
            EEnumFlags::None,
            UEnum::EAddMaxKeyIfMissing::Yes));
    TArray<TPair<FName, int64>> Enumerators;
    Enumerators.Emplace(*Enum->GenerateFullEnumName(TEXT("ChoiceA")), 0);
    Enumerators.Emplace(*Enum->GenerateFullEnumName(TEXT("ChoiceB")), 1);
    TestTrue(
        TEXT("User-defined enum values set"),
        Enum->SetEnums(
            Enumerators,
            UEnum::ECppForm::Namespaced,
            UEnum::EUnderlyingType::uint8,
            EEnumFlags::None,
            UEnum::EAddMaxKeyIfMissing::Yes));
    FString EnumSourceFile;
    if (TestTrue(TEXT("User-defined enum saves"), SaveDefinition(Enum, EnumSourceFile)))
    {
        SourceFiles.Add(EnumSourceFile);
        const FUERingExportResult Result = FUERingExportManager::Get().ExportAsset(FAssetData(Enum));
        TestTrue(TEXT("User-defined enum exports"), Result.IsSuccess());
        SemanticFiles.Add(Result.OutputFile);
        FString Json;
        TestTrue(TEXT("Enum semantic reads"), FFileHelper::LoadFileToString(Json, *Result.OutputFile));
        TestTrue(TEXT("Enum uses Definition exporter"), Json.Contains(TEXT("\"exporter\":\"Definition\"")));
        TestTrue(TEXT("Enum has specialized kind"), Json.Contains(TEXT("\"kind\":\"UserDefinedEnum\"")));
        TestTrue(TEXT("Enum entries export"), Json.Contains(TEXT("ChoiceA")) && Json.Contains(TEXT("ChoiceB")));
    }

    for (const FString& File : SemanticFiles)
    {
        IFileManager::Get().Delete(*File, false, true);
        IFileManager::Get().Delete(*FPaths::ChangeExtension(File, TEXT("md")), false, true);
        IFileManager::Get().Delete(*FPaths::ChangeExtension(File, TEXT("zh-CN.md")), false, true);
    }
    for (const FString& File : SourceFiles)
    {
        IFileManager::Get().Delete(*File, false, true);
    }
    if (!SourceFiles.IsEmpty())
    {
        IFileManager::Get().DeleteDirectory(*FPaths::GetPath(SourceFiles[0]), false, false);
    }
    if (!SemanticFiles.IsEmpty())
    {
        IFileManager::Get().DeleteDirectory(*FPaths::GetPath(SemanticFiles[0]), false, false);
    }
    return !HasAnyErrors();
}

#endif
