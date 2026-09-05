#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FProperty;
class UObject;

namespace UERingPropertySerializer
{
    bool IsPrivateName(const FString& Name);
    bool ShouldExportProperty(const FProperty& Property);

    TSharedRef<FJsonObject> SerializeProperty(
        const FProperty& Property,
        const void* Container,
        const UObject* Owner = nullptr);

    TArray<TSharedPtr<FJsonValue>> SerializeObjectProperties(
        const UObject& Object,
        const UObject* Baseline = nullptr,
        bool bOnlyModified = false,
        uint64 RequiredPropertyFlags = 0);

    TArray<TSharedPtr<FJsonValue>> SerializeNamedObjectProperties(
        const UObject& Object,
        const TArray<FName>& PropertyNames);

    int64 GetContainerElementCount(const UObject& Object, FName PropertyName);
}
