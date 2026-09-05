#include "UERingMetaSoundExporter.h"

#include "UERingPropertySerializer.h"
#include "UERingSemanticUtils.h"

namespace UERingMetaSoundExporter
{
    const TSharedPtr<FJsonObject>* ObjectField(
        const TSharedRef<FJsonObject>& Object,
        const TCHAR* Name)
    {
        const TSharedPtr<FJsonObject>* Value = nullptr;
        return Object->TryGetObjectField(Name, Value) && Value != nullptr ? Value : nullptr;
    }

    TSharedPtr<FJsonObject> Fields(const TSharedPtr<FJsonObject>& Wrapper)
    {
        if (!Wrapper.IsValid()) return nullptr;
        const TSharedPtr<FJsonObject>* Value = nullptr;
        return Wrapper->TryGetObjectField(TEXT("fields"), Value) && Value != nullptr ? *Value : nullptr;
    }

    FString StringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
    {
        FString Value;
        if (Object.IsValid()) Object->TryGetStringField(Name, Value);
        return Value;
    }

    FString EnumName(const TSharedPtr<FJsonObject>& Object)
    {
        return StringField(Object, TEXT("name"));
    }

    FString GuidString(const TSharedPtr<FJsonObject>& Wrapper)
    {
        const TSharedPtr<FJsonObject> Value = Fields(Wrapper);
        if (!Value.IsValid()) return FString();
        double A = 0.0;
        double B = 0.0;
        double C = 0.0;
        double D = 0.0;
        if (!Value->TryGetNumberField(TEXT("A"), A)
            || !Value->TryGetNumberField(TEXT("B"), B)
            || !Value->TryGetNumberField(TEXT("C"), C)
            || !Value->TryGetNumberField(TEXT("D"), D))
        {
            return FString();
        }
        return FString::Printf(
            TEXT("%08x-%08x-%08x-%08x"),
            static_cast<uint32>(static_cast<int32>(A)),
            static_cast<uint32>(static_cast<int32>(B)),
            static_cast<uint32>(static_cast<int32>(C)),
            static_cast<uint32>(static_cast<int32>(D)));
    }

