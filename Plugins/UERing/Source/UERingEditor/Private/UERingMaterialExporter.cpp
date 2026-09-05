#include "UERingMaterialExporter.h"

#include "Dom/JsonValue.h"
#include "MaterialExpressionIO.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialParameterCollection.h"
#include "Shader/ShaderTypes.h"
#include "UERingDomainGraphExporter.h"
#include "UERingPropertySerializer.h"
#include "UERingSemanticUtils.h"
#include "UObject/UnrealType.h"

namespace UERingMaterialExporter
{
    FString StringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
    {
        FString Result;
        if (Object.IsValid()) Object->TryGetStringField(Name, Result);
        return Result;
    }

    TSharedPtr<FJsonValue> CompactValue(const TSharedPtr<FJsonValue>& Value)
    {
        if (!Value.IsValid()) return MakeShared<FJsonValueNull>();
        if (Value->Type == EJson::Array)
        {
            TArray<TSharedPtr<FJsonValue>> Values;
            Values.Reserve(Value->AsArray().Num());
            for (const TSharedPtr<FJsonValue>& Child : Value->AsArray())
            {
                Values.Add(CompactValue(Child));
            }
            return MakeShared<FJsonValueArray>(Values);
        }
        if (Value->Type != EJson::Object)
        {
            return Value;
        }

        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        if (!Object.IsValid()) return MakeShared<FJsonValueNull>();
        FString StructType;
        const TSharedPtr<FJsonObject>* Fields = nullptr;
        if (Object->TryGetStringField(TEXT("structType"), StructType)
            && Object->TryGetObjectField(TEXT("fields"), Fields)
            && Fields != nullptr)
        {
            if (StructType.EndsWith(TEXT(".Guid")))
            {
                double A = 0.0;
                double B = 0.0;
                double C = 0.0;
                double D = 0.0;
                auto GuidComponent = [](const double Value, uint32& OutValue)
                {
                    if (!FMath::IsFinite(Value)
                        || Value < static_cast<double>(MIN_int32)
                        || Value > static_cast<double>(MAX_uint32)
                        || FMath::TruncToDouble(Value) != Value)
                    {
                        return false;
                    }
                    OutValue = static_cast<uint32>(static_cast<int64>(Value));
                    return true;
                };
                uint32 GuidA = 0;
                uint32 GuidB = 0;
                uint32 GuidC = 0;
                uint32 GuidD = 0;
                if ((*Fields)->TryGetNumberField(TEXT("A"), A)
                    && (*Fields)->TryGetNumberField(TEXT("B"), B)
                    && (*Fields)->TryGetNumberField(TEXT("C"), C)
                    && (*Fields)->TryGetNumberField(TEXT("D"), D)
                    && GuidComponent(A, GuidA)
                    && GuidComponent(B, GuidB)
                    && GuidComponent(C, GuidC)
                    && GuidComponent(D, GuidD))
                {
                    const FGuid Guid(GuidA, GuidB, GuidC, GuidD);
                    return MakeShared<FJsonValueString>(
                        Guid.ToString(EGuidFormats::DigitsWithHyphensLower));
                }
            }
            return CompactValue(MakeShared<FJsonValueObject>(*Fields));
        }

        FString EnumName;
        if (Object->HasField(TEXT("enum"))
            && Object->TryGetStringField(TEXT("name"), EnumName))
        {
            return MakeShared<FJsonValueString>(EnumName);
        }

        TArray<FString> Keys;
        for (const auto& Pair : Object->Values)
        {
            Keys.Add(FString(Pair.Key));
        }
        Keys.Sort();
        const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        for (const FString& Key : Keys)
        {
            Result->SetField(Key, CompactValue(Object->TryGetField(Key)));
        }
        return MakeShared<FJsonValueObject>(Result);
    }

