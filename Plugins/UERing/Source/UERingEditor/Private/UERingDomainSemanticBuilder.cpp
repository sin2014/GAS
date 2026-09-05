#include "UERingDomainSemanticBuilder.h"

#include "Algo/Unique.h"
#include "Engine/Blueprint.h"
#include "Engine/DataAsset.h"
#include "Misc/PackageName.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "UERingPropertySerializer.h"
#include "UERingSettings.h"
#include "UERingVersion.h"
#include "UObject/AnsiStrProperty.h"
#include "UObject/PropertyOptional.h"
#include "UObject/TextProperty.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "UObject/Utf8StrProperty.h"

namespace UERingDomainSemanticBuilder
{
    namespace
    {
        struct FDomainRule
        {
            const TCHAR* Key;
            TArray<FString> ClassMarkers;
            TArray<FString> PropertyMarkers;
        };

        const TArray<FDomainRule>& Rules()
        {
            static const TArray<FDomainRule> Value = {
                {
                    TEXT("gas"),
                    { TEXT("GameplayAbility"), TEXT("GameplayEffect"), TEXT("AttributeSet"),
                      TEXT("LyraAbilitySet"), TEXT("LyraAbilityTagRelationshipMapping"),
                      TEXT("GameplayCueNotify_Actor"), TEXT("GameplayCueNotify_Static") },
                    { TEXT("Ability"), TEXT("Effect"), TEXT("Attribute"), TEXT("Tag"),
                      TEXT("Cost"), TEXT("Cooldown"), TEXT("Activation"), TEXT("Granted") }
                },
                {
                    TEXT("lyraExperience"),
                    { TEXT("LyraExperienceDefinition"), TEXT("LyraExperienceActionSet"),
                      TEXT("LyraUserFacingExperienceDefinition"), TEXT("LyraPawnData") },
                    { TEXT("Experience"), TEXT("Pawn"), TEXT("Action"), TEXT("GameFeature"),
                      TEXT("Playlist"), TEXT("Map"), TEXT("LoadingScreen") }
                },
                {
                    TEXT("gameFeature"),
                    { TEXT("GameFeatureData"), TEXT("GameFeatureAction") },
                    { TEXT("Action"), TEXT("GameFeature"), TEXT("PrimaryAsset"), TEXT("Plugin"),
                      TEXT("AssetType"), TEXT("Scan") }
                },
                {
                    TEXT("stateTree"),
                    { TEXT("StateTree") },
                    { TEXT("Schema"), TEXT("Root"), TEXT("State"), TEXT("Task"),
                      TEXT("Evaluator"), TEXT("Condition"), TEXT("Transition"), TEXT("Parameter") }
                },
                {
                    TEXT("dataRegistry"),
                    { TEXT("DataRegistry") },
                    { TEXT("Registry"), TEXT("Source"), TEXT("ItemStruct"), TEXT("Rules"),
                      TEXT("DataTable"), TEXT("CurveTable"), TEXT("Cache") }
                },
                {
                    TEXT("enhancedInput"),
                    { TEXT("InputAction"), TEXT("InputMappingContext"), TEXT("InputModifier"),
                      TEXT("InputTrigger"), TEXT("LyraInputConfig"), TEXT("PlayerMappableInputConfig") },
                    { TEXT("Mapping"), TEXT("Action"), TEXT("Trigger"), TEXT("Modifier"),
                      TEXT("ValueType"), TEXT("Key"), TEXT("Priority"), TEXT("Input") }
                }
            };
            return Value;
        }

        bool ContainsSemanticToken(const FString& Text, const FString& Marker)
        {
            int32 SearchFrom = 0;
            while (SearchFrom < Text.Len())
            {
                const int32 Index = Text.Find(
                    Marker,
                    ESearchCase::IgnoreCase,
                    ESearchDir::FromStart,
                    SearchFrom);
                if (Index == INDEX_NONE) return false;
                const bool bTokenStart = Index == 0
                    || !FChar::IsAlnum(Text[Index - 1])
                    || FChar::IsUpper(Text[Index]);
                if (bTokenStart) return true;
                SearchFrom = Index + 1;
            }
            return false;
        }

        bool ContainsAny(const FString& Text, const TArray<FString>& Markers)
        {
            for (const FString& Marker : Markers)
            {
                if (ContainsSemanticToken(Text, Marker)) return true;
            }
            return false;
        }

        FString ClassNameFromPath(const FString& Path)
        {
            FString ClassName = Path;
            int32 DotIndex = INDEX_NONE;
            if (ClassName.FindLastChar(TEXT('.'), DotIndex)) ClassName.RightChopInline(DotIndex + 1);
            return ClassName;
        }

        bool MatchesClassRule(
            const FUERingExportContext& Context,
            const TArray<FString>& Lineage,
            const FDomainRule& Rule)
        {
            TArray<FString> ClassNames = {
                Context.AssetData.AssetClassPath.GetAssetName().ToString()
            };
            for (const FString& Class : Lineage)
            {
                if (Class.StartsWith(TEXT("/Script/"))) ClassNames.Add(ClassNameFromPath(Class));
            }
            for (const FString& ClassName : ClassNames)
            {
                for (const FString& Marker : Rule.ClassMarkers)
                {
                    if (ClassName.Equals(Marker, ESearchCase::IgnoreCase)) return true;
                }
            }
            return false;
        }

        TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& Values)
        {
            TArray<TSharedPtr<FJsonValue>> Result;
            Result.Reserve(Values.Num());
            for (const FString& Value : Values)
            {
                Result.Add(MakeShared<FJsonValueString>(Value));
            }
            return Result;
        }

        const UClass* EffectiveClass(const UObject& Asset)
        {
            if (const UBlueprint* Blueprint = Cast<UBlueprint>(&Asset))
            {
                if (Blueprint->GeneratedClass != nullptr) return Blueprint->GeneratedClass;
                if (Blueprint->ParentClass != nullptr) return Blueprint->ParentClass;
            }
            return Asset.GetClass();
        }

        TArray<FString> ClassLineage(const UObject& Asset)
        {
            TArray<FString> Result;
            for (const UClass* Class = EffectiveClass(Asset); Class != nullptr; Class = Class->GetSuperClass())
            {
                Result.Add(Class->GetPathName());
            }
            return Result;
        }

        FString DetectionText(const FUERingExportContext& Context, const TArray<FString>& Lineage)
        {
            FString Result = Context.AssetData.AssetClassPath.ToString();
            for (const FString& Class : Lineage)
            {
                // Project package and asset names are not type evidence. Including them caused
                // folders such as GameplayCues and assets named *Experience* to become domains.
                if (Class.StartsWith(TEXT("/Script/"))) Result += TEXT(" ") + Class;
            }
            return Result;
        }

        FString RoleFor(const FString& Domain, const FString& Detection)
        {
            if (Domain == TEXT("gas"))
            {
                if (Detection.Contains(TEXT("GameplayEffect"), ESearchCase::IgnoreCase)) return TEXT("effect");
                if (Detection.Contains(TEXT("AttributeSet"), ESearchCase::IgnoreCase)) return TEXT("attributeSet");
                if (Detection.Contains(TEXT("AbilityTagRelationship"), ESearchCase::IgnoreCase))
                {
                    return TEXT("tagRelationshipMapping");
                }
                if (Detection.Contains(TEXT("AbilitySet"), ESearchCase::IgnoreCase)) return TEXT("abilitySet");
                if (Detection.Contains(TEXT("GameplayCue"), ESearchCase::IgnoreCase)) return TEXT("gameplayCue");
                return TEXT("ability");
            }
            if (Domain == TEXT("lyraExperience"))
            {
                if (Detection.Contains(TEXT("ActionSet"), ESearchCase::IgnoreCase)) return TEXT("actionSet");
                if (Detection.Contains(TEXT("UserFacing"), ESearchCase::IgnoreCase)) return TEXT("playlist");
                if (Detection.Contains(TEXT("PawnData"), ESearchCase::IgnoreCase)) return TEXT("pawnData");
                return TEXT("experience");
            }
            if (Domain == TEXT("gameFeature"))
            {
                return Detection.Contains(TEXT("Action"), ESearchCase::IgnoreCase)
                    ? TEXT("action") : TEXT("featureData");
            }
            if (Domain == TEXT("stateTree"))
            {
                if (Detection.Contains(TEXT("Task"), ESearchCase::IgnoreCase)) return TEXT("task");
                if (Detection.Contains(TEXT("Evaluator"), ESearchCase::IgnoreCase)) return TEXT("evaluator");
                if (Detection.Contains(TEXT("Condition"), ESearchCase::IgnoreCase)) return TEXT("condition");
                return TEXT("tree");
            }
            if (Domain == TEXT("dataRegistry")) return TEXT("registry");
            if (Detection.Contains(TEXT("InputAction"), ESearchCase::IgnoreCase)) return TEXT("action");
            if (Detection.Contains(TEXT("MappingContext"), ESearchCase::IgnoreCase)) return TEXT("mappingContext");
            if (Detection.Contains(TEXT("Modifier"), ESearchCase::IgnoreCase)) return TEXT("modifier");
            if (Detection.Contains(TEXT("Trigger"), ESearchCase::IgnoreCase)) return TEXT("trigger");
            return TEXT("inputConfig");
        }

        const TArray<FString>& GasFieldsForRole(const FString& Role)
        {
            static const TArray<FString> Ability = {
                TEXT("AbilityTags"), TEXT("bReplicateInputDirectly"), TEXT("ReplicationPolicy"),
                TEXT("InstancingPolicy"), TEXT("bServerRespectsRemoteAbilityCancellation"),
                TEXT("bRetriggerInstancedAbility"), TEXT("NetExecutionPolicy"),
                TEXT("NetSecurityPolicy"), TEXT("CostGameplayEffectClass"),
                TEXT("AbilityTriggers"), TEXT("CooldownGameplayEffectClass"),
                TEXT("CancelAbilitiesWithTag"), TEXT("BlockAbilitiesWithTag"),
                TEXT("ActivationOwnedTags"), TEXT("ActivationRequiredTags"),
                TEXT("ActivationBlockedTags"), TEXT("SourceRequiredTags"),
                TEXT("SourceBlockedTags"), TEXT("TargetRequiredTags"),
                TEXT("TargetBlockedTags"), TEXT("ActivationPolicy"), TEXT("ActivationGroup"),
                TEXT("AdditionalCosts"), TEXT("FailureTagToUserFacingMessages"),
                TEXT("FailureTagToAnimMontage"), TEXT("bLogCancelation")
            };
            static const TArray<FString> Effect = {
                TEXT("DurationPolicy"), TEXT("DurationMagnitude"), TEXT("MaxDurationMagnitude"),
                TEXT("Period"), TEXT("bExecutePeriodicEffectOnApplication"),
                TEXT("PeriodicInhibitionPolicy"), TEXT("Modifiers"), TEXT("Executions"),
                TEXT("bRequireModifierSuccessToTriggerCues"), TEXT("bSuppressStackingCues"),
                TEXT("GameplayCues"), TEXT("OverflowEffects"), TEXT("bDenyOverflowApplication"),
                TEXT("bClearStackOnOverflow"), TEXT("StackingType"), TEXT("StackLimitCount"),
                TEXT("StackDurationRefreshPolicy"), TEXT("StackPeriodResetPolicy"),
                TEXT("StackExpirationPolicy"), TEXT("bFactorInStackCount"),
                TEXT("GrantedAbilities"), TEXT("GEComponents"),
                TEXT("InheritableGameplayEffectTags"), TEXT("InheritableOwnedTagsContainer"),
                TEXT("InheritableBlockedAbilityTagsContainer"), TEXT("OngoingTagRequirements"),
                TEXT("ApplicationTagRequirements"), TEXT("RemovalTagRequirements"),
                TEXT("RemoveGameplayEffectsWithTags"), TEXT("GrantedApplicationImmunityTags"),
                TEXT("GrantedApplicationImmunityQuery"), TEXT("RemoveGameplayEffectQuery"),
                TEXT("ConditionalGameplayEffects"), TEXT("PrematureExpirationEffectClasses"),
                TEXT("RoutineExpirationEffectClasses")
            };
            static const TArray<FString> AbilitySet = {
                TEXT("GrantedGameplayAbilities"), TEXT("GrantedGameplayEffects"),
                TEXT("GrantedAttributes")
            };
            static const TArray<FString> TagRelationships = { TEXT("AbilityTagRelationships") };
            static const TArray<FString> GameplayCue = {
                TEXT("GameplayCueTag"), TEXT("GameplayCueName"), TEXT("IsOverride"),
                TEXT("bAutoAttachToOwner"), TEXT("bAutoDestroyOnRemove"),
                TEXT("AutoDestroyDelay"), TEXT("WarnIfTimelineIsStillRunning"),
                TEXT("WarnIfLatentActionIsStillRunning"), TEXT("bUniqueInstancePerInstigator"),
                TEXT("bUniqueInstancePerSourceObject"), TEXT("bAllowMultipleOnActiveEvents"),
                TEXT("bAllowMultipleWhileActiveEvents"), TEXT("NumPreallocatedInstances"),
                TEXT("DefaultPlacementInfo"), TEXT("BurstEffects"), TEXT("ApplicationEffects"),
                TEXT("LoopingEffects"), TEXT("RecurringEffects"), TEXT("RemovalEffects")
            };
            static const TArray<FString> Empty;
            if (Role == TEXT("ability")) return Ability;
            if (Role == TEXT("effect")) return Effect;
            if (Role == TEXT("abilitySet")) return AbilitySet;
            if (Role == TEXT("tagRelationshipMapping")) return TagRelationships;
            if (Role == TEXT("gameplayCue")) return GameplayCue;
            return Empty;
        }

        const TArray<FString>& ExperienceFieldsForRole(const FString& Role)
        {
            static const TArray<FString> Experience = {
                TEXT("GameFeaturesToEnable"), TEXT("DefaultPawnData"),
                TEXT("Actions"), TEXT("ActionSets")
            };
            static const TArray<FString> ActionSet = {
                TEXT("Actions"), TEXT("GameFeaturesToEnable")
            };
            static const TArray<FString> PawnData = {
                TEXT("PawnClass"), TEXT("AbilitySets"), TEXT("TagRelationshipMapping"),
                TEXT("InputConfig"), TEXT("DefaultCameraMode")
            };
            static const TArray<FString> Playlist = {
                TEXT("MapID"), TEXT("ExperienceID"), TEXT("ExtraArgs"), TEXT("TileTitle"),
                TEXT("TileSubTitle"), TEXT("TileDescription"), TEXT("TileIcon"),
                TEXT("LoadingScreenWidget"), TEXT("bIsDefaultExperience"),
                TEXT("bShowInFrontEnd"), TEXT("bRecordReplay"), TEXT("MaxPlayerCount")
            };
            static const TArray<FString> Empty;
            if (Role == TEXT("experience")) return Experience;
            if (Role == TEXT("actionSet")) return ActionSet;
            if (Role == TEXT("pawnData")) return PawnData;
            if (Role == TEXT("playlist")) return Playlist;
            return Empty;
        }

