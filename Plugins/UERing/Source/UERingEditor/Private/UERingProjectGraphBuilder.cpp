#include "UERingProjectGraphBuilder.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SQLiteDatabase.h"
#include "SQLitePreparedStatement.h"
#include "UERingVersion.h"

namespace UERingProjectGraphBuilder
{
    constexpr const TCHAR* GraphSchema = TEXT("com.ue-ring.usem.project-graph");
    constexpr const TCHAR* GraphVersion = TEXT("1.1.0");

    FString StringField(const TSharedRef<FJsonObject>& Object, const TCHAR* Field)
    {
        FString Value;
        Object->TryGetStringField(Field, Value);
        return Value;
    }

    int32 StableTextCompare(const FString& Left, const FString& Right)
    {
        const int32 IgnoreCase = Left.Compare(Right, ESearchCase::IgnoreCase);
        return IgnoreCase != 0 ? IgnoreCase : Left.Compare(Right, ESearchCase::CaseSensitive);
    }

    FString EscapePointerToken(FString Token)
    {
        Token.ReplaceInline(TEXT("~"), TEXT("~0"));
        Token.ReplaceInline(TEXT("/"), TEXT("~1"));
        return Token;
    }

    FString AssetNodeId(const FString& PackageName)
    {
        return TEXT("asset:") + PackageName;
    }

    FString PackageNodeId(const FString& PackageName)
    {
        FString Id = TEXT("package:") + PackageName;
        Id.ToLowerInline();
        return Id;
    }

    FString SymbolNodeId(const FString& Kind, const FString& Identity)
    {
        FString Id = TEXT("symbol:") + Kind + TEXT(":") + Identity;
        Id.ToLowerInline();
        return Id;
    }

    FString PackageFromObjectPath(FString ObjectPath)
    {
        int32 DotIndex = INDEX_NONE;
        if (ObjectPath.FindChar(TEXT('.'), DotIndex))
        {
            ObjectPath.LeftInline(DotIndex);
        }
        return FPackageName::IsValidLongPackageName(ObjectPath) ? ObjectPath : FString();
    }