    TSharedRef<FJsonObject> CompactProperties(
        const TArray<TSharedPtr<FJsonValue>>& Properties,
        const TSet<FName>& Skipped = {})
    {
        const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        for (const TSharedPtr<FJsonValue>& Value : Properties)
        {
            const TSharedPtr<FJsonObject> Property = Value.IsValid() ? Value->AsObject() : nullptr;
            FString Name;
            if (!Property.IsValid()
                || !Property->TryGetStringField(TEXT("name"), Name)
                || Skipped.Contains(FName(*Name)))
            {
                continue;
            }
            Result->SetField(Name, CompactValue(Property->TryGetField(TEXT("value"))));
        }
        return Result;
    }

    TSet<FName> RootSkippedProperties()
    {
        return {
            TEXT("AssetImportData"), TEXT("AssetUserData"), TEXT("EditorOnlyData"),
            TEXT("EditorPitch"), TEXT("EditorX"), TEXT("EditorY"), TEXT("EditorYaw"),
            TEXT("EnumerationObjects"), TEXT("LayerParameterExpansion"), TEXT("LightingGuid"),
            TEXT("ParameterOverviewExpansion"), TEXT("ParameterStateId"), TEXT("PreviewMesh"),
            TEXT("ReferencedDefaultTextures"), TEXT("ReferencedTextureGuids"), TEXT("StateId"),
            TEXT("TextureStreamingData"), TEXT("TextureStreamingDataVersion"), TEXT("ThumbnailInfo"),
            TEXT("CombinedInputTypes"), TEXT("CombinedOutputTypes"),
            TEXT("DependentFunctionExpressionCandidates"), TEXT("LibraryCategoriesText")
        };
    }

    TSet<FName> ExpressionSkippedProperties()
    {
        return {
            TEXT("Desc"), TEXT("Function"), TEXT("Material"), TEXT("MaterialExpressionEditorX"),
            TEXT("MaterialExpressionEditorY"), TEXT("MaterialExpressionGuid"), TEXT("MenuCategories"),
            TEXT("Outputs"), TEXT("SubgraphExpression"), TEXT("bCollapsed"),
            TEXT("bCommentBubbleVisible"), TEXT("bHidePreviewWindow"), TEXT("bIsParameterExpression"),
            TEXT("bLastPreviewed"), TEXT("bNeedToUpdatePreview"), TEXT("bRealtimePreview"),
            TEXT("bShaderInputData"), TEXT("bShowInputs"), TEXT("bShowMaskColorsOnPin"),
            TEXT("bShowOutputNameOnPin"), TEXT("bShowOutputs")
        };
    }

    FString ExpressionId(const UMaterialExpression& Expression)
    {
        const FGuid& Guid = const_cast<UMaterialExpression&>(Expression).GetMaterialExpressionId();
        if (Guid.IsValid())
        {
            return Guid.ToString(EGuidFormats::DigitsWithHyphensLower);
        }
        return Expression.GetName();
    }

    TMap<const UMaterialExpression*, FString> BuildNodeIds(
        TArray<UMaterialExpression*>& Expressions)
    {
        Expressions.RemoveAll([](const UMaterialExpression* Expression) { return Expression == nullptr; });
        Expressions.Sort([](const UMaterialExpression& Left, const UMaterialExpression& Right)
        {
            const FString LeftBase = ExpressionId(Left);
            const FString RightBase = ExpressionId(Right);
            if (LeftBase != RightBase) return LeftBase < RightBase;
            return Left.GetName() < Right.GetName();
        });
        TMap<const UMaterialExpression*, FString> Result;
        TSet<FString> UsedIds;
        for (UMaterialExpression* Expression : Expressions)
        {
            const FString BaseId = ExpressionId(*Expression);
            FString Id = BaseId;
            if (UsedIds.Contains(Id))
            {
                Id = BaseId + TEXT("~") + Expression->GetName();
                int32 Suffix = 2;
                while (UsedIds.Contains(Id))
                {
                    Id = BaseId + TEXT("~") + Expression->GetName()
                        + TEXT("~") + FString::FromInt(Suffix++);
                }
            }
            UsedIds.Add(Id);
            Result.Add(Expression, MoveTemp(Id));
        }
        return Result;
    }

