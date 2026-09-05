#include "UERingDomainGraphExporter.h"

#include "Algo/Unique.h"
#include "Dom/JsonValue.h"
#include "UERingPropertySerializer.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

namespace UERingDomainGraphExporter
{
    FString ClassName(const FAssetData& AssetData)
    {
        return AssetData.AssetClassPath.GetAssetName().ToString();
    }

    FString DomainKind(const FString& AssetClass)
    {
        if (AssetClass.Contains(TEXT("BehaviorTree")))
        {
            return TEXT("BehaviorTree");
        }
        if (AssetClass.Contains(TEXT("Blackboard")))
        {
            return TEXT("Blackboard");
        }
        if (AssetClass == TEXT("MaterialParameterCollection"))
        {
            return TEXT("MaterialParameterCollection");
        }
        if (AssetClass.StartsWith(TEXT("Material")))
        {
            return TEXT("MaterialGraph");
        }
        if (AssetClass.StartsWith(TEXT("Niagara")))
        {
            return TEXT("NiagaraGraph");
        }
        if (AssetClass.StartsWith(TEXT("PCG")))
        {
            return TEXT("PCGGraph");
        }
        return TEXT("OwnedObjectGraph");
    }

    bool IsDomainClass(const FString& AssetClass)
    {
        return AssetClass.Contains(TEXT("BehaviorTree"))
            || AssetClass.Contains(TEXT("Blackboard"))
            || AssetClass.StartsWith(TEXT("Material"))
            || AssetClass.StartsWith(TEXT("Niagara"))
            || AssetClass.StartsWith(TEXT("PCG"));
    }

    FString LocalPath(const UObject& Object, const UObject& Root)
    {
        return &Object == &Root ? TEXT("$root") : Object.GetPathName(&Root);
    }
}

FName FUERingDomainGraphExporter::GetName() const
{
    return TEXT("DomainGraph");
}

bool FUERingDomainGraphExporter::CanExport(const FAssetData& AssetData) const
{
    return UERingDomainGraphExporter::IsDomainClass(
        UERingDomainGraphExporter::ClassName(AssetData));
}

