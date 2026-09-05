#pragma once

#include "Dom/JsonObject.h"
#include "UERingExportTypes.h"

namespace UERingSemanticUtils
{
    void AddOmission(
        FUERingSemanticPayload& Payload,
        const FString& Path,
        const FString& Code,
        const FString& Reason,
        const FString& RecoverabilityImpact,
        int64 OriginalCount = INDEX_NONE,
        const FString& SourceDigest = FString());

    void SetSelectedProperties(
        const UObject& Object,
        const TArray<FName>& Names,
        const TSharedRef<FJsonObject>& Target,
        const TCHAR* FieldName = TEXT("properties"));
}