    FString InputName(const UMaterialExpression& Expression, const int32 Index)
    {
        FString Name = Expression.GetInputName(Index).ToString();
        if (const FExpressionInput* Input = Expression.GetInput(Index))
        {
            if ((Name.IsEmpty() || Name == TEXT("None")) && !Input->InputName.IsNone())
            {
                Name = Input->InputName.ToString();
            }
        }
        if (Name.IsEmpty() || Name == TEXT("None"))
        {
            Name = FString::Printf(TEXT("Input%d"), Index);
        }
        return Name;
    }

    FString ChannelMask(const FExpressionInput& Input)
    {
        if (Input.Mask == 0) return FString();
        FString Mask;
        if (Input.MaskR != 0) Mask += TEXT("r");
        if (Input.MaskG != 0) Mask += TEXT("g");
        if (Input.MaskB != 0) Mask += TEXT("b");
        if (Input.MaskA != 0) Mask += TEXT("a");
        return Mask.IsEmpty() ? TEXT("none") : Mask;
    }

    TSharedPtr<FJsonValue> MaterialConstant(const FMaterialInputDescription& Description)
    {
        using namespace UE::Shader;
        const int32 Type = static_cast<int32>(Description.Type);
        int32 Count = 0;
        enum class EComponentKind : uint8 { Float, Double, Integer, Boolean };
        EComponentKind Kind = EComponentKind::Float;
        if (Type >= static_cast<int32>(EValueType::Float1)
            && Type <= static_cast<int32>(EValueType::Float4))
        {
            Count = Type - static_cast<int32>(EValueType::Float1) + 1;
        }
        else if (Type >= static_cast<int32>(EValueType::Double1)
            && Type <= static_cast<int32>(EValueType::Double4))
        {
            Kind = EComponentKind::Double;
            Count = Type - static_cast<int32>(EValueType::Double1) + 1;
        }
        else if (Type >= static_cast<int32>(EValueType::Int1)
            && Type <= static_cast<int32>(EValueType::Int4))
        {
            Kind = EComponentKind::Integer;
            Count = Type - static_cast<int32>(EValueType::Int1) + 1;
        }
        else if (Type >= static_cast<int32>(EValueType::Bool1)
            && Type <= static_cast<int32>(EValueType::Bool4))
        {
            Kind = EComponentKind::Boolean;
            Count = Type - static_cast<int32>(EValueType::Bool1) + 1;
        }
        if (Count <= 0 || Description.ConstantValue.Component.Num() < Count)
        {
            return MakeShared<FJsonValueNull>();
        }
        auto ComponentValue = [&Description, Kind](const int32 Index) -> TSharedPtr<FJsonValue>
        {
            const FValueComponent& Component = Description.ConstantValue.Component[Index];
            if (Kind == EComponentKind::Boolean)
            {
                return MakeShared<FJsonValueBoolean>(Component.AsBool());
            }
            if (Kind == EComponentKind::Integer)
            {
                return MakeShared<FJsonValueNumber>(Component.Int);
            }
            return MakeShared<FJsonValueNumber>(
                Kind == EComponentKind::Double ? Component.Double : Component.Float);
        };
        if (Count == 1) return ComponentValue(0);
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 Index = 0; Index < Count; ++Index) Values.Add(ComponentValue(Index));
        return MakeShared<FJsonValueArray>(Values);
    }

    FString OutputName(const UMaterialExpression& Expression, const int32 OutputIndex)
    {
        const TArray<FExpressionOutput>& Outputs = const_cast<UMaterialExpression&>(Expression).GetOutputs();
        if (!Outputs.IsValidIndex(OutputIndex) || Outputs[OutputIndex].OutputName.IsNone())
        {
            return FString();
        }
        return Outputs[OutputIndex].OutputName.ToString();
    }

    void AddConnection(
        const FExpressionInput& Input,
        const FString& TargetNode,
        const FString& TargetInput,
        const TMap<const UMaterialExpression*, FString>& NodeIds,
        TArray<TSharedPtr<FJsonValue>>& Connections,
        int32& DanglingConnectionCount)
    {
        if (Input.Expression == nullptr) return;
        const FString* SourceNode = NodeIds.Find(Input.Expression);
        if (SourceNode == nullptr)
        {
            ++DanglingConnectionCount;
            return;
        }
        const TSharedRef<FJsonObject> Connection = MakeShared<FJsonObject>();
        Connection->SetStringField(TEXT("sourceNode"), *SourceNode);
        Connection->SetNumberField(TEXT("sourceOutputIndex"), Input.OutputIndex);
        const FString SourceOutputName = OutputName(*Input.Expression, Input.OutputIndex);
        if (!SourceOutputName.IsEmpty())
        {
            Connection->SetStringField(TEXT("sourceOutputName"), SourceOutputName);
        }
        const FString Mask = ChannelMask(Input);
        if (!Mask.IsEmpty()) Connection->SetStringField(TEXT("channelMask"), Mask);
        Connection->SetStringField(TEXT("targetNode"), TargetNode);
        Connection->SetStringField(TEXT("targetInput"), TargetInput);
        Connections.Add(MakeShared<FJsonValueObject>(Connection));
    }

    TArray<TSharedPtr<FJsonValue>> InstanceProperties(const UObject& Instance)
    {
        return UERingPropertySerializer::SerializeNamedObjectProperties(Instance, {
            TEXT("Parent"), TEXT("ScalarParameterValues"), TEXT("VectorParameterValues"),
            TEXT("DoubleVectorParameterValues"), TEXT("TextureParameterValues"),
            TEXT("TextureCollectionParameterValues"), TEXT("ParameterCollectionParameterValues"),
            TEXT("RuntimeVirtualTextureParameterValues"), TEXT("SparseVolumeTextureParameterValues"),
            TEXT("FontParameterValues"), TEXT("UserSceneTextureOverrides"),
            TEXT("StaticParametersRuntime"), TEXT("BasePropertyOverrides"), TEXT("LightmassSettings"),
            TEXT("NaniteOverrideMaterial"), TEXT("PhysMaterial"), TEXT("PhysMaterialMask"),
            TEXT("PhysicalMaterialMap"), TEXT("SubsurfaceProfile"), TEXT("SpecularProfileOverride"),
            TEXT("ToonProfileOverride"), TEXT("NeuralProfile"), TEXT("BlendableLocationOverride"),
            TEXT("BlendablePriorityOverride"), TEXT("bOverrideBlendableLocation"),
            TEXT("bOverrideBlendablePriority"), TEXT("bOverridePhysMaterial"),
            TEXT("bOverrideSpecularProfile"), TEXT("bOverrideSubsurfaceProfile"),
            TEXT("bOverrideToonProfile")
        });
    }

    TArray<TSharedPtr<FJsonValue>> FunctionInstanceProperties(const UObject& Instance)
    {
        return UERingPropertySerializer::SerializeNamedObjectProperties(Instance, {
            TEXT("Parent"), TEXT("Base"), TEXT("ScalarParameterValues"),
            TEXT("VectorParameterValues"), TEXT("DoubleVectorParameterValues"),
            TEXT("TextureParameterValues"), TEXT("TextureCollectionParameterValues"),
            TEXT("ParameterCollectionParameterValues"), TEXT("FontParameterValues"),
            TEXT("StaticSwitchParameterValues"), TEXT("StaticComponentMaskParameterValues"),
            TEXT("RuntimeVirtualTextureParameterValues"), TEXT("SparseVolumeTextureParameterValues")
        });
    }

    void AddRegenerableCacheOmission(
        FUERingSemanticPayload& Payload,
        const FUERingExportContext& Context)
    {
        UERingSemanticUtils::AddOmission(
            Payload,
            TEXT("/semantics/derivedCaches"),
            TEXT("regenerableDerivedData"),
            TEXT("Shader, texture streaming, reference, state, and preview caches are regenerated by Unreal Engine."),
            TEXT("compiledCachesRegenerated"),
            0,
            Context.SourceHash);
    }

    void CopyFields(
        const TSharedPtr<FJsonObject>& Source,
        const TSharedRef<FJsonObject>& Target,
        const TArray<FName>& Names)
    {
        if (!Source.IsValid()) return;
        for (const FName Name : Names)
        {
            if (const TSharedPtr<FJsonValue> Value = Source->TryGetField(Name.ToString()))
            {
                Target->SetField(Name.ToString(), Value);
            }
        }
    }

    void SortConnections(TArray<TSharedPtr<FJsonValue>>& Connections)
    {
        Connections.Sort([](const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right)
        {
            const TSharedPtr<FJsonObject> LeftObject = Left.IsValid() ? Left->AsObject() : nullptr;
            const TSharedPtr<FJsonObject> RightObject = Right.IsValid() ? Right->AsObject() : nullptr;
            const FString LeftKey = StringField(LeftObject, TEXT("targetNode")) + TEXT("|")
                + StringField(LeftObject, TEXT("targetInput")) + TEXT("|")
                + StringField(LeftObject, TEXT("sourceNode"));
            const FString RightKey = StringField(RightObject, TEXT("targetNode")) + TEXT("|")
                + StringField(RightObject, TEXT("targetInput")) + TEXT("|")
                + StringField(RightObject, TEXT("sourceNode"));
            if (LeftKey != RightKey) return LeftKey < RightKey;
            return LeftObject->GetNumberField(TEXT("sourceOutputIndex"))
                < RightObject->GetNumberField(TEXT("sourceOutputIndex"));
        });
    }

    void BuildExpressionGraph(
        const TArray<UMaterialExpression*>& SourceExpressions,
        const TSharedRef<FJsonObject>& Semantics,
        TArray<TSharedPtr<FJsonValue>>& Connections,
        int32& DanglingConnectionCount,
        TMap<const UMaterialExpression*, FString>& OutNodeIds)
    {
        TArray<UMaterialExpression*> Expressions = SourceExpressions;
        OutNodeIds = BuildNodeIds(Expressions);

        TArray<TSharedPtr<FJsonValue>> Nodes;
        TArray<TSharedPtr<FJsonValue>> InterfaceInputs;
        TArray<TSharedPtr<FJsonValue>> InterfaceOutputs;
        for (UMaterialExpression* Expression : Expressions)
        {
            const FString NodeId = OutNodeIds.FindChecked(Expression);
            const TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
            Node->SetStringField(TEXT("id"), NodeId);
            Node->SetStringField(TEXT("name"), Expression->GetName());
            Node->SetStringField(TEXT("class"), Expression->GetClass()->GetPathName());

            TSet<FName> Skipped = ExpressionSkippedProperties();
            const UObject* Baseline = Expression->GetClass()->GetDefaultObject();
            const TSharedRef<FJsonObject> Configuration = CompactProperties(
                UERingPropertySerializer::SerializeObjectProperties(*Expression, Baseline, true),
                Skipped);
            if (!Configuration->Values.IsEmpty())
            {
                Node->SetObjectField(TEXT("configuration"), Configuration);
            }

            TArray<TSharedPtr<FJsonValue>> Inputs;
            for (int32 InputIndex = 0; InputIndex < Expression->CountInputs(); ++InputIndex)
            {
                const FExpressionInput* Input = Expression->GetInput(InputIndex);
                if (Input == nullptr) continue;
                const FString Name = InputName(*Expression, InputIndex);
                const TSharedRef<FJsonObject> InputObject = MakeShared<FJsonObject>();
                InputObject->SetStringField(TEXT("name"), Name);
                InputObject->SetBoolField(TEXT("required"), Expression->IsInputConnectionRequired(InputIndex));
                InputObject->SetNumberField(
                    TEXT("valueType"),
                    static_cast<double>(static_cast<uint64>(Expression->GetInputValueType(InputIndex))));
                Inputs.Add(MakeShared<FJsonValueObject>(InputObject));
                AddConnection(*Input, NodeId, Name, OutNodeIds, Connections, DanglingConnectionCount);
            }
            if (!Inputs.IsEmpty()) Node->SetArrayField(TEXT("inputs"), Inputs);

            TArray<TSharedPtr<FJsonValue>> Outputs;
            const TArray<FExpressionOutput>& ExpressionOutputs = Expression->GetOutputs();
            for (int32 OutputIndex = 0; OutputIndex < ExpressionOutputs.Num(); ++OutputIndex)
            {
                const FExpressionOutput& Output = ExpressionOutputs[OutputIndex];
                const TSharedRef<FJsonObject> OutputObject = MakeShared<FJsonObject>();
                OutputObject->SetNumberField(TEXT("index"), OutputIndex);
                if (!Output.OutputName.IsNone())
                {
                    OutputObject->SetStringField(TEXT("name"), Output.OutputName.ToString());
                }
                OutputObject->SetNumberField(
                    TEXT("valueType"),
                    static_cast<double>(static_cast<uint64>(Expression->GetOutputValueType(OutputIndex))));
                Outputs.Add(MakeShared<FJsonValueObject>(OutputObject));
            }
            if (!Outputs.IsEmpty()) Node->SetArrayField(TEXT("outputs"), Outputs);

            const FString ClassName = Expression->GetClass()->GetName();
            if (ClassName == TEXT("MaterialExpressionFunctionInput"))
            {
                const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
                Entry->SetStringField(TEXT("node"), NodeId);
                CopyFields(Configuration, Entry, {
                    TEXT("Id"), TEXT("InputName"), TEXT("InputType"), TEXT("SortPriority"),
                    TEXT("PreviewValue"), TEXT("bUsePreviewValueAsDefault"), TEXT("BlendInputRelevance")
                });
                InterfaceInputs.Add(MakeShared<FJsonValueObject>(Entry));
            }
            else if (ClassName == TEXT("MaterialExpressionFunctionOutput"))
            {
                const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
                Entry->SetStringField(TEXT("node"), NodeId);
                CopyFields(Configuration, Entry, {
                    TEXT("Id"), TEXT("OutputName"), TEXT("SortPriority")
                });
                InterfaceOutputs.Add(MakeShared<FJsonValueObject>(Entry));
            }
            Nodes.Add(MakeShared<FJsonValueObject>(Node));
        }
        Semantics->SetNumberField(TEXT("nodeCount"), Nodes.Num());
        Semantics->SetArrayField(TEXT("nodes"), Nodes);
        if (!InterfaceInputs.IsEmpty() || !InterfaceOutputs.IsEmpty())
        {
            const TSharedRef<FJsonObject> Interface = MakeShared<FJsonObject>();
            if (!InterfaceInputs.IsEmpty()) Interface->SetArrayField(TEXT("inputs"), InterfaceInputs);
            if (!InterfaceOutputs.IsEmpty()) Interface->SetArrayField(TEXT("outputs"), InterfaceOutputs);
            Semantics->SetObjectField(TEXT("interface"), Interface);
        }
    }
}

