#include "UERingBlueprintExporter.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Components/SceneComponent.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "Curves/RichCurve.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/TimelineTemplate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_Composite.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Timeline.h"
#include "K2Node_Variable.h"
#include "Misc/Paths.h"
#include "SourceCodeNavigation.h"
#include "UERingPropertySerializer.h"
#include "UERingPinTypeSerializer.h"
#include "UERingOwnedObjectSerializer.h"
#include "UERingSettings.h"
#include "UObject/UnrealType.h"

namespace UERingBlueprintExporter
{
    using FJsonObjectRef = TSharedRef<FJsonObject>;

    struct FGraphEntry
    {
        UEdGraph* Graph = nullptr;
        FString Type;
        FString Path;
        FString ParentGraphPath;
        FString OwnerNodeId;
    };

    FString BlueprintStatusToString(const EBlueprintStatus Status)
    {
        switch (Status)
        {
        case BS_Unknown: return TEXT("unknown");
        case BS_Dirty: return TEXT("dirty");
        case BS_Error: return TEXT("error");
        case BS_UpToDate: return TEXT("upToDate");
        case BS_BeingCreated: return TEXT("beingCreated");
        case BS_UpToDateWithWarnings: return TEXT("upToDateWithWarnings");
        default: return TEXT("unknown");
        }
    }

    TArray<TSharedPtr<FJsonValue>> StringArray(const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> JsonValues;
        JsonValues.Reserve(Values.Num());
        for (const FString& Value : Values)
        {
            JsonValues.Add(MakeShared<FJsonValueString>(Value));
        }
        return JsonValues;
    }

    FString NodeId(const UEdGraphNode& Node)
    {
        return (Node.NodeGuid.IsValid()
            ? Node.NodeGuid.ToString(EGuidFormats::DigitsWithHyphensLower)
            : TEXT("no-guid"))
            + TEXT("/") + Node.GetName();
    }

    FString GraphKind(const UEdGraph& Graph, const FString& Fallback)
    {
        if (const UEdGraphSchema* Schema = Graph.GetSchema())
        {
            FString SchemaName = Schema->GetClass()->GetName();
            if (SchemaName.StartsWith(TEXT("Animation")) && SchemaName.RemoveFromEnd(TEXT("Schema")))
            {
                return SchemaName;
            }
        }
        return Fallback;
    }

    TArray<FString> VariableFlags(const uint64 PropertyFlags)
    {
        TArray<FString> Flags;
        if ((PropertyFlags & CPF_Edit) != 0) Flags.Add(TEXT("InstanceEditable"));
        if ((PropertyFlags & CPF_BlueprintVisible) != 0) Flags.Add(TEXT("BlueprintVisible"));
        if ((PropertyFlags & CPF_BlueprintReadOnly) != 0) Flags.Add(TEXT("BlueprintReadOnly"));
        if ((PropertyFlags & CPF_ExposeOnSpawn) != 0) Flags.Add(TEXT("ExposeOnSpawn"));
        if ((PropertyFlags & CPF_SaveGame) != 0) Flags.Add(TEXT("SaveGame"));
        if ((PropertyFlags & CPF_Net) != 0) Flags.Add(TEXT("Replicated"));
        Flags.Sort();
        return Flags;
    }

    FJsonObjectRef SerializeVariable(
        const FBPVariableDescription& Variable,
        const FProperty* GeneratedProperty = nullptr,
        const UObject* DefaultObject = nullptr)
    {
        FJsonObjectRef Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Variable.VarName.ToString());
        if (Variable.VarGuid.IsValid())
        {
            Json->SetStringField(
                TEXT("guid"),
                Variable.VarGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
        }
        Json->SetObjectField(TEXT("type"), UERingPinTypeSerializer::Serialize(Variable.VarType));
        if (!Variable.FriendlyName.IsEmpty() && Variable.FriendlyName != Variable.VarName.ToString())
        {
            Json->SetStringField(TEXT("friendlyName"), Variable.FriendlyName);
        }
        if (GeneratedProperty != nullptr && DefaultObject != nullptr)
        {
            const FJsonObjectRef SerializedDefault = UERingPropertySerializer::SerializeProperty(
                *GeneratedProperty,
                DefaultObject,
                DefaultObject);
            Json->SetField(TEXT("defaultValue"), SerializedDefault->Values.FindChecked(TEXT("value")));
        }
        else if (!Variable.DefaultValue.IsEmpty())
        {
            Json->SetStringField(TEXT("defaultValue"), Variable.DefaultValue);
        }
        const FString Category = Variable.Category.ToString();
        if (!Category.IsEmpty())
        {
            Json->SetStringField(TEXT("category"), Category);
        }
        const TArray<FString> Flags = VariableFlags(Variable.PropertyFlags);
        if (!Flags.IsEmpty())
        {
            Json->SetArrayField(TEXT("flags"), StringArray(Flags));
        }
        if (!Variable.RepNotifyFunc.IsNone())
        {
            Json->SetStringField(TEXT("repNotify"), Variable.RepNotifyFunc.ToString());
        }
        if ((Variable.PropertyFlags & CPF_Net) != 0)
        {
            Json->SetStringField(
                TEXT("replicationCondition"),
                UEnum::GetValueAsString(Variable.ReplicationCondition.GetValue()));
        }

        TArray<FBPVariableMetaDataEntry> Metadata = Variable.MetaDataArray;
        Metadata.Sort([](const FBPVariableMetaDataEntry& Left, const FBPVariableMetaDataEntry& Right)
        {
            return Left.DataKey.LexicalLess(Right.DataKey);
        });
        FJsonObjectRef JsonMetadata = MakeShared<FJsonObject>();
        for (const FBPVariableMetaDataEntry& Entry : Metadata)
        {
            const FString Key = Entry.DataKey.ToString();
            JsonMetadata->SetStringField(
                Key,
                UERingPropertySerializer::IsPrivateName(Key) ? TEXT("[REDACTED]") : Entry.DataValue);
        }
        if (!JsonMetadata->Values.IsEmpty())
        {
            Json->SetObjectField(TEXT("metadata"), JsonMetadata);
        }
        return Json;
    }