bool FUERingDomainGraphExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    using namespace UERingDomainGraphExporter;
    UObject* Root = Context.Asset.Get();
    if (Root == nullptr)
    {
        OutError = TEXT("The domain graph asset could not be loaded.");
        return false;
    }

    const FString AssetClass = Root->GetClass()->GetName();
    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), DomainKind(AssetClass));
    Semantics->SetStringField(TEXT("representation"), TEXT("owned-object-graph-v1"));
    Semantics->SetStringField(TEXT("assetClass"), Root->GetClass()->GetPathName());
    Semantics->SetArrayField(
        TEXT("rootProperties"),
        UERingPropertySerializer::SerializeObjectProperties(*Root));

    TArray<UObject*> OwnedObjects;
    GetObjectsWithOuter(
        Root,
        OwnedObjects,
        EGetObjectsFlags::IncludeNestedObjects,
        RF_Transient | RF_ClassDefaultObject,
        EInternalObjectFlags::Garbage);
    OwnedObjects.RemoveAll([Root](const UObject* Object)
    {
        return Object == nullptr || Object == Root || Object->GetOutermost() != Root->GetOutermost();
    });
    OwnedObjects.Sort([Root](const UObject& Left, const UObject& Right)
    {
        return LocalPath(Left, *Root) < LocalPath(Right, *Root);
    });

    TSet<const UObject*> CandidateSet;
    CandidateSet.Add(Root);
    for (const UObject* Object : OwnedObjects)
    {
        CandidateSet.Add(Object);
    }

    TSet<const UObject*> ReachableSet;
    ReachableSet.Add(Root);
    TArray<UObject*> ReachabilityQueue = { Root };
    TMap<const UObject*, TArray<UObject*>> ReferencesByObject;
    for (int32 QueueIndex = 0; QueueIndex < ReachabilityQueue.Num(); ++QueueIndex)
    {
        UObject* Object = ReachabilityQueue[QueueIndex];
        TArray<UObject*> References;
        FReferenceFinder Finder(References, Root, false, true, false, true);
        Finder.FindReferences(Object);
        References.RemoveAll([Object, &CandidateSet](const UObject* Target)
        {
            return Target == nullptr || Target == Object || !CandidateSet.Contains(Target);
        });
        References.Sort([Root](const UObject& Left, const UObject& Right)
        {
            return LocalPath(Left, *Root) < LocalPath(Right, *Root);
        });
        References.SetNum(Algo::Unique(References));
        for (UObject* Target : References)
        {
            if (!ReachableSet.Contains(Target))
            {
                ReachableSet.Add(Target);
                ReachabilityQueue.Add(Target);
            }
        }
        ReferencesByObject.Add(Object, MoveTemp(References));
    }
    OwnedObjects.RemoveAll([&ReachableSet](const UObject* Object)
    {
        return !ReachableSet.Contains(Object);
    });

    TMap<FString, int32> ClassCounts;
    TArray<TSharedPtr<FJsonValue>> Objects;
    TArray<TSharedPtr<FJsonValue>> Edges;
    TSet<FString> EdgeKeys;
    TArray<UObject*> AllObjects;
    AllObjects.Add(Root);
    AllObjects.Append(OwnedObjects);
    for (int32 Index = 0; Index < AllObjects.Num(); ++Index)
    {
        UObject* Object = AllObjects[Index];
        const TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
        JsonObject->SetNumberField(TEXT("index"), Index);
        JsonObject->SetStringField(TEXT("id"), LocalPath(*Object, *Root));
        JsonObject->SetStringField(TEXT("name"), Object->GetName());
        JsonObject->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
        if (Object != Root && Object->GetOuter() != nullptr)
        {
            JsonObject->SetStringField(TEXT("outerId"), LocalPath(*Object->GetOuter(), *Root));
        }
        if (Object != Root)
        {
            const TArray<TSharedPtr<FJsonValue>> Properties =
                UERingPropertySerializer::SerializeObjectProperties(*Object);
            if (!Properties.IsEmpty()) JsonObject->SetArrayField(TEXT("properties"), Properties);
        }
        Objects.Add(MakeShared<FJsonValueObject>(JsonObject));
        ClassCounts.FindOrAdd(Object->GetClass()->GetPathName())++;

        const TArray<UObject*>* References = ReferencesByObject.Find(Object);
        if (References == nullptr) continue;
        for (const UObject* Target : *References)
        {
            const FString SourceId = LocalPath(*Object, *Root);
            const FString TargetId = LocalPath(*Target, *Root);
            const FString EdgeKey = SourceId + TEXT("\n") + TargetId;
            if (EdgeKeys.Contains(EdgeKey))
            {
                continue;
            }
            EdgeKeys.Add(EdgeKey);
            const TSharedRef<FJsonObject> Edge = MakeShared<FJsonObject>();
            Edge->SetStringField(TEXT("source"), SourceId);
            Edge->SetStringField(TEXT("target"), TargetId);
            Edge->SetStringField(TEXT("kind"), TEXT("objectReference"));
            Edges.Add(MakeShared<FJsonValueObject>(Edge));
        }
    }

    TArray<FString> ClassNames;
    ClassCounts.GetKeys(ClassNames);
    ClassNames.Sort();
    TArray<TSharedPtr<FJsonValue>> Histogram;
    for (const FString& Name : ClassNames)
    {
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("class"), Name);
        Entry->SetNumberField(TEXT("count"), ClassCounts.FindChecked(Name));
        Histogram.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Semantics->SetNumberField(TEXT("objectCount"), Objects.Num());
    Semantics->SetNumberField(TEXT("edgeCount"), Edges.Num());
    Semantics->SetArrayField(TEXT("classHistogram"), Histogram);
    Semantics->SetArrayField(TEXT("objects"), Objects);
    Semantics->SetArrayField(TEXT("edges"), Edges);
    OutPayload.Semantics = Semantics;
    return true;
}