        bool ShouldSelectProperty(
            const FProperty& Property,
            const FString& Domain,
            const FString& Role,
            const TArray<FString>& Markers)
        {
            if (Domain == TEXT("gas"))
            {
                if (Role == TEXT("attributeSet"))
                {
                    const FStructProperty* StructProperty = CastField<FStructProperty>(&Property);
                    return StructProperty != nullptr
                        && StructProperty->Struct != nullptr
                        && StructProperty->Struct->GetName() == TEXT("GameplayAttributeData");
                }
                return GasFieldsForRole(Role).Contains(Property.GetName());
            }
            if (Domain == TEXT("lyraExperience"))
            {
                return ExperienceFieldsForRole(Role).Contains(Property.GetName());
            }
            if (Domain == TEXT("gameFeature"))
            {
                if (Role == TEXT("action")) return true;
                return Property.GetName() == TEXT("Actions")
                    || Property.GetName() == TEXT("PrimaryAssetTypesToScan");
            }
            return ContainsAny(Property.GetName(), Markers);
        }

        TArray<TSharedPtr<FJsonValue>> SelectedProperties(
            const UObject& Object,
            const FString& Domain,
            const FString& Role,
            const TArray<FString>& Markers,
            const UObject* Baseline = nullptr)
        {
            TArray<const FProperty*> Properties;
            for (TFieldIterator<FProperty> It(Object.GetClass(), EFieldIterationFlags::IncludeSuper); It; ++It)
            {
                const FProperty* Property = *It;
                const UClass* OwnerClass = Property->GetOwnerClass();
                const bool bCanCompareBaseline = Baseline != nullptr
                    && OwnerClass != nullptr
                    && Baseline->GetClass()->IsChildOf(OwnerClass);
                if (UERingPropertySerializer::ShouldExportProperty(*Property)
                    && ShouldSelectProperty(*Property, Domain, Role, Markers)
                    && (!bCanCompareBaseline || !Property->Identical_InContainer(&Object, Baseline)))
                {
                    Properties.Add(Property);
                }
            }
            Properties.Sort([](const FProperty& Left, const FProperty& Right)
            {
                return Left.GetName() < Right.GetName();
            });

            TArray<TSharedPtr<FJsonValue>> Result;
            for (const FProperty* Property : Properties)
            {
                Result.Add(MakeShared<FJsonValueObject>(
                    UERingPropertySerializer::SerializeProperty(*Property, &Object, &Object)));
            }
            return Result;
        }

        FString NormalizedGroup(const FString& Domain, const FString& PropertyName)
        {
            if (Domain == TEXT("gas"))
            {
                static const TArray<FString> ExecutionPolicyFields = {
                    TEXT("bReplicateInputDirectly"), TEXT("ReplicationPolicy"),
                    TEXT("InstancingPolicy"), TEXT("bServerRespectsRemoteAbilityCancellation"),
                    TEXT("bRetriggerInstancedAbility"), TEXT("NetExecutionPolicy"),
                    TEXT("NetSecurityPolicy"), TEXT("ActivationPolicy"), TEXT("ActivationGroup")
                };
                if (ExecutionPolicyFields.Contains(PropertyName)) return TEXT("executionPolicy");
                if (PropertyName.Contains(TEXT("Granted"), ESearchCase::IgnoreCase)) return TEXT("grants");
                if (PropertyName.Contains(TEXT("Tag"), ESearchCase::IgnoreCase)
                    || PropertyName.Contains(TEXT("Trigger"), ESearchCase::IgnoreCase))
                {
                    return TEXT("tagsAndTriggers");
                }
                if (PropertyName.Contains(TEXT("Cost"), ESearchCase::IgnoreCase)
                    || PropertyName.Contains(TEXT("Cooldown"), ESearchCase::IgnoreCase))
                {
                    return TEXT("costsAndCooldown");
                }
                if (ContainsAny(PropertyName, {
                    TEXT("Duration"), TEXT("Period"), TEXT("Modifier"), TEXT("Execution"),
                    TEXT("Stack"), TEXT("Immunity"), TEXT("Requirement"), TEXT("Chance"),
                    TEXT("GameplayCue") }))
                {
                    return TEXT("effectBehavior");
                }
                return TEXT("behavior");
            }
            if (Domain == TEXT("lyraExperience"))
            {
                if (PropertyName.Contains(TEXT("GameFeature"), ESearchCase::IgnoreCase))
                {
                    return TEXT("gameFeatures");
                }
                if (PropertyName.Contains(TEXT("Pawn"), ESearchCase::IgnoreCase)) return TEXT("pawn");
                if (PropertyName.Contains(TEXT("AbilitySet"), ESearchCase::IgnoreCase)) return TEXT("abilities");
                if (PropertyName.Contains(TEXT("TagRelationship"), ESearchCase::IgnoreCase))
                {
                    return TEXT("tagsAndTriggers");
                }
                if (PropertyName.Contains(TEXT("Input"), ESearchCase::IgnoreCase)) return TEXT("input");
                if (PropertyName.Contains(TEXT("Camera"), ESearchCase::IgnoreCase)) return TEXT("camera");
                if (PropertyName.Contains(TEXT("Action"), ESearchCase::IgnoreCase)) return TEXT("actions");
                if (ContainsAny(PropertyName, {
                    TEXT("Map"), TEXT("Playlist"), TEXT("LoadingScreen"),
                    TEXT("ExperienceID"), TEXT("DefaultExperience") }))
                {
                    return TEXT("selection");
                }
                return TEXT("configuration");
            }
            if (Domain == TEXT("gameFeature"))
            {
                if (PropertyName.Contains(TEXT("Action"), ESearchCase::IgnoreCase)) return TEXT("actions");
                if (ContainsAny(PropertyName, { TEXT("Scan"), TEXT("AssetType"), TEXT("PrimaryAsset") }))
                {
                    return TEXT("assetScanning");
                }
                if (ContainsAny(PropertyName, { TEXT("Plugin"), TEXT("GameFeature") }))
                {
                    return TEXT("plugins");
                }
                if (ContainsAny(PropertyName, { TEXT("Input"), TEXT("Mapping") })) return TEXT("input");
                if (PropertyName.Contains(TEXT("Component"), ESearchCase::IgnoreCase)) return TEXT("components");
                if (PropertyName.Contains(TEXT("Registr"), ESearchCase::IgnoreCase)) return TEXT("registries");
                if (PropertyName.Contains(TEXT("Cue"), ESearchCase::IgnoreCase)) return TEXT("gameplayCues");
                return TEXT("configuration");
            }
            return TEXT("configuration");
        }

        TSharedRef<FJsonObject> NormalizeProperties(
            const TArray<TSharedPtr<FJsonValue>>& Properties,
            const FString& Domain)
        {
            TMap<FString, TSharedPtr<FJsonObject>> Groups;
            for (const TSharedPtr<FJsonValue>& Value : Properties)
            {
                const TSharedPtr<FJsonObject> Property = Value.IsValid() ? Value->AsObject() : nullptr;
                if (!Property.IsValid()) continue;
                FString Name;
                if (!Property->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) continue;
                const TSharedPtr<FJsonValue>* PropertyValue = Property->Values.Find(TEXT("value"));
                if (PropertyValue == nullptr || !PropertyValue->IsValid()) continue;
                const FString GroupName = NormalizedGroup(Domain, Name);
                TSharedPtr<FJsonObject>& Group = Groups.FindOrAdd(GroupName);
                if (!Group.IsValid()) Group = MakeShared<FJsonObject>();
                Group->SetField(Name, *PropertyValue);
            }
            const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            TArray<FString> GroupNames;
            Groups.GetKeys(GroupNames);
            GroupNames.Sort();
            for (const FString& GroupName : GroupNames)
            {
                Result->SetObjectField(GroupName, Groups.FindChecked(GroupName).ToSharedRef());
            }
            return Result;
        }

        void ReuseCanonicalPropertyValues(
            TArray<TSharedPtr<FJsonValue>>& Properties,
            const TArray<TSharedPtr<FJsonValue>>* CanonicalProperties)
        {
            if (CanonicalProperties == nullptr) return;
            TMap<FString, TSharedPtr<FJsonValue>> ValueByName;
            for (const TSharedPtr<FJsonValue>& EntryValue : *CanonicalProperties)
            {
                const TSharedPtr<FJsonObject> Entry = EntryValue.IsValid()
                    ? EntryValue->AsObject()
                    : nullptr;
                FString Name;
                if (Entry.IsValid() && Entry->TryGetStringField(TEXT("name"), Name))
                {
                    if (const TSharedPtr<FJsonValue>* Value = Entry->Values.Find(TEXT("value")))
                    {
                        ValueByName.Add(Name, *Value);
                    }
                }
            }
            for (const TSharedPtr<FJsonValue>& PropertyValue : Properties)
            {
                const TSharedPtr<FJsonObject> Property = PropertyValue.IsValid()
                    ? PropertyValue->AsObject()
                    : nullptr;
                FString Name;
                if (Property.IsValid() && Property->TryGetStringField(TEXT("name"), Name))
                {
                    if (const TSharedPtr<FJsonValue>* CanonicalValue = ValueByName.Find(Name))
                    {
                        Property->SetField(TEXT("value"), *CanonicalValue);
                    }
                }
            }
        }

        FString OwnedObjectId(const UObject& Object, const UObject& Asset)
        {
            const FString FullPath = Object.GetPathName();
            FString PackageObjectPath;
            if (FullPath.Split(
                TEXT(":"),
                nullptr,
                &PackageObjectPath,
                ESearchCase::CaseSensitive,
                ESearchDir::FromStart)
                && !PackageObjectPath.IsEmpty())
            {
                return PackageObjectPath;
            }
            return Object.GetPathName(&Asset);
        }

        void AddOwnedObjects(
            const UObject& Asset,
            const UObject* ClassDefault,
            const TArray<const FDomainRule*>& MatchedRules,
            const TSharedRef<FJsonObject>& Semantics,
            const TSharedRef<FJsonObject>& Domain)
        {
            const FDomainRule* GameFeatureRule = nullptr;
            bool bIncludesExperience = false;
            for (const FDomainRule* Rule : MatchedRules)
            {
                bIncludesExperience |= FString(Rule->Key) == TEXT("lyraExperience");
            }
            for (const FDomainRule& Rule : Rules())
            {
                if (FString(Rule.Key) == TEXT("gameFeature"))
                {
                    GameFeatureRule = &Rule;
                    break;
                }
            }
            TArray<UObject*> Owned;
            const auto GatherOwned = [&Owned](const UObject* Outer)
            {
                if (Outer == nullptr) return;
                TArray<UObject*> Objects;
                GetObjectsWithOuter(
                    Outer,
                    Objects,
                    EGetObjectsFlags::IncludeNestedObjects,
                    RF_Transient | RF_ClassDefaultObject,
                    EInternalObjectFlags::Garbage);
                Owned.Append(Objects);
            };
            GatherOwned(&Asset);
            GatherOwned(ClassDefault);
            Owned.RemoveAll([&Asset, &MatchedRules, bIncludesExperience](const UObject* Object)
            {
                if (Object == nullptr || Object->GetOutermost() != Asset.GetOutermost()) return true;
                const FString ClassName = Object->GetClass()->GetPathName();
                if (ClassName.Contains(TEXT("EdGraph"), ESearchCase::IgnoreCase)
                    || ClassName.Contains(TEXT("K2Node"), ESearchCase::IgnoreCase)
                    || ClassName.Contains(TEXT("BlueprintGeneratedClass"), ESearchCase::IgnoreCase))
                {
                    return true;
                }
                for (const FDomainRule* Rule : MatchedRules)
                {
                    if (ContainsAny(ClassName, Rule->ClassMarkers)) return false;
                }
                // Lyra Experience and ActionSet assets own instanced GameFeatureAction objects.
                // Their concrete action classes are the executable configuration of the asset.
                if (bIncludesExperience
                    && ClassName.Contains(TEXT("GameFeatureAction"), ESearchCase::IgnoreCase))
                {
                    return false;
                }
                return true;
            });
            Owned.Sort([&Asset](const UObject& Left, const UObject& Right)
            {
                return OwnedObjectId(Left, Asset) < OwnedObjectId(Right, Asset);
            });

            TMap<FString, const TArray<TSharedPtr<FJsonValue>>*> CanonicalPropertiesById;
            const TArray<TSharedPtr<FJsonValue>>* CanonicalOwnedObjects = nullptr;
            if (Semantics->TryGetArrayField(TEXT("ownedObjects"), CanonicalOwnedObjects)
                && CanonicalOwnedObjects != nullptr)
            {
                for (const TSharedPtr<FJsonValue>& EntryValue : *CanonicalOwnedObjects)
                {
                    const TSharedPtr<FJsonObject> Entry = EntryValue.IsValid()
                        ? EntryValue->AsObject()
                        : nullptr;
                    const TArray<TSharedPtr<FJsonValue>>* Properties = nullptr;
                    FString Id;
                    if (Entry.IsValid()
                        && Entry->TryGetStringField(TEXT("id"), Id)
                        && Entry->TryGetArrayField(TEXT("properties"), Properties)
                        && Properties != nullptr)
                    {
                        CanonicalPropertiesById.Add(Id, Properties);
                    }
                }
            }

            TArray<TSharedPtr<FJsonValue>> JsonObjects;
            for (const UObject* Object : Owned)
            {
                const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
                Json->SetStringField(TEXT("id"), OwnedObjectId(*Object, Asset));
                Json->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
                const FDomainRule* ObjectRule = nullptr;
                for (const FDomainRule* Rule : MatchedRules)
                {
                    if (ContainsAny(Object->GetClass()->GetPathName(), Rule->ClassMarkers))
                    {
                        ObjectRule = Rule;
                        break;
                    }
                }
                if (ObjectRule == nullptr && bIncludesExperience && GameFeatureRule != nullptr
                    && Object->GetClass()->GetPathName().Contains(
                        TEXT("GameFeatureAction"), ESearchCase::IgnoreCase))
                {
                    ObjectRule = GameFeatureRule;
                }
                const FString ObjectRole = ObjectRule != nullptr
                    ? RoleFor(ObjectRule->Key, Object->GetClass()->GetPathName())
                    : TEXT("configuration");
                TArray<TSharedPtr<FJsonValue>> Properties = ObjectRule != nullptr
                    ? SelectedProperties(
                        *Object,
                        ObjectRule->Key,
                        ObjectRole,
                        ObjectRule->PropertyMarkers)
                    : UERingPropertySerializer::SerializeObjectProperties(*Object);
                ReuseCanonicalPropertyValues(
                    Properties,
                    CanonicalPropertiesById.FindRef(OwnedObjectId(*Object, Asset)));
                if (!Properties.IsEmpty())
                {
                    Json->SetObjectField(
                        TEXT("configuration"),
                        NormalizeProperties(Properties, ObjectRule != nullptr ? ObjectRule->Key : TEXT("configuration")));
                }
                JsonObjects.Add(MakeShared<FJsonValueObject>(Json));
            }
            if (!JsonObjects.IsEmpty()) Domain->SetArrayField(TEXT("ownedObjects"), JsonObjects);
        }

