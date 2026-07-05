#pragma once

#include "Commandlets/Commandlet.h"
#include "StormHeroesOrganizeCommandlet.generated.h"

UCLASS()
class UStormHeroesOrganizeCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    UStormHeroesOrganizeCommandlet();

    virtual int32 Main(const FString& Params) override;
};
