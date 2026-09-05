#include "UERingWorldExporter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "UERingPropertySerializer.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionHelpers.h"

namespace UERingWorldExporter
{
    FTransform PersistentWorldTransform(
        const USceneComponent& Component,
        TSet<const USceneComponent*>& Visited)
    {
        const FTransform RelativeTransform = Component.GetRelativeTransform();
        const USceneComponent* Parent = Component.GetAttachParent();
        if (Parent == nullptr || Visited.Contains(&Component))
        {
            return RelativeTransform;
        }

        Visited.Add(&Component);
        FTransform WorldTransform = RelativeTransform * PersistentWorldTransform(*Parent, Visited);
        Visited.Remove(&Component);

        if (Component.IsUsingAbsoluteLocation())
        {
            WorldTransform.CopyTranslation(RelativeTransform);
        }
        if (Component.IsUsingAbsoluteRotation())
        {
            WorldTransform.CopyRotation(RelativeTransform);
        }
        if (Component.IsUsingAbsoluteScale())
        {
            WorldTransform.CopyScale3D(RelativeTransform);
        }
        return WorldTransform;
    }

    FTransform PersistentActorTransform(const AActor& Actor)
    {
        if (const USceneComponent* RootComponent = Actor.GetRootComponent())
        {
            TSet<const USceneComponent*> Visited;
            return PersistentWorldTransform(*RootComponent, Visited);
        }
        return Actor.GetActorTransform();
    }

