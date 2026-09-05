#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FEdGraphPinType;

namespace UERingPinTypeSerializer
{
    TSharedRef<FJsonObject> Serialize(const FEdGraphPinType& Type);
}