    bool ReadJson(const FString& Filename, TSharedPtr<FJsonObject>& OutRoot)
    {
        FString Json;
        return FFileHelper::LoadFileToString(Json, *Filename)
            && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), OutRoot)
            && OutRoot.IsValid();
    }

    bool WriteJson(const FString& Filename, const TSharedRef<FJsonObject>& Root, FString& OutError)
    {
        FString Json;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
        if (!FJsonSerializer::Serialize(Root, Writer))
        {
            OutError = FString::Printf(TEXT("Could not serialize unified project graph: %s"), *Filename);
            return false;
        }
        Json += LINE_TERMINATOR;
        if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true))
        {
            OutError = FString::Printf(TEXT("Could not create unified project graph directory: %s"), *Filename);
            return false;
        }
        const FString TempFile = Filename + TEXT(".tmp");
        if (!FFileHelper::SaveStringToFile(Json, *TempFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
            || !IFileManager::Get().Move(*Filename, *TempFile, true, true, false, true))
        {
            IFileManager::Get().Delete(*TempFile, false, true);
            OutError = FString::Printf(TEXT("Could not write unified project graph: %s"), *Filename);
            return false;
        }
        return true;
    }

    class FAccumulator
    {
    public:
        TMap<FString, TSharedPtr<FJsonObject>> Nodes;
        TMap<FString, TSharedPtr<FJsonObject>> Edges;
        TMap<FString, FString> ProjectPackages;
        FString ActiveContributorPackage;

        static FString EdgeKey(const TSharedRef<FJsonObject>& Edge)
        {
            return StringField(Edge, TEXT("from")) + TEXT("\x0001")
                + StringField(Edge, TEXT("relation")) + TEXT("\x0001")
                + StringField(Edge, TEXT("to")) + TEXT("\x0001")
                + StringField(Edge, TEXT("qualifier")) + TEXT("\x0001")
                + StringField(Edge, TEXT("sourceNodeId")) + TEXT("\x0001")
                + StringField(Edge, TEXT("sourcePinId")) + TEXT("\x0001")
                + StringField(Edge, TEXT("targetPinId"));
        }

        void AddNode(
            const FString& Id,
            const FString& Kind,
            const FString& Label,
            const FString& PackageName = FString(),
            const FString& Subtype = FString())
        {
            if (Id.IsEmpty() || Nodes.Contains(Id)) return;
            const TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
            Node->SetStringField(TEXT("id"), Id);
            Node->SetStringField(TEXT("kind"), Kind);
            Node->SetStringField(TEXT("label"), Label);
            if (!PackageName.IsEmpty()) Node->SetStringField(TEXT("packageName"), PackageName);
            if (!Subtype.IsEmpty()) Node->SetStringField(TEXT("subtype"), Subtype);
            Nodes.Add(Id, Node);
        }

        FString AddPackageTarget(const FString& PackageName)
        {
            if (const FString* CanonicalPackage = ProjectPackages.Find(PackageName))
            {
                return AssetNodeId(*CanonicalPackage);
            }
            const FString Id = PackageNodeId(PackageName);
            AddNode(Id, TEXT("externalPackage"), PackageName, PackageName);
            return Id;
        }

        FString AddTypeTarget(const FString& TypePath)
        {
            const FString PackageName = PackageFromObjectPath(TypePath);
            if (const FString* CanonicalPackage = ProjectPackages.Find(PackageName))
            {
                return AssetNodeId(*CanonicalPackage);
            }
            const FString Id = SymbolNodeId(TEXT("class"), TypePath);
            AddNode(Id, TEXT("symbol"), TypePath, FString(), TEXT("class"));
            return Id;
        }

        void AddEdge(
            const FString& From,
            const FString& To,
            const FString& Relation,
            const double Confidence,
            const FString& EvidenceSource,
            const FString& EvidencePointer = FString(),
            const FString& Qualifier = FString(),
            const FString& SourceNodeId = FString(),
            const FString& SourceTitle = FString(),
            const FString& SourcePinId = FString(),
            const FString& TargetPinId = FString())
        {
            if (From.IsEmpty() || To.IsEmpty() || Relation.IsEmpty()) return;
            const TSharedRef<FJsonObject> Edge = MakeShared<FJsonObject>();
            Edge->SetStringField(TEXT("from"), From);
            Edge->SetStringField(TEXT("to"), To);
            Edge->SetStringField(TEXT("relation"), Relation);
            Edge->SetNumberField(TEXT("confidence"), Confidence);
            Edge->SetStringField(TEXT("evidenceSource"), EvidenceSource);
            if (!EvidencePointer.IsEmpty()) Edge->SetStringField(TEXT("evidencePointer"), EvidencePointer);
            if (!Qualifier.IsEmpty()) Edge->SetStringField(TEXT("qualifier"), Qualifier);
            if (!SourceNodeId.IsEmpty()) Edge->SetStringField(TEXT("sourceNodeId"), SourceNodeId);
            if (!SourceTitle.IsEmpty()) Edge->SetStringField(TEXT("sourceTitle"), SourceTitle);
            if (!SourcePinId.IsEmpty()) Edge->SetStringField(TEXT("sourcePinId"), SourcePinId);
            if (!TargetPinId.IsEmpty()) Edge->SetStringField(TEXT("targetPinId"), TargetPinId);
            if (!ActiveContributorPackage.IsEmpty())
            {
                Edge->SetStringField(TEXT("contributorPackage"), ActiveContributorPackage);
            }
            const FString Key = EdgeKey(Edge);
            if (!Edges.Contains(Key)) Edges.Add(Key, Edge);
        }
    };

    FString FindTagQualifier(const TSharedPtr<FJsonValue>& Value)
    {
        if (!Value.IsValid()) return FString();
        if (Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Object = Value->AsObject();
            FString TagName;
            if (Object.IsValid()
                && Object->TryGetStringField(TEXT("TagName"), TagName)
                && !TagName.IsEmpty()
                && TagName != TEXT("None"))
            {
                return TagName;
            }
            if (Object.IsValid())
            {
                for (const auto& Pair : Object->Values)
                {
                    TagName = FindTagQualifier(Pair.Value);
                    if (!TagName.IsEmpty()) return TagName;
                }
            }
        }
        else if (Value->Type == EJson::Array)
        {
            for (const TSharedPtr<FJsonValue>& Child : Value->AsArray())
            {
                const FString TagName = FindTagQualifier(Child);
                if (!TagName.IsEmpty()) return TagName;
            }
        }
        return FString();
    }

    FString AssetReferenceRelation(
        const FString& PropertyName,
        const FString& OwnerClass,
        const FString& TargetClass,
        const FString& Domain,
        const FString& Role)
    {
        if (PropertyName.Contains(TEXT("GrantedGameplayAbilities"), ESearchCase::IgnoreCase)
            || (Domain == TEXT("gas") && Role == TEXT("abilitySet")
                && PropertyName.Equals(TEXT("Ability"), ESearchCase::IgnoreCase)))
        {
            return TEXT("grantsAbility");
        }
        if (PropertyName.Contains(TEXT("GrantedGameplayEffects"), ESearchCase::IgnoreCase)
            || PropertyName.Equals(TEXT("GameplayEffect"), ESearchCase::IgnoreCase))
        {
            return TEXT("grantsEffect");
        }
        if (PropertyName.Contains(TEXT("GrantedAttributes"), ESearchCase::IgnoreCase)
            || (Domain == TEXT("gas") && Role == TEXT("abilitySet")
                && PropertyName.Equals(TEXT("AttributeSet"), ESearchCase::IgnoreCase)))
        {
            return TEXT("grantsAttributeSet");
        }
        if (PropertyName.Contains(TEXT("AbilitySet"), ESearchCase::IgnoreCase)) return TEXT("usesAbilitySet");
        if (PropertyName.Contains(TEXT("DefaultPawnData"), ESearchCase::IgnoreCase)) return TEXT("usesDefaultPawnData");
        if (PropertyName.Contains(TEXT("PawnData"), ESearchCase::IgnoreCase)) return TEXT("usesPawnData");
        if (PropertyName.Contains(TEXT("ActionSet"), ESearchCase::IgnoreCase)) return TEXT("includesActionSet");
        if (PropertyName.Contains(TEXT("InputConfig"), ESearchCase::IgnoreCase)) return TEXT("usesInputConfig");
        if (PropertyName.Contains(TEXT("InputMapping"), ESearchCase::IgnoreCase))
        {
            return OwnerClass.Contains(TEXT("GameFeatureAction"), ESearchCase::IgnoreCase)
                ? TEXT("addsInputMapping") : TEXT("usesInputMapping");
        }
        if (PropertyName.Contains(TEXT("RegistriesToAdd"), ESearchCase::IgnoreCase)) return TEXT("addsDataRegistry");
        if (PropertyName.Equals(TEXT("ComponentClass"), ESearchCase::IgnoreCase)) return TEXT("addsComponent");
        if (PropertyName.Contains(TEXT("SourceTable"), ESearchCase::IgnoreCase)
            || (TargetClass.Contains(TEXT("DataTable"), ESearchCase::IgnoreCase)
                && Domain == TEXT("dataRegistry")))
        {
            return TEXT("readsDataTable");
        }
        if (PropertyName.Contains(TEXT("CameraMode"), ESearchCase::IgnoreCase)) return TEXT("usesCameraMode");
        if (PropertyName.Contains(TEXT("PawnClass"), ESearchCase::IgnoreCase)) return TEXT("usesPawnClass");
        if (PropertyName.Contains(TEXT("TagRelationship"), ESearchCase::IgnoreCase)) return TEXT("usesTagRelationship");
        if (PropertyName.Contains(TEXT("CooldownGameplayEffect"), ESearchCase::IgnoreCase)) return TEXT("usesCooldownEffect");
        if (TargetClass.Contains(TEXT("InputAction"), ESearchCase::IgnoreCase)) return TEXT("usesInputAction");
        return TEXT("referencesAsset");
    }

    void AddSemanticReferences(
        const TSharedPtr<FJsonValue>& Value,
        const FString& Pointer,
        const FString& SourcePackage,
        const FString& SemanticFile,
        FAccumulator& Graph,
        const FString& PropertyName = FString(),
        const FString& OwnerClass = FString(),
        const FString& Domain = FString(),
        const FString& Role = FString(),
        const FString& Qualifier = FString(),
        const FString& SourceNode = FString())
    {
        if (!Value.IsValid()) return;
        if (Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Object = Value->AsObject();
            if (!Object.IsValid()) return;
            FString LocalProperty = PropertyName;
            FString SerializedPropertyName;
            if (Object->TryGetStringField(TEXT("name"), SerializedPropertyName)
                && !SerializedPropertyName.IsEmpty())
            {
                LocalProperty = SerializedPropertyName;
            }
            FString LocalOwnerClass = OwnerClass;
            FString LocalObjectId;
            FString ObjectClass;
            const bool bHasLocalObjectIdentity = Object->TryGetStringField(TEXT("id"), LocalObjectId)
                && Object->TryGetStringField(TEXT("class"), ObjectClass)
                && !LocalObjectId.IsEmpty();
            if (bHasLocalObjectIdentity)
            {
                LocalOwnerClass = ObjectClass;
            }
            FString LocalQualifier = Qualifier;
            FString StructType;
            if (Object->TryGetStringField(TEXT("structType"), StructType))
            {
                const FString FoundQualifier = FindTagQualifier(Value);
                if (!FoundQualifier.IsEmpty()) LocalQualifier = FoundQualifier;
            }
            FString LocalSourceNode = SourceNode.IsEmpty() ? AssetNodeId(SourcePackage) : SourceNode;
            if (bHasLocalObjectIdentity)
            {
                LocalSourceNode = TEXT("object:") + SourcePackage + TEXT("#") + LocalObjectId;
                Graph.AddNode(LocalSourceNode, TEXT("object"), LocalObjectId, SourcePackage, LocalOwnerClass);
                Graph.AddEdge(
                    AssetNodeId(SourcePackage),
                    LocalSourceNode,
                    LocalOwnerClass.Contains(TEXT("GameFeatureAction"), ESearchCase::IgnoreCase)
                        ? TEXT("containsAction") : TEXT("containsObject"),
                    1.0,
                    SemanticFile,
                    Pointer);
            }
            FString ObjectPath;
            if (Object->TryGetStringField(TEXT("objectPath"), ObjectPath))
            {
                if (ObjectPath.StartsWith(TEXT("/Script/"), ESearchCase::IgnoreCase))
                {
                    Graph.AddEdge(
                        LocalSourceNode,
                        Graph.AddTypeTarget(ObjectPath),
                        AssetReferenceRelation(
                            LocalProperty, LocalOwnerClass, ObjectClass, Domain, Role),
                        0.98,
                        SemanticFile,
                        Pointer + TEXT("/objectPath"),
                        LocalQualifier);
                }
                else
                {
                    const FString TargetPackage = PackageFromObjectPath(ObjectPath);
                    if (!TargetPackage.IsEmpty())
                    {
                        FString Target;
                        FString Relation;
                        if (TargetPackage == SourcePackage && ObjectPath.Contains(TEXT(":")))
                        {
                            FString SubobjectId;
                            ObjectPath.Split(
                                TEXT(":"),
                                nullptr,
                                &SubobjectId,
                                ESearchCase::CaseSensitive,
                                ESearchDir::FromEnd);
                            Target = TEXT("object:") + SourcePackage + TEXT("#") + SubobjectId;
                            Graph.AddNode(Target, TEXT("object"), SubobjectId, SourcePackage, ObjectClass);
                            Relation = LocalProperty.Contains(TEXT("Actions"), ESearchCase::IgnoreCase)
                                ? TEXT("containsAction") : TEXT("referencesObject");
                        }
                        else if (TargetPackage != SourcePackage)
                        {
                            Target = Graph.AddPackageTarget(TargetPackage);
                            Relation = AssetReferenceRelation(
                                LocalProperty, LocalOwnerClass, ObjectClass, Domain, Role);
                        }
                        if (!Target.IsEmpty())
                        {
                            Graph.AddEdge(
                                LocalSourceNode,
                                Target,
                                Relation,
                                0.98,
                                SemanticFile,
                                Pointer + TEXT("/objectPath"),
                                LocalQualifier);
                        }
                    }
                }
            }
            TArray<TPair<FString, TSharedPtr<FJsonValue>>> Children;
            for (const auto& Pair : Object->Values)
            {
                Children.Emplace(FString(Pair.Key), Pair.Value);
            }
            Children.Sort([](const auto& Left, const auto& Right) { return Left.Key < Right.Key; });
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Child : Children)
            {
                FString ChildProperty = LocalProperty;
                if (Child.Key != TEXT("value") && Child.Key != TEXT("fields")
                    && Child.Key != TEXT("properties") && Child.Key != TEXT("assetProperties")
                    && Child.Key != TEXT("classDefaultOverrides") && Child.Key != TEXT("ownedObjects"))
                {
                    ChildProperty = Child.Key;
                }
                AddSemanticReferences(
                    Child.Value,
                    Pointer + TEXT("/") + EscapePointerToken(Child.Key),
                    SourcePackage,
                    SemanticFile,
                    Graph,
                    ChildProperty,
                    LocalOwnerClass,
                    Domain,
                    Role,
                    LocalQualifier,
                    LocalSourceNode);
            }
        }
        else if (Value->Type == EJson::Array)
        {
            const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
            for (int32 Index = 0; Index < Values.Num(); ++Index)
            {
                AddSemanticReferences(
                    Values[Index],
                    Pointer + TEXT("/") + FString::FromInt(Index),
                    SourcePackage,
                    SemanticFile,
                    Graph,
                    PropertyName,
                    OwnerClass,
                    Domain,
                    Role,
                    Qualifier,
                    SourceNode);
            }
        }
        else if (Value->Type == EJson::String
            && PropertyName.Contains(TEXT("GameFeaturesToEnable"), ESearchCase::IgnoreCase))
        {
            const FString PluginName = Value->AsString();
            if (!PluginName.IsEmpty())
            {
                const FString Target = TEXT("gameFeaturePlugin:") + PluginName;
                Graph.AddNode(Target, TEXT("gameFeaturePlugin"), PluginName);
                Graph.AddEdge(
                    SourceNode.IsEmpty() ? AssetNodeId(SourcePackage) : SourceNode,
                    Target,
                    TEXT("enablesGameFeature"),
                    0.98,
                    SemanticFile,
                    Pointer);
            }
        }
    }

    FString MaterialNodeId(const FString& PackageName, const FString& LocalId)
    {
        return TEXT("materialNode:") + PackageName + TEXT("#") + LocalId;
    }

    bool AddMaterialGraph(
        const TSharedRef<FJsonObject>& Semantics,
        const FString& SourcePackage,
        const FString& SemanticFile,
        FAccumulator& Graph)
    {
        const FString Representation = StringField(Semantics, TEXT("representation"));
        if (Representation != TEXT("material-expression-graph-v1")
            && Representation != TEXT("material-function-graph-v1"))
        {
            return false;
        }

        const FString AssetId = AssetNodeId(SourcePackage);
        const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
        if (Semantics->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes != nullptr)
        {
            for (int32 Index = 0; Index < Nodes->Num(); ++Index)
            {
                const TSharedPtr<FJsonObject> Node = (*Nodes)[Index].IsValid()
                    ? (*Nodes)[Index]->AsObject() : nullptr;
                if (!Node.IsValid()) continue;
                const FString LocalId = StringField(Node.ToSharedRef(), TEXT("id"));
                if (LocalId.IsEmpty()) continue;
                const FString Class = StringField(Node.ToSharedRef(), TEXT("class"));
                FString Label = StringField(Node.ToSharedRef(), TEXT("name"));
                if (Label.IsEmpty()) Label = Class;
                const FString NodeId = MaterialNodeId(SourcePackage, LocalId);
                const FString Pointer = TEXT("/semantics/nodes/") + FString::FromInt(Index);
                Graph.AddNode(NodeId, TEXT("materialNode"), Label, SourcePackage, Class);
                Graph.AddEdge(
                    AssetId,
                    NodeId,
                    TEXT("containsMaterialNode"),
                    1.0,
                    SemanticFile,
                    Pointer);
                if (const TSharedPtr<FJsonValue> Configuration = Node->TryGetField(TEXT("configuration")))
                {
                    AddSemanticReferences(
                        Configuration,
                        Pointer + TEXT("/configuration"),
                        SourcePackage,
                        SemanticFile,
                        Graph,
                        TEXT("configuration"),
                        Class,
                        FString(),
                        FString(),
                        FString(),
                        NodeId);
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* Connections = nullptr;
        if (Semantics->TryGetArrayField(TEXT("connections"), Connections) && Connections != nullptr)
        {
            for (int32 Index = 0; Index < Connections->Num(); ++Index)
            {
                const TSharedPtr<FJsonObject> Connection = (*Connections)[Index].IsValid()
                    ? (*Connections)[Index]->AsObject() : nullptr;
                if (!Connection.IsValid()) continue;
                const FString SourceLocalId = StringField(Connection.ToSharedRef(), TEXT("sourceNode"));
                const FString TargetLocalId = StringField(Connection.ToSharedRef(), TEXT("targetNode"));
                const FString TargetInput = StringField(Connection.ToSharedRef(), TEXT("targetInput"));
                if (SourceLocalId.IsEmpty() || TargetLocalId.IsEmpty()) continue;
                const FString SourceNode = MaterialNodeId(SourcePackage, SourceLocalId);
                const bool bTargetsRoot = TargetLocalId.StartsWith(TEXT("$"));
                const FString TargetNode = bTargetsRoot
                    ? AssetId : MaterialNodeId(SourcePackage, TargetLocalId);
                double OutputIndex = 0.0;
                Connection->TryGetNumberField(TEXT("sourceOutputIndex"), OutputIndex);
                const FString OutputName = StringField(Connection.ToSharedRef(), TEXT("sourceOutputName"));
                Graph.AddEdge(
                    SourceNode,
                    TargetNode,
                    bTargetsRoot ? TEXT("feedsMaterialInput") : TEXT("feedsExpressionInput"),
                    1.0,
                    SemanticFile,
                    TEXT("/semantics/connections/") + FString::FromInt(Index),
                    TargetInput,
                    SourceLocalId,
                    OutputName,
                    FString::FromInt(static_cast<int32>(OutputIndex)),
                    TargetInput);
            }
        }
        return true;
    }

    FString MemberRelation(const FString& Kind, const FString& NodeClass)
    {
        if (Kind == TEXT("function")) return TEXT("calls");
        if (Kind == TEXT("event")) return TEXT("handlesEvent");
        if (Kind == TEXT("delegate") || Kind == TEXT("delegateFunction"))
        {
            if (NodeClass.Contains(TEXT("AddDelegate"))) return TEXT("delegateAdd");
            if (NodeClass.Contains(TEXT("RemoveDelegate"))) return TEXT("delegateRemove");
            if (NodeClass.Contains(TEXT("CallDelegate"))) return TEXT("delegateCall");
            if (NodeClass.Contains(TEXT("AssignDelegate"))) return TEXT("delegateAssign");
            if (NodeClass.Contains(TEXT("ClearDelegate"))) return TEXT("delegateClear");
            if (NodeClass.Contains(TEXT("CreateDelegate"))) return TEXT("delegateCreate");
            return TEXT("referencesDelegate");
        }
        if (Kind == TEXT("property"))
        {
            return NodeClass.Contains(TEXT("VariableSet")) ? TEXT("writesProperty") : TEXT("readsProperty");
        }
        if (Kind == TEXT("macro")) return TEXT("expandsMacro");
        if (Kind == TEXT("cast")) return TEXT("castsTo");
        if (Kind == TEXT("collapsedGraph")) return TEXT("entersCollapsedGraph");
        if (Kind == TEXT("timeline")) return TEXT("usesTimeline");
        return TEXT("referencesSymbol");
    }

    FString CanonicalMemberOwner(FString Owner)
    {
        Owner.ReplaceInline(TEXT(".SKEL_"), TEXT("."));
        Owner.ReplaceInline(TEXT(".REINST_"), TEXT("."));
        return Owner;
    }

    void AddMemberReference(
        const TSharedRef<FJsonObject>& Member,
        const TSharedRef<FJsonObject>& Node,
        const FString& SourcePackage,
        const FString& BlueprintNodeId,
        const FString& Pointer,
        const FString& SemanticFile,
        FAccumulator& Graph)
    {
        const FString Kind = StringField(Member, TEXT("kind"));
        const FString NodeClass = StringField(Node, TEXT("class"));
        const FString NodeId = StringField(Node, TEXT("id"));
        const FString Title = StringField(Node, TEXT("title"));
        FString Target;
        FString Label;
        if (Kind == TEXT("macro"))
        {
            const FString MacroGraph = StringField(Member, TEXT("graph"));
            if (!MacroGraph.IsEmpty())
            {
                Target = TEXT("graph:") + MacroGraph;
                Graph.AddNode(Target, TEXT("graph"), MacroGraph, PackageFromObjectPath(MacroGraph), TEXT("Macro"));
            }
            const FString BlueprintPath = StringField(Member, TEXT("blueprint"));
            const FString PackageName = PackageFromObjectPath(BlueprintPath);
            if (Target.IsEmpty() && !PackageName.IsEmpty())
            {
                Target = Graph.AddPackageTarget(PackageName);
                Label = BlueprintPath;
            }
        }
        else if (Kind == TEXT("cast"))
        {
            Label = StringField(Member, TEXT("targetType"));
            if (!Label.IsEmpty()) Target = Graph.AddTypeTarget(Label);
        }
        else if (Kind == TEXT("collapsedGraph"))
        {
            Label = StringField(Member, TEXT("graph"));
            Target = TEXT("graph:") + Label;
            Graph.AddNode(Target, TEXT("graph"), Label, SourcePackage, TEXT("CollapsedGraph"));
        }
        else if (Kind == TEXT("timeline"))
        {
            Label = StringField(Member, TEXT("name"));
            Target = SymbolNodeId(TEXT("timeline"), SourcePackage + TEXT("#") + Label);
            Graph.AddNode(Target, TEXT("symbol"), Label, SourcePackage, TEXT("timeline"));
        }
        else
        {
            const FString Owner = CanonicalMemberOwner(StringField(Member, TEXT("owner")));
            FString Name = StringField(Member, TEXT("name"));
            if ((Name.IsEmpty() || Name == TEXT("None")) && Kind == TEXT("event"))
            {
                Name = Title.IsEmpty() ? NodeId : Title;
                int32 NewlineIndex = INDEX_NONE;
                if (Name.FindChar(TEXT('\n'), NewlineIndex)) Name.LeftInline(NewlineIndex);
            }
            if (Name.IsEmpty()) return;
            Label = Owner.IsEmpty() ? Name : Owner + TEXT("::") + Name;
            const FString SymbolKind = Kind == TEXT("event") ? TEXT("function") : Kind;
            Target = SymbolNodeId(SymbolKind, Owner + TEXT("#") + Name);
            Graph.AddNode(Target, TEXT("symbol"), Label, FString(), SymbolKind);
        }
        if (!Target.IsEmpty())
        {
            Graph.AddEdge(
                BlueprintNodeId,
                Target,
                MemberRelation(Kind, NodeClass),
                Kind == TEXT("event") && StringField(Member, TEXT("name")) == TEXT("None") ? 0.8 : 1.0,
                SemanticFile,
                Pointer,
                Kind,
                NodeId,
                Title);
        }
    }

    void AddSidecar(
        const FString& SourcePackage,
        const FString& SemanticFile,
        const TSharedRef<FJsonObject>& Root,
        FAccumulator& Graph)
    {
        const TSharedPtr<FJsonObject>* SemanticsPtr = nullptr;
        if (!Root->TryGetObjectField(TEXT("semantics"), SemanticsPtr) || SemanticsPtr == nullptr) return;
        const TSharedRef<FJsonObject> Semantics = (*SemanticsPtr).ToSharedRef();
        const FString AssetId = AssetNodeId(SourcePackage);

        FString ParentClass;
        if (Semantics->TryGetStringField(TEXT("parentClass"), ParentClass) && !ParentClass.IsEmpty())
        {
            Graph.AddEdge(
                AssetId,
                Graph.AddTypeTarget(ParentClass),
                TEXT("inheritsFrom"),
                1.0,
                SemanticFile,
                TEXT("/semantics/parentClass"));
        }
        const TArray<TSharedPtr<FJsonValue>>* Interfaces = nullptr;
        if (Semantics->TryGetArrayField(TEXT("interfaces"), Interfaces) && Interfaces != nullptr)
        {
            for (int32 Index = 0; Index < Interfaces->Num(); ++Index)
            {
                FString Interface;
                if ((*Interfaces)[Index].IsValid() && (*Interfaces)[Index]->TryGetString(Interface))
                {
                    Graph.AddEdge(
                        AssetId,
                        Graph.AddTypeTarget(Interface),
                        TEXT("implements"),
                        1.0,
                        SemanticFile,
                        TEXT("/semantics/interfaces/") + FString::FromInt(Index));
                }
            }
        }

        FString SourceClass;
        Semantics->TryGetStringField(TEXT("class"), SourceClass);
        const bool bMaterialGraph = AddMaterialGraph(
            Semantics,
            SourcePackage,
            SemanticFile,
            Graph);
        TArray<TPair<FString, TSharedPtr<FJsonValue>>> SemanticFields;
        for (const auto& Pair : Semantics->Values)
        {
            const FString Key(Pair.Key);
            if (Key != TEXT("graphs") && Key != TEXT("domain")
                && !(bMaterialGraph && (Key == TEXT("nodes") || Key == TEXT("connections"))))
            {
                SemanticFields.Emplace(Key, Pair.Value);
            }
        }
        SemanticFields.Sort([](const auto& Left, const auto& Right) { return Left.Key < Right.Key; });
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : SemanticFields)
        {
            AddSemanticReferences(
                Field.Value,
                TEXT("/semantics/") + EscapePointerToken(Field.Key),
                SourcePackage,
                SemanticFile,
                Graph,
                Field.Key,
                SourceClass);
        }

        const TSharedPtr<FJsonObject>* Domain = nullptr;
        if (Semantics->TryGetObjectField(TEXT("domain"), Domain) && Domain != nullptr)
        {
            const TSharedPtr<FJsonObject>* Projections = nullptr;
            if ((*Domain)->TryGetObjectField(TEXT("projections"), Projections) && Projections != nullptr)
            {
                TArray<TPair<FString, TSharedPtr<FJsonValue>>> SortedProjections;
                for (const auto& Pair : (*Projections)->Values)
                {
                    SortedProjections.Emplace(FString(Pair.Key), Pair.Value);
                }
                SortedProjections.Sort([](const auto& Left, const auto& Right) { return Left.Key < Right.Key; });
                for (const TPair<FString, TSharedPtr<FJsonValue>>& DomainPair : SortedProjections)
                {
                    const FString& DomainName = DomainPair.Key;
                    const TSharedPtr<FJsonObject> Projection = DomainPair.Value.IsValid()
                        ? DomainPair.Value->AsObject() : nullptr;
                    FString Role;
                    if (Projection.IsValid()) Projection->TryGetStringField(TEXT("role"), Role);
                    const FString DomainNodeId = TEXT("domain:") + DomainName + TEXT(":") + Role;
                    Graph.AddNode(DomainNodeId, TEXT("domainRole"), DomainName + TEXT(":") + Role, FString(), Role);
                    Graph.AddEdge(
                        AssetId,
                        DomainNodeId,
                        TEXT("hasDomainRole"),
                        1.0,
                        SemanticFile,
                        TEXT("/semantics/domain/projections/") + EscapePointerToken(DomainName));
                    if (Projection.IsValid())
                    {
                        AddSemanticReferences(
                            DomainPair.Value,
                            TEXT("/semantics/domain/projections/") + EscapePointerToken(DomainName),
                            SourcePackage,
                            SemanticFile,
                            Graph,
                            FString(),
                            SourceClass,
                            DomainName,
                            Role);
                    }
                }
            }
            const TArray<TSharedPtr<FJsonValue>>* OwnedObjects = nullptr;
            if ((*Domain)->TryGetArrayField(TEXT("ownedObjects"), OwnedObjects) && OwnedObjects != nullptr)
            {
                AddSemanticReferences(
                    MakeShared<FJsonValueArray>(*OwnedObjects),
                    TEXT("/semantics/domain/ownedObjects"),
                    SourcePackage,
                    SemanticFile,
                    Graph,
                    FString(),
                    SourceClass);
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
        if (!Semantics->TryGetArrayField(TEXT("graphs"), Graphs) || Graphs == nullptr) return;
        for (int32 GraphIndex = 0; GraphIndex < Graphs->Num(); ++GraphIndex)
        {
            const TSharedPtr<FJsonObject>* BlueprintGraphPtr = nullptr;
            if (!(*Graphs)[GraphIndex].IsValid()
                || !(*Graphs)[GraphIndex]->TryGetObject(BlueprintGraphPtr)
                || BlueprintGraphPtr == nullptr)
            {
                continue;
            }
            const TSharedRef<FJsonObject> BlueprintGraph = (*BlueprintGraphPtr).ToSharedRef();
            const FString GraphName = StringField(BlueprintGraph, TEXT("name"));
            FString GraphPath = StringField(BlueprintGraph, TEXT("graphPath"));
            if (GraphPath.IsEmpty()) GraphPath = SourcePackage + TEXT("#") + GraphName;
            const FString GraphId = TEXT("graph:") + GraphPath;
            const FString GraphPointer = TEXT("/semantics/graphs/") + FString::FromInt(GraphIndex);
            Graph.AddNode(
                GraphId,
                TEXT("graph"),
                GraphName,
                SourcePackage,
                StringField(BlueprintGraph, TEXT("graphKind")));
            Graph.AddEdge(AssetId, GraphId, TEXT("containsGraph"), 1.0, SemanticFile, GraphPointer);

            const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
            if (!BlueprintGraph->TryGetArrayField(TEXT("nodes"), Nodes) || Nodes == nullptr) continue;
            TMap<FString, FString> PinOwners;
            TMap<FString, FString> PinCategories;
            TMap<FString, FString> PinNames;
            for (int32 NodeIndex = 0; NodeIndex < Nodes->Num(); ++NodeIndex)
            {
                const TSharedPtr<FJsonObject>* NodePtr = nullptr;
                if (!(*Nodes)[NodeIndex].IsValid()
                    || !(*Nodes)[NodeIndex]->TryGetObject(NodePtr)
                    || NodePtr == nullptr)
                {
                    continue;
                }
                const TSharedRef<FJsonObject> Node = (*NodePtr).ToSharedRef();
                const FString RawNodeId = StringField(Node, TEXT("id"));
                const FString BlueprintNodeId = TEXT("bpnode:") + GraphPath + TEXT("#") + RawNodeId;
                const FString NodePointer = GraphPointer + TEXT("/nodes/") + FString::FromInt(NodeIndex);
                Graph.AddNode(
                    BlueprintNodeId,
                    TEXT("blueprintNode"),
                    StringField(Node, TEXT("title")),
                    SourcePackage,
                    StringField(Node, TEXT("class")));
                const TSharedPtr<FJsonObject> ProjectNode = Graph.Nodes.FindRef(BlueprintNodeId);
                if (ProjectNode.IsValid())
                {
                    ProjectNode->SetStringField(TEXT("graphId"), GraphId);
                    ProjectNode->SetStringField(TEXT("rawNodeId"), RawNodeId);
                }
                Graph.AddEdge(
                    GraphId,
                    BlueprintNodeId,
                    TEXT("containsNode"),
                    1.0,
                    SemanticFile,
                    NodePointer);

                const TArray<TSharedPtr<FJsonValue>>* Pins = nullptr;
                if (Node->TryGetArrayField(TEXT("pins"), Pins) && Pins != nullptr)
                {
                    for (const TSharedPtr<FJsonValue>& PinValue : *Pins)
                    {
                        const TSharedPtr<FJsonObject>* PinPtr = nullptr;
                        if (!PinValue.IsValid() || !PinValue->TryGetObject(PinPtr) || PinPtr == nullptr) continue;
                        const TSharedRef<FJsonObject> Pin = (*PinPtr).ToSharedRef();
                        const FString PinId = StringField(Pin, TEXT("id"));
                        PinOwners.Add(PinId, BlueprintNodeId);
                        PinNames.Add(PinId, StringField(Pin, TEXT("name")));
                        const TSharedPtr<FJsonObject>* Type = nullptr;
                        FString Category;
                        if (Pin->TryGetObjectField(TEXT("type"), Type) && Type != nullptr)
                        {
                            (*Type)->TryGetStringField(TEXT("category"), Category);
                        }
                        PinCategories.Add(PinId, Category);
                    }
                }
                const TSharedPtr<FJsonObject>* Member = nullptr;
                if (Node->TryGetObjectField(TEXT("memberReference"), Member) && Member != nullptr)
                {
                    AddMemberReference(
                        (*Member).ToSharedRef(),
                        Node,
                        SourcePackage,
                        BlueprintNodeId,
                        NodePointer + TEXT("/memberReference"),
                        SemanticFile,
                        Graph);
                }
            }

            const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
            if (BlueprintGraph->TryGetArrayField(TEXT("links"), Links) && Links != nullptr)
            {
                const FString SemanticKind = StringField(Semantics, TEXT("kind"));
                for (int32 LinkIndex = 0; LinkIndex < Links->Num(); ++LinkIndex)
                {
                    const TSharedPtr<FJsonObject>* LinkPtr = nullptr;
                    if (!(*Links)[LinkIndex].IsValid()
                        || !(*Links)[LinkIndex]->TryGetObject(LinkPtr)
                        || LinkPtr == nullptr)
                    {
                        continue;
                    }
                    const TSharedRef<FJsonObject> Link = (*LinkPtr).ToSharedRef();
                    const FString FromPin = StringField(Link, TEXT("fromPin"));
                    const FString ToPin = StringField(Link, TEXT("toPin"));
                    const FString* FromNode = PinOwners.Find(FromPin);
                    const FString* ToNode = PinOwners.Find(ToPin);
                    if (FromNode == nullptr || ToNode == nullptr) continue;
                    const FString FromCategory = PinCategories.FindRef(FromPin);
                    const FString ToCategory = PinCategories.FindRef(ToPin);
                    const FString Relation = FromCategory == TEXT("exec") || ToCategory == TEXT("exec")
                        ? TEXT("execFlow")
                        : SemanticKind == TEXT("ControlRigBlueprint")
                            ? TEXT("rigValueFlow") : TEXT("dataFlow");
                    Graph.AddEdge(
                        *FromNode,
                        *ToNode,
                        Relation,
                        1.0,
                        SemanticFile,
                        GraphPointer + TEXT("/links/") + FString::FromInt(LinkIndex),
                        PinNames.FindRef(FromPin) + TEXT("->") + PinNames.FindRef(ToPin),
                        FString(),
                        FString(),
                        FromPin,
                        ToPin);
                }
            }
        }
    }
}

namespace UERingProjectGraphBuilder
{
    void AddContributions(
        const TSharedRef<FJsonObject>& ProjectIndex,
        const TSharedRef<FJsonObject>& DependencyGraph,
        const TSet<FString>* ContributorFilter,
        FAccumulator& Graph)
    {
        const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
        if (ProjectIndex->TryGetArrayField(TEXT("assets"), Assets) && Assets != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Assets)
            {
                const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(EntryPtr) || EntryPtr == nullptr) continue;
                const FString PackageName = StringField((*EntryPtr).ToSharedRef(), TEXT("packageName"));
                if (!PackageName.IsEmpty()) Graph.ProjectPackages.Add(PackageName, PackageName);
            }
            for (const TSharedPtr<FJsonValue>& Value : *Assets)
            {
                const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(EntryPtr) || EntryPtr == nullptr) continue;
                const TSharedRef<FJsonObject> Entry = (*EntryPtr).ToSharedRef();
                const FString PackageName = StringField(Entry, TEXT("packageName"));
                if (ContributorFilter != nullptr && !ContributorFilter->Contains(PackageName)) continue;
                const FString AssetId = AssetNodeId(PackageName);
                Graph.AddNode(
                    AssetId,
                    TEXT("asset"),
                    FPackageName::GetLongPackageAssetName(PackageName),
                    PackageName,
                    StringField(Entry, TEXT("semanticKind")));
                const TSharedPtr<FJsonObject> Node = Graph.Nodes.FindRef(AssetId);
                if (Node.IsValid())
                {
                    Node->SetStringField(TEXT("assetClass"), StringField(Entry, TEXT("assetClass")));
                    Node->SetStringField(TEXT("semanticFile"), StringField(Entry, TEXT("semanticFile")));
                    const TArray<TSharedPtr<FJsonValue>>* Domains = nullptr;
                    if (Entry->TryGetArrayField(TEXT("domains"), Domains)
                        && Domains != nullptr && !Domains->IsEmpty())
                    {
                        Node->SetArrayField(TEXT("domains"), *Domains);
                    }
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* Dependencies = nullptr;
        if (DependencyGraph->TryGetArrayField(TEXT("edges"), Dependencies) && Dependencies != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Dependencies)
            {
                const TSharedPtr<FJsonObject>* EdgePtr = nullptr;
                if (!Value.IsValid() || !Value->TryGetObject(EdgePtr) || EdgePtr == nullptr) continue;
                const TSharedRef<FJsonObject> Edge = (*EdgePtr).ToSharedRef();
                const FString From = StringField(Edge, TEXT("from"));
                if (ContributorFilter != nullptr && !ContributorFilter->Contains(From)) continue;
                Graph.ActiveContributorPackage = From;
                Graph.AddEdge(
                    AssetNodeId(From),
                    Graph.AddPackageTarget(StringField(Edge, TEXT("to"))),
                    TEXT("dependsOn"),
                    1.0,
                    TEXT("assetRegistry"),
                    FString(),
                    StringField(Edge, TEXT("type")));
            }
        }

        if (Assets == nullptr) return;
        for (const TSharedPtr<FJsonValue>& Value : *Assets)
        {
            const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(EntryPtr) || EntryPtr == nullptr) continue;
            const TSharedRef<FJsonObject> Entry = (*EntryPtr).ToSharedRef();
            const FString PackageName = StringField(Entry, TEXT("packageName"));
            if (ContributorFilter != nullptr && !ContributorFilter->Contains(PackageName)) continue;
            const FString SemanticFile = StringField(Entry, TEXT("semanticFile"));
            if (PackageName.IsEmpty() || SemanticFile.IsEmpty()) continue;
            Graph.ActiveContributorPackage = PackageName;
            const FString AbsoluteSemanticFile = FPaths::ConvertRelativePathToFull(
                FPaths::Combine(FPaths::ProjectDir(), SemanticFile));
            TSharedPtr<FJsonObject> Root;
            if (ReadJson(AbsoluteSemanticFile, Root))
            {
                AddSidecar(PackageName, SemanticFile, Root.ToSharedRef(), Graph);
            }
        }
    }

    TSharedRef<FJsonObject> BuildRoot(
        const TSharedRef<FJsonObject>& ProjectIndex,
        const FAccumulator& Graph)
    {
        const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("schema"), GraphSchema);
        Root->SetStringField(TEXT("schemaVersion"), GraphVersion);
        Root->SetStringField(TEXT("usemSchemaVersion"), UE_RING_SCHEMA_VERSION);
        Root->SetStringField(TEXT("generatedAtUtc"), StringField(ProjectIndex, TEXT("generatedAtUtc")));

        TMap<FString, int32> NodesByKind;
        TArray<FString> NodeIds;
        Graph.Nodes.GetKeys(NodeIds);
        NodeIds.Sort([](const FString& Left, const FString& Right)
        {
            return StableTextCompare(Left, Right) < 0;
        });
        TArray<TSharedPtr<FJsonValue>> JsonNodes;
        JsonNodes.Reserve(NodeIds.Num());
        for (const FString& NodeId : NodeIds)
        {
            const TSharedPtr<FJsonObject> Node = Graph.Nodes.FindChecked(NodeId);
            NodesByKind.FindOrAdd(StringField(Node.ToSharedRef(), TEXT("kind")))++;
            JsonNodes.Add(MakeShared<FJsonValueObject>(Node.ToSharedRef()));
        }

        TMap<FString, int32> EdgesByRelation;
        TArray<FString> EdgeKeys;
        Graph.Edges.GetKeys(EdgeKeys);
        EdgeKeys.Sort();
        TArray<TSharedPtr<FJsonValue>> JsonEdges;
        JsonEdges.Reserve(EdgeKeys.Num());
        for (const FString& EdgeKey : EdgeKeys)
        {
            const TSharedPtr<FJsonObject> Edge = Graph.Edges.FindChecked(EdgeKey);
            EdgesByRelation.FindOrAdd(StringField(Edge.ToSharedRef(), TEXT("relation")))++;
            JsonEdges.Add(MakeShared<FJsonValueObject>(Edge.ToSharedRef()));
        }
        Root->SetArrayField(TEXT("nodes"), JsonNodes);
        Root->SetArrayField(TEXT("edges"), JsonEdges);

        const TSharedRef<FJsonObject> Statistics = MakeShared<FJsonObject>();
        Statistics->SetNumberField(TEXT("nodeCount"), JsonNodes.Num());
        Statistics->SetNumberField(TEXT("edgeCount"), JsonEdges.Num());
        const TSharedRef<FJsonObject> NodeStats = MakeShared<FJsonObject>();
        TArray<FString> NodeKinds;
        NodesByKind.GetKeys(NodeKinds);
        NodeKinds.Sort();
        for (const FString& Kind : NodeKinds) NodeStats->SetNumberField(Kind, NodesByKind.FindChecked(Kind));
        const TSharedRef<FJsonObject> EdgeStats = MakeShared<FJsonObject>();
        TArray<FString> EdgeRelations;
        EdgesByRelation.GetKeys(EdgeRelations);
        EdgeRelations.Sort();
        for (const FString& Relation : EdgeRelations)
        {
            EdgeStats->SetNumberField(Relation, EdgesByRelation.FindChecked(Relation));
        }
        Statistics->SetObjectField(TEXT("nodesByKind"), NodeStats);
        Statistics->SetObjectField(TEXT("edgesByRelation"), EdgeStats);
        Root->SetObjectField(TEXT("statistics"), Statistics);
        return Root;
    }

    bool LoadAccumulator(const FString& Filename, FAccumulator& Graph)
    {
        TSharedPtr<FJsonObject> Root;
        FString Schema;
        FString Version;
        FString UsemVersion;
        if (!ReadJson(Filename, Root)
            || !Root->TryGetStringField(TEXT("schema"), Schema) || Schema != GraphSchema
            || !Root->TryGetStringField(TEXT("schemaVersion"), Version) || Version != GraphVersion
            || !Root->TryGetStringField(TEXT("usemSchemaVersion"), UsemVersion)
            || UsemVersion != UE_RING_SCHEMA_VERSION)
        {
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Edges = nullptr;
        if (!Root->TryGetArrayField(TEXT("nodes"), Nodes) || Nodes == nullptr
            || !Root->TryGetArrayField(TEXT("edges"), Edges) || Edges == nullptr)
        {
            return false;
        }
        for (const TSharedPtr<FJsonValue>& Value : *Nodes)
        {
            const TSharedPtr<FJsonObject>* Node = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Node) || Node == nullptr) return false;
            const FString Id = StringField((*Node).ToSharedRef(), TEXT("id"));
            if (Id.IsEmpty()) return false;
            Graph.Nodes.Add(Id, *Node);
        }
        for (const TSharedPtr<FJsonValue>& Value : *Edges)
        {
            const TSharedPtr<FJsonObject>* Edge = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Edge) || Edge == nullptr
                || StringField((*Edge).ToSharedRef(), TEXT("contributorPackage")).IsEmpty())
            {
                return false;
            }
            Graph.Edges.Add(FAccumulator::EdgeKey((*Edge).ToSharedRef()), *Edge);
        }
        return true;
    }
}