    TSharedRef<FJsonObject> Vector(const FVector& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("x"), Value.X);
        Json->SetNumberField(TEXT("y"), Value.Y);
        Json->SetNumberField(TEXT("z"), Value.Z);
        return Json;
    }

    TSharedRef<FJsonObject> Rotator(const FRotator& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("pitch"), Value.Pitch);
        Json->SetNumberField(TEXT("yaw"), Value.Yaw);
        Json->SetNumberField(TEXT("roll"), Value.Roll);
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> Strings(TArray<FString> Values)
    {
        Values.Sort();
        TArray<TSharedPtr<FJsonValue>> Result;
        for (const FString& Value : Values)
        {
            Result.Add(MakeShared<FJsonValueString>(Value));
        }
        return Result;
    }

    FString ActorKey(const AActor& Actor)
    {
        return Actor.GetActorGuid().IsValid()
            ? Actor.GetActorGuid().ToString(EGuidFormats::DigitsWithHyphensLower)
            : Actor.GetPathName();
    }

    TSharedRef<FJsonObject> Transform(const FTransform& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetObjectField(TEXT("location"), Vector(Value.GetLocation()));
        Json->SetObjectField(TEXT("rotation"), Rotator(Value.Rotator()));
        Json->SetObjectField(TEXT("scale"), Vector(Value.GetScale3D()));
        return Json;
    }

    TSharedRef<FJsonObject> Bounds(const FBox& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetBoolField(TEXT("valid"), Value.IsValid != 0);
        if (Value.IsValid)
        {
            Json->SetObjectField(TEXT("min"), Vector(Value.Min));
            Json->SetObjectField(TEXT("max"), Vector(Value.Max));
        }
        return Json;
    }

    TSharedRef<FJsonObject> SerializeActorDescriptor(const FWorldPartitionActorDescInstance& Descriptor)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("guid"), Descriptor.GetGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        Json->SetStringField(TEXT("name"), Descriptor.GetActorName().ToString());
        Json->SetStringField(TEXT("label"), Descriptor.GetActorLabel().ToString());
        Json->SetStringField(TEXT("baseClass"), Descriptor.GetBaseClass().ToString());
        Json->SetStringField(TEXT("nativeClass"), Descriptor.GetNativeClass().ToString());
        Json->SetStringField(TEXT("actorPackage"), Descriptor.GetActorPackage().ToString());
        Json->SetStringField(TEXT("actorSoftPath"), Descriptor.GetActorSoftPath().ToString());
        Json->SetStringField(TEXT("folder"), Descriptor.GetFolderPath().ToString());
        Json->SetStringField(TEXT("runtimeGrid"), Descriptor.GetRuntimeGrid().ToString());
        Json->SetBoolField(TEXT("spatiallyLoaded"), Descriptor.GetIsSpatiallyLoaded());
        Json->SetBoolField(TEXT("editorOnly"), Descriptor.GetActorIsEditorOnly());
        Json->SetBoolField(TEXT("runtimeOnly"), Descriptor.GetActorIsRuntimeOnly());
        Json->SetBoolField(TEXT("runtimeRelevant"), Descriptor.IsRuntimeRelevant());
        Json->SetBoolField(TEXT("editorRelevant"), Descriptor.IsEditorRelevant());
        Json->SetBoolField(TEXT("hlodRelevant"), Descriptor.GetActorIsHLODRelevant());
        Json->SetBoolField(TEXT("mainWorldOnly"), Descriptor.IsMainWorldOnly());
        Json->SetObjectField(TEXT("transform"), Transform(Descriptor.GetActorTransform()));
        Json->SetObjectField(TEXT("editorBounds"), Bounds(Descriptor.GetEditorBounds()));
        Json->SetObjectField(TEXT("runtimeBounds"), Bounds(Descriptor.GetRuntimeBounds()));

        TArray<FString> Tags;
        for (const FName Tag : Descriptor.GetTags()) Tags.Add(Tag.ToString());
        Json->SetArrayField(TEXT("tags"), Strings(MoveTemp(Tags)));
        TArray<FString> DataLayers;
        for (const FName DataLayer : Descriptor.GetDataLayers()) DataLayers.Add(DataLayer.ToString());
        Json->SetArrayField(TEXT("dataLayers"), Strings(MoveTemp(DataLayers)));
        TArray<FString> References;
        for (const FGuid& Reference : Descriptor.GetReferences())
        {
            References.Add(Reference.ToString(EGuidFormats::DigitsWithHyphensLower));
        }
        Json->SetArrayField(TEXT("references"), Strings(MoveTemp(References)));
        if (Descriptor.GetParentActor().IsValid())
        {
            Json->SetStringField(
                TEXT("parentActorGuid"),
                Descriptor.GetParentActor().ToString(EGuidFormats::DigitsWithHyphensLower));
        }
        if (Descriptor.GetContentBundleGuid().IsValid())
        {
            Json->SetStringField(
                TEXT("contentBundleGuid"),
                Descriptor.GetContentBundleGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
        }
        if (Descriptor.GetExternalDataLayerAsset().IsValid())
        {
            Json->SetStringField(TEXT("externalDataLayerAsset"), Descriptor.GetExternalDataLayerAsset().ToString());
        }
        return Json;
    }

    TSharedRef<FJsonObject> SerializeLoadedActor(const AActor& Actor)
    {
        const TSharedRef<FJsonObject> JsonActor = MakeShared<FJsonObject>();
        JsonActor->SetStringField(TEXT("guid"), ActorKey(Actor));
        JsonActor->SetStringField(TEXT("name"), Actor.GetName());
        JsonActor->SetStringField(TEXT("label"), Actor.GetActorLabel());
        JsonActor->SetStringField(TEXT("class"), Actor.GetClass()->GetPathName());
        JsonActor->SetStringField(
            TEXT("archetype"),
            Actor.GetArchetype() != nullptr ? Actor.GetArchetype()->GetPathName() : FString());
        JsonActor->SetStringField(TEXT("folder"), Actor.GetFolderPath().ToString());
        JsonActor->SetBoolField(TEXT("externalPackage"), Actor.IsPackageExternal());
        JsonActor->SetBoolField(TEXT("editorOnly"), Actor.IsEditorOnly());
        JsonActor->SetObjectField(TEXT("transform"), Transform(PersistentActorTransform(Actor)));
        JsonActor->SetStringField(
            TEXT("transformSource"),
            Actor.GetRootComponent() != nullptr
                ? TEXT("persistentRootComponentHierarchy")
                : TEXT("actorRuntimeTransformFallback"));

        TArray<FString> Tags;
        for (const FName Tag : Actor.Tags) Tags.Add(Tag.ToString());
        JsonActor->SetArrayField(TEXT("tags"), Strings(MoveTemp(Tags)));
        JsonActor->SetArrayField(
            TEXT("properties"),
            UERingPropertySerializer::SerializeObjectProperties(
                Actor,
                Actor.GetArchetype(),
                true,
                CPF_Edit | CPF_SaveGame));

        TInlineComponentArray<UActorComponent*> Components(const_cast<AActor*>(&Actor));
        Components.Sort([](const UActorComponent& Left, const UActorComponent& Right)
        {
            return Left.GetName() < Right.GetName();
        });
        TArray<TSharedPtr<FJsonValue>> JsonComponents;
        for (const UActorComponent* Component : Components)
        {
            const TSharedRef<FJsonObject> JsonComponent = MakeShared<FJsonObject>();
            JsonComponent->SetStringField(TEXT("name"), Component->GetName());
            JsonComponent->SetStringField(TEXT("class"), Component->GetClass()->GetPathName());
            JsonComponent->SetStringField(
                TEXT("archetype"),
                Component->GetArchetype() != nullptr ? Component->GetArchetype()->GetPathName() : FString());
            JsonComponent->SetStringField(TEXT("creationMethod"), UEnum::GetValueAsString(Component->CreationMethod));
            JsonComponent->SetBoolField(TEXT("active"), Component->IsActive());
            TArray<FString> ComponentTags;
            for (const FName Tag : Component->ComponentTags) ComponentTags.Add(Tag.ToString());
            JsonComponent->SetArrayField(TEXT("tags"), Strings(MoveTemp(ComponentTags)));
            JsonComponent->SetArrayField(
                TEXT("properties"),
                UERingPropertySerializer::SerializeObjectProperties(
                    *Component,
                    Component->GetArchetype(),
                    true,
                    CPF_Edit | CPF_SaveGame));
            if (const USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
            {
                JsonComponent->SetStringField(
                    TEXT("attachParent"),
                    SceneComponent->GetAttachParent() != nullptr
                        ? SceneComponent->GetAttachParent()->GetName()
                        : FString());
                JsonComponent->SetStringField(TEXT("attachSocket"), SceneComponent->GetAttachSocketName().ToString());
                JsonComponent->SetObjectField(TEXT("relativeTransform"), Transform(SceneComponent->GetRelativeTransform()));
            }
            JsonComponents.Add(MakeShared<FJsonValueObject>(JsonComponent));
        }
        JsonActor->SetArrayField(TEXT("components"), JsonComponents);
        return JsonActor;
    }
}

