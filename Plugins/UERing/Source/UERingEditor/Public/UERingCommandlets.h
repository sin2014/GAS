#pragma once

#include "Commandlets/Commandlet.h"

#include "UERingCommandlets.generated.h"

UCLASS()
class UERINGEDITOR_API UUERingExportCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    UUERingExportCommandlet();
    virtual int32 Main(const FString& Params) override;
};

UCLASS()
class UERINGEDITOR_API UUERingValidateCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    UUERingValidateCommandlet();
    virtual int32 Main(const FString& Params) override;
};