        void AddAssetReferences(const UObject& Asset, const TSharedRef<FJsonObject>& Domain)
        {
            TArray<UObject*> References;
            FReferenceFinder Finder(
                References,
                const_cast<UObject*>(&Asset),
                false,
                true,
                false,
                true);
            Finder.FindReferences(const_cast<UObject*>(&Asset));
            References.RemoveAll([&Asset](const UObject* Object)
            {
                return Object == nullptr
                    || Object->GetOutermost() == Asset.GetOutermost()
                    || !Object->IsAsset()
                    || !FPackageName::IsValidLongPackageName(Object->GetOutermost()->GetName());
            });
            References.Sort([](const UObject& Left, const UObject& Right)
            {
                return Left.GetPathName() < Right.GetPathName();
            });

            TSet<FString> Seen;
            TArray<TSharedPtr<FJsonValue>> JsonReferences;
            for (const UObject* Object : References)
            {
                const FString Path = Object->GetPathName();
                if (Seen.Contains(Path)) continue;
                Seen.Add(Path);
                const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
                Json->SetStringField(TEXT("objectPath"), Path);
                Json->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
                JsonReferences.Add(MakeShared<FJsonValueObject>(Json));
            }
            if (!JsonReferences.IsEmpty()) Domain->SetArrayField(TEXT("references"), JsonReferences);
        }

        void AddCode(TArray<FString>& Values, const TCHAR* Code)
        {
            Values.AddUnique(Code);
        }

        FString StableIdentifier(FString Value)
        {
            for (TCHAR& Character : Value)
            {
                if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-'))
                {
                    Character = TEXT('_');
                }
            }
            return Value.IsEmpty() ? TEXT("unnamed") : Value;
        }

        FString NativeModuleName(const UClass* Class)
        {
            if (Class == nullptr) return FString();
            FString Module = Class->GetOutermost()->GetName();
            Module.RemoveFromStart(TEXT("/Script/"));
            return Module;
        }

        FString NativeCppName(const UClass* Class)
        {
            return Class != nullptr ? Class->GetPrefixCPP() + Class->GetName() : FString();
        }

        FString CanonicalModuleHeader(FString Header)
        {
            Header.ReplaceInline(TEXT("\\"), TEXT("/"));
            if (!Header.RemoveFromStart(TEXT("Public/")))
            {
                if (!Header.RemoveFromStart(TEXT("Private/")))
                {
                    Header.RemoveFromStart(TEXT("Classes/"));
                }
            }
            return Header;
        }

