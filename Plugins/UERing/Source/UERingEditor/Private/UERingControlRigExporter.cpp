#include "UERingControlRigExporter.h"

#include "Algo/Unique.h"
#include "ControlRigBlueprintLegacy.h"
#include "Engine/Blueprint.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMLink.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMModel/Nodes/RigVMTemplateNode.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "Rigs/RigHierarchy.h"
#include "UERingBlueprintExporter.h"
#include "UERingPropertySerializer.h"
#include "UERingSemanticUtils.h"
#include "UObject/UnrealType.h"

namespace UERingControlRigExporter
{
    TArray<TSharedPtr<FJsonValue>> Strings(TArray<FString> Values)
    {
        Values.Sort();
        Values.SetNum(Algo::Unique(Values));
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Values.Num());
        for (const FString& Value : Values)
        {
            Result.Add(MakeShared<FJsonValueString>(Value));
        }
        return Result;
    }

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
        const FQuat Rotation = Value.GetRotation();
        const TSharedRef<FJsonObject> JsonRotation = MakeShared<FJsonObject>();
        JsonRotation->SetNumberField(TEXT("x"), Rotation.X);
        JsonRotation->SetNumberField(TEXT("y"), Rotation.Y);
        JsonRotation->SetNumberField(TEXT("z"), Rotation.Z);
        JsonRotation->SetNumberField(TEXT("w"), Rotation.W);
        Json->SetObjectField(TEXT("rotation"), JsonRotation);
        Json->SetObjectField(TEXT("scale"), Vector(Value.GetScale3D()));
        return Json;
    }

    TSharedRef<FJsonObject> Key(const FRigElementKey& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Value.Name.ToString());
        Json->SetStringField(TEXT("type"), UEnum::GetValueAsString(Value.Type));
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> StructProperties(const UScriptStruct& Struct, const void* Data)
    {
        TArray<TSharedPtr<FJsonValue>> Properties;
        for (TFieldIterator<FProperty> It(&Struct, EFieldIterationFlags::IncludeSuper); It; ++It)
        {
            if (UERingPropertySerializer::ShouldExportProperty(**It)
                && !UERingPropertySerializer::IsPrivateName((*It)->GetName()))
            {
                Properties.Add(MakeShared<FJsonValueObject>(
                    UERingPropertySerializer::SerializeProperty(**It, Data)));
            }
        }
        Properties.Sort([](const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right)
        {
            return Left->AsObject()->GetStringField(TEXT("name"))
                < Right->AsObject()->GetStringField(TEXT("name"));
        });
        return Properties;
    }

    TSharedRef<FJsonObject> RigVMPin(const URigVMPin& Pin)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("path"), Pin.GetPinPath(true));
        Json->SetStringField(TEXT("name"), Pin.GetName());
        Json->SetStringField(TEXT("displayName"), Pin.GetDisplayName().ToString());
        Json->SetStringField(TEXT("direction"), UEnum::GetValueAsString(Pin.GetDirection()));
        Json->SetStringField(TEXT("cppType"), Pin.GetCPPType());
        if (const UObject* TypeObject = Pin.GetCPPTypeObject())
        {
            Json->SetStringField(TEXT("cppTypeObject"), TypeObject->GetPathName());
        }
        Json->SetStringField(TEXT("defaultValue"), Pin.GetDefaultValue());
        Json->SetStringField(TEXT("boundVariablePath"), Pin.GetBoundVariablePath());
        Json->SetBoolField(TEXT("expanded"), Pin.IsExpanded());
        Json->SetBoolField(TEXT("constant"), Pin.IsDefinedAsConstant());
        Json->SetBoolField(TEXT("orphaned"), Pin.IsOrphanPin());
        Json->SetBoolField(TEXT("programmatic"), Pin.IsProgrammaticPin());
        Json->SetBoolField(TEXT("trait"), Pin.IsTraitPin());
        if (const URigVMPin* Parent = Pin.GetParentPin())
        {
            Json->SetStringField(TEXT("parentPinPath"), Parent->GetPinPath(true));
        }
        TArray<FString> SubPinPaths;
        for (const URigVMPin* SubPin : Pin.GetSubPins())
        {
            if (SubPin != nullptr) SubPinPaths.Add(SubPin->GetPinPath(true));
        }
        Json->SetArrayField(TEXT("subPinPaths"), Strings(MoveTemp(SubPinPaths)));
        return Json;
    }

    TSharedRef<FJsonObject> RigVMNode(const URigVMNode& Node)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("path"), Node.GetNodePath(true));
        Json->SetStringField(TEXT("name"), Node.GetName());
        Json->SetStringField(TEXT("class"), Node.GetClass()->GetPathName());
        Json->SetStringField(TEXT("title"), Node.GetNodeTitle());
        Json->SetStringField(TEXT("subTitle"), Node.GetNodeSubTitle());
        Json->SetBoolField(TEXT("injected"), Node.IsInjected());
        Json->SetBoolField(TEXT("visibleInUi"), Node.IsVisibleInUI());
        Json->SetBoolField(TEXT("pure"), Node.IsPure());
        const FVector2D Position = Node.GetPosition();
        const TSharedRef<FJsonObject> JsonPosition = MakeShared<FJsonObject>();
        JsonPosition->SetNumberField(TEXT("x"), Position.X);
        JsonPosition->SetNumberField(TEXT("y"), Position.Y);
        Json->SetObjectField(TEXT("position"), JsonPosition);
        if (const URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(&Node))
        {
            if (const UScriptStruct* ScriptStruct = UnitNode->GetScriptStruct())
            {
                Json->SetStringField(TEXT("scriptStruct"), ScriptStruct->GetPathName());
            }
        }
        else if (const URigVMTemplateNode* TemplateNode = Cast<URigVMTemplateNode>(&Node))
        {
            if (const UScriptStruct* ScriptStruct = TemplateNode->GetScriptStruct())
            {
                Json->SetStringField(TEXT("scriptStruct"), ScriptStruct->GetPathName());
            }
        }

        TArray<URigVMPin*> Pins = Node.GetAllPinsRecursively();
        Pins.Sort([](const URigVMPin& Left, const URigVMPin& Right)
        {
            return Left.GetPinPath(true) < Right.GetPinPath(true);
        });
        TArray<TSharedPtr<FJsonValue>> JsonPins;
        JsonPins.Reserve(Pins.Num());
        for (const URigVMPin* Pin : Pins)
        {
            if (Pin != nullptr) JsonPins.Add(MakeShared<FJsonValueObject>(RigVMPin(*Pin)));
        }
        Json->SetArrayField(TEXT("pins"), MoveTemp(JsonPins));
        return Json;
    }

    TSharedRef<FJsonObject> RigVMGraph(
        const URigVMGraph& Graph,
        int32& OutNodeCount,
        int32& OutPinCount,
        int32& OutLinkCount)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("path"), Graph.GetNodePath());
        Json->SetStringField(TEXT("name"), Graph.GetName());
        Json->SetStringField(TEXT("graphName"), Graph.GetGraphName());
        Json->SetNumberField(TEXT("depth"), Graph.GetGraphDepth());
        if (const URigVMGraph* Parent = Graph.GetParentGraph())
        {
            Json->SetStringField(TEXT("parentGraphPath"), Parent->GetNodePath());
        }
        TArray<FString> EventNames;
        for (const FName EventName : Graph.GetEventNames()) EventNames.Add(EventName.ToString());
        Json->SetArrayField(TEXT("eventNames"), Strings(MoveTemp(EventNames)));

        TArray<URigVMNode*> Nodes = Graph.GetNodes();
        Nodes.Sort([](const URigVMNode& Left, const URigVMNode& Right)
        {
            return Left.GetNodePath(true) < Right.GetNodePath(true);
        });
        TArray<TSharedPtr<FJsonValue>> JsonNodes;
        JsonNodes.Reserve(Nodes.Num());
        for (const URigVMNode* Node : Nodes)
        {
            if (Node == nullptr) continue;
            JsonNodes.Add(MakeShared<FJsonValueObject>(RigVMNode(*Node)));
            ++OutNodeCount;
            OutPinCount += Node->GetAllPinsRecursively().Num();
        }
        Json->SetArrayField(TEXT("nodes"), MoveTemp(JsonNodes));

        TArray<URigVMLink*> Links = Graph.GetLinks();
        Links.Sort([](const URigVMLink& Left, const URigVMLink& Right)
        {
            return Left.GetPinPathRepresentation() < Right.GetPinPathRepresentation();
        });
        TArray<TSharedPtr<FJsonValue>> JsonLinks;
        JsonLinks.Reserve(Links.Num());
        for (const URigVMLink* Link : Links)
        {
            if (Link == nullptr) continue;
            const TSharedRef<FJsonObject> JsonLink = MakeShared<FJsonObject>();
            JsonLink->SetStringField(TEXT("sourcePinPath"), Link->GetSourcePinPath());
            JsonLink->SetStringField(TEXT("targetPinPath"), Link->GetTargetPinPath());
            JsonLinks.Add(MakeShared<FJsonValueObject>(JsonLink));
            ++OutLinkCount;
        }
        Json->SetArrayField(TEXT("links"), MoveTemp(JsonLinks));
        return Json;
    }
}

