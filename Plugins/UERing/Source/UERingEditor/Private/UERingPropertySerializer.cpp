#include "UERingPropertySerializer.h"

#include "Internationalization/Text.h"
#include "Misc/PackageName.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "UERingSettings.h"
#include "UObject/AnsiStrProperty.h"
#include "UObject/Package.h"
#include "UObject/PropertyOptional.h"
#include "UObject/ScriptDelegates.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"
#include "UObject/Utf8StrProperty.h"

namespace UERingPropertySerializer
{
    namespace
    {
        constexpr int32 MaxDepth = 24;
        constexpr int32 MaxExportTextChars = 16 * 1024;

        FString ExportText(
            const FProperty& Property,
            const void* Value,
            const UObject* Owner)
        {
            FString Result;
            Property.ExportTextItem_Direct(
                Result,
                Value,
                nullptr,
                const_cast<UObject*>(Owner),
                PPF_None);
            return Result;
        }

        FString JsonSortKey(const TSharedPtr<FJsonValue>& Value)
        {
            FString Result;
            const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
                TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
            FJsonSerializer::Serialize(Value, TEXT(""), Writer);
            return Result;
        }

        TSharedPtr<FJsonValue> SerializeValue(
            const FProperty& Property,
            const void* Value,
            const UObject* Owner,
            const int32 Depth);

        FString EnumName(const UEnum* Enum, const int64 Value)
        {
            return Enum != nullptr ? Enum->GetNameStringByValue(Value) : FString();
        }

        TSharedPtr<FJsonValue> SerializePropertyValue(
            const FProperty& Property,
            const void* Container,
            const UObject* Owner,
            const int32 Depth)
        {
            if (Property.ArrayDim <= 1)
            {
                return SerializeValue(
                    Property,
                    Property.ContainerPtrToValuePtr<void>(Container),
                    Owner,
                    Depth);
            }
            TArray<TSharedPtr<FJsonValue>> Values;
            Values.Reserve(Property.ArrayDim);
            for (int32 Index = 0; Index < Property.ArrayDim; ++Index)
            {
                Values.Add(SerializeValue(
                    Property,
                    Property.ContainerPtrToValuePtr<void>(Container, Index),
                    Owner,
                    Depth));
            }
            return MakeShared<FJsonValueArray>(Values);
        }

        TSharedRef<FJsonObject> EnumValue(const UEnum& Enum, const int64 Value)
        {
            const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
            Json->SetStringField(TEXT("enum"), Enum.GetPathName());
            Json->SetStringField(TEXT("name"), Enum.GetAuthoredNameStringByValue(Value));
            Json->SetNumberField(TEXT("value"), static_cast<double>(Value));
            return Json;
        }

        TSharedRef<FJsonObject> ObjectReference(const UObject* Object, const FString& FallbackPath = FString())
        {
            if (const UPropertyBag* PropertyBag = Cast<UPropertyBag>(Object))
            {
                const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
                Json->SetStringField(TEXT("dynamicType"), TEXT("propertyBag"));
                Json->SetStringField(TEXT("layoutId"), PropertyBag->GetName());
                Json->SetStringField(TEXT("class"), PropertyBag->GetClass()->GetPathName());
                return Json;
            }
            const FString ObjectPath = Object != nullptr ? Object->GetPathName() : FallbackPath;
            const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
            Json->SetStringField(TEXT("objectPath"), ObjectPath);
            if (Object != nullptr)
            {
                Json->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
            }
            return Json;
        }

