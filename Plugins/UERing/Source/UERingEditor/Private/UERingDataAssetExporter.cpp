#include "UERingDataAssetExporter.h"

#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "UERingOwnedObjectSerializer.h"
#include "UERingPropertySerializer.h"
#include "UObject/PropertyOptional.h"
#include "UObject/ScriptDelegates.h"
#include "UObject/UnrealType.h"

namespace UERingOwnedObjectSerializer
{
    constexpr int32 MaxReferenceDepth = 24;

    FString OwnedObjectId(const UObject& Object, const UObject& Asset)
    {
        return Object.GetPathName(&Asset);
    }

    bool IsStateTreeAsset(const UObject& Asset)
    {
        for (const UClass* Class = Asset.GetClass(); Class != nullptr; Class = Class->GetSuperClass())
        {
            if (Class->GetPathName() == TEXT("/Script/StateTreeModule.StateTree")) return true;
        }
        return false;
    }

    bool IsPersistentOwnedObject(
        const UObject* Object,
        const UObject& Asset,
        const bool bIncludeArchetypeObjects)
    {
        return Object != nullptr
            && Object != &Asset
            && Object->GetOutermost() == Asset.GetOutermost()
            && Object->IsIn(&Asset)
            && !Object->HasAnyFlags(RF_Transient | RF_ClassDefaultObject)
            && (bIncludeArchetypeObjects || !Object->HasAnyFlags(RF_ArchetypeObject));
    }

    void GatherValueReferences(
        const FProperty& Property,
        const void* Value,
        TArray<UObject*>& OutReferences,
        const int32 Depth);

    void GatherPropertyReferences(
        const FProperty& Property,
        const void* Container,
        TArray<UObject*>& OutReferences,
        const int32 Depth)
    {
        for (int32 Index = 0; Index < Property.ArrayDim; ++Index)
        {
            GatherValueReferences(
                Property,
                Property.ContainerPtrToValuePtr<void>(Container, Index),
                OutReferences,
                Depth);
        }
    }

