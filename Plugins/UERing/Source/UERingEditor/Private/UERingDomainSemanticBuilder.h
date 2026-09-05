#pragma once

#include "Dom/JsonObject.h"
#include "UERingExportTypes.h"

namespace UERingDomainSemanticBuilder
{
    bool AddDomainSemantics(
        const FUERingExportContext& Context,
        const TSharedRef<FJsonObject>& Semantics);

    TSharedRef<FJsonObject> BuildReconstructionIR(
        const FUERingExportContext& Context,
        const FString& ExporterName,
        const TSharedRef<FJsonObject>& Semantics,
        const TArray<TSharedPtr<FJsonValue>>& Omissions,
        bool bHasDomainSemantics);
}
