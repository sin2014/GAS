#include "UERingWidgetBlueprintExporter.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Components/PanelSlot.h"
#include "UERingBlueprintExporter.h"
#include "UERingOwnedObjectSerializer.h"
#include "UERingPropertySerializer.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace UERingWidgetBlueprintExporter
{
    TSharedRef<FJsonObject> Vector2D(const FVector2D& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("x"), Value.X);
        Json->SetNumberField(TEXT("y"), Value.Y);
        return Json;
    }

    FString WidgetKey(const UWidget& Widget)
    {
        const UPanelWidget* Parent = Widget.GetParent();
        const int32 Index = Parent != nullptr ? Parent->GetChildIndex(&Widget) : 0;
        return Parent != nullptr
            ? WidgetKey(*Parent) + FString::Printf(TEXT("/%06d:%s"), Index, *Widget.GetName())
            : FString::Printf(TEXT("/%06d:%s"), Index, *Widget.GetName());
    }

    TSharedRef<FJsonObject> SerializeWidget(
        const UWidget& Widget,
        const bool bInherited,
        const FString& InheritedFrom)
    {
        const TSharedRef<FJsonObject> JsonWidget = MakeShared<FJsonObject>();
        JsonWidget->SetStringField(TEXT("name"), Widget.GetName());
        JsonWidget->SetStringField(TEXT("class"), Widget.GetClass()->GetPathName());
        JsonWidget->SetStringField(
            TEXT("parent"),
            Widget.GetParent() != nullptr ? Widget.GetParent()->GetName() : FString());
        JsonWidget->SetNumberField(
            TEXT("order"),
            Widget.GetParent() != nullptr ? Widget.GetParent()->GetChildIndex(&Widget) : 0);
        JsonWidget->SetStringField(TEXT("hierarchyPath"), WidgetKey(Widget));
        JsonWidget->SetBoolField(TEXT("isVariable"), Widget.bIsVariable);
        JsonWidget->SetBoolField(TEXT("inherited"), bInherited);
        JsonWidget->SetStringField(TEXT("inheritedFrom"), InheritedFrom);
        JsonWidget->SetStringField(
            TEXT("slotClass"),
            Widget.Slot != nullptr ? Widget.Slot->GetClass()->GetPathName() : FString());
        JsonWidget->SetStringField(TEXT("visibility"), UEnum::GetValueAsString(Widget.GetVisibility()));
        JsonWidget->SetBoolField(TEXT("enabled"), Widget.GetIsEnabled());
        JsonWidget->SetNumberField(TEXT("renderOpacity"), Widget.GetRenderOpacity());

        const FWidgetTransform& WidgetTransform = Widget.GetRenderTransform();
        const TSharedRef<FJsonObject> RenderTransform = MakeShared<FJsonObject>();
        RenderTransform->SetObjectField(TEXT("translation"), Vector2D(WidgetTransform.Translation));
        RenderTransform->SetObjectField(TEXT("scale"), Vector2D(WidgetTransform.Scale));
        RenderTransform->SetObjectField(TEXT("shear"), Vector2D(WidgetTransform.Shear));
        RenderTransform->SetNumberField(TEXT("angle"), WidgetTransform.Angle);
        JsonWidget->SetObjectField(TEXT("renderTransform"), RenderTransform);
        JsonWidget->SetObjectField(TEXT("renderTransformPivot"), Vector2D(Widget.GetRenderTransformPivot()));

        JsonWidget->SetArrayField(
            TEXT("properties"),
            UERingPropertySerializer::SerializeObjectProperties(
                Widget,
                Widget.GetClass()->GetDefaultObject(),
                true,
                CPF_Edit | CPF_SaveGame));
        if (Widget.Slot != nullptr)
        {
            JsonWidget->SetArrayField(
                TEXT("slotProperties"),
                UERingPropertySerializer::SerializeObjectProperties(
                    *Widget.Slot,
                    Widget.Slot->GetClass()->GetDefaultObject(),
                    true,
                    CPF_Edit | CPF_SaveGame));
        }
        else
        {
            JsonWidget->SetArrayField(TEXT("slotProperties"), TArray<TSharedPtr<FJsonValue>>());
        }
        return JsonWidget;
    }

    TSharedRef<FJsonObject> SerializeInheritedWidgetDeclaration(
        const FObjectPropertyBase& Property,
        const UClass& DeclaringClass)
    {
        const bool bOptional = Property.HasMetaData(TEXT("BindWidgetOptional"))
            || Property.HasMetaData(TEXT("OptionalWidget"))
            || Property.GetBoolMetaData(TEXT("OptionalWidget"));

        const TSharedRef<FJsonObject> JsonWidget = MakeShared<FJsonObject>();
        JsonWidget->SetStringField(TEXT("name"), Property.GetName());
        JsonWidget->SetStringField(
            TEXT("class"),
            Property.PropertyClass != nullptr ? Property.PropertyClass->GetPathName() : FString());
        JsonWidget->SetStringField(TEXT("parent"), FString());
        JsonWidget->SetNumberField(TEXT("order"), -1);
        JsonWidget->SetStringField(
            TEXT("hierarchyPath"),
            FString::Printf(TEXT("/declaration:%s:%s"), *DeclaringClass.GetPathName(), *Property.GetName()));
        JsonWidget->SetBoolField(TEXT("isVariable"), false);
        JsonWidget->SetBoolField(TEXT("inherited"), true);
        JsonWidget->SetStringField(TEXT("inheritedFrom"), DeclaringClass.GetPathName());
        JsonWidget->SetBoolField(TEXT("declarationOnly"), true);
        JsonWidget->SetBoolField(TEXT("required"), !bOptional);
        JsonWidget->SetStringField(
            TEXT("optionalReason"),
            Property.HasMetaData(TEXT("BindWidgetOptional"))
                ? TEXT("BindWidgetOptional")
                : Property.HasMetaData(TEXT("OptionalWidget"))
                    ? TEXT("OptionalWidget")
                    : FString());
        JsonWidget->SetStringField(
            TEXT("bindingMetadata"),
            Property.HasMetaData(TEXT("BindWidgetOptional")) ? TEXT("BindWidgetOptional") : TEXT("BindWidget"));
        JsonWidget->SetStringField(TEXT("slotClass"), FString());
        JsonWidget->SetArrayField(TEXT("properties"), TArray<TSharedPtr<FJsonValue>>());
        JsonWidget->SetArrayField(TEXT("slotProperties"), TArray<TSharedPtr<FJsonValue>>());
        return JsonWidget;
    }
}