    void GatherValueReferences(
        const FProperty& Property,
        const void* Value,
        TArray<UObject*>& OutReferences,
        const int32 Depth)
    {
        if (Depth > MaxReferenceDepth) return;
        if (const FOptionalProperty* Optional = CastField<FOptionalProperty>(&Property))
        {
            if (Optional->IsSet(Value))
            {
                GatherValueReferences(
                    *Optional->GetValueProperty(),
                    Optional->GetValuePointerForRead(Value),
                    OutReferences,
                    Depth + 1);
            }
            return;
        }
        if (const FDelegateProperty* Delegate = CastField<FDelegateProperty>(&Property))
        {
            if (const UObject* Referenced = Delegate->GetPropertyValue(Value).GetUObject())
            {
                OutReferences.AddUnique(const_cast<UObject*>(Referenced));
            }
            return;
        }
        if (const FMulticastDelegateProperty* Delegate = CastField<FMulticastDelegateProperty>(&Property))
        {
            if (const FMulticastScriptDelegate* ValueDelegate = Delegate->GetMulticastDelegate(Value))
            {
                for (UObject* Referenced : ValueDelegate->GetAllObjects())
                {
                    if (Referenced != nullptr) OutReferences.AddUnique(Referenced);
                }
            }
            return;
        }
        if (const FSoftObjectProperty* SoftObject = CastField<FSoftObjectProperty>(&Property))
        {
            if (UObject* Referenced = SoftObject->GetPropertyValue(Value).Get())
            {
                OutReferences.AddUnique(Referenced);
            }
            return;
        }
        if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(&Property))
        {
            if (UObject* Referenced = Object->GetObjectPropertyValue(Value))
            {
                OutReferences.AddUnique(Referenced);
            }
            return;
        }
        if (const FArrayProperty* Array = CastField<FArrayProperty>(&Property))
        {
            FScriptArrayHelper Helper(Array, Value);
            for (int32 Index = 0; Index < Helper.Num(); ++Index)
            {
                GatherValueReferences(*Array->Inner, Helper.GetRawPtr(Index), OutReferences, Depth + 1);
            }
            return;
        }
        if (const FSetProperty* Set = CastField<FSetProperty>(&Property))
        {
            FScriptSetHelper Helper(Set, Value);
            for (FScriptSetHelper::FIterator It(Helper); It; ++It)
            {
                GatherValueReferences(*Set->ElementProp, Helper.GetElementPtr(It), OutReferences, Depth + 1);
            }
            return;
        }
        if (const FMapProperty* Map = CastField<FMapProperty>(&Property))
        {
            FScriptMapHelper Helper(Map, Value);
            for (FScriptMapHelper::FIterator It(Helper); It; ++It)
            {
                GatherValueReferences(*Map->KeyProp, Helper.GetKeyPtr(It), OutReferences, Depth + 1);
                GatherValueReferences(*Map->ValueProp, Helper.GetValuePtr(It), OutReferences, Depth + 1);
            }
            return;
        }
        if (const FStructProperty* Struct = CastField<FStructProperty>(&Property))
        {
            if (Struct->Struct == FInstancedStruct::StaticStruct())
            {
                const FInstancedStruct& Instanced = *static_cast<const FInstancedStruct*>(Value);
                const UScriptStruct* ValueStruct = Instanced.GetScriptStruct();
                if (ValueStruct != nullptr && Instanced.GetMemory() != nullptr)
                {
                    for (TFieldIterator<FProperty> It(
                        ValueStruct, EFieldIterationFlags::IncludeSuper); It; ++It)
                    {
                        if (UERingPropertySerializer::ShouldExportProperty(**It))
                        {
                            GatherPropertyReferences(
                                **It, Instanced.GetMemory(), OutReferences, Depth + 1);
                        }
                    }
                }
                return;
            }
            if (Struct->Struct == FInstancedPropertyBag::StaticStruct())
            {
                const FInstancedPropertyBag& Bag = *static_cast<const FInstancedPropertyBag*>(Value);
                const UPropertyBag* BagStruct = Bag.GetPropertyBagStruct();
                const FConstStructView BagValue = Bag.GetValue();
                if (BagStruct != nullptr && BagValue.GetMemory() != nullptr)
                {
                    for (TFieldIterator<FProperty> It(BagStruct); It; ++It)
                    {
                        GatherPropertyReferences(**It, BagValue.GetMemory(), OutReferences, Depth + 1);
                    }
                }
                return;
            }
            for (TFieldIterator<FProperty> It(Struct->Struct, EFieldIterationFlags::IncludeSuper); It; ++It)
            {
                if (UERingPropertySerializer::ShouldExportProperty(**It)
                    && !UERingPropertySerializer::IsPrivateName((*It)->GetName()))
                {
                    GatherPropertyReferences(**It, Value, OutReferences, Depth + 1);
                }
            }
        }
    }

    void GatherObjectReferences(const UObject& Object, TArray<UObject*>& OutReferences)
    {
        for (TFieldIterator<FProperty> It(Object.GetClass(), EFieldIterationFlags::IncludeSuper); It; ++It)
        {
            if (UERingPropertySerializer::ShouldExportProperty(**It)
                && !UERingPropertySerializer::IsPrivateName((*It)->GetName()))
            {
                GatherPropertyReferences(**It, &Object, OutReferences, 0);
            }
        }
    }

    int32 OwnedObjectDepth(const UObject& Object, const UObject& Asset)
    {
        int32 Depth = 0;
        for (const UObject* Outer = Object.GetOuter(); Outer != nullptr && Outer != &Asset; Outer = Outer->GetOuter())
        {
            ++Depth;
        }
        return Depth;
    }

    TArray<UObject*> GatherReachableOwnedObjects(
        const UObject& Asset,
        const TArray<const FProperty*>* RootPropertyFilter,
        const bool bIncludeArchetypeObjects)
    {
        TSet<UObject*> Reachable;
        TArray<UObject*> Queue;
        const auto EnqueueWithOuters = [
            &Asset, &Reachable, &Queue, bIncludeArchetypeObjects](UObject* Object)
        {
            for (UObject* Current = Object;
                IsPersistentOwnedObject(Current, Asset, bIncludeArchetypeObjects);
                Current = Current->GetOuter())
            {
                if (!Reachable.Contains(Current))
                {
                    Reachable.Add(Current);
                    Queue.Add(Current);
                }
            }
        };

        TArray<UObject*> References;
        if (RootPropertyFilter != nullptr)
        {
            for (const FProperty* Property : *RootPropertyFilter)
            {
                if (Property != nullptr)
                {
                    GatherPropertyReferences(*Property, &Asset, References, 0);
                }
            }
        }
        else
        {
            GatherObjectReferences(Asset, References);
        }
        for (UObject* Reference : References) EnqueueWithOuters(Reference);
        for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
        {
            References.Reset();
            GatherObjectReferences(*Queue[QueueIndex], References);
            for (UObject* Reference : References) EnqueueWithOuters(Reference);
        }

        Queue.Sort([&Asset](const UObject& Left, const UObject& Right)
        {
            const int32 LeftDepth = OwnedObjectDepth(Left, Asset);
            const int32 RightDepth = OwnedObjectDepth(Right, Asset);
            return LeftDepth != RightDepth
                ? LeftDepth < RightDepth
                : OwnedObjectId(Left, Asset) < OwnedObjectId(Right, Asset);
        });
        return Queue;
    }

    void RewriteOwnedReferences(
        const TSharedPtr<FJsonValue>& Value,
        const TMap<FString, FString>& IdByObjectPath)
    {
        if (!Value.IsValid()) return;
        if (Value->Type == EJson::Array)
        {
            for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
            {
                RewriteOwnedReferences(Item, IdByObjectPath);
            }
            return;
        }
        if (Value->Type != EJson::Object) return;

        const TSharedPtr<FJsonObject> Object = Value->AsObject();
        FString ObjectPath;
        if (Object.IsValid() && Object->TryGetStringField(TEXT("objectPath"), ObjectPath))
        {
            if (const FString* OwnedId = IdByObjectPath.Find(ObjectPath))
            {
                Object->RemoveField(TEXT("objectPath"));
                Object->SetStringField(TEXT("ownedObjectId"), *OwnedId);
            }
        }
        if (Object.IsValid())
        {
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
            {
                RewriteOwnedReferences(Pair.Value, IdByObjectPath);
            }
        }
    }

    TArray<TSharedPtr<FJsonValue>> SerializeOwnedObjects(
        const UObject& Asset,
        TArray<TSharedPtr<FJsonValue>>& RootProperties,
        const FString& RootId,
        const TArray<const FProperty*>* RootPropertyFilter,
        const bool bIncludeArchetypeObjects)
    {
        const TArray<UObject*> OwnedObjects = GatherReachableOwnedObjects(
            Asset,
            RootPropertyFilter,
            bIncludeArchetypeObjects);
        TMap<FString, FString> IdByObjectPath;
        IdByObjectPath.Add(Asset.GetPathName(), RootId);
        for (const UObject* Object : OwnedObjects)
        {
            IdByObjectPath.Add(Object->GetPathName(), OwnedObjectId(*Object, Asset));
        }
        for (const TSharedPtr<FJsonValue>& Property : RootProperties)
        {
            RewriteOwnedReferences(Property, IdByObjectPath);
        }

        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(OwnedObjects.Num());
        for (const UObject* Object : OwnedObjects)
        {
            TArray<TSharedPtr<FJsonValue>> Properties =
                UERingPropertySerializer::SerializeObjectProperties(*Object);
            for (const TSharedPtr<FJsonValue>& Property : Properties)
            {
                RewriteOwnedReferences(Property, IdByObjectPath);
            }

            const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
            Json->SetStringField(TEXT("id"), OwnedObjectId(*Object, Asset));
            Json->SetStringField(TEXT("name"), Object->GetName());
            Json->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
            Json->SetStringField(
                TEXT("outerId"),
                Object->GetOuter() == &Asset
                    ? RootId
                    : OwnedObjectId(*Object->GetOuter(), Asset));
            Json->SetStringField(
                TEXT("creationMethod"),
                Object->HasAnyFlags(RF_DefaultSubObject)
                    ? TEXT("findDefaultSubobject")
                    : TEXT("newObject"));
            Json->SetArrayField(TEXT("properties"), MoveTemp(Properties));
            Result.Add(MakeShared<FJsonValueObject>(Json));
        }
        return Result;
    }
}

