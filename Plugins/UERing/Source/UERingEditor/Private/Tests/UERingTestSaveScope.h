#pragma once

#include "UObject/UnrealType.h"

class FUERingTestSaveScope final
{
public:
    FUERingTestSaveScope()
    {
        UClass* SettingsClass = FindObject<UClass>(
            nullptr,
            TEXT("/Script/DataValidation.DataValidationSettings"));
        if (SettingsClass == nullptr)
        {
            return;
        }
        SettingsObject = SettingsClass->GetDefaultObject();
        ValidateOnSaveProperty = FindFProperty<FBoolProperty>(
            SettingsClass,
            TEXT("bValidateOnSave"));
        if (SettingsObject != nullptr && ValidateOnSaveProperty != nullptr)
        {
            bPreviousValidateOnSave =
                ValidateOnSaveProperty->GetPropertyValue_InContainer(SettingsObject);
            ValidateOnSaveProperty->SetPropertyValue_InContainer(SettingsObject, false);
        }
    }

    ~FUERingTestSaveScope()
    {
        if (SettingsObject != nullptr && ValidateOnSaveProperty != nullptr)
        {
            ValidateOnSaveProperty->SetPropertyValue_InContainer(
                SettingsObject,
                bPreviousValidateOnSave);
        }
    }

private:
    UObject* SettingsObject = nullptr;
    FBoolProperty* ValidateOnSaveProperty = nullptr;
    bool bPreviousValidateOnSave = true;
};
