#include "UERingReflectionExporter.h"

#include "UERingPropertySerializer.h"

FName FUERingReflectionExporter::GetName() const
{
    return TEXT("ReflectionFallback");
}

bool FUERingReflectionExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.IsValid();
}

bool FUERingReflectionExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    const UObject* Asset = Context.Asset.Get();
    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("ReflectionFallback"));
    Semantics->SetBoolField(TEXT("fallbackReflectionOnly"), true);
    Semantics->SetBoolField(TEXT("assetLoaded"), Asset != nullptr);
    Semantics->SetStringField(
        TEXT("class"),
        Asset != nullptr ? Asset->GetClass()->GetPathName() : Context.AssetData.AssetClassPath.ToString());
    Semantics->SetStringField(
        TEXT("outer"),
        Asset != nullptr && Asset->GetOuter() != nullptr ? Asset->GetOuter()->GetPathName() : FString());
    Semantics->SetArrayField(TEXT("properties"), Asset != nullptr
        ? UERingPropertySerializer::SerializeObjectProperties(*Asset)
        : TArray<TSharedPtr<FJsonValue>>());
    OutPayload.Semantics = Semantics;

    const TSharedRef<FJsonObject> Diagnostic = MakeShared<FJsonObject>();
    Diagnostic->SetStringField(TEXT("severity"), TEXT("info"));
    Diagnostic->SetStringField(TEXT("code"), TEXT("fallbackReflectionOnly"));
    Diagnostic->SetStringField(
        TEXT("message"),
        TEXT("No specialized exporter matched this asset; graph or custom serialized data may be incomplete."));
    OutPayload.Diagnostics.Add(MakeShared<FJsonValueObject>(Diagnostic));

    if (Asset == nullptr)
    {
        const TSharedRef<FJsonObject> LoadDiagnostic = MakeShared<FJsonObject>();
        LoadDiagnostic->SetStringField(TEXT("severity"), TEXT("warning"));
        LoadDiagnostic->SetStringField(TEXT("code"), TEXT("assetLoadFailed"));
        LoadDiagnostic->SetStringField(
            TEXT("message"),
            TEXT("The asset object could not be loaded; only registry metadata and dependency information are available."));
        OutPayload.Diagnostics.Add(MakeShared<FJsonValueObject>(LoadDiagnostic));
    }
    return true;
}