        TSharedPtr<FJsonValue> SerializeValue(
            const FProperty& Property,
            const void* Value,
            const UObject* Owner,
            const int32 Depth)
        {
            if (Depth > MaxDepth)
            {
                return MakeShared<FJsonValueString>(TEXT("[MAX_DEPTH]"));
            }

            if (const FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(&Property))
            {
                const bool bIsSet = OptionalProperty->IsSet(Value);
                const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
                Json->SetBoolField(TEXT("isSet"), bIsSet);
                if (bIsSet)
                {
                    Json->SetField(
                        TEXT("value"),
                        SerializeValue(
                            *OptionalProperty->GetValueProperty(),
                            OptionalProperty->GetValuePointerForRead(Value),
                            Owner,
                            Depth + 1));
                }
                return MakeShared<FJsonValueObject>(Json);
            }

            if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(&Property))
            {
                const int64 EnumNumber = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);
                return MakeShared<FJsonValueObject>(EnumValue(*EnumProperty->GetEnum(), EnumNumber));
            }
            if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(&Property))
            {
                if (const UEnum* Enum = NumericProperty->GetIntPropertyEnum())
                {
                    return MakeShared<FJsonValueObject>(EnumValue(*Enum, NumericProperty->GetSignedIntPropertyValue(Value)));
                }
                if (NumericProperty->IsFloatingPoint())
                {
                    return MakeShared<FJsonValueNumber>(NumericProperty->GetFloatingPointPropertyValue(Value));
                }
                if (NumericProperty->IsInteger())
                {
                    return MakeShared<FJsonValueNumber>(static_cast<double>(NumericProperty->GetSignedIntPropertyValue(Value)));
                }
            }
            if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(&Property))
            {
                return MakeShared<FJsonValueBoolean>(BoolProperty->GetPropertyValue(Value));
            }
            if (const FStrProperty* StringProperty = CastField<FStrProperty>(&Property))
            {
                return MakeShared<FJsonValueString>(StringProperty->GetPropertyValue(Value));
            }
            if (const FUtf8StrProperty* StringProperty = CastField<FUtf8StrProperty>(&Property))
            {
                return MakeShared<FJsonValueString>(FString(StringProperty->GetPropertyValue(Value)));
            }
            if (const FAnsiStrProperty* StringProperty = CastField<FAnsiStrProperty>(&Property))
            {
                return MakeShared<FJsonValueString>(FString(StringProperty->GetPropertyValue(Value)));
            }
            if (const FNameProperty* NameProperty = CastField<FNameProperty>(&Property))
            {
                return MakeShared<FJsonValueString>(NameProperty->GetPropertyValue(Value).ToString());
            }
            if (const FTextProperty* TextProperty = CastField<FTextProperty>(&Property))
            {
                const FText Text = TextProperty->GetPropertyValue(Value);
                const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
                const FString* Source = FTextInspector::GetSourceString(Text);
                const FString SourceString = Source != nullptr ? *Source : FString();
                const FString Namespace = FTextInspector::GetNamespace(Text).Get(FString());
                const FString Key = FTextInspector::GetKey(Text).Get(FString());
                const FString DisplayText = Text.ToString();
                if (!SourceString.IsEmpty()) Json->SetStringField(TEXT("source"), SourceString);
                if (!Namespace.IsEmpty()) Json->SetStringField(TEXT("namespace"), Namespace);
                if (!Key.IsEmpty()) Json->SetStringField(TEXT("key"), Key);
                if (DisplayText != SourceString) Json->SetStringField(TEXT("displayText"), DisplayText);
                return MakeShared<FJsonValueObject>(Json);
            }
            if (const FDelegateProperty* DelegateProperty = CastField<FDelegateProperty>(&Property))
            {
                const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
                Json->SetStringField(TEXT("delegateKind"), TEXT("single"));
                Json->SetStringField(
                    TEXT("signature"),
                    DelegateProperty->SignatureFunction != nullptr
                        ? DelegateProperty->SignatureFunction->GetPathName()
                        : FString());

                TArray<TSharedPtr<FJsonValue>> Bindings;
                const FScriptDelegate& Delegate = DelegateProperty->GetPropertyValue(Value);
                if (Delegate.IsBound())
                {
                    const TSharedRef<FJsonObject> Binding = MakeShared<FJsonObject>();
                    Binding->SetObjectField(TEXT("object"), ObjectReference(Delegate.GetUObject()));
                    Binding->SetStringField(TEXT("function"), Delegate.GetFunctionName().ToString());
                    Bindings.Add(MakeShared<FJsonValueObject>(Binding));
                }
                Json->SetArrayField(TEXT("bindings"), MoveTemp(Bindings));
                return MakeShared<FJsonValueObject>(Json);
            }
            if (const FMulticastDelegateProperty* DelegateProperty =
                CastField<FMulticastDelegateProperty>(&Property))
            {
                const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
                Json->SetStringField(TEXT("delegateKind"), TEXT("multicast"));
                Json->SetStringField(
                    TEXT("signature"),
                    DelegateProperty->SignatureFunction != nullptr
                        ? DelegateProperty->SignatureFunction->GetPathName()
                        : FString());
                Json->SetArrayField(TEXT("bindings"), {});

                const FMulticastScriptDelegate* Delegate = DelegateProperty->GetMulticastDelegate(Value);
                if (Delegate != nullptr && Delegate->IsBound())
                {
                    Json->SetNumberField(
                        TEXT("unresolvedBindingCount"),
                        FMath::Max(1, Delegate->GetAllObjects().Num()));
                    Json->SetStringField(TEXT("exportText"), ExportText(Property, Value, Owner));
                }
                return MakeShared<FJsonValueObject>(Json);
            }
            if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(&Property))
            {
                const FSoftObjectPtr SoftObject = SoftObjectProperty->GetPropertyValue(Value);
                return MakeShared<FJsonValueObject>(ObjectReference(SoftObject.Get(), SoftObject.ToSoftObjectPath().ToString()));
            }
            if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(&Property))
            {
                return MakeShared<FJsonValueObject>(ObjectReference(ObjectProperty->GetObjectPropertyValue(Value)));
            }
            if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
            {
                FScriptArrayHelper Helper(ArrayProperty, Value);
                TArray<TSharedPtr<FJsonValue>> JsonValues;
                JsonValues.Reserve(Helper.Num());
                for (int32 Index = 0; Index < Helper.Num(); ++Index)
                {
                    JsonValues.Add(SerializeValue(*ArrayProperty->Inner, Helper.GetRawPtr(Index), Owner, Depth + 1));
                }
                return MakeShared<FJsonValueArray>(JsonValues);
            }
            if (const FSetProperty* SetProperty = CastField<FSetProperty>(&Property))
            {
                FScriptSetHelper Helper(SetProperty, Value);
                TArray<TPair<FString, TSharedPtr<FJsonValue>>> SortedValues;
                for (FScriptSetHelper::FIterator It(Helper); It; ++It)
                {
                    const TSharedPtr<FJsonValue> JsonValue = SerializeValue(
                        *SetProperty->ElementProp,
                        Helper.GetElementPtr(It),
                        Owner,
                        Depth + 1);
                    SortedValues.Emplace(JsonSortKey(JsonValue), JsonValue);
                }
                SortedValues.Sort([](const auto& Left, const auto& Right) { return Left.Key < Right.Key; });
                TArray<TSharedPtr<FJsonValue>> JsonValues;
                for (const auto& Pair : SortedValues)
                {
                    JsonValues.Add(Pair.Value);
                }
                return MakeShared<FJsonValueArray>(JsonValues);
            }
            if (const FMapProperty* MapProperty = CastField<FMapProperty>(&Property))
            {
                FScriptMapHelper Helper(MapProperty, Value);
                TArray<TPair<FString, TSharedPtr<FJsonValue>>> SortedEntries;
                for (FScriptMapHelper::FIterator It(Helper); It; ++It)
                {
                    const TSharedPtr<FJsonValue> Key = SerializeValue(
                        *MapProperty->KeyProp,
                        Helper.GetKeyPtr(It),
                        Owner,
                        Depth + 1);
                    const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
                    Entry->SetField(TEXT("key"), Key);
                    Entry->SetField(
                        TEXT("value"),
                        SerializeValue(*MapProperty->ValueProp, Helper.GetValuePtr(It), Owner, Depth + 1));
                    SortedEntries.Emplace(JsonSortKey(Key), MakeShared<FJsonValueObject>(Entry));
                }
                SortedEntries.Sort([](const auto& Left, const auto& Right) { return Left.Key < Right.Key; });
                TArray<TSharedPtr<FJsonValue>> JsonEntries;
                for (const auto& Pair : SortedEntries)
                {
                    JsonEntries.Add(Pair.Value);
                }
                return MakeShared<FJsonValueArray>(JsonEntries);
            }
            if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
            {
                if (StructProperty->Struct == FInstancedStruct::StaticStruct())
                {
                    const FInstancedStruct& Instanced = *static_cast<const FInstancedStruct*>(Value);
                    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
                    Json->SetNumberField(TEXT("instancedStructVersion"), 1);
                    Json->SetBoolField(TEXT("isValid"), Instanced.IsValid());
                    const TSharedRef<FJsonObject> FieldsJson = MakeShared<FJsonObject>();
                    if (Instanced.IsValid())
                    {
                        const UScriptStruct* ValueStruct = Instanced.GetScriptStruct();
                        Json->SetStringField(TEXT("valueStruct"), ValueStruct->GetPathName());
                        TArray<const FProperty*> Fields;
                        for (TFieldIterator<FProperty> It(
                            ValueStruct, EFieldIterationFlags::IncludeSuper); It; ++It)
                        {
                            if (ShouldExportProperty(**It)) Fields.Add(*It);
                        }
                        Fields.Sort([](const FProperty& Left, const FProperty& Right)
                        {
                            return Left.GetName() < Right.GetName();
                        });
                        for (const FProperty* Field : Fields)
                        {
                            if (IsPrivateName(Field->GetName()))
                            {
                                FieldsJson->SetStringField(Field->GetName(), TEXT("[REDACTED]"));
                            }
                            else
                            {
                                FieldsJson->SetField(
                                    Field->GetName(),
                                    SerializePropertyValue(
                                        *Field,
                                        Instanced.GetMemory(),
                                        Owner,
                                        Depth + 1));
                            }
                        }
                    }
                    Json->SetObjectField(TEXT("fields"), FieldsJson);
                    return MakeShared<FJsonValueObject>(Json);
                }
                if (StructProperty->Struct == FInstancedPropertyBag::StaticStruct())
                {
                    const FInstancedPropertyBag& Bag = *static_cast<const FInstancedPropertyBag*>(Value);
                    const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
                    Json->SetNumberField(TEXT("propertyBagVersion"), 1);
                    Json->SetBoolField(TEXT("isValid"), Bag.IsValid());

                    TArray<TSharedPtr<FJsonValue>> JsonProperties;
                    const UPropertyBag* BagStruct = Bag.GetPropertyBagStruct();
                    const FConstStructView BagValue = Bag.GetValue();
                    if (BagStruct != nullptr)
                    {
                        Json->SetStringField(TEXT("layoutId"), BagStruct->GetName());
                        JsonProperties.Reserve(BagStruct->GetPropertyDescs().Num());
                        for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
                        {
                            const FProperty* DynamicProperty = nullptr;
                            for (TFieldIterator<FProperty> It(BagStruct); It; ++It)
                            {
                                if (BagStruct->FindPropertyDescByProperty(*It) == &Desc)
                                {
                                    DynamicProperty = *It;
                                    break;
                                }
                            }

                            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
                            Entry->SetStringField(
                                TEXT("id"),
                                Desc.ID.ToString(EGuidFormats::DigitsWithHyphensLower));
                            Entry->SetStringField(TEXT("name"), Desc.Name.ToString());
                            Entry->SetStringField(
                                TEXT("valueType"),
                                EnumName(StaticEnum<EPropertyBagPropertyType>(), static_cast<int64>(Desc.ValueType)));
                            TArray<TSharedPtr<FJsonValue>> ContainerTypes;
                            for (const EPropertyBagContainerType ContainerType : Desc.ContainerTypes)
                            {
                                ContainerTypes.Add(MakeShared<FJsonValueString>(
                                    EnumName(
                                        StaticEnum<EPropertyBagContainerType>(),
                                        static_cast<int64>(ContainerType))));
                            }
                            Entry->SetArrayField(TEXT("containerTypes"), MoveTemp(ContainerTypes));
                            Entry->SetStringField(TEXT("propertyFlags"), LexToString(Desc.PropertyFlags));
                            Entry->SetStringField(
                                TEXT("keyType"),
                                EnumName(StaticEnum<EPropertyBagPropertyType>(), static_cast<int64>(Desc.KeyType)));
                            if (Desc.ValueTypeObject != nullptr)
                            {
                                Entry->SetObjectField(TEXT("valueTypeObject"), ObjectReference(Desc.ValueTypeObject));
                            }
                            if (Desc.KeyTypeObject != nullptr)
                            {
                                Entry->SetObjectField(TEXT("keyTypeObject"), ObjectReference(Desc.KeyTypeObject));
                            }
#if WITH_EDITORONLY_DATA
                            TArray<TSharedPtr<FJsonValue>> MetaData;
                            MetaData.Reserve(Desc.MetaData.Num());
                            for (const FPropertyBagPropertyDescMetaData& Meta : Desc.MetaData)
                            {
                                const TSharedRef<FJsonObject> MetaEntry = MakeShared<FJsonObject>();
                                MetaEntry->SetStringField(TEXT("key"), Meta.Key.ToString());
                                MetaEntry->SetStringField(TEXT("value"), Meta.Value);
                                MetaData.Add(MakeShared<FJsonValueObject>(MetaEntry));
                            }
                            Entry->SetArrayField(TEXT("metadata"), MoveTemp(MetaData));
                            if (Desc.MetaClass != nullptr)
                            {
                                Entry->SetObjectField(TEXT("metaClass"), ObjectReference(Desc.MetaClass));
                            }
#endif
                            if (DynamicProperty != nullptr && BagValue.GetMemory() != nullptr)
                            {
                                Entry->SetField(
                                    TEXT("value"),
                                    SerializePropertyValue(
                                        *DynamicProperty,
                                        BagValue.GetMemory(),
                                        Owner,
                                        Depth + 1));
                            }
                            else
                            {
                                const TSharedRef<FJsonObject> Omitted = MakeShared<FJsonObject>();
                                Omitted->SetBoolField(TEXT("omitted"), true);
                                Omitted->SetStringField(
                                    TEXT("reason"), TEXT("propertyBagValueUnavailable"));
                                Entry->SetObjectField(TEXT("value"), Omitted);
                            }
                            JsonProperties.Add(MakeShared<FJsonValueObject>(Entry));
                        }
                    }
                    Json->SetArrayField(TEXT("properties"), MoveTemp(JsonProperties));
                    return MakeShared<FJsonValueObject>(Json);
                }
                TArray<const FProperty*> Fields;
                for (TFieldIterator<FProperty> It(StructProperty->Struct, EFieldIterationFlags::IncludeSuper); It; ++It)
                {
                    if (ShouldExportProperty(**It))
                    {
                        Fields.Add(*It);
                    }
                }
                Fields.Sort([](const FProperty& Left, const FProperty& Right)
                {
                    return Left.GetName() < Right.GetName();
                });

                const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
                Json->SetStringField(TEXT("structType"), StructProperty->Struct->GetPathName());
                const TSharedRef<FJsonObject> FieldsJson = MakeShared<FJsonObject>();
                for (const FProperty* Field : Fields)
                {
                    if (IsPrivateName(Field->GetName()))
                    {
                        FieldsJson->SetStringField(Field->GetName(), TEXT("[REDACTED]"));
                    }
                    else
                    {
                        FieldsJson->SetField(
                            Field->GetName(),
                            SerializePropertyValue(*Field, Value, Owner, Depth + 1));
                    }
                }
                Json->SetObjectField(TEXT("fields"), FieldsJson);
                return MakeShared<FJsonValueObject>(Json);
            }

            FString Text = ExportText(Property, Value, Owner);
            if (Text.Len() <= MaxExportTextChars)
            {
                return MakeShared<FJsonValueString>(MoveTemp(Text));
            }
            const TSharedRef<FJsonObject> Omitted = MakeShared<FJsonObject>();
            Omitted->SetBoolField(TEXT("omitted"), true);
            Omitted->SetNumberField(TEXT("length"), Text.Len());
            return MakeShared<FJsonValueObject>(Omitted);
        }
    }

    bool IsPrivateName(const FString& Name)
    {
        // GameplayTagQuery bytecode is authored gameplay data. Its reflected field names contain
        // "Token", but neither field is an authentication token or a credential.
        if (Name == TEXT("QueryTokenStream") || Name == TEXT("TokenStreamVersion"))
        {
            return false;
        }
        for (const FString& Pattern : GetDefault<UUERingSettings>()->PrivacyFilters)
        {
            if (!Pattern.IsEmpty() && Name.MatchesWildcard(Pattern, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    bool ShouldExportProperty(const FProperty& Property)
    {
        if (Property.HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated | CPF_SkipSerialization))
        {
            return false;
        }
        return GetDefault<UUERingSettings>()->bIncludeEditorOnlyData
            || !Property.HasAnyPropertyFlags(CPF_EditorOnly);
    }

    TSharedRef<FJsonObject> SerializeProperty(
        const FProperty& Property,
        const void* Container,
        const UObject* Owner)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Property.GetName());
        FString ExtendedType;
        Json->SetStringField(TEXT("type"), Property.GetCPPType(&ExtendedType) + ExtendedType);
        if (IsPrivateName(Property.GetName()))
        {
            Json->SetStringField(TEXT("value"), TEXT("[REDACTED]"));
            Json->SetBoolField(TEXT("redacted"), true);
            return Json;
        }

        Json->SetField(TEXT("value"), SerializePropertyValue(Property, Container, Owner, 0));
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> SerializeObjectProperties(
        const UObject& Object,
        const UObject* Baseline,
        const bool bOnlyModified,
        const uint64 RequiredPropertyFlags)
    {
        TArray<const FProperty*> Properties;
        for (TFieldIterator<FProperty> It(Object.GetClass(), EFieldIterationFlags::IncludeSuper); It; ++It)
        {
            if (!ShouldExportProperty(**It))
            {
                continue;
            }
            if (RequiredPropertyFlags != 0 && !(*It)->HasAnyPropertyFlags(RequiredPropertyFlags))
            {
                continue;
            }
            if (bOnlyModified && Baseline != nullptr && (*It)->Identical_InContainer(&Object, Baseline))
            {
                continue;
            }
            Properties.Add(*It);
        }
        Properties.Sort([](const FProperty& Left, const FProperty& Right)
        {
            return Left.GetName() < Right.GetName();
        });

        TArray<TSharedPtr<FJsonValue>> JsonProperties;
        JsonProperties.Reserve(Properties.Num());
        for (const FProperty* Property : Properties)
        {
            JsonProperties.Add(MakeShared<FJsonValueObject>(SerializeProperty(*Property, &Object, &Object)));
        }
        return JsonProperties;
    }

    TArray<TSharedPtr<FJsonValue>> SerializeNamedObjectProperties(
        const UObject& Object,
        const TArray<FName>& PropertyNames)
    {
        TArray<const FProperty*> Properties;
        for (const FName PropertyName : PropertyNames)
        {
            const FProperty* Property = FindFProperty<FProperty>(Object.GetClass(), PropertyName);
            if (Property != nullptr && ShouldExportProperty(*Property))
            {
                Properties.AddUnique(Property);
            }
        }
        Properties.Sort([](const FProperty& Left, const FProperty& Right)
        {
            return Left.GetName() < Right.GetName();
        });

        TArray<TSharedPtr<FJsonValue>> JsonProperties;
        JsonProperties.Reserve(Properties.Num());
        for (const FProperty* Property : Properties)
        {
            JsonProperties.Add(MakeShared<FJsonValueObject>(SerializeProperty(*Property, &Object, &Object)));
        }
        return JsonProperties;
    }

    int64 GetContainerElementCount(const UObject& Object, const FName PropertyName)
    {
        const FProperty* Property = FindFProperty<FProperty>(Object.GetClass(), PropertyName);
        if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
        {
            return FScriptArrayHelper(
                ArrayProperty,
                ArrayProperty->ContainerPtrToValuePtr<void>(&Object)).Num();
        }
        if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
        {
            return FScriptSetHelper(
                SetProperty,
                SetProperty->ContainerPtrToValuePtr<void>(&Object)).Num();
        }
        if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
        {
            return FScriptMapHelper(
                MapProperty,
                MapProperty->ContainerPtrToValuePtr<void>(&Object)).Num();
        }
        return INDEX_NONE;
    }
}
