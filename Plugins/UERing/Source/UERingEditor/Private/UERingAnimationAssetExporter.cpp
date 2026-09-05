#include "UERingAnimationAssetExporter.h"

#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "UERingPropertySerializer.h"
#include "UERingSemanticUtils.h"

namespace UERingAnimationAssetExporter
{
    TSharedRef<FJsonObject> Vector(const FVector& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("x"), Value.X);
        Json->SetNumberField(TEXT("y"), Value.Y);
        Json->SetNumberField(TEXT("z"), Value.Z);
        return Json;
    }

    TSharedRef<FJsonObject> Transform(const FTransform& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetObjectField(TEXT("translation"), Vector(Value.GetTranslation()));
        Json->SetObjectField(TEXT("rotationEuler"), Vector(Value.Rotator().Euler()));
        Json->SetObjectField(TEXT("scale"), Vector(Value.GetScale3D()));
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> FloatCurves(const IAnimationDataModel& Model)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const FFloatCurve& Curve : Model.GetFloatCurves())
        {
            TArray<float> Times;
            TArray<float> Values;
            Curve.GetKeys(Times, Values);
            const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
            Json->SetStringField(TEXT("name"), Curve.GetName().ToString());
            Json->SetNumberField(TEXT("flags"), Curve.GetCurveTypeFlags());
            TArray<TSharedPtr<FJsonValue>> Keys;
            for (int32 Index = 0; Index < FMath::Min(Times.Num(), Values.Num()); ++Index)
            {
                const TSharedRef<FJsonObject> Key = MakeShared<FJsonObject>();
                Key->SetNumberField(TEXT("time"), Times[Index]);
                Key->SetNumberField(TEXT("value"), Values[Index]);
                Keys.Add(MakeShared<FJsonValueObject>(Key));
            }
            Json->SetArrayField(TEXT("keys"), Keys);
            Result.Add(MakeShared<FJsonValueObject>(Json));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> TransformCurves(const IAnimationDataModel& Model)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const FTransformCurve& Curve : Model.GetTransformCurves())
        {
            TArray<float> Times;
            TArray<FTransform> Values;
            Curve.GetKeys(Times, Values);
            const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
            Json->SetStringField(TEXT("name"), Curve.GetName().ToString());
            Json->SetNumberField(TEXT("flags"), Curve.GetCurveTypeFlags());
            TArray<TSharedPtr<FJsonValue>> Keys;
            for (int32 Index = 0; Index < FMath::Min(Times.Num(), Values.Num()); ++Index)
            {
                const TSharedRef<FJsonObject> Key = MakeShared<FJsonObject>();
                Key->SetNumberField(TEXT("time"), Times[Index]);
                Key->SetObjectField(TEXT("value"), Transform(Values[Index]));
                Keys.Add(MakeShared<FJsonValueObject>(Key));
            }
            Json->SetArrayField(TEXT("keys"), Keys);
            Result.Add(MakeShared<FJsonValueObject>(Json));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> AttributeInterfaces(const IAnimationDataModel& Model)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const FAnimatedBoneAttribute& Attribute : Model.GetAttributes())
        {
            const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
            Json->SetStringField(TEXT("name"), Attribute.Identifier.GetName().ToString());
            Json->SetStringField(TEXT("boneName"), Attribute.Identifier.GetBoneName().ToString());
            Json->SetNumberField(TEXT("boneIndex"), Attribute.Identifier.GetBoneIndex());
            Json->SetStringField(TEXT("valueType"), Attribute.Identifier.GetScriptStructPath().ToString());
            TArray<TSharedPtr<FJsonValue>> Times;
            for (const FAttributeKey& Key : Attribute.Curve.GetConstRefOfKeys())
            {
                Times.Add(MakeShared<FJsonValueNumber>(Key.Time));
            }
            Json->SetArrayField(TEXT("keyTimes"), Times);
            Result.Add(MakeShared<FJsonValueObject>(Json));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> SyncMarkers(const TConstArrayView<FAnimSyncMarker> Markers)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Markers.Num());
        for (const FAnimSyncMarker& Marker : Markers)
        {
            const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
            Json->SetStringField(TEXT("name"), Marker.MarkerName.ToString());
            Json->SetNumberField(TEXT("time"), Marker.Time);
#if WITH_EDITORONLY_DATA
            Json->SetNumberField(TEXT("trackIndex"), Marker.TrackIndex);
            Json->SetStringField(TEXT("guid"), Marker.Guid.ToString(EGuidFormats::DigitsWithHyphens));
#endif
            Result.Add(MakeShared<FJsonValueObject>(Json));
        }
        return Result;
    }

    TArray<FName> CommonProperties()
    {
        return {
            TEXT("Skeleton"),
            TEXT("MetaData"),
            TEXT("AssetUserData"),
            TEXT("PreviewPoseAsset")
        };
    }

    TArray<FName> LogicProperties(const UObject& Asset)
    {
        TArray<FName> Properties = CommonProperties();
        if (Asset.IsA<UAnimMontage>())
        {
            Properties.Append({
                TEXT("CompositeSections"),
                TEXT("SlotAnimTracks"),
                TEXT("Notifies"),
                TEXT("BlendIn"),
                TEXT("BlendOut"),
                TEXT("BlendOutTriggerTime"),
                TEXT("RateScale"),
                TEXT("SyncGroup"),
                TEXT("SyncSlotIndex"),
                TEXT("bEnableAutoBlendOut"),
                TEXT("bEnableRootMotionTranslation"),
                TEXT("bEnableRootMotionRotation")
            });
        }
        else if (Asset.IsA<UBlendSpace>())
        {
            Properties.Append({
                TEXT("BlendParameters"),
                TEXT("SampleData"),
                TEXT("InterpolationParam"),
                TEXT("NotifyTriggerMode"),
                TEXT("TargetWeightInterpolationSpeedPerSec"),
                TEXT("PerBoneBlend"),
                TEXT("AxisToScaleAnimation"),
                TEXT("bAllowMeshSpaceBlending"),
                TEXT("bLoop"),
                TEXT("SampleIndexWithMarkers"),
                TEXT("AnalysisProperties")
            });
        }
        else if (Asset.IsA<UAnimSequenceBase>())
        {
            Properties.Append({
                TEXT("Notifies"),
                TEXT("RateScale"),
                TEXT("bLoop"),
                TEXT("bEnableRootMotion"),
                TEXT("RootMotionRootLock"),
                TEXT("bForceRootLock"),
                TEXT("AdditiveAnimType"),
                TEXT("RefPoseType"),
                TEXT("RefPoseSeq"),
                TEXT("RefFrameIndex"),
                TEXT("Interpolation"),
                TEXT("RetargetSource"),
                TEXT("RetargetSourceAssetReferencePose"),
                TEXT("BoneCompressionSettings"),
                TEXT("CurveCompressionSettings")
            });
        }
        return Properties;
    }

    FString Role(const UObject& Asset)
    {
        if (Asset.IsA<UAnimMontage>()) return TEXT("montage");
        if (Asset.IsA<UBlendSpace>()) return TEXT("blendSpace");
        if (Asset.IsA<UAnimSequence>()) return TEXT("sequence");
        if (Asset.IsA<UAnimSequenceBase>()) return TEXT("timeline");
        return TEXT("animationAsset");
    }
}

