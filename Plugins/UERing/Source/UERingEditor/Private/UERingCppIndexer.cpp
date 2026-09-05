#include "UERingCppIndexer.h"

#include "Algo/Unique.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UERingExportManager.h"
#include "UERingBlueprintMigrationReporter.h"
#include "UERingSettings.h"
#include "UERingVersion.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace UERingCppIndexer
{
    using FReverseLinks = TMap<FString, TArray<TSharedPtr<FJsonValue>>>;

    bool WriteJson(const FString& Filename, const TSharedRef<FJsonObject>& Root, FString& OutError)
    {
        FString Json;
        const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
        if (!FJsonSerializer::Serialize(Root, Writer))
        {
            OutError = FString::Printf(TEXT("Could not serialize C++ index: %s"), *Filename);
            return false;
        }
        Json += LINE_TERMINATOR;
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        const FString Temp = Filename + TEXT(".tmp");
        if (!FFileHelper::SaveStringToFile(Json, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
            || !IFileManager::Get().Move(*Filename, *Temp, true, true, false, true))
        {
            IFileManager::Get().Delete(*Temp, false, true);
            OutError = FString::Printf(TEXT("Could not write C++ index: %s"), *Filename);
            return false;
        }
        return true;
    }

    FString ProjectRelative(FString Filename)
    {
        FPaths::MakePathRelativeTo(Filename, *FPaths::ProjectDir());
        FPaths::NormalizeFilename(Filename);
        return Filename;
    }

    void CollectSourceFiles(const FString& Root, TArray<FString>& OutFiles)
    {
        if (!IFileManager::Get().DirectoryExists(*Root))
        {
            return;
        }
        TArray<FString> Files;
        IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.*"), true, false);
        for (const FString& File : Files)
        {
            const FString Extension = FPaths::GetExtension(File, true).ToLower();
            if (Extension == TEXT(".h") || Extension == TEXT(".hpp")
                || Extension == TEXT(".cpp") || Extension == TEXT(".inl"))
            {
                OutFiles.Add(File);
            }
        }
    }

    void CollectReverseLinksFromFile(const FString& File, FReverseLinks& OutReverseLinks)
    {
        FString Json;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(Json, *File)
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root)
            || !Root.IsValid())
        {
            return;
        }
        const TSharedPtr<FJsonObject>* Asset = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
        if (!Root->TryGetObjectField(TEXT("asset"), Asset)
            || !Root->TryGetArrayField(TEXT("cppLinks"), Links)
            || Asset == nullptr || Links == nullptr)
        {
            return;
        }
        FString PackageName;
        (*Asset)->TryGetStringField(TEXT("packageName"), PackageName);
        for (const TSharedPtr<FJsonValue>& Value : *Links)
        {
            const TSharedPtr<FJsonObject>* Link = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(Link) || Link == nullptr)
            {
                continue;
            }
            FString Symbol;
            FString AssetNode;
            (*Link)->TryGetStringField(TEXT("symbol"), Symbol);
            (*Link)->TryGetStringField(TEXT("assetNode"), AssetNode);
            if (Symbol.IsEmpty())
            {
                continue;
            }
            const TSharedRef<FJsonObject> Reverse = MakeShared<FJsonObject>();
            Reverse->SetStringField(TEXT("asset"), PackageName);
            Reverse->SetStringField(TEXT("node"), AssetNode);
            OutReverseLinks.FindOrAdd(Symbol).Add(MakeShared<FJsonValueObject>(Reverse));
        }
    }

    FString ReverseLinkKey(const TSharedPtr<FJsonValue>& Value)
    {
        const TSharedPtr<FJsonObject>* Object = nullptr;
        FString Asset;
        FString Node;
        if (Value.IsValid() && Value->TryGetObject(Object) && Object != nullptr)
        {
            (*Object)->TryGetStringField(TEXT("asset"), Asset);
            (*Object)->TryGetStringField(TEXT("node"), Node);
        }
        return Asset + TEXT("\x0001") + Node;
    }

    void SortReverseReferences(TArray<TSharedPtr<FJsonValue>>& References)
    {
        References.Sort([](const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right)
        {
            return ReverseLinkKey(Left) < ReverseLinkKey(Right);
        });
        FString PreviousKey;
        References.RemoveAll([&PreviousKey](const TSharedPtr<FJsonValue>& Value)
        {
            const FString Key = ReverseLinkKey(Value);
            const bool bDuplicate = Key == PreviousKey;
            PreviousKey = Key;
            return bDuplicate;
        });
    }

    void SortReverseLinks(FReverseLinks& ReverseLinks)
    {
        for (TPair<FString, TArray<TSharedPtr<FJsonValue>>>& Pair : ReverseLinks)
        {
            SortReverseReferences(Pair.Value);
        }
    }

    FReverseLinks CollectReverseLinks()
    {
        FReverseLinks ReverseLinks;
        TArray<FString> SemanticFiles;
        IFileManager::Get().FindFilesRecursive(
            SemanticFiles,
            *FUERingExportManager::Get().GetOutputRoot(),
            TEXT("*.uesem.json"),
            true,
            false);
        SemanticFiles.Sort();
        for (const FString& File : SemanticFiles)
        {
            CollectReverseLinksFromFile(File, ReverseLinks);
        }
        SortReverseLinks(ReverseLinks);
        return ReverseLinks;
    }

    bool LoadJsonObject(const FString& Filename, TSharedPtr<FJsonObject>& OutRoot)
    {
        FString Json;
        return FFileHelper::LoadFileToString(Json, *Filename)
            && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), OutRoot)
            && OutRoot.IsValid();
    }

    bool FilterDirtyReferences(
        const TSharedRef<FJsonObject>& Object,
        const TSet<FString>& DirtyPackages)
    {
        const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
        if (!Object->TryGetArrayField(TEXT("blueprintReferences"), Existing) || Existing == nullptr)
        {
            return false;
        }
        TArray<TSharedPtr<FJsonValue>> Filtered;
        Filtered.Reserve(Existing->Num());
        for (const TSharedPtr<FJsonValue>& Value : *Existing)
        {
            const TSharedPtr<FJsonObject>* Reference = nullptr;
            FString Asset;
            if (!Value.IsValid()
                || !Value->TryGetObject(Reference)
                || Reference == nullptr
                || !(*Reference)->TryGetStringField(TEXT("asset"), Asset))
            {
                return false;
            }
            if (!DirtyPackages.Contains(Asset))
            {
                Filtered.Add(Value);
            }
        }
        Object->SetArrayField(TEXT("blueprintReferences"), Filtered);
        return true;
    }

    bool LoadProjectModules(const FString& CppDirectory, TSet<FString>& OutModules)
    {
        TSharedPtr<FJsonObject> Source;
        const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
        FString Schema;
        FString SchemaVersion;
        if (!LoadJsonObject(FPaths::Combine(CppDirectory, TEXT("source-index.uesem.json")), Source)
            || !Source->TryGetStringField(TEXT("schema"), Schema)
            || Schema != TEXT("com.ue-ring.usem.cpp-source")
            || !Source->TryGetStringField(TEXT("schemaVersion"), SchemaVersion)
            || SchemaVersion != UE_RING_SCHEMA_VERSION
            || !Source->TryGetArrayField(TEXT("files"), Files)
            || Files == nullptr)
        {
            return false;
        }
        for (const TSharedPtr<FJsonValue>& Value : *Files)
        {
            const TSharedPtr<FJsonObject>* File = nullptr;
            FString Path;
            if (!Value.IsValid()
                || !Value->TryGetObject(File)
                || File == nullptr
                || !(*File)->TryGetStringField(TEXT("path"), Path))
            {
                return false;
            }
            TArray<FString> Parts;
            Path.ParseIntoArray(Parts, TEXT("/"), true);
            const int32 SourceIndex = Parts.IndexOfByKey(TEXT("Source"));
            if (SourceIndex != INDEX_NONE && Parts.IsValidIndex(SourceIndex + 1))
            {
                OutModules.Add(Parts[SourceIndex + 1]);
            }
        }
        return true;
    }
}