FName FUERingDataAssetExporter::GetName() const
{
    return TEXT("DataAsset");
}

bool FUERingDataAssetExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.IsInstanceOf(UDataAsset::StaticClass());
}

bool FUERingDataAssetExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    UDataAsset* DataAsset = Cast<UDataAsset>(Context.Asset.Get());
    if (DataAsset == nullptr)
    {
        OutError = TEXT("The loaded object is not a DataAsset.");
        return false;
    }

    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("DataAsset"));
    Semantics->SetStringField(TEXT("representation"), TEXT("data-asset-properties-v2"));
    Semantics->SetStringField(TEXT("class"), DataAsset->GetClass()->GetPathName());
    Semantics->SetStringField(TEXT("primaryAssetId"), DataAsset->GetPrimaryAssetId().ToString());

    TArray<TSharedPtr<FJsonValue>> Properties =
        UERingPropertySerializer::SerializeObjectProperties(*DataAsset);
    // UPrimaryDataAsset resets and regenerates this cache from authored metadata during PreSave.
    Properties.RemoveAll([](const TSharedPtr<FJsonValue>& Value)
    {
        const TSharedPtr<FJsonObject>* Property = nullptr;
        FString Name;
        return Value.IsValid()
            && Value->TryGetObject(Property)
            && Property != nullptr
            && (*Property)->TryGetStringField(TEXT("name"), Name)
            && Name == TEXT("AssetBundleData");
    });
    if (UERingOwnedObjectSerializer::IsStateTreeAsset(*DataAsset))
    {
        const TSharedRef<FJsonObject> Policy = MakeShared<FJsonObject>();
        Policy->SetStringField(TEXT("strategy"), TEXT("state-tree-editor-compile-v1"));
        Policy->SetArrayField(
            TEXT("authoredRootProperties"),
            { MakeShared<FJsonValueString>(TEXT("EditorData")) });
        TArray<TSharedPtr<FJsonValue>> DerivedProperties;
        for (const TSharedPtr<FJsonValue>& PropertyValue : Properties)
        {
            const TSharedPtr<FJsonObject> Property = PropertyValue.IsValid()
                ? PropertyValue->AsObject()
                : nullptr;
            FString Name;
            if (Property.IsValid()
                && Property->TryGetStringField(TEXT("name"), Name)
                && Name != TEXT("EditorData"))
            {
                DerivedProperties.Add(MakeShared<FJsonValueString>(Name));
            }
        }
        Policy->SetArrayField(TEXT("derivedRootProperties"), MoveTemp(DerivedProperties));
        Semantics->SetObjectField(TEXT("reconstructionPolicy"), Policy);
    }
    Semantics->SetArrayField(
        TEXT("ownedObjects"),
        UERingOwnedObjectSerializer::SerializeOwnedObjects(*DataAsset, Properties));
    Semantics->SetArrayField(TEXT("properties"), MoveTemp(Properties));
    OutPayload.Semantics = Semantics;
    return true;
}
