#if WITH_DEV_AUTOMATION_TESTS

#include "UERingExportManager.h"
#include "UERingCommandlets.h"
#include "UERingExporterRegistry.h"
#include "UERingBundleBuilder.h"
#include "UERingBlueprintMigrationReporter.h"
#include "UERingCppIndexer.h"
#include "UERingValidator.h"
#include "UERingIndexManager.h"
#include "UERingPaperTileSetExporter.h"
#include "UERingPaperTileMapExporter.h"
#include "UERingPropertySerializer.h"
#include "UERingLevelSequenceExporter.h"
#include "UERingDomainGraphExporter.h"
#include "UERingMaterialExporter.h"
#include "UERingAnimBlueprintExporter.h"
#include "UERingAnimationAssetExporter.h"
#include "UERingAudioAssetExporter.h"
#include "UERingDerivedArtifactWriter.h"
#include "UERingSummaryWriter.h"
#include "UERingReflectionExporter.h"
#include "UERingSettings.h"
#include "Tests/UERingNativeBindWidgetTestBase.h"
#include "Tests/UERingTestSaveScope.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PointLightComponent.h"
#include "Components/RichTextBlockImageDecorator.h"
#include "Components/TextBlock.h"
#include "Curves/SimpleCurve.h"
#include "Engine/Blueprint.h"
#include "Curves/CurveFloat.h"
#include "Engine/CurveTable.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Engine/PrimaryAssetLabel.h"
#include "Engine/PointLight.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Sound/SoundWave.h"
#include "EdGraphSchema_K2.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "LevelSequence.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PaperTileSet.h"
#include "PaperTileLayer.h"
#include "PaperTileMap.h"
#include "MovieScene.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SQLiteDatabase.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "IModelContextProtocolModule.h"
#include "IModelContextProtocolTool.h"
#include "InputModifiers.h"

namespace UERingP0Tests
{
    bool SaveAsset(UPackage* Package, UObject* Asset, const bool bMap, FString& OutFilename)
    {
        FUERingTestSaveScope SaveScope;
        OutFilename = FPackageName::LongPackageNameToFilename(
            Package->GetName(),
            bMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
        FAssetRegistryModule::AssetCreated(Asset);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Asset, *OutFilename, Args);
    }