    TArray<FString> FunctionFlags(const int32 Flags)
    {
        TArray<FString> Values;
        if ((Flags & FUNC_Public) != 0) Values.Add(TEXT("public"));
        if ((Flags & FUNC_Protected) != 0) Values.Add(TEXT("protected"));
        if ((Flags & FUNC_Private) != 0) Values.Add(TEXT("private"));
        if ((Flags & FUNC_Static) != 0) Values.Add(TEXT("static"));
        if ((Flags & FUNC_Const) != 0) Values.Add(TEXT("const"));
        if ((Flags & FUNC_BlueprintPure) != 0) Values.Add(TEXT("blueprintPure"));
        if ((Flags & FUNC_BlueprintCallable) != 0) Values.Add(TEXT("blueprintCallable"));
        if ((Flags & FUNC_BlueprintEvent) != 0) Values.Add(TEXT("blueprintEvent"));
        if ((Flags & FUNC_Event) != 0) Values.Add(TEXT("event"));
        if ((Flags & FUNC_Net) != 0) Values.Add(TEXT("net"));
        if ((Flags & FUNC_NetServer) != 0) Values.Add(TEXT("server"));
        if ((Flags & FUNC_NetClient) != 0) Values.Add(TEXT("client"));
        if ((Flags & FUNC_NetMulticast) != 0) Values.Add(TEXT("netMulticast"));
        if ((Flags & FUNC_NetReliable) != 0) Values.Add(TEXT("reliable"));
        if ((Flags & FUNC_BlueprintAuthorityOnly) != 0) Values.Add(TEXT("authorityOnly"));
        if ((Flags & FUNC_BlueprintCosmetic) != 0) Values.Add(TEXT("cosmetic"));
        Values.Sort();
        return Values;
    }

    FJsonObjectRef SerializeFunctionMetadata(const FKismetUserDeclaredFunctionMetadata& Metadata)
    {
        FJsonObjectRef Json = MakeShared<FJsonObject>();
        auto AddText = [&Json](const TCHAR* Name, const FText& Value)
        {
            if (!Value.IsEmpty()) Json->SetStringField(Name, Value.ToString());
        };
        AddText(TEXT("tooltip"), Metadata.ToolTip);
        AddText(TEXT("category"), Metadata.Category);
        AddText(TEXT("keywords"), Metadata.Keywords);
        AddText(TEXT("compactNodeTitle"), Metadata.CompactNodeTitle);
        if (!Metadata.InstanceTitleColor.Equals(FLinearColor::White))
        {
            Json->SetStringField(TEXT("instanceTitleColor"), Metadata.InstanceTitleColor.ToString());
        }
        if (Metadata.bIsDeprecated) Json->SetBoolField(TEXT("deprecated"), true);
        if (!Metadata.DeprecationMessage.IsEmpty())
        {
            Json->SetStringField(TEXT("deprecationMessage"), Metadata.DeprecationMessage);
        }
        if (Metadata.bCallInEditor) Json->SetBoolField(TEXT("callInEditor"), true);
        if (Metadata.bThreadSafe) Json->SetBoolField(TEXT("threadSafe"), true);
        if (Metadata.bIsUnsafeDuringActorConstruction)
        {
            Json->SetBoolField(TEXT("unsafeDuringActorConstruction"), true);
        }
        if (Metadata.HasLatentFunctions != INDEX_NONE)
        {
            Json->SetBoolField(TEXT("hasLatentFunctions"), Metadata.HasLatentFunctions > 0);
        }
        TArray<TPair<FName, FString>> Entries;
        for (const TPair<FName, FString>& Entry : Metadata.GetMetaDataMap())
        {
            Entries.Add(Entry);
        }
        Entries.Sort([](const auto& Left, const auto& Right) { return Left.Key.LexicalLess(Right.Key); });
        FJsonObjectRef Extra = MakeShared<FJsonObject>();
        for (const auto& Entry : Entries)
        {
            const FString Key = Entry.Key.ToString();
            Extra->SetStringField(
                Key,
                UERingPropertySerializer::IsPrivateName(Key) ? TEXT("[REDACTED]") : Entry.Value);
        }
        if (!Extra->Values.IsEmpty()) Json->SetObjectField(TEXT("extra"), Extra);
        return Json;
    }

