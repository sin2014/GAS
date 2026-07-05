#pragma once

#include "Commandlets/Commandlet.h"
#include "StormHeroesFixMaterialsCommandlet.generated.h"

UCLASS()
class UStormHeroesFixMaterialsCommandlet final : public UCommandlet
{
    GENERATED_BODY()

public:
    UStormHeroesFixMaterialsCommandlet();

    virtual int32 Main(const FString& Params) override;
};