FName FUERingWorldExporter::GetName() const
{
    return TEXT("World");
}

bool FUERingWorldExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.IsInstanceOf(UWorld::StaticClass());
}

bool FUERingWorldExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    using namespace UERingWorldExporter;

    UWorld* World = Cast<UWorld>(Context.Asset.Get());
    if (World == nullptr || World->PersistentLevel == nullptr)
    {
        OutError = TEXT("The loaded object is not a valid World with a persistent level.");
        return false;
    }

    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("World"));
    Semantics->SetStringField(TEXT("worldType"), LexToString(World->WorldType.GetValue()));
    Semantics->SetBoolField(TEXT("usesWorldPartition"), World->GetWorldPartition() != nullptr);
    Semantics->SetBoolField(TEXT("usesExternalActors"), World->PersistentLevel->IsUsingExternalActors());
    Semantics->SetStringField(
        TEXT("worldPartitionClass"),
        World->GetWorldPartition() != nullptr ? World->GetWorldPartition()->GetClass()->GetPathName() : FString());
    Semantics->SetStringField(
        TEXT("actorScope"),
        World->GetWorldPartition() != nullptr
            ? TEXT("persistentLevelAndWorldPartitionActors")
            : TEXT("persistentLevelSavedActors"));
    Semantics->SetBoolField(TEXT("runtimeSpawnedActorsExcluded"), true);
    Semantics->SetNumberField(TEXT("persistentLevelActorSlots"), World->PersistentLevel->Actors.Num());

    TArray<AActor*> Actors;
    int32 InvalidActorSlots = 0;
    int32 TransientActors = 0;
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (!IsValid(Actor))
        {
            ++InvalidActorSlots;
        }
        else if (Actor->HasAnyFlags(RF_Transient))
        {
            ++TransientActors;
        }
        else
        {
            Actors.Add(Actor);
        }
    }
    Semantics->SetNumberField(TEXT("sourceActorCount"), Actors.Num());
    Semantics->SetNumberField(TEXT("skippedInvalidActorSlots"), InvalidActorSlots);
    Semantics->SetNumberField(TEXT("skippedTransientActors"), TransientActors);
    Actors.Sort([](const AActor& Left, const AActor& Right)
    {
        return ActorKey(Left) < ActorKey(Right);
    });

    TArray<TSharedPtr<FJsonValue>> JsonActors;
    JsonActors.Reserve(Actors.Num());
    for (const AActor* Actor : Actors)
    {
        const TSharedRef<FJsonObject> JsonActor = MakeShared<FJsonObject>();
        JsonActor->SetStringField(TEXT("guid"), ActorKey(*Actor));
        JsonActor->SetStringField(TEXT("name"), Actor->GetName());
        JsonActor->SetStringField(TEXT("label"), Actor->GetActorLabel());
        JsonActor->SetStringField(TEXT("class"), Actor->GetClass()->GetPathName());
        JsonActor->SetStringField(
            TEXT("archetype"),
            Actor->GetArchetype() != nullptr ? Actor->GetArchetype()->GetPathName() : FString());
        JsonActor->SetStringField(TEXT("folder"), Actor->GetFolderPath().ToString());
        JsonActor->SetBoolField(TEXT("externalPackage"), Actor->IsPackageExternal());
        JsonActor->SetBoolField(TEXT("editorOnly"), Actor->IsEditorOnly());

        // In package-loaded inactive worlds ComponentToWorld is often stale identity data.
        // Rebuild from serialized relative transforms without registering components.
        const FTransform Transform = PersistentActorTransform(*Actor);
        const TSharedRef<FJsonObject> JsonTransform = MakeShared<FJsonObject>();
        JsonTransform->SetObjectField(TEXT("location"), Vector(Transform.GetLocation()));
        JsonTransform->SetObjectField(TEXT("rotation"), Rotator(Transform.Rotator()));
        JsonTransform->SetObjectField(TEXT("scale"), Vector(Transform.GetScale3D()));
        JsonActor->SetObjectField(TEXT("transform"), JsonTransform);
        JsonActor->SetStringField(
            TEXT("transformSource"),
            Actor->GetRootComponent() != nullptr
                ? TEXT("persistentRootComponentHierarchy")
                : TEXT("actorRuntimeTransformFallback"));

        TArray<FString> Tags;
        for (const FName Tag : Actor->Tags)
        {
            Tags.Add(Tag.ToString());
        }
        JsonActor->SetArrayField(TEXT("tags"), Strings(MoveTemp(Tags)));
        JsonActor->SetArrayField(
            TEXT("properties"),
            UERingPropertySerializer::SerializeObjectProperties(
                *Actor,
                Actor->GetArchetype(),
                true,
                CPF_Edit | CPF_SaveGame));

        TInlineComponentArray<UActorComponent*> Components(Actor);
        Components.Sort([](const UActorComponent& Left, const UActorComponent& Right)
        {
            return Left.GetName() < Right.GetName();
        });
        TArray<TSharedPtr<FJsonValue>> JsonComponents;
        for (const UActorComponent* Component : Components)
        {
            const TSharedRef<FJsonObject> JsonComponent = MakeShared<FJsonObject>();
            JsonComponent->SetStringField(TEXT("name"), Component->GetName());
            JsonComponent->SetStringField(TEXT("class"), Component->GetClass()->GetPathName());
            JsonComponent->SetStringField(
                TEXT("archetype"),
                Component->GetArchetype() != nullptr ? Component->GetArchetype()->GetPathName() : FString());
            JsonComponent->SetStringField(TEXT("creationMethod"), UEnum::GetValueAsString(Component->CreationMethod));
            JsonComponent->SetBoolField(TEXT("active"), Component->IsActive());
            TArray<FString> ComponentTags;
            for (const FName Tag : Component->ComponentTags)
            {
                ComponentTags.Add(Tag.ToString());
            }
            JsonComponent->SetArrayField(TEXT("tags"), Strings(MoveTemp(ComponentTags)));
            JsonComponent->SetArrayField(
                TEXT("properties"),
                UERingPropertySerializer::SerializeObjectProperties(
                    *Component,
                    Component->GetArchetype(),
                    true,
                    CPF_Edit | CPF_SaveGame));
            if (const USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
            {
                JsonComponent->SetStringField(
                    TEXT("attachParent"),
                    SceneComponent->GetAttachParent() != nullptr
                        ? SceneComponent->GetAttachParent()->GetName()
                        : FString());
                JsonComponent->SetStringField(TEXT("attachSocket"), SceneComponent->GetAttachSocketName().ToString());
                const FTransform RelativeTransform = SceneComponent->GetRelativeTransform();
                const TSharedRef<FJsonObject> Relative = MakeShared<FJsonObject>();
                Relative->SetObjectField(TEXT("location"), Vector(RelativeTransform.GetLocation()));
                Relative->SetObjectField(TEXT("rotation"), Rotator(RelativeTransform.Rotator()));
                Relative->SetObjectField(TEXT("scale"), Vector(RelativeTransform.GetScale3D()));
                JsonComponent->SetObjectField(TEXT("relativeTransform"), Relative);
            }
            JsonComponents.Add(MakeShared<FJsonValueObject>(JsonComponent));
        }
        JsonActor->SetArrayField(TEXT("components"), JsonComponents);
        JsonActors.Add(MakeShared<FJsonValueObject>(JsonActor));
    }
    Semantics->SetArrayField(TEXT("actors"), JsonActors);

    if (UWorldPartition* WorldPartition = World->GetWorldPartition())
    {
        const bool bInitializeForExport = !WorldPartition->IsInitialized();
        if (bInitializeForExport)
        {
            WorldPartition->Initialize(World, FTransform::Identity);
            if (!WorldPartition->IsInitialized())
            {
                OutError = TEXT("World Partition actor descriptor container could not be initialized.");
                return false;
            }
        }

        TArray<const FWorldPartitionActorDescInstance*> Descriptors;
        FWorldPartitionHelpers::ForEachActorDescInstance(
            WorldPartition,
            [&Descriptors](const FWorldPartitionActorDescInstance* Descriptor)
            {
                if (Descriptor != nullptr && Descriptor->IsValid()) Descriptors.Add(Descriptor);
                return true;
            });
        Descriptors.Sort([](
            const FWorldPartitionActorDescInstance& Left,
            const FWorldPartitionActorDescInstance& Right)
        {
            return Left.GetGuid().ToString(EGuidFormats::Digits)
                < Right.GetGuid().ToString(EGuidFormats::Digits);
        });

        TSet<FGuid> PersistentActorGuids;
        for (const AActor* Actor : Actors)
        {
            if (Actor->GetActorGuid().IsValid()) PersistentActorGuids.Add(Actor->GetActorGuid());
        }

        int32 LoadedActorCount = 0;
        int32 LoadFailureCount = 0;
        int32 DuplicatePersistentActorCount = 0;
        TArray<FString> ActorPackages;
        TArray<TSharedPtr<FJsonValue>> JsonDescriptors;
        JsonDescriptors.Reserve(Descriptors.Num());
        for (const FWorldPartitionActorDescInstance* Descriptor : Descriptors)
        {
            if (PersistentActorGuids.Contains(Descriptor->GetGuid()))
            {
                ++DuplicatePersistentActorCount;
                continue;
            }

            const TSharedRef<FJsonObject> JsonDescriptor = SerializeActorDescriptor(*Descriptor);
            ActorPackages.Add(Descriptor->GetActorPackage().ToString());
            OutPayload.AdditionalHardDependencies.Add(Descriptor->GetActorPackage());
            FWorldPartitionReference Reference(
                const_cast<FWorldPartitionActorDescInstance*>(Descriptor));
            if (AActor* LoadedActor = Reference.GetActor())
            {
                JsonDescriptor->SetStringField(TEXT("serializationStatus"), TEXT("full"));
                JsonDescriptor->SetObjectField(TEXT("actor"), SerializeLoadedActor(*LoadedActor));
                ++LoadedActorCount;
            }
            else
            {
                JsonDescriptor->SetStringField(TEXT("serializationStatus"), TEXT("descriptorOnly"));
                JsonDescriptor->SetStringField(TEXT("loadFailure"), Descriptor->GetUnloadedReason().ToString());
                ++LoadFailureCount;
            }
            JsonDescriptors.Add(MakeShared<FJsonValueObject>(JsonDescriptor));
        }
        Semantics->SetNumberField(TEXT("worldPartitionActorDescriptorCount"), JsonDescriptors.Num());
        Semantics->SetNumberField(TEXT("worldPartitionActorLoadedCount"), LoadedActorCount);
        Semantics->SetNumberField(TEXT("worldPartitionActorLoadFailureCount"), LoadFailureCount);
        Semantics->SetNumberField(TEXT("worldPartitionPersistentDuplicateCount"), DuplicatePersistentActorCount);
        Semantics->SetArrayField(TEXT("worldPartitionActorPackages"), Strings(MoveTemp(ActorPackages)));
        Semantics->SetArrayField(TEXT("worldPartitionActors"), MoveTemp(JsonDescriptors));

        TArray<FString> ExternalObjectPackages;
        const FString WorldPackageName = Context.AssetData.PackageName.ToString();
        int32 RelativePathStart = INDEX_NONE;
        if (WorldPackageName.FindChar(TEXT('/'), RelativePathStart))
        {
            RelativePathStart = WorldPackageName.Find(
                TEXT("/"),
                ESearchCase::CaseSensitive,
                ESearchDir::FromStart,
                RelativePathStart + 1);
        }
        if (RelativePathStart != INDEX_NONE)
        {
            const FString ExternalObjectRoot = WorldPackageName.Left(RelativePathStart)
                + TEXT("/__ExternalObjects__/")
                + WorldPackageName.Mid(RelativePathStart + 1);
            const FString ExternalObjectDirectory = FPackageName::LongPackageNameToFilename(ExternalObjectRoot);
            TArray<FString> ExternalObjectFiles;
            IFileManager::Get().FindFilesRecursive(
                ExternalObjectFiles,
                *ExternalObjectDirectory,
                TEXT("*.uasset"),
                true,
                false);
            for (const FString& ExternalObjectFile : ExternalObjectFiles)
            {
                FString PackageName;
                if (FPackageName::TryConvertFilenameToLongPackageName(ExternalObjectFile, PackageName))
                {
                    ExternalObjectPackages.Add(PackageName);
                    OutPayload.AdditionalHardDependencies.Add(FName(*PackageName));
                }
            }
        }
        Semantics->SetNumberField(
            TEXT("worldPartitionExternalObjectPackageCount"),
            ExternalObjectPackages.Num());
        Semantics->SetArrayField(
            TEXT("worldPartitionExternalObjectPackages"),
            Strings(MoveTemp(ExternalObjectPackages)));

        // Loading the actor descriptors also registers external actor/object package edges
        // that are unavailable from the commandlet's initial Asset Registry snapshot.
        IAssetRegistry& AssetRegistry =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        AssetRegistry.GetDependencies(
            Context.AssetData.PackageName,
            OutPayload.AdditionalHardDependencies,
            UE::AssetRegistry::EDependencyCategory::Package,
            UE::AssetRegistry::EDependencyQuery::Hard);
        AssetRegistry.GetDependencies(
            Context.AssetData.PackageName,
            OutPayload.AdditionalSoftDependencies,
            UE::AssetRegistry::EDependencyCategory::Package,
            UE::AssetRegistry::EDependencyQuery::Soft);
        if (bInitializeForExport)
        {
            WorldPartition->Uninitialize();
        }
    }

    TArray<FString> StreamingLevels;
    for (const ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (StreamingLevel != nullptr)
        {
            StreamingLevels.Add(StreamingLevel->GetWorldAssetPackageName());
        }
    }
    Semantics->SetArrayField(TEXT("streamingLevels"), Strings(MoveTemp(StreamingLevels)));
    OutPayload.Semantics = Semantics;
    return true;
}
