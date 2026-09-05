#include "UERingSemanticUtils.h"

#include "UERingPropertySerializer.h"

namespace UERingSemanticUtils
{
    void AddOmission(
        FUERingSemanticPayload& Payload,
        const FString& Path,
        const FString& Code,
        const FString& Reason,
        const FString& RecoverabilityImpact,
        const int64 OriginalCount,
        const FString& SourceDigest)
    {
        const TSharedRef<FJsonObject> Omission = MakeShared<FJsonObject>();
        Omission->SetStringField(TEXT("path"), Path);
        Omission->SetStringField(TEXT("code"), Code);
        Omission->SetStringField(TEXT("reason"), Reason);
        Omission->SetStringField(TEXT("recoverabilityImpact"), RecoverabilityImpact);
        if (OriginalCount != INDEX_NONE)
        {
            Omission->SetNumberField(TEXT("originalCount"), OriginalCount);
        }
        if (!SourceDigest.IsEmpty())
        {
            Omission->SetStringField(TEXT("sourceDigest"), SourceDigest);
        }
        Payload.Omissions.Add(MakeShared<FJsonValueObject>(Omission));
    }

    void SetSelectedProperties(
        const UObject& Object,
        const TArray<FName>& Names,
        const TSharedRef<FJsonObject>& Target,
        const TCHAR* FieldName)
    {
        const TArray<TSharedPtr<FJsonValue>> Properties =
            UERingPropertySerializer::SerializeNamedObjectProperties(Object, Names);
        if (!Properties.IsEmpty())
        {
            Target->SetArrayField(FieldName, Properties);
        }
    }
}