FString FUERingProjectGraphBuilder::GetGraphFile(const FString& IndexDirectory)
{
    return FPaths::Combine(IndexDirectory, TEXT("project.uesem.graph.json"));
}

bool FUERingProjectGraphBuilder::Rebuild(
    const FString& IndexDirectory,
    const TSharedRef<FJsonObject>& ProjectIndex,
    const TSharedRef<FJsonObject>& DependencyGraph,
    TSharedPtr<FJsonObject>& OutGraph,
    FString& OutError)
{
    using namespace UERingProjectGraphBuilder;

    FAccumulator Graph;
    AddContributions(ProjectIndex, DependencyGraph, nullptr, Graph);
    const TSharedRef<FJsonObject> Root = BuildRoot(ProjectIndex, Graph);
    if (!WriteJson(GetGraphFile(IndexDirectory), Root, OutError)) return false;
    OutGraph = Root;
    return true;
}

bool FUERingProjectGraphBuilder::Update(
    const FString& IndexDirectory,
    const TSharedRef<FJsonObject>& ProjectIndex,
    const TSharedRef<FJsonObject>& DependencyGraph,
    const TSet<FString>& ContributorPackages,
    TSharedPtr<FJsonObject>& OutGraph,
    FString& OutError)
{
    using namespace UERingProjectGraphBuilder;

    FAccumulator Graph;
    if (!LoadAccumulator(GetGraphFile(IndexDirectory), Graph))
    {
        return Rebuild(IndexDirectory, ProjectIndex, DependencyGraph, OutGraph, OutError);
    }

    const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
    if (ProjectIndex->TryGetArrayField(TEXT("assets"), Assets) && Assets != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Assets)
        {
            const TSharedPtr<FJsonObject>* Entry = nullptr;
            if (Value.IsValid() && Value->TryGetObject(Entry) && Entry != nullptr)
            {
                const FString PackageName = StringField((*Entry).ToSharedRef(), TEXT("packageName"));
                if (!PackageName.IsEmpty()) Graph.ProjectPackages.Add(PackageName, PackageName);
            }
        }
    }

    for (auto It = Graph.Edges.CreateIterator(); It; ++It)
    {
        if (ContributorPackages.Contains(StringField(It.Value().ToSharedRef(), TEXT("contributorPackage"))))
        {
            It.RemoveCurrent();
        }
    }
    for (auto It = Graph.Nodes.CreateIterator(); It; ++It)
    {
        if (ContributorPackages.Contains(StringField(It.Value().ToSharedRef(), TEXT("packageName"))))
        {
            It.RemoveCurrent();
        }
    }

    AddContributions(ProjectIndex, DependencyGraph, &ContributorPackages, Graph);

    TSet<FString> ReferencedNodes;
    for (const auto& Pair : Graph.Edges)
    {
        ReferencedNodes.Add(StringField(Pair.Value.ToSharedRef(), TEXT("from")));
        ReferencedNodes.Add(StringField(Pair.Value.ToSharedRef(), TEXT("to")));
    }
    for (const auto& Pair : Graph.ProjectPackages)
    {
        ReferencedNodes.Add(AssetNodeId(Pair.Key));
    }
    for (auto It = Graph.Nodes.CreateIterator(); It; ++It)
    {
        if (!ReferencedNodes.Contains(It.Key())) It.RemoveCurrent();
    }
    for (const auto& Pair : Graph.Edges)
    {
        const TSharedRef<FJsonObject> Edge = Pair.Value.ToSharedRef();
        if (!Graph.Nodes.Contains(StringField(Edge, TEXT("from")))
            || !Graph.Nodes.Contains(StringField(Edge, TEXT("to"))))
        {
            // Membership changes can invalidate edges contributed by an unreported referencer.
            return Rebuild(IndexDirectory, ProjectIndex, DependencyGraph, OutGraph, OutError);
        }
    }

    const TSharedRef<FJsonObject> Root = BuildRoot(ProjectIndex, Graph);
    if (!WriteJson(GetGraphFile(IndexDirectory), Root, OutError)) return false;
    OutGraph = Root;
    return true;
}