FName FUERingWidgetBlueprintExporter::GetName() const
{
    return TEXT("WidgetBlueprint");
}

bool FUERingWidgetBlueprintExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.IsInstanceOf(UWidgetBlueprint::StaticClass());
}

bool FUERingWidgetBlueprintExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    using namespace UERingWidgetBlueprintExporter;

    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(Context.Asset.Get());
    if (WidgetBlueprint == nullptr)
    {
        OutError = TEXT("The loaded object is not a Widget Blueprint.");
        return false;
    }

    const FUERingBlueprintExporter BlueprintExporter;
    if (!BlueprintExporter.BuildPayload(Context, OutPayload, OutError))
    {
        return false;
    }
    OutPayload.Semantics->SetStringField(TEXT("kind"), TEXT("WidgetBlueprint"));

    TArray<UWidget*> Widgets;
    if (WidgetBlueprint->WidgetTree != nullptr)
    {
        WidgetBlueprint->WidgetTree->GetAllWidgets(Widgets);
    }
    Widgets.Sort([](const UWidget& Left, const UWidget& Right)
    {
        return WidgetKey(Left) < WidgetKey(Right);
    });

    TArray<TSharedPtr<FJsonValue>> JsonWidgets;
    for (const UWidget* Widget : Widgets)
    {
        JsonWidgets.Add(MakeShared<FJsonValueObject>(SerializeWidget(*Widget, false, FString())));
    }
    OutPayload.Semantics->SetArrayField(TEXT("widgets"), JsonWidgets);

    TArray<TPair<FString, TSharedRef<FJsonObject>>> SortedInheritedWidgets;
    TSet<FName> InheritedWidgetNames;
    for (UClass* Class = WidgetBlueprint->GeneratedClass != nullptr
            ? WidgetBlueprint->GeneratedClass->GetSuperClass()
            : nullptr;
         Class != nullptr;
         Class = Class->GetSuperClass())
    {
        const UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(Class);
        if (WidgetClass != nullptr && WidgetClass->GetWidgetTreeArchetype() != nullptr)
        {
            TArray<UWidget*> ParentWidgets;
            WidgetClass->GetWidgetTreeArchetype()->GetAllWidgets(ParentWidgets);
            for (const UWidget* ParentWidget : ParentWidgets)
            {
                if (ParentWidget != nullptr)
                {
                    const FString Key = Class->GetPathName() + TEXT(":") + WidgetKey(*ParentWidget);
                    if (!InheritedWidgetNames.Contains(ParentWidget->GetFName()))
                    {
                        InheritedWidgetNames.Add(ParentWidget->GetFName());
                        SortedInheritedWidgets.Emplace(
                            Key,
                            SerializeWidget(*ParentWidget, true, Class->GetPathName()));
                    }
                }
            }
        }

        for (TFieldIterator<FObjectPropertyBase> PropertyIt(Class, EFieldIterationFlags::None);
             PropertyIt;
             ++PropertyIt)
        {
            const FObjectPropertyBase* Property = *PropertyIt;
            if (Property == nullptr
                || Property->PropertyClass == nullptr
                || !Property->PropertyClass->IsChildOf(UWidget::StaticClass())
                || InheritedWidgetNames.Contains(Property->GetFName())
                || (!Property->HasMetaData(TEXT("BindWidget"))
                    && !Property->HasMetaData(TEXT("BindWidgetOptional"))))
            {
                continue;
            }

            const FString Key = Class->GetPathName() + TEXT(":declaration:") + Property->GetName();
            InheritedWidgetNames.Add(Property->GetFName());
            SortedInheritedWidgets.Emplace(
                Key,
                SerializeInheritedWidgetDeclaration(*Property, *Class));
        }
    }
    SortedInheritedWidgets.Sort([](const auto& Left, const auto& Right) { return Left.Key < Right.Key; });
    TArray<TSharedPtr<FJsonValue>> JsonInheritedWidgets;
    for (const auto& Pair : SortedInheritedWidgets)
    {
        JsonInheritedWidgets.Add(MakeShared<FJsonValueObject>(Pair.Value));
    }
    OutPayload.Semantics->SetArrayField(TEXT("inheritedWidgets"), JsonInheritedWidgets);

    TArray<FString> NamedSlots;
    if (const UWidgetBlueprintGeneratedClass* GeneratedClass =
        Cast<UWidgetBlueprintGeneratedClass>(WidgetBlueprint->GeneratedClass))
    {
        for (const FName Name : GeneratedClass->NamedSlots)
        {
            NamedSlots.Add(Name.ToString());
        }
    }
    NamedSlots.Sort();
    TArray<TSharedPtr<FJsonValue>> JsonNamedSlots;
    for (const FString& Name : NamedSlots)
    {
        JsonNamedSlots.Add(MakeShared<FJsonValueString>(Name));
    }
    OutPayload.Semantics->SetArrayField(TEXT("namedSlots"), JsonNamedSlots);

    TArray<UWidgetAnimation*> Animations;
    for (UWidgetAnimation* Animation : WidgetBlueprint->Animations)
    {
        if (Animation != nullptr) Animations.Add(Animation);
    }
    Animations.Sort([](const UWidgetAnimation& Left, const UWidgetAnimation& Right)
    {
        return Left.GetName() < Right.GetName();
    });
    TArray<TSharedPtr<FJsonValue>> JsonAnimations;
    JsonAnimations.Reserve(Animations.Num());
    for (const UWidgetAnimation* Animation : Animations)
    {
        TArray<TSharedPtr<FJsonValue>> AnimationProperties =
            UERingPropertySerializer::SerializeObjectProperties(*Animation);
        TArray<TSharedPtr<FJsonValue>> AnimationObjects =
            UERingOwnedObjectSerializer::SerializeOwnedObjects(
                *Animation,
                AnimationProperties,
                TEXT("$animation"));

        const TSharedRef<FJsonObject> JsonAnimation = MakeShared<FJsonObject>();
        JsonAnimation->SetStringField(TEXT("name"), Animation->GetName());
        JsonAnimation->SetStringField(TEXT("class"), Animation->GetClass()->GetPathName());
        JsonAnimation->SetArrayField(TEXT("properties"), MoveTemp(AnimationProperties));
        JsonAnimation->SetNumberField(TEXT("objectCount"), AnimationObjects.Num());
        JsonAnimation->SetArrayField(TEXT("objects"), MoveTemp(AnimationObjects));
        JsonAnimations.Add(MakeShared<FJsonValueObject>(JsonAnimation));
    }
    OutPayload.Semantics->SetNumberField(TEXT("animationCount"), JsonAnimations.Num());
    OutPayload.Semantics->SetArrayField(TEXT("animations"), MoveTemp(JsonAnimations));

    TArray<FDelegateEditorBinding> Bindings = WidgetBlueprint->Bindings;
    Bindings.Sort([](const FDelegateEditorBinding& Left, const FDelegateEditorBinding& Right)
    {
        const FString LeftKey = Left.ObjectName + TEXT(":") + Left.PropertyName.ToString();
        const FString RightKey = Right.ObjectName + TEXT(":") + Right.PropertyName.ToString();
        return LeftKey < RightKey;
    });
    TArray<TSharedPtr<FJsonValue>> JsonBindings;
    for (const FDelegateEditorBinding& Binding : Bindings)
    {
        const TSharedRef<FJsonObject> JsonBinding = MakeShared<FJsonObject>();
        JsonBinding->SetStringField(TEXT("object"), Binding.ObjectName);
        JsonBinding->SetStringField(TEXT("property"), Binding.PropertyName.ToString());
        JsonBinding->SetStringField(TEXT("function"), Binding.FunctionName.ToString());
        JsonBinding->SetStringField(TEXT("sourceProperty"), Binding.SourceProperty.ToString());
        JsonBinding->SetStringField(TEXT("memberGuid"), Binding.MemberGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
        JsonBindings.Add(MakeShared<FJsonValueObject>(JsonBinding));
    }
    OutPayload.Semantics->SetArrayField(TEXT("bindings"), JsonBindings);
    return true;
}