    FJsonObjectRef SerializePin(
        const UEdGraphPin& Pin,
        const FString& CompactId,
        const TMap<const UEdGraphPin*, FString>& PinIds)
    {
        FJsonObjectRef JsonPin = MakeShared<FJsonObject>();
        JsonPin->SetStringField(TEXT("id"), CompactId);
        JsonPin->SetStringField(TEXT("name"), Pin.PinName.ToString());
        JsonPin->SetStringField(TEXT("direction"), Pin.Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        JsonPin->SetObjectField(TEXT("type"), UERingPinTypeSerializer::Serialize(Pin.PinType));
        if (Pin.PersistentGuid.IsValid())
        {
            JsonPin->SetStringField(
                TEXT("persistentGuid"),
                Pin.PersistentGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
        }
        if (const FString* ParentId = PinIds.Find(Pin.ParentPin))
        {
            JsonPin->SetStringField(TEXT("parentPinId"), *ParentId);
        }
        TArray<FString> SubPinIds;
        for (const UEdGraphPin* SubPin : Pin.SubPins)
        {
            if (const FString* SubPinId = PinIds.Find(SubPin)) SubPinIds.Add(*SubPinId);
        }
        if (!SubPinIds.IsEmpty())
        {
            JsonPin->SetArrayField(TEXT("subPinIds"), StringArray(SubPinIds));
        }
        if (Pin.Direction == EGPD_Input && Pin.LinkedTo.IsEmpty())
        {
            if (!Pin.DefaultValue.IsEmpty())
            {
                JsonPin->SetStringField(TEXT("defaultValue"), Pin.DefaultValue);
            }
            if (Pin.DefaultObject != nullptr)
            {
                JsonPin->SetStringField(TEXT("defaultObject"), Pin.DefaultObject->GetPathName());
            }
            if (!Pin.DefaultTextValue.IsEmpty())
            {
                JsonPin->SetStringField(TEXT("defaultText"), Pin.DefaultTextValue.ToString());
            }
        }
        if (Pin.bHidden)
        {
            JsonPin->SetBoolField(TEXT("hidden"), true);
        }
        if (Pin.bAdvancedView)
        {
            JsonPin->SetBoolField(TEXT("advanced"), true);
        }
        if (Pin.bOrphanedPin) JsonPin->SetBoolField(TEXT("orphaned"), true);
        if (Pin.bNotConnectable) JsonPin->SetBoolField(TEXT("notConnectable"), true);
        if (Pin.bDefaultValueIsIgnored) JsonPin->SetBoolField(TEXT("defaultValueIgnored"), true);
        if (Pin.bDefaultValueIsReadOnly) JsonPin->SetBoolField(TEXT("defaultValueReadOnly"), true);
        if (!Pin.AutogeneratedDefaultValue.IsEmpty()
            && Pin.AutogeneratedDefaultValue != Pin.DefaultValue)
        {
            JsonPin->SetStringField(TEXT("autogeneratedDefaultValue"), Pin.AutogeneratedDefaultValue);
        }
        return JsonPin;
    }

    FJsonObjectRef SerializeNode(
        const UEdGraphNode& Node,
        const UBlueprint& Blueprint,
        const TMap<const UEdGraphPin*, FString>& PinIds)
    {
        FJsonObjectRef JsonNode = MakeShared<FJsonObject>();
        JsonNode->SetStringField(TEXT("id"), NodeId(Node));
        JsonNode->SetStringField(TEXT("class"), Node.GetClass()->GetName());
        JsonNode->SetStringField(TEXT("title"), Node.GetNodeTitle(ENodeTitleType::FullTitle).ToString());
        if (!Node.NodeComment.IsEmpty())
        {
            JsonNode->SetStringField(TEXT("comment"), Node.NodeComment);
        }
        if (Node.GetDesiredEnabledState() != ENodeEnabledState::Enabled)
        {
            JsonNode->SetStringField(TEXT("enabledState"), LexToString(Node.GetDesiredEnabledState()));
        }
        if (GetDefault<UUERingSettings>()->bIncludeNodePositions)
        {
            FJsonObjectRef Position = MakeShared<FJsonObject>();
            Position->SetNumberField(TEXT("x"), Node.NodePosX);
            Position->SetNumberField(TEXT("y"), Node.NodePosY);
            JsonNode->SetObjectField(TEXT("position"), Position);
        }

        FJsonObjectRef MemberReference = MakeShared<FJsonObject>();
        bool bHasMemberReference = false;
        if (const UK2Node_BaseMCDelegate* DelegateNode = Cast<UK2Node_BaseMCDelegate>(&Node))
        {
            const FProperty* Property = DelegateNode->GetProperty();
            const UClass* OwnerClass = Property != nullptr
                ? Property->GetOwnerClass()
                : DelegateNode->DelegateReference.GetMemberParentClass();
            const UFunction* Signature = DelegateNode->GetDelegateSignature();
            MemberReference->SetStringField(TEXT("kind"), TEXT("delegate"));
            MemberReference->SetStringField(TEXT("operation"), Node.GetClass()->GetName());
            MemberReference->SetStringField(TEXT("name"), DelegateNode->GetPropertyName().ToString());
            MemberReference->SetStringField(
                TEXT("owner"), OwnerClass != nullptr ? OwnerClass->GetPathName() : FString());
            MemberReference->SetStringField(
                TEXT("signature"), Signature != nullptr ? Signature->GetPathName() : FString());
            bHasMemberReference = true;
        }
        else if (const UK2Node_CreateDelegate* CreateDelegate = Cast<UK2Node_CreateDelegate>(&Node))
        {
            const UClass* ScopeClass = CreateDelegate->GetScopeClass();
            const UFunction* Signature = CreateDelegate->GetDelegateSignature();
            MemberReference->SetStringField(TEXT("kind"), TEXT("delegateFunction"));
            MemberReference->SetStringField(TEXT("operation"), TEXT("create"));
            MemberReference->SetStringField(TEXT("name"), CreateDelegate->GetFunctionName().ToString());
            MemberReference->SetStringField(
                TEXT("owner"), ScopeClass != nullptr ? ScopeClass->GetPathName() : FString());
            MemberReference->SetStringField(
                TEXT("signature"), Signature != nullptr ? Signature->GetPathName() : FString());
            bHasMemberReference = true;
        }
        else if (const UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(&Node))
        {
            if (const UFunction* Function = CallFunction->GetTargetFunction())
            {
                MemberReference->SetStringField(TEXT("kind"), TEXT("function"));
                MemberReference->SetStringField(TEXT("owner"), Function->GetOwnerClass()->GetPathName());
                MemberReference->SetStringField(TEXT("name"), Function->GetName());
                bHasMemberReference = true;
            }
        }
        else if (const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(&Node))
        {
            if (const FProperty* Property = VariableNode->GetPropertyForVariable())
            {
                MemberReference->SetStringField(TEXT("kind"), TEXT("property"));
                MemberReference->SetStringField(
                    TEXT("owner"),
                    Property->GetOwnerStruct() != nullptr ? Property->GetOwnerStruct()->GetPathName() : FString());
                MemberReference->SetStringField(TEXT("name"), Property->GetName());
                bHasMemberReference = true;
            }
        }
        else if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(&Node))
        {
            MemberReference->SetStringField(TEXT("kind"), TEXT("customEvent"));
            MemberReference->SetStringField(TEXT("name"), CustomEvent->CustomFunctionName.ToString());
            const TArray<FString> Flags = FunctionFlags(
                CustomEvent->FunctionFlags | CustomEvent->GetNetFlags());
            if (!Flags.IsEmpty()) MemberReference->SetArrayField(TEXT("functionFlags"), StringArray(Flags));
            if (CustomEvent->bCallInEditor) MemberReference->SetBoolField(TEXT("callInEditor"), true);
            if (CustomEvent->bIsDeprecated) MemberReference->SetBoolField(TEXT("deprecated"), true);
            if (!CustomEvent->DeprecationMessage.IsEmpty())
            {
                MemberReference->SetStringField(TEXT("deprecationMessage"), CustomEvent->DeprecationMessage);
            }
            if (CustomEvent->IsOverride()) MemberReference->SetBoolField(TEXT("override"), true);
            const FJsonObjectRef Metadata = SerializeFunctionMetadata(
                const_cast<UK2Node_CustomEvent*>(CustomEvent)->GetUserDefinedMetaData());
            if (!Metadata->Values.IsEmpty()) MemberReference->SetObjectField(TEXT("metadata"), Metadata);
            bHasMemberReference = true;
        }
        else if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(&Node))
        {
            MemberReference->SetStringField(TEXT("kind"), TEXT("event"));
            MemberReference->SetStringField(TEXT("name"), EventNode->EventReference.GetMemberName().ToString());
            if (const UFunction* Function = EventNode->EventReference.ResolveMember<UFunction>(Blueprint.GeneratedClass))
            {
                MemberReference->SetStringField(
                    TEXT("owner"),
                    Function->GetOwnerClass() != nullptr ? Function->GetOwnerClass()->GetPathName() : FString());
            }
            bHasMemberReference = true;
        }
        else if (const UK2Node_DynamicCast* DynamicCast = Cast<UK2Node_DynamicCast>(&Node))
        {
            MemberReference->SetStringField(TEXT("kind"), TEXT("cast"));
            MemberReference->SetStringField(
                TEXT("targetType"),
                DynamicCast->TargetType != nullptr ? DynamicCast->TargetType->GetPathName() : FString());
            bHasMemberReference = true;
        }
        else if (const UK2Node_MacroInstance* Macro = Cast<UK2Node_MacroInstance>(&Node))
        {
            MemberReference->SetStringField(TEXT("kind"), TEXT("macro"));
            MemberReference->SetStringField(
                TEXT("graph"),
                Macro->GetMacroGraph() != nullptr ? Macro->GetMacroGraph()->GetPathName() : FString());
            MemberReference->SetStringField(
                TEXT("blueprint"),
                Macro->GetSourceBlueprint() != nullptr ? Macro->GetSourceBlueprint()->GetPathName() : FString());
            bHasMemberReference = true;
        }
        else if (const UK2Node_Timeline* Timeline = Cast<UK2Node_Timeline>(&Node))
        {
            MemberReference->SetStringField(TEXT("kind"), TEXT("timeline"));
            MemberReference->SetStringField(TEXT("name"), Timeline->TimelineName.ToString());
            MemberReference->SetStringField(
                TEXT("guid"),
                Timeline->TimelineGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
            bHasMemberReference = true;
        }
        else if (const UK2Node_Composite* Composite = Cast<UK2Node_Composite>(&Node))
        {
            MemberReference->SetStringField(TEXT("kind"), TEXT("collapsedGraph"));
            MemberReference->SetStringField(
                TEXT("graph"),
                Composite->BoundGraph != nullptr ? Composite->BoundGraph->GetPathName() : FString());
            bHasMemberReference = true;
        }
        if (bHasMemberReference)
        {
            JsonNode->SetObjectField(TEXT("memberReference"), MemberReference);
        }

        TArray<UEdGraphPin*> Pins;
        Pins.Reserve(Node.Pins.Num());
        for (UEdGraphPin* Pin : Node.Pins)
        {
            if (Pin != nullptr)
            {
                Pins.Add(Pin);
            }
        }
        Pins.Sort([&PinIds](const UEdGraphPin& Left, const UEdGraphPin& Right)
        {
            return PinIds.FindChecked(&Left) < PinIds.FindChecked(&Right);
        });

        TArray<TSharedPtr<FJsonValue>> JsonPins;
        JsonPins.Reserve(Pins.Num());
        for (const UEdGraphPin* Pin : Pins)
        {
            JsonPins.Add(MakeShared<FJsonValueObject>(
                SerializePin(*Pin, PinIds.FindChecked(Pin), PinIds)));
        }
        JsonNode->SetArrayField(TEXT("pins"), JsonPins);
        return JsonNode;
    }

    FJsonObjectRef SerializeGraph(const FGraphEntry& Entry, const UBlueprint& Blueprint)
    {
        FJsonObjectRef JsonGraph = MakeShared<FJsonObject>();
        JsonGraph->SetStringField(TEXT("name"), Entry.Graph->GetName());
        JsonGraph->SetStringField(TEXT("graphKind"), Entry.Type);
        JsonGraph->SetStringField(TEXT("graphPath"), Entry.Path);
        if (!Entry.ParentGraphPath.IsEmpty())
        {
            JsonGraph->SetStringField(TEXT("parentGraphPath"), Entry.ParentGraphPath);
        }
        if (!Entry.OwnerNodeId.IsEmpty())
        {
            JsonGraph->SetStringField(TEXT("ownerNodeId"), Entry.OwnerNodeId);
        }

        TArray<UEdGraphNode*> Nodes;
        Nodes.Reserve(Entry.Graph->Nodes.Num());
        for (UEdGraphNode* Node : Entry.Graph->Nodes)
        {
            if (Node != nullptr)
            {
                Nodes.AddUnique(Node);
            }
        }
        Nodes.Sort([](const UEdGraphNode& Left, const UEdGraphNode& Right)
        {
            return NodeId(Left) < NodeId(Right);
        });

        TMap<FString, int32> LocalIdCounts;
        for (const UEdGraphNode* Node : Nodes)
        {
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin != nullptr)
                {
                    const FString LocalId = Pin->PinId.IsValid()
                        ? Pin->PinId.ToString(EGuidFormats::DigitsWithHyphensLower)
                        : Pin->GetName();
                    LocalIdCounts.FindOrAdd(LocalId)++;
                }
            }
        }
        TMap<const UEdGraphPin*, FString> PinIds;
        for (const UEdGraphNode* Node : Nodes)
        {
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin == nullptr) continue;
                const FString LocalId = Pin->PinId.IsValid()
                    ? Pin->PinId.ToString(EGuidFormats::DigitsWithHyphensLower)
                    : Pin->GetName();
                PinIds.Add(
                    Pin,
                    LocalIdCounts.FindRef(LocalId) == 1 ? LocalId : NodeId(*Node) + TEXT("/") + LocalId);
            }
        }

        const UK2Node_FunctionEntry* FunctionEntry = nullptr;
        for (const UEdGraphNode* Node : Nodes)
        {
            if (const UK2Node_FunctionEntry* Candidate = Cast<UK2Node_FunctionEntry>(Node))
            {
                FunctionEntry = Candidate;
                break;
            }
        }
        if (FunctionEntry != nullptr)
        {
            const TArray<FString> Flags = FunctionFlags(FunctionEntry->GetFunctionFlags());
            if (!Flags.IsEmpty()) JsonGraph->SetArrayField(TEXT("functionFlags"), StringArray(Flags));
            const FJsonObjectRef Metadata = SerializeFunctionMetadata(FunctionEntry->MetaData);
            if (!Metadata->Values.IsEmpty()) JsonGraph->SetObjectField(TEXT("functionMetadata"), Metadata);

            TArray<FBPVariableDescription> Locals = FunctionEntry->LocalVariables;
            Locals.Sort([](const FBPVariableDescription& Left, const FBPVariableDescription& Right)
            {
                return Left.VarName.LexicalLess(Right.VarName);
            });
            TArray<TSharedPtr<FJsonValue>> JsonLocals;
            for (const FBPVariableDescription& Local : Locals)
            {
                JsonLocals.Add(MakeShared<FJsonValueObject>(SerializeVariable(Local)));
            }
            if (!JsonLocals.IsEmpty()) JsonGraph->SetArrayField(TEXT("localVariables"), JsonLocals);
        }

        TArray<TSharedPtr<FJsonValue>> JsonNodes;
        TArray<TPair<FString, FJsonObjectRef>> Links;
        TSet<FString> LinkKeys;
        for (const UEdGraphNode* Node : Nodes)
        {
            JsonNodes.Add(MakeShared<FJsonValueObject>(SerializeNode(*Node, Blueprint, PinIds)));
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin == nullptr || Pin->Direction != EGPD_Output)
                {
                    continue;
                }
                for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (LinkedPin == nullptr || LinkedPin->GetOwningNode() == nullptr)
                    {
                        continue;
                    }
                    const FString* FromPinId = PinIds.Find(Pin);
                    const FString* ToPinId = PinIds.Find(LinkedPin);
                    if (FromPinId == nullptr || ToPinId == nullptr)
                    {
                        continue;
                    }
                    FJsonObjectRef Link = MakeShared<FJsonObject>();
                    Link->SetStringField(TEXT("fromPin"), *FromPinId);
                    Link->SetStringField(TEXT("toPin"), *ToPinId);
                    const FString LinkKey = Link->GetStringField(TEXT("fromPin")) + TEXT("->")
                        + Link->GetStringField(TEXT("toPin"));
                    if (!LinkKeys.Contains(LinkKey))
                    {
                        LinkKeys.Add(LinkKey);
                        Links.Emplace(LinkKey, Link);
                    }
                }
            }
        }
        Links.Sort([](const TPair<FString, FJsonObjectRef>& Left, const TPair<FString, FJsonObjectRef>& Right)
        {
            return Left.Key < Right.Key;
        });
        TArray<TSharedPtr<FJsonValue>> JsonLinks;
        JsonLinks.Reserve(Links.Num());
        for (const TPair<FString, FJsonObjectRef>& Link : Links)
        {
            JsonLinks.Add(MakeShared<FJsonValueObject>(Link.Value));
        }
        JsonGraph->SetArrayField(TEXT("nodes"), JsonNodes);
        if (!JsonLinks.IsEmpty())
        {
            JsonGraph->SetArrayField(TEXT("links"), JsonLinks);
        }
        return JsonGraph;
    }

    TArray<TSharedPtr<FJsonValue>> SerializeRichCurve(const FRichCurve* Curve)
    {
        TArray<TSharedPtr<FJsonValue>> Keys;
        if (Curve == nullptr)
        {
            return Keys;
        }
        for (const FRichCurveKey& Key : Curve->GetCopyOfKeys())
        {
            const TSharedRef<FJsonObject> JsonKey = MakeShared<FJsonObject>();
            JsonKey->SetNumberField(TEXT("time"), Key.Time);
            JsonKey->SetNumberField(TEXT("value"), Key.Value);
            JsonKey->SetStringField(TEXT("interpMode"), UEnum::GetValueAsString(Key.InterpMode));
            JsonKey->SetStringField(TEXT("tangentMode"), UEnum::GetValueAsString(Key.TangentMode));
            JsonKey->SetNumberField(TEXT("arriveTangent"), Key.ArriveTangent);
            JsonKey->SetNumberField(TEXT("leaveTangent"), Key.LeaveTangent);
            Keys.Add(MakeShared<FJsonValueObject>(JsonKey));
        }
        return Keys;
    }

    void AddMissingTimelineCurveDiagnostic(
        const UTimelineTemplate& Timeline,
        const FString& TrackKind,
        const FName TrackName,
        TArray<TSharedPtr<FJsonValue>>& Diagnostics)
    {
        const TSharedRef<FJsonObject> Diagnostic = MakeShared<FJsonObject>();
        Diagnostic->SetStringField(TEXT("severity"), TEXT("warning"));
        Diagnostic->SetStringField(TEXT("code"), TEXT("timelineCurveMissing"));
        Diagnostic->SetStringField(
            TEXT("message"),
            FString::Printf(
                TEXT("Timeline '%s' %s track '%s' has no readable curve object."),
                *Timeline.GetName(),
                *TrackKind,
                *TrackName.ToString()));
        Diagnostics.Add(MakeShared<FJsonValueObject>(Diagnostic));
    }

    FJsonObjectRef SerializeTimeline(
        const UTimelineTemplate& Timeline,
        TArray<TSharedPtr<FJsonValue>>& Diagnostics)
    {
        FJsonObjectRef Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Timeline.GetName());
        Json->SetStringField(TEXT("guid"), Timeline.TimelineGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
        Json->SetNumberField(TEXT("length"), Timeline.TimelineLength);
        Json->SetStringField(TEXT("lengthMode"), UEnum::GetValueAsString(Timeline.LengthMode.GetValue()));
        Json->SetBoolField(TEXT("autoPlay"), Timeline.bAutoPlay);
        Json->SetBoolField(TEXT("loop"), Timeline.bLoop);
        Json->SetBoolField(TEXT("replicated"), Timeline.bReplicated);
        Json->SetBoolField(TEXT("ignoreTimeDilation"), Timeline.bIgnoreTimeDilation);

        TArray<TPair<FString, TSharedRef<FJsonObject>>> Tracks;
        for (const FTTEventTrack& Track : Timeline.EventTracks)
        {
            FJsonObjectRef JsonTrack = MakeShared<FJsonObject>();
            JsonTrack->SetStringField(TEXT("kind"), TEXT("event"));
            JsonTrack->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
            JsonTrack->SetStringField(TEXT("function"), Track.GetFunctionName().ToString());
            JsonTrack->SetStringField(TEXT("curveStatus"), Track.CurveKeys != nullptr ? TEXT("available") : TEXT("missing"));
            JsonTrack->SetStringField(
                TEXT("curvePath"),
                Track.CurveKeys != nullptr ? Track.CurveKeys->GetPathName() : FString());
            JsonTrack->SetArrayField(
                TEXT("keys"),
                SerializeRichCurve(Track.CurveKeys != nullptr ? &Track.CurveKeys->FloatCurve : nullptr));
            if (Track.CurveKeys == nullptr)
            {
                AddMissingTimelineCurveDiagnostic(Timeline, TEXT("event"), Track.GetTrackName(), Diagnostics);
            }
            Tracks.Emplace(TEXT("event:") + Track.GetTrackName().ToString(), JsonTrack);
        }
        for (const FTTFloatTrack& Track : Timeline.FloatTracks)
        {
            FJsonObjectRef JsonTrack = MakeShared<FJsonObject>();
            JsonTrack->SetStringField(TEXT("kind"), TEXT("float"));
            JsonTrack->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
            JsonTrack->SetStringField(TEXT("property"), Track.GetPropertyName().ToString());
            JsonTrack->SetStringField(TEXT("curveStatus"), Track.CurveFloat != nullptr ? TEXT("available") : TEXT("missing"));
            JsonTrack->SetStringField(
                TEXT("curvePath"),
                Track.CurveFloat != nullptr ? Track.CurveFloat->GetPathName() : FString());
            JsonTrack->SetArrayField(
                TEXT("keys"),
                SerializeRichCurve(Track.CurveFloat != nullptr ? &Track.CurveFloat->FloatCurve : nullptr));
            if (Track.CurveFloat == nullptr)
            {
                AddMissingTimelineCurveDiagnostic(Timeline, TEXT("float"), Track.GetTrackName(), Diagnostics);
            }
            Tracks.Emplace(TEXT("float:") + Track.GetTrackName().ToString(), JsonTrack);
        }
        for (const FTTVectorTrack& Track : Timeline.VectorTracks)
        {
            FJsonObjectRef JsonTrack = MakeShared<FJsonObject>();
            JsonTrack->SetStringField(TEXT("kind"), TEXT("vector"));
            JsonTrack->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
            JsonTrack->SetStringField(TEXT("property"), Track.GetPropertyName().ToString());
            JsonTrack->SetStringField(TEXT("curveStatus"), Track.CurveVector != nullptr ? TEXT("available") : TEXT("missing"));
            JsonTrack->SetStringField(
                TEXT("curvePath"),
                Track.CurveVector != nullptr ? Track.CurveVector->GetPathName() : FString());
            const TSharedRef<FJsonObject> Channels = MakeShared<FJsonObject>();
            for (int32 Index = 0; Index < 3; ++Index)
            {
                Channels->SetArrayField(
                    Index == 0 ? TEXT("x") : Index == 1 ? TEXT("y") : TEXT("z"),
                    SerializeRichCurve(Track.CurveVector != nullptr ? &Track.CurveVector->FloatCurves[Index] : nullptr));
            }
            JsonTrack->SetObjectField(TEXT("channels"), Channels);
            if (Track.CurveVector == nullptr)
            {
                AddMissingTimelineCurveDiagnostic(Timeline, TEXT("vector"), Track.GetTrackName(), Diagnostics);
            }
            Tracks.Emplace(TEXT("vector:") + Track.GetTrackName().ToString(), JsonTrack);
        }
        for (const FTTLinearColorTrack& Track : Timeline.LinearColorTracks)
        {
            FJsonObjectRef JsonTrack = MakeShared<FJsonObject>();
            JsonTrack->SetStringField(TEXT("kind"), TEXT("linearColor"));
            JsonTrack->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
            JsonTrack->SetStringField(TEXT("property"), Track.GetPropertyName().ToString());
            JsonTrack->SetStringField(
                TEXT("curveStatus"),
                Track.CurveLinearColor != nullptr ? TEXT("available") : TEXT("missing"));
            JsonTrack->SetStringField(
                TEXT("curvePath"),
                Track.CurveLinearColor != nullptr ? Track.CurveLinearColor->GetPathName() : FString());
            const TSharedRef<FJsonObject> Channels = MakeShared<FJsonObject>();
            const TCHAR* Names[] = { TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a") };
            for (int32 Index = 0; Index < 4; ++Index)
            {
                Channels->SetArrayField(
                    Names[Index],
                    SerializeRichCurve(
                        Track.CurveLinearColor != nullptr ? &Track.CurveLinearColor->FloatCurves[Index] : nullptr));
            }
            JsonTrack->SetObjectField(TEXT("channels"), Channels);
            if (Track.CurveLinearColor == nullptr)
            {
                AddMissingTimelineCurveDiagnostic(Timeline, TEXT("linearColor"), Track.GetTrackName(), Diagnostics);
            }
            Tracks.Emplace(TEXT("linearColor:") + Track.GetTrackName().ToString(), JsonTrack);
        }
        Tracks.Sort([](const auto& Left, const auto& Right) { return Left.Key < Right.Key; });
        TArray<TSharedPtr<FJsonValue>> JsonTracks;
        for (const auto& Track : Tracks)
        {
            JsonTracks.Add(MakeShared<FJsonValueObject>(Track.Value));
        }
        Json->SetArrayField(TEXT("tracks"), JsonTracks);
        return Json;
    }

}

