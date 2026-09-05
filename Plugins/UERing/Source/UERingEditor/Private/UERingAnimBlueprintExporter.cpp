#include "UERingAnimBlueprintExporter.h"

#include "Engine/Blueprint.h"
#include "UERingBlueprintExporter.h"
#include "UERingSemanticUtils.h"

FName FUERingAnimBlueprintExporter::GetName() const
{
    return TEXT("AnimBlueprint");
}

bool FUERingAnimBlueprintExporter::CanExport(const FAssetData& AssetData) const
{
    const FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
    return AssetData.IsInstanceOf(UBlueprint::StaticClass())
        && (ClassName.Contains(TEXT("AnimBlueprint")) || ClassName.Contains(TEXT("AnimBP")));
}

bool FUERingAnimBlueprintExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    UBlueprint* Blueprint = Cast<UBlueprint>(Context.Asset.Get());
    if (Blueprint == nullptr)
    {
        OutError = TEXT("The loaded object is not an animation Blueprint.");
        return false;
    }

    FUERingBlueprintExporter BaseExporter;
    if (!BaseExporter.BuildPayload(Context, OutPayload, OutError))
    {
        return false;
    }
    OutPayload.Semantics->SetStringField(TEXT("kind"), TEXT("AnimBlueprint"));
    OutPayload.Semantics->SetStringField(TEXT("baseKind"), TEXT("Blueprint"));
    OutPayload.Semantics->SetStringField(TEXT("animationBlueprintClass"), Blueprint->GetClass()->GetPathName());
    OutPayload.Semantics->SetStringField(TEXT("representation"), TEXT("animation-blueprint-graph-v1"));
    UERingSemanticUtils::SetSelectedProperties(
        *Blueprint,
        {
            TEXT("TargetSkeleton"),
            TEXT("PreviewSkeletalMesh"),
            TEXT("bUseMultiThreadedAnimationUpdate"),
            TEXT("bWarnAboutBlueprintUsage"),
            TEXT("ParentAssetOverrides")
        },
        OutPayload.Semantics,
        TEXT("animation"));
    UERingSemanticUtils::AddOmission(
        OutPayload,
        TEXT("/semantics/compiledAnimClass"),
        TEXT("derivedCompiledArtifact"),
        TEXT("Animation graphs, state machines, transitions, pins, defaults, notifies, and asset references are preserved; generated animation bytecode and editor caches are derived and omitted."),
        TEXT("noSemanticBehaviorLoss"));
    return true;
}
