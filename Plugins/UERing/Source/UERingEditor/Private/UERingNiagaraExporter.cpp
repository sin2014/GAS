#include "UERingNiagaraExporter.h"

#include "UERingDomainGraphExporter.h"
#include "UERingPropertySerializer.h"
#include "UERingSemanticUtils.h"
#include "UObject/UObjectHash.h"

namespace UERingNiagaraExporter
{
    TArray<FName> CommonProperties()
    {
        return {
            TEXT("EffectType"),
            TEXT("ExposedParameters"),
            TEXT("FixedBounds"),
            TEXT("LibraryVisibility"),
            TEXT("ParameterDefinitionsSubscriptions"),
            TEXT("Platforms"),
            TEXT("ScalabilityOverrides"),
            TEXT("SimTarget"),
            TEXT("bFixedBounds"),
            TEXT("bInterpolatedSpawning"),
            TEXT("bLocalSpace"),
            TEXT("bRequiresPersistentIDs")
        };
    }

    TArray<FName> PropertiesForClass(const FString& ClassName)
    {
        TArray<FName> Names = CommonProperties();
        if (ClassName == TEXT("NiagaraSystem"))
        {
            Names.Append({
                TEXT("EmitterHandles"),
                TEXT("SystemSpawnScript"),
                TEXT("SystemUpdateScript"),
                TEXT("SystemScalabilityOverrides"),
                TEXT("WarmupTickCount"),
                TEXT("WarmupTickDelta"),
                TEXT("WarmupTime")
            });
        }
        else if (ClassName == TEXT("NiagaraEmitter"))
        {
            Names.Append({
                TEXT("EventHandlerScriptProps"),
                TEXT("RendererProperties"),
                TEXT("SimulationStages"),
                TEXT("EmitterSpawnScriptProps"),
                TEXT("EmitterUpdateScriptProps"),
                TEXT("GPUComputeScript"),
                TEXT("ParticleSpawnScriptProps"),
                TEXT("ParticleUpdateScriptProps"),
                TEXT("Parent")
            });
        }
        else if (ClassName == TEXT("NiagaraScript"))
        {
            Names.Append({
                TEXT("CachedDefaultDataInterfaces"),
                TEXT("DefaultDataInterfaces"),
                TEXT("RapidIterationParameters"),
                TEXT("Usage"),
                TEXT("UsageId")
            });
        }
        return Names;
    }

    FString RoleForClass(const FString& ClassName)
    {
        if (ClassName == TEXT("NiagaraSystem")) return TEXT("system");
        if (ClassName == TEXT("NiagaraEmitter")) return TEXT("emitter");
        if (ClassName == TEXT("NiagaraScript")) return TEXT("script");
        if (ClassName == TEXT("NiagaraParameterDefinitions")) return TEXT("parameterDefinitions");
        if (ClassName == TEXT("NiagaraDataChannelAsset")) return TEXT("dataChannel");
        return TEXT("configuration");
    }
}

FName FUERingNiagaraExporter::GetName() const
{
    return TEXT("NiagaraLogic");
}

bool FUERingNiagaraExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.AssetClassPath.GetAssetName().ToString().StartsWith(TEXT("Niagara"));
}

bool FUERingNiagaraExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    UObject* Asset = Context.Asset.Get();
    if (Asset == nullptr)
    {
        OutError = TEXT("The Niagara asset could not be loaded.");
        return false;
    }
    if (Context.Profile != EUERingExportProfile::Logic)
    {
        FUERingDomainGraphExporter FullExporter;
        return FullExporter.BuildPayload(Context, OutPayload, OutError);
    }

    using namespace UERingNiagaraExporter;
    const FString ClassName = Asset->GetClass()->GetName();
    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("NiagaraLogic"));
    Semantics->SetStringField(TEXT("representation"), TEXT("niagara-interface-v1"));
    Semantics->SetStringField(TEXT("role"), RoleForClass(ClassName));
    Semantics->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetPathName());
    UERingSemanticUtils::SetSelectedProperties(
        *Asset,
        PropertiesForClass(ClassName),
        Semantics,
        TEXT("interfaceProperties"));

    TArray<UObject*> OwnedObjects;
    GetObjectsWithOuter(
        Asset,
        OwnedObjects,
        EGetObjectsFlags::IncludeNestedObjects,
        RF_Transient | RF_ClassDefaultObject,
        EInternalObjectFlags::Garbage);
    TMap<FString, int32> ClassCounts;
    int32 PersistentObjectCount = 0;
    for (const UObject* Object : OwnedObjects)
    {
        if (Object != nullptr && Object->GetOutermost() == Asset->GetOutermost())
        {
            ++PersistentObjectCount;
            ClassCounts.FindOrAdd(Object->GetClass()->GetPathName())++;
        }
    }
    TArray<FString> Classes;
    ClassCounts.GetKeys(Classes);
    Classes.Sort();
    TArray<TSharedPtr<FJsonValue>> Histogram;
    for (const FString& Class : Classes)
    {
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("class"), Class);
        Entry->SetNumberField(TEXT("count"), ClassCounts.FindChecked(Class));
        Histogram.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Semantics->SetNumberField(TEXT("internalObjectCount"), PersistentObjectCount);
    if (!Histogram.IsEmpty())
    {
        Semantics->SetArrayField(TEXT("internalClassHistogram"), Histogram);
    }
    OutPayload.Semantics = Semantics;
    UERingSemanticUtils::AddOmission(
        OutPayload,
        TEXT("/semantics/internalImplementation"),
        TEXT("replaceablePresentationImplementation"),
        TEXT("Logic profile preserves the Niagara interface, parameters, scripts, settings, and asset dependencies while omitting the replaceable VFX implementation graph."),
        TEXT("presentationAssetNotReconstructable"),
        PersistentObjectCount,
        Context.SourceHash);
    return true;
}