FName FUERingBlueprintExporter::GetName() const
{
    return TEXT("Blueprint");
}

bool FUERingBlueprintExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.IsInstanceOf(UBlueprint::StaticClass());
}


bool FUERingBlueprintExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    using namespace UERingBlueprintExporter;

    UBlueprint* Blueprint = Cast<UBlueprint>(Context.Asset.Get());
    if (Blueprint == nullptr)
    {
        OutError = TEXT("The loaded object is not a Blueprint.");
        return false;
    }

    FJsonObjectRef Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("Blueprint"));
    Semantics->SetStringField(
        TEXT("parentClass"),
        Blueprint->ParentClass != nullptr ? Blueprint->ParentClass->GetPathName() : FString());
    if (Blueprint->Status != BS_UpToDate)
    {
        Semantics->SetStringField(TEXT("compileStatus"), BlueprintStatusToString(Blueprint->Status));
    }

    TArray<FString> Interfaces;
    for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
    {
        if (Interface.Interface != nullptr)
        {
            Interfaces.Add(Interface.Interface->GetPathName());
        }
    }
    Interfaces.Sort();
    if (!Interfaces.IsEmpty())
    {
        Semantics->SetArrayField(TEXT("interfaces"), StringArray(Interfaces));
    }

    TArray<TSharedPtr<FJsonValue>> Components;
    if (Blueprint->SimpleConstructionScript != nullptr)
    {
        TArray<USCS_Node*> ComponentNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
        TMap<const USCS_Node*, FString> ParentNames;
        for (const USCS_Node* ParentNode : ComponentNodes)
        {
            if (ParentNode == nullptr)
            {
                continue;
            }
            for (const USCS_Node* ChildNode : ParentNode->GetChildNodes())
            {
                if (ChildNode != nullptr)
                {
                    ParentNames.Add(ChildNode, ParentNode->GetVariableName().ToString());
                }
            }
        }
        ComponentNodes.Sort([](const USCS_Node& Left, const USCS_Node& Right)
        {
            return Left.GetVariableName().LexicalLess(Right.GetVariableName());
        });
        for (const USCS_Node* ComponentNode : ComponentNodes)
        {
            FJsonObjectRef Component = MakeShared<FJsonObject>();
            Component->SetStringField(TEXT("name"), ComponentNode->GetVariableName().ToString());
            Component->SetStringField(
                TEXT("class"),
                ComponentNode->ComponentClass != nullptr ? ComponentNode->ComponentClass->GetPathName() : FString());
            const FString* TreeParent = ParentNames.Find(ComponentNode);
            const FString AttachParent = TreeParent != nullptr
                ? *TreeParent
                : ComponentNode->ParentComponentOrVariableName.ToString();
            if (!AttachParent.IsEmpty())
            {
                Component->SetStringField(TEXT("attachParent"), AttachParent);
            }

            UActorComponent* ComponentTemplate = Blueprint->GeneratedClass != nullptr
                ? ComponentNode->GetActualComponentTemplate(Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
                : nullptr;
            if (ComponentTemplate != nullptr)
            {
                const TArray<TSharedPtr<FJsonValue>> Properties =
                    UERingPropertySerializer::SerializeObjectProperties(
                        *ComponentTemplate,
                        ComponentTemplate->GetArchetype(),
                        true,
                        CPF_Edit | CPF_SaveGame);
                if (!Properties.IsEmpty())
                {
                    Component->SetArrayField(TEXT("properties"), Properties);
                }
                if (const USceneComponent* SceneComponent = Cast<USceneComponent>(ComponentTemplate))
                {
                    const FTransform RelativeTransform = SceneComponent->GetRelativeTransform();
                    const TSharedRef<FJsonObject> Transform = MakeShared<FJsonObject>();
                    Transform->SetStringField(TEXT("location"), RelativeTransform.GetLocation().ToString());
                    Transform->SetStringField(TEXT("rotation"), RelativeTransform.Rotator().ToString());
                    Transform->SetStringField(TEXT("scale"), RelativeTransform.GetScale3D().ToString());
                    Component->SetObjectField(TEXT("relativeTransform"), Transform);
                }
            }
            Components.Add(MakeShared<FJsonValueObject>(Component));
        }
    }
    if (!Components.IsEmpty())
    {
        Semantics->SetArrayField(TEXT("components"), Components);
    }

    TArray<FBPVariableDescription> Variables = Blueprint->NewVariables;
    Variables.Sort([](const FBPVariableDescription& Left, const FBPVariableDescription& Right)
    {
        return Left.VarName.LexicalLess(Right.VarName);
    });
    TArray<TSharedPtr<FJsonValue>> JsonVariables;
    const UObject* ClassDefaultObject = Blueprint->GeneratedClass != nullptr
        ? Blueprint->GeneratedClass->GetDefaultObject(false)
        : nullptr;
    for (const FBPVariableDescription& Variable : Variables)
    {
        const FProperty* GeneratedProperty = Blueprint->GeneratedClass != nullptr
            ? FindFProperty<FProperty>(Blueprint->GeneratedClass, Variable.VarName)
            : nullptr;
        JsonVariables.Add(MakeShared<FJsonValueObject>(
            SerializeVariable(Variable, GeneratedProperty, ClassDefaultObject)));
    }
    if (!JsonVariables.IsEmpty())
    {
        Semantics->SetArrayField(TEXT("variables"), JsonVariables);
    }

    if (Blueprint->GeneratedClass != nullptr && Blueprint->ParentClass != nullptr && ClassDefaultObject != nullptr)
    {
        const UObject* ParentDefaultObject = Blueprint->ParentClass->GetDefaultObject(false);
        TArray<const FProperty*> InheritedProperties;
        for (TFieldIterator<FProperty> It(Blueprint->GeneratedClass, EFieldIterationFlags::IncludeSuper); It; ++It)
        {
            const FProperty* Property = *It;
            constexpr uint64 SemanticFlags = CPF_Edit | CPF_BlueprintVisible | CPF_SaveGame | CPF_Config | CPF_Net;
            if (Property->GetOwnerClass() != Blueprint->GeneratedClass
                && UERingPropertySerializer::ShouldExportProperty(*Property)
                && Property->HasAnyPropertyFlags(SemanticFlags)
                && ParentDefaultObject != nullptr
                && !Property->Identical_InContainer(ClassDefaultObject, ParentDefaultObject))
            {
                InheritedProperties.Add(Property);
            }
        }
        InheritedProperties.Sort([](const FProperty& Left, const FProperty& Right)
        {
            return Left.GetName() < Right.GetName();
        });
        TArray<TSharedPtr<FJsonValue>> JsonClassDefaults;
        for (const FProperty* Property : InheritedProperties)
        {
            JsonClassDefaults.Add(MakeShared<FJsonValueObject>(
                UERingPropertySerializer::SerializeProperty(*Property, ClassDefaultObject, ClassDefaultObject)));
        }
        if (!JsonClassDefaults.IsEmpty())
        {
            TArray<TSharedPtr<FJsonValue>> JsonOwnedObjects =
                UERingOwnedObjectSerializer::SerializeOwnedObjects(
                    *ClassDefaultObject,
                    JsonClassDefaults,
                    TEXT("$classDefault"),
                    &InheritedProperties,
                    true);
            Semantics->SetArrayField(TEXT("classDefaults"), JsonClassDefaults);
            if (!JsonOwnedObjects.IsEmpty())
            {
                Semantics->SetArrayField(TEXT("classDefaultOwnedObjects"), MoveTemp(JsonOwnedObjects));
            }
        }
    }

    TArray<UTimelineTemplate*> Timelines;
    for (UTimelineTemplate* Timeline : Blueprint->Timelines)
    {
        if (Timeline != nullptr)
        {
            Timelines.Add(Timeline);
        }
    }
    Timelines.Sort([](const UTimelineTemplate& Left, const UTimelineTemplate& Right)
    {
        return Left.GetName() < Right.GetName();
    });
    TArray<TSharedPtr<FJsonValue>> JsonTimelines;
    for (const UTimelineTemplate* Timeline : Timelines)
    {
        JsonTimelines.Add(MakeShared<FJsonValueObject>(SerializeTimeline(*Timeline, OutPayload.Diagnostics)));
    }
    if (!JsonTimelines.IsEmpty())
    {
        Semantics->SetArrayField(TEXT("timelines"), JsonTimelines);
    }

    TArray<FGraphEntry> Graphs;
    TSet<const UEdGraph*> SeenGraphs;
    TFunction<void(UEdGraph*, const FString&, const FString&, const FString&)> AddGraphRecursive;
    AddGraphRecursive = [&Graphs, &SeenGraphs, &AddGraphRecursive](
        UEdGraph* Graph,
        const FString& Type,
        const FString& ParentGraphPath,
        const FString& OwnerNodeId)
    {
        if (Graph == nullptr || SeenGraphs.Contains(Graph))
        {
            return;
        }
        SeenGraphs.Add(Graph);
        Graphs.Add({
            Graph,
            GraphKind(*Graph, Type),
            Graph->GetPathName(),
            ParentGraphPath,
            OwnerNodeId });

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node == nullptr)
            {
                continue;
            }
            for (UEdGraph* SubGraph : Node->GetSubGraphs())
            {
                AddGraphRecursive(
                    SubGraph,
                    Node->IsA<UK2Node_Composite>() ? TEXT("Collapsed") : TEXT("SubGraph"),
                    Graph->GetPathName(),
                    NodeId(*Node));
            }
        }
        for (UEdGraph* SubGraph : Graph->SubGraphs)
        {
            AddGraphRecursive(SubGraph, TEXT("SubGraph"), Graph->GetPathName(), FString());
        }
    };
    auto AddGraphs = [&AddGraphRecursive](const TArray<TObjectPtr<UEdGraph>>& SourceGraphs, const TCHAR* Type)
    {
        for (UEdGraph* Graph : SourceGraphs)
        {
            AddGraphRecursive(Graph, Type, FString(), FString());
        }
    };
    AddGraphs(Blueprint->UbergraphPages, TEXT("Ubergraph"));
    AddGraphs(Blueprint->FunctionGraphs, TEXT("Function"));
    AddGraphs(Blueprint->MacroGraphs, TEXT("Macro"));
    AddGraphs(Blueprint->DelegateSignatureGraphs, TEXT("DelegateSignature"));
    for (const FBPInterfaceDescription& Interface : Blueprint->ImplementedInterfaces)
    {
        AddGraphs(Interface.Graphs, TEXT("Interface"));
    }
    Graphs.Sort([](const FGraphEntry& Left, const FGraphEntry& Right)
    {
        const FString LeftKey = Left.Type + TEXT(":") + Left.Path;
        const FString RightKey = Right.Type + TEXT(":") + Right.Path;
        return LeftKey < RightKey;
    });

    TArray<TSharedPtr<FJsonValue>> JsonGraphs;
    TMap<FString, FJsonObjectRef> CppLinksByKey;
    auto AddCppLink = [&CppLinksByKey](
        const FString& AssetNode,
        const TCHAR* SymbolKind,
        const FString& Symbol,
        const FString& Owner)
    {
        FJsonObjectRef Link = MakeShared<FJsonObject>();
        Link->SetStringField(TEXT("assetNode"), AssetNode);
        Link->SetStringField(TEXT("symbolKind"), SymbolKind);
        Link->SetStringField(TEXT("symbol"), Symbol);
        Link->SetStringField(TEXT("owner"), Owner);
        const FString Key = Symbol + TEXT(":") + AssetNode;
        if (!CppLinksByKey.Contains(Key))
        {
            CppLinksByKey.Add(Key, Link);
        }
    };
    if (Blueprint->ParentClass != nullptr && Blueprint->ParentClass->HasAnyClassFlags(CLASS_Native))
    {
        AddCppLink(TEXT("$parentClass"), TEXT("UClass"), Blueprint->ParentClass->GetPathName(), Blueprint->ParentClass->GetPathName());
    }
    if (Blueprint->SimpleConstructionScript != nullptr)
    {
        for (const USCS_Node* ComponentNode : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (ComponentNode != nullptr && ComponentNode->ComponentClass != nullptr
                && ComponentNode->ComponentClass->HasAnyClassFlags(CLASS_Native))
            {
                AddCppLink(
                    TEXT("$component:") + ComponentNode->GetVariableName().ToString(),
                    TEXT("UClass"),
                    ComponentNode->ComponentClass->GetPathName(),
                    ComponentNode->ComponentClass->GetPathName());
            }
        }
    }
    for (const FGraphEntry& Graph : Graphs)
    {
        JsonGraphs.Add(MakeShared<FJsonValueObject>(SerializeGraph(Graph, *Blueprint)));
        for (const UEdGraphNode* Node : Graph.Graph->Nodes)
        {
            if (const UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(Node))
            {
                const UFunction* Function = CallFunction->GetTargetFunction();
                const UClass* OwnerClass = Function != nullptr ? Function->GetOwnerClass() : nullptr;
                if (Function != nullptr && OwnerClass != nullptr && OwnerClass->HasAnyClassFlags(CLASS_Native))
                {
                    AddCppLink(NodeId(*Node), TEXT("UFunction"), Function->GetPathName(), OwnerClass->GetPathName());
                }
            }
            if (const UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))
            {
                const FProperty* Property = VariableNode->GetPropertyForVariable();
                const UClass* OwnerClass = Property != nullptr ? Property->GetOwnerClass() : nullptr;
                if (Property != nullptr && OwnerClass != nullptr && OwnerClass->HasAnyClassFlags(CLASS_Native))
                {
                    AddCppLink(
                        NodeId(*Node), TEXT("FProperty"),
                        OwnerClass->GetPathName() + TEXT(":") + Property->GetName(),
                        OwnerClass->GetPathName());
                }
            }
            if (const UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
            {
                const UFunction* Function = EventNode->EventReference.ResolveMember<UFunction>(Blueprint->GeneratedClass);
                const UClass* OwnerClass = Function != nullptr ? Function->GetOwnerClass() : nullptr;
                if (Function != nullptr && OwnerClass != nullptr && OwnerClass->HasAnyClassFlags(CLASS_Native))
                {
                    AddCppLink(NodeId(*Node), TEXT("UFunction"), Function->GetPathName(), OwnerClass->GetPathName());
                }
            }
            if (const UK2Node_BaseMCDelegate* DelegateNode = Cast<UK2Node_BaseMCDelegate>(Node))
            {
                const FProperty* Property = DelegateNode->GetProperty();
                const UClass* OwnerClass = Property != nullptr ? Property->GetOwnerClass() : nullptr;
                if (Property != nullptr && OwnerClass != nullptr && OwnerClass->HasAnyClassFlags(CLASS_Native))
                {
                    AddCppLink(
                        NodeId(*Node), TEXT("FProperty"),
                        OwnerClass->GetPathName() + TEXT(":") + Property->GetName(),
                        OwnerClass->GetPathName());
                }
            }
            if (const UK2Node_CreateDelegate* CreateDelegate = Cast<UK2Node_CreateDelegate>(Node))
            {
                const UClass* ScopeClass = CreateDelegate->GetScopeClass();
                const UFunction* Function = ScopeClass != nullptr
                    ? ScopeClass->FindFunctionByName(CreateDelegate->GetFunctionName())
                    : nullptr;
                if (Function != nullptr && ScopeClass->HasAnyClassFlags(CLASS_Native))
                {
                    AddCppLink(NodeId(*Node), TEXT("UFunction"), Function->GetPathName(), ScopeClass->GetPathName());
                }
            }
        }
    }
    TArray<TPair<FString, FJsonObjectRef>> SortedCppLinks;
    SortedCppLinks.Reserve(CppLinksByKey.Num());
    for (const TPair<FString, FJsonObjectRef>& Pair : CppLinksByKey)
    {
        SortedCppLinks.Emplace(Pair.Key, Pair.Value);
    }
    SortedCppLinks.Sort([](const TPair<FString, FJsonObjectRef>& Left, const TPair<FString, FJsonObjectRef>& Right)
    {
        return Left.Key < Right.Key;
    });
    for (TPair<FString, FJsonObjectRef>& Pair : SortedCppLinks)
    {
        FString OwnerPath;
        Pair.Value->TryGetStringField(TEXT("owner"), OwnerPath);
        const UClass* OwnerClass = FindObject<UClass>(nullptr, *OwnerPath);
        FString HeaderPath;
        FString SourcePath;
        if (OwnerClass != nullptr)
        {
            FSourceCodeNavigation::FindClassHeaderPath(OwnerClass, HeaderPath);
            FSourceCodeNavigation::FindClassSourcePath(OwnerClass, SourcePath);
        }
        FPaths::NormalizeFilename(HeaderPath);
        FPaths::NormalizeFilename(SourcePath);
        if (!HeaderPath.IsEmpty())
        {
            Pair.Value->SetStringField(TEXT("header"), HeaderPath);
        }
        if (!SourcePath.IsEmpty())
        {
            Pair.Value->SetStringField(TEXT("source"), SourcePath);
        }
    }
    OutPayload.CppLinks.Reserve(SortedCppLinks.Num());
    for (const TPair<FString, FJsonObjectRef>& Link : SortedCppLinks)
    {
        OutPayload.CppLinks.Add(MakeShared<FJsonValueObject>(Link.Value));
    }
    if (!JsonGraphs.IsEmpty())
    {
        Semantics->SetArrayField(TEXT("graphs"), JsonGraphs);
    }
    OutPayload.Semantics = Semantics;

    if (Blueprint->Status == BS_Error || Blueprint->Status == BS_UpToDateWithWarnings)
    {
        FJsonObjectRef Diagnostic = MakeShared<FJsonObject>();
        Diagnostic->SetStringField(
            TEXT("severity"),
            Blueprint->Status == BS_Error ? TEXT("error") : TEXT("warning"));
        Diagnostic->SetStringField(TEXT("code"), TEXT("blueprint.compileStatus"));
        Diagnostic->SetStringField(TEXT("message"), FString::Printf(
            TEXT("Blueprint compile status is %s."),
            *BlueprintStatusToString(Blueprint->Status)));
        OutPayload.Diagnostics.Add(MakeShared<FJsonValueObject>(Diagnostic));
    }
    return true;
}
