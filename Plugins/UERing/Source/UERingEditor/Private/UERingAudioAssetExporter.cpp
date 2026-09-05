#include "UERingAudioAssetExporter.h"

#include "UERingPropertySerializer.h"
#include "UERingSemanticUtils.h"

namespace UERingAudioAssetExporter
{
    bool IsBulkAudioClass(const FString& ClassName)
    {
        return ClassName == TEXT("AudioImpulseResponse") || ClassName == TEXT("SoundWave");
    }

    TArray<FName> InterfaceProperties(const FString& ClassName)
    {
        if (ClassName == TEXT("AudioImpulseResponse"))
        {
            return {
                TEXT("SampleRate"),
                TEXT("NumChannels"),
                TEXT("bTrueStereo"),
                TEXT("bIsEvenChannelCount"),
                TEXT("NormalizationVolumeDb")
            };
        }
        return {
            TEXT("Duration"),
            TEXT("SampleRate"),
            TEXT("NumChannels"),
            TEXT("SoundGroup"),
            TEXT("VirtualizationMode"),
            TEXT("LoadingBehavior"),
            TEXT("CompressionQuality"),
            TEXT("SoundClassObject"),
            TEXT("ConcurrencySet"),
            TEXT("AttenuationSettings"),
            TEXT("SourceEffectChain"),
            TEXT("bLooping"),
            TEXT("bStreaming"),
            TEXT("bProcedural")
        };
    }
}

FName FUERingAudioAssetExporter::GetName() const
{
    return TEXT("AudioLogic");
}

bool FUERingAudioAssetExporter::CanExport(const FAssetData& AssetData) const
{
    return UERingAudioAssetExporter::IsBulkAudioClass(
        AssetData.AssetClassPath.GetAssetName().ToString());
}

bool FUERingAudioAssetExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    UObject* Asset = Context.Asset.Get();
    if (Asset == nullptr)
    {
        OutError = TEXT("The audio asset could not be loaded.");
        return false;
    }

    const FString ClassName = Asset->GetClass()->GetName();
    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("AudioLogic"));
    Semantics->SetStringField(TEXT("representation"), TEXT("audio-interface-v1"));
    Semantics->SetStringField(TEXT("role"), ClassName == TEXT("AudioImpulseResponse")
        ? TEXT("impulseResponse") : TEXT("soundWave"));
    Semantics->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetPathName());

    if (Context.Profile == EUERingExportProfile::Logic)
    {
        UERingSemanticUtils::SetSelectedProperties(
            *Asset,
            UERingAudioAssetExporter::InterfaceProperties(ClassName),
            Semantics,
            TEXT("interfaceProperties"));

        const FName BulkProperty = ClassName == TEXT("AudioImpulseResponse")
            ? FName(TEXT("ImpulseResponse")) : FName(TEXT("RawData"));
        const int64 SampleOrChunkCount =
            UERingPropertySerializer::GetContainerElementCount(*Asset, BulkProperty);
        UERingSemanticUtils::AddOmission(
            OutPayload,
            ClassName == TEXT("AudioImpulseResponse")
                ? TEXT("/semantics/impulseSamples") : TEXT("/semantics/audioSamples"),
            TEXT("replaceableAudioBulk"),
            TEXT("Logic profile preserves playback settings, routing, references, and timing metadata while omitting replaceable encoded or sampled audio data."),
            TEXT("presentationAssetNotReconstructable"),
            SampleOrChunkCount,
            Context.SourceHash);
    }
    else
    {
        Semantics->SetArrayField(
            TEXT("properties"),
            UERingPropertySerializer::SerializeObjectProperties(*Asset));
    }

    OutPayload.Semantics = Semantics;
    return true;
}