bool FUERingProjectGraphBuilder::BuildContributions(
    const TSharedRef<FJsonObject>& ProjectIndex,
    const TSharedRef<FJsonObject>& DependencyGraph,
    const TSet<FString>& ContributorPackages,
    TSharedPtr<FJsonObject>& OutGraph)
{
    using namespace UERingProjectGraphBuilder;

    FAccumulator Graph;
    AddContributions(ProjectIndex, DependencyGraph, &ContributorPackages, Graph);
    OutGraph = BuildRoot(ProjectIndex, Graph);
    return true;
}

bool FUERingProjectGraphBuilder::MaterializeFromSqlite(
    const FString& IndexDirectory,
    const TSharedRef<FJsonObject>& ProjectIndex,
    TSharedPtr<FJsonObject>& OutGraph,
    FString& OutError)
{
    using namespace UERingProjectGraphBuilder;

    const FString DatabaseFile = FPaths::Combine(IndexDirectory, TEXT("project.uesem.sqlite"));
    FSQLiteDatabase Database;
    int32 UserVersion = 0;
    if (!Database.Open(*DatabaseFile, ESQLiteDatabaseOpenMode::ReadOnly)
        || !Database.GetUserVersion(UserVersion)
        || UserVersion != 6)
    {
        OutError = FString::Printf(TEXT("Could not open compatible SQLite graph source: %s"), *DatabaseFile);
        Database.Close();
        return false;
    }

    TMap<FString, TSharedPtr<FJsonObject>> EntriesByPackage;
    const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
    if (ProjectIndex->TryGetArrayField(TEXT("assets"), Assets) && Assets != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Assets)
        {
            const TSharedPtr<FJsonObject>* Entry = nullptr;
            FString PackageName;
            if (Value.IsValid() && Value->TryGetObject(Entry) && Entry != nullptr
                && (*Entry)->TryGetStringField(TEXT("packageName"), PackageName))
            {
                EntriesByPackage.Add(PackageName, *Entry);
            }
        }
    }

    const FString GraphFile = GetGraphFile(IndexDirectory);
    const FString TempFile = GraphFile + TEXT(".tmp");
    IFileManager::Get().Delete(*TempFile, false, true);
    TUniquePtr<FArchive> Archive(IFileManager::Get().CreateFileWriter(*TempFile));
    if (!Archive.IsValid())
    {
        OutError = FString::Printf(TEXT("Could not create streaming project graph: %s"), *TempFile);
        Database.Close();
        return false;
    }
    const TSharedRef<TJsonWriter<UTF8CHAR, TCondensedJsonPrintPolicy<UTF8CHAR>>> Writer =
        TJsonWriterFactory<UTF8CHAR, TCondensedJsonPrintPolicy<UTF8CHAR>>::Create(Archive.Get());
    Writer->WriteObjectStart();
    Writer->WriteValue(TEXT("schema"), GraphSchema);
    Writer->WriteValue(TEXT("schemaVersion"), GraphVersion);
    Writer->WriteValue(TEXT("usemSchemaVersion"), UE_RING_SCHEMA_VERSION);
    Writer->WriteValue(TEXT("generatedAtUtc"), StringField(ProjectIndex, TEXT("generatedAtUtc")));
    Writer->WriteArrayStart(TEXT("nodes"));

    TMap<FString, int64> NodesByKind;
    FSQLitePreparedStatement NodeQuery(
        Database,
        TEXT("SELECT node_id, kind, subtype, label, package_name, graph_id, raw_node_id "
             "FROM graph_nodes ORDER BY node_id COLLATE NOCASE, node_id COLLATE BINARY"));
    if (!NodeQuery.IsValid())
    {
        OutError = FString::Printf(TEXT("Could not query SQLite graph nodes: %s"), *Database.GetLastError());
        Writer->Close();
        Archive->Close();
        Database.Close();
        IFileManager::Get().Delete(*TempFile, false, true);
        return false;
    }
    const int64 NodeRows = NodeQuery.Execute(
        [&Writer, &EntriesByPackage, &NodesByKind](const FSQLitePreparedStatement& Row)
        {
            FString Id;
            FString Kind;
            FString Subtype;
            FString Label;
            FString PackageName;
            FString GraphId;
            FString RawNodeId;
            if (!Row.GetColumnValueByIndex(0, Id)
                || !Row.GetColumnValueByIndex(1, Kind)
                || !Row.GetColumnValueByIndex(2, Subtype)
                || !Row.GetColumnValueByIndex(3, Label)
                || !Row.GetColumnValueByIndex(4, PackageName)
                || !Row.GetColumnValueByIndex(5, GraphId)
                || !Row.GetColumnValueByIndex(6, RawNodeId))
            {
                return ESQLitePreparedStatementExecuteRowResult::Error;
            }
            Writer->WriteObjectStart();
            Writer->WriteValue(TEXT("id"), Id);
            Writer->WriteValue(TEXT("kind"), Kind);
            Writer->WriteValue(TEXT("label"), Label);
            if (!PackageName.IsEmpty()) Writer->WriteValue(TEXT("packageName"), PackageName);
            if (!Subtype.IsEmpty()) Writer->WriteValue(TEXT("subtype"), Subtype);
            if (!GraphId.IsEmpty()) Writer->WriteValue(TEXT("graphId"), GraphId);
            if (!RawNodeId.IsEmpty()) Writer->WriteValue(TEXT("rawNodeId"), RawNodeId);
            if (Kind == TEXT("asset"))
            {
                const TSharedPtr<FJsonObject> Entry = EntriesByPackage.FindRef(PackageName);
                if (!Entry.IsValid())
                {
                    return ESQLitePreparedStatementExecuteRowResult::Error;
                }
                Writer->WriteValue(TEXT("assetClass"), StringField(Entry.ToSharedRef(), TEXT("assetClass")));
                Writer->WriteValue(TEXT("semanticFile"), StringField(Entry.ToSharedRef(), TEXT("semanticFile")));
                const TArray<TSharedPtr<FJsonValue>>* Domains = nullptr;
                if (Entry->TryGetArrayField(TEXT("domains"), Domains)
                    && Domains != nullptr && !Domains->IsEmpty())
                {
                    Writer->WriteArrayStart(TEXT("domains"));
                    for (const TSharedPtr<FJsonValue>& Domain : *Domains)
                    {
                        FString DomainName;
                        if (Domain.IsValid() && Domain->TryGetString(DomainName))
                        {
                            Writer->WriteValue(DomainName);
                        }
                    }
                    Writer->WriteArrayEnd();
                }
            }
            Writer->WriteObjectEnd();
            NodesByKind.FindOrAdd(Kind)++;
            return ESQLitePreparedStatementExecuteRowResult::Continue;
        });
    NodeQuery.Destroy();
    if (NodeRows < 0)
    {
        OutError = FString::Printf(TEXT("Could not read SQLite graph nodes: %s"), *Database.GetLastError());
        Writer->Close();
        Archive->Close();
        Database.Close();
        IFileManager::Get().Delete(*TempFile, false, true);
        return false;
    }
    Writer->WriteArrayEnd();
    Writer->WriteArrayStart(TEXT("edges"));

    TMap<FString, int64> EdgesByRelation;
    FSQLitePreparedStatement EdgeQuery(
        Database,
        TEXT("SELECT source_node, target_node, relation, confidence, evidence_source, "
             "evidence_pointer, qualifier, source_node_id, source_title, source_pin_id, "
             "target_pin_id, contributor_package FROM graph_edges "
             "ORDER BY source_node COLLATE NOCASE, relation COLLATE NOCASE, target_node COLLATE NOCASE, "
             "qualifier COLLATE NOCASE, source_node_id COLLATE NOCASE, source_pin_id COLLATE NOCASE, "
             "target_pin_id COLLATE NOCASE"));
    if (!EdgeQuery.IsValid())
    {
        OutError = FString::Printf(TEXT("Could not query SQLite graph edges: %s"), *Database.GetLastError());
        Writer->Close();
        Archive->Close();
        Database.Close();
        IFileManager::Get().Delete(*TempFile, false, true);
        return false;
    }
    const int64 EdgeRows = EdgeQuery.Execute(
        [&Writer, &EdgesByRelation](const FSQLitePreparedStatement& Row)
        {
            FString From;
            FString To;
            FString Relation;
            double Confidence = 0.0;
            FString EvidenceSource;
            FString EvidencePointer;
            FString Qualifier;
            FString SourceNodeId;
            FString SourceTitle;
            FString SourcePinId;
            FString TargetPinId;
            FString ContributorPackage;
            if (!Row.GetColumnValueByIndex(0, From)
                || !Row.GetColumnValueByIndex(1, To)
                || !Row.GetColumnValueByIndex(2, Relation)
                || !Row.GetColumnValueByIndex(3, Confidence)
                || !Row.GetColumnValueByIndex(4, EvidenceSource)
                || !Row.GetColumnValueByIndex(5, EvidencePointer)
                || !Row.GetColumnValueByIndex(6, Qualifier)
                || !Row.GetColumnValueByIndex(7, SourceNodeId)
                || !Row.GetColumnValueByIndex(8, SourceTitle)
                || !Row.GetColumnValueByIndex(9, SourcePinId)
                || !Row.GetColumnValueByIndex(10, TargetPinId)
                || !Row.GetColumnValueByIndex(11, ContributorPackage))
            {
                return ESQLitePreparedStatementExecuteRowResult::Error;
            }
            Writer->WriteObjectStart();
            Writer->WriteValue(TEXT("from"), From);
            Writer->WriteValue(TEXT("to"), To);
            Writer->WriteValue(TEXT("relation"), Relation);
            Writer->WriteValue(TEXT("confidence"), Confidence);
            Writer->WriteValue(TEXT("evidenceSource"), EvidenceSource);
            if (!EvidencePointer.IsEmpty()) Writer->WriteValue(TEXT("evidencePointer"), EvidencePointer);
            if (!Qualifier.IsEmpty()) Writer->WriteValue(TEXT("qualifier"), Qualifier);
            if (!SourceNodeId.IsEmpty()) Writer->WriteValue(TEXT("sourceNodeId"), SourceNodeId);
            if (!SourceTitle.IsEmpty()) Writer->WriteValue(TEXT("sourceTitle"), SourceTitle);
            if (!SourcePinId.IsEmpty()) Writer->WriteValue(TEXT("sourcePinId"), SourcePinId);
            if (!TargetPinId.IsEmpty()) Writer->WriteValue(TEXT("targetPinId"), TargetPinId);
            Writer->WriteValue(TEXT("contributorPackage"), ContributorPackage);
            Writer->WriteObjectEnd();
            EdgesByRelation.FindOrAdd(Relation)++;
            return ESQLitePreparedStatementExecuteRowResult::Continue;
        });
    EdgeQuery.Destroy();
    if (EdgeRows < 0)
    {
        OutError = FString::Printf(TEXT("Could not read SQLite graph edges: %s"), *Database.GetLastError());
        Writer->Close();
        Archive->Close();
        Database.Close();
        IFileManager::Get().Delete(*TempFile, false, true);
        return false;
    }
    Writer->WriteArrayEnd();
    Writer->WriteObjectStart(TEXT("statistics"));
    Writer->WriteValue(TEXT("nodeCount"), static_cast<double>(NodeRows));
    Writer->WriteValue(TEXT("edgeCount"), static_cast<double>(EdgeRows));
    Writer->WriteObjectStart(TEXT("nodesByKind"));
    TArray<FString> NodeKinds;
    NodesByKind.GetKeys(NodeKinds);
    NodeKinds.Sort();
    for (const FString& Kind : NodeKinds)
    {
        Writer->WriteValue(Kind, static_cast<double>(NodesByKind.FindChecked(Kind)));
    }
    Writer->WriteObjectEnd();
    Writer->WriteObjectStart(TEXT("edgesByRelation"));
    TArray<FString> EdgeRelations;
    EdgesByRelation.GetKeys(EdgeRelations);
    EdgeRelations.Sort();
    for (const FString& Relation : EdgeRelations)
    {
        Writer->WriteValue(Relation, static_cast<double>(EdgesByRelation.FindChecked(Relation)));
    }
    Writer->WriteObjectEnd();
    Writer->WriteObjectEnd();
    Writer->WriteObjectEnd();
    const bool bWriterClosed = Writer->Close();
    TJsonPrintPolicy<UTF8CHAR>::WriteString(Archive.Get(), FStringView(LINE_TERMINATOR));
    const bool bArchiveClosed = Archive->Close();
    const bool bDatabaseClosed = Database.Close();
    if (!bWriterClosed || !bArchiveClosed || !bDatabaseClosed
        || !IFileManager::Get().Move(*GraphFile, *TempFile, true, true, false, true))
    {
        IFileManager::Get().Delete(*TempFile, false, true);
        OutError = FString::Printf(TEXT("Could not finalize streaming project graph: %s"), *GraphFile);
        return false;
    }
    OutGraph = MakeShared<FJsonObject>();
    OutGraph->SetStringField(TEXT("schema"), GraphSchema);
    OutGraph->SetStringField(TEXT("schemaVersion"), GraphVersion);
    return true;
}
