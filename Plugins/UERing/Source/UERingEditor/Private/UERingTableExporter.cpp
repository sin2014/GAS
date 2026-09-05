#include "UERingTableExporter.h"

#include "Curves/RichCurve.h"
#include "Curves/SimpleCurve.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "UERingPropertySerializer.h"
#include "UObject/UnrealType.h"

namespace UERingTableExporter
{
    TArray<FName> SortedNames(const TArray<FName>& Names)
    {
        TArray<FName> Result = Names;
        Result.Sort(FNameLexicalLess());
        return Result;
    }

    TSharedRef<FJsonObject> SerializeDataTable(UDataTable& Table)
    {
        const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
        Semantics->SetStringField(TEXT("kind"), TEXT("DataTable"));
        const UScriptStruct* RowStruct = Table.GetRowStruct();
        Semantics->SetStringField(TEXT("rowStruct"), RowStruct != nullptr ? RowStruct->GetPathName() : FString());

        TArray<const FProperty*> Properties;
        if (RowStruct != nullptr)
        {
            for (TFieldIterator<FProperty> It(RowStruct, EFieldIterationFlags::IncludeSuper); It; ++It)
            {
                if (!It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated | CPF_SkipSerialization))
                {
                    Properties.Add(*It);
                }
            }
        }
        Properties.Sort([](const FProperty& Left, const FProperty& Right)
        {
            return Left.GetName() < Right.GetName();
        });

        TArray<TSharedPtr<FJsonValue>> FieldDefinitions;
        for (const FProperty* Property : Properties)
        {
            const TSharedRef<FJsonObject> Field = MakeShared<FJsonObject>();
            Field->SetStringField(TEXT("internalName"), Property->GetName());
            Field->SetStringField(TEXT("authoredName"), Property->GetAuthoredName());
            Field->SetStringField(TEXT("displayName"), Property->GetDisplayNameText().ToString());
            Field->SetStringField(TEXT("type"), Property->GetCPPType());
            FieldDefinitions.Add(MakeShared<FJsonValueObject>(Field));
        }
        Semantics->SetArrayField(TEXT("fieldDefinitions"), FieldDefinitions);

        TArray<FName> RowNames;
        Table.GetRowMap().GenerateKeyArray(RowNames);
        RowNames = SortedNames(RowNames);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const FName RowName : RowNames)
        {
            const uint8* const* RowPointer = Table.GetRowMap().Find(RowName);
            if (RowPointer == nullptr || *RowPointer == nullptr)
            {
                continue;
            }

            const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("name"), RowName.ToString());
            const TSharedRef<FJsonObject> Values = MakeShared<FJsonObject>();
            const TSharedRef<FJsonObject> ValueDetails = MakeShared<FJsonObject>();
            for (const FProperty* Property : Properties)
            {
                const TSharedRef<FJsonObject> Serialized = UERingPropertySerializer::SerializeProperty(
                    *Property,
                    *RowPointer,
                    &Table);
                Values->SetField(Property->GetName(), Serialized->Values.FindChecked(TEXT("value")));

                const TSharedRef<FJsonObject> Detail = MakeShared<FJsonObject>();
                Detail->SetStringField(TEXT("internalName"), Property->GetName());
                Detail->SetStringField(TEXT("authoredName"), Property->GetAuthoredName());
                Detail->SetStringField(TEXT("displayName"), Property->GetDisplayNameText().ToString());
                Detail->SetStringField(TEXT("type"), Property->GetCPPType());
                bool bRedacted = false;
                if (Serialized->TryGetBoolField(TEXT("redacted"), bRedacted) && bRedacted)
                {
                    Detail->SetBoolField(TEXT("redacted"), true);
                }
                ValueDetails->SetObjectField(Property->GetName(), Detail);
            }
            Row->SetObjectField(TEXT("values"), Values);
            Row->SetObjectField(TEXT("valueDetails"), ValueDetails);
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Semantics->SetArrayField(TEXT("rows"), Rows);
        return Semantics;
    }

    TSharedRef<FJsonObject> SerializeCurveTable(UCurveTable& Table)
    {
        const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
        Semantics->SetStringField(TEXT("kind"), TEXT("CurveTable"));
        Semantics->SetStringField(TEXT("curveMode"), UEnum::GetValueAsString(Table.GetCurveTableMode()));

        TArray<FName> RowNames;
        Table.GetRowMap().GenerateKeyArray(RowNames);
        RowNames = SortedNames(RowNames);
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const FName RowName : RowNames)
        {
            FRealCurve* const* CurvePointer = Table.GetRowMap().Find(RowName);
            if (CurvePointer == nullptr || *CurvePointer == nullptr)
            {
                continue;
            }

            const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("name"), RowName.ToString());
            TArray<TSharedPtr<FJsonValue>> Keys;
            if (Table.GetCurveTableMode() == ECurveTableMode::RichCurves)
            {
                for (const FRichCurveKey& Key : static_cast<FRichCurve*>(*CurvePointer)->GetCopyOfKeys())
                {
                    const TSharedRef<FJsonObject> JsonKey = MakeShared<FJsonObject>();
                    JsonKey->SetNumberField(TEXT("time"), Key.Time);
                    JsonKey->SetNumberField(TEXT("value"), Key.Value);
                    JsonKey->SetStringField(TEXT("interpMode"), UEnum::GetValueAsString(Key.InterpMode));
                    JsonKey->SetNumberField(TEXT("arriveTangent"), Key.ArriveTangent);
                    JsonKey->SetNumberField(TEXT("leaveTangent"), Key.LeaveTangent);
                    Keys.Add(MakeShared<FJsonValueObject>(JsonKey));
                }
            }
            else if (Table.GetCurveTableMode() == ECurveTableMode::SimpleCurves)
            {
                for (const FSimpleCurveKey& Key : static_cast<FSimpleCurve*>(*CurvePointer)->GetCopyOfKeys())
                {
                    const TSharedRef<FJsonObject> JsonKey = MakeShared<FJsonObject>();
                    JsonKey->SetNumberField(TEXT("time"), Key.Time);
                    JsonKey->SetNumberField(TEXT("value"), Key.Value);
                    Keys.Add(MakeShared<FJsonValueObject>(JsonKey));
                }
            }
            Row->SetArrayField(TEXT("keys"), Keys);
            Rows.Add(MakeShared<FJsonValueObject>(Row));
        }
        Semantics->SetArrayField(TEXT("rows"), Rows);
        return Semantics;
    }
}

FName FUERingTableExporter::GetName() const
{
    return TEXT("Table");
}

bool FUERingTableExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.IsInstanceOf(UDataTable::StaticClass())
        || AssetData.IsInstanceOf(UCurveTable::StaticClass());
}

bool FUERingTableExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    if (UDataTable* Table = Cast<UDataTable>(Context.Asset.Get()))
    {
        OutPayload.Semantics = UERingTableExporter::SerializeDataTable(*Table);
        return true;
    }
    if (UCurveTable* Table = Cast<UCurveTable>(Context.Asset.Get()))
    {
        OutPayload.Semantics = UERingTableExporter::SerializeCurveTable(*Table);
        return true;
    }
    OutError = TEXT("The loaded object is not a DataTable or CurveTable.");
    return false;
}