    bool LoadJsonObject(const FString& Filename, TSharedPtr<FJsonObject>& OutRoot)
    {
        FString Json;
        return FFileHelper::LoadFileToString(Json, *Filename)
            && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), OutRoot)
            && OutRoot.IsValid();
    }

    bool SaveCondensedJsonObject(const FString& Filename, const TSharedRef<FJsonObject>& Root)
    {
        FString Json;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
        return FJsonSerializer::Serialize(Root, Writer)
            && FFileHelper::SaveStringToFile(
                Json + LINE_TERMINATOR,
                *Filename,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    void DeleteExportArtifacts(const FString& SemanticFile)
    {
        IFileManager::Get().Delete(*SemanticFile, false, true);
        IFileManager::Get().Delete(*FPaths::ChangeExtension(SemanticFile, TEXT("md")), false, true);
        IFileManager::Get().Delete(*FPaths::ChangeExtension(SemanticFile, TEXT("zh-CN.md")), false, true);
        FUERingDerivedArtifactWriter::RemoveArtifacts(SemanticFile);
    }

    class FTestSdkExporter final : public IUERingAssetExporter
    {
    public:
        virtual FName GetName() const override { return TEXT("TestSdkExporter"); }
        virtual int32 GetPriority() const override { return 1000; }
        virtual bool CanExport(const FAssetData& AssetData) const override
        {
            return AssetData.IsInstanceOf(UPrimaryAssetLabel::StaticClass());
        }
        virtual bool BuildPayload(
            const FUERingExportContext& Context,
            FUERingSemanticPayload& OutPayload,
            FString& OutError) const override
        {
            OutPayload.Semantics->SetStringField(TEXT("kind"), TEXT("CustomSdkAsset"));
            OutPayload.Semantics->SetStringField(TEXT("representation"), TEXT("test-sdk-v1"));
            OutPayload.AdditionalHardDependencies.Add(TEXT("/Game/UERingTests/AdditionalHard"));
            OutPayload.AdditionalSoftDependencies.Add(TEXT("/Game/UERingTests/AdditionalSoft"));
            return true;
        }
    };

    bool ExportAndCheck(
        FAutomationTestBase& Test,
        UObject* Asset,
        const FString& ExpectedKind,
        FString& OutSemanticFile)
    {
        const FUERingExportResult Result = FUERingExportManager::Get().ExportAsset(FAssetData(Asset));
        if (!Test.TestTrue(*FString::Printf(TEXT("%s export succeeds"), *ExpectedKind), Result.IsSuccess()))
        {
            Test.AddError(Result.Error);
            return false;
        }
        OutSemanticFile = Result.OutputFile;
        FString Json;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(Json, *Result.OutputFile)
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
            || !Root.IsValid())
        {
            Test.AddError(TEXT("Exported semantic JSON could not be parsed."));
            return false;
        }
        const TSharedPtr<FJsonObject>* Semantics = nullptr;
        if (!Root->TryGetObjectField(TEXT("semantics"), Semantics) || Semantics == nullptr)
        {
            Test.AddError(TEXT("Exported semantic JSON has no semantics object."));
            return false;
        }
        return Test.TestEqual(
            *FString::Printf(TEXT("%s semantic kind"), *ExpectedKind),
            (*Semantics)->GetStringField(TEXT("kind")),
            ExpectedKind);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUERingCommandletLoadContextsTest,
    "UERing.Commandlet.LoadContexts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUERingCommandletLoadContextsTest::RunTest(const FString& Parameters)
{
    const UUERingExportCommandlet* Export = GetDefault<UUERingExportCommandlet>();
    const UUERingValidateCommandlet* Validate = GetDefault<UUERingValidateCommandlet>();
    TestTrue(TEXT("Export commandlet loads client-only assets"), Export->IsClient);
    TestTrue(TEXT("Export commandlet loads server-only assets"), Export->IsServer);
    TestTrue(TEXT("Export commandlet loads editor-only assets"), Export->IsEditor);
    TestTrue(TEXT("Validate commandlet loads client-only assets"), Validate->IsClient);
    TestTrue(TEXT("Validate commandlet loads server-only assets"), Validate->IsServer);
    TestTrue(TEXT("Validate commandlet loads editor-only assets"), Validate->IsEditor);
    TestFalse(TEXT("Large-project JSON is compact by default"), GetDefault<UUERingSettings>()->bPrettyJson);
    TestFalse(
        TEXT("GameplayTagQuery token stream is not treated as a credential"),
        UERingPropertySerializer::IsPrivateName(TEXT("QueryTokenStream")));
    TestFalse(
        TEXT("GameplayTagQuery token stream version is not treated as a credential"),
        UERingPropertySerializer::IsPrivateName(TEXT("TokenStreamVersion")));
    TestTrue(
        TEXT("Authentication tokens remain private"),
        UERingPropertySerializer::IsPrivateName(TEXT("ServiceAuthToken")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUERingP0AssetTypesTest,
    "UERing.Exporter.P0.AssetTypes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUERingP0AssetTypesTest::RunTest(const FString& Parameters)
{
    using namespace UERingP0Tests;
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TArray<FString> SourceFiles;
    TArray<FString> SemanticFiles;

    const FString DataAssetPackageName = TEXT("/Game/UERingTests/DA_") + Suffix;
    UPackage* DataAssetPackage = CreatePackage(*DataAssetPackageName);
    UPrimaryAssetLabel* DataAsset = NewObject<UPrimaryAssetLabel>(
        DataAssetPackage, *FPackageName::GetLongPackageAssetName(DataAssetPackageName), RF_Public | RF_Standalone);
    const FString UnloadedReferencePath = TEXT("/Game/UERingTests/UnloadedReference.UnloadedReference");
    DataAsset->ExplicitAssets.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(UnloadedReferencePath)));
    FString Filename;
    if (TestTrue(TEXT("DataAsset saves"), SaveAsset(DataAssetPackage, DataAsset, false, Filename)))
    {
        SourceFiles.Add(Filename);
        FString Semantic;
        ExportAndCheck(*this, DataAsset, TEXT("DataAsset"), Semantic);
        SemanticFiles.Add(Semantic);
        FString DataAssetJson;
        TestTrue(TEXT("DataAsset semantic JSON reads"), FFileHelper::LoadFileToString(DataAssetJson, *Semantic));
        TestTrue(
            TEXT("Unloaded soft object references preserve their object path"),
                DataAssetJson.Contains(UnloadedReferencePath));
        TestTrue(
            TEXT("DataAsset uses reconstructable property semantics"),
            DataAssetJson.Contains(TEXT("\"representation\":\"data-asset-properties-v2\""))
                && DataAssetJson.Contains(TEXT("\"ownedObjects\":[]")));
        TestFalse(
            TEXT("DataAsset omits save-time generated AssetBundleData"),
            DataAssetJson.Contains(TEXT("\"name\":\"AssetBundleData\"")));
        TestTrue(
            TEXT("DataAsset reconstruction emits the create opcode"),
            DataAssetJson.Contains(TEXT("\"opcode\":\"editor.dataAsset.create\"")));
        TestTrue(
            TEXT("DataAsset reconstruction emits the property opcode"),
            DataAssetJson.Contains(TEXT("\"opcode\":\"editor.dataAsset.applyProperties\"")));
    }

    const FString DomainPackageName = TEXT("/Game/UERingTests/IA_Domain_") + Suffix;
    UPackage* DomainPackage = CreatePackage(*DomainPackageName);
    UUERingInputActionDomainTestAsset* DomainAsset = NewObject<UUERingInputActionDomainTestAsset>(
        DomainPackage,
        *FPackageName::GetLongPackageAssetName(DomainPackageName),
        RF_Public | RF_Standalone);
    DomainAsset->OptionalSetValue = 73;
    FPropertyBagPropertyDesc SemanticDistanceDesc(
        TEXT("SemanticDistance"), EPropertyBagPropertyType::Float);
    SemanticDistanceDesc.ID = FGuid(
        0x11111111, 0x22223333, 0x44445555, 0x66667777);
#if WITH_EDITOR
    SemanticDistanceDesc.SetMetaData(TEXT("Category"), TEXT("UERing Test"));
#endif
    DomainAsset->SemanticPropertyBag.AddProperties({ SemanticDistanceDesc });
    DomainAsset->SemanticPropertyBag.SetValueFloat(TEXT("SemanticDistance"), 420.0f);
    FUERingSemanticInstancedStructPayload& InstancedPayload =
        DomainAsset->SemanticInstancedStruct.InitializeAs<FUERingSemanticInstancedStructPayload>();
    InstancedPayload.SemanticScale = 2.5f;
    InstancedPayload.SemanticMode = TEXT("StateTreeCondition");
    UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(
        DomainAsset, TEXT("InputModifierDeadZone_0"), RF_Transactional);
    DeadZone->LowerThreshold = 0.37f;
    DeadZone->UpperThreshold = 0.91f;
    DeadZone->Type = EDeadZoneType::Axial;
    DomainAsset->Modifiers.Add(DeadZone);
    if (TestTrue(TEXT("Domain DataAsset saves"), SaveAsset(DomainPackage, DomainAsset, false, Filename)))
    {
        SourceFiles.Add(Filename);
        FString Semantic;
        ExportAndCheck(*this, DomainAsset, TEXT("DataAsset"), Semantic);
        SemanticFiles.Add(Semantic);
        FString DomainJson;
        TestTrue(TEXT("Domain semantic JSON reads"), FFileHelper::LoadFileToString(DomainJson, *Semantic));
        TestTrue(
            TEXT("Enhanced Input domain projection is exported"),
            DomainJson.Contains(TEXT("\"domain\""))
                && DomainJson.Contains(TEXT("\"enhancedInput\""))
                && DomainJson.Contains(TEXT("\"ValueType\"")));
        TestTrue(
            TEXT("Domain DataAsset uses the executable reflection builder"),
            DomainJson.Contains(TEXT("\"reconstruction\""))
                && DomainJson.Contains(TEXT("\"status\":\"ready\""))
                && DomainJson.Contains(TEXT("editor.dataAsset.applyProperties"))
                && !DomainJson.Contains(TEXT("assetBuilderBackendUnavailable")));
        TestTrue(
            TEXT("DataAsset owned object graph is canonical and reconstructable"),
            DomainJson.Contains(TEXT("\"representation\":\"data-asset-properties-v2\""))
                && DomainJson.Contains(TEXT("\"ownedObjects\""))
                && DomainJson.Contains(TEXT("\"id\":\"InputModifierDeadZone_0\""))
                && DomainJson.Contains(TEXT("\"outerId\":\"$asset\""))
                && DomainJson.Contains(TEXT("\"ownedObjectId\":\"InputModifierDeadZone_0\""))
                && DomainJson.Contains(TEXT("\"name\":\"LowerThreshold\""))
                && DomainJson.Contains(TEXT("editor.dataAsset.createOwnedObjects"))
                && DomainJson.Contains(TEXT("editor.dataAsset.applyOwnedObjectProperties")));
        TSharedPtr<FJsonObject> DomainRoot;
        const TSharedPtr<FJsonObject>* DomainSemantics = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* DomainProperties = nullptr;
        TestTrue(
            TEXT("Domain semantic parses for typed property assertions"),
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(DomainJson), DomainRoot)
                && DomainRoot.IsValid()
                && DomainRoot->TryGetObjectField(TEXT("semantics"), DomainSemantics)
                && DomainSemantics != nullptr
                && (*DomainSemantics)->TryGetArrayField(TEXT("properties"), DomainProperties)
                && DomainProperties != nullptr);
        TSharedPtr<FJsonObject> OptionalSetProperty;
        TSharedPtr<FJsonObject> OptionalUnsetProperty;
        TSharedPtr<FJsonObject> EmptyDelegateProperty;
        TSharedPtr<FJsonObject> PropertyBagProperty;
        TSharedPtr<FJsonObject> InstancedStructProperty;
        if (DomainProperties != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& PropertyValue : *DomainProperties)
            {
                const TSharedPtr<FJsonObject> Property =
                    PropertyValue.IsValid() ? PropertyValue->AsObject() : nullptr;
                FString Name;
                if (!Property.IsValid() || !Property->TryGetStringField(TEXT("name"), Name)) continue;
                if (Name == TEXT("OptionalSetValue")) OptionalSetProperty = Property;
                else if (Name == TEXT("OptionalUnsetValue")) OptionalUnsetProperty = Property;
                else if (Name == TEXT("EmptySemanticDelegate")) EmptyDelegateProperty = Property;
                else if (Name == TEXT("SemanticPropertyBag")) PropertyBagProperty = Property;
                else if (Name == TEXT("SemanticInstancedStruct")) InstancedStructProperty = Property;
            }
        }
        const TSharedPtr<FJsonObject>* OptionalSetValue = nullptr;
        bool bOptionalSet = false;
        double OptionalNumber = 0.0;
        TestTrue(
            TEXT("Set Optional property has a typed inner value"),
            OptionalSetProperty.IsValid()
                && OptionalSetProperty->GetStringField(TEXT("type")).StartsWith(TEXT("TOptional<"))
                && OptionalSetProperty->TryGetObjectField(TEXT("value"), OptionalSetValue)
                && OptionalSetValue != nullptr
                && (*OptionalSetValue)->TryGetBoolField(TEXT("isSet"), bOptionalSet)
                && bOptionalSet
                && (*OptionalSetValue)->TryGetNumberField(TEXT("value"), OptionalNumber)
                && OptionalNumber == 73.0);
        const TSharedPtr<FJsonObject>* OptionalUnsetValue = nullptr;
        bool bOptionalUnset = true;
        TestTrue(
            TEXT("Unset Optional property has no synthetic inner value"),
            OptionalUnsetProperty.IsValid()
                && OptionalUnsetProperty->GetStringField(TEXT("type")).StartsWith(TEXT("TOptional<"))
                && OptionalUnsetProperty->TryGetObjectField(TEXT("value"), OptionalUnsetValue)
                && OptionalUnsetValue != nullptr
                && (*OptionalUnsetValue)->TryGetBoolField(TEXT("isSet"), bOptionalUnset)
                && !bOptionalUnset
                && !(*OptionalUnsetValue)->HasField(TEXT("value")));
        const TSharedPtr<FJsonObject>* EmptyDelegateValue = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* EmptyBindings = nullptr;
        FString DelegateKind;
        TestTrue(
            TEXT("Empty multicast delegates use typed reconstructable semantics"),
            EmptyDelegateProperty.IsValid()
                && EmptyDelegateProperty->TryGetObjectField(TEXT("value"), EmptyDelegateValue)
                && EmptyDelegateValue != nullptr
                && (*EmptyDelegateValue)->TryGetStringField(TEXT("delegateKind"), DelegateKind)
                && DelegateKind == TEXT("multicast")
                && (*EmptyDelegateValue)->TryGetArrayField(TEXT("bindings"), EmptyBindings)
                && EmptyBindings != nullptr
                && EmptyBindings->IsEmpty());
        const TSharedPtr<FJsonObject>* PropertyBagValue = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* BagProperties = nullptr;
        const TSharedPtr<FJsonObject>* BagEntry = nullptr;
        FString BagName;
        FString BagType;
        FString BagFlags;
        double BagVersion = 0.0;
        double BagNumber = 0.0;
        bool bBagValid = false;
        TestTrue(
            TEXT("Instanced Property Bag preserves descriptor identity and typed values"),
            PropertyBagProperty.IsValid()
                && PropertyBagProperty->TryGetObjectField(TEXT("value"), PropertyBagValue)
                && PropertyBagValue != nullptr
                && (*PropertyBagValue)->TryGetNumberField(TEXT("propertyBagVersion"), BagVersion)
                && BagVersion == 1.0
                && (*PropertyBagValue)->TryGetBoolField(TEXT("isValid"), bBagValid)
                && bBagValid
                && (*PropertyBagValue)->TryGetArrayField(TEXT("properties"), BagProperties)
                && BagProperties != nullptr
                && BagProperties->Num() == 1
                && (*BagProperties)[0]->TryGetObject(BagEntry)
                && BagEntry != nullptr
                && (*BagEntry)->TryGetStringField(TEXT("name"), BagName)
                && BagName == TEXT("SemanticDistance")
                && (*BagEntry)->TryGetStringField(TEXT("valueType"), BagType)
                && BagType == TEXT("Float")
                && (*BagEntry)->TryGetStringField(TEXT("propertyFlags"), BagFlags)
                && !BagFlags.IsEmpty()
                && (*BagEntry)->TryGetNumberField(TEXT("value"), BagNumber)
                && BagNumber == 420.0);
        const TSharedPtr<FJsonObject>* InstancedStructValue = nullptr;
        const TSharedPtr<FJsonObject>* InstancedStructFields = nullptr;
        FString InstancedStructType;
        FString InstancedMode;
        double InstancedVersion = 0.0;
        double InstancedScale = 0.0;
        bool bInstancedValid = false;
        TestTrue(
            TEXT("Instanced Struct preserves its dynamic type and typed fields"),
            InstancedStructProperty.IsValid()
                && InstancedStructProperty->TryGetObjectField(
                    TEXT("value"), InstancedStructValue)
                && InstancedStructValue != nullptr
                && (*InstancedStructValue)->TryGetNumberField(
                    TEXT("instancedStructVersion"), InstancedVersion)
                && InstancedVersion == 1.0
                && (*InstancedStructValue)->TryGetBoolField(
                    TEXT("isValid"), bInstancedValid)
                && bInstancedValid
                && (*InstancedStructValue)->TryGetStringField(
                    TEXT("valueStruct"), InstancedStructType)
                && InstancedStructType.EndsWith(TEXT(".UERingSemanticInstancedStructPayload"))
                && (*InstancedStructValue)->TryGetObjectField(
                    TEXT("fields"), InstancedStructFields)
                && InstancedStructFields != nullptr
                && (*InstancedStructFields)->TryGetNumberField(
                    TEXT("SemanticScale"), InstancedScale)
                && InstancedScale == 2.5
                && (*InstancedStructFields)->TryGetStringField(
                    TEXT("SemanticMode"), InstancedMode)
                && InstancedMode == TEXT("StateTreeCondition"));
        const FString CanonicalOwnedReference =
            TEXT("\"ownedObjectId\":\"InputModifierDeadZone_0\"");
        const int32 FirstCanonicalReference = DomainJson.Find(CanonicalOwnedReference);
        TestTrue(
            TEXT("Domain projection reuses the canonical owned object reference"),
            FirstCanonicalReference != INDEX_NONE
                && DomainJson.Find(
                    CanonicalOwnedReference,
                    ESearchCase::CaseSensitive,
                    ESearchDir::FromStart,
                    FirstCanonicalReference + CanonicalOwnedReference.Len()) != INDEX_NONE);
    }

    const FString DataTablePackageName = TEXT("/Game/UERingTests/DT_") + Suffix;
    UPackage* DataTablePackage = CreatePackage(*DataTablePackageName);
    UDataTable* DataTable = NewObject<UDataTable>(
        DataTablePackage, *FPackageName::GetLongPackageAssetName(DataTablePackageName), RF_Public | RF_Standalone);
    DataTable->RowStruct = FRichImageRow::StaticStruct();
    FRichImageRow ImageRow;
    ImageRow.Brush.ImageSize = FVector2D(64.0, 32.0);
    ImageRow.Brush.DrawAs = ESlateBrushDrawType::Image;
    DataTable->AddRow(TEXT("Example"), ImageRow);
    if (TestTrue(TEXT("DataTable saves"), SaveAsset(DataTablePackage, DataTable, false, Filename)))
    {
        SourceFiles.Add(Filename);
        FString Semantic;
        ExportAndCheck(*this, DataTable, TEXT("DataTable"), Semantic);
        SemanticFiles.Add(Semantic);
        FString DataTableJson;
        TestTrue(TEXT("DataTable semantic JSON reads"), FFileHelper::LoadFileToString(DataTableJson, *Semantic));
        TestFalse(TEXT("Public DataTable values are not prefixed as redacted"), DataTableJson.Contains(TEXT("[REDACTED]")));
        TestTrue(TEXT("DataTable rows include typed value details"), DataTableJson.Contains(TEXT("\"valueDetails\"")));
        TestTrue(
            TEXT("DataTable exposes authored and display field names"),
            DataTableJson.Contains(TEXT("\"fieldDefinitions\""))
                && DataTableJson.Contains(TEXT("\"authoredName\""))
                && DataTableJson.Contains(TEXT("\"displayName\"")));
    }

    const FString CurveTablePackageName = TEXT("/Game/UERingTests/CT_") + Suffix;
    UPackage* CurveTablePackage = CreatePackage(*CurveTablePackageName);
    UCurveTable* CurveTable = NewObject<UCurveTable>(
        CurveTablePackage, *FPackageName::GetLongPackageAssetName(CurveTablePackageName), RF_Public | RF_Standalone);
    CurveTable->AddSimpleCurve(TEXT("Speed")).AddKey(0.0f, 1.0f);
    if (TestTrue(TEXT("CurveTable saves"), SaveAsset(CurveTablePackage, CurveTable, false, Filename)))
    {
        SourceFiles.Add(Filename);
        FString Semantic;
        ExportAndCheck(*this, CurveTable, TEXT("CurveTable"), Semantic);
        SemanticFiles.Add(Semantic);
    }

    const FString WidgetPackageName = TEXT("/Game/UERingTests/WBP_") + Suffix;
    UPackage* WidgetPackage = CreatePackage(*WidgetPackageName);
    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
        UUERingNativeBindWidgetTestBase::StaticClass(),
        WidgetPackage,
        *FPackageName::GetLongPackageAssetName(WidgetPackageName),
        BPTYPE_Normal,
        UWidgetBlueprint::StaticClass(),
        UWidgetBlueprintGeneratedClass::StaticClass(),
        TEXT("UERingP0AssetTypesTest")));
    if (TestNotNull(TEXT("Widget Blueprint creates"), WidgetBlueprint))
    {
        UCanvasPanel* RootCanvas = WidgetBlueprint->WidgetTree->ConstructWidget<UCanvasPanel>(
            UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetBlueprint->WidgetTree->RootWidget = RootCanvas;
        UTextBlock* Label = WidgetBlueprint->WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), TEXT("SemanticLabel"));
        Label->bIsVariable = true;
        FWidgetTransform RenderTransform;
        RenderTransform.Translation = FVector2D(-114.0, -400.0);
        RenderTransform.Scale = FVector2D(1.5, 1.5);
        Label->SetRenderTransform(RenderTransform);
        UCanvasPanelSlot* LabelSlot = RootCanvas->AddChildToCanvas(Label);
        LabelSlot->SetPosition(FVector2D(32.0, 48.0));
        LabelSlot->SetSize(FVector2D(240.0, 80.0));
        UWidgetAnimation* WidgetAnimation = NewObject<UWidgetAnimation>(
            WidgetBlueprint,
            TEXT("SemanticHover"),
            RF_Transactional);
        WidgetAnimation->MovieScene = NewObject<UMovieScene>(
            WidgetAnimation,
            TEXT("MovieScene"),
            RF_Transactional);
        WidgetAnimation->MovieScene->SetPlaybackRange(0, 60);
        UMovieSceneFloatTrack* WidgetFloatTrack =
            WidgetAnimation->MovieScene->AddTrack<UMovieSceneFloatTrack>();
        UMovieSceneFloatSection* WidgetFloatSection =
            Cast<UMovieSceneFloatSection>(WidgetFloatTrack->CreateNewSection());
        WidgetFloatSection->SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(60)));
        WidgetFloatSection->GetChannel().AddLinearKey(FFrameNumber(0), 0.0f);
        WidgetFloatSection->GetChannel().AddLinearKey(FFrameNumber(60), 1.0f);
        WidgetFloatTrack->AddSection(*WidgetFloatSection);
        WidgetBlueprint->Animations.Add(WidgetAnimation);
        if (TestTrue(TEXT("Widget Blueprint saves"), SaveAsset(WidgetPackage, WidgetBlueprint, false, Filename)))
        {
            SourceFiles.Add(Filename);
            FString Semantic;
            ExportAndCheck(*this, WidgetBlueprint, TEXT("WidgetBlueprint"), Semantic);
            SemanticFiles.Add(Semantic);
            FString WidgetJson;
            TestTrue(TEXT("Widget semantic JSON reads"), FFileHelper::LoadFileToString(WidgetJson, *Semantic));
            TestTrue(TEXT("Widget variable state is exported"), WidgetJson.Contains(TEXT("\"isVariable\":true")));
            TestTrue(TEXT("Widget render transform is exported"), WidgetJson.Contains(TEXT("\"renderTransform\"")));
            TestTrue(TEXT("Widget slot properties are exported"), WidgetJson.Contains(TEXT("\"slotProperties\"")));
            TestTrue(
                TEXT("Widget animations preserve their owned MovieScene object graph"),
                WidgetJson.Contains(TEXT("\"animationCount\":1"))
                    && WidgetJson.Contains(TEXT("\"name\":\"SemanticHover\""))
                    && WidgetJson.Contains(TEXT("\"class\":\"/Script/UMG.WidgetAnimation\""))
                    && WidgetJson.Contains(TEXT("\"ownedObjectId\":\"MovieScene\""))
                    && WidgetJson.Contains(TEXT("MovieSceneFloatSection")));
            TestTrue(
                TEXT("Native BindWidget declarations are exported as inherited widgets"),
                WidgetJson.Contains(TEXT("\"name\":\"SemanticLabel\""))
                    && WidgetJson.Contains(TEXT("\"declarationOnly\":true"))
                    && WidgetJson.Contains(TEXT("\"bindingMetadata\":\"BindWidgetOptional\""))
                    && WidgetJson.Contains(TEXT("\"optionalReason\":\"BindWidgetOptional\""))
                    && WidgetJson.Contains(TEXT("\"required\":false"))
                    && WidgetJson.Contains(UUERingNativeBindWidgetTestBase::StaticClass()->GetPathName()));
        }
    }

    const FString ReflectionPackageName = TEXT("/Game/UERingTests/Curve_") + Suffix;
    UPackage* ReflectionPackage = CreatePackage(*ReflectionPackageName);
    UCurveFloat* ReflectionCurve = NewObject<UCurveFloat>(
        ReflectionPackage,
        *FPackageName::GetLongPackageAssetName(ReflectionPackageName),
        RF_Public | RF_Standalone);
    ReflectionCurve->FloatCurve.AddKey(0.0f, 2.5f);
    if (TestTrue(TEXT("Reflection fallback asset saves"), SaveAsset(ReflectionPackage, ReflectionCurve, false, Filename)))
    {
        SourceFiles.Add(Filename);
        FString Semantic;
        ExportAndCheck(*this, ReflectionCurve, TEXT("ReflectionFallback"), Semantic);
        SemanticFiles.Add(Semantic);
        FString ReflectionJson;
        TestTrue(TEXT("Reflection fallback semantic JSON reads"), FFileHelper::LoadFileToString(ReflectionJson, *Semantic));
        TestTrue(TEXT("Reflection fallback declares its limitation"), ReflectionJson.Contains(TEXT("fallbackReflectionOnly")));
    }

    FUERingReflectionExporter ReflectionExporter;
    FUERingExportContext UnloadedContext;
    FUERingSemanticPayload UnloadedPayload;
    FString UnloadedError;
    TestTrue(
        TEXT("Reflection fallback tolerates an unloaded object"),
        ReflectionExporter.BuildPayload(UnloadedContext, UnloadedPayload, UnloadedError));
    TestFalse(TEXT("Unloaded reflection fallback is marked as not loaded"),
        UnloadedPayload.Semantics->GetBoolField(TEXT("assetLoaded")));
    TestEqual(TEXT("Unloaded reflection fallback records both diagnostics"), UnloadedPayload.Diagnostics.Num(), 2);

    UPaperTileSet* TileSet = NewObject<UPaperTileSet>();
    UTexture2D* TileSheet = UTexture2D::CreateTransient(256, 256);
    TileSheet->Source.Init(256, 256, 1, 1, TSF_G8);
    TileSet->SetTileSize(FIntPoint(1, 1));
    TileSet->SetTileSheetTexture(TileSheet);
    TestEqual(TEXT("PaperTileSet test fixture has 65,536 logical tiles"), TileSet->GetTileCount(), 65536);
    FArrayProperty* PerTileDataProperty = FindFProperty<FArrayProperty>(
        UPaperTileSet::StaticClass(),
        UPaperTileSet::GetPerTilePropertyName());
    if (TestNotNull(TEXT("PaperTileSet per-tile metadata property resolves"), PerTileDataProperty))
    {
        FScriptArrayHelper PerTileData(
            PerTileDataProperty,
            PerTileDataProperty->ContainerPtrToValuePtr<void>(TileSet));
        PerTileData.Resize(TileSet->GetTileCount());
        FPaperTileMetadata* OverrideMetadata = TileSet->GetMutableTileMetadata(123);
        if (!TestNotNull(TEXT("PaperTileSet sparse override metadata resolves"), OverrideMetadata))
        {
            return false;
        }
        OverrideMetadata->UserDataName = TEXT("SemanticOverride");

        FUERingExportContext TileSetContext;
        TileSetContext.Asset = TileSet;
        FUERingSemanticPayload TileSetPayload;
        FString TileSetError;
        FUERingPaperTileSetExporter TileSetExporter;
        TestTrue(
            TEXT("PaperTileSet specialized export succeeds"),
            TileSetExporter.BuildPayload(TileSetContext, TileSetPayload, TileSetError));
        TestEqual(
            TEXT("PaperTileSet uses sparse semantic encoding"),
            TileSetPayload.Semantics->GetStringField(TEXT("metadataEncoding")),
            FString(TEXT("sparse-default-v1")));
        TestEqual(
            TEXT("PaperTileSet preserves logical tile count"),
            static_cast<int32>(TileSetPayload.Semantics->GetNumberField(TEXT("tileCount"))),
            65536);
        TestEqual(
            TEXT("PaperTileSet emits only meaningful metadata overrides"),
            static_cast<int32>(TileSetPayload.Semantics->GetNumberField(TEXT("overrideCount"))),
            1);
        FString TileSetJson;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> TileSetWriter =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&TileSetJson);
        TestTrue(
            TEXT("PaperTileSet sparse semantics serialize"),
            FJsonSerializer::Serialize(TileSetPayload.Semantics, TileSetWriter));
        TestTrue(
            TEXT("PaperTileSet sparse semantics stay below 64 KiB"),
            TileSetJson.Len() < 64 * 1024);
    }

    UPaperTileMap* TileMap = NewObject<UPaperTileMap>();
    TileMap->InitializeNewEmptyTileMap(TileSet);
    TileMap->ResizeMap(8, 4);
    UPaperTileLayer* TileLayer = TileMap->TileLayers[0];
    FPaperTileInfo BaseCell;
    BaseCell.TileSet = TileSet;
    BaseCell.PackedTileIndex = 7;
    TileLayer->SetCell(1, 2, BaseCell);
    TileLayer->SetCell(2, 2, BaseCell);
    TileLayer->SetCell(3, 2, BaseCell);
    FPaperTileInfo FlippedCell = BaseCell;
    FlippedCell.SetFlagValue(EPaperTileFlags::FlipHorizontal, true);
    TileLayer->SetCell(6, 2, FlippedCell);
    FUERingExportContext TileMapContext;
    TileMapContext.Asset = TileMap;
    FUERingSemanticPayload TileMapPayload;
    FString TileMapError;
    FUERingPaperTileMapExporter TileMapExporter;
    TestTrue(
        TEXT("PaperTileMap specialized export succeeds"),
        TileMapExporter.BuildPayload(TileMapContext, TileMapPayload, TileMapError));
    TestEqual(
        TEXT("PaperTileMap uses sparse row segments"),
        TileMapPayload.Semantics->GetStringField(TEXT("cellEncoding")),
        FString(TEXT("sparse-row-segments-v1")));
    TestEqual(
        TEXT("PaperTileMap preserves occupied cell count"),
        static_cast<int32>(TileMapPayload.Semantics->GetNumberField(TEXT("occupiedCellCount"))),
        4);
    TestEqual(
        TEXT("PaperTileMap emits two non-empty row segments"),
        static_cast<int32>(TileMapPayload.Semantics->GetNumberField(TEXT("segmentCount"))),
        2);
    TestEqual(
        TEXT("PaperTileMap indexes its one referenced tile set"),
        TileMapPayload.Semantics->GetArrayField(TEXT("tileSets")).Num(),
        1);

    const FString LevelSequencePackageName = TEXT("/Game/UERingTests/LS_") + Suffix;
    UPackage* LevelSequencePackage = CreatePackage(*LevelSequencePackageName);
    ULevelSequence* LevelSequence = NewObject<ULevelSequence>(
        LevelSequencePackage,
        *FPackageName::GetLongPackageAssetName(LevelSequencePackageName),
        RF_Public | RF_Standalone);
    LevelSequence->Initialize();
    UMovieScene* MovieScene = LevelSequence->GetMovieScene();
    MovieScene->SetPlaybackRange(0, 120);
    MovieScene->AddPossessable(TEXT("SemanticActor"), AActor::StaticClass());
    UMovieSceneFloatTrack* FloatTrack = MovieScene->AddTrack<UMovieSceneFloatTrack>();
    UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(FloatTrack->CreateNewSection());
    FloatSection->SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(120)));
    FloatSection->GetChannel().AddLinearKey(FFrameNumber(0), 1.25f);
    FloatSection->GetChannel().AddLinearKey(FFrameNumber(60), 9.5f);
    FloatTrack->AddSection(*FloatSection);
    FUERingExportContext LevelSequenceContext;
    LevelSequenceContext.Asset = LevelSequence;
    FUERingSemanticPayload LevelSequencePayload;
    FString LevelSequenceError;
    FUERingLevelSequenceExporter LevelSequenceExporter;
    TestTrue(
        TEXT("LevelSequence specialized export succeeds"),
        LevelSequenceExporter.BuildPayload(LevelSequenceContext, LevelSequencePayload, LevelSequenceError));
    TestEqual(
        TEXT("LevelSequence preserves its binding"),
        static_cast<int32>(LevelSequencePayload.Semantics->GetNumberField(TEXT("bindingCount"))),
        1);
    TestEqual(
        TEXT("LevelSequence preserves its global track"),
        static_cast<int32>(LevelSequencePayload.Semantics->GetNumberField(TEXT("globalTrackCount"))),
        1);
    FString LevelSequenceJson;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> LevelSequenceWriter =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&LevelSequenceJson);
    TestTrue(
        TEXT("LevelSequence semantics serialize"),
        FJsonSerializer::Serialize(LevelSequencePayload.Semantics, LevelSequenceWriter));
    TestTrue(
        TEXT("LevelSequence exports two key times and typed channel values"),
        LevelSequenceJson.Contains(TEXT("\"keyCount\":2"))
            && LevelSequenceJson.Contains(TEXT("\"keyTimes\":[0,60]"))
            && LevelSequenceJson.Contains(TEXT("FloatCurve")));

    UMaterial* Material = NewObject<UMaterial>();
    UMaterialExpressionConstant* ConstantExpression = NewObject<UMaterialExpressionConstant>(Material);
    ConstantExpression->R = 0.75f;
    UMaterialExpressionAdd* AddExpression = NewObject<UMaterialExpressionAdd>(Material);
    const FGuid DuplicateExpressionGuid = FGuid::NewGuid();
    ConstantExpression->GetMaterialExpressionId() = DuplicateExpressionGuid;
    AddExpression->GetMaterialExpressionId() = DuplicateExpressionGuid;
    AddExpression->A.Expression = ConstantExpression;
    AddExpression->ConstB = 0.375f;
    Material->GetExpressionInputForProperty(MP_BaseColor)->Expression = AddExpression;
    Material->GetExpressionCollection().AddExpression(ConstantExpression);
    Material->GetExpressionCollection().AddExpression(AddExpression);
    UMaterialExpressionConstant* OrphanExpression = NewObject<UMaterialExpressionConstant>(
        Material,
        TEXT("UnreachableOrphanExpression"));
    OrphanExpression->R = 0.25f;
    FUERingExportContext MaterialContext;
    MaterialContext.Asset = Material;
    FUERingSemanticPayload MaterialPayload;
    FString MaterialError;
    FUERingMaterialExporter MaterialExporter;
    TestTrue(TEXT("Material layer functions use the compact logic exporter"),
        MaterialExporter.CanExport(FAssetData(NewObject<UMaterialFunctionMaterialLayer>())));
    TestTrue(TEXT("Material layer function instances use the compact logic exporter"),
        MaterialExporter.CanExport(FAssetData(NewObject<UMaterialFunctionMaterialLayerInstance>())));
    TestTrue(TEXT("Material layer blends use the compact logic exporter"),
        MaterialExporter.CanExport(FAssetData(NewObject<UMaterialFunctionMaterialLayerBlend>())));
    TestTrue(TEXT("Material layer blend instances use the compact logic exporter"),
        MaterialExporter.CanExport(FAssetData(NewObject<UMaterialFunctionMaterialLayerBlendInstance>())));
    TestTrue(
        TEXT("Material logic graph export succeeds"),
        MaterialExporter.BuildPayload(MaterialContext, MaterialPayload, MaterialError));
    TestEqual(
        TEXT("Material uses its dedicated semantic kind"),
        MaterialPayload.Semantics->GetStringField(TEXT("kind")),
        FString(TEXT("MaterialLogic")));
    TestEqual(
        TEXT("Material uses a typed expression graph"),
        MaterialPayload.Semantics->GetStringField(TEXT("representation")),
        FString(TEXT("material-expression-graph-v1")));
    TestEqual(
        TEXT("Material graph includes its two authored expressions"),
        MaterialPayload.Semantics->GetNumberField(TEXT("nodeCount")),
        2.0);
    const TArray<TSharedPtr<FJsonValue>>* MaterialObjects = nullptr;
    TestTrue(TEXT("Material graph exposes expression nodes"),
        MaterialPayload.Semantics->TryGetArrayField(TEXT("nodes"), MaterialObjects));
    bool bFoundConstantExpression = false;
    bool bFoundAddExpression = false;
    bool bFoundOrphanExpression = false;
    bool bRootDuplicatesProperties = false;
    bool bFoundConstBDefault = false;
    TSet<FString> MaterialNodeIds;
    if (MaterialObjects != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *MaterialObjects)
        {
            const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
            FString Class;
            if (Object.IsValid() && Object->TryGetStringField(TEXT("class"), Class))
            {
                MaterialNodeIds.Add(Object->GetStringField(TEXT("id")));
                bFoundConstantExpression |= Class.EndsWith(TEXT("MaterialExpressionConstant"));
                bFoundAddExpression |= Class.EndsWith(TEXT("MaterialExpressionAdd"));
                bFoundOrphanExpression |= Object->GetStringField(TEXT("name")) == TEXT("UnreachableOrphanExpression");
                bRootDuplicatesProperties |= Object->GetStringField(TEXT("id")) == TEXT("$root");
                const TSharedPtr<FJsonObject>* Configuration = nullptr;
                double ConstB = 0.0;
                bFoundConstBDefault |= Class.EndsWith(TEXT("MaterialExpressionAdd"))
                    && Object->TryGetObjectField(TEXT("configuration"), Configuration)
                    && Configuration != nullptr
                    && (*Configuration)->TryGetNumberField(TEXT("ConstB"), ConstB)
                    && FMath::IsNearlyEqual(ConstB, 0.375);
            }
        }
    }
    TestTrue(TEXT("Material graph contains the constant expression"), bFoundConstantExpression);
    TestTrue(TEXT("Material graph contains the add expression"), bFoundAddExpression);
    TestFalse(TEXT("Unreachable package objects are excluded"), bFoundOrphanExpression);
    TestFalse(TEXT("Material root is not duplicated in the node list"), bRootDuplicatesProperties);
    TestEqual(TEXT("Duplicate expression GUIDs receive unique local node IDs"),
        MaterialNodeIds.Num(), 2);
    const TArray<TSharedPtr<FJsonValue>>* MaterialConnections = nullptr;
    TestTrue(TEXT("Material graph exposes typed connections"),
        MaterialPayload.Semantics->TryGetArrayField(TEXT("connections"), MaterialConnections));
    bool bFoundExpressionInput = false;
    bool bFoundMaterialInput = false;
    if (MaterialConnections != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *MaterialConnections)
        {
            const TSharedPtr<FJsonObject> Connection = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Connection.IsValid()) continue;
            const FString TargetNode = Connection->GetStringField(TEXT("targetNode"));
            const FString TargetInput = Connection->GetStringField(TEXT("targetInput"));
            bFoundExpressionInput |= TargetInput == TEXT("A");
            bFoundMaterialInput |= TargetNode == TEXT("$material") && TargetInput == TEXT("MP_BaseColor");
        }
    }
    TestTrue(TEXT("Expression connection preserves the target input"), bFoundExpressionInput);
    TestTrue(TEXT("Root connection preserves the material input"), bFoundMaterialInput);
    TestEqual(
        TEXT("Material graph contains no dangling connection"),
        MaterialPayload.Semantics->GetNumberField(TEXT("danglingConnectionCount")),
        0.0);
    FString MaterialJson;
    const TSharedRef<TJsonWriter<>> MaterialWriter = TJsonWriterFactory<>::Create(&MaterialJson);
    TestTrue(TEXT("Material logic semantics serialize"),
        FJsonSerializer::Serialize(MaterialPayload.Semantics, MaterialWriter));
    TestFalse(TEXT("Material logic omits editor positions"),
        MaterialJson.Contains(TEXT("MaterialExpressionEditorX")));
    TestFalse(TEXT("Material logic omits localized menu categories"),
        MaterialJson.Contains(TEXT("MenuCategories")));
    TestTrue(TEXT("Unconnected material input defaults remain reconstructable"),
        bFoundConstBDefault);

    UMaterialFunction* Function = NewObject<UMaterialFunction>();
    UMaterialExpressionFunctionInput* FunctionInput =
        NewObject<UMaterialExpressionFunctionInput>(Function);
    FunctionInput->InputName = TEXT("Scale");
    FunctionInput->PreviewValue = FVector4f(2.5f, 0.0f, 0.0f, 0.0f);
    FunctionInput->bUsePreviewValueAsDefault = true;
    Function->GetExpressionCollection().AddExpression(FunctionInput);
    FUERingExportContext FunctionContext;
    FunctionContext.Asset = Function;
    FUERingSemanticPayload FunctionPayload;
    FString FunctionError;
    TestTrue(TEXT("Material function logic export succeeds"),
        MaterialExporter.BuildPayload(FunctionContext, FunctionPayload, FunctionError));
    FString FunctionJson;
    const TSharedRef<TJsonWriter<>> FunctionWriter = TJsonWriterFactory<>::Create(&FunctionJson);
    TestTrue(TEXT("Material function semantics serialize"),
        FJsonSerializer::Serialize(FunctionPayload.Semantics, FunctionWriter));
    TestTrue(TEXT("Function input preview defaults remain reconstructable"),
        FunctionJson.Contains(TEXT("PreviewValue"))
            && FunctionJson.Contains(TEXT("bUsePreviewValueAsDefault")));

    UMaterialFunctionInstance* FunctionInstance = NewObject<UMaterialFunctionInstance>();
    FunctionInstance->SetParent(Function);
    FScalarParameterValue FunctionScalarOverride;
    FunctionScalarOverride.ParameterInfo.Name = TEXT("Scale");
    FunctionScalarOverride.ParameterValue = 3.5f;
    FunctionInstance->ScalarParameterValues.Add(FunctionScalarOverride);
    FUERingExportContext FunctionInstanceContext;
    FunctionInstanceContext.Asset = FunctionInstance;
    FUERingSemanticPayload FunctionInstancePayload;
    FString FunctionInstanceError;
    TestTrue(TEXT("Material function instance export succeeds"),
        MaterialExporter.BuildPayload(
            FunctionInstanceContext,
            FunctionInstancePayload,
            FunctionInstanceError));
    TestEqual(
        TEXT("Function instance has a non-owning override representation"),
        FunctionInstancePayload.Semantics->GetStringField(TEXT("representation")),
        FString(TEXT("material-function-instance-v1")));
    TestFalse(TEXT("Function instance does not claim ownership of its parent graph"),
        FunctionInstancePayload.Semantics->HasField(TEXT("nodes")));
    const TSharedPtr<FJsonObject>* FunctionInstanceParent = nullptr;
    TestTrue(TEXT("Function instance preserves its parent reference"),
        FunctionInstancePayload.Semantics->TryGetObjectField(
            TEXT("parent"),
            FunctionInstanceParent)
            && FunctionInstanceParent != nullptr
            && (*FunctionInstanceParent)->GetStringField(TEXT("objectPath")) == Function->GetPathName());
    const TSharedPtr<FJsonObject>* FunctionInstanceOverrides = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* FunctionScalarOverrides = nullptr;
    TestTrue(TEXT("Function instance preserves authored scalar overrides"),
        FunctionInstancePayload.Semantics->TryGetObjectField(
            TEXT("overrides"),
            FunctionInstanceOverrides)
            && FunctionInstanceOverrides != nullptr
            && (*FunctionInstanceOverrides)->TryGetArrayField(
                TEXT("ScalarParameterValues"),
                FunctionScalarOverrides)
            && FunctionScalarOverrides != nullptr
            && FunctionScalarOverrides->Num() == 1);

    UMaterialParameterCollection* ParameterCollection = NewObject<UMaterialParameterCollection>();
    FCollectionScalarParameter CollectionParameter;
    CollectionParameter.ParameterName = TEXT("HighGuidParameter");
    CollectionParameter.Id = FGuid(MAX_uint32, 0x80000000u, 1u, 2u);
    CollectionParameter.DefaultValue = 0.5f;
    ParameterCollection->ScalarParameters.Add(CollectionParameter);
    FUERingExportContext CollectionContext;
    CollectionContext.Asset = ParameterCollection;
    FUERingSemanticPayload CollectionPayload;
    FString CollectionError;
    TestTrue(TEXT("Material parameter collection export succeeds"),
        MaterialExporter.BuildPayload(CollectionContext, CollectionPayload, CollectionError));
    TestEqual(
        TEXT("Material parameter collection has a distinct representation"),
        CollectionPayload.Semantics->GetStringField(TEXT("representation")),
        FString(TEXT("material-parameter-collection-v1")));
    FString CollectionJson;
    const TSharedRef<TJsonWriter<>> CollectionWriter = TJsonWriterFactory<>::Create(&CollectionJson);
    TestTrue(TEXT("Material collection semantics serialize"),
        FJsonSerializer::Serialize(CollectionPayload.Semantics, CollectionWriter));
    TestTrue(TEXT("Material parameter GUID preserves all unsigned 32-bit components"),
        CollectionJson.Contains(CollectionParameter.Id.ToString(EGuidFormats::DigitsWithHyphensLower)));

    UMaterialInstanceConstant* MaterialInstance = NewObject<UMaterialInstanceConstant>();
    MaterialInstance->PhysicalMaterialMap[1] = NewObject<UPhysicalMaterial>(MaterialInstance);
    FStaticComponentMaskParameter StaticMask;
    StaticMask.ParameterInfo.Name = TEXT("ChannelMask");
    StaticMask.bOverride = true;
    StaticMask.R = true;
    StaticMask.B = true;
    const FStaticTerrainLayerWeightParameter StaticTerrain(TEXT("Mud"), 3);
    UMaterialInstanceEditorOnlyData* InstanceEditorData = MaterialInstance->GetEditorOnlyData();
    if (TestNotNull(TEXT("Material instance editor-only static parameter data exists"), InstanceEditorData))
    {
        InstanceEditorData->StaticParameters.StaticComponentMaskParameters.Add(StaticMask);
        InstanceEditorData->StaticParameters.TerrainLayerWeightParameters.Add(StaticTerrain);
    }
    FUERingExportContext InstanceContext;
    InstanceContext.Asset = MaterialInstance;
    FUERingSemanticPayload InstancePayload;
    FString InstanceError;
    TestTrue(TEXT("Material instance export succeeds"),
        MaterialExporter.BuildPayload(InstanceContext, InstancePayload, InstanceError));
    const TSharedPtr<FJsonObject>* InstanceOverrides = nullptr;
    if (TestTrue(TEXT("Material instance exposes local overrides"),
            InstancePayload.Semantics->TryGetObjectField(TEXT("overrides"), InstanceOverrides)))
    {
        const TArray<TSharedPtr<FJsonValue>>* PhysicalMaterials = nullptr;
        TestTrue(TEXT("Fixed physical-material array is preserved"),
            (*InstanceOverrides)->TryGetArrayField(TEXT("PhysicalMaterialMap"), PhysicalMaterials));
        if (PhysicalMaterials != nullptr)
        {
            TestEqual(TEXT("Every physical-material mask slot is present"),
                PhysicalMaterials->Num(), static_cast<int32>(EPhysicalMaterialMaskColor::MAX));
        }
        FString InstanceJson;
        const TSharedRef<TJsonWriter<>> InstanceWriter = TJsonWriterFactory<>::Create(&InstanceJson);
        TestTrue(TEXT("Material instance semantics serialize"),
            FJsonSerializer::Serialize(InstancePayload.Semantics, InstanceWriter));
        TestTrue(TEXT("Editor-only static component masks remain in logic semantics"),
            InstanceJson.Contains(TEXT("StaticComponentMaskParameters"))
                && InstanceJson.Contains(TEXT("ChannelMask")));
        TestTrue(TEXT("Editor-only terrain layer weights remain in logic semantics"),
            InstanceJson.Contains(TEXT("TerrainLayerWeightParameters"))
                && InstanceJson.Contains(TEXT("Mud"))
                && InstanceJson.Contains(TEXT("WeightmapIndex")));
    }
    const FString DerivedSemanticFile = FPaths::Combine(
        FUERingExportManager::Get().GetOutputRoot(),
        TEXT("content/Game/UERingTests/Derived.uesem.json"));
    FString MermaidFile;
    FString GraphvizFile;
    FString DerivedError;
    TestTrue(
        TEXT("Owned object graph visualizations write"),
        FUERingDerivedArtifactWriter::WriteGraphArtifacts(
            DerivedSemanticFile,
            MaterialPayload.Semantics,
            MermaidFile,
            GraphvizFile,
            DerivedError));
    FString MermaidText;
    FString GraphvizText;
    TestTrue(TEXT("Mermaid graph reads"), FFileHelper::LoadFileToString(MermaidText, *MermaidFile));
    TestTrue(TEXT("Graphviz graph reads"), FFileHelper::LoadFileToString(GraphvizText, *GraphvizFile));
    TestTrue(
        TEXT("Both graph formats contain nodes and an edge"),
        MermaidText.Contains(TEXT("flowchart TD"))
            && MermaidText.Contains(TEXT(" --> "))
            && GraphvizText.Contains(TEXT("digraph UERing"))
            && GraphvizText.Contains(TEXT(" -> ")));
    TestTrue(TEXT("Material visualization includes the final output node"),
        MermaidText.Contains(TEXT("Material Output"))
            && GraphvizText.Contains(TEXT("Material Output")));
    FString MaterialOutputNode;
    TArray<FString> MermaidLines;
    MermaidText.ParseIntoArrayLines(MermaidLines, false);
    for (const FString& Line : MermaidLines)
    {
        if (Line.Contains(TEXT("Material Output")))
        {
            int32 BracketIndex = INDEX_NONE;
            if (Line.FindChar(TEXT('['), BracketIndex))
            {
                MaterialOutputNode = Line.Left(BracketIndex).TrimStartAndEnd();
            }
            break;
        }
    }
    TestTrue(TEXT("Material visualization connects logic into the final output node"),
        !MaterialOutputNode.IsEmpty()
            && MermaidText.Contains(TEXT(" --> ") + MaterialOutputNode));

    const TSharedRef<FJsonObject> PreviousRoot = MakeShared<FJsonObject>();
    const TSharedRef<FJsonObject> PreviousAsset = MakeShared<FJsonObject>();
    PreviousAsset->SetStringField(TEXT("packageName"), TEXT("/Game/UERingTests/Derived"));
    PreviousAsset->SetStringField(TEXT("sourceHash"), TEXT("sha256:before"));
    PreviousRoot->SetObjectField(TEXT("asset"), PreviousAsset);
    const TSharedRef<FJsonObject> PreviousSemantics = MakeShared<FJsonObject>();
    PreviousSemantics->SetStringField(TEXT("kind"), TEXT("MaterialGraph"));
    PreviousSemantics->SetNumberField(TEXT("value"), 1);
    PreviousRoot->SetObjectField(TEXT("semantics"), PreviousSemantics);
    const TSharedRef<FJsonObject> CurrentRoot = MakeShared<FJsonObject>();
    const TSharedRef<FJsonObject> CurrentAsset = MakeShared<FJsonObject>();
    CurrentAsset->SetStringField(TEXT("packageName"), TEXT("/Game/UERingTests/Derived"));
    CurrentAsset->SetStringField(TEXT("sourceHash"), TEXT("sha256:after"));
    CurrentAsset->SetStringField(TEXT("exportedAtUtc"), TEXT("2026-07-31T00:00:00Z"));
    CurrentRoot->SetObjectField(TEXT("asset"), CurrentAsset);
    const TSharedRef<FJsonObject> CurrentSemantics = MakeShared<FJsonObject>();
    CurrentSemantics->SetStringField(TEXT("kind"), TEXT("MaterialGraph"));
    CurrentSemantics->SetNumberField(TEXT("value"), 2);
    CurrentRoot->SetObjectField(TEXT("semantics"), CurrentSemantics);
    FString DiffFile;
    TestTrue(
        TEXT("Semantic change summary writes"),
        FUERingDerivedArtifactWriter::WriteChangeSummary(
            DerivedSemanticFile,
            PreviousRoot,
            CurrentRoot,
            DiffFile,
            DerivedError));
    FString DiffText;
    TestTrue(TEXT("Semantic change summary reads"), FFileHelper::LoadFileToString(DiffText, *DiffFile));
    TestTrue(
        TEXT("Semantic change summary records hashes and changed path"),
        DiffText.Contains(TEXT("sha256:before"))
            && DiffText.Contains(TEXT("sha256:after"))
            && DiffText.Contains(TEXT("/semantics/value"))
            && DiffText.Contains(TEXT("\"changedCount\": 1")));
    PreviousAsset->SetStringField(TEXT("packageName"), TEXT("/Game/UERingTests/BeforeRename"));
    PreviousAsset->SetStringField(TEXT("sourceHash"), TEXT("sha256:same"));
    CurrentAsset->SetStringField(TEXT("packageName"), TEXT("/Game/UERingTests/AfterRename"));
    CurrentAsset->SetStringField(TEXT("sourceHash"), TEXT("sha256:same"));
    CurrentSemantics->SetNumberField(TEXT("value"), 1);
    DerivedError.Reset();
    TestTrue(
        TEXT("Identity-only semantic change summary writes"),
        FUERingDerivedArtifactWriter::WriteChangeSummary(
            DerivedSemanticFile,
            PreviousRoot,
            CurrentRoot,
            DiffFile,
            DerivedError));
    DiffText.Reset();
    TestTrue(TEXT("Identity-only change summary reads"), FFileHelper::LoadFileToString(DiffText, *DiffFile));
    TestTrue(
        TEXT("Rename is represented as an asset identity change"),
        DiffText.Contains(TEXT("/asset/packageName"))
            && DiffText.Contains(TEXT("/Game/UERingTests/BeforeRename"))
            && DiffText.Contains(TEXT("/Game/UERingTests/AfterRename"))
            && DiffText.Contains(TEXT("\"changedCount\": 1")));
    IFileManager::Get().Delete(*MermaidFile, false, true);
    IFileManager::Get().Delete(*GraphvizFile, false, true);
    IFileManager::Get().Delete(*DiffFile, false, true);

    UAnimBlueprint* AnimBlueprint = NewObject<UAnimBlueprint>();
    FUERingExportContext AnimBlueprintContext;
    AnimBlueprintContext.Asset = AnimBlueprint;
    FUERingSemanticPayload AnimBlueprintPayload;
    FString AnimBlueprintError;
    FUERingAnimBlueprintExporter AnimBlueprintExporter;
    TestTrue(
        TEXT("Anim Blueprint specialized export succeeds"),
        AnimBlueprintExporter.BuildPayload(AnimBlueprintContext, AnimBlueprintPayload, AnimBlueprintError));
    TestEqual(
        TEXT("Anim Blueprint uses its dedicated semantic kind"),
        AnimBlueprintPayload.Semantics->GetStringField(TEXT("kind")),
        FString(TEXT("AnimBlueprint")));
    TestTrue(
        TEXT("Anim Blueprint retains dedicated animation semantics"),
        AnimBlueprintPayload.Semantics->HasField(TEXT("animation")));
    TestFalse(TEXT("Empty Anim Blueprint graphs are omitted"),
        AnimBlueprintPayload.Semantics->HasField(TEXT("graphs")));
    TestFalse(TEXT("Empty Anim Blueprint variables are omitted"),
        AnimBlueprintPayload.Semantics->HasField(TEXT("variables")));

    const FString AnimationPackageName = TEXT("/Game/UERingTests/AS_") + Suffix;
    UPackage* AnimationPackage = CreatePackage(*AnimationPackageName);
    UAnimSequence* AnimationSequence = NewObject<UAnimSequence>(
        AnimationPackage,
        *FPackageName::GetLongPackageAssetName(AnimationPackageName),
        RF_Public | RF_Standalone);
    FAnimSyncMarker TestSyncMarker;
    TestSyncMarker.MarkerName = TEXT("FootDown");
    TestSyncMarker.Time = 0.25f;
