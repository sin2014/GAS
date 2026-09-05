#include "UERingSettings.h"

FString UERingExportProfileName(const EUERingExportProfile Profile)
{
    switch (Profile)
    {
    case EUERingExportProfile::Logic:
        return TEXT("logic");
    case EUERingExportProfile::Reconstruction:
        return TEXT("reconstruction");
    case EUERingExportProfile::Forensics:
        return TEXT("forensics");
    default:
        return TEXT("logic");
    }
}

#define LOCTEXT_NAMESPACE "UERingSettings"

UUERingSettings::UUERingSettings()
{
    OutputRoot.Path = TEXT(".uesem");
    SummaryLanguages = { TEXT("en"), TEXT("zh-CN") };
    IgnoredPaths = { TEXT("/Game/Developers/**") };
    PrivacyFilters = {
        TEXT("*ApiKey*"),
        TEXT("*Auth*"),
        TEXT("*Credential*"),
        TEXT("*Password*"),
        TEXT("*Private*Server*"),
        TEXT("*Secret*"),
        TEXT("*Token*")
    };
}

FName UUERingSettings::GetCategoryName() const
{
    return TEXT("Plugins");
}

FName UUERingSettings::GetSectionName() const
{
    return TEXT("UERing");
}

FText UUERingSettings::GetSectionText() const
{
    return LOCTEXT("SectionText", "UE Ring Semantic Export");
}

FText UUERingSettings::GetSectionDescription() const
{
    return LOCTEXT("SectionDescription", "Configure deterministic AI semantic exports for Unreal assets.");
}

#undef LOCTEXT_NAMESPACE
