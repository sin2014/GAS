#include "UERingPinTypeSerializer.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

namespace UERingPinTypeSerializer
{
    namespace
    {
        TSharedRef<FJsonObject> SerializeTerminal(const FEdGraphTerminalType& Type)
        {
            const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
            Json->SetStringField(TEXT("category"), Type.TerminalCategory.ToString());
            if (!Type.TerminalSubCategory.IsNone())
            {
                Json->SetStringField(TEXT("subCategory"), Type.TerminalSubCategory.ToString());
            }
            if (const UObject* TypeObject = Type.TerminalSubCategoryObject.Get())
            {
                Json->SetStringField(TEXT("typeObject"), TypeObject->GetPathName());
            }
            if (Type.bTerminalIsConst) Json->SetBoolField(TEXT("const"), true);
            if (Type.bTerminalIsWeakPointer) Json->SetBoolField(TEXT("weakPointer"), true);
            if (Type.bTerminalIsUObjectWrapper) Json->SetBoolField(TEXT("objectWrapper"), true);
            return Json;
        }
    }

    TSharedRef<FJsonObject> Serialize(const FEdGraphPinType& Type)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("category"), Type.PinCategory.ToString());
        if (!Type.PinSubCategory.IsNone())
        {
            Json->SetStringField(TEXT("subCategory"), Type.PinSubCategory.ToString());
        }
        if (const UObject* TypeObject = Type.PinSubCategoryObject.Get())
        {
            Json->SetStringField(TEXT("typeObject"), TypeObject->GetPathName());
        }
        if (Type.PinSubCategoryMemberReference.MemberParent != nullptr)
        {
            Json->SetStringField(
                TEXT("memberParent"),
                Type.PinSubCategoryMemberReference.MemberParent->GetPathName());
        }
        if (!Type.PinSubCategoryMemberReference.MemberName.IsNone())
        {
            Json->SetStringField(TEXT("memberName"), Type.PinSubCategoryMemberReference.MemberName.ToString());
        }
        if (Type.PinSubCategoryMemberReference.MemberGuid.IsValid())
        {
            Json->SetStringField(
                TEXT("memberGuid"),
                Type.PinSubCategoryMemberReference.MemberGuid.ToString(EGuidFormats::DigitsWithHyphensLower));
        }
        if (Type.ContainerType != EPinContainerType::None)
        {
            Json->SetStringField(
                TEXT("container"),
                Type.IsArray() ? TEXT("array") : Type.IsSet() ? TEXT("set") : TEXT("map"));
        }
        if (Type.IsMap()) Json->SetObjectField(TEXT("valueType"), SerializeTerminal(Type.PinValueType));
        if (Type.bIsReference) Json->SetBoolField(TEXT("reference"), true);
        if (Type.bIsConst) Json->SetBoolField(TEXT("const"), true);
        if (Type.bIsWeakPointer) Json->SetBoolField(TEXT("weakPointer"), true);
        if (Type.bIsUObjectWrapper) Json->SetBoolField(TEXT("objectWrapper"), true);
        if (Type.bSerializeAsSinglePrecisionFloat) Json->SetBoolField(TEXT("singlePrecision"), true);
        return Json;
    }
}