#if WITH_EDITORONLY_DATA
    TestSyncMarker.TrackIndex = 2;
#endif
    AnimationSequence->AuthoredSyncMarkers.Add(TestSyncMarker);
    FUERingExportContext AnimationContext;
    AnimationContext.Asset = AnimationSequence;
    AnimationContext.Profile = EUERingExportProfile::Logic;
    FUERingSemanticPayload AnimationPayload;
    FString AnimationError;
    FUERingAnimationAssetExporter AnimationExporter;
    TestTrue(
        TEXT("Animation Logic export succeeds"),
        AnimationExporter.BuildPayload(AnimationContext, AnimationPayload, AnimationError));
    TestEqual(
        TEXT("Animation Logic uses its dedicated semantic kind"),
        AnimationPayload.Semantics->GetStringField(TEXT("kind")),
        FString(TEXT("AnimationLogic")));
    TestFalse(
        TEXT("Animation Logic omits unrestricted reflected properties"),
        AnimationPayload.Semantics->HasField(TEXT("properties")));
    const TSharedPtr<FJsonObject>* AnimationTimeline = nullptr;
    TestTrue(
        TEXT("Animation Logic includes an explicit timeline"),
        AnimationPayload.Semantics->TryGetObjectField(TEXT("timeline"), AnimationTimeline)
            && AnimationTimeline != nullptr);
    if (AnimationTimeline != nullptr)
    {
        const TArray<TSharedPtr<FJsonValue>>* SyncMarkers = nullptr;
        TestTrue(
            TEXT("Animation Logic preserves sync markers outside privacy filtering"),
            (*AnimationTimeline)->TryGetArrayField(TEXT("syncMarkers"), SyncMarkers)
                && SyncMarkers != nullptr
                && SyncMarkers->Num() == 1
                && (*SyncMarkers)[0]->AsObject()->GetStringField(TEXT("name")) == TEXT("FootDown"));
    }

    UAnimMontage* AnimationMontage = NewObject<UAnimMontage>();
    AnimationMontage->MarkerData.AuthoredSyncMarkers.Add(TestSyncMarker);
    FUERingExportContext MontageContext;
    MontageContext.Asset = AnimationMontage;
    MontageContext.Profile = EUERingExportProfile::Logic;
    FUERingSemanticPayload MontagePayload;
    FString MontageError;
    TestTrue(
        TEXT("Animation Montage export succeeds"),
        AnimationExporter.BuildPayload(MontageContext, MontagePayload, MontageError));
    const TSharedPtr<FJsonObject>* MontageTimeline = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* MontageSyncMarkers = nullptr;
    TestTrue(
        TEXT("Animation Montage preserves marker sync data outside privacy filtering"),
        MontagePayload.Semantics->TryGetObjectField(TEXT("timeline"), MontageTimeline)
            && MontageTimeline != nullptr
            && (*MontageTimeline)->TryGetArrayField(TEXT("syncMarkers"), MontageSyncMarkers)
            && MontageSyncMarkers != nullptr
            && MontageSyncMarkers->Num() == 1
            && (*MontageSyncMarkers)[0]->AsObject()->GetStringField(TEXT("name")) == TEXT("FootDown"));

    USoundWave* SoundWave = NewObject<USoundWave>();
    FUERingExportContext AudioContext;
    AudioContext.Asset = SoundWave;
    AudioContext.Profile = EUERingExportProfile::Logic;
    FUERingSemanticPayload AudioPayload;
    FString AudioError;
    FUERingAudioAssetExporter AudioExporter;
    TestTrue(
        TEXT("Audio Logic export succeeds"),
        AudioExporter.BuildPayload(AudioContext, AudioPayload, AudioError));
    TestEqual(
        TEXT("Audio Logic uses its dedicated semantic kind"),
        AudioPayload.Semantics->GetStringField(TEXT("kind")),
        FString(TEXT("AudioLogic")));
    TestTrue(TEXT("Audio Logic records bulk omission"), !AudioPayload.Omissions.IsEmpty());

    const FString ExternalPackageName = TEXT("/Game/__ExternalActors__/UERingTests/External_") + Suffix;
    UPackage* ExternalPackage = CreatePackage(*ExternalPackageName);
    UPrimaryAssetLabel* ExternalAsset = NewObject<UPrimaryAssetLabel>(
        ExternalPackage,
        *FPackageName::GetLongPackageAssetName(ExternalPackageName),
        RF_Public | RF_Standalone);
    TestFalse(
        TEXT("World Partition external objects are covered by their owning map"),
        FUERingExportManager::Get().CanExport(FAssetData(ExternalAsset)));

    const FString WorldPackageName = TEXT("/Game/UERingTests/L_") + Suffix;
    UPackage* WorldPackage = CreatePackage(*WorldPackageName);
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Editor,
        false,
        *FPackageName::GetLongPackageAssetName(WorldPackageName),
        WorldPackage,
        false);
    if (TestNotNull(TEXT("World creates"), World))
    {
        World->SetFlags(RF_Public | RF_Standalone);
        APointLight* Actor = World->SpawnActor<APointLight>(
            APointLight::StaticClass(),
            FTransform(FVector(100.0, 200.0, 300.0)));
        TestNotNull(TEXT("Map test actor spawns"), Actor);
        Actor->PointLightComponent->SetIntensity(4321.0f);
        APointLight* TransientActor = World->SpawnActor<APointLight>(
            APointLight::StaticClass(),
            FTransform(FVector(400.0, 500.0, 600.0)));
        TestNotNull(TEXT("Transient map test actor spawns"), TransientActor);
        TransientActor->Rename(TEXT("TransientRuntimeLight"));
        TransientActor->SetFlags(RF_Transient);
        if (TestTrue(TEXT("World saves"), SaveAsset(WorldPackage, World, true, Filename)))
        {
            SourceFiles.Add(Filename);
            Actor->GetRootComponent()->SetComponentToWorld(FTransform::Identity);
            TestTrue(
                TEXT("Map test simulates an inactive world's stale runtime transform cache"),
                Actor->GetActorTransform().Equals(FTransform::Identity));
            FString Semantic;
            ExportAndCheck(*this, World, TEXT("World"), Semantic);
            SemanticFiles.Add(Semantic);
            FString WorldJson;
            TestTrue(TEXT("World semantic JSON reads"), FFileHelper::LoadFileToString(WorldJson, *Semantic));
            TestTrue(TEXT("World component instance property overrides are exported"), WorldJson.Contains(TEXT("\"name\":\"Intensity\"")));
            TestTrue(TEXT("World component relative transform is exported"), WorldJson.Contains(TEXT("\"relativeTransform\"")));
            TestTrue(
                TEXT("World actor transform is rebuilt from persistent component data"),
                WorldJson.Contains(TEXT("\"transformSource\":\"persistentRootComponentHierarchy\""))
                    && WorldJson.Contains(TEXT("\"x\":100"))
                    && WorldJson.Contains(TEXT("\"y\":200"))
                    && WorldJson.Contains(TEXT("\"z\":300")));
            TestTrue(TEXT("World actor scope is explicit"), WorldJson.Contains(TEXT("\"actorScope\":\"persistentLevelSavedActors\"")));
            TestTrue(TEXT("World reports skipped transient actors"), WorldJson.Contains(TEXT("\"skippedTransientActors\"")));
            TestFalse(TEXT("Transient editor/runtime actors are excluded"), WorldJson.Contains(TEXT("TransientRuntimeLight")));
        }
    }

    for (const FString& Semantic : SemanticFiles)
    {
        DeleteExportArtifacts(Semantic);
    }
    for (const FString& Source : SourceFiles)
    {
        IFileManager::Get().Delete(*Source, false, true);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUERingP1ExtensibilityAndMcpTest,
    "UERing.Exporter.P1.ExtensibilityAndMCP",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUERingP1ExtensibilityAndMcpTest::RunTest(const FString& Parameters)
{
    using namespace UERingP0Tests;
    IModelContextProtocolModule* McpModule = IModelContextProtocolModule::Get();
    if (!TestNotNull(TEXT("Official ModelContextProtocol module is available"), McpModule))
    {
        return false;
    }
    const TArray<FString> ToolNames = {
        TEXT("ue_ring_get_semantic"),
        TEXT("ue_ring_export_asset"),
        TEXT("ue_ring_validate_semantics"),
        TEXT("ue_ring_query_graph")
    };
    for (const FString& ToolName : ToolNames)
    {
        const TSharedPtr<IModelContextProtocolTool> Tool = McpModule->FindTool(ToolName);
        TestTrue(*FString::Printf(TEXT("MCP tool %s is registered"), *ToolName), Tool.IsValid());
        if (Tool.IsValid())
        {
            const TSharedPtr<FJsonObject> Schema = Tool->GetInputJsonSchema();
            TestTrue(*FString::Printf(TEXT("MCP tool %s has an object input schema"), *ToolName),
                Schema.IsValid() && Schema->GetStringField(TEXT("type")) == TEXT("object"));
        }
    }

    const TSharedPtr<IModelContextProtocolTool> GraphTool =
        McpModule->FindTool(TEXT("ue_ring_query_graph"));
    if (GraphTool.IsValid())
    {
        const TSharedRef<FJsonObject> GraphParams = MakeShared<FJsonObject>();
        GraphParams->SetStringField(TEXT("node_id"), TEXT("asset:/Game/UERingTests/DoesNotExist"));
        GraphParams->SetStringField(TEXT("direction"), TEXT("both"));
        GraphParams->SetNumberField(TEXT("limit"), 10);
        const FModelContextProtocolToolResult GraphResult = GraphTool->Run(GraphParams);
        bool bIsError = false;
        GraphResult.JsonObject->TryGetBoolField(TEXT("isError"), bIsError);
        TestFalse(TEXT("MCP graph query executes against the project index"), bIsError);
        const TSharedPtr<FJsonObject>* Structured = nullptr;
        TestTrue(TEXT("MCP graph query returns structured content"),
            GraphResult.JsonObject->TryGetObjectField(TEXT("structuredContent"), Structured));
        if (Structured != nullptr)
        {
            TestFalse(TEXT("MCP graph query reports an unknown node"),
                (*Structured)->GetBoolField(TEXT("nodeExists")));
            TestEqual(TEXT("Unknown MCP graph node has no edges"),
                static_cast<int32>((*Structured)->GetNumberField(TEXT("count"))), 0);
        }
    }

    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString PackageName = TEXT("/Game/UERingTests/SDK_") + Suffix;
    UPackage* Package = CreatePackage(*PackageName);
    UPrimaryAssetLabel* Asset = NewObject<UPrimaryAssetLabel>(
        Package,
        *FPackageName::GetLongPackageAssetName(PackageName),
        RF_Public | RF_Standalone);
    FString SourceFile;
    if (!TestTrue(TEXT("SDK test asset saves"), SaveAsset(Package, Asset, false, SourceFile)))
    {
        return false;
    }

    TUniquePtr<IUERingAssetExporter> TestExporter = MakeUnique<FTestSdkExporter>();
    const FUERingExporterHandle Handle = FUERingExporterRegistry::Register(MoveTemp(TestExporter));
    TestTrue(TEXT("Custom exporter receives a valid registration handle"), Handle != 0);
    const FUERingExportResult CustomExport = FUERingExportManager::Get().ExportAsset(FAssetData(Asset));
    TestTrue(TEXT("Custom SDK exporter runs"), CustomExport.IsSuccess());
    TestEqual(TEXT("Custom SDK exporter wins by priority"),
        CustomExport.ExporterName, FString(TEXT("TestSdkExporter")));
    TSharedPtr<FJsonObject> CustomRoot;
    TestTrue(TEXT("Custom SDK semantic parses"), LoadJsonObject(CustomExport.OutputFile, CustomRoot));
    if (CustomRoot.IsValid())
    {
        const TSharedPtr<FJsonObject>* Semantics = nullptr;
        TestTrue(TEXT("Custom SDK semantic has a payload"),
            CustomRoot->TryGetObjectField(TEXT("semantics"), Semantics));
        if (Semantics != nullptr)
        {
            TestEqual(TEXT("Custom SDK semantic kind is preserved"),
                (*Semantics)->GetStringField(TEXT("kind")), FString(TEXT("CustomSdkAsset")));
        }
        const TSharedPtr<FJsonObject>* Dependencies = nullptr;
        TestTrue(TEXT("Custom SDK semantic has merged dependencies"),
            CustomRoot->TryGetObjectField(TEXT("dependencies"), Dependencies));
        if (Dependencies != nullptr)
        {
            const TArray<TSharedPtr<FJsonValue>>& Hard = (*Dependencies)->GetArrayField(TEXT("hard"));
            const TArray<TSharedPtr<FJsonValue>>& Soft = (*Dependencies)->GetArrayField(TEXT("soft"));
            TestTrue(TEXT("Exporter-provided hard dependency is preserved"), Hard.ContainsByPredicate(
                [](const TSharedPtr<FJsonValue>& Value)
                {
                    return Value.IsValid() && Value->AsString() == TEXT("/Game/UERingTests/AdditionalHard");
                }));
            TestTrue(TEXT("Exporter-provided soft dependency is preserved"), Soft.ContainsByPredicate(
                [](const TSharedPtr<FJsonValue>& Value)
                {
                    return Value.IsValid() && Value->AsString() == TEXT("/Game/UERingTests/AdditionalSoft");
                }));
        }
    }

    TestTrue(TEXT("Custom exporter unregisters"), FUERingExporterRegistry::Unregister(Handle));
    TestFalse(TEXT("Custom exporter handle cannot be removed twice"), FUERingExporterRegistry::Unregister(Handle));
    const FUERingExportResult BuiltInExport = FUERingExportManager::Get().ExportAsset(FAssetData(Asset));
    TestTrue(TEXT("Built-in exporter resumes after SDK unregister"), BuiltInExport.IsSuccess());
    TestEqual(TEXT("Built-in DataAsset exporter is restored"),
        BuiltInExport.ExporterName, FString(TEXT("DataAsset")));

    const TSharedPtr<IModelContextProtocolTool> QueryTool = McpModule->FindTool(TEXT("ue_ring_get_semantic"));
    if (QueryTool.IsValid())
    {
        const TSharedRef<FJsonObject> QueryParams = MakeShared<FJsonObject>();
        QueryParams->SetStringField(TEXT("package_name"), PackageName);
        QueryParams->SetNumberField(TEXT("max_bytes"), 4 * 1024 * 1024);
        const FModelContextProtocolToolResult QueryResult = QueryTool->Run(QueryParams);
        bool bIsError = false;
        QueryResult.JsonObject->TryGetBoolField(TEXT("isError"), bIsError);
        TestFalse(TEXT("MCP query succeeds"), bIsError);
        const TSharedPtr<FJsonObject>* Structured = nullptr;
        TestTrue(TEXT("MCP query returns structured content"),
            QueryResult.JsonObject->TryGetObjectField(TEXT("structuredContent"), Structured));
        if (Structured != nullptr)
        {
            TestEqual(TEXT("MCP query returns the requested package"),
                (*Structured)->GetStringField(TEXT("packageName")), PackageName);
            TestFalse(TEXT("Small MCP semantic content is included"),
                (*Structured)->GetBoolField(TEXT("contentOmitted")));
        }

        const TSharedRef<FJsonObject> LimitedParams = MakeShared<FJsonObject>();
        LimitedParams->SetStringField(TEXT("package_name"), PackageName);
        LimitedParams->SetNumberField(TEXT("max_bytes"), 1024);
        const FModelContextProtocolToolResult LimitedResult = QueryTool->Run(LimitedParams);
        const TSharedPtr<FJsonObject>* LimitedStructured = nullptr;
        TestTrue(TEXT("Budgeted MCP query returns structured content"),
            LimitedResult.JsonObject->TryGetObjectField(TEXT("structuredContent"), LimitedStructured));
        if (LimitedStructured != nullptr)
        {
            TestTrue(TEXT("MCP omits semantic content when the final response exceeds max_bytes"),
                (*LimitedStructured)->GetBoolField(TEXT("contentOmitted")));
            TestTrue(TEXT("MCP reports a final response payload within max_bytes"),
                (*LimitedStructured)->GetNumberField(TEXT("responsePayloadBytes")) <= 1024.0);
        }

        const TSharedRef<FJsonObject> TraversalParams = MakeShared<FJsonObject>();
        TraversalParams->SetStringField(TEXT("package_name"), TEXT("/../../C/sentinel"));
        const FModelContextProtocolToolResult TraversalResult = QueryTool->Run(TraversalParams);
        bool bTraversalError = false;
        TraversalResult.JsonObject->TryGetBoolField(TEXT("isError"), bTraversalError);
        TestTrue(TEXT("MCP rejects path traversal package names"), bTraversalError);
    }

    const TSharedPtr<IModelContextProtocolTool> ExportTool = McpModule->FindTool(TEXT("ue_ring_export_asset"));
    if (ExportTool.IsValid())
    {
        const TSharedRef<FJsonObject> ExportParams = MakeShared<FJsonObject>();
        ExportParams->SetStringField(TEXT("package_name"), PackageName);
        ExportParams->SetBoolField(TEXT("rebuild_indexes"), false);
        const FModelContextProtocolToolResult McpExport = ExportTool->Run(ExportParams);
        bool bIsError = false;
        McpExport.JsonObject->TryGetBoolField(TEXT("isError"), bIsError);
        if (bIsError)
        {
            FString ResultJson;
            const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
                TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ResultJson);
            FJsonSerializer::Serialize(McpExport.JsonObject.ToSharedRef(), Writer);
            AddError(TEXT("MCP export returned an error: ") + ResultJson);
        }
        TestFalse(TEXT("MCP export succeeds"), bIsError);
    }

    DeleteExportArtifacts(BuiltInExport.OutputFile);
    IFileManager::Get().Delete(*SourceFile, false, true);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUERingP0BatchAndDeterminismTest,
    "UERing.Exporter.P0.BatchAndDeterminism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUERingP0BatchAndDeterminismTest::RunTest(const FString& Parameters)
{
    using namespace UERingP0Tests;
    UUERingSettings* MutableSettings = GetMutableDefault<UUERingSettings>();
    const bool bPreviousAutoExport = MutableSettings->bEnableAutoExport;
    MutableSettings->bEnableAutoExport = false;
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString PackageName = TEXT("/Game/UERingTests/BP_Batch_") + Suffix;
    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(),
        Package,
        *FPackageName::GetLongPackageAssetName(PackageName),
        BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        TEXT("UERingP0BatchAndDeterminismTest"));
    if (Blueprint != nullptr && !Blueprint->UbergraphPages.IsEmpty())
    {
        UEdGraph* EventGraph = Blueprint->UbergraphPages[0];
        UK2Node_CallDelegate* CallDelegate = NewObject<UK2Node_CallDelegate>(EventGraph);
        CallDelegate->CreateNewGuid();
        EventGraph->AddNode(CallDelegate, false, false);
        if (const FProperty* DamageDelegate = FindFProperty<FProperty>(AActor::StaticClass(), TEXT("OnTakeAnyDamage")))
        {
            CallDelegate->SetFromProperty(DamageDelegate, true, AActor::StaticClass());
        }
        CallDelegate->AllocateDefaultPins();

        UK2Node_CreateDelegate* CreateDelegate = NewObject<UK2Node_CreateDelegate>(EventGraph);
        CreateDelegate->CreateNewGuid();
        EventGraph->AddNode(CreateDelegate, false, false);
        CreateDelegate->AllocateDefaultPins();
        CreateDelegate->SetFunction(TEXT("ReceiveTick"));

        for (const FName EventName : { FName(TEXT("Fire")), FName(TEXT("fire")) })
        {
            UK2Node_CustomEvent* CustomEvent = NewObject<UK2Node_CustomEvent>(EventGraph);
            CustomEvent->CreateNewGuid();
            CustomEvent->CustomFunctionName = EventName;
            if (EventName == TEXT("Fire"))
            {
                CustomEvent->FunctionFlags = FUNC_Net | FUNC_NetServer | FUNC_NetReliable;
                CustomEvent->bCallInEditor = true;
            }
            EventGraph->AddNode(CustomEvent, false, false);
            CustomEvent->AllocateDefaultPins();
        }
    }
    FString SourceFile;
    if (!TestTrue(TEXT("Batch Blueprint saves"), SaveAsset(Package, Blueprint, false, SourceFile)))
    {
        MutableSettings->bEnableAutoExport = bPreviousAutoExport;
        return false;
    }

    const FAssetData ValidAsset(Blueprint);
    FAssetData RedirectorAsset = ValidAsset;
    RedirectorAsset.AssetName = TEXT("RedirectorForBatch");
    RedirectorAsset.AssetClassPath = UObjectRedirector::StaticClass()->GetClassPathName();
    TestEqual(
        TEXT("Standalone redirectors are explicitly excluded"),
        FUERingExportManager::Get().GetExclusionReason(RedirectorAsset),
        FString(TEXT("objectRedirector")));
    TArray<FAssetData> SamePackageAssets = { RedirectorAsset, ValidAsset };
    FUERingExportManager::Get().CanonicalizeAssetsByPackage(SamePackageAssets);
    TestEqual(TEXT("Package selection emits one canonical asset"), SamePackageAssets.Num(), 1);
    if (!SamePackageAssets.IsEmpty())
    {
        TestEqual(
            TEXT("Package selection prefers the real asset over a redirector"),
            SamePackageAssets[0].GetSoftObjectPath().ToString(),
            ValidAsset.GetSoftObjectPath().ToString());
    }
    const FUERingExportResult First = FUERingExportManager::Get().ExportAsset(ValidAsset);
    TestTrue(TEXT("Initial deterministic export succeeds"), First.IsSuccess());
    const FString EnglishSummary = FPaths::ChangeExtension(First.OutputFile, TEXT("md"));
    const FString ChineseSummary = FPaths::ChangeExtension(First.OutputFile, TEXT("zh-CN.md"));
    FString ChineseMarkdown;
    TestTrue(TEXT("English Markdown summary is generated"), IFileManager::Get().FileExists(*EnglishSummary));
    TestTrue(TEXT("Chinese Markdown summary is generated"),
        FFileHelper::LoadFileToString(ChineseMarkdown, *ChineseSummary));
    TestTrue(TEXT("Chinese Markdown summary contains localized UTF-8 content"),
        ChineseMarkdown.Contains(TEXT("语义摘要"))
            && ChineseMarkdown.Contains(TEXT("相邻的确定性 USEM JSON 是权威数据源")));
    FString FirstJson;
    TestTrue(TEXT("Initial deterministic JSON reads"), FFileHelper::LoadFileToString(FirstJson, *First.OutputFile));
    TestTrue(
        TEXT("Multicast delegate nodes export a structured member reference"),
        FirstJson.Contains(TEXT("\"kind\":\"delegate\""))
            && FirstJson.Contains(TEXT("\"name\":\"OnTakeAnyDamage\""))
            && FirstJson.Contains(TEXT("\"owner\":\"/Script/Engine.Actor\"")));
    TestTrue(
        TEXT("Create Delegate nodes export the selected function reference"),
        FirstJson.Contains(TEXT("\"kind\":\"delegateFunction\""))
            && FirstJson.Contains(TEXT("\"name\":\"ReceiveTick\"")));
    TestTrue(
        TEXT("Custom events export authoritative names and RPC flags"),
        FirstJson.Contains(TEXT("\"kind\":\"customEvent\""))
            && FirstJson.Contains(TEXT("\"name\":\"Fire\""))
            && FirstJson.Contains(TEXT("\"server\""))
            && FirstJson.Contains(TEXT("\"reliable\""))
            && FirstJson.Contains(TEXT("\"callInEditor\":true")));
    IFileManager::Get().Delete(*First.OutputFile, false, true);
    const FUERingExportResult Rebuilt = FUERingExportManager::Get().ExportAsset(ValidAsset);
    FString RebuiltJson;
    TestTrue(TEXT("Rebuilt deterministic JSON reads"), FFileHelper::LoadFileToString(RebuiltJson, *Rebuilt.OutputFile));
    TestEqual(TEXT("Same source rebuilds byte-identical JSON"), RebuiltJson, FirstJson);

    const FString MissingPackageName = TEXT("/Game/UERingTests/DA_Missing_") + Suffix;
    UPackage* MissingPackage = CreatePackage(*MissingPackageName);
    UPrimaryAssetLabel* MissingAsset = NewObject<UPrimaryAssetLabel>(
        MissingPackage,
        *FPackageName::GetLongPackageAssetName(MissingPackageName),
        RF_Public | RF_Standalone);

    TArray<FAssetData> Batch;
    for (int32 Index = 0; Index < 100; ++Index)
    {
        Batch.Add(Index == 50 ? FAssetData(MissingAsset) : ValidAsset);
    }
    const TArray<FUERingExportResult> Results = FUERingExportManager::Get().ExportAssets(Batch);
    TestEqual(TEXT("Batch returns one result per asset"), Results.Num(), 100);
    int32 SuccessCount = 0;
    int32 FailureCount = 0;
    for (const FUERingExportResult& Result : Results)
    {
        Result.IsSuccess() ? ++SuccessCount : ++FailureCount;
    }
    TestEqual(TEXT("One failed asset does not stop 99 valid exports"), SuccessCount, 99);
    TestEqual(TEXT("Exactly one batch asset fails"), FailureCount, 1);

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.ScanFilesSynchronous({ SourceFile }, true);
    FString IndexError;
    TestTrue(TEXT("Project index rebuilds"), FUERingIndexManager::Rebuild(IndexError));
    const FString IndexFile = FPaths::Combine(
        FUERingExportManager::Get().GetOutputRoot(),
        TEXT("index/project.uesem.index.json"));
    FString IndexJson;
    TSharedPtr<FJsonObject> IndexRoot;
    TestTrue(TEXT("Project index parses"),
        FFileHelper::LoadFileToString(IndexJson, *IndexFile)
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(IndexJson), IndexRoot)
        && IndexRoot.IsValid());
    TSharedPtr<FJsonObject> MatchingEntry;
    int32 MatchingEntryCount = 0;
    if (IndexRoot.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
        if (IndexRoot->TryGetArrayField(TEXT("assets"), Entries) && Entries != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Entries)
            {
                const TSharedPtr<FJsonObject>* Entry = nullptr;
                if (!Value->TryGetObject(Entry) || Entry == nullptr
                    || (*Entry)->GetStringField(TEXT("packageName")) != PackageName)
                {
                    continue;
                }
                MatchingEntry = *Entry;
                ++MatchingEntryCount;
            }
        }
    }
    TestEqual(TEXT("Project index emits one entry per package"), MatchingEntryCount, 1);
    if (TestTrue(TEXT("Project index contains the exported asset"), MatchingEntry.IsValid()))
    {
        const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
        TestEqual(TEXT("Index status is current"), MatchingEntry->GetStringField(TEXT("status")), FString(TEXT("ok")));
        TestFalse(TEXT("Index source hash is populated"), MatchingEntry->GetStringField(TEXT("sourceHash")).IsEmpty());
        TestFalse(TEXT("Index semantic hash is populated"), MatchingEntry->GetStringField(TEXT("semanticHash")).IsEmpty());
        TestFalse(TEXT("Index export time is populated"), MatchingEntry->GetStringField(TEXT("exportedAtUtc")).IsEmpty());
        TestTrue(TEXT("Index dependency count is numeric"), MatchingEntry->HasTypedField<EJson::Number>(TEXT("dependencyCount")));
        TestTrue(TEXT("Index referencer count is numeric"), MatchingEntry->HasTypedField<EJson::Number>(TEXT("referencerCount")));
        TestTrue(TEXT("Index owner module is present"), MatchingEntry->HasTypedField<EJson::String>(TEXT("ownerModule")));
        TestTrue(TEXT("Index primary asset id is present"), MatchingEntry->HasTypedField<EJson::String>(TEXT("primaryAssetId")));
        TestTrue(TEXT("Index asset tags are present"), MatchingEntry->TryGetArrayField(TEXT("assetTags"), Tags));
        TestEqual(TEXT("Index contains the semantic kind"),
            MatchingEntry->GetStringField(TEXT("semanticKind")), FString(TEXT("Blueprint")));
        TestFalse(TEXT("Index contains the exporter name"),
            MatchingEntry->GetStringField(TEXT("exporter")).IsEmpty());
    }

    const FString ProjectGraphFile = FPaths::Combine(
        FUERingExportManager::Get().GetOutputRoot(),
        TEXT("index/project.uesem.graph.json"));
    TSharedPtr<FJsonObject> ProjectGraph;
    TestTrue(TEXT("Unified project semantic graph parses"),
        LoadJsonObject(ProjectGraphFile, ProjectGraph));
    if (ProjectGraph.IsValid())
    {
        TestEqual(
            TEXT("Unified project graph schema is explicit"),
            ProjectGraph->GetStringField(TEXT("schema")),
            FString(TEXT("com.ue-ring.usem.project-graph")));
        const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Edges = nullptr;
        TestTrue(TEXT("Unified project graph has nodes"),
            ProjectGraph->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes != nullptr && !Nodes->IsEmpty());
        TestTrue(TEXT("Unified project graph has edges"),
            ProjectGraph->TryGetArrayField(TEXT("edges"), Edges) && Edges != nullptr && !Edges->IsEmpty());
        if (Nodes != nullptr && Edges != nullptr)
        {
            TSet<FString> NodeIds;
            for (const TSharedPtr<FJsonValue>& NodeValue : *Nodes)
            {
                const TSharedPtr<FJsonObject>* Node = nullptr;
                if (NodeValue.IsValid() && NodeValue->TryGetObject(Node) && Node != nullptr)
                {
                    NodeIds.Add((*Node)->GetStringField(TEXT("id")));
                }
            }
            bool bContainsGraph = false;
            bool bDelegateRelation = false;
            bool bEvidencePointer = false;
            bool bContributorOwnership = true;
            bool bAllEndpointsResolve = true;
            bool bAllSymbolIdsCanonical = true;
            for (const FString& NodeId : NodeIds)
            {
                if (NodeId.StartsWith(TEXT("symbol:")))
                {
                    bAllSymbolIdsCanonical &= NodeId.Equals(NodeId.ToLower(), ESearchCase::CaseSensitive);
                }
            }
            for (const TSharedPtr<FJsonValue>& EdgeValue : *Edges)
            {
                const TSharedPtr<FJsonObject>* Edge = nullptr;
                if (!EdgeValue.IsValid() || !EdgeValue->TryGetObject(Edge) || Edge == nullptr) continue;
                const FString From = (*Edge)->GetStringField(TEXT("from"));
                const FString To = (*Edge)->GetStringField(TEXT("to"));
                const FString Relation = (*Edge)->GetStringField(TEXT("relation"));
                FString ContributorPackage;
                bContributorOwnership &= (*Edge)->TryGetStringField(
                    TEXT("contributorPackage"), ContributorPackage)
                    && ContributorPackage.StartsWith(TEXT("/"));
                bAllEndpointsResolve &= NodeIds.Contains(From) && NodeIds.Contains(To);
                if (From == TEXT("asset:") + PackageName && Relation == TEXT("containsGraph"))
                {
                    bContainsGraph = true;
                }
                if (Relation.StartsWith(TEXT("delegate"))
                    && (*Edge)->GetStringField(TEXT("evidenceSource")).Contains(TEXT(".uesem.json")))
                {
                    bDelegateRelation = true;
                    FString Pointer;
                    bEvidencePointer |= (*Edge)->TryGetStringField(TEXT("evidencePointer"), Pointer)
                        && Pointer.Contains(TEXT("/memberReference"));
                }
            }
            TestTrue(TEXT("Every unified graph edge endpoint resolves"), bAllEndpointsResolve);
            TestTrue(TEXT("Unified graph symbol IDs have canonical casing"), bAllSymbolIdsCanonical);
            TestTrue(TEXT("Unified graph connects the asset to its Blueprint graph"), bContainsGraph);
            TestTrue(TEXT("Unified graph extracts Blueprint delegate relations"), bDelegateRelation);
            TestTrue(TEXT("Unified graph relations retain JSON Pointer evidence"), bEvidencePointer);
            TestTrue(TEXT("Every unified graph edge has an owning contributor package"), bContributorOwnership);
        }
    }
    FString FirstProjectGraphJson;
    TestTrue(TEXT("Unified project graph JSON reads for determinism"),
        FFileHelper::LoadFileToString(FirstProjectGraphJson, *ProjectGraphFile));
    FString FirstIndexJson;
    TestTrue(TEXT("Project index JSON reads before incremental update"),
        FFileHelper::LoadFileToString(FirstIndexJson, *IndexFile));
    const FString DependencyFile = FPaths::Combine(
        FUERingExportManager::Get().GetOutputRoot(),
        TEXT("index/dependencies.uesem.json"));
    FString FirstDependencyJson;
    TestTrue(TEXT("Dependency graph JSON reads before incremental update"),
        FFileHelper::LoadFileToString(FirstDependencyJson, *DependencyFile));
    TestTrue(TEXT("Single-package incremental index update succeeds"),
        FUERingIndexManager::UpdatePackages({ FName(*PackageName) }, IndexError));
    FString IncrementalIndexJson;
    FString IncrementalDependencyJson;
    FString IncrementalProjectGraphJson;
    TestTrue(TEXT("Incremental project index JSON reads"),
        FFileHelper::LoadFileToString(IncrementalIndexJson, *IndexFile));
    TestTrue(TEXT("Incremental dependency graph JSON reads"),
        FFileHelper::LoadFileToString(IncrementalDependencyJson, *DependencyFile));
    TestTrue(TEXT("Incremental unified graph JSON reads"),
        FFileHelper::LoadFileToString(IncrementalProjectGraphJson, *ProjectGraphFile));
    auto TestByteIdentical = [this](const TCHAR* Label, const FString& Actual, const FString& Expected)
    {
        if (Actual == Expected) return true;
        const int32 CommonLength = FMath::Min(Actual.Len(), Expected.Len());
        int32 Difference = 0;
        while (Difference < CommonLength && Actual[Difference] == Expected[Difference]) ++Difference;
        const int32 ContextStart = FMath::Max(0, Difference - 80);
        AddError(FString::Printf(
            TEXT("%s differs at character %d (actual length %d, expected length %d). Actual: %s Expected: %s"),
            Label,
            Difference,
            Actual.Len(),
            Expected.Len(),
            *Actual.Mid(ContextStart, 160),
            *Expected.Mid(ContextStart, 160)));
        return false;
    };
    TestByteIdentical(
        TEXT("No-change incremental project index"), IncrementalIndexJson, FirstIndexJson);
    TestByteIdentical(
        TEXT("No-change incremental dependency graph"), IncrementalDependencyJson, FirstDependencyJson);
    TestByteIdentical(
        TEXT("No-change incremental unified graph"), IncrementalProjectGraphJson, FirstProjectGraphJson);
    TestTrue(TEXT("Project index and unified graph rebuild deterministically"),
        FUERingIndexManager::Rebuild(IndexError));
    FString SecondProjectGraphJson;
    TestTrue(TEXT("Rebuilt unified project graph JSON reads"),
        FFileHelper::LoadFileToString(SecondProjectGraphJson, *ProjectGraphFile));
    TestByteIdentical(
        TEXT("Unified project graph after full rebuild"), SecondProjectGraphJson, FirstProjectGraphJson);
    FString FullIndexJson;
    FString FullDependencyJson;
    TestTrue(TEXT("Full project index JSON reads after incremental comparison"),
        FFileHelper::LoadFileToString(FullIndexJson, *IndexFile));
    TestTrue(TEXT("Full dependency graph JSON reads after incremental comparison"),
        FFileHelper::LoadFileToString(FullDependencyJson, *DependencyFile));
    TestByteIdentical(
        TEXT("Incremental and full project indexes"), FullIndexJson, IncrementalIndexJson);
    TestByteIdentical(
        TEXT("Incremental and full dependency graphs"), FullDependencyJson, IncrementalDependencyJson);

    TSharedPtr<FJsonObject> MutatedRoot;
    const TSharedPtr<FJsonObject>* MutatedSemantics = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* MutatedGraphs = nullptr;
    const TSharedPtr<FJsonObject>* MutatedGraph = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* MutatedNodes = nullptr;
    const TSharedPtr<FJsonObject>* MutatedNode = nullptr;
    bool bMutatedGraphNode = LoadJsonObject(Rebuilt.OutputFile, MutatedRoot)
        && MutatedRoot->TryGetObjectField(TEXT("semantics"), MutatedSemantics)
        && (*MutatedSemantics)->TryGetArrayField(TEXT("graphs"), MutatedGraphs)
        && !MutatedGraphs->IsEmpty()
        && (*MutatedGraphs)[0]->TryGetObject(MutatedGraph)
        && (*MutatedGraph)->TryGetArrayField(TEXT("nodes"), MutatedNodes)
        && !MutatedNodes->IsEmpty()
        && (*MutatedNodes)[0]->TryGetObject(MutatedNode);
    TestTrue(TEXT("Test sidecar exposes a graph node for changed-contribution coverage"), bMutatedGraphNode);
    if (bMutatedGraphNode)
    {
        (*MutatedNode)->SetStringField(TEXT("title"), TEXT("IncrementalChangedTitle"));
        TestTrue(TEXT("Changed test sidecar writes"), SaveCondensedJsonObject(Rebuilt.OutputFile, MutatedRoot.ToSharedRef()));
        TestTrue(TEXT("Changed single-package incremental index update succeeds"),
            FUERingIndexManager::UpdatePackages({ FName(*PackageName) }, IndexError));

        FString ChangedIncrementalIndexJson;
        FString ChangedIncrementalDependencyJson;
        FString ChangedIncrementalProjectGraphJson;
        TestTrue(TEXT("Changed incremental project index JSON reads"),
            FFileHelper::LoadFileToString(ChangedIncrementalIndexJson, *IndexFile));
        TestTrue(TEXT("Changed incremental dependency graph JSON reads"),
            FFileHelper::LoadFileToString(ChangedIncrementalDependencyJson, *DependencyFile));
        TestTrue(TEXT("Changed incremental unified graph JSON reads"),
            FFileHelper::LoadFileToString(ChangedIncrementalProjectGraphJson, *ProjectGraphFile));
        TestTrue(TEXT("Changed incremental unified graph includes the updated semantic node"),
            ChangedIncrementalProjectGraphJson.Contains(TEXT("IncrementalChangedTitle")));

        TestTrue(TEXT("Full rebuild after changed incremental update succeeds"),
            FUERingIndexManager::Rebuild(IndexError));
        FString ChangedFullIndexJson;
        FString ChangedFullDependencyJson;
        FString ChangedFullProjectGraphJson;
        TestTrue(TEXT("Changed full project index JSON reads"),
            FFileHelper::LoadFileToString(ChangedFullIndexJson, *IndexFile));
        TestTrue(TEXT("Changed full dependency graph JSON reads"),
            FFileHelper::LoadFileToString(ChangedFullDependencyJson, *DependencyFile));
        TestTrue(TEXT("Changed full unified graph JSON reads"),
            FFileHelper::LoadFileToString(ChangedFullProjectGraphJson, *ProjectGraphFile));
        TestByteIdentical(
            TEXT("Changed incremental and full project indexes"),
            ChangedFullIndexJson,
            ChangedIncrementalIndexJson);
        TestByteIdentical(
            TEXT("Changed incremental and full dependency graphs"),
            ChangedFullDependencyJson,
            ChangedIncrementalDependencyJson);
        TestByteIdentical(
            TEXT("Changed incremental and full unified graphs"),
            ChangedFullProjectGraphJson,
            ChangedIncrementalProjectGraphJson);
    }

    const FString SqliteFile = FPaths::Combine(
        FUERingExportManager::Get().GetOutputRoot(),
        TEXT("index/project.uesem.sqlite"));
    FSQLiteDatabase Database;
    TestTrue(TEXT("SQLite search index is generated"), IFileManager::Get().FileExists(*SqliteFile));
    if (TestTrue(TEXT("SQLite search index opens read-only"),
        Database.Open(*SqliteFile, ESQLiteDatabaseOpenMode::ReadOnly)))
    {
        TestTrue(TEXT("SQLite search index passes a quick integrity check"),
            Database.PerformQuickIntegrityCheck());
        int32 SqliteUserVersion = 0;
        TestTrue(TEXT("SQLite search index exposes its schema version"),
            Database.GetUserVersion(SqliteUserVersion));
        TestEqual(TEXT("SQLite incremental schema is version 6"), SqliteUserVersion, 6);
        FSQLitePreparedStatement Query(
            Database,
            TEXT("SELECT semantic_kind, exporter, search_text FROM assets WHERE package_name = ?"));
        FString IndexedKind;
        FString IndexedExporter;
        FString IndexedSearchText;
        TestTrue(TEXT("SQLite package query prepares"), Query.IsValid());
        TestTrue(TEXT("SQLite package query binds"), Query.SetBindingValueByIndex(1, PackageName));
        const int64 RowCount = Query.Execute(
            [&IndexedKind, &IndexedExporter, &IndexedSearchText](const FSQLitePreparedStatement& Row)
            {
                return Row.GetColumnValueByIndex(0, IndexedKind)
                    && Row.GetColumnValueByIndex(1, IndexedExporter)
                    && Row.GetColumnValueByIndex(2, IndexedSearchText)
                    ? ESQLitePreparedStatementExecuteRowResult::Continue
                    : ESQLitePreparedStatementExecuteRowResult::Error;
            });
        TestEqual(TEXT("SQLite package query returns one row"), RowCount, static_cast<int64>(1));
        TestEqual(TEXT("SQLite stores semantic kind"), IndexedKind, FString(TEXT("Blueprint")));
        TestFalse(TEXT("SQLite stores exporter name"), IndexedExporter.IsEmpty());
        TestTrue(TEXT("SQLite search text includes package and class"),
            IndexedSearchText.Contains(PackageName) && IndexedSearchText.Contains(TEXT("Blueprint")));
        TestTrue(TEXT("SQLite package query releases"), Query.Destroy());

        FSQLitePreparedStatement GraphQuery(
            Database,
            TEXT("SELECT COUNT(*) FROM graph_edges e "
                 "JOIN graph_nodes n ON n.node_id = e.source_node "
                 "WHERE n.package_name = ? AND e.relation = 'containsGraph'"));
        int64 GraphEdgeCount = 0;
        TestTrue(TEXT("SQLite unified graph query prepares"), GraphQuery.IsValid());
        TestTrue(TEXT("SQLite unified graph query binds"), GraphQuery.SetBindingValueByIndex(1, PackageName));
        const int64 GraphRowCount = GraphQuery.Execute(
            [&GraphEdgeCount](const FSQLitePreparedStatement& Row)
            {
                return Row.GetColumnValueByIndex(0, GraphEdgeCount)
                    ? ESQLitePreparedStatementExecuteRowResult::Continue
                    : ESQLitePreparedStatementExecuteRowResult::Error;
            });
        TestEqual(TEXT("SQLite unified graph query returns one aggregate row"), GraphRowCount, static_cast<int64>(1));
        TestTrue(TEXT("SQLite contains Blueprint graph relations"), GraphEdgeCount > 0);
        TestTrue(TEXT("SQLite unified graph query releases"), GraphQuery.Destroy());
        TestTrue(TEXT("SQLite search index closes"), Database.Close());
    }

    FString MigrationError;
    TestTrue(TEXT("Blueprint-to-C++ migration assistance report rebuilds"),
        FUERingBlueprintMigrationReporter::Rebuild(MigrationError));
    const FString MigrationJsonFile = FPaths::Combine(
        FUERingExportManager::Get().GetOutputRoot(),
        TEXT("reports/blueprint-cpp-migration.uesem.json"));
    const FString MigrationMarkdownFile = FPaths::Combine(
        FUERingExportManager::Get().GetOutputRoot(),
        TEXT("reports/blueprint-cpp-migration.md"));
    FString FirstMigrationJson;
    TSharedPtr<FJsonObject> MigrationRoot;
    TestTrue(TEXT("Migration assistance JSON parses"),
        FFileHelper::LoadFileToString(FirstMigrationJson, *MigrationJsonFile)
            && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FirstMigrationJson), MigrationRoot)
            && MigrationRoot.IsValid());
    TestTrue(TEXT("Migration assistance Markdown is generated"),
        IFileManager::Get().FileExists(*MigrationMarkdownFile));
    if (MigrationRoot.IsValid())
    {
        TestTrue(TEXT("Migration report explicitly avoids claiming generated equivalent C++"),
            MigrationRoot->GetStringField(TEXT("disclaimer")).Contains(TEXT("not generated equivalent C++")));
        const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
        TSharedPtr<FJsonObject> MatchingCandidate;
        if (MigrationRoot->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& CandidateValue : *Candidates)
            {
                const TSharedPtr<FJsonObject>* Candidate = nullptr;
                if (CandidateValue.IsValid()
                    && CandidateValue->TryGetObject(Candidate)
                    && Candidate != nullptr
                    && (*Candidate)->GetStringField(TEXT("packageName")) == PackageName)
                {
                    MatchingCandidate = *Candidate;
                    break;
                }
            }
        }
        if (TestTrue(TEXT("Migration report contains the exported Blueprint"), MatchingCandidate.IsValid()))
        {
            const TSharedPtr<FJsonObject>* Metrics = nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Recommendations = nullptr;
            TestTrue(TEXT("Migration candidate contains evidence metrics"),
                MatchingCandidate->TryGetObjectField(TEXT("metrics"), Metrics)
                    && Metrics != nullptr
                    && (*Metrics)->HasTypedField<EJson::Number>(TEXT("nodes")));
            TestTrue(TEXT("Migration candidate contains recommendations"),
                MatchingCandidate->TryGetArrayField(TEXT("recommendations"), Recommendations)
                    && Recommendations != nullptr
                    && !Recommendations->IsEmpty());
        }
    }
    TestTrue(TEXT("Migration assistance report updates one package incrementally"),
        FUERingBlueprintMigrationReporter::UpdatePackages({ FName(*PackageName) }, MigrationError));
    FString IncrementalMigrationJson;
    TestTrue(TEXT("Incremental migration assistance JSON reads"),
        FFileHelper::LoadFileToString(IncrementalMigrationJson, *MigrationJsonFile));
    TestByteIdentical(
        TEXT("Incremental and full migration assistance reports"),
        IncrementalMigrationJson,
        FirstMigrationJson);
    TestTrue(TEXT("Migration assistance report rebuilds deterministically"),
        FUERingBlueprintMigrationReporter::Rebuild(MigrationError));
    FString SecondMigrationJson;
    TestTrue(TEXT("Rebuilt migration assistance JSON reads"),
        FFileHelper::LoadFileToString(SecondMigrationJson, *MigrationJsonFile));
    TestEqual(TEXT("Same semantics produce a byte-identical migration report"),
        SecondMigrationJson, FirstMigrationJson);

    FString CppIndexError;
    TestTrue(TEXT("C++ reflection index rebuilds before incremental comparison"),
        FUERingCppIndexer::Rebuild(CppIndexError));
    const FString ReflectionFile = FPaths::Combine(
        FUERingExportManager::Get().GetOutputRoot(),
        TEXT("cpp/reflection.uesem.json"));
    FString FullReflectionJson;
    TestTrue(TEXT("Full C++ reflection JSON reads"),
        FFileHelper::LoadFileToString(FullReflectionJson, *ReflectionFile));
    TestTrue(TEXT("C++ reflection index updates one package incrementally"),
        FUERingCppIndexer::UpdatePackages({ FName(*PackageName) }, CppIndexError));
    FString IncrementalReflectionJson;
    TestTrue(TEXT("Incremental C++ reflection JSON reads"),
        FFileHelper::LoadFileToString(IncrementalReflectionJson, *ReflectionFile));
    TestByteIdentical(
        TEXT("Incremental and full C++ reflection indexes"),
        IncrementalReflectionJson,
        FullReflectionJson);

    IFileManager::Get().Delete(*Rebuilt.OutputFile, false, true);
    FUERingSummaryWriter::Remove(Rebuilt.OutputFile);
    TestTrue(TEXT("Deleted Blueprint is removed from C++ reflection incrementally"),
        FUERingCppIndexer::UpdatePackages({ FName(*PackageName) }, CppIndexError));
    FString DeletedIncrementalReflectionJson;
    TestTrue(TEXT("Deleted incremental C++ reflection JSON reads"),
        FFileHelper::LoadFileToString(DeletedIncrementalReflectionJson, *ReflectionFile));
    TestTrue(TEXT("Deleted Blueprint is removed from migration assistance incrementally"),
        FUERingBlueprintMigrationReporter::UpdatePackages({ FName(*PackageName) }, MigrationError));
    FString DeletedIncrementalMigrationJson;
    TestTrue(TEXT("Deleted incremental migration assistance JSON reads"),
        FFileHelper::LoadFileToString(DeletedIncrementalMigrationJson, *MigrationJsonFile));
    TestTrue(TEXT("C++ reflection index rebuilds after deleted incremental update"),
        FUERingCppIndexer::Rebuild(CppIndexError));
    FString DeletedFullReflectionJson;
    TestTrue(TEXT("Deleted full C++ reflection JSON reads"),
        FFileHelper::LoadFileToString(DeletedFullReflectionJson, *ReflectionFile));
    TestByteIdentical(
        TEXT("Deleted incremental and full C++ reflection indexes"),
        DeletedIncrementalReflectionJson,
        DeletedFullReflectionJson);
    TestTrue(TEXT("Migration assistance report rebuilds after deleted incremental update"),
        FUERingBlueprintMigrationReporter::Rebuild(MigrationError));
    FString DeletedFullMigrationJson;
    TestTrue(TEXT("Deleted full migration assistance JSON reads"),
        FFileHelper::LoadFileToString(DeletedFullMigrationJson, *MigrationJsonFile));
    TestByteIdentical(
        TEXT("Deleted incremental and full migration assistance reports"),
        DeletedIncrementalMigrationJson,
        DeletedFullMigrationJson);
    IFileManager::Get().Delete(*SourceFile, false, true);
    MutableSettings->bEnableAutoExport = bPreviousAutoExport;
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUERingP0FullExportCleanupTest,
    "UERing.Exporter.P0.FullExportCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUERingP0FullExportCleanupTest::RunTest(const FString& Parameters)
{
    UUERingSettings* Settings = GetMutableDefault<UUERingSettings>();
    const FString PreviousOutputRoot = Settings->OutputRoot.Path;
    const bool bPreviousGraphs = Settings->bIncludeGraphVisualizations;
    const bool bPreviousCpp = Settings->bIncludeCppIndex;
    const bool bPreviousMigration = Settings->bIncludeBlueprintMigrationReport;
    const FString TestRoot = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("UERingTests/FullExportCleanup_") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    Settings->OutputRoot.Path = TestRoot;
    Settings->bIncludeGraphVisualizations = true;
    Settings->bIncludeCppIndex = true;
    Settings->bIncludeBlueprintMigrationReport = true;

    auto WriteFixture = [this, &TestRoot](const FString& RelativePath)
    {
        const FString Filename = FPaths::Combine(TestRoot, RelativePath);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        TestTrue(
            *FString::Printf(TEXT("Cleanup fixture writes: %s"), *RelativePath),
            FFileHelper::SaveStringToFile(TEXT("fixture\n"), *Filename));
        return Filename;
    };

    const FString OldDiff = WriteFixture(TEXT("diffs/old.change.json"));
    const FString OldBundle = WriteFixture(TEXT("bundles/old/manifest.json"));
    const FString OldTombstone = WriteFixture(TEXT("tombstones/old.deleted.json"));
    const FString OldLog = WriteFixture(TEXT("logs/export-errors.jsonl"));
    const FString CurrentGraph = WriteFixture(TEXT("graphs/current.callgraph.mmd"));
    const FString CurrentCpp = WriteFixture(TEXT("cpp/reflection.uesem.json"));
    const FString CurrentReport = WriteFixture(TEXT("reports/current.md"));

    FString Error;
    TestTrue(TEXT("Full export preparation succeeds"),
        FUERingExportManager::Get().PrepareFullExport(Error));
    TestFalse(TEXT("Full export removes prior diffs"), IFileManager::Get().FileExists(*OldDiff));
    TestFalse(TEXT("Full export removes prior Bundles"), IFileManager::Get().FileExists(*OldBundle));
    TestFalse(TEXT("Full export removes prior tombstones"), IFileManager::Get().FileExists(*OldTombstone));
    TestFalse(TEXT("Full export removes prior logs"), IFileManager::Get().FileExists(*OldLog));
    TestTrue(TEXT("Enabled graph output is retained until rebuilt"), IFileManager::Get().FileExists(*CurrentGraph));
    TestTrue(TEXT("Enabled C++ output is retained until rebuilt"), IFileManager::Get().FileExists(*CurrentCpp));
    TestTrue(TEXT("Enabled reports are retained until rebuilt"), IFileManager::Get().FileExists(*CurrentReport));

    const FString OrphanSemantic = WriteFixture(TEXT("content/Game/Removed.uesem.json"));
    const FString OrphanEnglish = WriteFixture(TEXT("content/Game/Removed.uesem.md"));
    const FString OrphanChinese = WriteFixture(TEXT("content/Game/Removed.uesem.zh-CN.md"));
    FString OrphanMermaid;
    FString OrphanGraphviz;
    FUERingDerivedArtifactWriter::GetGraphArtifactFiles(OrphanSemantic, OrphanMermaid, OrphanGraphviz);
    WriteFixture(TEXT("graphs/content/Game/Removed.uesem.callgraph.mmd"));
    WriteFixture(TEXT("graphs/content/Game/Removed.uesem.callgraph.dot"));
    const FString OrphanChange = WriteFixture(TEXT("diffs/content/Game/Removed.uesem.change.json"));
    const FString DetachedSummary = WriteFixture(TEXT("content/Game/Detached.uesem.md"));
    const FString DetachedGraph = WriteFixture(TEXT("graphs/content/Game/Detached.uesem.callgraph.mmd"));

    TestTrue(TEXT("Full export finalization succeeds"),
        FUERingExportManager::Get().FinalizeFullExport(TArray<FAssetData>(), Error));
    TestFalse(TEXT("Full export removes orphan semantics"), IFileManager::Get().FileExists(*OrphanSemantic));
    TestFalse(TEXT("Full export removes orphan English summaries"), IFileManager::Get().FileExists(*OrphanEnglish));
    TestFalse(TEXT("Full export removes orphan Chinese summaries"), IFileManager::Get().FileExists(*OrphanChinese));
    TestFalse(TEXT("Full export removes orphan Mermaid graphs"), IFileManager::Get().FileExists(*OrphanMermaid));
    TestFalse(TEXT("Full export removes orphan Graphviz graphs"), IFileManager::Get().FileExists(*OrphanGraphviz));
    TestFalse(TEXT("Full export removes orphan change summaries"), IFileManager::Get().FileExists(*OrphanChange));
    TestFalse(TEXT("Full export removes summaries without a sidecar"), IFileManager::Get().FileExists(*DetachedSummary));
    TestFalse(TEXT("Full export removes graphs without a sidecar"), IFileManager::Get().FileExists(*DetachedGraph));
    TestFalse(TEXT("Full export removes empty orphan semantic directories"),
        IFileManager::Get().DirectoryExists(*FPaths::GetPath(OrphanSemantic)));
    TestFalse(TEXT("Full export removes empty orphan graph directories"),
        IFileManager::Get().DirectoryExists(*FPaths::GetPath(OrphanMermaid)));

    IFileManager::Get().DeleteDirectory(*TestRoot, false, true);
    Settings->OutputRoot.Path = PreviousOutputRoot;
    Settings->bIncludeGraphVisualizations = bPreviousGraphs;
    Settings->bIncludeCppIndex = bPreviousCpp;
    Settings->bIncludeBlueprintMigrationReport = bPreviousMigration;
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUERingP0BundleSecurityAndCleanupTest,
    "UERing.Exporter.P0.BundleSecurityAndCleanup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUERingP0BundleSecurityAndCleanupTest::RunTest(const FString& Parameters)
{
    using namespace UERingP0Tests;
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString BundleFolder = TEXT("/Game/UERingTests/Bundle_") + Suffix;
    const FString PackageName = BundleFolder + TEXT("/BP_Primary");
    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(),
        Package,
        *FPackageName::GetLongPackageAssetName(PackageName),
        BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        TEXT("UERingP0BundleSecurityAndCleanupTest"));
    FString SourceFile;
    if (!TestTrue(TEXT("Bundle test Blueprint saves"), SaveAsset(Package, Blueprint, false, SourceFile)))
    {
        return false;
    }

    const FUERingExportResult Export = FUERingExportManager::Get().ExportAsset(FAssetData(Blueprint));
    if (!TestTrue(TEXT("Bundle test semantic export succeeds"), Export.IsSuccess()))
    {
        IFileManager::Get().Delete(*SourceFile, false, true);
        return false;
    }

    const FString SecondPackageName = BundleFolder + TEXT("/BP_Secondary");
    UPackage* SecondPackage = CreatePackage(*SecondPackageName);
    UBlueprint* SecondBlueprint = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(),
        SecondPackage,
        *FPackageName::GetLongPackageAssetName(SecondPackageName),
        BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        TEXT("UERingP0BundleSecurityAndCleanupTest"));
    FString SecondSourceFile;
    if (!TestTrue(TEXT("Second bundle test Blueprint saves"),
        SaveAsset(SecondPackage, SecondBlueprint, false, SecondSourceFile)))
    {
        DeleteExportArtifacts(Export.OutputFile);
        IFileManager::Get().Delete(*SourceFile, false, true);
        return false;
    }
    const FUERingExportResult SecondExport = FUERingExportManager::Get().ExportAsset(FAssetData(SecondBlueprint));
    if (!TestTrue(TEXT("Second bundle test semantic export succeeds"), SecondExport.IsSuccess()))
    {
        DeleteExportArtifacts(Export.OutputFile);
        IFileManager::Get().Delete(*SourceFile, false, true);
        IFileManager::Get().Delete(*SecondSourceFile, false, true);
        return false;
    }
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.ScanFilesSynchronous({ SourceFile, SecondSourceFile }, true);

    const FString ConfigFilename = FString::Printf(TEXT("UERingSecurityTest_%s.ini"), *Suffix);
    const FString ConfigFile = FPaths::Combine(FPaths::ProjectConfigDir(), ConfigFilename);
    const FString Secret = TEXT("UERING_P0_SECRET_MUST_NOT_LEAK");
    const FString ConfigText = FString::Printf(
        TEXT("[Credentials]\nApiKey=%s\nProjectPath=%sContent\nUserPath=%sDocuments\nPublicEndpoint=https://example.invalid\n"),
        *Secret,
        *FPaths::ProjectDir(),
        FPlatformProcess::UserDir());
    TestTrue(TEXT("Bundle security fixture writes"), FFileHelper::SaveStringToFile(
        ConfigText, *ConfigFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

    TArray<FString> PreviewFiles;
    FString Error;
    TestTrue(TEXT("Bundle preview succeeds"), FUERingBundleBuilder::Preview(PackageName, PreviewFiles, Error));
    TestTrue(TEXT("Bundle preview includes the sanitized config target"),
        PreviewFiles.Contains(FPaths::Combine(TEXT("config"), ConfigFilename)));
    TestTrue(TEXT("Bundle preview includes a context plan"), PreviewFiles.Contains(TEXT("context-plan.json")));
    TestTrue(TEXT("Bundle preview includes the English prompt"),
        PreviewFiles.Contains(TEXT("prompts/analysis.en.md")));
    TestTrue(TEXT("Bundle preview includes the Chinese prompt"),
        PreviewFiles.Contains(TEXT("prompts/analysis.zh-CN.md")));

    FString BundleDirectory;
    Error.Reset();
    TestTrue(TEXT("Bundle builds"), FUERingBundleBuilder::Build(PackageName, BundleDirectory, Error));
    if (!Error.IsEmpty())
    {
        AddError(Error);
    }
    const FString BundledConfig = FPaths::Combine(BundleDirectory, TEXT("config"), ConfigFilename);
    FString SanitizedConfig;
    TestTrue(TEXT("Sanitized config is included"), FFileHelper::LoadFileToString(SanitizedConfig, *BundledConfig));
    TestFalse(TEXT("Bundle does not contain plaintext API key"), SanitizedConfig.Contains(Secret));
    TestTrue(TEXT("Sensitive config field is redacted"), SanitizedConfig.Contains(TEXT("ApiKey=[REDACTED]")));
    TestTrue(TEXT("Project path is replaced"), SanitizedConfig.Contains(TEXT("${PROJECT_DIR}")));
    TestTrue(TEXT("User path is replaced"), SanitizedConfig.Contains(TEXT("${USER_DIR}")));

    TSharedPtr<FJsonObject> ContextPlan;
    TestTrue(TEXT("Bundle context plan parses"), LoadJsonObject(
        FPaths::Combine(BundleDirectory, TEXT("context-plan.json")), ContextPlan));
    if (ContextPlan.IsValid())
    {
        TestEqual(TEXT("Asset bundle context scope"),
            ContextPlan->GetStringField(TEXT("scope")), FString(TEXT("asset")));
        TestTrue(TEXT("Context plan records root bytes"),
            ContextPlan->HasTypedField<EJson::Number>(TEXT("rootBytes")));
        TestFalse(TEXT("Small root semantic fits the configured budget"),
            ContextPlan->GetBoolField(TEXT("rootAssetsExceedBudget")));
    }
    FString EnglishPrompt;
    FString ChinesePrompt;
    TestTrue(TEXT("English prompt reads"), FFileHelper::LoadFileToString(
        EnglishPrompt, *FPaths::Combine(BundleDirectory, TEXT("prompts/analysis.en.md"))));
    TestTrue(TEXT("Chinese prompt reads"), FFileHelper::LoadFileToString(
        ChinesePrompt, *FPaths::Combine(BundleDirectory, TEXT("prompts/analysis.zh-CN.md"))));
    TestTrue(TEXT("English prompt explains initial context selection"),
        EnglishPrompt.Contains(TEXT("includeInInitialContext")));
    TestTrue(TEXT("Chinese prompt remains valid UTF-8 text"),
        ChinesePrompt.Contains(TEXT("项目语义分析")));

    FString OriginalSemantic;
    TestTrue(TEXT("Original semantic reads before cppLinks boundary test"),
        FFileHelper::LoadFileToString(OriginalSemantic, *Export.OutputFile));
    TSharedPtr<FJsonObject> TamperedRoot;
    TestTrue(TEXT("Original semantic parses before cppLinks boundary test"),
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(OriginalSemantic), TamperedRoot)
            && TamperedRoot.IsValid());
    const FString OutsideHeader = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("UERingTests"), TEXT("OutsideBundleRoot.h"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutsideHeader), true);
    TestTrue(TEXT("Outside cppLinks fixture writes"), FFileHelper::SaveStringToFile(
        TEXT("// must never enter a bundle\n"), *OutsideHeader));
    if (TamperedRoot.IsValid())
    {
        const TSharedRef<FJsonObject> MaliciousLink = MakeShared<FJsonObject>();
        MaliciousLink->SetStringField(TEXT("owner"), TEXT("Malicious"));
        MaliciousLink->SetStringField(TEXT("header"), OutsideHeader);
        TamperedRoot->SetArrayField(TEXT("cppLinks"), {
            MakeShared<FJsonValueObject>(MaliciousLink)
        });
        FString TamperedJson;
        const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&TamperedJson);
        FJsonSerializer::Serialize(TamperedRoot.ToSharedRef(), Writer);
        TamperedJson += LINE_TERMINATOR;
        TestTrue(TEXT("Tampered semantic fixture writes"), FFileHelper::SaveStringToFile(
            TamperedJson, *Export.OutputFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        FString RejectedBundle;
        Error.Reset();
        TestFalse(TEXT("Bundle rejects cppLinks outside approved source roots"),
            FUERingBundleBuilder::Build(PackageName, RejectedBundle, Error));
        TestTrue(TEXT("Rejected cppLinks reports its source boundary"),
            Error.Contains(TEXT("approved C/C++ roots")));
        TestTrue(TEXT("Original semantic is restored after boundary test"),
            FFileHelper::SaveStringToFile(
                OriginalSemantic,
                *Export.OutputFile,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    }
    IFileManager::Get().Delete(*OutsideHeader, false, true);

    const FString StaleMarker = FPaths::Combine(BundleDirectory, TEXT("stale-marker.txt"));
    FFileHelper::SaveStringToFile(TEXT("stale"), *StaleMarker);
    Error.Reset();
    TestTrue(TEXT("Bundle rebuild succeeds"), FUERingBundleBuilder::Build(PackageName, BundleDirectory, Error));
    TestFalse(TEXT("Bundle rebuild removes stale files"), IFileManager::Get().FileExists(*StaleMarker));

    FUERingBundleRequest FolderRequest;
    FolderRequest.Scope = EUERingBundleScope::Folder;
    FolderRequest.Value = BundleFolder;
    TArray<FString> FolderPreview;
    Error.Reset();
    TestTrue(TEXT("Folder bundle preview succeeds"),
        FUERingBundleBuilder::Preview(FolderRequest, FolderPreview, Error));
    int32 SelectedPreviewFiles = 0;
    for (const FString& PreviewFile : FolderPreview)
    {
        SelectedPreviewFiles += PreviewFile.StartsWith(TEXT("assets/selected/")) ? 1 : 0;
    }
    TestEqual(TEXT("Folder preview contains both selected roots"), SelectedPreviewFiles, 2);

    FString SecondOriginalSemantic;
    TestTrue(TEXT("Second semantic reads before context budget test"),
        FFileHelper::LoadFileToString(SecondOriginalSemantic, *SecondExport.OutputFile));
    TSharedPtr<FJsonObject> BudgetPrimaryRoot;
    TestTrue(TEXT("Primary semantic parses before collection deduplication test"),
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(OriginalSemantic), BudgetPrimaryRoot)
            && BudgetPrimaryRoot.IsValid());
    FString BudgetPrimarySemantic = OriginalSemantic;
    if (BudgetPrimaryRoot.IsValid())
    {
        const TSharedPtr<FJsonObject>* ExistingDependencies = nullptr;
        TSharedRef<FJsonObject> BudgetDependencies = MakeShared<FJsonObject>();
        if (BudgetPrimaryRoot->TryGetObjectField(TEXT("dependencies"), ExistingDependencies)
            && ExistingDependencies != nullptr)
        {
            BudgetDependencies = (*ExistingDependencies).ToSharedRef();
        }
        BudgetDependencies->SetArrayField(TEXT("hard"), {
            MakeShared<FJsonValueString>(SecondPackageName)
        });
        BudgetPrimaryRoot->SetObjectField(TEXT("dependencies"), BudgetDependencies);
        BudgetPrimarySemantic.Reset();
        const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> BudgetWriter =
            TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&BudgetPrimarySemantic);
        FJsonSerializer::Serialize(BudgetPrimaryRoot.ToSharedRef(), BudgetWriter);
        BudgetPrimarySemantic += LINE_TERMINATOR;
    }
    const FString ContextPadding = FString::ChrN(700 * 1024, TEXT(' '));
    TestTrue(TEXT("Primary semantic is padded for context budget test"),
        FFileHelper::SaveStringToFile(
            BudgetPrimarySemantic + ContextPadding,
            *Export.OutputFile,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    TestTrue(TEXT("Second semantic is padded for context budget test"),
        FFileHelper::SaveStringToFile(
            SecondOriginalSemantic + ContextPadding,
            *SecondExport.OutputFile,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    UUERingSettings* MutableSettings = GetMutableDefault<UUERingSettings>();
    const int32 PreviousBundleBudget = MutableSettings->MaxBundleContextMiB;
    MutableSettings->MaxBundleContextMiB = 1;

    FString FolderBundleDirectory;
    Error.Reset();
    TestTrue(TEXT("Folder bundle builds"),
        FUERingBundleBuilder::Build(FolderRequest, FolderBundleDirectory, Error));
    if (!Error.IsEmpty())
    {
        AddError(Error);
    }
    FString SafeSecondPackage = SecondPackageName;
    SafeSecondPackage.RemoveFromStart(TEXT("/"));
    SafeSecondPackage.ReplaceInline(TEXT("/"), TEXT("__"));
    TestFalse(TEXT("Collection bundle does not duplicate a root as a dependency"),
        IFileManager::Get().FileExists(*FPaths::Combine(
            FolderBundleDirectory,
            TEXT("assets/dependencies"),
            SafeSecondPackage + TEXT(".uesem.json"))));
    TSharedPtr<FJsonObject> FolderManifest;
    TestTrue(TEXT("Folder bundle manifest parses"), LoadJsonObject(
        FPaths::Combine(FolderBundleDirectory, TEXT("manifest.json")), FolderManifest));
    if (FolderManifest.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* RootAssets = nullptr;
        TestEqual(TEXT("Folder bundle manifest scope"),
            FolderManifest->GetStringField(TEXT("scope")), FString(TEXT("folder")));
        TestTrue(TEXT("Folder bundle manifest has roots"),
            FolderManifest->TryGetArrayField(TEXT("rootAssets"), RootAssets));
        if (RootAssets != nullptr)
        {
            TestEqual(TEXT("Folder bundle manifest has both roots"), RootAssets->Num(), 2);
        }
    }
    TSharedPtr<FJsonObject> FolderPlan;
    TestTrue(TEXT("Folder bundle context plan parses"), LoadJsonObject(
        FPaths::Combine(FolderBundleDirectory, TEXT("context-plan.json")), FolderPlan));
    if (FolderPlan.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
        TestTrue(TEXT("Folder context plan has files"), FolderPlan->TryGetArrayField(TEXT("files"), Files));
        int32 RootPriorityFiles = 0;
        if (Files != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Files)
            {
                const TSharedPtr<FJsonObject> File = Value.IsValid() ? Value->AsObject() : nullptr;
                RootPriorityFiles += File.IsValid()
                    && File->GetStringField(TEXT("priority")) == TEXT("root")
                    && File->GetBoolField(TEXT("includeInInitialContext")) ? 1 : 0;
            }
        }
        TestEqual(TEXT("Folder context budget defers the second large root"), RootPriorityFiles, 1);
        TestTrue(TEXT("Folder roots exceed the configured context budget"),
            FolderPlan->GetBoolField(TEXT("rootAssetsExceedBudget")));
        TestTrue(TEXT("Selected folder context remains within budget"),
            FolderPlan->GetNumberField(TEXT("selectedBytes")) <= 1024.0 * 1024.0);
        TestEqual(TEXT("Folder context records one deferred root"),
            static_cast<int32>(FolderPlan->GetNumberField(TEXT("deferredRootFiles"))), 1);
    }

    MutableSettings->MaxBundleContextMiB = PreviousBundleBudget;
    FFileHelper::SaveStringToFile(
        OriginalSemantic,
        *Export.OutputFile,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FFileHelper::SaveStringToFile(
        SecondOriginalSemantic,
        *SecondExport.OutputFile,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    IFileManager::Get().Delete(*ConfigFile, false, true);
    DeleteExportArtifacts(Export.OutputFile);
    DeleteExportArtifacts(SecondExport.OutputFile);
    IFileManager::Get().Delete(*SourceFile, false, true);
    IFileManager::Get().Delete(*SecondSourceFile, false, true);
    IFileManager::Get().DeleteDirectory(*BundleDirectory, false, true);
    IFileManager::Get().DeleteDirectory(*FolderBundleDirectory, false, true);
    IFileManager::Get().DeleteDirectory(*FPaths::GetPath(SourceFile), false, false);
    IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Export.OutputFile), false, false);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUERingP0ValidationFailuresTest,
    "UERing.Exporter.P0.ValidationFailures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUERingP0ValidationFailuresTest::RunTest(const FString& Parameters)
{
    using namespace UERingP0Tests;
    const FString TestContentDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UERingTests"));
    auto CleanupAbandonedValidationFixtures = [&TestContentDirectory]()
    {
        IFileManager& FileManager = IFileManager::Get();
        for (const TCHAR* Pattern : { TEXT("BP_Stale_*.uasset"), TEXT("DA_MissingValidation_*.uasset") })
        {
            TArray<FString> Files;
            FileManager.FindFiles(Files, *FPaths::Combine(TestContentDirectory, Pattern), true, false);
            for (const FString& File : Files)
            {
                FileManager.Delete(*FPaths::Combine(TestContentDirectory, File), false, true);
            }
        }
        FileManager.DeleteDirectory(*TestContentDirectory, false, false);
    };
    CleanupAbandonedValidationFixtures();

    UUERingSettings* MutableSettings = GetMutableDefault<UUERingSettings>();
    const bool bPreviousAutoExport = MutableSettings->bEnableAutoExport;
    MutableSettings->bEnableAutoExport = false;
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);

    const FString StalePackageName = TEXT("/Game/UERingTests/BP_Stale_") + Suffix;
    UPackage* StalePackage = CreatePackage(*StalePackageName);
    UBlueprint* StaleBlueprint = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(), StalePackage, *FPackageName::GetLongPackageAssetName(StalePackageName),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(),
        TEXT("UERingP0ValidationFailuresTest"));
    FString StaleSource;
    TestTrue(TEXT("Stale test asset saves"), SaveAsset(StalePackage, StaleBlueprint, false, StaleSource));
    const FUERingExportResult StaleExport = FUERingExportManager::Get().ExportAsset(FAssetData(StaleBlueprint));
    TestTrue(TEXT("Stale test asset initially exports"), StaleExport.IsSuccess());

    FString Json;
    TSharedPtr<FJsonObject> Root;
    if (FFileHelper::LoadFileToString(Json, *StaleExport.OutputFile)
        && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
        && Root.IsValid())
    {
        const TSharedPtr<FJsonObject>* AssetObject = nullptr;
        if (Root->TryGetObjectField(TEXT("asset"), AssetObject) && AssetObject != nullptr)
        {
            (*AssetObject)->SetStringField(TEXT("sourceHash"), TEXT("sha256:0000000000000000000000000000000000000000000000000000000000000000"));
        }
        Json.Reset();
        const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
        FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
        Json += LINE_TERMINATOR;
        FFileHelper::SaveStringToFile(Json, *StaleExport.OutputFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    const FString MissingPackageName = TEXT("/Game/UERingTests/DA_MissingValidation_") + Suffix;
    UPackage* MissingPackage = CreatePackage(*MissingPackageName);
    UPrimaryAssetLabel* MissingAsset = NewObject<UPrimaryAssetLabel>(
        MissingPackage, *FPackageName::GetLongPackageAssetName(MissingPackageName), RF_Public | RF_Standalone);
    FString MissingSource;
    TestTrue(TEXT("Missing semantic test asset saves"), SaveAsset(MissingPackage, MissingAsset, false, MissingSource));

    const FString OrphanFile = FPaths::Combine(
        FUERingExportManager::Get().GetOutputRoot(),
        TEXT("content/Game/UERingTests/Orphan.uesem.json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(OrphanFile), true);
    FFileHelper::SaveStringToFile(TEXT("{}\n"), *OrphanFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Registry.ScanFilesSynchronous({ StaleSource, MissingSource }, true);
    const FUERingValidationReport Report = FUERingValidator::Validate();
    TestTrue(TEXT("Validator detects stale sidecars"), Report.Stale >= 1);
    TestTrue(TEXT("Validator detects missing sidecars"), Report.Missing >= 1);
    TestTrue(TEXT("Validator detects orphan sidecars"), Report.Orphan >= 1);
    TestFalse(TEXT("Validation fails when semantic state is inconsistent"), Report.IsValid());

    IFileManager::Get().Delete(*StaleExport.OutputFile, false, true);
    IFileManager::Get().Delete(*FPaths::ChangeExtension(StaleExport.OutputFile, TEXT("md")), false, true);
    IFileManager::Get().Delete(*OrphanFile, false, true);
    IFileManager::Get().Delete(*StaleSource, false, true);
    IFileManager::Get().Delete(*MissingSource, false, true);
    CleanupAbandonedValidationFixtures();
    MutableSettings->bEnableAutoExport = bPreviousAutoExport;
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUERingP0LifecycleTest,
    "UERing.Exporter.P0.Lifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUERingP0LifecycleTest::RunTest(const FString& Parameters)
{
    using namespace UERingP0Tests;
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString PackageName = TEXT("/Game/UERingTests/DA_Auto_") + Suffix;
    UPackage* Package = CreatePackage(*PackageName);
    UPrimaryAssetLabel* Asset = NewObject<UPrimaryAssetLabel>(
        Package, *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone);
    UUERingSettings* MutableSettings = GetMutableDefault<UUERingSettings>();
    const bool bPreviousKeepTombstone = MutableSettings->bKeepTombstone;
    MutableSettings->bKeepTombstone = true;
    FString SourceFile;
    if (!TestTrue(TEXT("Automatic export test asset saves"), SaveAsset(Package, Asset, false, SourceFile)))
    {
        MutableSettings->bKeepTombstone = bPreviousKeepTombstone;
        return false;
    }
    const FString SemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(PackageName, false);

    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
        [this, SourceFile, SemanticFile, Asset, PackageName, bPreviousKeepTombstone]()
        {
            TestTrue(TEXT("Saving a supported asset automatically creates its sidecar"),
                IFileManager::Get().FileExists(*SemanticFile));
            const FString OldEnglishSummary = FPaths::ChangeExtension(SemanticFile, TEXT("md"));
            const FString OldChineseSummary = FPaths::ChangeExtension(SemanticFile, TEXT("zh-CN.md"));
            TestTrue(TEXT("Automatic export creates the English summary"),
                IFileManager::Get().FileExists(*OldEnglishSummary));
            TestTrue(TEXT("Automatic export creates the Chinese summary"),
                IFileManager::Get().FileExists(*OldChineseSummary));

            FString OldMermaid;
            FString OldGraphviz;
            FUERingDerivedArtifactWriter::GetGraphArtifactFiles(SemanticFile, OldMermaid, OldGraphviz);
            const FString OldDiff = FUERingDerivedArtifactWriter::GetChangeSummaryFile(SemanticFile);
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(OldMermaid), true);
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(OldDiff), true);
            TestTrue(TEXT("Lifecycle graph fixture writes"),
                FFileHelper::SaveStringToFile(TEXT("flowchart TD\n"), *OldMermaid));
            TestTrue(TEXT("Lifecycle Graphviz fixture writes"),
                FFileHelper::SaveStringToFile(TEXT("digraph UERing {}\n"), *OldGraphviz));
            TestTrue(TEXT("Lifecycle diff fixture writes"),
                FFileHelper::SaveStringToFile(TEXT("{}\n"), *OldDiff));

            IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
            const FString RenamedPackage = Asset->GetOutermost()->GetName() + TEXT("_Renamed");
            UPackage* RenamedOuter = CreatePackage(*RenamedPackage);
            UPrimaryAssetLabel* RenamedAsset = NewObject<UPrimaryAssetLabel>(
                RenamedOuter, *FPackageName::GetLongPackageAssetName(RenamedPackage), RF_Public | RF_Standalone);
            FString RenamedSource;
            SaveAsset(RenamedOuter, RenamedAsset, false, RenamedSource);
            const FAssetData RenamedData(RenamedAsset);
            const FString OldObjectPath = FAssetData(Asset).GetSoftObjectPath().ToString();
            const FString RenamedSemantic = FUERingExportManager::Get().GetSemanticFileForPackage(RenamedPackage, false);
            FString RenamedMermaid;
            FString RenamedGraphviz;
            FUERingDerivedArtifactWriter::GetGraphArtifactFiles(
                RenamedSemantic, RenamedMermaid, RenamedGraphviz);
            const FString RenamedDiff = FUERingDerivedArtifactWriter::GetChangeSummaryFile(RenamedSemantic);
            const FString RenamedEnglishSummary = FPaths::ChangeExtension(RenamedSemantic, TEXT("md"));
            const FString RenamedChineseSummary = FPaths::ChangeExtension(RenamedSemantic, TEXT("zh-CN.md"));
            Registry.OnAssetRenamed().Broadcast(RenamedData, OldObjectPath);
            TestFalse(TEXT("Rename removes the old semantic path"), IFileManager::Get().FileExists(*SemanticFile));
            TestTrue(TEXT("Rename moves the semantic file to the new path"), IFileManager::Get().FileExists(*RenamedSemantic));
            TestFalse(TEXT("Rename removes the old Mermaid path"), IFileManager::Get().FileExists(*OldMermaid));
            TestTrue(TEXT("Rename moves the Mermaid artifact"), IFileManager::Get().FileExists(*RenamedMermaid));
            TestFalse(TEXT("Rename removes the old Graphviz path"), IFileManager::Get().FileExists(*OldGraphviz));
            TestTrue(TEXT("Rename moves the Graphviz artifact"), IFileManager::Get().FileExists(*RenamedGraphviz));
            TestFalse(TEXT("Rename removes the old diff path"), IFileManager::Get().FileExists(*OldDiff));
            TestTrue(TEXT("Rename moves the diff artifact"), IFileManager::Get().FileExists(*RenamedDiff));
            TestFalse(TEXT("Rename removes the old English summary"),
                IFileManager::Get().FileExists(*OldEnglishSummary));
            TestTrue(TEXT("Rename moves the English summary"),
                IFileManager::Get().FileExists(*RenamedEnglishSummary));
            TestFalse(TEXT("Rename removes the old Chinese summary"),
                IFileManager::Get().FileExists(*OldChineseSummary));
            TestTrue(TEXT("Rename moves the Chinese summary"),
                IFileManager::Get().FileExists(*RenamedChineseSummary));
            FString RelativeOldPackage = PackageName;
            RelativeOldPackage.RemoveFromStart(TEXT("/"));
            const FString RenameTombstone = FPaths::Combine(
                FUERingExportManager::Get().GetOutputRoot(),
                TEXT("tombstones"),
                RelativeOldPackage + TEXT(".deleted.json"));
            TSharedPtr<FJsonObject> Tombstone;
            TestTrue(TEXT("Rename tombstone parses"), LoadJsonObject(RenameTombstone, Tombstone));
            if (Tombstone.IsValid())
            {
                TestEqual(TEXT("Rename tombstone keeps the old package identity"),
                    Tombstone->GetStringField(TEXT("packageName")), PackageName);
                TestEqual(TEXT("Rename tombstone keeps the old object identity"),
                    Tombstone->GetStringField(TEXT("objectPath")), OldObjectPath);
                TestEqual(TEXT("Rename tombstone records the new package identity"),
                    Tombstone->GetStringField(TEXT("newPackageName")), RenamedPackage);
                TestEqual(TEXT("Rename tombstone records the new object identity"),
                    Tombstone->GetStringField(TEXT("newObjectPath")), RenamedData.GetSoftObjectPath().ToString());
            }

            const FUERingExportResult RenamedExport = FUERingExportManager::Get().ExportAsset(RenamedData);
            TestTrue(TEXT("Renamed asset re-exports with current identity"), RenamedExport.IsSuccess());
            TSharedPtr<FJsonObject> RenamedRoot;
            TestTrue(TEXT("Renamed semantic parses"), LoadJsonObject(RenamedSemantic, RenamedRoot));
            if (RenamedRoot.IsValid())
            {
                const TSharedPtr<FJsonObject>* AssetObject = nullptr;
                TestTrue(TEXT("Renamed semantic has asset identity"),
                    RenamedRoot->TryGetObjectField(TEXT("asset"), AssetObject));
                if (AssetObject != nullptr)
                {
                    TestEqual(TEXT("Renamed semantic contains the new package name"),
                        (*AssetObject)->GetStringField(TEXT("packageName")), RenamedPackage);
                }
            }
            TestFalse(TEXT("Graph-less re-export removes stale Mermaid output"),
                IFileManager::Get().FileExists(*RenamedMermaid));
            TestFalse(TEXT("Graph-less re-export removes stale Graphviz output"),
                IFileManager::Get().FileExists(*RenamedGraphviz));

            Registry.OnAssetRemoved().Broadcast(RenamedData);
            TestFalse(TEXT("Delete removes the renamed semantic file"), IFileManager::Get().FileExists(*RenamedSemantic));
            TestFalse(TEXT("Delete removes the renamed diff artifact"), IFileManager::Get().FileExists(*RenamedDiff));
            TestFalse(TEXT("Delete removes the renamed English summary"),
                IFileManager::Get().FileExists(*RenamedEnglishSummary));
            TestFalse(TEXT("Delete removes the renamed Chinese summary"),
                IFileManager::Get().FileExists(*RenamedChineseSummary));
            FString RelativeRenamedPackage = RenamedPackage;
            RelativeRenamedPackage.RemoveFromStart(TEXT("/"));
            IFileManager::Get().Delete(*RenameTombstone, false, true);
            IFileManager::Get().Delete(*FPaths::Combine(
                FUERingExportManager::Get().GetOutputRoot(),
                TEXT("tombstones"),
                RelativeRenamedPackage + TEXT(".deleted.json")), false, true);
            GetMutableDefault<UUERingSettings>()->bKeepTombstone = bPreviousKeepTombstone;
            IFileManager::Get().Delete(*SourceFile, false, true);
            IFileManager::Get().Delete(*RenamedSource, false, true);
        },
        1.5f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUERingP0BlueprintAutoUpdateTest,
    "UERing.Exporter.P0.BlueprintAutoUpdate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUERingP0BlueprintAutoUpdateTest::RunTest(const FString& Parameters)
{
    using namespace UERingP0Tests;
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString PackageName = TEXT("/Game/UERingTests/BP_AutoUpdate_") + Suffix;
    UPackage* Package = CreatePackage(*PackageName);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(), Package, *FPackageName::GetLongPackageAssetName(PackageName),
        BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(),
        TEXT("UERingP0BlueprintAutoUpdateTest"));
    FEdGraphPinType IntegerType;
    IntegerType.PinCategory = UEdGraphSchema_K2::PC_Int;
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("Health"), IntegerType, TEXT("100"));
    FString SourceFile;
    if (!TestTrue(TEXT("Auto-update Blueprint saves initially"), SaveAsset(Package, Blueprint, false, SourceFile)))
    {
        return false;
    }
    const FString SemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(PackageName, false);
    const TSharedRef<FString> FirstHash = MakeShared<FString>();

    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
        [this, Blueprint, SourceFile, SemanticFile, FirstHash]()
        {
            FString Json;
            TSharedPtr<FJsonObject> Root;
            TestTrue(TEXT("Initial Blueprint save auto-exports"),
                FFileHelper::LoadFileToString(Json, *SemanticFile)
                && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
                && Root.IsValid());
            if (Root.IsValid())
            {
                const TSharedPtr<FJsonObject>* AssetObject = nullptr;
                Root->TryGetObjectField(TEXT("asset"), AssetObject);
                if (AssetObject != nullptr)
                {
                    (*AssetObject)->TryGetStringField(TEXT("sourceHash"), *FirstHash);
                }
            }
            for (FBPVariableDescription& Variable : Blueprint->NewVariables)
            {
                if (Variable.VarName == TEXT("Health"))
                {
                    Variable.DefaultValue = TEXT("125");
                }
            }
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
            FString IgnoredFilename;
            SaveAsset(Blueprint->GetOutermost(), Blueprint, false, IgnoredFilename);
        },
        1.2f));

    ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
        [this, SourceFile, SemanticFile, FirstHash]()
        {
            FString Json;
            TSharedPtr<FJsonObject> Root;
            TestTrue(TEXT("Modified Blueprint save updates sidecar"),
                FFileHelper::LoadFileToString(Json, *SemanticFile)
                && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
                && Root.IsValid());
            if (Root.IsValid())
            {
                const TSharedPtr<FJsonObject>* AssetObject = nullptr;
                const TSharedPtr<FJsonObject>* Semantics = nullptr;
                Root->TryGetObjectField(TEXT("asset"), AssetObject);
                Root->TryGetObjectField(TEXT("semantics"), Semantics);
                FString UpdatedHash;
                if (AssetObject != nullptr)
                {
                    (*AssetObject)->TryGetStringField(TEXT("sourceHash"), UpdatedHash);
                }
                TestNotEqual(TEXT("Modified source produces a new hash"), UpdatedHash, *FirstHash);
                bool bFoundUpdatedDefault = false;
                if (Semantics != nullptr)
                {
                    const TArray<TSharedPtr<FJsonValue>>* Variables = nullptr;
                    if ((*Semantics)->TryGetArrayField(TEXT("variables"), Variables) && Variables != nullptr)
                    {
                        for (const TSharedPtr<FJsonValue>& Value : *Variables)
                        {
                            const TSharedPtr<FJsonObject>* Variable = nullptr;
                            if (Value->TryGetObject(Variable) && Variable != nullptr
                                && (*Variable)->GetStringField(TEXT("name")) == TEXT("Health")
                                && (*Variable)->GetStringField(TEXT("defaultValue")) == TEXT("125"))
                            {
                                bFoundUpdatedDefault = true;
                            }
                        }
                    }
                }
                TestTrue(TEXT("Updated variable default is exported"), bFoundUpdatedDefault);
            }
            IFileManager::Get().Delete(*SemanticFile, false, true);
            IFileManager::Get().Delete(*FPaths::ChangeExtension(SemanticFile, TEXT("md")), false, true);
            IFileManager::Get().Delete(*SourceFile, false, true);
        },
        1.2f));
    return true;
}

#endif