FName FUERingAnimationAssetExporter::GetName() const
{
    return TEXT("AnimationLogic");
}

bool FUERingAnimationAssetExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.IsInstanceOf(UAnimationAsset::StaticClass());
}

bool FUERingAnimationAssetExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    UAnimationAsset* Asset = Cast<UAnimationAsset>(Context.Asset.Get());
    if (Asset == nullptr)
    {
        OutError = TEXT("The loaded object is not an animation asset.");
        return false;
    }

    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("AnimationLogic"));
    Semantics->SetStringField(TEXT("representation"), TEXT("animation-logic-v1"));
    Semantics->SetStringField(TEXT("role"), UERingAnimationAssetExporter::Role(*Asset));
    Semantics->SetStringField(TEXT("assetClass"), Asset->GetClass()->GetPathName());

    if (Context.Profile != EUERingExportProfile::Logic)
    {
        Semantics->SetArrayField(
            TEXT("properties"),
            UERingPropertySerializer::SerializeObjectProperties(*Asset));
        OutPayload.Semantics = Semantics;
        return true;
    }

    UERingSemanticUtils::SetSelectedProperties(
        *Asset,
        UERingAnimationAssetExporter::LogicProperties(*Asset),
        Semantics,
        TEXT("logicProperties"));

#if WITH_EDITOR
    if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Asset))
    {
        int64 PoseTrackCount = INDEX_NONE;
        const TSharedRef<FJsonObject> Timeline = MakeShared<FJsonObject>();
        Timeline->SetNumberField(TEXT("playLength"), Sequence->GetPlayLength());

        if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Sequence))
        {
            const TArray<TSharedPtr<FJsonValue>> Markers =
                UERingAnimationAssetExporter::SyncMarkers(AnimSequence->AuthoredSyncMarkers);
            if (!Markers.IsEmpty())
            {
                Timeline->SetArrayField(TEXT("syncMarkers"), Markers);
            }
        }
        else if (const UAnimMontage* Montage = Cast<UAnimMontage>(Sequence))
        {
            const TArray<TSharedPtr<FJsonValue>> Markers =
                UERingAnimationAssetExporter::SyncMarkers(Montage->MarkerData.AuthoredSyncMarkers);
            if (!Markers.IsEmpty())
            {
                Timeline->SetArrayField(TEXT("syncMarkers"), Markers);
            }
        }

        if (Sequence->GetSkeleton() != nullptr)
        {
            const int32 SampledKeyCount = Sequence->GetNumberOfSampledKeys();
            const FFrameRate SampledFrameRate = Sequence->GetSamplingFrameRate();
            Timeline->SetNumberField(TEXT("sampledFrameCount"), FMath::Max(0, SampledKeyCount - 1));
            Timeline->SetNumberField(TEXT("sampledKeyCount"), SampledKeyCount);
            Timeline->SetNumberField(TEXT("sampledFrameRateNumerator"), SampledFrameRate.Numerator);
            Timeline->SetNumberField(TEXT("sampledFrameRateDenominator"), SampledFrameRate.Denominator);

            if (const IAnimationDataModel* Model = Sequence->GetDataModel())
            {
                Timeline->SetNumberField(TEXT("sourceFrameCount"), Model->GetNumberOfFrames());
                Timeline->SetNumberField(TEXT("sourceKeyCount"), Model->GetNumberOfKeys());
                Timeline->SetNumberField(TEXT("boneTrackCount"), Model->GetNumBoneTracks());
                const FFrameRate SourceFrameRate = Model->GetFrameRate();
                Timeline->SetNumberField(TEXT("sourceFrameRateNumerator"), SourceFrameRate.Numerator);
                Timeline->SetNumberField(TEXT("sourceFrameRateDenominator"), SourceFrameRate.Denominator);
                Timeline->SetArrayField(TEXT("floatCurves"),
                    UERingAnimationAssetExporter::FloatCurves(*Model));
                Timeline->SetArrayField(TEXT("transformCurves"),
                    UERingAnimationAssetExporter::TransformCurves(*Model));
                const TArray<TSharedPtr<FJsonValue>> Attributes =
                    UERingAnimationAssetExporter::AttributeInterfaces(*Model);
                if (!Attributes.IsEmpty())
                {
                    Timeline->SetArrayField(TEXT("attributeInterfaces"), Attributes);
                    UERingSemanticUtils::AddOmission(
                        OutPayload,
                        TEXT("/semantics/timeline/attributeInterfaces/*/values"),
                        TEXT("opaqueAnimationAttributeValues"),
                        TEXT("Attribute names, bone bindings, types, and key times are preserved, but custom UScriptStruct key payloads are not available through the stable animation data-model interface."),
                        TEXT("customAttributeBehaviorRequiresSourceAsset"),
                        Attributes.Num(),
                        Context.SourceHash);
                }
                PoseTrackCount = Model->GetNumBoneTracks();
            }
        }
        Semantics->SetObjectField(TEXT("timeline"), Timeline);
        if (Asset->IsA<UAnimSequence>())
        {
            UERingSemanticUtils::AddOmission(
                OutPayload,
                TEXT("/semantics/timeline/rawPoseTracks"),
                TEXT("replaceableAnimationPoseSamples"),
                TEXT("Logic profile preserves notifies, sync markers, root-motion settings, curves, attributes, timings, and referenced assets while omitting replaceable per-bone pose samples."),
                TEXT("presentationAssetNotReconstructable"),
                PoseTrackCount,
                Context.SourceHash);
        }
    }
#endif

    OutPayload.Semantics = Semantics;
    return true;
}
