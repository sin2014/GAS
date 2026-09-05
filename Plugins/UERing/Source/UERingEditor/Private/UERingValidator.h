#pragma once

#include "CoreMinimal.h"

struct FUERingValidationReport
{
    int32 Checked = 0;
    int32 Missing = 0;
    int32 Stale = 0;
    int32 Orphan = 0;
    int32 Invalid = 0;
    TArray<FString> Messages;

    bool IsValid() const
    {
        return Missing == 0 && Stale == 0 && Orphan == 0 && Invalid == 0;
    }
};

class FUERingValidator
{
public:
    static FUERingValidationReport Validate();
};
