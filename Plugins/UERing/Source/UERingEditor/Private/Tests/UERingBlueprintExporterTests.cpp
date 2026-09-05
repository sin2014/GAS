#if WITH_DEV_AUTOMATION_TESTS

#include "UERingExportManager.h"
#include "UERingVersion.h"
#include "Tests/UERingTestSaveScope.h"
#include "Tests/UERingNativeBindWidgetTestBase.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_FunctionEntry.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUERingBlueprintExporterTest,
    "UERing.Exporter.Blueprint.SingleAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUERingBlueprintExporterTest::RunTest(const FString& Parameters)
{
    const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString AssetNameString = TEXT("BP_UERingExportTest_") + UniqueSuffix;
    const FString PackageName = TEXT("/Game/UERingTests/") + AssetNameString;
    const FName AssetName(*AssetNameString);
    const FString PackageFilename = FPackageName::LongPackageNameToFilename(
        PackageName,
        FPackageName::GetAssetPackageExtension());

    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        AUERingBlueprintDefaultsTestBase::StaticClass(),
        Package,
        AssetName,
        BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        TEXT("UERingBlueprintExporterTest"));
    if (!TestNotNull(TEXT("Blueprint is created"), Blueprint))
    {
        return false;
    }

    FEdGraphTerminalType MapValueType;
    MapValueType.TerminalCategory = UEdGraphSchema_K2::PC_Int;
    const FEdGraphPinType MapType(
        UEdGraphSchema_K2::PC_Name,
        NAME_None,
        nullptr,
        EPinContainerType::Map,
        false,
        MapValueType);
    TestTrue(
        TEXT("Map member variable is added"),
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SemanticLookup"), MapType));

    UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        TEXT("AnalyzeSemantic"),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, FunctionGraph, true, nullptr);
    for (UEdGraphNode* Node : FunctionGraph->Nodes)
    {
        if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
        {
            Entry->MetaData.Category = FText::FromString(TEXT("Semantic Test"));
            Entry->MetaData.Keywords = FText::FromString(TEXT("offline analysis"));
            Entry->MetaData.bCallInEditor = true;
            Entry->SetExtraFlags(FUNC_Public | FUNC_BlueprintCallable);
            FBPVariableDescription Local;
            Local.VarName = TEXT("LocalScore");
            Local.VarGuid = FGuid::NewGuid();
            Local.VarType.PinCategory = UEdGraphSchema_K2::PC_Real;
            Local.VarType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
            Local.DefaultValue = TEXT("3.5");
            Entry->LocalVariables.Add(Local);
            break;
        }
    }
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (AUERingBlueprintDefaultsTestBase* Defaults =
        Cast<AUERingBlueprintDefaultsTestBase>(Blueprint->GeneratedClass->GetDefaultObject()))
    {
        Defaults->InheritedSemanticValue = 42;
        Defaults->InstancedSemanticConfig = NewObject<USceneComponent>(
            Defaults,
            TEXT("InstancedSemanticConfig"));
        Defaults->InstancedSemanticConfig->ComponentTags.Add(TEXT("SemanticQuantity_9"));
    }

    FAssetRegistryModule::AssetCreated(Blueprint);
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    bool bSaved = false;
    {
        FUERingTestSaveScope SaveScope;
        bSaved = TestTrue(
            TEXT("Blueprint package is saved"),
            UPackage::SavePackage(Package, Blueprint, *PackageFilename, SaveArgs));
    }
    if (!bSaved)
    {
        IFileManager::Get().Delete(*PackageFilename, false, true);
        return false;
    }

    const FAssetData AssetData(Blueprint);
    const FUERingExportResult FirstResult = FUERingExportManager::Get().ExportAsset(AssetData);
    TestTrue(TEXT("First export succeeds"), FirstResult.IsSuccess());
    TestEqual(TEXT("First export writes a file"), FirstResult.Status, EUERingExportStatus::Exported);
    TestTrue(TEXT("Semantic file exists"), IFileManager::Get().FileExists(*FirstResult.OutputFile));

    FString JsonText;
    TSharedPtr<FJsonObject> Root;
    if (TestTrue(
            TEXT("Semantic file can be read"),
            FFileHelper::LoadFileToString(JsonText, *FirstResult.OutputFile)))
    {
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
        TestTrue(TEXT("Semantic file is valid JSON"), FJsonSerializer::Deserialize(Reader, Root));
    }

    if (Root.IsValid())
    {
        TestEqual(
            TEXT("Schema identifier"),
            Root->GetStringField(TEXT("schema")),
            FString(TEXT("com.ue-ring.usem.asset")));
        TestEqual(
            TEXT("Schema version"),
            Root->GetStringField(TEXT("schemaVersion")),
            FString(UE_RING_SCHEMA_VERSION));
        TestEqual(
            TEXT("Semantic generator revision"),
            static_cast<int32>(Root->GetNumberField(TEXT("semanticRevision"))),
            UE_RING_SEMANTIC_REVISION);
        TestEqual(TEXT("Exporter is a readable name"), Root->GetStringField(TEXT("exporter")), FString(TEXT("Blueprint")));
        TestFalse(TEXT("Engine metadata is centralized in the project index"), Root->HasField(TEXT("engine")));
        TestFalse(TEXT("Project metadata is centralized in the project index"), Root->HasField(TEXT("project")));

        const TSharedPtr<FJsonObject>* Reconstruction = nullptr;
        if (TestTrue(
                TEXT("Reconstruction IR exists"),
                Root->TryGetObjectField(TEXT("reconstruction"), Reconstruction)))
        {
            TestEqual(
                TEXT("Reconstruction IR version is executable v2"),
                (*Reconstruction)->GetStringField(TEXT("irVersion")),
                FString(TEXT("2.0.0")));
            TestEqual(
                TEXT("Reconstruction contract is explicit"),
                (*Reconstruction)->GetStringField(TEXT("contract")),
                FString(TEXT("com.ue-ring.reconstruction")));
            const TArray<TSharedPtr<FJsonValue>>& Targets =
                (*Reconstruction)->GetArrayField(TEXT("targets"));
            if (TestEqual(TEXT("Reconstruction has one backend target"), Targets.Num(), 1))
            {
                const TSharedPtr<FJsonObject> Target = Targets[0]->AsObject();
                TestEqual(TEXT("Blueprint target is native C++"),
                    Target->GetStringField(TEXT("target")), FString(TEXT("nativeClassCpp")));
                TestEqual(TEXT("Blueprint target uses the UE C++ backend"),
                    Target->GetStringField(TEXT("backend")), FString(TEXT("ueCpp")));
                TestTrue(TEXT("Target exposes derived readiness"),
                    Target->GetStringField(TEXT("status")) == TEXT("ready")
                        || Target->GetStringField(TEXT("status")) == TEXT("partial")
                        || Target->GetStringField(TEXT("status")) == TEXT("blocked"));
            }
            const TArray<TSharedPtr<FJsonValue>>& Operations =
                (*Reconstruction)->GetArrayField(TEXT("operations"));
            TestTrue(
                TEXT("Reconstruction provides typed source-backed operations"),
                Operations.Num() > 0
                    && Operations[0]->AsObject()->HasField(TEXT("opcode"))
                    && Operations[0]->AsObject()->GetArrayField(TEXT("sourcePointers")).Num() > 0);
            for (int32 Index = 1; Index < Operations.Num(); ++Index)
            {
                TestTrue(
                    TEXT("Reconstruction operations use canonical case-sensitive ID order"),
                    Operations[Index - 1]->AsObject()->GetStringField(TEXT("id")).Compare(
                        Operations[Index]->AsObject()->GetStringField(TEXT("id")),
                        ESearchCase::CaseSensitive) <= 0);
            }
            const TSharedPtr<FJsonObject>* Coverage = nullptr;
            if (TestTrue(TEXT("Reconstruction coverage exists"),
                    (*Reconstruction)->TryGetObjectField(TEXT("coverage"), Coverage)))
            {
                const double ExactRatio = (*Coverage)->GetNumberField(TEXT("exactRatio"));
                TestTrue(TEXT("Exact operation ratio is bounded"),
                    ExactRatio >= 0.0 && ExactRatio <= 1.0);
                TestEqual(TEXT("Coverage counts every operation"),
                    static_cast<int32>((*Coverage)->GetNumberField(TEXT("totalOperationCount"))),
                    Operations.Num());
            }
            const TSharedPtr<FJsonObject>* Execution = nullptr;
            if (TestTrue(TEXT("Reconstruction execution summary exists"),
                    (*Reconstruction)->TryGetObjectField(TEXT("execution"), Execution)))
            {
                TestEqual(TEXT("Execution counts every operation"),
                    static_cast<int32>((*Execution)->GetNumberField(TEXT("operationCount"))),
                    Operations.Num());
            }
        }

        const TSharedPtr<FJsonObject>* AssetObject = nullptr;
        if (TestTrue(TEXT("Asset object exists"), Root->TryGetObjectField(TEXT("asset"), AssetObject)))
        {
            TestEqual(
                TEXT("Package name is exported"),
                (*AssetObject)->GetStringField(TEXT("packageName")),
                PackageName);
            TestTrue(
                TEXT("Source hash is SHA-256"),
                (*AssetObject)->GetStringField(TEXT("sourceHash")).StartsWith(TEXT("sha256:")));
            TestFalse(TEXT("Sidecar path is owned by the project index"), (*AssetObject)->HasField(TEXT("semanticFile")));
            TestFalse(TEXT("Export timestamp is owned by the project index"), (*AssetObject)->HasField(TEXT("exportedAtUtc")));
        }

        const TSharedPtr<FJsonObject>* Semantics = nullptr;
        if (TestTrue(TEXT("Semantics object exists"), Root->TryGetObjectField(TEXT("semantics"), Semantics)))
        {
            TestEqual(
                TEXT("Blueprint semantic kind"),
                (*Semantics)->GetStringField(TEXT("kind")),
                FString(TEXT("Blueprint")));
            TestTrue(TEXT("Empty graph arrays use the documented omitted default"),
                !(*Semantics)->HasField(TEXT("graphs")) || (*Semantics)->GetArrayField(TEXT("graphs")).Num() > 0);
            TestTrue(TEXT("Empty variable arrays use the documented omitted default"),
                !(*Semantics)->HasField(TEXT("variables")) || (*Semantics)->GetArrayField(TEXT("variables")).Num() > 0);
            TestTrue(TEXT("Empty component arrays use the documented omitted default"),
                !(*Semantics)->HasField(TEXT("components")) || (*Semantics)->GetArrayField(TEXT("components")).Num() > 0);
            const FString SemanticsText = JsonText;
            TestTrue(
                TEXT("Map key and value types are structured"),
                SemanticsText.Contains(TEXT("\"container\":\"map\""))
                    && SemanticsText.Contains(TEXT("\"valueType\"")));
            TestTrue(
                TEXT("Inherited class default overrides are exported"),
                SemanticsText.Contains(TEXT("\"classDefaults\""))
                    && SemanticsText.Contains(TEXT("InheritedSemanticValue")));
            TestTrue(
                TEXT("Instanced class default objects and their authored values are exported"),
                SemanticsText.Contains(TEXT("\"classDefaultOwnedObjects\""))
                    && SemanticsText.Contains(TEXT("InstancedSemanticConfig"))
                    && SemanticsText.Contains(TEXT("ComponentTags"))
                    && SemanticsText.Contains(TEXT("SemanticQuantity_9")));
            TestTrue(
                TEXT("Function metadata and locals are exported"),
                SemanticsText.Contains(TEXT("\"functionMetadata\""))
                    && SemanticsText.Contains(TEXT("\"localVariables\""))
                    && SemanticsText.Contains(TEXT("LocalScore")));
        }

        const TArray<TSharedPtr<FJsonValue>>* CppLinks = nullptr;
        if (TestTrue(TEXT("C++ links are exported"), Root->TryGetArrayField(TEXT("cppLinks"), CppLinks))
            && CppLinks != nullptr)
        {
            bool bFoundNativeParent = false;
            for (const TSharedPtr<FJsonValue>& Value : *CppLinks)
            {
                const TSharedPtr<FJsonObject>* Link = nullptr;
                if (Value.IsValid() && Value->TryGetObject(Link) && Link != nullptr
                    && (*Link)->GetStringField(TEXT("assetNode")) == TEXT("$parentClass"))
                {
                    bFoundNativeParent = true;
                    TestFalse(TEXT("Native parent link resolves a header"), (*Link)->GetStringField(TEXT("header")).IsEmpty());
                }
            }
            TestTrue(TEXT("Blueprint links to its native parent class"), bFoundNativeParent);
        }
    }

    const FUERingExportResult SecondResult = FUERingExportManager::Get().ExportAsset(AssetData);
    TestTrue(TEXT("Second export succeeds"), SecondResult.IsSuccess());
    TestEqual(TEXT("Unchanged source is skipped"), SecondResult.Status, EUERingExportStatus::Unchanged);

    if (!FirstResult.OutputFile.IsEmpty())
    {
        IFileManager::Get().Delete(*FirstResult.OutputFile, false, true);
        IFileManager::Get().Delete(*FPaths::ChangeExtension(FirstResult.OutputFile, TEXT("md")), false, true);
        IFileManager::Get().Delete(*FPaths::ChangeExtension(FirstResult.OutputFile, TEXT("zh-CN.md")), false, true);
    }
    IFileManager::Get().Delete(*PackageFilename, false, true);
    IFileManager::Get().DeleteDirectory(*FPaths::GetPath(PackageFilename), false, false);
    if (!FirstResult.OutputFile.IsEmpty())
    {
        IFileManager::Get().DeleteDirectory(*FPaths::GetPath(FirstResult.OutputFile), false, false);
    }
    return !HasAnyErrors();
}

#endif
