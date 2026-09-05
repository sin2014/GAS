#include "UERingLevelSequenceExporter.h"

#include "Channels/MovieSceneChannel.h"
#include "Channels/MovieSceneChannelEditorData.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSection.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneTrack.h"
#include "UERingPropertySerializer.h"

namespace UERingLevelSequenceExporter
{
    FString Guid(const FGuid& Value)
    {
        return Value.IsValid() ? Value.ToString(EGuidFormats::DigitsWithHyphensLower) : FString();
    }

    TSharedRef<FJsonObject> ObjectReference(const UObject* Object)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("path"), Object != nullptr ? Object->GetPathName() : FString());
        Json->SetStringField(TEXT("class"), Object != nullptr ? Object->GetClass()->GetPathName() : FString());
        return Json;
    }

    TSharedRef<FJsonObject> FrameRate(const FFrameRate& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("numerator"), Value.Numerator);
        Json->SetNumberField(TEXT("denominator"), Value.Denominator);
        Json->SetNumberField(TEXT("decimal"), Value.AsDecimal());
        return Json;
    }

    TSharedRef<FJsonObject> FrameRange(const TRange<FFrameNumber>& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetBoolField(TEXT("empty"), Value.IsEmpty());
        Json->SetBoolField(TEXT("hasStart"), Value.HasLowerBound());
        Json->SetBoolField(TEXT("hasEnd"), Value.HasUpperBound());
        if (Value.HasLowerBound())
        {
            Json->SetNumberField(TEXT("startFrame"), Value.GetLowerBoundValue().Value);
            Json->SetBoolField(TEXT("startInclusive"), Value.GetLowerBound().IsInclusive());
        }
        if (Value.HasUpperBound())
        {
            Json->SetNumberField(TEXT("endFrame"), Value.GetUpperBoundValue().Value);
            Json->SetBoolField(TEXT("endInclusive"), Value.GetUpperBound().IsInclusive());
        }
        return Json;
    }

    TSharedRef<FJsonObject> SerializeSection(const UMovieSceneSection& Section, const int32 Order)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("order"), Order);
        Json->SetStringField(TEXT("name"), Section.GetName());
        Json->SetStringField(TEXT("class"), Section.GetClass()->GetPathName());
        Json->SetObjectField(TEXT("range"), FrameRange(Section.GetRange()));
        Json->SetNumberField(TEXT("rowIndex"), Section.GetRowIndex());
        Json->SetNumberField(TEXT("overlapPriority"), Section.GetOverlapPriority());
        Json->SetBoolField(TEXT("active"), Section.IsActive());
        Json->SetBoolField(TEXT("locked"), Section.IsLocked());
        Json->SetNumberField(TEXT("preRollFrames"), Section.GetPreRollFrames());
        Json->SetNumberField(TEXT("postRollFrames"), Section.GetPostRollFrames());
        Json->SetNumberField(TEXT("completionMode"), static_cast<int32>(Section.GetCompletionMode()));
        const FOptionalMovieSceneBlendType BlendType = Section.GetBlendType();
        Json->SetBoolField(TEXT("hasBlendType"), BlendType.IsValid());
        if (BlendType.IsValid())
        {
            Json->SetNumberField(TEXT("blendType"), static_cast<int32>(BlendType.Get()));
        }

        TArray<TSharedPtr<FJsonValue>> Channels;
        int32 TotalKeys = 0;
        const FMovieSceneChannelProxy& Proxy = Section.GetChannelProxy();
        for (const FMovieSceneChannelEntry& Entry : Proxy.GetAllEntries())
        {
            const TArrayView<FMovieSceneChannel* const> EntryChannels = Entry.GetChannels();
#if WITH_EDITOR
            const TArrayView<const FMovieSceneChannelMetaData> MetaData = Entry.GetMetaData();
#endif
            for (int32 ChannelIndex = 0; ChannelIndex < EntryChannels.Num(); ++ChannelIndex)
            {
                FMovieSceneChannel* Channel = EntryChannels[ChannelIndex];
                if (Channel == nullptr)
                {
                    continue;
                }
                const TSharedRef<FJsonObject> JsonChannel = MakeShared<FJsonObject>();
                JsonChannel->SetNumberField(TEXT("index"), ChannelIndex);
                JsonChannel->SetStringField(TEXT("type"), Entry.GetChannelTypeName().ToString());
#if WITH_EDITOR
                if (MetaData.IsValidIndex(ChannelIndex))
                {
                    JsonChannel->SetStringField(TEXT("name"), MetaData[ChannelIndex].Name.ToString());
                    JsonChannel->SetStringField(TEXT("displayName"), MetaData[ChannelIndex].DisplayText.ToString());
                    JsonChannel->SetStringField(TEXT("group"), MetaData[ChannelIndex].Group.ToString());
                    JsonChannel->SetBoolField(TEXT("enabled"), MetaData[ChannelIndex].bEnabled);
                }
#endif
                TArray<FFrameNumber> KeyTimes;
                Channel->GetKeys(TRange<FFrameNumber>::All(), &KeyTimes, nullptr);
                TArray<TSharedPtr<FJsonValue>> JsonKeyTimes;
                for (const FFrameNumber KeyTime : KeyTimes)
                {
                    JsonKeyTimes.Add(MakeShared<FJsonValueNumber>(KeyTime.Value));
                }
                JsonChannel->SetNumberField(TEXT("keyCount"), Channel->GetNumKeys());
                JsonChannel->SetArrayField(TEXT("keyTimes"), JsonKeyTimes);
                TotalKeys += Channel->GetNumKeys();
                Channels.Add(MakeShared<FJsonValueObject>(JsonChannel));
            }
        }
        Json->SetNumberField(TEXT("channelCount"), Channels.Num());
        Json->SetNumberField(TEXT("keyCount"), TotalKeys);
        Json->SetArrayField(TEXT("channels"), Channels);
        Json->SetArrayField(TEXT("properties"), UERingPropertySerializer::SerializeObjectProperties(Section));
        return Json;
    }

    TSharedRef<FJsonObject> SerializeTrack(
        const UMovieSceneTrack& Track,
        const FString& Role,
        const int32 Order)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("order"), Order);
        Json->SetStringField(TEXT("role"), Role);
        Json->SetStringField(TEXT("name"), Track.GetName());
        Json->SetStringField(TEXT("trackName"), Track.GetTrackName().ToString());
        Json->SetStringField(TEXT("displayName"), Track.GetDisplayName().ToString());
        Json->SetStringField(TEXT("class"), Track.GetClass()->GetPathName());
        Json->SetStringField(TEXT("objectBindingGuid"), Guid(Track.FindObjectBindingGuid()));
        Json->SetArrayField(TEXT("properties"), UERingPropertySerializer::SerializeObjectProperties(Track));

        TArray<TSharedPtr<FJsonValue>> Sections;
        const TArray<UMovieSceneSection*>& TrackSections = Track.GetAllSections();
        for (int32 SectionIndex = 0; SectionIndex < TrackSections.Num(); ++SectionIndex)
        {
            if (TrackSections[SectionIndex] != nullptr)
            {
                Sections.Add(MakeShared<FJsonValueObject>(SerializeSection(*TrackSections[SectionIndex], SectionIndex)));
            }
        }
        Json->SetNumberField(TEXT("sectionCount"), Sections.Num());
        Json->SetArrayField(TEXT("sections"), Sections);
        return Json;
    }
}