bool FUERingCppIndexer::Rebuild(FString& OutError)
{
    using namespace UERingCppIndexer;

    TArray<FString> SourceFiles;
    CollectSourceFiles(FPaths::Combine(FPaths::ProjectDir(), TEXT("Source")), SourceFiles);
    CollectSourceFiles(FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins")), SourceFiles);
    SourceFiles.Sort();
    SourceFiles.SetNum(Algo::Unique(SourceFiles));

    TSet<FString> ProjectModules;
    for (const FString& SourceFile : SourceFiles)
    {
        FString Relative = ProjectRelative(SourceFile);
        TArray<FString> Parts;
        Relative.ParseIntoArray(Parts, TEXT("/"), true);
        const int32 SourceIndex = Parts.IndexOfByKey(TEXT("Source"));
        if (SourceIndex != INDEX_NONE && Parts.IsValidIndex(SourceIndex + 1))
        {
            ProjectModules.Add(Parts[SourceIndex + 1]);
        }
    }

    TMap<FString, TArray<TSharedPtr<FJsonValue>>> ReverseLinks = CollectReverseLinks();
    TArray<UClass*> Classes;
    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;
        FString ModuleName = Class->GetOutermost()->GetName();
        ModuleName.RemoveFromStart(TEXT("/Script/"));
        if (Class->HasAnyClassFlags(CLASS_Native)
            && (ProjectModules.Contains(ModuleName) || ReverseLinks.Contains(Class->GetPathName())))
        {
            Classes.Add(Class);
        }
    }
    Classes.Sort([](const UClass& Left, const UClass& Right)
    {
        return Left.GetPathName() < Right.GetPathName();
    });

    TArray<TSharedPtr<FJsonValue>> JsonClasses;
    for (const UClass* Class : Classes)
    {
        const TSharedRef<FJsonObject> JsonClass = MakeShared<FJsonObject>();
        JsonClass->SetStringField(TEXT("symbol"), Class->GetPathName());
        JsonClass->SetStringField(TEXT("name"), Class->GetName());
        JsonClass->SetStringField(
            TEXT("super"),
            Class->GetSuperClass() != nullptr ? Class->GetSuperClass()->GetPathName() : FString());
        JsonClass->SetStringField(TEXT("module"), Class->GetOutermost()->GetName());
        JsonClass->SetStringField(TEXT("header"), Class->GetMetaData(TEXT("ModuleRelativePath")));

        TArray<UFunction*> Functions;
        for (TFieldIterator<UFunction> It(Class, EFieldIterationFlags::None); It; ++It)
        {
            Functions.Add(*It);
        }
        Functions.Sort([](const UFunction& Left, const UFunction& Right)
        {
            return Left.GetName() < Right.GetName();
        });
        TArray<TSharedPtr<FJsonValue>> JsonFunctions;
        for (const UFunction* Function : Functions)
        {
            const TSharedRef<FJsonObject> JsonFunction = MakeShared<FJsonObject>();
            JsonFunction->SetStringField(TEXT("symbol"), Function->GetPathName());
            JsonFunction->SetStringField(TEXT("name"), Function->GetName());
            JsonFunction->SetStringField(TEXT("header"), Function->GetMetaData(TEXT("ModuleRelativePath")));
            JsonFunction->SetBoolField(TEXT("blueprintCallable"), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
            JsonFunction->SetArrayField(TEXT("blueprintReferences"), ReverseLinks.FindRef(Function->GetPathName()));
            JsonFunctions.Add(MakeShared<FJsonValueObject>(JsonFunction));
        }
        JsonClass->SetArrayField(TEXT("functions"), JsonFunctions);

        TArray<const FProperty*> Properties;
        for (TFieldIterator<FProperty> It(Class, EFieldIterationFlags::None); It; ++It)
        {
            Properties.Add(*It);
        }
        Properties.Sort([](const FProperty& Left, const FProperty& Right)
        {
            return Left.GetName() < Right.GetName();
        });
        TArray<TSharedPtr<FJsonValue>> JsonProperties;
        for (const FProperty* Property : Properties)
        {
            const TSharedRef<FJsonObject> JsonProperty = MakeShared<FJsonObject>();
            JsonProperty->SetStringField(TEXT("symbol"), Class->GetPathName() + TEXT(":") + Property->GetName());
            JsonProperty->SetStringField(TEXT("name"), Property->GetName());
            JsonProperty->SetStringField(TEXT("type"), Property->GetCPPType());
            JsonProperties.Add(MakeShared<FJsonValueObject>(JsonProperty));
        }
        JsonClass->SetArrayField(TEXT("properties"), JsonProperties);
        JsonClass->SetArrayField(TEXT("blueprintReferences"), ReverseLinks.FindRef(Class->GetPathName()));
        JsonClasses.Add(MakeShared<FJsonValueObject>(JsonClass));
    }

    const TSharedRef<FJsonObject> Reflection = MakeShared<FJsonObject>();
    Reflection->SetStringField(TEXT("schema"), TEXT("com.ue-ring.usem.cpp-reflection"));
    Reflection->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
    Reflection->SetArrayField(TEXT("classes"), JsonClasses);

    TArray<TSharedPtr<FJsonValue>> JsonFiles;
    for (const FString& SourceFile : SourceFiles)
    {
        const TSharedRef<FJsonObject> File = MakeShared<FJsonObject>();
        File->SetStringField(TEXT("path"), ProjectRelative(SourceFile));
        File->SetNumberField(TEXT("size"), IFileManager::Get().FileSize(*SourceFile));
        JsonFiles.Add(MakeShared<FJsonValueObject>(File));
    }
    const TSharedRef<FJsonObject> Source = MakeShared<FJsonObject>();
    Source->SetStringField(TEXT("schema"), TEXT("com.ue-ring.usem.cpp-source"));
    Source->SetStringField(TEXT("schemaVersion"), UE_RING_SCHEMA_VERSION);
    Source->SetArrayField(TEXT("files"), JsonFiles);

    const FString CppDirectory = FPaths::Combine(FUERingExportManager::Get().GetOutputRoot(), TEXT("cpp"));
    if (!WriteJson(FPaths::Combine(CppDirectory, TEXT("reflection.uesem.json")), Reflection, OutError)
        || !WriteJson(FPaths::Combine(CppDirectory, TEXT("source-index.uesem.json")), Source, OutError))
    {
        return false;
    }
    return !GetDefault<UUERingSettings>()->bIncludeBlueprintMigrationReport
        || FUERingBlueprintMigrationReporter::Rebuild(OutError);
}

bool FUERingCppIndexer::UpdatePackages(
    const TArray<FName>& PackageNames,
    FString& OutError)
{
    using namespace UERingCppIndexer;

    if (PackageNames.IsEmpty())
    {
        return true;
    }

    const FString CppDirectory = FPaths::Combine(
        FUERingExportManager::Get().GetOutputRoot(),
        TEXT("cpp"));
    const FString ReflectionFile = FPaths::Combine(
        CppDirectory,
        TEXT("reflection.uesem.json"));
    TSharedPtr<FJsonObject> Reflection;
    const TArray<TSharedPtr<FJsonValue>>* StoredClasses = nullptr;
    FString Schema;
    FString SchemaVersion;
    TSet<FString> ProjectModules;
    if (!LoadJsonObject(ReflectionFile, Reflection)
        || !Reflection->TryGetStringField(TEXT("schema"), Schema)
        || Schema != TEXT("com.ue-ring.usem.cpp-reflection")
        || !Reflection->TryGetStringField(TEXT("schemaVersion"), SchemaVersion)
        || SchemaVersion != UE_RING_SCHEMA_VERSION
        || !Reflection->TryGetArrayField(TEXT("classes"), StoredClasses)
        || StoredClasses == nullptr
        || !LoadProjectModules(CppDirectory, ProjectModules))
    {
        return Rebuild(OutError);
    }

    TSet<FString> DirtyPackages;
    for (const FName PackageName : PackageNames)
    {
        DirtyPackages.Add(PackageName.ToString());
    }

    TArray<TSharedPtr<FJsonValue>> Classes = *StoredClasses;
    TMap<FString, TSharedPtr<FJsonObject>> Symbols;
    for (const TSharedPtr<FJsonValue>& ClassValue : Classes)
    {
        const TSharedPtr<FJsonObject>* ClassPtr = nullptr;
        FString ClassSymbol;
        const TArray<TSharedPtr<FJsonValue>>* Functions = nullptr;
        if (!ClassValue.IsValid()
            || !ClassValue->TryGetObject(ClassPtr)
            || ClassPtr == nullptr
            || !(*ClassPtr)->TryGetStringField(TEXT("symbol"), ClassSymbol)
            || !(*ClassPtr)->TryGetArrayField(TEXT("functions"), Functions)
            || Functions == nullptr
            || !FilterDirtyReferences((*ClassPtr).ToSharedRef(), DirtyPackages))
        {
            return Rebuild(OutError);
        }
        Symbols.Add(ClassSymbol, *ClassPtr);
        for (const TSharedPtr<FJsonValue>& FunctionValue : *Functions)
        {
            const TSharedPtr<FJsonObject>* FunctionPtr = nullptr;
            FString FunctionSymbol;
            if (!FunctionValue.IsValid()
                || !FunctionValue->TryGetObject(FunctionPtr)
                || FunctionPtr == nullptr
                || !(*FunctionPtr)->TryGetStringField(TEXT("symbol"), FunctionSymbol)
                || !FilterDirtyReferences((*FunctionPtr).ToSharedRef(), DirtyPackages))
            {
                return Rebuild(OutError);
            }
            Symbols.Add(FunctionSymbol, *FunctionPtr);
        }
    }

    FReverseLinks ChangedLinks;
    for (const FString& PackageName : DirtyPackages)
    {
        for (const bool bIsMap : { false, true })
        {
            const FString SemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(
                PackageName,
                bIsMap);
            if (IFileManager::Get().FileExists(*SemanticFile))
            {
                CollectReverseLinksFromFile(SemanticFile, ChangedLinks);
            }
        }
    }
    SortReverseLinks(ChangedLinks);

    for (const TPair<FString, TArray<TSharedPtr<FJsonValue>>>& Pair : ChangedLinks)
    {
        TSharedPtr<FJsonObject>* Target = Symbols.Find(Pair.Key);
        if (Target == nullptr)
        {
            // A class-level reverse link changes the class membership of the reflection index.
            // Rebuild in that uncommon case so native metadata is emitted completely.
            if (!Pair.Key.Contains(TEXT(":")))
            {
                return Rebuild(OutError);
            }
            continue;
        }
        const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
        if (!(*Target)->TryGetArrayField(TEXT("blueprintReferences"), Existing) || Existing == nullptr)
        {
            return Rebuild(OutError);
        }
        TArray<TSharedPtr<FJsonValue>> Updated = *Existing;
        Updated.Append(Pair.Value);
        SortReverseReferences(Updated);
        (*Target)->SetArrayField(TEXT("blueprintReferences"), Updated);
    }

    Classes.RemoveAll([&ProjectModules](const TSharedPtr<FJsonValue>& ClassValue)
    {
        const TSharedPtr<FJsonObject>* ClassPtr = nullptr;
        if (!ClassValue.IsValid() || !ClassValue->TryGetObject(ClassPtr) || ClassPtr == nullptr)
        {
            return false;
        }
        FString Module;
        (*ClassPtr)->TryGetStringField(TEXT("module"), Module);
        Module.RemoveFromStart(TEXT("/Script/"));
        const TArray<TSharedPtr<FJsonValue>>* References = nullptr;
        return !ProjectModules.Contains(Module)
            && (*ClassPtr)->TryGetArrayField(TEXT("blueprintReferences"), References)
            && References != nullptr
            && References->IsEmpty();
    });
    Classes.Sort([](const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right)
    {
        const TSharedPtr<FJsonObject>* LeftObject = nullptr;
        const TSharedPtr<FJsonObject>* RightObject = nullptr;
        FString LeftSymbol;
        FString RightSymbol;
        if (Left.IsValid() && Left->TryGetObject(LeftObject) && LeftObject != nullptr)
        {
            (*LeftObject)->TryGetStringField(TEXT("symbol"), LeftSymbol);
        }
        if (Right.IsValid() && Right->TryGetObject(RightObject) && RightObject != nullptr)
        {
            (*RightObject)->TryGetStringField(TEXT("symbol"), RightSymbol);
        }
        return LeftSymbol < RightSymbol;
    });
    Reflection->SetArrayField(TEXT("classes"), Classes);
    if (!WriteJson(ReflectionFile, Reflection.ToSharedRef(), OutError))
    {
        return false;
    }
    return !GetDefault<UUERingSettings>()->bIncludeBlueprintMigrationReport
        || FUERingBlueprintMigrationReporter::UpdatePackages(PackageNames, OutError);
}
