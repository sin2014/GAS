#include "UERingDefinitionExporter.h"

#include "Engine/UserDefinedEnum.h"
#include "EdGraphSchema_K2.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UERingPinTypeSerializer.h"
#include "UERingPropertySerializer.h"
#include "UObject/UnrealType.h"

namespace UERingDefinitionExporter
{
    TArray<TSharedPtr<FJsonValue>> PropertyFlags(const FProperty& Property)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        auto Add = [&Values](const TCHAR* Value)
        {
            Values.Add(MakeShared<FJsonValueString>(Value));
        };
        if (Property.HasAnyPropertyFlags(CPF_Edit)) Add(TEXT("InstanceEditable"));
        if (Property.HasAnyPropertyFlags(CPF_BlueprintVisible)) Add(TEXT("BlueprintVisible"));
        if (Property.HasAnyPropertyFlags(CPF_BlueprintReadOnly)) Add(TEXT("BlueprintReadOnly"));
        if (Property.HasAnyPropertyFlags(CPF_SaveGame)) Add(TEXT("SaveGame"));
        return Values;
    }

    TSharedRef<FJsonObject> StructSemantics(const UUserDefinedStruct& Struct)
    {
        const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
        Semantics->SetStringField(TEXT("kind"), TEXT("UserDefinedStruct"));
        if (Struct.Guid.IsValid())
        {
            Semantics->SetStringField(TEXT("guid"), Struct.Guid.ToString(EGuidFormats::DigitsWithHyphensLower));
        }
        if (Struct.Status != UDSS_UpToDate)
        {
            Semantics->SetStringField(TEXT("status"), UEnum::GetValueAsString(Struct.Status.GetValue()));
        }

        TArray<const FProperty*> Properties;
        for (TFieldIterator<FProperty> It(&Struct, EFieldIterationFlags::None); It; ++It)
        {
            if (UERingPropertySerializer::ShouldExportProperty(**It))
            {
                Properties.Add(*It);
            }
        }
        Properties.Sort([&Struct](const FProperty& Left, const FProperty& Right)
        {
            return Struct.GetAuthoredNameForField(&Left) < Struct.GetAuthoredNameForField(&Right);
        });

        const uint8* Defaults = Struct.GetDefaultInstance();
        TArray<TSharedPtr<FJsonValue>> Fields;
        Fields.Reserve(Properties.Num());
        for (const FProperty* Property : Properties)
        {
            const TSharedRef<FJsonObject> Field = MakeShared<FJsonObject>();
            const FString AuthoredName = Struct.GetAuthoredNameForField(Property);
            const FString DisplayName = Property->GetDisplayNameText().ToString();
            Field->SetStringField(TEXT("name"), Property->GetName());
            if (!AuthoredName.IsEmpty() && AuthoredName != Property->GetName())
            {
                Field->SetStringField(TEXT("authoredName"), AuthoredName);
            }
            if (!DisplayName.IsEmpty() && DisplayName != AuthoredName)
            {
                Field->SetStringField(TEXT("displayName"), DisplayName);
            }
            FEdGraphPinType PinType;
            if (GetDefault<UEdGraphSchema_K2>()->ConvertPropertyToPinType(Property, PinType))
            {
                Field->SetObjectField(TEXT("type"), UERingPinTypeSerializer::Serialize(PinType));
            }
            else
            {
                const TSharedRef<FJsonObject> Type = MakeShared<FJsonObject>();
                Type->SetStringField(TEXT("category"), TEXT("unsupported"));
                Type->SetStringField(TEXT("cppType"), Property->GetCPPType());
                Field->SetObjectField(TEXT("type"), Type);
            }
            const FGuid PropertyGuid = Struct.FindPropertyGuidFromName(Property->GetFName());
            if (PropertyGuid.IsValid())
            {
                Field->SetStringField(TEXT("guid"), PropertyGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
            }
            if (Defaults != nullptr)
            {
                const TSharedRef<FJsonObject> Serialized =
                    UERingPropertySerializer::SerializeProperty(*Property, Defaults, &Struct);
                Field->SetField(TEXT("defaultValue"), Serialized->Values.FindChecked(TEXT("value")));
            }
            const TArray<TSharedPtr<FJsonValue>> Flags = PropertyFlags(*Property);
            if (!Flags.IsEmpty())
            {
                Field->SetArrayField(TEXT("flags"), Flags);
            }
            const FString Tooltip = Property->GetToolTipText().ToString();
            if (!Tooltip.IsEmpty())
            {
                Field->SetStringField(TEXT("tooltip"), Tooltip);
            }
            Fields.Add(MakeShared<FJsonValueObject>(Field));
        }
        if (!Fields.IsEmpty())
        {
            Semantics->SetArrayField(TEXT("fields"), Fields);
        }
        return Semantics;
    }

    TSharedRef<FJsonObject> EnumSemantics(const UUserDefinedEnum& Enum)
    {
        const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
        Semantics->SetStringField(TEXT("kind"), TEXT("UserDefinedEnum"));
        if (Enum.HasAnyEnumFlags(EEnumFlags::Flags))
        {
            Semantics->SetBoolField(TEXT("bitflags"), true);
        }
#if WITH_EDITORONLY_DATA
        if (!Enum.EnumDescription.IsEmpty())
        {
            Semantics->SetStringField(TEXT("description"), Enum.EnumDescription.ToString());
        }
#endif

        TArray<TSharedPtr<FJsonValue>> Entries;
        for (int32 Index = 0; Index < Enum.NumEnums(); ++Index)
        {
            const FString Name = Enum.GetNameStringByIndex(Index);
            if (Name.EndsWith(TEXT("_MAX")) || Enum.HasMetaData(TEXT("Hidden"), Index))
            {
                continue;
            }
            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            const FString AuthoredName = Enum.GetAuthoredNameStringByIndex(Index);
            const FString DisplayName = Enum.GetDisplayNameTextByIndex(Index).ToString();
            Entry->SetStringField(TEXT("name"), Name);
            if (!AuthoredName.IsEmpty() && AuthoredName != Name)
            {
                Entry->SetStringField(TEXT("authoredName"), AuthoredName);
            }
            if (!DisplayName.IsEmpty() && DisplayName != AuthoredName)
            {
                Entry->SetStringField(TEXT("displayName"), DisplayName);
            }
            Entry->SetNumberField(TEXT("value"), Enum.GetValueByIndex(Index));
            Entries.Add(MakeShared<FJsonValueObject>(Entry));
        }
        if (!Entries.IsEmpty())
        {
            Semantics->SetArrayField(TEXT("entries"), Entries);
        }
        return Semantics;
    }
}

FName FUERingDefinitionExporter::GetName() const
{
    return TEXT("Definition");
}

bool FUERingDefinitionExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.IsInstanceOf(UUserDefinedStruct::StaticClass())
        || AssetData.IsInstanceOf(UUserDefinedEnum::StaticClass());
}

bool FUERingDefinitionExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    if (const UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(Context.Asset.Get()))
    {
        OutPayload.Semantics = UERingDefinitionExporter::StructSemantics(*Struct);
        return true;
    }
    if (const UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(Context.Asset.Get()))
    {
        OutPayload.Semantics = UERingDefinitionExporter::EnumSemantics(*Enum);
        return true;
    }
    OutError = TEXT("The loaded object is not a user-defined struct or enum.");
    return false;
}