FName FUERingControlRigExporter::GetName() const
{
    return TEXT("ControlRig");
}

bool FUERingControlRigExporter::CanExport(const FAssetData& AssetData) const
{
    const FString ClassName = AssetData.AssetClassPath.GetAssetName().ToString();
    return AssetData.IsInstanceOf(UBlueprint::StaticClass())
        && (ClassName.Contains(TEXT("ControlRigBlueprint"))
            || ClassName.Contains(TEXT("RigVMBlueprint")));
}

bool FUERingControlRigExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    UBlueprint* Blueprint = Cast<UBlueprint>(Context.Asset.Get());
    if (Blueprint == nullptr)
    {
        OutError = TEXT("The loaded object is not a Control Rig Blueprint.");
        return false;
    }

    FUERingBlueprintExporter BlueprintExporter;
    if (!BlueprintExporter.BuildPayload(Context, OutPayload, OutError))
    {
        return false;
    }

    OutPayload.Semantics->SetStringField(TEXT("kind"), TEXT("ControlRig"));
    OutPayload.Semantics->SetStringField(TEXT("baseKind"), TEXT("Blueprint"));
    OutPayload.Semantics->SetStringField(TEXT("representation"), TEXT("control-rig-blueprint-mirror-rigvm-model-and-hierarchy-v3"));
    UERingSemanticUtils::SetSelectedProperties(
        *Blueprint,
        {
            TEXT("PreviewSkeletalMesh"),
            TEXT("ShapeLibraries"),
            TEXT("VMRuntimeSettings"),
            TEXT("HierarchySettings"),
            TEXT("RigModuleSettings"),
            TEXT("PublicFunctions")
        },
        OutPayload.Semantics,
        TEXT("rigProperties"));

    if (const UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>(Blueprint))
    {
        using namespace UERingControlRigExporter;
        TArray<URigVMGraph*> Models = ControlRigBlueprint->GetAllModels();
        Models.RemoveAll([](const URigVMGraph* Graph)
        {
            return Graph == nullptr || Graph->GetName() == TEXT("RigVMFunctionLibrary");
        });
        Models.Sort([](const URigVMGraph& Left, const URigVMGraph& Right)
        {
            return Left.GetNodePath() < Right.GetNodePath();
        });
        int32 RigVMNodeCount = 0;
        int32 RigVMPinCount = 0;
        int32 RigVMLinkCount = 0;
        TArray<TSharedPtr<FJsonValue>> JsonModels;
        JsonModels.Reserve(Models.Num());
        for (const URigVMGraph* Model : Models)
        {
            JsonModels.Add(MakeShared<FJsonValueObject>(
                RigVMGraph(*Model, RigVMNodeCount, RigVMPinCount, RigVMLinkCount)));
        }
        OutPayload.Semantics->SetNumberField(TEXT("rigVmModelGraphCount"), JsonModels.Num());
        OutPayload.Semantics->SetNumberField(TEXT("rigVmModelNodeCount"), RigVMNodeCount);
        OutPayload.Semantics->SetNumberField(TEXT("rigVmModelPinCount"), RigVMPinCount);
        OutPayload.Semantics->SetNumberField(TEXT("rigVmModelLinkCount"), RigVMLinkCount);
        OutPayload.Semantics->SetArrayField(TEXT("rigVmModels"), MoveTemp(JsonModels));

        if (const URigHierarchy* Hierarchy = ControlRigBlueprint->GetHierarchy())
        {
            TArray<FRigElementKey> Keys = Hierarchy->GetAllKeys();
            Keys.Sort([](const FRigElementKey& Left, const FRigElementKey& Right)
            {
                const FString LeftId = UEnum::GetValueAsString(Left.Type) + TEXT(":") + Left.Name.ToString();
                const FString RightId = UEnum::GetValueAsString(Right.Type) + TEXT(":") + Right.Name.ToString();
                return LeftId < RightId;
            });
            TArray<TSharedPtr<FJsonValue>> Elements;
            Elements.Reserve(Keys.Num());
            for (const FRigElementKey& ElementKey : Keys)
            {
                const TSharedRef<FJsonObject> Element = Key(ElementKey);
                TArray<TSharedPtr<FJsonValue>> Parents;
                for (const FRigElementKey& Parent : Hierarchy->GetParents(ElementKey, false))
                {
                    Parents.Add(MakeShared<FJsonValueObject>(Key(Parent)));
                }
                Element->SetArrayField(TEXT("parents"), MoveTemp(Parents));
                Element->SetObjectField(TEXT("initialLocalTransform"), Transform(Hierarchy->GetLocalTransform(ElementKey, true)));
                Element->SetObjectField(TEXT("currentLocalTransform"), Transform(Hierarchy->GetLocalTransform(ElementKey, false)));
                if (const FRigControlElement* Control = Hierarchy->Find<FRigControlElement>(ElementKey))
                {
                    Element->SetArrayField(
                        TEXT("controlSettings"),
                        StructProperties(*FRigControlSettings::StaticStruct(), &Control->Settings));
                }
                Elements.Add(MakeShared<FJsonValueObject>(Element));
            }
            OutPayload.Semantics->SetNumberField(TEXT("hierarchyElementCount"), Elements.Num());
            OutPayload.Semantics->SetArrayField(TEXT("hierarchy"), MoveTemp(Elements));
        }
    }

    UERingSemanticUtils::AddOmission(
        OutPayload,
        TEXT("/semantics/compiledVM"),
        TEXT("derivedCompiledArtifact"),
        TEXT("Blueprint graph mirrors, authoritative RigVM model graphs, hierarchy elements, control settings, transforms, pins, defaults, and event topology are preserved; generated VM bytecode and editor caches are derived and intentionally omitted."),
        TEXT("noSemanticBehaviorLoss"));
    return true;
}