    TSharedPtr<FJsonObject> CompactLiteral(const TSharedPtr<FJsonObject>& Wrapper)
    {
        const TSharedPtr<FJsonObject> Literal = Fields(Wrapper);
        if (!Literal.IsValid()) return nullptr;
        const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        if (const TSharedPtr<FJsonObject>* Type = ObjectField(Literal.ToSharedRef(), TEXT("Type")))
        {
            Result->SetStringField(TEXT("type"), EnumName(*Type));
        }
        static const TPair<const TCHAR*, const TCHAR*> ValueFields[] = {
            { TEXT("AsBoolean"), TEXT("boolean") },
            { TEXT("AsFloat"), TEXT("float") },
            { TEXT("AsInteger"), TEXT("integer") },
            { TEXT("AsString"), TEXT("string") },
            { TEXT("AsUObject"), TEXT("object") }
        };
        for (const TPair<const TCHAR*, const TCHAR*>& Pair : ValueFields)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (Literal->TryGetArrayField(Pair.Key, Values) && Values != nullptr && !Values->IsEmpty())
            {
                Result->SetArrayField(Pair.Value, *Values);
            }
        }
        double NumDefault = 0.0;
        if (Literal->TryGetNumberField(TEXT("AsNumDefault"), NumDefault) && NumDefault != 0.0)
        {
            Result->SetNumberField(TEXT("numDefault"), NumDefault);
        }
        return Result;
    }

    TSharedRef<FJsonObject> CompactVertex(const TSharedPtr<FJsonObject>& Wrapper)
    {
        const TSharedPtr<FJsonObject> Vertex = Fields(Wrapper);
        const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        if (!Vertex.IsValid()) return Result;
        Result->SetStringField(TEXT("name"), StringField(Vertex, TEXT("Name")));
        Result->SetStringField(TEXT("type"), StringField(Vertex, TEXT("TypeName")));
        const FString VertexId = GuidString(Vertex->GetObjectField(TEXT("VertexID")));
        if (!VertexId.IsEmpty()) Result->SetStringField(TEXT("id"), VertexId);
        if (const TSharedPtr<FJsonObject>* Access = ObjectField(Vertex.ToSharedRef(), TEXT("AccessType")))
        {
            Result->SetStringField(TEXT("access"), EnumName(*Access));
        }
        if (const TSharedPtr<FJsonObject>* Default = ObjectField(Vertex.ToSharedRef(), TEXT("DefaultLiteral")))
        {
            if (const TSharedPtr<FJsonObject> Literal = CompactLiteral(*Default))
            {
                Result->SetObjectField(TEXT("default"), Literal);
            }
        }
        const TArray<TSharedPtr<FJsonValue>>* Defaults = nullptr;
        if (Vertex->TryGetArrayField(TEXT("Defaults"), Defaults) && Defaults != nullptr && !Defaults->IsEmpty())
        {
            const TSharedPtr<FJsonObject>* DefaultWrapper = nullptr;
            if ((*Defaults)[0].IsValid() && (*Defaults)[0]->TryGetObject(DefaultWrapper) && DefaultWrapper != nullptr)
            {
                const TSharedPtr<FJsonObject> DefaultFields = Fields(*DefaultWrapper);
                if (DefaultFields.IsValid())
                {
                    const TSharedPtr<FJsonObject>* LiteralWrapper = nullptr;
                    if (DefaultFields->TryGetObjectField(TEXT("Literal"), LiteralWrapper) && LiteralWrapper != nullptr)
                    {
                        if (const TSharedPtr<FJsonObject> Literal = CompactLiteral(*LiteralWrapper))
                        {
                            Result->SetObjectField(TEXT("default"), Literal);
                        }
                    }
                }
            }
        }
        return Result;
    }

    void AddVertices(
        const TSharedPtr<FJsonObject>& Interface,
        const TCHAR* SourceName,
        const TCHAR* TargetName,
        const TSharedRef<FJsonObject>& Target)
    {
        if (!Interface.IsValid()) return;
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Interface->TryGetArrayField(SourceName, Values) || Values == nullptr) return;
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Values->Num());
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const TSharedPtr<FJsonObject>* Wrapper = nullptr;
            if (Value.IsValid() && Value->TryGetObject(Wrapper) && Wrapper != nullptr)
            {
                Result.Add(MakeShared<FJsonValueObject>(CompactVertex(*Wrapper)));
            }
        }
        Target->SetArrayField(TargetName, Result);
    }

    TSharedRef<FJsonObject> CompactInterface(const TSharedPtr<FJsonObject>& Wrapper)
    {
        const TSharedPtr<FJsonObject> Interface = Fields(Wrapper);
        const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        AddVertices(Interface, TEXT("Inputs"), TEXT("inputs"), Result);
        AddVertices(Interface, TEXT("Outputs"), TEXT("outputs"), Result);
        AddVertices(Interface, TEXT("Environment"), TEXT("environment"), Result);
        return Result;
    }

    FString CompactClassName(const TSharedPtr<FJsonObject>& Wrapper)
    {
        const TSharedPtr<FJsonObject> FieldsObject = Fields(Wrapper);
        if (!FieldsObject.IsValid()) return FString();
        const FString Namespace = StringField(FieldsObject, TEXT("Namespace"));
        const FString Name = StringField(FieldsObject, TEXT("Name"));
        const FString Variant = StringField(FieldsObject, TEXT("Variant"));
        FString Result = Namespace.IsEmpty() || Namespace == TEXT("None") ? Name : Namespace + TEXT(".") + Name;
        if (!Variant.IsEmpty() && Variant != TEXT("None")) Result += TEXT(".") + Variant;
        return Result;
    }

    TSharedRef<FJsonObject> CompactNode(const TSharedPtr<FJsonObject>& Wrapper)
    {
        const TSharedPtr<FJsonObject> Node = Fields(Wrapper);
        const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        if (!Node.IsValid()) return Result;
        Result->SetStringField(TEXT("id"), GuidString(Node->GetObjectField(TEXT("ID"))));
        Result->SetStringField(TEXT("classId"), GuidString(Node->GetObjectField(TEXT("ClassID"))));
        const FString Name = StringField(Node, TEXT("Name"));
        if (!Name.IsEmpty() && Name != TEXT("None")) Result->SetStringField(TEXT("name"), Name);
        const TSharedPtr<FJsonObject>* Interface = nullptr;
        if (Node->TryGetObjectField(TEXT("Interface"), Interface) && Interface != nullptr)
        {
            Result->SetObjectField(TEXT("interface"), CompactInterface(*Interface));
        }
        const TArray<TSharedPtr<FJsonValue>>* Literals = nullptr;
        if (Node->TryGetArrayField(TEXT("InputLiterals"), Literals) && Literals != nullptr)
        {
            TArray<TSharedPtr<FJsonValue>> CompactLiterals;
            for (const TSharedPtr<FJsonValue>& Value : *Literals)
            {
                const TSharedPtr<FJsonObject>* LiteralWrapper = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(LiteralWrapper) || LiteralWrapper == nullptr) continue;
                const TSharedPtr<FJsonObject> LiteralFields = Fields(*LiteralWrapper);
                if (!LiteralFields.IsValid()) continue;
                const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
                Entry->SetStringField(TEXT("vertexId"), GuidString(LiteralFields->GetObjectField(TEXT("VertexID"))));
                const TSharedPtr<FJsonObject>* LiteralValue = nullptr;
                if (LiteralFields->TryGetObjectField(TEXT("Value"), LiteralValue) && LiteralValue != nullptr)
                {
                    if (const TSharedPtr<FJsonObject> CompactValue = CompactLiteral(*LiteralValue))
                    {
                        Entry->SetObjectField(TEXT("value"), CompactValue);
                    }
                }
                CompactLiterals.Add(MakeShared<FJsonValueObject>(Entry));
            }
            if (!CompactLiterals.IsEmpty()) Result->SetArrayField(TEXT("inputLiterals"), CompactLiterals);
        }
        return Result;
    }

    TSharedRef<FJsonObject> CompactEdge(const TSharedPtr<FJsonObject>& Wrapper)
    {
        const TSharedPtr<FJsonObject> Edge = Fields(Wrapper);
        const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        if (!Edge.IsValid()) return Result;
        Result->SetStringField(TEXT("fromNodeId"), GuidString(Edge->GetObjectField(TEXT("FromNodeID"))));
        Result->SetStringField(TEXT("fromVertexId"), GuidString(Edge->GetObjectField(TEXT("FromVertexID"))));
        Result->SetStringField(TEXT("toNodeId"), GuidString(Edge->GetObjectField(TEXT("ToNodeID"))));
        Result->SetStringField(TEXT("toVertexId"), GuidString(Edge->GetObjectField(TEXT("ToVertexID"))));
        return Result;
    }

    TSharedRef<FJsonObject> CompactVariable(const TSharedPtr<FJsonObject>& Wrapper)
    {
        const TSharedPtr<FJsonObject> Variable = Fields(Wrapper);
        const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        if (!Variable.IsValid()) return Result;
        Result->SetStringField(TEXT("id"), GuidString(Variable->GetObjectField(TEXT("ID"))));
        Result->SetStringField(TEXT("name"), StringField(Variable, TEXT("Name")));
        Result->SetStringField(TEXT("type"), StringField(Variable, TEXT("TypeName")));
        const TSharedPtr<FJsonObject>* Literal = nullptr;
        if (Variable->TryGetObjectField(TEXT("Literal"), Literal) && Literal != nullptr)
        {
            if (const TSharedPtr<FJsonObject> CompactValue = CompactLiteral(*Literal))
            {
                Result->SetObjectField(TEXT("literal"), CompactValue);
            }
        }
        return Result;
    }

    TSharedRef<FJsonObject> CompactPage(const TSharedPtr<FJsonObject>& Wrapper)
    {
        const TSharedPtr<FJsonObject> Page = Fields(Wrapper);
        const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        if (!Page.IsValid()) return Result;
        Result->SetStringField(TEXT("id"), GuidString(Page->GetObjectField(TEXT("PageID"))));
        static const TPair<const TCHAR*, const TCHAR*> Arrays[] = {
            { TEXT("Nodes"), TEXT("nodes") },
            { TEXT("Edges"), TEXT("edges") },
            { TEXT("Variables"), TEXT("variables") }
        };
        for (int32 KindIndex = 0; KindIndex < UE_ARRAY_COUNT(Arrays); ++KindIndex)
        {
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Page->TryGetArrayField(Arrays[KindIndex].Key, Values) || Values == nullptr) continue;
            TArray<TSharedPtr<FJsonValue>> Compact;
            for (const TSharedPtr<FJsonValue>& Value : *Values)
            {
                const TSharedPtr<FJsonObject>* WrapperValue = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(WrapperValue) || WrapperValue == nullptr) continue;
                TSharedRef<FJsonObject> Entry = KindIndex == 0
                    ? CompactNode(*WrapperValue)
                    : KindIndex == 1 ? CompactEdge(*WrapperValue) : CompactVariable(*WrapperValue);
                Compact.Add(MakeShared<FJsonValueObject>(Entry));
            }
            Result->SetArrayField(Arrays[KindIndex].Value, Compact);
        }
        return Result;
    }

    void AddGraphClass(
        const TSharedPtr<FJsonObject>& Wrapper,
        const TSharedRef<FJsonObject>& Target)
    {
        const TSharedPtr<FJsonObject> GraphClass = Fields(Wrapper);
        if (!GraphClass.IsValid()) return;
        Target->SetStringField(TEXT("id"), GuidString(GraphClass->GetObjectField(TEXT("ID"))));
        const TSharedPtr<FJsonObject>* Interface = nullptr;
        if (GraphClass->TryGetObjectField(TEXT("Interface"), Interface) && Interface != nullptr)
        {
            Target->SetObjectField(TEXT("interface"), CompactInterface(*Interface));
        }
        const TSharedPtr<FJsonObject>* MetadataWrapper = nullptr;
        if (GraphClass->TryGetObjectField(TEXT("Metadata"), MetadataWrapper) && MetadataWrapper != nullptr)
        {
            const TSharedPtr<FJsonObject> Metadata = Fields(*MetadataWrapper);
            if (Metadata.IsValid())
            {
                const TSharedPtr<FJsonObject>* ClassName = nullptr;
                if (Metadata->TryGetObjectField(TEXT("ClassName"), ClassName) && ClassName != nullptr)
                {
                    Target->SetStringField(TEXT("className"), CompactClassName(*ClassName));
                }
                const TSharedPtr<FJsonObject>* Type = nullptr;
                if (Metadata->TryGetObjectField(TEXT("Type"), Type) && Type != nullptr)
                {
                    Target->SetStringField(TEXT("classType"), EnumName(*Type));
                }
            }
        }
        TArray<TSharedPtr<FJsonValue>> Pages;
        const TSharedPtr<FJsonObject>* LegacyPage = nullptr;
        if (GraphClass->TryGetObjectField(TEXT("Graph"), LegacyPage) && LegacyPage != nullptr)
        {
            const TSharedRef<FJsonObject> Page = CompactPage(*LegacyPage);
            const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
            if (Page->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes != nullptr && !Nodes->IsEmpty())
            {
                Pages.Add(MakeShared<FJsonValueObject>(Page));
            }
        }
        const TArray<TSharedPtr<FJsonValue>>* PagedGraphs = nullptr;
        if (GraphClass->TryGetArrayField(TEXT("PagedGraphs"), PagedGraphs) && PagedGraphs != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *PagedGraphs)
            {
                const TSharedPtr<FJsonObject>* Page = nullptr;
                if (Value.IsValid() && Value->TryGetObject(Page) && Page != nullptr)
                {
                    Pages.Add(MakeShared<FJsonValueObject>(CompactPage(*Page)));
                }
            }
        }
        Target->SetArrayField(TEXT("pages"), Pages);
    }

    TSharedPtr<FJsonObject> FindPropertyValue(
        const TArray<TSharedPtr<FJsonValue>>& Properties,
        const TArray<FString>& Names)
    {
        for (const TSharedPtr<FJsonValue>& Value : Properties)
        {
            const TSharedPtr<FJsonObject>* Property = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Property) || Property == nullptr) continue;
            FString Name;
            (*Property)->TryGetStringField(TEXT("name"), Name);
            if (!Names.Contains(Name)) continue;
            const TSharedPtr<FJsonObject>* PropertyValue = nullptr;
            if ((*Property)->TryGetObjectField(TEXT("value"), PropertyValue) && PropertyValue != nullptr)
            {
                return *PropertyValue;
            }
        }
        return nullptr;
    }
}