        TSharedRef<FJsonObject> Fidelity(
            const TCHAR* Status,
            const TCHAR* Rule,
            const double Confidence,
            const FString& ReasonCode = FString())
        {
            const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("status"), Status);
            Result->SetStringField(TEXT("rule"), Rule);
            Result->SetNumberField(TEXT("confidence"), Confidence);
            if (!ReasonCode.IsEmpty()) Result->SetStringField(TEXT("reasonCode"), ReasonCode);
            return Result;
        }

        TSharedRef<FJsonObject> Operation(
            const FUERingExportContext& Context,
            const FString& Id,
            const FString& Opcode,
            const FString& Phase,
            const FString& TargetId,
            const TArray<FString>& DependsOn,
            const TSharedRef<FJsonObject>& Operands,
            const TArray<FString>& SourcePointers,
            const TCHAR* Criticality,
            const TCHAR* Status,
            const TCHAR* FidelityStatus,
            const TCHAR* Rule,
            const double Confidence,
            const FString& ReasonCode = FString())
        {
            const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("id"), Id);
            Result->SetStringField(TEXT("opcode"), Opcode);
            Result->SetNumberField(TEXT("opcodeVersion"), 1);
            Result->SetStringField(TEXT("phase"), Phase);
            Result->SetStringField(TEXT("targetId"), TargetId);
            Result->SetArrayField(TEXT("dependsOn"), Strings(DependsOn));
            Result->SetObjectField(TEXT("operands"), Operands);
            Result->SetArrayField(TEXT("results"), {});
            Result->SetArrayField(TEXT("sourcePointers"), Strings(SourcePointers));
            Result->SetArrayField(TEXT("preconditions"), {});
            Result->SetArrayField(TEXT("postconditions"), {});
            Result->SetStringField(TEXT("criticality"), Criticality);
            Result->SetStringField(TEXT("status"), Status);
            Result->SetObjectField(
                TEXT("fidelity"),
                Fidelity(FidelityStatus, Rule, Confidence, ReasonCode));
            Result->SetStringField(TEXT("failurePolicy"), TEXT("abort"));
            Result->SetStringField(TEXT("idempotencyKey"), Context.InputFingerprint + TEXT(":") + Id);
            return Result;
        }

        TSharedRef<FJsonObject> Loss(
            const FString& Id,
            const FString& ReasonCode,
            const TCHAR* Impact,
            const TArray<FString>& SourcePointers)
        {
            const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("id"), Id);
            Result->SetStringField(TEXT("reasonCode"), ReasonCode);
            Result->SetStringField(TEXT("impact"), Impact);
            Result->SetArrayField(TEXT("sourcePointers"), Strings(SourcePointers));
            Result->SetBoolField(TEXT("recoverableFromSourceAsset"), true);
            return Result;
        }

        TSharedRef<FJsonObject> Symbol(
            const FString& Id,
            const TCHAR* Kind,
            const FString& UnrealPath,
            const FString& CppName,
            const FString& Module,
            const FString& Header,
            const TCHAR* Resolution,
            const FString& SourcePointer)
        {
            const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("id"), Id);
            Result->SetStringField(TEXT("kind"), Kind);
            Result->SetStringField(TEXT("unrealPath"), UnrealPath);
            if (!CppName.IsEmpty()) Result->SetStringField(TEXT("cppName"), CppName);
            if (!Module.IsEmpty()) Result->SetStringField(TEXT("module"), Module);
            if (!Header.IsEmpty()) Result->SetStringField(TEXT("header"), Header);
            Result->SetStringField(TEXT("resolution"), Resolution);
            Result->SetStringField(TEXT("sourcePointer"), SourcePointer);
            return Result;
        }

        struct FDataAssetRecoveryAudit
        {
            TMap<FString, TArray<FString>> PointersByReason;

            void Add(const TCHAR* Reason, const FString& Pointer)
            {
                PointersByReason.FindOrAdd(Reason).AddUnique(Pointer);
            }

            bool IsExact() const
            {
                return PointersByReason.IsEmpty();
            }

            void Append(const FDataAssetRecoveryAudit& Other)
            {
                for (const TPair<FString, TArray<FString>>& Pair : Other.PointersByReason)
                {
                    for (const FString& Pointer : Pair.Value)
                    {
                        PointersByReason.FindOrAdd(Pair.Key).AddUnique(Pointer);
                    }
                }
            }

            FString PrimaryReason() const
            {
                for (const TCHAR* Reason : {
                    TEXT("redactedDataAssetValue"),
                    TEXT("invalidDataAssetOwnedObjectGraph"),
                    TEXT("dataAssetOwnedObjectGraphUnavailable"),
                    TEXT("unsupportedDataAssetPropertyType"),
                    TEXT("invalidDataAssetPropertyValue") })
                {
                    if (PointersByReason.Contains(Reason)) return Reason;
                }
                return FString();
            }

            TArray<FString> AllPointers() const
            {
                TArray<FString> Result;
                for (const TPair<FString, TArray<FString>>& Pair : PointersByReason)
                {
                    Result.Append(Pair.Value);
                }
                Result.Sort();
                Result.SetNum(Algo::Unique(Result));
                return Result;
            }
        };

        struct FDataAssetRecoveryAudits
        {
            FDataAssetRecoveryAudit RootProperties;
            FDataAssetRecoveryAudit OwnedStructure;
            FDataAssetRecoveryAudit OwnedProperties;
            int32 OwnedObjectCount = 0;
        };

        bool IsRedactionMarker(const TSharedPtr<FJsonValue>& Value)
        {
            FString Text;
            return Value.IsValid()
                && Value->TryGetString(Text)
                && (Text.Contains(TEXT("[REDACTED]"))
                    || Text.Contains(TEXT("[MAX_DEPTH]"))
                    || Text.Contains(TEXT("[OMITTED]")));
        }

        void AuditDataAssetProperty(
            const FProperty& Property,
            const TSharedPtr<FJsonValue>& Value,
            const FString& SourcePackage,
            const TMap<FString, UObject*>& OwnedObjects,
            const FString& Pointer,
            FDataAssetRecoveryAudit& Audit);

        void AuditDataAssetValue(
            const FProperty& Property,
            const TSharedPtr<FJsonValue>& Value,
            const FString& SourcePackage,
            const TMap<FString, UObject*>& OwnedObjects,
            const FString& Pointer,
            FDataAssetRecoveryAudit& Audit)
        {
            if (!Value.IsValid())
            {
                Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                return;
            }
            if (IsRedactionMarker(Value))
            {
                Audit.Add(TEXT("redactedDataAssetValue"), Pointer);
                return;
            }
            if (const FOptionalProperty* Optional = CastField<FOptionalProperty>(&Property))
            {
                const TSharedPtr<FJsonObject>* Object = nullptr;
                bool bIsSet = false;
                if (!Value->TryGetObject(Object) || Object == nullptr
                    || !(*Object)->TryGetBoolField(TEXT("isSet"), bIsSet)
                    || (*Object)->Values.Num() != (bIsSet ? 2 : 1)
                    || (bIsSet && !(*Object)->HasField(TEXT("value"))))
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                    return;
                }
                if (bIsSet)
                {
                    AuditDataAssetValue(
                        *Optional->GetValueProperty(),
                        (*Object)->Values.FindRef(TEXT("value")),
                        SourcePackage,
                        OwnedObjects,
                        Pointer + TEXT("/value"),
                        Audit);
                }
                return;
            }
            if (CastField<FEnumProperty>(&Property) != nullptr)
            {
                const TSharedPtr<FJsonObject>* Object = nullptr;
                if (!Value->TryGetObject(Object) || Object == nullptr)
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                }
                return;
            }
            if (const FNumericProperty* Numeric = CastField<FNumericProperty>(&Property))
            {
                if (Numeric->GetIntPropertyEnum() != nullptr)
                {
                    const TSharedPtr<FJsonObject>* Object = nullptr;
                    if (!Value->TryGetObject(Object) || Object == nullptr)
                    {
                        Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                    }
                    return;
                }
                double Number = 0.0;
                if (!Value->TryGetNumber(Number)
                    || !FMath::IsFinite(Number)
                    || (Numeric->IsInteger()
                        && (Number != FMath::RoundToDouble(Number)
                            || FMath::Abs(Number) > 9007199254740991.0)))
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                }
                return;
            }
            if (CastField<FBoolProperty>(&Property) != nullptr)
            {
                bool Boolean = false;
                if (!Value->TryGetBool(Boolean)) Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                return;
            }
            if (CastField<FStrProperty>(&Property) != nullptr
                || CastField<FUtf8StrProperty>(&Property) != nullptr
                || CastField<FAnsiStrProperty>(&Property) != nullptr
                || CastField<FNameProperty>(&Property) != nullptr)
            {
                FString Text;
                if (!Value->TryGetString(Text)) Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                return;
            }
            if (CastField<FTextProperty>(&Property) != nullptr)
            {
                const TSharedPtr<FJsonObject>* Object = nullptr;
                if (!Value->TryGetObject(Object) || Object == nullptr)
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                    return;
                }
                FString Source;
                FString Display;
                (*Object)->TryGetStringField(TEXT("source"), Source);
                if ((*Object)->TryGetStringField(TEXT("displayText"), Display) && Display != Source)
                {
                    Audit.Add(TEXT("unsupportedDataAssetPropertyType"), Pointer + TEXT("/displayText"));
                }
                return;
            }
            if (const FDelegateProperty* Delegate = CastField<FDelegateProperty>(&Property))
            {
                const TSharedPtr<FJsonObject>* Object = nullptr;
                const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
                FString Kind;
                FString Signature;
                const FString ExpectedSignature = Delegate->SignatureFunction != nullptr
                    ? Delegate->SignatureFunction->GetPathName()
                    : FString();
                if (!Value->TryGetObject(Object) || Object == nullptr
                    || !(*Object)->TryGetStringField(TEXT("delegateKind"), Kind)
                    || Kind != TEXT("single")
                    || !(*Object)->TryGetStringField(TEXT("signature"), Signature)
                    || Signature != ExpectedSignature
                    || !(*Object)->TryGetArrayField(TEXT("bindings"), Bindings)
                    || Bindings == nullptr)
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                    return;
                }
                if (!Bindings->IsEmpty())
                {
                    Audit.Add(TEXT("unsupportedDataAssetPropertyType"), Pointer + TEXT("/bindings"));
                }
                return;
            }
            if (const FMulticastDelegateProperty* Delegate =
                CastField<FMulticastDelegateProperty>(&Property))
            {
                const TSharedPtr<FJsonObject>* Object = nullptr;
                const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
                FString Kind;
                FString Signature;
                double UnresolvedBindingCount = 0.0;
                const FString ExpectedSignature = Delegate->SignatureFunction != nullptr
                    ? Delegate->SignatureFunction->GetPathName()
                    : FString();
                if (!Value->TryGetObject(Object) || Object == nullptr
                    || !(*Object)->TryGetStringField(TEXT("delegateKind"), Kind)
                    || Kind != TEXT("multicast")
                    || !(*Object)->TryGetStringField(TEXT("signature"), Signature)
                    || Signature != ExpectedSignature
                    || !(*Object)->TryGetArrayField(TEXT("bindings"), Bindings)
                    || Bindings == nullptr)
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                    return;
                }
                if (!Bindings->IsEmpty()
                    || ((*Object)->TryGetNumberField(TEXT("unresolvedBindingCount"), UnresolvedBindingCount)
                        && UnresolvedBindingCount > 0.0))
                {
                    Audit.Add(TEXT("unsupportedDataAssetPropertyType"), Pointer + TEXT("/bindings"));
                }
                return;
            }
            if (CastField<FSoftObjectProperty>(&Property) != nullptr
                || CastField<FObjectPropertyBase>(&Property) != nullptr)
            {
                const TSharedPtr<FJsonObject>* Object = nullptr;
                FString ObjectPath;
                FString OwnedObjectId;
                if (!Value->TryGetObject(Object) || Object == nullptr)
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                    return;
                }
                const bool bHasObjectPath = (*Object)->TryGetStringField(TEXT("objectPath"), ObjectPath);
                const bool bHasOwnedObjectId = (*Object)->TryGetStringField(TEXT("ownedObjectId"), OwnedObjectId);
                if (bHasObjectPath == bHasOwnedObjectId)
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                    return;
                }
                if (bHasOwnedObjectId)
                {
                    UObject* const* Referenced = OwnedObjects.Find(OwnedObjectId);
                    const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(&Property);
                    if (Referenced == nullptr || *Referenced == nullptr || ObjectProperty == nullptr
                        || !(*Referenced)->IsA(ObjectProperty->PropertyClass))
                    {
                        Audit.Add(TEXT("invalidDataAssetOwnedObjectGraph"), Pointer + TEXT("/ownedObjectId"));
                        return;
                    }
                    if (const FClassProperty* ClassProperty = CastField<FClassProperty>(&Property))
                    {
                        const UClass* ReferencedClass = Cast<UClass>(*Referenced);
                        if (ReferencedClass == nullptr || !ReferencedClass->IsChildOf(ClassProperty->MetaClass))
                        {
                            Audit.Add(TEXT("invalidDataAssetOwnedObjectGraph"), Pointer + TEXT("/ownedObjectId"));
                        }
                    }
                    return;
                }
                if (ObjectPath.StartsWith(SourcePackage + TEXT("."))
                    || ObjectPath.StartsWith(SourcePackage + TEXT(":")))
                {
                    Audit.Add(TEXT("dataAssetOwnedObjectGraphUnavailable"), Pointer + TEXT("/objectPath"));
                }
                return;
            }
            if (const FArrayProperty* Array = CastField<FArrayProperty>(&Property))
            {
                const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
                if (!Value->TryGetArray(Values) || Values == nullptr)
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                    return;
                }
                for (int32 Index = 0; Index < Values->Num(); ++Index)
                {
                    AuditDataAssetValue(
                        *Array->Inner,
                        (*Values)[Index],
                        SourcePackage,
                        OwnedObjects,
                        FString::Printf(TEXT("%s/%d"), *Pointer, Index),
                        Audit);
                }
                return;
            }
            if (const FSetProperty* Set = CastField<FSetProperty>(&Property))
            {
                const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
                if (!Value->TryGetArray(Values) || Values == nullptr)
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                    return;
                }
                for (int32 Index = 0; Index < Values->Num(); ++Index)
                {
                    AuditDataAssetValue(
                        *Set->ElementProp,
                        (*Values)[Index],
                        SourcePackage,
                        OwnedObjects,
                        FString::Printf(TEXT("%s/%d"), *Pointer, Index),
                        Audit);
                }
                return;
            }
            if (const FMapProperty* Map = CastField<FMapProperty>(&Property))
            {
                const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
                if (!Value->TryGetArray(Entries) || Entries == nullptr)
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                    return;
                }
                for (int32 Index = 0; Index < Entries->Num(); ++Index)
                {
                    const TSharedPtr<FJsonObject>* Entry = nullptr;
                    if (!(*Entries)[Index].IsValid()
                        || !(*Entries)[Index]->TryGetObject(Entry)
                        || Entry == nullptr)
                    {
                        Audit.Add(TEXT("invalidDataAssetPropertyValue"),
                            FString::Printf(TEXT("%s/%d"), *Pointer, Index));
                        continue;
                    }
                    AuditDataAssetValue(
                        *Map->KeyProp,
                        (*Entry)->Values.FindRef(TEXT("key")),
                        SourcePackage,
                        OwnedObjects,
                        FString::Printf(TEXT("%s/%d/key"), *Pointer, Index),
                        Audit);
                    AuditDataAssetValue(
                        *Map->ValueProp,
                        (*Entry)->Values.FindRef(TEXT("value")),
                        SourcePackage,
                        OwnedObjects,
                        FString::Printf(TEXT("%s/%d/value"), *Pointer, Index),
                        Audit);
                }
                return;
            }
            if (const FStructProperty* Struct = CastField<FStructProperty>(&Property))
            {
                if (Struct->Struct == FInstancedStruct::StaticStruct())
                {
                    const TSharedPtr<FJsonObject>* Object = nullptr;
                    const TSharedPtr<FJsonObject>* Fields = nullptr;
                    double Version = 0.0;
                    bool bIsValid = false;
                    if (!Value->TryGetObject(Object) || Object == nullptr
                        || !(*Object)->TryGetNumberField(TEXT("instancedStructVersion"), Version)
                        || Version != 1.0
                        || !(*Object)->TryGetBoolField(TEXT("isValid"), bIsValid)
                        || !(*Object)->TryGetObjectField(TEXT("fields"), Fields)
                        || Fields == nullptr)
                    {
                        Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                        return;
                    }
                    if (!bIsValid)
                    {
                        if (!(*Fields)->Values.IsEmpty() || (*Object)->HasField(TEXT("valueStruct")))
                        {
                            Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                        }
                        return;
                    }

                    FString ValueStructPath;
                    if (!(*Object)->TryGetStringField(TEXT("valueStruct"), ValueStructPath)
                        || ValueStructPath.IsEmpty())
                    {
                        Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer + TEXT("/valueStruct"));
                        return;
                    }
                    const UScriptStruct* ValueStruct = LoadObject<UScriptStruct>(
                        nullptr, *ValueStructPath);
                    if (ValueStruct == nullptr)
                    {
                        Audit.Add(TEXT("unsupportedDataAssetPropertyType"), Pointer + TEXT("/valueStruct"));
                        return;
                    }
                    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Fields)->Values)
                    {
                        const FProperty* Field = FindFProperty<FProperty>(ValueStruct, *Pair.Key);
                        if (Field == nullptr || !UERingPropertySerializer::ShouldExportProperty(*Field))
                        {
                            Audit.Add(
                                TEXT("unsupportedDataAssetPropertyType"),
                                Pointer + TEXT("/fields/") + Pair.Key);
                            continue;
                        }
                        AuditDataAssetProperty(
                            *Field,
                            Pair.Value,
                            SourcePackage,
                            OwnedObjects,
                            Pointer + TEXT("/fields/") + Pair.Key,
                            Audit);
                    }
                    return;
                }
                if (Struct->Struct == FInstancedPropertyBag::StaticStruct())
                {
                    const TSharedPtr<FJsonObject>* Object = nullptr;
                    const TArray<TSharedPtr<FJsonValue>>* Properties = nullptr;
                    double Version = 0.0;
                    bool bIsValid = false;
                    if (!Value->TryGetObject(Object) || Object == nullptr
                        || !(*Object)->TryGetNumberField(TEXT("propertyBagVersion"), Version)
                        || Version != 1.0
                        || !(*Object)->TryGetBoolField(TEXT("isValid"), bIsValid)
                        || !(*Object)->TryGetArrayField(TEXT("properties"), Properties)
                        || Properties == nullptr)
                    {
                        Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                        return;
                    }
                    if (!bIsValid)
                    {
                        if (!Properties->IsEmpty())
                        {
                            Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer + TEXT("/properties"));
                        }
                        return;
                    }

                    FString LayoutId;
                    if (!(*Object)->TryGetStringField(TEXT("layoutId"), LayoutId)
                        || LayoutId.IsEmpty())
                    {
                        Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer + TEXT("/layoutId"));
                        return;
                    }

                    const UEnum* ValueTypeEnum = StaticEnum<EPropertyBagPropertyType>();
                    const UEnum* ContainerTypeEnum = StaticEnum<EPropertyBagContainerType>();
                    TArray<FPropertyBagPropertyDesc> Descs;
                    TArray<TSharedPtr<FJsonObject>> Entries;
                    Descs.Reserve(Properties->Num());
                    Entries.Reserve(Properties->Num());
                    for (int32 Index = 0; Index < Properties->Num(); ++Index)
                    {
                        const FString EntryPointer = FString::Printf(
                            TEXT("%s/properties/%d"), *Pointer, Index);
                        const TSharedPtr<FJsonObject> Entry = (*Properties)[Index].IsValid()
                            ? (*Properties)[Index]->AsObject()
                            : nullptr;
                        FString Id;
                        FString Name;
                        FString ValueTypeName;
                        FString KeyTypeName;
                        FString PropertyFlags;
                        const TArray<TSharedPtr<FJsonValue>>* ContainerTypes = nullptr;
                        if (!Entry.IsValid()
                            || !Entry->TryGetStringField(TEXT("id"), Id)
                            || !Entry->TryGetStringField(TEXT("name"), Name)
                            || Name.IsEmpty()
                            || !Entry->TryGetStringField(TEXT("valueType"), ValueTypeName)
                            || !Entry->TryGetStringField(TEXT("keyType"), KeyTypeName)
                            || !Entry->TryGetStringField(TEXT("propertyFlags"), PropertyFlags)
                            || !Entry->TryGetArrayField(TEXT("containerTypes"), ContainerTypes)
                            || ContainerTypes == nullptr
                            || !Entry->HasField(TEXT("value")))
                        {
                            Audit.Add(TEXT("invalidDataAssetPropertyValue"), EntryPointer);
                            continue;
                        }

                        FGuid Guid;
                        uint64 Flags = 0;
                        const int64 ValueTypeValue = ValueTypeEnum != nullptr
                            ? ValueTypeEnum->GetValueByNameString(ValueTypeName)
                            : INDEX_NONE;
                        const int64 KeyTypeValue = ValueTypeEnum != nullptr
                            ? ValueTypeEnum->GetValueByNameString(KeyTypeName)
                            : INDEX_NONE;
                        if (!FGuid::Parse(Id, Guid)
                            || ValueTypeValue <= static_cast<int64>(EPropertyBagPropertyType::None)
                            || ValueTypeValue >= static_cast<int64>(EPropertyBagPropertyType::Count)
                            || KeyTypeValue < static_cast<int64>(EPropertyBagPropertyType::None)
                            || KeyTypeValue >= static_cast<int64>(EPropertyBagPropertyType::Count)
                            || !LexTryParseString(Flags, *PropertyFlags))
                        {
                            Audit.Add(TEXT("invalidDataAssetPropertyValue"), EntryPointer);
                            continue;
                        }

                        FPropertyBagContainerTypes ParsedContainerTypes;
                        bool bContainersValid = ContainerTypes->Num() <= 2;
                        for (const TSharedPtr<FJsonValue>& ContainerValue : *ContainerTypes)
                        {
                            FString ContainerName;
                            const int64 ContainerTypeValue =
                                ContainerValue.IsValid() && ContainerValue->TryGetString(ContainerName)
                                    && ContainerTypeEnum != nullptr
                                ? ContainerTypeEnum->GetValueByNameString(ContainerName)
                                : INDEX_NONE;
                            if (ContainerTypeValue <= static_cast<int64>(EPropertyBagContainerType::None)
                                || ContainerTypeValue >= static_cast<int64>(EPropertyBagContainerType::Count)
                                || !ParsedContainerTypes.Add(
                                    static_cast<EPropertyBagContainerType>(ContainerTypeValue)))
                            {
                                bContainersValid = false;
                                break;
                            }
                        }
                        if (!bContainersValid)
                        {
                            Audit.Add(
                                TEXT("invalidDataAssetPropertyValue"),
                                EntryPointer + TEXT("/containerTypes"));
                            continue;
                        }

                        const auto LoadTypeObject = [&](const TCHAR* Field) -> const UObject*
                        {
                            const TSharedPtr<FJsonObject>* Reference = nullptr;
                            FString ObjectPath;
                            if (!Entry->TryGetObjectField(Field, Reference) || Reference == nullptr)
                            {
                                return nullptr;
                            }
                            if (!(*Reference)->TryGetStringField(TEXT("objectPath"), ObjectPath)
                                || ObjectPath.IsEmpty())
                            {
                                return nullptr;
                            }
                            return StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
                        };

                        FPropertyBagPropertyDesc Desc;
                        Desc.ID = Guid;
                        Desc.Name = FName(*Name);
                        Desc.ValueType = static_cast<EPropertyBagPropertyType>(ValueTypeValue);
                        Desc.ContainerTypes = ParsedContainerTypes;
                        Desc.PropertyFlags = Flags;
                        Desc.KeyType = static_cast<EPropertyBagPropertyType>(KeyTypeValue);
                        Desc.ValueTypeObject = LoadTypeObject(TEXT("valueTypeObject"));
                        Desc.KeyTypeObject = LoadTypeObject(TEXT("keyTypeObject"));
#if WITH_EDITORONLY_DATA
                        const TArray<TSharedPtr<FJsonValue>>* MetaData = nullptr;
                        if (Entry->TryGetArrayField(TEXT("metadata"), MetaData) && MetaData != nullptr)
                        {
                            for (const TSharedPtr<FJsonValue>& MetaValue : *MetaData)
                            {
                                const TSharedPtr<FJsonObject> Meta = MetaValue.IsValid()
                                    ? MetaValue->AsObject()
                                    : nullptr;
                                FString Key;
                                FString MetaText;
                                if (!Meta.IsValid()
                                    || !Meta->TryGetStringField(TEXT("key"), Key)
                                    || !Meta->TryGetStringField(TEXT("value"), MetaText))
                                {
                                    Audit.Add(
                                        TEXT("invalidDataAssetPropertyValue"),
                                        EntryPointer + TEXT("/metadata"));
                                    continue;
                                }
                                Desc.MetaData.Emplace(FName(*Key), MetaText);
                            }
                        }
                        Desc.MetaClass = Cast<UClass>(
                            const_cast<UObject*>(LoadTypeObject(TEXT("metaClass"))));
#endif
                        Descs.Add(MoveTemp(Desc));
                        Entries.Add(Entry);
                    }
                    if (Descs.Num() != Properties->Num()) return;

                    const UPropertyBag* BagStruct = UPropertyBag::GetOrCreateFromDescs(Descs);
                    if (BagStruct == nullptr
                        || BagStruct->GetName() != LayoutId
                        || BagStruct->GetPropertyDescs().Num() != Descs.Num())
                    {
                        Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer + TEXT("/layoutId"));
                        return;
                    }
                    for (int32 Index = 0; Index < Entries.Num(); ++Index)
                    {
                        const FPropertyBagPropertyDesc& Desc = BagStruct->GetPropertyDescs()[Index];
                        const FProperty* DynamicProperty = nullptr;
                        for (TFieldIterator<FProperty> It(BagStruct); It; ++It)
                        {
                            if (BagStruct->FindPropertyDescByProperty(*It) == &Desc)
                            {
                                DynamicProperty = *It;
                                break;
                            }
                        }
                        if (DynamicProperty == nullptr)
                        {
                            Audit.Add(
                                TEXT("unsupportedDataAssetPropertyType"),
                                FString::Printf(TEXT("%s/properties/%d"), *Pointer, Index));
                            continue;
                        }
                        AuditDataAssetProperty(
                            *DynamicProperty,
                            Entries[Index]->Values.FindRef(TEXT("value")),
                            SourcePackage,
                            OwnedObjects,
                            FString::Printf(TEXT("%s/properties/%d/value"), *Pointer, Index),
                            Audit);
                    }
                    return;
                }
                const TSharedPtr<FJsonObject>* Object = nullptr;
                const TSharedPtr<FJsonObject>* Fields = nullptr;
                FString StructType;
                if (!Value->TryGetObject(Object) || Object == nullptr
                    || !(*Object)->TryGetStringField(TEXT("structType"), StructType)
                    || StructType != Struct->Struct->GetPathName()
                    || !(*Object)->TryGetObjectField(TEXT("fields"), Fields)
                    || Fields == nullptr)
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                    return;
                }
                for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Fields)->Values)
                {
                    const FProperty* Field = FindFProperty<FProperty>(Struct->Struct, *Pair.Key);
                    if (Field == nullptr || !UERingPropertySerializer::ShouldExportProperty(*Field))
                    {
                        Audit.Add(TEXT("unsupportedDataAssetPropertyType"), Pointer + TEXT("/fields/") + Pair.Key);
                        continue;
                    }
                    AuditDataAssetProperty(
                        *Field,
                        Pair.Value,
                        SourcePackage,
                        OwnedObjects,
                        Pointer + TEXT("/fields/") + Pair.Key,
                        Audit);
                }
                return;
            }

            Audit.Add(TEXT("unsupportedDataAssetPropertyType"), Pointer);
        }

        void AuditDataAssetProperty(
            const FProperty& Property,
            const TSharedPtr<FJsonValue>& Value,
            const FString& SourcePackage,
            const TMap<FString, UObject*>& OwnedObjects,
            const FString& Pointer,
            FDataAssetRecoveryAudit& Audit)
        {
            if (Property.ArrayDim <= 1)
            {
                AuditDataAssetValue(Property, Value, SourcePackage, OwnedObjects, Pointer, Audit);
                return;
            }
            const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
            if (!Value.IsValid() || !Value->TryGetArray(Values) || Values == nullptr
                || Values->Num() != Property.ArrayDim)
            {
                Audit.Add(TEXT("invalidDataAssetPropertyValue"), Pointer);
                return;
            }
            for (int32 Index = 0; Index < Values->Num(); ++Index)
            {
                AuditDataAssetValue(
                    Property,
                    (*Values)[Index],
                    SourcePackage,
                    OwnedObjects,
                    FString::Printf(TEXT("%s/%d"), *Pointer, Index),
                    Audit);
            }
        }

        void AuditDataAssetPropertyEntries(
            const UObject& Object,
            const TArray<TSharedPtr<FJsonValue>>& Properties,
            const FString& PointerBase,
            const FString& SourcePackage,
            const TMap<FString, UObject*>& OwnedObjects,
            FDataAssetRecoveryAudit& Audit)
        {
            for (int32 Index = 0; Index < Properties.Num(); ++Index)
            {
                const TSharedPtr<FJsonObject> Entry = Properties[Index].IsValid()
                    ? Properties[Index]->AsObject()
                    : nullptr;
                FString Name;
                if (!Entry.IsValid() || !Entry->TryGetStringField(TEXT("name"), Name))
                {
                    Audit.Add(TEXT("invalidDataAssetPropertyValue"),
                        FString::Printf(TEXT("%s/%d"), *PointerBase, Index));
                    continue;
                }
                const FProperty* Property = FindFProperty<FProperty>(Object.GetClass(), *Name);
                if (Property == nullptr || !UERingPropertySerializer::ShouldExportProperty(*Property))
                {
                    Audit.Add(TEXT("unsupportedDataAssetPropertyType"),
                        FString::Printf(TEXT("%s/%d"), *PointerBase, Index));
                    continue;
                }
                AuditDataAssetProperty(
                    *Property,
                    Entry->Values.FindRef(TEXT("value")),
                    SourcePackage,
                    OwnedObjects,
                    FString::Printf(TEXT("%s/%d/value"), *PointerBase, Index),
                    Audit);
            }
        }

        FDataAssetRecoveryAudits AuditDataAsset(
            const UObject& Asset,
            const TSharedRef<FJsonObject>& Semantics)
        {
            FDataAssetRecoveryAudits Audits;
            TArray<UObject*> ActualOwnedObjects;
            GetObjectsWithOuter(
                &Asset,
                ActualOwnedObjects,
                EGetObjectsFlags::IncludeNestedObjects,
                RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject,
                EInternalObjectFlags::Garbage);
            TMap<FString, UObject*> ActualById;
            for (UObject* Object : ActualOwnedObjects)
            {
                if (Object != nullptr && Object->GetOutermost() == Asset.GetOutermost())
                {
                    ActualById.Add(Object->GetPathName(&Asset), Object);
                }
            }

            const TArray<TSharedPtr<FJsonValue>>* OwnedEntries = nullptr;
            if (!Semantics->TryGetArrayField(TEXT("ownedObjects"), OwnedEntries) || OwnedEntries == nullptr)
            {
                Audits.OwnedStructure.Add(
                    TEXT("invalidDataAssetOwnedObjectGraph"), TEXT("/semantics/ownedObjects"));
                return Audits;
            }
            Audits.OwnedObjectCount = OwnedEntries->Num();
            TMap<FString, UObject*> ExportedOwnedObjects;
            ExportedOwnedObjects.Add(TEXT("$asset"), const_cast<UObject*>(&Asset));
            for (int32 Index = 0; Index < OwnedEntries->Num(); ++Index)
            {
                const FString Pointer = FString::Printf(TEXT("/semantics/ownedObjects/%d"), Index);
                const TSharedPtr<FJsonObject> Entry = (*OwnedEntries)[Index].IsValid()
                    ? (*OwnedEntries)[Index]->AsObject()
                    : nullptr;
                FString Id;
                FString Name;
                FString ClassPath;
                FString OuterId;
                FString CreationMethod;
                if (!Entry.IsValid()
                    || !Entry->TryGetStringField(TEXT("id"), Id)
                    || Id.IsEmpty()
                    || !Entry->TryGetStringField(TEXT("name"), Name)
                    || !Entry->TryGetStringField(TEXT("class"), ClassPath)
                    || !Entry->TryGetStringField(TEXT("outerId"), OuterId)
                    || !Entry->TryGetStringField(TEXT("creationMethod"), CreationMethod)
                    || (CreationMethod != TEXT("newObject")
                        && CreationMethod != TEXT("findDefaultSubobject")))
                {
                    Audits.OwnedStructure.Add(TEXT("invalidDataAssetOwnedObjectGraph"), Pointer);
                    continue;
                }
                if (ExportedOwnedObjects.Contains(Id))
                {
                    Audits.OwnedStructure.Add(
                        TEXT("invalidDataAssetOwnedObjectGraph"), Pointer + TEXT("/id"));
                    continue;
                }
                UObject* const* Actual = ActualById.Find(Id);
                if (Actual == nullptr || *Actual == nullptr)
                {
                    Audits.OwnedStructure.Add(
                        TEXT("invalidDataAssetOwnedObjectGraph"), Pointer + TEXT("/id"));
                    continue;
                }
                UObject* ExpectedOuter = nullptr;
                if (OuterId == TEXT("$asset"))
                {
                    ExpectedOuter = const_cast<UObject*>(&Asset);
                }
                else if (UObject* const* Outer = ExportedOwnedObjects.Find(OuterId))
                {
                    ExpectedOuter = *Outer;
                }
                if (ExpectedOuter == nullptr
                    || (*Actual)->GetName() != Name
                    || (*Actual)->GetClass()->GetPathName() != ClassPath
                    || (*Actual)->GetOuter() != ExpectedOuter
                    || ((*Actual)->HasAnyFlags(RF_DefaultSubObject)
                        != (CreationMethod == TEXT("findDefaultSubobject"))))
                {
                    Audits.OwnedStructure.Add(TEXT("invalidDataAssetOwnedObjectGraph"), Pointer);
                    continue;
                }
                ExportedOwnedObjects.Add(Id, *Actual);
            }

            for (int32 Index = 0; Index < OwnedEntries->Num(); ++Index)
            {
                const TSharedPtr<FJsonObject> Entry = (*OwnedEntries)[Index].IsValid()
                    ? (*OwnedEntries)[Index]->AsObject()
                    : nullptr;
                FString Id;
                const TArray<TSharedPtr<FJsonValue>>* Properties = nullptr;
                if (!Entry.IsValid()
                    || !Entry->TryGetStringField(TEXT("id"), Id)
                    || !Entry->TryGetArrayField(TEXT("properties"), Properties)
                    || Properties == nullptr)
                {
                    Audits.OwnedProperties.Add(
                        TEXT("invalidDataAssetOwnedObjectGraph"),
                        FString::Printf(TEXT("/semantics/ownedObjects/%d/properties"), Index));
                    continue;
                }
                UObject* const* Object = ExportedOwnedObjects.Find(Id);
                if (Object == nullptr || *Object == nullptr)
                {
                    Audits.OwnedProperties.Add(
                        TEXT("invalidDataAssetOwnedObjectGraph"),
                        FString::Printf(TEXT("/semantics/ownedObjects/%d/id"), Index));
                    continue;
                }
                AuditDataAssetPropertyEntries(
                    **Object,
                    *Properties,
                    FString::Printf(TEXT("/semantics/ownedObjects/%d/properties"), Index),
                    Asset.GetOutermost()->GetName(),
                    ExportedOwnedObjects,
                    Audits.OwnedProperties);
            }

            const TArray<TSharedPtr<FJsonValue>>* Properties = nullptr;
            if (!Semantics->TryGetArrayField(TEXT("properties"), Properties) || Properties == nullptr)
            {
                Audits.RootProperties.Add(
                    TEXT("invalidDataAssetPropertyValue"), TEXT("/semantics/properties"));
                return Audits;
            }
            TArray<TSharedPtr<FJsonValue>> AuditedProperties = *Properties;
            const TSharedPtr<FJsonObject>* ReconstructionPolicy = nullptr;
            FString Strategy;
            const TArray<TSharedPtr<FJsonValue>>* AuthoredNames = nullptr;
            if (Semantics->TryGetObjectField(TEXT("reconstructionPolicy"), ReconstructionPolicy)
                && ReconstructionPolicy != nullptr
                && (*ReconstructionPolicy)->TryGetStringField(TEXT("strategy"), Strategy)
                && Strategy == TEXT("state-tree-editor-compile-v1")
                && (*ReconstructionPolicy)->TryGetArrayField(
                    TEXT("authoredRootProperties"), AuthoredNames)
                && AuthoredNames != nullptr)
            {
                TSet<FString> AuthoredNameSet;
                for (const TSharedPtr<FJsonValue>& NameValue : *AuthoredNames)
                {
                    FString Name;
                    if (NameValue.IsValid() && NameValue->TryGetString(Name))
                    {
                        AuthoredNameSet.Add(Name);
                    }
                }
                AuditedProperties.RemoveAll([&AuthoredNameSet](const TSharedPtr<FJsonValue>& Value)
                {
                    const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
                    FString Name;
                    return !Entry.IsValid()
                        || !Entry->TryGetStringField(TEXT("name"), Name)
                        || !AuthoredNameSet.Contains(Name);
                });
                if (AuditedProperties.Num() != AuthoredNameSet.Num())
                {
                    Audits.RootProperties.Add(
                        TEXT("invalidDataAssetPropertyValue"),
                        TEXT("/semantics/reconstructionPolicy/authoredRootProperties"));
                }
            }
            AuditDataAssetPropertyEntries(
                Asset,
                AuditedProperties,
                TEXT("/semantics/properties"),
                Asset.GetOutermost()->GetName(),
                ExportedOwnedObjects,
                Audits.RootProperties);
            return Audits;
        }
    }

    bool AddDomainSemantics(
        const FUERingExportContext& Context,
        const TSharedRef<FJsonObject>& Semantics)
    {
        UObject* Asset = Context.Asset.Get();
        if (Asset == nullptr || Context.AssetData.AssetClassPath.GetAssetName() == TEXT("ObjectRedirector"))
        {
            return false;
        }

        const TArray<FString> Lineage = ClassLineage(*Asset);
        const FString Detection = DetectionText(Context, Lineage);
        TArray<const FDomainRule*> Matched;
        for (const FDomainRule& Rule : Rules())
        {
            if (MatchesClassRule(Context, Lineage, Rule)) Matched.Add(&Rule);
        }
        if (Matched.IsEmpty()) return false;

        const TSharedRef<FJsonObject> Domain = MakeShared<FJsonObject>();
        Domain->SetStringField(TEXT("representation"), TEXT("domain-projection-v1"));
        Domain->SetArrayField(TEXT("classLineage"), Strings(Lineage));
        const TSharedRef<FJsonObject> Projections = MakeShared<FJsonObject>();
        const TArray<TSharedPtr<FJsonValue>>* CanonicalAssetProperties = nullptr;
        Semantics->TryGetArrayField(TEXT("properties"), CanonicalAssetProperties);

        UObject* ClassDefault = nullptr;
        const UObject* ParentDefault = nullptr;
        if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset))
        {
            if (Blueprint->GeneratedClass != nullptr)
            {
                ClassDefault = Blueprint->GeneratedClass->GetDefaultObject(false);
                if (Blueprint->ParentClass != nullptr)
                {
                    ParentDefault = Blueprint->ParentClass->GetDefaultObject(false);
                }
            }
        }

        for (const FDomainRule* Rule : Matched)
        {
            const TSharedRef<FJsonObject> Projection = MakeShared<FJsonObject>();
            const FString Role = RoleFor(Rule->Key, Detection);
            Projection->SetStringField(TEXT("role"), Role);
            Projection->SetStringField(
                TEXT("representation"),
                FString(Rule->Key) + TEXT("-") + Role + TEXT("-v2"));
            TArray<TSharedPtr<FJsonValue>> AssetProperties =
                SelectedProperties(*Asset, Rule->Key, Role, Rule->PropertyMarkers);
            ReuseCanonicalPropertyValues(AssetProperties, CanonicalAssetProperties);
            if (!AssetProperties.IsEmpty())
            {
                Projection->SetObjectField(TEXT("asset"), NormalizeProperties(AssetProperties, Rule->Key));
            }
            if (ClassDefault != nullptr)
            {
                const TArray<TSharedPtr<FJsonValue>> Defaults =
                    SelectedProperties(
                        *ClassDefault,
                        Rule->Key,
                        Role,
                        Rule->PropertyMarkers,
                        ParentDefault);
                if (!Defaults.IsEmpty())
                {
                    Projection->SetObjectField(TEXT("defaults"), NormalizeProperties(Defaults, Rule->Key));
                }
            }
            Projections->SetObjectField(Rule->Key, Projection);
        }

        Domain->SetObjectField(TEXT("projections"), Projections);
        AddOwnedObjects(*Asset, ClassDefault, Matched, Semantics, Domain);
        AddAssetReferences(*Asset, Domain);
        Semantics->SetObjectField(TEXT("domain"), Domain);
        return true;
    }

    TSharedRef<FJsonObject> BuildReconstructionIR(
        const FUERingExportContext& Context,
        const FString& ExporterName,
        const TSharedRef<FJsonObject>& Semantics,
        const TArray<TSharedPtr<FJsonValue>>& Omissions,
        const bool bHasDomainSemantics)
    {
        FString Kind;
        Semantics->TryGetStringField(TEXT("kind"), Kind);
        const TSharedRef<FJsonObject> IR = MakeShared<FJsonObject>();
        IR->SetStringField(TEXT("irVersion"), TEXT("2.0.0"));
        IR->SetStringField(TEXT("contract"), TEXT("com.ue-ring.reconstruction"));
        IR->SetStringField(TEXT("assetKind"), Kind);
        IR->SetStringField(TEXT("profile"), UERingExportProfileName(Context.Profile));

        const TSharedRef<FJsonObject> Source = MakeShared<FJsonObject>();
        Source->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
        Source->SetNumberField(TEXT("semanticRevision"), UE_RING_SEMANTIC_REVISION);
        Source->SetStringField(TEXT("inputFingerprint"), Context.InputFingerprint);
        Source->SetStringField(TEXT("sourceHash"), Context.SourceHash);
        Source->SetStringField(TEXT("assetPointer"), TEXT("/asset"));
        Source->SetStringField(TEXT("semanticsPointer"), TEXT("/semantics"));
        IR->SetObjectField(TEXT("source"), Source);

        TArray<TSharedPtr<FJsonValue>> JsonSymbols;
        TArray<TSharedPtr<FJsonValue>> JsonOperations;
        TArray<TSharedPtr<FJsonValue>> JsonLosses;
        TArray<FString> BlockerRefs;
        int32 ExactCount = 0;
        int32 InferredCount = 0;
        int32 UnsupportedCount = 0;

        const bool bBlueprint = ExporterName.Contains(TEXT("Blueprint"))
            || ExporterName == TEXT("ControlRig");
        FString Representation;
        Semantics->TryGetStringField(TEXT("representation"), Representation);
        const bool bMaterialInstance = Context.AssetData.AssetClassPath.GetAssetName()
                == TEXT("MaterialInstanceConstant")
            && Representation == TEXT("material-instance-v1");
        const bool bDataAsset = ExporterName == TEXT("DataAsset")
            && Cast<UDataAsset>(Context.Asset.Get()) != nullptr
            && Representation == TEXT("data-asset-properties-v2");
        const FString TargetId = bBlueprint ? TEXT("target:cpp") : TEXT("target:editor");

        auto AddExecutable = [&](const TSharedRef<FJsonObject>& Value, const bool bInferred = false)
        {
            JsonOperations.Add(MakeShared<FJsonValueObject>(Value));
            bInferred ? ++InferredCount : ++ExactCount;
        };
        auto AddBlocked = [&](const TSharedRef<FJsonObject>& Value, const FString& ReasonCode,
                              const TCHAR* Impact, const TArray<FString>& SourcePointers)
        {
            JsonOperations.Add(MakeShared<FJsonValueObject>(Value));
            ++UnsupportedCount;
            const FString LossId = TEXT("loss:") + StableIdentifier(Value->GetStringField(TEXT("id")));
            JsonLosses.Add(MakeShared<FJsonValueObject>(Loss(LossId, ReasonCode, Impact, SourcePointers)));
            BlockerRefs.Add(LossId);
        };

        const FString AssetPath = Context.AssetData.GetSoftObjectPath().ToString();
        JsonSymbols.Add(MakeShared<FJsonValueObject>(Symbol(
            TEXT("symbol:asset"),
            TEXT("asset"),
            AssetPath,
            FString(),
            FString(),
            FString(),
            TEXT("exact"),
            TEXT("/asset/objectPath"))));

        if (bBlueprint)
        {
            const UBlueprint* Blueprint = Cast<UBlueprint>(Context.Asset.Get());
            const UClass* ParentClass = Blueprint != nullptr ? Blueprint->ParentClass : nullptr;
            const UClass* GeneratedClass = Blueprint != nullptr ? Blueprint->GeneratedClass : nullptr;
            const FString BlueprintName = Blueprint != nullptr
                ? Blueprint->GetName()
                : Context.AssetData.AssetName.ToString();
            const FString CppName = (ParentClass != nullptr ? ParentClass->GetPrefixCPP() : TEXT("U"))
                + BlueprintName;
            const FString ParentSymbolId = TEXT("symbol:parent");
            const FString ParentHeader = ParentClass != nullptr
                ? CanonicalModuleHeader(ParentClass->GetMetaData(TEXT("ModuleRelativePath")))
                : FString();
            JsonSymbols.Add(MakeShared<FJsonValueObject>(Symbol(
                TEXT("symbol:generated"),
                TEXT("generatedClass"),
                GeneratedClass != nullptr ? GeneratedClass->GetPathName() : AssetPath + TEXT("_C"),
                CppName,
                TEXT("UERingGenerated"),
                BlueprintName + TEXT(".h"),
                TEXT("exact"),
                TEXT("/asset/objectPath"))));
            JsonSymbols.Add(MakeShared<FJsonValueObject>(Symbol(
                ParentSymbolId,
                TEXT("nativeClass"),
                ParentClass != nullptr ? ParentClass->GetPathName() : FString(),
                NativeCppName(ParentClass),
                NativeModuleName(ParentClass),
                ParentHeader,
                ParentClass != nullptr && !ParentHeader.IsEmpty() ? TEXT("exact") : TEXT("unresolved"),
                TEXT("/semantics/parentClass"))));

            const TSharedRef<FJsonObject> ClassOperands = MakeShared<FJsonObject>();
            ClassOperands->SetStringField(TEXT("symbolId"), TEXT("symbol:generated"));
            ClassOperands->SetStringField(TEXT("name"), BlueprintName);
            ClassOperands->SetStringField(TEXT("cppName"), CppName);
            ClassOperands->SetStringField(TEXT("parentSymbolId"), ParentSymbolId);
            ClassOperands->SetStringField(TEXT("module"), TEXT("UERingGenerated"));
            ClassOperands->SetStringField(TEXT("header"), BlueprintName + TEXT(".h"));
            AddExecutable(Operation(
                Context,
                TEXT("op:class"),
                TEXT("cpp.class.declare"),
                TEXT("declare"),
                TargetId,
                {},
                ClassOperands,
                { TEXT("/asset"), TEXT("/semantics/parentClass") },
                TEXT("structure"),
                TEXT("executable"),
                TEXT("exact"),
                TEXT("blueprint.class.v1"),
                1.0));

            const TArray<TSharedPtr<FJsonValue>>* Variables = nullptr;
            if (Semantics->TryGetArrayField(TEXT("variables"), Variables) && Variables != nullptr)
            {
                for (int32 Index = 0; Index < Variables->Num(); ++Index)
                {
                    const TSharedPtr<FJsonObject> Variable = (*Variables)[Index].IsValid()
                        ? (*Variables)[Index]->AsObject()
                        : nullptr;
                    if (!Variable.IsValid()) continue;
                    FString Name;
                    Variable->TryGetStringField(TEXT("name"), Name);
                    const FProperty* Property = GeneratedClass != nullptr
                        ? FindFProperty<FProperty>(GeneratedClass, *Name)
                        : nullptr;
                    const FString OpId = TEXT("op:property:") + StableIdentifier(Name);
                    const FString Pointer = FString::Printf(TEXT("/semantics/variables/%d"), Index);
                    const TSharedRef<FJsonObject> Operands = MakeShared<FJsonObject>();
                    Operands->SetStringField(TEXT("ownerSymbolId"), TEXT("symbol:generated"));
                    Operands->SetStringField(TEXT("name"), Name);
                    FString Category;
                    if (Variable->TryGetStringField(TEXT("category"), Category) && !Category.IsEmpty())
                    {
                        Operands->SetStringField(TEXT("category"), Category);
                    }
                    if (Property != nullptr)
                    {
                        FString ExtendedType;
                        const FString CppType = Property->GetCPPType(&ExtendedType) + ExtendedType;
                        Operands->SetStringField(TEXT("cppType"), CppType);
                        Operands->SetNumberField(TEXT("arrayDim"), Property->ArrayDim);
                    }
                    if (const TSharedPtr<FJsonValue>* Type = Variable->Values.Find(TEXT("type")))
                    {
                        Operands->SetField(TEXT("type"), *Type);
                    }
                    if (const TSharedPtr<FJsonValue>* DefaultValue = Variable->Values.Find(TEXT("defaultValue")))
                    {
                        Operands->SetField(TEXT("defaultValue"), *DefaultValue);
                    }
                    const TArray<TSharedPtr<FJsonValue>>* Flags = nullptr;
                    if (Variable->TryGetArrayField(TEXT("flags"), Flags) && Flags != nullptr)
                    {
                        Operands->SetArrayField(TEXT("flags"), *Flags);
                    }
                    const TSharedRef<FJsonObject> PropertyOperation = Operation(
                        Context,
                        OpId,
                        TEXT("cpp.property.declare"),
                        TEXT("declare"),
                        TargetId,
                        { TEXT("op:class") },
                        Operands,
                        { Pointer },
                        TEXT("structure"),
                        Property != nullptr ? TEXT("executable") : TEXT("blocked"),
                        Property != nullptr ? TEXT("exact") : TEXT("unsupported"),
                        TEXT("blueprint.property.v1"),
                        Property != nullptr ? 1.0 : 0.0,
                        Property != nullptr ? FString() : TEXT("unresolvedPropertyType"));
                    if (Property != nullptr)
                    {
                        AddExecutable(PropertyOperation);
                    }
                    else
                    {
                        AddBlocked(
                            PropertyOperation,
                            TEXT("unresolvedPropertyType"),
                            TEXT("blocksReconstruction"),
                            { Pointer });
                    }
                }
            }

            const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
            if (Semantics->TryGetArrayField(TEXT("graphs"), Graphs) && Graphs != nullptr)
            {
                for (int32 Index = 0; Index < Graphs->Num(); ++Index)
                {
                    const TSharedPtr<FJsonObject> Graph = (*Graphs)[Index].IsValid()
                        ? (*Graphs)[Index]->AsObject()
                        : nullptr;
                    if (!Graph.IsValid()) continue;
                    FString Name;
                    FString GraphPath;
                    Graph->TryGetStringField(TEXT("name"), Name);
                    Graph->TryGetStringField(TEXT("graphPath"), GraphPath);
                    const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
                    const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
                    Graph->TryGetArrayField(TEXT("nodes"), Nodes);
                    Graph->TryGetArrayField(TEXT("links"), Links);
                    const int32 NodeCount = Nodes != nullptr ? Nodes->Num() : 0;
                    const FString Pointer = FString::Printf(TEXT("/semantics/graphs/%d"), Index);
                    const TSharedRef<FJsonObject> Operands = MakeShared<FJsonObject>();
                    Operands->SetStringField(TEXT("name"), Name);
                    Operands->SetStringField(TEXT("graphPath"), GraphPath);
                    Operands->SetNumberField(TEXT("nodeCount"), NodeCount);
                    Operands->SetNumberField(TEXT("linkCount"), Links != nullptr ? Links->Num() : 0);
                    if (NodeCount > 0)
                    {
                        TArray<FString> NodeClasses;
                        for (const TSharedPtr<FJsonValue>& NodeValue : *Nodes)
                        {
                            const TSharedPtr<FJsonObject> Node = NodeValue.IsValid()
                                ? NodeValue->AsObject()
                                : nullptr;
                            FString NodeClass;
                            if (Node.IsValid() && Node->TryGetStringField(TEXT("class"), NodeClass))
                            {
                                NodeClasses.AddUnique(NodeClass);
                            }
                        }
                        NodeClasses.Sort();
                        Operands->SetArrayField(TEXT("nodeClasses"), Strings(NodeClasses));
                    }
                    const FString OpId = TEXT("op:graph:")
                        + FString::FromInt(Index) + TEXT(":") + StableIdentifier(Name);
                    const TSharedRef<FJsonObject> GraphOperation = Operation(
                        Context,
                        OpId,
                        NodeCount == 0 ? TEXT("cpp.graph.assertEmpty") : TEXT("cpp.graph.translate"),
                        TEXT("translate"),
                        TargetId,
                        { TEXT("op:class") },
                        Operands,
                        { Pointer },
                        TEXT("behavior"),
                        NodeCount == 0 ? TEXT("executable") : TEXT("blocked"),
                        NodeCount == 0 ? TEXT("exact") : TEXT("unsupported"),
                        NodeCount == 0 ? TEXT("blueprint.empty-graph.v1") : TEXT("blueprint.graph-lowering.v1"),
                        NodeCount == 0 ? 1.0 : 0.0,
                        NodeCount == 0 ? FString() : TEXT("unsupportedGraphNodes"));
                    if (NodeCount == 0)
                    {
                        AddExecutable(GraphOperation);
                    }
                    else
                    {
                        AddBlocked(
                            GraphOperation,
                            TEXT("unsupportedGraphNodes"),
                            TEXT("blocksReconstruction"),
                            { Pointer });
                    }
                }
            }

            auto AddUnsupportedSection = [&](const TCHAR* Field, const TCHAR* Opcode,
                                             const TCHAR* ReasonCode, const bool bBehaviorCritical)
            {
                const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
                if (!Semantics->TryGetArrayField(Field, Values) || Values == nullptr || Values->IsEmpty()) return;
                const FString Pointer = TEXT("/semantics/") + FString(Field);
                const FString OpId = TEXT("op:section:") + StableIdentifier(Field);
                const TSharedRef<FJsonObject> Operands = MakeShared<FJsonObject>();
                Operands->SetStringField(TEXT("section"), Field);
                Operands->SetNumberField(TEXT("count"), Values->Num());
                const TSharedRef<FJsonObject> Value = Operation(
                    Context,
                    OpId,
                    Opcode,
                    TEXT("configure"),
                    TargetId,
                    { TEXT("op:class") },
                    Operands,
                    { Pointer },
                    bBehaviorCritical ? TEXT("behavior") : TEXT("defaults"),
                    TEXT("blocked"),
                    TEXT("unsupported"),
                    TEXT("blueprint.section.v1"),
                    0.0,
                    ReasonCode);
                AddBlocked(
                    Value,
                    ReasonCode,
                    TEXT("blocksReconstruction"),
                    { Pointer });
            };
            AddUnsupportedSection(TEXT("interfaces"), TEXT("cpp.interface.translate"),
                TEXT("interfaceGenerationUnavailable"), true);
            AddUnsupportedSection(TEXT("components"), TEXT("cpp.component.translate"),
                TEXT("componentConstructionUnavailable"), true);
            AddUnsupportedSection(TEXT("classDefaults"), TEXT("cpp.defaults.translate"),
                TEXT("classDefaultLoweringUnavailable"), false);
            AddUnsupportedSection(TEXT("timelines"), TEXT("cpp.timeline.translate"),
                TEXT("timelineLoweringUnavailable"), true);
        }
        else if (bDataAsset)
        {
            UObject* DataAsset = Context.Asset.Get();
            const FString ClassPath = DataAsset->GetClass()->GetPathName();
            const FDataAssetRecoveryAudits Audits = AuditDataAsset(*DataAsset, Semantics);
            JsonSymbols.Add(MakeShared<FJsonValueObject>(Symbol(
                TEXT("symbol:data-asset-class"),
                TEXT("nativeClass"),
                ClassPath,
                NativeCppName(DataAsset->GetClass()),
                NativeModuleName(DataAsset->GetClass()),
                CanonicalModuleHeader(DataAsset->GetClass()->GetMetaData(TEXT("ModuleRelativePath"))),
                TEXT("exact"),
                TEXT("/semantics/class"))));

            const TSharedRef<FJsonObject> CreateOperands = MakeShared<FJsonObject>();
            CreateOperands->SetStringField(TEXT("classSymbolId"), TEXT("symbol:data-asset-class"));
            CreateOperands->SetStringField(TEXT("classPath"), ClassPath);
            CreateOperands->SetStringField(TEXT("sourcePackageName"), Context.AssetData.PackageName.ToString());
            CreateOperands->SetStringField(TEXT("sourceObjectPath"), AssetPath);
            CreateOperands->SetStringField(TEXT("objectName"), Context.AssetData.AssetName.ToString());
            AddExecutable(Operation(
                Context,
                TEXT("op:data-asset:create"),
                TEXT("editor.dataAsset.create"),
                TEXT("declare"),
                TargetId,
                {},
                CreateOperands,
                { TEXT("/asset"), TEXT("/semantics/class") },
                TEXT("structure"),
                TEXT("executable"),
                TEXT("exact"),
                TEXT("data-asset.create.v2"),
                1.0));

            const bool bHasOwnedStage = Audits.OwnedObjectCount > 0
                || !Audits.OwnedStructure.IsExact();
            FString RootDependency = TEXT("op:data-asset:create");
            if (bHasOwnedStage)
            {
                const FString CreateOwnedReason = Audits.OwnedStructure.PrimaryReason();
                const TArray<FString> CreateOwnedPointers = Audits.OwnedStructure.IsExact()
                    ? TArray<FString>{ TEXT("/semantics/ownedObjects") }
                    : Audits.OwnedStructure.AllPointers();
                const TSharedRef<FJsonObject> CreateOwnedOperands = MakeShared<FJsonObject>();
                CreateOwnedOperands->SetStringField(
                    TEXT("ownedObjectsPointer"), TEXT("/semantics/ownedObjects"));
                CreateOwnedOperands->SetNumberField(
                    TEXT("ownedObjectCount"), Audits.OwnedObjectCount);
                const TSharedRef<FJsonObject> CreateOwnedOperation = Operation(
                    Context,
                    TEXT("op:data-asset:create-owned-objects"),
                    TEXT("editor.dataAsset.createOwnedObjects"),
                    TEXT("declare"),
                    TargetId,
                    { TEXT("op:data-asset:create") },
                    CreateOwnedOperands,
                    CreateOwnedPointers,
                    TEXT("structure"),
                    Audits.OwnedStructure.IsExact() ? TEXT("executable") : TEXT("blocked"),
                    Audits.OwnedStructure.IsExact() ? TEXT("exact") : TEXT("unsupported"),
                    TEXT("data-asset.owned-objects.create.v1"),
                    Audits.OwnedStructure.IsExact() ? 1.0 : 0.0,
                    CreateOwnedReason);
                if (Audits.OwnedStructure.IsExact())
                {
                    AddExecutable(CreateOwnedOperation);
                }
                else
                {
                    AddBlocked(
                        CreateOwnedOperation,
                        CreateOwnedReason,
                        TEXT("blocksReconstruction"),
                        CreateOwnedPointers);
                }

                FDataAssetRecoveryAudit OwnedApplyAudit = Audits.OwnedProperties;
                OwnedApplyAudit.Append(Audits.OwnedStructure);
                const FString ApplyOwnedReason = OwnedApplyAudit.PrimaryReason();
                const TArray<FString> ApplyOwnedPointers = OwnedApplyAudit.IsExact()
                    ? TArray<FString>{ TEXT("/semantics/ownedObjects") }
                    : OwnedApplyAudit.AllPointers();
                const TSharedRef<FJsonObject> ApplyOwnedOperands = MakeShared<FJsonObject>();
                ApplyOwnedOperands->SetStringField(
                    TEXT("ownedObjectsPointer"), TEXT("/semantics/ownedObjects"));
                ApplyOwnedOperands->SetStringField(TEXT("representation"), Representation);
                const TSharedRef<FJsonObject> ApplyOwnedOperation = Operation(
                    Context,
                    TEXT("op:data-asset:apply-owned-object-properties"),
                    TEXT("editor.dataAsset.applyOwnedObjectProperties"),
                    TEXT("configure"),
                    TargetId,
                    { TEXT("op:data-asset:create-owned-objects") },
                    ApplyOwnedOperands,
                    ApplyOwnedPointers,
                    TEXT("defaults"),
                    OwnedApplyAudit.IsExact() ? TEXT("executable") : TEXT("blocked"),
                    OwnedApplyAudit.IsExact() ? TEXT("exact") : TEXT("unsupported"),
                    TEXT("data-asset.owned-properties.v1"),
                    OwnedApplyAudit.IsExact() ? 1.0 : 0.0,
                    ApplyOwnedReason);
                if (OwnedApplyAudit.IsExact())
                {
                    AddExecutable(ApplyOwnedOperation);
                }
                else
                {
                    AddBlocked(
                        ApplyOwnedOperation,
                        ApplyOwnedReason,
                        TEXT("blocksReconstruction"),
                        ApplyOwnedPointers);
                }
                RootDependency = TEXT("op:data-asset:apply-owned-object-properties");
            }

            FDataAssetRecoveryAudit RootAudit = Audits.RootProperties;
            RootAudit.Append(Audits.OwnedStructure);
            const FString ApplyReason = RootAudit.PrimaryReason();
            const TArray<FString> ApplyPointers = RootAudit.IsExact()
                ? TArray<FString>{ TEXT("/semantics/properties") }
                : RootAudit.AllPointers();
            const TSharedRef<FJsonObject> ApplyOperands = MakeShared<FJsonObject>();
            ApplyOperands->SetStringField(TEXT("classSymbolId"), TEXT("symbol:data-asset-class"));
            ApplyOperands->SetStringField(TEXT("propertiesPointer"), TEXT("/semantics/properties"));
            ApplyOperands->SetStringField(TEXT("representation"), Representation);
            const TSharedRef<FJsonObject> ApplyOperation = Operation(
                Context,
                TEXT("op:data-asset:apply-properties"),
                TEXT("editor.dataAsset.applyProperties"),
                TEXT("configure"),
                TargetId,
                { RootDependency },
                ApplyOperands,
                ApplyPointers,
                TEXT("defaults"),
                RootAudit.IsExact() ? TEXT("executable") : TEXT("blocked"),
                RootAudit.IsExact() ? TEXT("exact") : TEXT("unsupported"),
                TEXT("data-asset.reflection-properties.v2"),
                RootAudit.IsExact() ? 1.0 : 0.0,
                ApplyReason);
            if (RootAudit.IsExact())
            {
                AddExecutable(ApplyOperation);
            }
            else
            {
                AddBlocked(
                    ApplyOperation,
                    ApplyReason,
                    TEXT("blocksReconstruction"),
                    ApplyPointers);
            }

            FString SaveDependency = TEXT("op:data-asset:apply-properties");
            const TSharedPtr<FJsonObject>* ReconstructionPolicy = nullptr;
            FString ReconstructionStrategy;
            if (Semantics->TryGetObjectField(TEXT("reconstructionPolicy"), ReconstructionPolicy)
                && ReconstructionPolicy != nullptr
                && (*ReconstructionPolicy)->TryGetStringField(
                    TEXT("strategy"), ReconstructionStrategy)
                && ReconstructionStrategy == TEXT("state-tree-editor-compile-v1"))
            {
                const TSharedRef<FJsonObject> CompileOperands = MakeShared<FJsonObject>();
                CompileOperands->SetStringField(TEXT("strategy"), ReconstructionStrategy);
                AddExecutable(Operation(
                    Context,
                    TEXT("op:data-asset:compile-state-tree"),
                    TEXT("editor.stateTree.compile"),
                    TEXT("configure"),
                    TargetId,
                    { TEXT("op:data-asset:apply-properties") },
                    CompileOperands,
                    { TEXT("/semantics/reconstructionPolicy") },
                    TEXT("behavior"),
                    TEXT("executable"),
                    TEXT("exact"),
                    TEXT("state-tree.editor-compile.v1"),
                    1.0));
                SaveDependency = TEXT("op:data-asset:compile-state-tree");
            }

            const TSharedRef<FJsonObject> SaveOperands = MakeShared<FJsonObject>();
            SaveOperands->SetStringField(TEXT("assetFormat"), TEXT("uasset"));
            AddExecutable(Operation(
                Context,
                TEXT("op:data-asset:save"),
                TEXT("editor.asset.save"),
                TEXT("verify"),
                TargetId,
                { SaveDependency },
                SaveOperands,
                { TEXT("/asset/sourceFile") },
                TEXT("structure"),
                TEXT("executable"),
                TEXT("exact"),
                TEXT("editor.asset.save.v1"),
                1.0));
        }
        else if (bMaterialInstance)
        {
            const TSharedPtr<FJsonObject>* Parent = nullptr;
            FString ParentPath;
            if (Semantics->TryGetObjectField(TEXT("parent"), Parent) && Parent != nullptr)
            {
                (*Parent)->TryGetStringField(TEXT("objectPath"), ParentPath);
            }
            JsonSymbols.Add(MakeShared<FJsonValueObject>(Symbol(
                TEXT("symbol:parent"),
                TEXT("materialInterface"),
                ParentPath,
                FString(),
                FString(),
                FString(),
                ParentPath.IsEmpty() ? TEXT("unresolved") : TEXT("exact"),
                TEXT("/semantics/parent/objectPath"))));

            TArray<FString> UnsupportedOverridePointers;
            const TSharedPtr<FJsonObject>* Overrides = nullptr;
            if (Semantics->TryGetObjectField(TEXT("overrides"), Overrides) && Overrides != nullptr)
            {
                auto AddNonEmptyArray = [&](const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FString& Pointer)
                {
                    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
                    if (Object.IsValid() && Object->TryGetArrayField(Field, Values) && Values != nullptr && !Values->IsEmpty())
                    {
                        UnsupportedOverridePointers.Add(Pointer);
                    }
                };
                for (const TCHAR* Field : {
                    TEXT("DoubleVectorParameterValues"), TEXT("FontParameterValues"),
                    TEXT("ParameterCollectionParameterValues"), TEXT("RuntimeVirtualTextureParameterValues"),
                    TEXT("SparseVolumeTextureParameterValues"), TEXT("TextureCollectionParameterValues"),
                    TEXT("UserSceneTextureOverrides") })
                {
                    AddNonEmptyArray(*Overrides, Field, TEXT("/semantics/overrides/") + FString(Field));
                }
                auto AddNonEmptyReference = [&](const TCHAR* Field)
                {
                    const TSharedPtr<FJsonObject>* Reference = nullptr;
                    FString ObjectPath;
                    if ((*Overrides)->TryGetObjectField(Field, Reference) && Reference != nullptr
                        && (*Reference)->TryGetStringField(TEXT("objectPath"), ObjectPath) && !ObjectPath.IsEmpty())
                    {
                        UnsupportedOverridePointers.Add(TEXT("/semantics/overrides/") + FString(Field));
                    }
                };
                for (const TCHAR* Field : { TEXT("PhysMaterialMask"), TEXT("SpecularProfileOverride"),
                                            TEXT("ToonProfileOverride"), TEXT("NeuralProfile") })
                {
                    AddNonEmptyReference(Field);
                }
                const TArray<TSharedPtr<FJsonValue>>* PhysicalMaterialMap = nullptr;
                if ((*Overrides)->TryGetArrayField(TEXT("PhysicalMaterialMap"), PhysicalMaterialMap) && PhysicalMaterialMap != nullptr)
                {
                    for (const TSharedPtr<FJsonValue>& Value : *PhysicalMaterialMap)
                    {
                        const TSharedPtr<FJsonObject> Reference = Value.IsValid() ? Value->AsObject() : nullptr;
                        FString ObjectPath;
                        if (Reference.IsValid() && Reference->TryGetStringField(TEXT("objectPath"), ObjectPath) && !ObjectPath.IsEmpty())
                        {
                            UnsupportedOverridePointers.Add(TEXT("/semantics/overrides/PhysicalMaterialMap"));
                            break;
                        }
                    }
                }
                bool bUnsupportedProfileOverride = false;
                (*Overrides)->TryGetBoolField(TEXT("bOverrideSpecularProfile"), bUnsupportedProfileOverride);
                if (!bUnsupportedProfileOverride)
                {
                    (*Overrides)->TryGetBoolField(TEXT("bOverrideToonProfile"), bUnsupportedProfileOverride);
                }
                if (bUnsupportedProfileOverride)
                {
                    UnsupportedOverridePointers.Add(TEXT("/semantics/overrides/ProfileOverrides"));
                }
                bool bUnsupportedBlendableOverride = false;
                (*Overrides)->TryGetBoolField(TEXT("bOverrideBlendableLocation"), bUnsupportedBlendableOverride);
                if (!bUnsupportedBlendableOverride)
                {
                    (*Overrides)->TryGetBoolField(TEXT("bOverrideBlendablePriority"), bUnsupportedBlendableOverride);
                }
                if (bUnsupportedBlendableOverride)
                {
                    UnsupportedOverridePointers.Add(TEXT("/semantics/overrides/BlendableOverrides"));
                }
                auto HasMaterialLayers = [](const TSharedPtr<FJsonObject>& MaterialLayers)
                {
                    if (!MaterialLayers.IsValid()) return false;
                    for (const TCHAR* Field : { TEXT("Blends"), TEXT("Layers"), TEXT("DeletedParentLayerGuids"),
                                                TEXT("LayerGuids"), TEXT("LayerLinkStates"), TEXT("LayerNames"),
                                                TEXT("LayerStates"), TEXT("RestrictToBlendRelatives"),
                                                TEXT("RestrictToLayerRelatives") })
                    {
                        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
                        if (MaterialLayers->TryGetArrayField(Field, Values) && Values != nullptr && !Values->IsEmpty())
                        {
                            return true;
                        }
                    }
                    return false;
                };
                const TSharedPtr<FJsonObject>* RuntimeStatic = nullptr;
                if ((*Overrides)->TryGetObjectField(TEXT("StaticParametersRuntime"), RuntimeStatic) && RuntimeStatic != nullptr)
                {
                    bool bHasMaterialLayers = false;
                    (*RuntimeStatic)->TryGetBoolField(TEXT("bHasMaterialLayers"), bHasMaterialLayers);
                    const TSharedPtr<FJsonObject>* MaterialLayers = nullptr;
                    if (bHasMaterialLayers
                        || ((*RuntimeStatic)->TryGetObjectField(TEXT("MaterialLayers"), MaterialLayers)
                            && MaterialLayers != nullptr && HasMaterialLayers(*MaterialLayers)))
                    {
                        UnsupportedOverridePointers.Add(TEXT("/semantics/overrides/StaticParametersRuntime/MaterialLayers"));
                    }
                }
                const TSharedPtr<FJsonObject>* EditorStatic = nullptr;
                if ((*Overrides)->TryGetObjectField(TEXT("EditorStaticParameters"), EditorStatic) && EditorStatic != nullptr)
                {
                    const TSharedPtr<FJsonObject>* MaterialLayers = nullptr;
                    if ((*EditorStatic)->TryGetObjectField(TEXT("MaterialLayers"), MaterialLayers)
                        && MaterialLayers != nullptr && HasMaterialLayers(*MaterialLayers))
                    {
                        UnsupportedOverridePointers.Add(TEXT("/semantics/overrides/EditorStaticParameters/MaterialLayers"));
                    }
                    AddNonEmptyArray(*EditorStatic, TEXT("TerrainLayerWeightParameters"),
                        TEXT("/semantics/overrides/EditorStaticParameters/TerrainLayerWeightParameters"));
                }
                const TSharedPtr<FJsonObject>* Lightmass = nullptr;
                if ((*Overrides)->TryGetObjectField(TEXT("LightmassSettings"), Lightmass) && Lightmass != nullptr)
                {
                    for (const TCHAR* Field : { TEXT("bOverrideCastShadowAsMasked"), TEXT("bOverrideDiffuseBoost"),
                                                TEXT("bOverrideEmissiveBoost"), TEXT("bOverrideExportResolutionScale") })
                    {
                        bool bEnabled = false;
                        if ((*Lightmass)->TryGetBoolField(Field, bEnabled) && bEnabled)
                        {
                            UnsupportedOverridePointers.Add(TEXT("/semantics/overrides/LightmassSettings"));
                            break;
                        }
                    }
                }
            }
            UnsupportedOverridePointers.Sort();
            UnsupportedOverridePointers.SetNum(Algo::Unique(UnsupportedOverridePointers));

            const TSharedRef<FJsonObject> CreateOperands = MakeShared<FJsonObject>();
            CreateOperands->SetStringField(TEXT("assetClass"), TEXT("MaterialInstanceConstant"));
            CreateOperands->SetStringField(TEXT("sourcePackageName"), Context.AssetData.PackageName.ToString());
            CreateOperands->SetStringField(TEXT("sourceObjectPath"), AssetPath);
            CreateOperands->SetStringField(TEXT("objectName"), Context.AssetData.AssetName.ToString());
            AddExecutable(Operation(
                Context,
                TEXT("op:material-instance:create"),
                TEXT("editor.materialInstance.create"),
                TEXT("declare"),
                TargetId,
                {},
                CreateOperands,
                { TEXT("/asset") },
                TEXT("structure"),
                TEXT("executable"),
                TEXT("exact"),
                TEXT("material-instance.create.v1"),
                1.0));

            const TSharedRef<FJsonObject> ApplyOperands = MakeShared<FJsonObject>();
            ApplyOperands->SetStringField(TEXT("parentSymbolId"), TEXT("symbol:parent"));
            ApplyOperands->SetStringField(TEXT("representation"), Representation);
            ApplyOperands->SetStringField(TEXT("overridesPointer"), TEXT("/semantics/overrides"));
            const TSharedRef<FJsonObject> ApplyOperation = Operation(
                Context,
                TEXT("op:material-instance:apply-overrides"),
                TEXT("editor.materialInstance.applyOverrides"),
                TEXT("configure"),
                TargetId,
                { TEXT("op:material-instance:create") },
                ApplyOperands,
                { TEXT("/semantics/overrides"), TEXT("/semantics/parent") },
                TEXT("defaults"),
                ParentPath.IsEmpty() || !UnsupportedOverridePointers.IsEmpty() ? TEXT("blocked") : TEXT("executable"),
                ParentPath.IsEmpty() || !UnsupportedOverridePointers.IsEmpty() ? TEXT("unsupported") : TEXT("exact"),
                TEXT("material-instance.overrides.v1"),
                ParentPath.IsEmpty() || !UnsupportedOverridePointers.IsEmpty() ? 0.0 : 1.0,
                ParentPath.IsEmpty() ? TEXT("unresolvedMaterialParent")
                    : (!UnsupportedOverridePointers.IsEmpty() ? TEXT("unsupportedMaterialInstanceOverrides") : FString()));
            if (ParentPath.IsEmpty())
            {
                AddBlocked(
                    ApplyOperation,
                    TEXT("unresolvedMaterialParent"),
                    TEXT("blocksReconstruction"),
                    { TEXT("/semantics/parent") });
            }
            else if (!UnsupportedOverridePointers.IsEmpty())
            {
                AddBlocked(
                    ApplyOperation,
                    TEXT("unsupportedMaterialInstanceOverrides"),
                    TEXT("blocksReconstruction"),
                    UnsupportedOverridePointers);
            }
            else
            {
                AddExecutable(ApplyOperation);
            }

            const TSharedRef<FJsonObject> SaveOperands = MakeShared<FJsonObject>();
            SaveOperands->SetStringField(TEXT("assetFormat"), TEXT("uasset"));
            AddExecutable(Operation(
                Context,
                TEXT("op:material-instance:save"),
                TEXT("editor.asset.save"),
                TEXT("verify"),
                TargetId,
                { TEXT("op:material-instance:apply-overrides") },
                SaveOperands,
                { TEXT("/asset/sourceFile") },
                TEXT("structure"),
                TEXT("executable"),
                TEXT("exact"),
                TEXT("editor.asset.save.v1"),
                1.0));
        }
        else
        {
            const TSharedRef<FJsonObject> Operands = MakeShared<FJsonObject>();
            Operands->SetStringField(TEXT("exporter"), ExporterName);
            Operands->SetStringField(TEXT("assetKind"), Kind);
            Operands->SetBoolField(TEXT("hasDomainSemantics"), bHasDomainSemantics);
            const TSharedRef<FJsonObject> Value = Operation(
                Context,
                TEXT("op:asset-builder"),
                TEXT("cpp.asset.unsupported"),
                TEXT("configure"),
                TargetId,
                {},
                Operands,
                { TEXT("/semantics") },
                TEXT("behavior"),
                TEXT("blocked"),
                TEXT("unsupported"),
                TEXT("editor.asset-builder.v1"),
                0.0,
                TEXT("assetBuilderBackendUnavailable"));
            AddBlocked(
                Value,
                TEXT("assetBuilderBackendUnavailable"),
                TEXT("blocksReconstruction"),
                { TEXT("/semantics") });
        }

        for (int32 Index = 0; Index < Omissions.Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> Omission = Omissions[Index].IsValid()
                ? Omissions[Index]->AsObject()
                : nullptr;
            FString Code;
            if (!Omission.IsValid() || !Omission->TryGetStringField(TEXT("code"), Code)) continue;
            const FString Pointer = FString::Printf(TEXT("/omissions/%d"), Index);
            JsonLosses.Add(MakeShared<FJsonValueObject>(Loss(
                TEXT("loss:omission:") + FString::FromInt(Index),
                Code,
                TEXT("degradesFidelity"),
                { Pointer })));
        }
        if (Context.bPackageDirtyAfterLoad)
        {
            JsonLosses.Add(MakeShared<FJsonValueObject>(Loss(
                TEXT("loss:post-load-transform"),
                TEXT("enginePostLoadTransform"),
                TEXT("degradesFidelity"),
                { TEXT("/asset/sourceHash") })));
        }

        const int32 TotalCount = ExactCount + InferredCount + UnsupportedCount;
        const FString Readiness = UnsupportedCount == 0
            ? TEXT("ready")
            : ExactCount + InferredCount > 0 ? TEXT("partial") : TEXT("blocked");
        BlockerRefs.Sort();
        JsonOperations.Sort([](const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right)
        {
            return Left->AsObject()->GetStringField(TEXT("id")).Compare(
                Right->AsObject()->GetStringField(TEXT("id")),
                ESearchCase::CaseSensitive) < 0;
        });

        const TSharedRef<FJsonObject> Target = MakeShared<FJsonObject>();
        Target->SetStringField(TEXT("id"), TargetId);
        Target->SetStringField(TEXT("target"), bBlueprint ? TEXT("nativeClassCpp") : TEXT("editorAssetBuilderCpp"));
        Target->SetStringField(TEXT("backend"), bBlueprint ? TEXT("ueCpp") : TEXT("unrealEditorCpp"));
        Target->SetNumberField(TEXT("backendVersion"), 1);
        Target->SetStringField(TEXT("fidelity"), TEXT("semanticEquivalent"));
        Target->SetStringField(TEXT("status"), Readiness);
        Target->SetStringField(TEXT("writePolicy"), TEXT("replaceGenerated"));
        Target->SetArrayField(TEXT("blockerRefs"), Strings(BlockerRefs));
        IR->SetArrayField(TEXT("targets"), { MakeShared<FJsonValueObject>(Target) });
        IR->SetArrayField(TEXT("symbols"), JsonSymbols);
        IR->SetArrayField(TEXT("operations"), JsonOperations);
        IR->SetArrayField(TEXT("losses"), JsonLosses);

        const TSharedRef<FJsonObject> Coverage = MakeShared<FJsonObject>();
        Coverage->SetStringField(TEXT("readiness"), Readiness);
        Coverage->SetNumberField(TEXT("totalOperationCount"), TotalCount);
        Coverage->SetNumberField(TEXT("exactOperationCount"), ExactCount);
        Coverage->SetNumberField(TEXT("inferredOperationCount"), InferredCount);
        Coverage->SetNumberField(TEXT("unsupportedOperationCount"), UnsupportedCount);
        Coverage->SetNumberField(
            TEXT("exactRatio"),
            TotalCount > 0 ? static_cast<double>(ExactCount) / TotalCount : 0.0);
        IR->SetObjectField(TEXT("coverage"), Coverage);

        const TSharedRef<FJsonObject> Execution = MakeShared<FJsonObject>();
        Execution->SetBoolField(TEXT("fullyExecutable"), UnsupportedCount == 0);
        Execution->SetNumberField(TEXT("operationCount"), TotalCount);
        Execution->SetNumberField(TEXT("executableOperationCount"), ExactCount + InferredCount);
        Execution->SetNumberField(TEXT("blockedOperationCount"), UnsupportedCount);
        IR->SetObjectField(TEXT("execution"), Execution);

        TArray<TSharedPtr<FJsonValue>> Verification;
        auto AddVerification = [&](const TCHAR* Id, const TCHAR* KindName)
        {
            const TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
            Value->SetStringField(TEXT("id"), Id);
            Value->SetStringField(TEXT("kind"), KindName);
            Value->SetStringField(TEXT("targetId"), TargetId);
            Value->SetBoolField(TEXT("required"), true);
            Verification.Add(MakeShared<FJsonValueObject>(Value));
        };
        AddVerification(TEXT("verify:compile"), TEXT("ubtCompile"));
        AddVerification(TEXT("verify:reflection"), TEXT("reflectionCompare"));
        AddVerification(TEXT("verify:semantic-diff"), TEXT("semanticReexportDiff"));
        IR->SetArrayField(TEXT("verification"), Verification);
        return IR;
    }
}