FName FUERingMaterialExporter::GetName() const
{
    return TEXT("MaterialLogic");
}

bool FUERingMaterialExporter::CanExport(const FAssetData& AssetData) const
{
    const FName ClassName = AssetData.AssetClassPath.GetAssetName();
    return ClassName == TEXT("Material")
        || ClassName == TEXT("MaterialFunction")
        || ClassName == TEXT("MaterialFunctionInstance")
        || ClassName == TEXT("MaterialFunctionMaterialLayer")
        || ClassName == TEXT("MaterialFunctionMaterialLayerInstance")
        || ClassName == TEXT("MaterialFunctionMaterialLayerBlend")
        || ClassName == TEXT("MaterialFunctionMaterialLayerBlendInstance")
        || ClassName == TEXT("MaterialInstanceConstant")
        || ClassName == TEXT("MaterialParameterCollection");
}

bool FUERingMaterialExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    UObject* Asset = Context.Asset.Get();
    if (Asset == nullptr)
    {
        OutError = TEXT("The material asset could not be loaded.");
        return false;
    }
    if (Context.Profile != EUERingExportProfile::Logic)
    {
        FUERingDomainGraphExporter FullExporter;
        return FullExporter.BuildPayload(Context, OutPayload, OutError);
    }

    using namespace UERingMaterialExporter;
    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("MaterialLogic"));
    Semantics->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetPathName());

    if (UMaterialParameterCollection* Collection = Cast<UMaterialParameterCollection>(Asset))
    {
        Semantics->SetStringField(TEXT("role"), TEXT("parameterCollection"));
        Semantics->SetStringField(TEXT("representation"), TEXT("material-parameter-collection-v1"));
        const TSharedRef<FJsonObject> Parameters = CompactProperties(
            UERingPropertySerializer::SerializeNamedObjectProperties(
                *Collection,
                { TEXT("ScalarParameters"), TEXT("VectorParameters") }));
        Semantics->SetObjectField(TEXT("parameters"), Parameters);
        OutPayload.Semantics = Semantics;
        AddRegenerableCacheOmission(OutPayload, Context);
        return true;
    }

    if (UMaterialInstance* Instance = Cast<UMaterialInstance>(Asset))
    {
        Semantics->SetStringField(TEXT("role"), TEXT("instance"));
        Semantics->SetStringField(TEXT("representation"), TEXT("material-instance-v1"));
        TSharedRef<FJsonObject> Properties = CompactProperties(InstanceProperties(*Instance));
        if (const TSharedPtr<FJsonValue> Parent = Properties->TryGetField(TEXT("Parent")))
        {
            Semantics->SetField(TEXT("parent"), Parent);
            Properties->RemoveField(TEXT("Parent"));
        }
        if (const UMaterialInstanceEditorOnlyData* EditorData = Instance->GetEditorOnlyData())
        {
            const TSharedRef<FJsonObject> EditorProperties = CompactProperties(
                UERingPropertySerializer::SerializeNamedObjectProperties(
                    *EditorData,
                    { TEXT("StaticParameters") }));
            if (const TSharedPtr<FJsonValue> StaticParameters =
                    EditorProperties->TryGetField(TEXT("StaticParameters")))
            {
                Properties->SetField(TEXT("EditorStaticParameters"), StaticParameters);
            }
        }
        Semantics->SetObjectField(TEXT("overrides"), Properties);
        OutPayload.Semantics = Semantics;
        AddRegenerableCacheOmission(OutPayload, Context);
        return true;
    }

    if (UMaterialFunctionInstance* FunctionInstance = Cast<UMaterialFunctionInstance>(Asset))
    {
        Semantics->SetStringField(TEXT("role"), TEXT("functionInstance"));
        Semantics->SetStringField(TEXT("representation"), TEXT("material-function-instance-v1"));
        TSharedRef<FJsonObject> Properties = CompactProperties(
            FunctionInstanceProperties(*FunctionInstance));
        for (const TCHAR* IdentityField : { TEXT("Parent"), TEXT("Base") })
        {
            if (const TSharedPtr<FJsonValue> Value = Properties->TryGetField(IdentityField))
            {
                FString TargetName(IdentityField);
                TargetName.ToLowerInline();
                Semantics->SetField(TargetName, Value);
                Properties->RemoveField(IdentityField);
            }
        }
        Semantics->SetObjectField(TEXT("overrides"), Properties);
        OutPayload.Semantics = Semantics;
        AddRegenerableCacheOmission(OutPayload, Context);
        return true;
    }

    TArray<UMaterialExpression*> Expressions;
    TArray<TSharedPtr<FJsonValue>> Connections;
    TMap<const UMaterialExpression*, FString> NodeIds;
    int32 DanglingConnectionCount = 0;
    if (UMaterial* Material = Cast<UMaterial>(Asset))
    {
        Semantics->SetStringField(TEXT("role"), TEXT("material"));
        Semantics->SetStringField(TEXT("representation"), TEXT("material-expression-graph-v1"));
        Semantics->SetObjectField(
            TEXT("settings"),
            CompactProperties(
                UERingPropertySerializer::SerializeObjectProperties(*Material),
                RootSkippedProperties()));
        for (UMaterialExpression* Expression : Material->GetExpressions())
        {
            Expressions.Add(Expression);
        }
        BuildExpressionGraph(
            Expressions,
            Semantics,
            Connections,
            DanglingConnectionCount,
            NodeIds);
        TArray<TSharedPtr<FJsonValue>> Outputs;
        const UEnum* PropertyEnum = StaticEnum<EMaterialProperty>();
        for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
        {
            const EMaterialProperty Property = static_cast<EMaterialProperty>(PropertyIndex);
            FMaterialInputDescription Description;
            if (!Material->GetExpressionInputDescription(Property, Description)
                || Description.bHidden
                || Description.Input == nullptr)
            {
                continue;
            }
            const FString Name = PropertyEnum != nullptr
                ? PropertyEnum->GetNameStringByValue(PropertyIndex)
                : FString::Printf(TEXT("MP_%d"), PropertyIndex);
            const TSharedRef<FJsonObject> Output = MakeShared<FJsonObject>();
            Output->SetStringField(TEXT("name"), Name);
            Output->SetBoolField(TEXT("connected"), Description.Input->Expression != nullptr);
            Output->SetNumberField(TEXT("valueType"), static_cast<double>(Description.Type));
            if (Description.bUseConstant)
            {
                Output->SetField(TEXT("constant"), MaterialConstant(Description));
            }
            Outputs.Add(MakeShared<FJsonValueObject>(Output));
            AddConnection(
                *Description.Input,
                TEXT("$material"),
                Name,
                NodeIds,
                Connections,
                DanglingConnectionCount);
        }
        Semantics->SetArrayField(TEXT("materialInputs"), Outputs);
    }
    else if (UMaterialFunctionInterface* Function = Cast<UMaterialFunctionInterface>(Asset))
    {
        Semantics->SetStringField(TEXT("role"), TEXT("function"));
        Semantics->SetStringField(TEXT("representation"), TEXT("material-function-graph-v1"));
        Semantics->SetObjectField(
            TEXT("settings"),
            CompactProperties(
                UERingPropertySerializer::SerializeObjectProperties(*Function),
                RootSkippedProperties()));
        for (UMaterialExpression* Expression : Function->GetExpressions())
        {
            Expressions.Add(Expression);
        }
        BuildExpressionGraph(
            Expressions,
            Semantics,
            Connections,
            DanglingConnectionCount,
            NodeIds);
    }
    else
    {
        OutError = FString::Printf(TEXT("Unsupported material UObject class: %s"), *Asset->GetClass()->GetPathName());
        return false;
    }

    SortConnections(Connections);
    Semantics->SetNumberField(TEXT("connectionCount"), Connections.Num());
    Semantics->SetArrayField(TEXT("connections"), Connections);
    Semantics->SetNumberField(TEXT("danglingConnectionCount"), DanglingConnectionCount);
    OutPayload.Semantics = Semantics;
    UERingSemanticUtils::AddOmission(
        OutPayload,
        TEXT("/semantics/presentation"),
        TEXT("editorPresentation"),
        TEXT("Logic profile omits material node layout, comments, thumbnails, localized menus, and preview UI state."),
        TEXT("editorLayoutNotReconstructable"),
        Expressions.Num(),
        Context.SourceHash);
    AddRegenerableCacheOmission(OutPayload, Context);
    return true;
}