FName FUERingLevelSequenceExporter::GetName() const
{
    return TEXT("LevelSequence");
}

bool FUERingLevelSequenceExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.IsInstanceOf(ULevelSequence::StaticClass());
}

bool FUERingLevelSequenceExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    using namespace UERingLevelSequenceExporter;
    ULevelSequence* Sequence = Cast<ULevelSequence>(Context.Asset.Get());
    if (Sequence == nullptr)
    {
        OutError = TEXT("The loaded object is not a LevelSequence.");
        return false;
    }
    UMovieScene* MovieScene = Sequence->GetMovieScene();
    if (MovieScene == nullptr)
    {
        OutError = TEXT("The LevelSequence has no MovieScene.");
        return false;
    }

    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("LevelSequence"));
    Semantics->SetObjectField(TEXT("tickResolution"), FrameRate(MovieScene->GetTickResolution()));
    Semantics->SetObjectField(TEXT("displayRate"), FrameRate(MovieScene->GetDisplayRate()));
    Semantics->SetObjectField(TEXT("playbackRange"), FrameRange(MovieScene->GetPlaybackRange()));
    Semantics->SetNumberField(TEXT("evaluationType"), static_cast<int32>(MovieScene->GetEvaluationType()));
    Semantics->SetNumberField(TEXT("clockSource"), static_cast<int32>(MovieScene->GetClockSource()));
    Semantics->SetArrayField(TEXT("sequenceProperties"), UERingPropertySerializer::SerializeObjectProperties(*Sequence));

    TArray<TSharedPtr<FJsonValue>> GlobalTracks;
    TSet<const UMovieSceneTrack*> SeenGlobalTracks;
    for (int32 TrackIndex = 0; TrackIndex < MovieScene->GetTracks().Num(); ++TrackIndex)
    {
        const UMovieSceneTrack* Track = MovieScene->GetTracks()[TrackIndex];
        if (Track != nullptr)
        {
            SeenGlobalTracks.Add(Track);
            GlobalTracks.Add(MakeShared<FJsonValueObject>(SerializeTrack(*Track, TEXT("global"), TrackIndex)));
        }
    }
    if (const UMovieSceneTrack* CameraCutTrack = MovieScene->GetCameraCutTrack();
        CameraCutTrack != nullptr && !SeenGlobalTracks.Contains(CameraCutTrack))
    {
        GlobalTracks.Add(MakeShared<FJsonValueObject>(
            SerializeTrack(*CameraCutTrack, TEXT("cameraCut"), GlobalTracks.Num())));
    }
    Semantics->SetNumberField(TEXT("globalTrackCount"), GlobalTracks.Num());
    Semantics->SetArrayField(TEXT("globalTracks"), GlobalTracks);

    TArray<TSharedPtr<FJsonValue>> Bindings;
    const UMovieScene& ConstMovieScene = *MovieScene;
    const TArray<FMovieSceneBinding>& SceneBindings = ConstMovieScene.GetBindings();
    for (int32 BindingIndex = 0; BindingIndex < SceneBindings.Num(); ++BindingIndex)
    {
        const FMovieSceneBinding& Binding = SceneBindings[BindingIndex];
        const FGuid BindingGuid = Binding.GetObjectGuid();
        const TSharedRef<FJsonObject> JsonBinding = MakeShared<FJsonObject>();
        JsonBinding->SetNumberField(TEXT("order"), BindingIndex);
        JsonBinding->SetStringField(TEXT("guid"), Guid(BindingGuid));
        if (const FMovieScenePossessable* Possessable = MovieScene->FindPossessable(BindingGuid))
        {
            JsonBinding->SetStringField(TEXT("kind"), TEXT("possessable"));
            JsonBinding->SetStringField(TEXT("name"), Possessable->GetName());
            JsonBinding->SetStringField(
                TEXT("possessedClass"),
                Possessable->GetPossessedObjectClass() != nullptr
                    ? Possessable->GetPossessedObjectClass()->GetPathName()
                    : FString());
            JsonBinding->SetStringField(TEXT("parentGuid"), Guid(Possessable->GetParent()));
        }
        else if (const FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(BindingGuid))
        {
            JsonBinding->SetStringField(TEXT("kind"), TEXT("spawnable"));
            JsonBinding->SetStringField(TEXT("name"), Spawnable->GetName());
            JsonBinding->SetObjectField(TEXT("objectTemplate"), ObjectReference(Spawnable->GetObjectTemplate()));
            TArray<TSharedPtr<FJsonValue>> ChildPossessables;
            for (const FGuid& ChildGuid : Spawnable->GetChildPossessables())
            {
                ChildPossessables.Add(MakeShared<FJsonValueString>(Guid(ChildGuid)));
            }
            JsonBinding->SetArrayField(TEXT("childPossessables"), ChildPossessables);
        }
        else
        {
            JsonBinding->SetStringField(TEXT("kind"), TEXT("unknown"));
            JsonBinding->SetStringField(TEXT("name"), MovieScene->GetObjectDisplayName(BindingGuid).ToString());
        }

        TArray<TSharedPtr<FJsonValue>> BindingTracks;
        for (int32 TrackIndex = 0; TrackIndex < Binding.GetTracks().Num(); ++TrackIndex)
        {
            const UMovieSceneTrack* Track = Binding.GetTracks()[TrackIndex];
            if (Track != nullptr)
            {
                BindingTracks.Add(MakeShared<FJsonValueObject>(
                    SerializeTrack(*Track, TEXT("objectBinding"), TrackIndex)));
            }
        }
        JsonBinding->SetNumberField(TEXT("trackCount"), BindingTracks.Num());
        JsonBinding->SetArrayField(TEXT("tracks"), BindingTracks);
        Bindings.Add(MakeShared<FJsonValueObject>(JsonBinding));
    }
    Semantics->SetNumberField(TEXT("bindingCount"), Bindings.Num());
    Semantics->SetNumberField(TEXT("possessableCount"), MovieScene->GetPossessableCount());
    Semantics->SetNumberField(TEXT("spawnableCount"), MovieScene->GetSpawnableCount());
    Semantics->SetArrayField(TEXT("bindings"), Bindings);

    OutPayload.Semantics = Semantics;
    return true;
}