FName FUERingMetaSoundExporter::GetName() const
{
    return TEXT("MetaSound");
}

bool FUERingMetaSoundExporter::CanExport(const FAssetData& AssetData) const
{
    const FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
    return ClassName == TEXT("MetaSoundSource") || ClassName == TEXT("MetaSoundPatch");
}

bool FUERingMetaSoundExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    UObject* Asset = Context.Asset.Get();
    if (Asset == nullptr)
    {
        OutError = TEXT("The MetaSound asset could not be loaded.");
        return false;
    }
    const FString ClassName = Asset->GetClass()->GetName();
    const TArray<TSharedPtr<FJsonValue>> CoreProperties =
        UERingPropertySerializer::SerializeNamedObjectProperties(*Asset, {
            TEXT("RootMetaSoundDocument"),
            TEXT("RootMetasoundDocument"),
            TEXT("bIsPreset"),
            TEXT("AssetClassID"),
            TEXT("RegistryVersionMajor"),
            TEXT("RegistryVersionMinor")
        });
    const TSharedPtr<FJsonObject> DocumentWrapper = UERingMetaSoundExporter::FindPropertyValue(
        CoreProperties,
        { TEXT("RootMetaSoundDocument"), TEXT("RootMetasoundDocument") });
    const TSharedPtr<FJsonObject> Document = UERingMetaSoundExporter::Fields(DocumentWrapper);
    if (!Document.IsValid())
    {
        OutError = TEXT("The MetaSound frontend document was unavailable.");
        return false;
    }

    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("MetaSound"));
    Semantics->SetStringField(TEXT("representation"), TEXT("metasound-frontend-graph-v1"));
    Semantics->SetStringField(TEXT("role"),
        ClassName == TEXT("MetaSoundSource") ? TEXT("source") : TEXT("patch"));
    Semantics->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetPathName());

    for (const TSharedPtr<FJsonValue>& Value : CoreProperties)
    {
        const TSharedPtr<FJsonObject>* Property = nullptr;
        if (!Value.IsValid() || !Value->TryGetObject(Property) || Property == nullptr) continue;
        const FString Name = UERingMetaSoundExporter::StringField(*Property, TEXT("name"));
        if (Name == TEXT("bIsPreset"))
        {
            bool bPreset = false;
            if ((*Property)->TryGetBoolField(TEXT("value"), bPreset)) Semantics->SetBoolField(TEXT("isPreset"), bPreset);
        }
        else if (Name == TEXT("AssetClassID"))
        {
            const TSharedPtr<FJsonObject>* ValueObject = nullptr;
            if ((*Property)->TryGetObjectField(TEXT("value"), ValueObject) && ValueObject != nullptr)
            {
                Semantics->SetStringField(TEXT("assetClassId"), UERingMetaSoundExporter::GuidString(*ValueObject));
            }
        }
        else if (Name == TEXT("RegistryVersionMajor") || Name == TEXT("RegistryVersionMinor"))
        {
            double Number = 0.0;
            if ((*Property)->TryGetNumberField(TEXT("value"), Number))
            {
                Semantics->SetNumberField(Name == TEXT("RegistryVersionMajor")
                    ? TEXT("registryVersionMajor") : TEXT("registryVersionMinor"), Number);
            }
        }
    }

    const TSharedPtr<FJsonObject>* RootGraph = nullptr;
    if (Document->TryGetObjectField(TEXT("RootGraph"), RootGraph) && RootGraph != nullptr)
    {
        const TSharedRef<FJsonObject> Graph = MakeShared<FJsonObject>();
        UERingMetaSoundExporter::AddGraphClass(*RootGraph, Graph);
        Semantics->SetObjectField(TEXT("rootGraph"), Graph);
    }

    const TArray<TSharedPtr<FJsonValue>>* Dependencies = nullptr;
    if (Document->TryGetArrayField(TEXT("Dependencies"), Dependencies) && Dependencies != nullptr)
    {
        TArray<TSharedPtr<FJsonValue>> CompactDependencies;
        for (const TSharedPtr<FJsonValue>& Value : *Dependencies)
        {
            const TSharedPtr<FJsonObject>* Wrapper = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Wrapper) || Wrapper == nullptr) continue;
            const TSharedPtr<FJsonObject> Dependency = UERingMetaSoundExporter::Fields(*Wrapper);
            if (!Dependency.IsValid()) continue;
            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetStringField(TEXT("id"), UERingMetaSoundExporter::GuidString(
                Dependency->GetObjectField(TEXT("ID"))));
            const TSharedPtr<FJsonObject>* MetadataWrapper = nullptr;
            if (Dependency->TryGetObjectField(TEXT("Metadata"), MetadataWrapper) && MetadataWrapper != nullptr)
            {
                const TSharedPtr<FJsonObject> Metadata = UERingMetaSoundExporter::Fields(*MetadataWrapper);
                if (Metadata.IsValid())
                {
                    const TSharedPtr<FJsonObject>* ClassNameWrapper = nullptr;
                    if (Metadata->TryGetObjectField(TEXT("ClassName"), ClassNameWrapper) && ClassNameWrapper != nullptr)
                    {
                        Entry->SetStringField(TEXT("className"),
                            UERingMetaSoundExporter::CompactClassName(*ClassNameWrapper));
                    }
                    const TSharedPtr<FJsonObject>* Type = nullptr;
                    if (Metadata->TryGetObjectField(TEXT("Type"), Type) && Type != nullptr)
                    {
                        Entry->SetStringField(TEXT("classType"), UERingMetaSoundExporter::EnumName(*Type));
                    }
                }
            }
            const TSharedPtr<FJsonObject>* Interface = nullptr;
            if (Dependency->TryGetObjectField(TEXT("Interface"), Interface) && Interface != nullptr)
            {
                Entry->SetObjectField(TEXT("interface"), UERingMetaSoundExporter::CompactInterface(*Interface));
            }
            CompactDependencies.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Semantics->SetArrayField(TEXT("dependencies"), CompactDependencies);
    }

    const TArray<TSharedPtr<FJsonValue>>* Subgraphs = nullptr;
    if (Document->TryGetArrayField(TEXT("Subgraphs"), Subgraphs) && Subgraphs != nullptr)
    {
        TArray<TSharedPtr<FJsonValue>> CompactSubgraphs;
        for (const TSharedPtr<FJsonValue>& Value : *Subgraphs)
        {
            const TSharedPtr<FJsonObject>* Subgraph = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Subgraph) || Subgraph == nullptr) continue;
            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            UERingMetaSoundExporter::AddGraphClass(*Subgraph, Entry);
            CompactSubgraphs.Add(MakeShared<FJsonValueObject>(Entry));
        }
        if (!CompactSubgraphs.IsEmpty()) Semantics->SetArrayField(TEXT("subgraphs"), CompactSubgraphs);
    }

    if (ClassName == TEXT("MetaSoundSource"))
    {
        UERingSemanticUtils::SetSelectedProperties(*Asset, {
            TEXT("Duration"), TEXT("OutputFormat"), TEXT("SampleRateOverride"),
            TEXT("BlockRateOverride"), TEXT("NumChannels"), TEXT("bLooping"),
            TEXT("LoadingBehavior"), TEXT("AttenuationSettings"), TEXT("ConcurrencySet"),
            TEXT("SoundClassObject"), TEXT("SoundSubmixObject"), TEXT("SoundSubmixSends"),
            TEXT("SourceEffectChain"), TEXT("Volume"), TEXT("Pitch"), TEXT("VirtualizationMode")
        }, Semantics, TEXT("sourceSettings"));
    }
    OutPayload.Semantics = Semantics;
    return true;
}
