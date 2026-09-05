#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FProperty;

namespace UERingOwnedObjectSerializer
{
    TArray<TSharedPtr<FJsonValue>> SerializeOwnedObjects(
        const UObject& Root,
        TArray<TSharedPtr<FJsonValue>>& RootProperties,
        const FString& RootId = TEXT("$asset"),
        const TArray<const FProperty*>* RootPropertyFilter = nullptr,
        bool bIncludeArchetypeObjects = false);
}
