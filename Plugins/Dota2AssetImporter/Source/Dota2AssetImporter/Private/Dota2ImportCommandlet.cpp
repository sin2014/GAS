// 引入 Commandlet 的公共声明，包含 UDota2ImportCommandlet 类定义。
#include "Dota2ImportCommandlet.h"

// UAssetImportTask：编辑器自动化导入任务，封装源文件、目标路径、导入选项和工厂。
#include "AssetImportTask.h"
// AssetData：资产注册表返回的单个资产记录，用于 VerifyOnly 汇总导入结果。
#include "AssetRegistry/AssetData.h"
// AssetRegistry：资产注册表模块，用于让编辑器识别和刷新已导入资产。
#include "AssetRegistry/AssetRegistryModule.h"
// AssetTools：编辑器资产工具模块，负责批量导入和创建资产。
#include "AssetToolsModule.h"
// UAnimBlueprint：动画蓝图类型，用于把导入的角色网格绑定到指定动画蓝图。
#include "Animation/AnimBlueprint.h"
// UAnimSequence：动画序列类型，对应从 FBX 导入的动作资产。
#include "Animation/AnimSequence.h"
// UAnimationAsset：动画资产基类，用于兼容单节点预览动画。
#include "Animation/AnimationAsset.h"
#include "Animation/PreviewAssetAttachComponent.h"
#include "Animation/Skeleton.h"
// AnimationBlueprintLibrary：编辑器动画工具库，用来设置 Skeleton 的预览网格。
#include "AnimationBlueprintLibrary.h"
// USkeletalMeshComponent：蓝图组件模板类型，用于配置角色主体和分体部件。
#include "Components/SkeletalMeshComponent.h"
// Editor：提供编辑器全局能力和编辑器专用 API。
#include "Editor.h"
// UBlueprint：用于加载和修改目标角色蓝图资产。
#include "Engine/Blueprint.h"
// USCS_Node：蓝图 Simple Construction Script 节点，用于创建组件节点。
#include "Engine/SCS_Node.h"
// SimpleConstructionScript：蓝图组件树，用于查找和新增 SkeletalMeshComponent。
#include "Engine/SimpleConstructionScript.h"
// USkeletalMesh：骨骼网格体类型，对应角色身体、头、手臂等 FBX 导入结果。
#include "Engine/SkeletalMesh.h"
// UStaticMesh：静态网格体类型，对应 props 中的静态模型。
#include "Engine/StaticMesh.h"
// UTexture2D：贴图资产类型，用于导入和配置 color/normal/mask 贴图。
#include "Engine/Texture2D.h"
// FBX 动画导入数据，提供动画导入采样、曲线、骨骼轨道等设置。
#include "Factories/FbxAnimSequenceImportData.h"
// UFbxFactory：FBX 导入工厂，实际驱动 UE 的 FBX 导入流程。
#include "Factories/FbxFactory.h"
// UFbxImportUI：FBX 导入选项集合，控制导入类型、材质、贴图、骨架等。
#include "Factories/FbxImportUI.h"
// FBX 骨骼网格导入数据，提供骨骼网格专用导入设置。
#include "Factories/FbxSkeletalMeshImportData.h"
// FBX 静态网格导入数据，提供静态网格专用导入设置。
#include "Factories/FbxStaticMeshImportData.h"
// UMaterialFactoryNew：创建新材质资产时使用的工厂。
#include "Factories/MaterialFactoryNew.h"
// FileHelpers：提供保存脏包等编辑器文件辅助函数。
#include "FileHelpers.h"
// ACharacter：用于访问目标蓝图 CDO 上的原生 Mesh 组件。
#include "GameFramework/Character.h"
// IFileManager：创建验证报告输出目录。
#include "HAL/FileManager.h"
// IAssetTools：AssetTools 模块对外接口，执行导入任务和创建资产。
#include "IAssetTools.h"
// JsonObjectConverter：JSON/UStruct 转换工具；当前文件预留依赖，便于后续结构化解析。
#include "JsonObjectConverter.h"
// BlueprintEditorUtils：标记蓝图结构变化，保证组件树改动被编辑器识别。
#include "Kismet2/BlueprintEditorUtils.h"
// KismetEditorUtilities：编译蓝图，确保修改后的组件和动画设置生效。
#include "Kismet2/KismetEditorUtilities.h"
// UMaterial：材质资产类型，用于自动创建和重建材质节点图。
#include "Materials/Material.h"
// ComponentMask 材质表达式头；当前实现未显式创建该节点，保留给材质扩展使用。
#include "Materials/MaterialExpressionComponentMask.h"
// TextureSample 材质表达式，用于把导入贴图接入材质属性。
#include "Materials/MaterialExpressionTextureSample.h"
// MaterialEditingLibrary：编辑器材质图 API，用来创建表达式并连接材质属性。
#include "MaterialEditingLibrary.h"
// CommandLine：命令行相关工具；当前主要通过 FParse 解析 Params。
#include "Misc/CommandLine.h"
// FileHelper：读取 manifest JSON 文件文本。
#include "Misc/FileHelper.h"
// PackageName：包名/对象路径相关工具；当前保留给路径校验或扩展使用。
#include "Misc/PackageName.h"
// FParse：解析 Commandlet 参数中的 -Manifest=、-Dest=、-ReplaceExisting 等开关。
#include "Misc/Parse.h"
// ModuleManager：加载 AssetTools 等编辑器模块。
#include "Modules/ModuleManager.h"
// UPhysicsAsset：物理资产类型；骨骼网格导入时可让 UE 创建 PhysicsAsset。
#include "PhysicsEngine/PhysicsAsset.h"
// SkeletalMeshModel：骨骼网格渲染模型数据；当前保留给网格后处理扩展使用。
#include "Rendering/SkeletalMeshModel.h"
// JsonReader：把 manifest 字符串读成 JSON token 流。
#include "Serialization/JsonReader.h"
// JsonSerializer：把 manifest JSON 文本反序列化为 FJsonObject。
#include "Serialization/JsonSerializer.h"
// JsonWriter：把 VerifyOnly 检查结果写成 JSON 报告。
#include "Serialization/JsonWriter.h"
// UPackage：资产包类型；当前主要依赖 MarkPackageDirty/保存流程间接使用。
#include "UObject/Package.h"
// SavePackage：保存资产包相关接口；当前由 SaveDirtyPackages 统一保存。
#include "UObject/SavePackage.h"

// 定义本 Commandlet 的日志分类，后续 UE_LOG 会输出到 LogDota2ImportCommandlet。
DEFINE_LOG_CATEGORY_STATIC(LogDota2ImportCommandlet, Log, All);

// 匿名命名空间：把本文件的辅助结构和函数限制在当前 cpp 内，避免污染插件模块符号。
namespace
{
// manifest 中单个资源条目的内存表示。
// skeletal_meshes、animations、props 三类数组都会被解析成这个结构。
struct FDota2ManifestEntry
{
    // 导入后资产使用的名称，来自 manifest 的 name 字段。
    FString Name;

    // 源 FBX 文件或源资源文件的绝对路径，来自 manifest 的 file 字段。
    FString File;

    // 资源类型标记，主要用于 props 判断 static 或可作为骨骼 FX 导入。
    FString Type;

    // 动画目标骨架/网格名称；为空时默认使用主角色骨架。
    FString Target;

    // 可选导入子目录；为空时使用本工具原有的默认分类目录。
    FString DestSubdir;

    // 动画导出帧范围起点；仅当 manifest 提供 frame_start/frame_end 时有效。
    double FrameStart = 0.0;

    // 动画导出帧范围终点；用于跳过只有一帧的静态姿势条目。
    double FrameEnd = 0.0;

    // 是否同时成功读取 frame_start 和 frame_end。
    bool bHasFrameRange = false;
};

// 一组同名 Dota 风格贴图的集合。
// 例如 hero_body_color、hero_body_normal、hero_body_orm 会被归到同一个 Key。
struct FTextureSet
{
    // 分组键，通常是去掉 _color/_normal/_orm 等后缀后的贴图基础名。
    FString Key;

    // Base Color 贴图，按 sRGB 颜色贴图导入。
    UTexture2D* Color = nullptr;

    // Normal 贴图，按非 sRGB 法线贴图导入。
    UTexture2D* Normal = nullptr;

    // ORM 遮罩贴图，R=AO、G=Roughness、B=Metallic。
    UTexture2D* Orm = nullptr;

    // 高光遮罩贴图，当前取 R 通道连接到 Specular。
    UTexture2D* SpecMask = nullptr;

    // 细节遮罩贴图，当前只归组和设置压缩格式，尚未接入材质图。
    UTexture2D* DetailMask = nullptr;

    // 边缘光遮罩贴图，当前只归组和设置压缩格式，尚未接入材质图。
    UTexture2D* RimMask = nullptr;
};

// 清理 UE 资产名称：只保留字母、数字和下划线，合并重复下划线，并避免空名称。
FString CleanObjectName(FString Value)
{
    // 逐字符检查名称，把空格、横杠、点号等 UE 对象名不友好的字符替换成下划线。
    for (TCHAR& Ch : Value)
    {
        const bool bAllowed = FChar::IsAlnum(Ch) || Ch == TEXT('_');
        if (!bAllowed)
        {
            Ch = TEXT('_');
        }
    }

    // 连续下划线会降低可读性，也会影响后续模糊匹配，所以压缩成单个下划线。
    while (Value.Contains(TEXT("__")))
    {
        Value.ReplaceInline(TEXT("__"), TEXT("_"));
    }

    // 先去掉首尾空白，再去掉首尾下划线，得到更像 UE 资产名的结果。
    Value.TrimStartAndEndInline();
    bool bRemoved = false;
    do
    {
        bRemoved = false;
        Value.TrimCharInline(TEXT('_'), &bRemoved);
    }
    while (bRemoved);

    // 极端情况下如果清理后为空，给一个稳定兜底名，避免创建空对象名。
    return Value.IsEmpty() ? TEXT("Asset") : Value;
}

// 拼接 /Game 风格的 Content 路径，并处理左右两侧多余斜杠。
FString CombineContentPath(const FString& Left, const FString& Right)
{
    FString Result = Left;
    Result.RemoveFromEnd(TEXT("/"));
    FString Segment = Right;
    Segment.RemoveFromStart(TEXT("/"));
    return Result / Segment;
}

// 根据包路径和资产名构造 UE 对象路径：/Game/Path/Asset.Asset。
FString ObjectPath(const FString& PackagePath, const FString& AssetName)
{
    return PackagePath / AssetName + TEXT(".") + AssetName;
}

// manifest 条目可通过 dest_subdir 指定相对导入目录；旧 manifest 未提供时保持默认目录。
FString EntryDestPath(const FString& DefaultPath, const FString& DestRoot, const FDota2ManifestEntry& Entry)
{
    if (Entry.DestSubdir.IsEmpty())
    {
        return DefaultPath;
    }
    return CombineContentPath(DestRoot, Entry.DestSubdir);
}

// 返回文件名主干，不包含目录和扩展名。
FString FilenameStem(const FString& Path)
{
    return FPaths::GetBaseFilename(Path);
}

// 判断小写文件名是否是法线贴图。
bool IsNormalName(const FString& Lower)
{
    return Lower.Contains(TEXT("_normal"));
}

// 判断小写文件名是否是 ORM 遮罩贴图。
bool IsOrmName(const FString& Lower)
{
    return Lower.Contains(TEXT("_orm"));
}

// 判断小写文件名是否是高光遮罩贴图。
bool IsSpecMaskName(const FString& Lower)
{
    return Lower.Contains(TEXT("_specmask"));
}

// 判断小写文件名是否是细节遮罩贴图。
bool IsDetailMaskName(const FString& Lower)
{
    return Lower.Contains(TEXT("_detailmask"));
}

// 判断小写文件名是否是边缘光遮罩贴图。
bool IsRimMaskName(const FString& Lower)
{
    return Lower.Contains(TEXT("_rimmask"));
}

// Source2Viewer 会导出 diffuse/metalness/selfillum/tmasks 等遮罩族；这些也必须按线性 mask 贴图处理。
bool IsSource2AuxMaskName(const FString& Lower)
{
    return Lower.Contains(TEXT("_diffusemask"))
        || Lower.Contains(TEXT("_metalnessmask"))
        || Lower.Contains(TEXT("_selfillummask"))
        || Lower.Contains(TEXT("_tmasks"));
}

// 判断小写文件名是否是基础颜色贴图。
bool IsColorName(const FString& Lower)
{
    return Lower.Contains(TEXT("_color"));
}

// 从贴图名推导材质分组键，用于把同一部位的 color/normal/mask 归并成一个材质。
FString TextureGroupKey(const FString& TextureName)
{
    FString Lower = TextureName.ToLower();

    // 按后缀标记寻找最早出现的位置；最早标记之前的部分就是贴图组名。
    const TArray<FString> Markers =
    {
        TEXT("_diffusemask"),
        TEXT("_metalnessmask"),
        TEXT("_selfillummask"),
        TEXT("_vmat_g_tmasks"),
        TEXT("_tmasks"),
        TEXT("_detailmask"),
        TEXT("_specmask"),
        TEXT("_rimmask"),
        TEXT("_orm"),
        TEXT("_normal"),
        TEXT("_color")
    };

    // BestIndex 记录最靠前的标记位置；BestMarker 当前未参与后续逻辑，可用于调试扩展。
    int32 BestIndex = INDEX_NONE;
    FString BestMarker;
    for (const FString& Marker : Markers)
    {
        const int32 Found = Lower.Find(Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
        if (Found != INDEX_NONE && (BestIndex == INDEX_NONE || Found < BestIndex))
        {
            BestIndex = Found;
            BestMarker = Marker;
        }
    }

    // 找不到任何已知后缀时，整个文件名主干都作为分组键。
    if (BestIndex == INDEX_NONE)
    {
        return CleanObjectName(TextureName);
    }
    return CleanObjectName(TextureName.Left(BestIndex));
}

// 解析 -Name=Value 形式的命令行参数，并去掉外层引号。
FString ParseArgValue(const FString& Params, const TCHAR* Name)
{
    FString Value;
    FParse::Value(*Params, Name, Value);
    Value.TrimQuotesInline();
    return Value;
}

// 解析 -Switch 形式的布尔命令行开关。
bool ParseBoolSwitch(const FString& Params, const TCHAR* Name)
{
    return FParse::Param(*Params, Name);
}

// 解析浮点命令行参数；未提供时返回默认值。
float ParseFloatArg(const FString& Params, const TCHAR* Name, float DefaultValue)
{
    float Value = DefaultValue;
    FParse::Value(*Params, Name, Value);
    return Value;
}

// 从 manifest 根对象读取指定数组字段，并转换为 FDota2ManifestEntry 列表。
void ParseManifestArray(const TSharedPtr<FJsonObject>& Root, const FString& FieldName, TArray<FDota2ManifestEntry>& OutEntries)
{
    // 如果 manifest 没有这个数组字段，视为空数组，不把它当作解析错误。
    const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
    if (!Root->TryGetArrayField(FieldName, Array))
    {
        return;
    }

    // 数组中的非对象元素会被跳过，避免坏数据导致整个导入中断。
    for (const TSharedPtr<FJsonValue>& Value : *Array)
    {
        const TSharedPtr<FJsonObject> Obj = Value->AsObject();
        if (!Obj.IsValid())
        {
            continue;
        }

        // 字段读取失败时保留默认值；最终只有同时具备 name 和 file 的条目会被加入。
        FDota2ManifestEntry Entry;
        Obj->TryGetStringField(TEXT("name"), Entry.Name);
        Obj->TryGetStringField(TEXT("file"), Entry.File);
        Obj->TryGetStringField(TEXT("type"), Entry.Type);
        Obj->TryGetStringField(TEXT("target"), Entry.Target);
        Obj->TryGetStringField(TEXT("dest_subdir"), Entry.DestSubdir);

        // 使用单个 & 是为了确保两个字段都尝试读取；结果表示两者是否都存在。
        Entry.bHasFrameRange = Obj->TryGetNumberField(TEXT("frame_start"), Entry.FrameStart)
            & Obj->TryGetNumberField(TEXT("frame_end"), Entry.FrameEnd);
        if (!Entry.Name.IsEmpty() && !Entry.File.IsEmpty())
        {
            OutEntries.Add(Entry);
        }
    }
}

// 读取并解析 Blender 预处理步骤生成的 JSON manifest。
bool LoadManifest(const FString& ManifestPath, TSharedPtr<FJsonObject>& OutManifest)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *ManifestPath))
    {
        UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Could not read manifest: %s"), *ManifestPath);
        return false;
    }

    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, OutManifest) || !OutManifest.IsValid())
    {
        UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Could not parse manifest JSON: %s"), *ManifestPath);
        return false;
    }
    return true;
}

// 按 UE 对象路径同步加载任意 UObject。
UObject* LoadAssetByObjectPath(const FString& InObjectPath)
{
    return StaticLoadObject(UObject::StaticClass(), nullptr, *InObjectPath);
}

// 按 UE 对象路径加载资产并转换为指定类型，类型不匹配时返回 nullptr。
template <typename TObject>
TObject* LoadTypedAsset(const FString& InObjectPath)
{
    return Cast<TObject>(LoadAssetByObjectPath(InObjectPath));
}

// 创建一个同步、自动化的资产导入任务。
UAssetImportTask* MakeImportTask(const FString& File, const FString& DestPath, const FString& DestName, UObject* Options, UFactory* Factory, bool bReplaceExisting)
{
    // NewObject 创建临时任务对象，由 UE 对象系统管理生命周期。
    UAssetImportTask* Task = NewObject<UAssetImportTask>();

    // 源文件路径和目标 Content Browser 路径/资产名。
    Task->Filename = File;
    Task->DestinationPath = DestPath;
    Task->DestinationName = DestName;

    // 自动化导入不会弹出 FBX/贴图导入对话框。
    Task->bAutomated = true;

    // 是否覆盖已有资产由命令行 -ReplaceExisting 控制；覆盖时也替换旧导入设置。
    Task->bReplaceExisting = bReplaceExisting;
    Task->bReplaceExistingSettings = true;

    // 每个任务不单独保存，由流程最后统一保存所有脏包；导入保持同步，便于后续立即加载结果。
    Task->bSave = false;
    Task->bAsync = false;

    // Options/Factory 允许 FBX 使用自定义 UFbxImportUI；贴图导入传 nullptr 使用默认工厂。
    Task->Options = Options;
    Task->Factory = Factory;
    return Task;
}

// 执行单个导入任务。
void RunImportTask(UAssetImportTask* Task)
{
    // AssetTools 的接口接收任务数组，所以这里把单个任务包成一项数组。
    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    TArray<UAssetImportTask*> Tasks;
    Tasks.Add(Task);
    AssetToolsModule.Get().ImportAssetTasks(Tasks);
}

// 保存导入和修改过程中产生的所有脏内容包。
void SaveAllDirtyContent()
{
    // 第一个参数 false 表示不保存 map；第二个参数 true 表示保存 content packages。
    UEditorLoadingAndSavingUtils::SaveDirtyPackages(false, true);
}

// 根据贴图命名规则设置 sRGB 和压缩格式，避免材质采样类型错误。
void ConfigureTexture(UTexture2D* Texture, const FString& SourceName)
{
    if (!Texture)
    {
        return;
    }

    // 统一转小写后按 Dota/预处理脚本输出的后缀进行分类。
    const FString Lower = SourceName.ToLower();
    if (IsNormalName(Lower))
    {
        // 法线贴图不能使用 sRGB，并使用 UE 的法线压缩设置。
        Texture->SRGB = false;
        Texture->CompressionSettings = TC_Normalmap;
    }
    else if (IsOrmName(Lower) || IsSpecMaskName(Lower) || IsDetailMaskName(Lower) || IsRimMaskName(Lower) || IsSource2AuxMaskName(Lower))
    {
        // 遮罩贴图存放线性数据，关闭 sRGB 并使用 Masks 压缩以匹配 SAMPLERTYPE_Masks。
        Texture->SRGB = false;
        Texture->CompressionSettings = TC_Masks;
    }
    else
    {
        // 其他贴图按普通颜色贴图处理。
        Texture->SRGB = true;
        Texture->CompressionSettings = TC_Default;
    }

    // 通知编辑器该资产被修改，刷新渲染资源，并标记包需要保存。
    Texture->Modify();
    Texture->UpdateResource();
    Texture->MarkPackageDirty();
}

// 导入 manifest.textures 中列出的所有贴图，并按命名规则归组为材质输入。
TMap<FString, UTexture2D*> ImportTextures(
    const TSharedPtr<FJsonObject>& Manifest,
    const FString& TextureDestPath,
    TMap<FString, FTextureSet>& OutTextureSets,
    bool bReplaceExisting)
{
    // 返回值按源文件名记录导入后的 UTexture2D，OutTextureSets 则用于后续自动建材质。
    TMap<FString, UTexture2D*> ImportedTextures;
    const TSharedPtr<FJsonObject>* TextureObject = nullptr;
    if (!Manifest->TryGetObjectField(TEXT("textures"), TextureObject))
    {
        UE_LOG(LogDota2ImportCommandlet, Warning, TEXT("Manifest has no textures object."));
        return ImportedTextures;
    }

    // 先批量创建导入任务，再一次性提交给 AssetTools。
    TArray<UAssetImportTask*> Tasks;
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*TextureObject)->Values)
    {
        // textures 对象的 key 是源贴图名，value 是 Blender 步骤复制后的实际文件路径。
        const FString SourcePath = Pair.Value->AsString();
        if (!FPaths::FileExists(SourcePath))
        {
            UE_LOG(LogDota2ImportCommandlet, Warning, TEXT("Missing texture: %s"), *SourcePath);
            continue;
        }

        // UE 内资产名统一加 T_ 前缀，并清理非法字符。
        const FString SourceName = Pair.Key;
        const FString AssetName = CleanObjectName(TEXT("T_") + FilenameStem(SourceName));
        UAssetImportTask* Task = MakeImportTask(SourcePath, TextureDestPath, AssetName, nullptr, nullptr, bReplaceExisting);
        Tasks.Add(Task);
    }

    // 只有存在有效贴图文件时才调用导入，避免空任务数组产生无意义日志。
    if (Tasks.Num() > 0)
    {
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        AssetToolsModule.Get().ImportAssetTasks(Tasks);
    }

    // 导入完成后逐个取回 UTexture2D，配置压缩格式并填入贴图组。
    for (const UAssetImportTask* Task : Tasks)
    {
        const FString AssetName = Task->DestinationName;

        // 优先按目标对象路径加载；若资产名被导入器调整，则从任务返回对象兜底获取。
        UTexture2D* Texture = LoadTypedAsset<UTexture2D>(ObjectPath(TextureDestPath, AssetName));
        if (!Texture && Task->GetObjects().Num() > 0)
        {
            Texture = Cast<UTexture2D>(Task->GetObjects()[0]);
        }
        if (!Texture)
        {
            UE_LOG(LogDota2ImportCommandlet, Warning, TEXT("Texture import produced no UTexture2D: %s"), *Task->Filename);
            continue;
        }

        const FString SourceName = FPaths::GetCleanFilename(Task->Filename);
        ConfigureTexture(Texture, SourceName);
        ImportedTextures.Add(SourceName, Texture);

        // 归入材质贴图集合，后续每个集合会创建或更新一个材质。
        const FString Lower = SourceName.ToLower();
        const FString Key = TextureGroupKey(FilenameStem(SourceName));
        FTextureSet& Set = OutTextureSets.FindOrAdd(Key);
        Set.Key = Key;
        if (IsOrmName(Lower))
        {
            Set.Orm = Texture;
        }
        else if (IsNormalName(Lower))
        {
            Set.Normal = Texture;
        }
        else if (IsSpecMaskName(Lower))
        {
            Set.SpecMask = Texture;
        }
        else if (IsDetailMaskName(Lower) || IsSource2AuxMaskName(Lower))
        {
            Set.DetailMask = Texture;
        }
        else if (IsRimMaskName(Lower))
        {
            Set.RimMask = Texture;
        }
        else if (IsColorName(Lower))
        {
            Set.Color = Texture;
        }
    }

    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Imported/configured %d textures into %s."), ImportedTextures.Num(), *TextureDestPath);
    return ImportedTextures;
}

// 在材质图中创建一个 Texture Sample 节点，并设置贴图和采样类型。
UMaterialExpressionTextureSample* AddTextureSample(UMaterial* Material, UTexture2D* Texture, EMaterialSamplerType SamplerType, int32 X, int32 Y)
{
    if (!Material || !Texture)
    {
        return nullptr;
    }

    // CreateMaterialExpression 会把节点加入材质表达式数组；X/Y 只影响材质编辑器里的节点位置。
    UMaterialExpressionTextureSample* Sample = Cast<UMaterialExpressionTextureSample>(
        UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureSample::StaticClass(), X, Y));
    if (!Sample)
    {
        return nullptr;
    }

    Sample->Texture = Texture;
    Sample->SamplerType = SamplerType;
    return Sample;
}

// 将 Texture Sample 的指定输出通道连接到材质属性。
void ConnectTextureSample(UMaterialExpressionTextureSample* Sample, const FString& OutputName, EMaterialProperty Property)
{
    if (Sample)
    {
        UMaterialEditingLibrary::ConnectMaterialProperty(Sample, OutputName, Property);
    }
}

// 基于一组贴图创建或重建材质，并按 PBR 规则连接常用通道。
UMaterial* CreateOrUpdateMaterial(
    const FString& MaterialDestPath,
    const FString& Character,
    const FTextureSet& Set,
    bool bReplaceExisting)
{
    // 没有任何能接入当前材质图的贴图时，不创建空材质。
    if (!Set.Color && !Set.Normal && !Set.Orm && !Set.SpecMask)
    {
        return nullptr;
    }

    // 材质命名格式：M_角色名_贴图组名。
    const FString MaterialName = CleanObjectName(TEXT("M_") + Character + TEXT("_") + Set.Key);
    const FString MatObjectPath = ObjectPath(MaterialDestPath, MaterialName);
    UMaterial* Material = LoadTypedAsset<UMaterial>(MatObjectPath);

    if (!Material)
    {
        // 目标材质不存在时，用 MaterialFactoryNew 创建新 UMaterial 资产。
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
        Material = Cast<UMaterial>(AssetToolsModule.Get().CreateAsset(MaterialName, MaterialDestPath, UMaterial::StaticClass(), Factory, NAME_None, bReplaceExisting));
    }

    if (!Material)
    {
        UE_LOG(LogDota2ImportCommandlet, Warning, TEXT("Could not create material for texture set %s"), *Set.Key);
        return nullptr;
    }

    // 重新生成材质图，保证重复运行 Commandlet 时材质节点不会累积。
    Material->Modify();
    UMaterialEditingLibrary::DeleteAllMaterialExpressions(Material);

    // Dota 角色部件常见双面面片，默认生成不透明双面材质。
    Material->BlendMode = BLEND_Opaque;
    Material->TwoSided = true;

    // 声明材质可用于骨骼网格和静态网格，避免赋值后出现 usage 警告或运行时重编译。
    Material->SetUsageByFlag(MATUSAGE_SkeletalMesh, true);
    Material->SetUsageByFlag(MATUSAGE_StaticMesh, true);

    // 创建材质图节点。X/Y 是编辑器材质图中的节点摆放坐标，只影响可读性。
    UMaterialExpressionTextureSample* Color = AddTextureSample(Material, Set.Color, SAMPLERTYPE_Color, -700, -260);
    UMaterialExpressionTextureSample* Normal = AddTextureSample(Material, Set.Normal, SAMPLERTYPE_Normal, -700, 30);
    UMaterialExpressionTextureSample* Orm = AddTextureSample(Material, Set.Orm, SAMPLERTYPE_Masks, -700, 300);
    UMaterialExpressionTextureSample* Spec = AddTextureSample(Material, Set.SpecMask, SAMPLERTYPE_Masks, -700, 560);

    // PBR 通道连接：颜色接 BaseColor，法线接 Normal，ORM 拆通道接 AO/Roughness/Metallic。
    ConnectTextureSample(Color, TEXT("RGB"), MP_BaseColor);
    ConnectTextureSample(Normal, TEXT("RGB"), MP_Normal);
    ConnectTextureSample(Orm, TEXT("R"), MP_AmbientOcclusion);
    ConnectTextureSample(Orm, TEXT("G"), MP_Roughness);
    ConnectTextureSample(Orm, TEXT("B"), MP_Metallic);
    ConnectTextureSample(Spec, TEXT("R"), MP_Specular);

    // 触发材质编辑器更新和重编译，并标记包需要保存。
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();

    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Material %s created/updated from texture set %s"), *MaterialName, *Set.Key);
    return Material;
}

// 为所有贴图组批量创建材质，并用小写分组键建立查找表。
TMap<FString, UMaterial*> CreateMaterials(
    const FString& MaterialDestPath,
    const FString& Character,
    const TMap<FString, FTextureSet>& TextureSets,
    bool bReplaceExisting)
{
    TMap<FString, UMaterial*> Result;
    for (const TPair<FString, FTextureSet>& Pair : TextureSets)
    {
        // 只有成功创建/更新的材质才加入结果，避免后续槽位匹配到 nullptr。
        UMaterial* Material = CreateOrUpdateMaterial(MaterialDestPath, Character, Pair.Value, bReplaceExisting);
        if (Material)
        {
            Result.Add(Pair.Key.ToLower(), Material);
        }
    }
    return Result;
}

// 根据网格材质槽名称，从材质查找表中选择最匹配的材质。
UMaterial* FindBestMaterialForSlot(const FString& SlotName, const TMap<FString, UMaterial*>& MaterialsByKey)
{
    // 材质槽和贴图组都做清理/小写化，降低命名差异带来的匹配失败。
    const FString LowerSlot = CleanObjectName(SlotName.ToLower());
    int32 BestScore = 0;
    UMaterial* Best = nullptr;
    for (const TPair<FString, UMaterial*>& Pair : MaterialsByKey)
    {
        const FString Key = CleanObjectName(Pair.Key.ToLower());
        if (Key.IsEmpty())
        {
            continue;
        }

        // 槽名包含贴图组名时给高分；贴图组名包含槽名时给低一些的兜底分。
        int32 Score = 0;
        if (LowerSlot.Contains(Key))
        {
            Score = Key.Len() * 10;
        }
        else if (Key.Contains(LowerSlot))
        {
            Score = LowerSlot.Len();
        }

        if (Score > BestScore)
        {
            BestScore = Score;
            Best = Pair.Value;
        }
    }
    return Best;
}

// 为骨骼网格体的每个材质槽自动匹配并赋予生成的材质。
void AssignMaterialsToSkeletalMesh(USkeletalMesh* Mesh, const TMap<FString, UMaterial*>& MaterialsByKey)
{
    if (!Mesh)
    {
        return;
    }

    // 复制材质槽数组进行修改，最后一次性写回 USkeletalMesh。
    TArray<FSkeletalMaterial> Materials = Mesh->GetMaterials();
    int32 Assigned = 0;
    for (FSkeletalMaterial& Slot : Materials)
    {
        FString SlotName = Slot.MaterialSlotName.ToString();
#if WITH_EDITORONLY_DATA
        // 导入材质槽名有时比运行时槽名更接近源 Dota 材质名，因此一起参与匹配。
        if (!Slot.ImportedMaterialSlotName.IsNone())
        {
            SlotName += TEXT("_") + Slot.ImportedMaterialSlotName.ToString();
        }
#endif
        if (UMaterial* Material = FindBestMaterialForSlot(SlotName, MaterialsByKey))
        {
            Slot.MaterialInterface = Material;
            Assigned++;
        }
    }
    Mesh->SetMaterials(Materials);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Assigned %d/%d material slots on skeletal mesh %s."), Assigned, Materials.Num(), *Mesh->GetName());
}

// 为静态网格体的每个材质槽自动匹配并赋予生成的材质。
void AssignMaterialsToStaticMesh(UStaticMesh* Mesh, const TMap<FString, UMaterial*>& MaterialsByKey)
{
    if (!Mesh)
    {
        return;
    }

    // 静态网格材质槽类型是 FStaticMaterial，流程与骨骼网格类似。
    TArray<FStaticMaterial> Materials = Mesh->GetStaticMaterials();
    int32 Assigned = 0;
    for (FStaticMaterial& Slot : Materials)
    {
        FString SlotName = Slot.MaterialSlotName.ToString();
#if WITH_EDITORONLY_DATA
        // 导入材质槽名有时比运行时槽名更接近源 Dota 材质名，因此一起参与匹配。
        if (!Slot.ImportedMaterialSlotName.IsNone())
        {
            SlotName += TEXT("_") + Slot.ImportedMaterialSlotName.ToString();
        }
#endif
        if (UMaterial* Material = FindBestMaterialForSlot(SlotName, MaterialsByKey))
        {
            Slot.MaterialInterface = Material;
            Assigned++;
        }
    }
    Mesh->SetStaticMaterials(Materials);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Assigned %d/%d material slots on static mesh %s."), Assigned, Materials.Num(), *Mesh->GetName());
}

// 创建骨骼网格 FBX 导入选项。
UFbxImportUI* CreateSkeletalImportOptions(USkeleton* Skeleton, bool bCreatePhysicsAsset, const FRotator& ImportRotation, float ImportUniformScale)
{
    // 明确指定导入类型，避免自动检测把骨骼网格误判成其他类型。
    UFbxImportUI* ImportUI = NewObject<UFbxImportUI>();
    ImportUI->MeshTypeToImport = FBXIT_SkeletalMesh;
    ImportUI->OriginalImportType = FBXIT_SkeletalMesh;
    ImportUI->bAutomatedImportShouldDetectType = false;
    ImportUI->bImportAsSkeletal = true;
    ImportUI->bImportMesh = true;

    // 首个主网格传入空 Skeleton 并创建新 Skeleton/PhysicsAsset；后续分体网格复用主 Skeleton。
    ImportUI->Skeleton = Skeleton;
    ImportUI->bCreatePhysicsAsset = bCreatePhysicsAsset;

    // 本工具单独处理动画、材质和贴图，所以骨骼网格导入时只导入 mesh 本体。
    ImportUI->bImportAnimations = false;
    ImportUI->bImportMaterials = false;
    ImportUI->bImportTextures = false;
    ImportUI->bOverrideFullName = true;

    // 骨骼网格导入细节设置：应用旋转/缩放修正，保留顶点色、平滑组和变形目标。
    if (ImportUI->SkeletalMeshImportData)
    {
        ImportUI->SkeletalMeshImportData->ImportRotation = ImportRotation;
        ImportUI->SkeletalMeshImportData->ImportUniformScale = ImportUniformScale;
        ImportUI->SkeletalMeshImportData->ImportContentType = FBXICT_All;
        ImportUI->SkeletalMeshImportData->bImportMorphTargets = true;
        ImportUI->SkeletalMeshImportData->bPreserveSmoothingGroups = true;
        ImportUI->SkeletalMeshImportData->bImportMeshesInBoneHierarchy = false;
        ImportUI->SkeletalMeshImportData->bUpdateSkeletonReferencePose = false;
        ImportUI->SkeletalMeshImportData->bUseT0AsRefPose = false;
        ImportUI->SkeletalMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
    }

    return ImportUI;
}

// 创建静态网格 FBX 导入选项。
UFbxImportUI* CreateStaticImportOptions(const FRotator& ImportRotation, float ImportUniformScale)
{
    // 明确按静态网格导入，并关闭材质/贴图自动导入，因为材质由本工具统一生成。
    UFbxImportUI* ImportUI = NewObject<UFbxImportUI>();
    ImportUI->MeshTypeToImport = FBXIT_StaticMesh;
    ImportUI->OriginalImportType = FBXIT_StaticMesh;
    ImportUI->bAutomatedImportShouldDetectType = false;
    ImportUI->bImportAsSkeletal = false;
    ImportUI->bImportMesh = true;
    ImportUI->bImportAnimations = false;
    ImportUI->bImportMaterials = false;
    ImportUI->bImportTextures = false;
    ImportUI->bOverrideFullName = true;

    // 静态网格导入时应用旋转/缩放修正，合并子网格，生成光照 UV 和简单碰撞。
    if (ImportUI->StaticMeshImportData)
    {
        ImportUI->StaticMeshImportData->ImportRotation = ImportRotation;
        ImportUI->StaticMeshImportData->ImportUniformScale = ImportUniformScale;
        ImportUI->StaticMeshImportData->bCombineMeshes = true;
        ImportUI->StaticMeshImportData->bGenerateLightmapUVs = true;
        ImportUI->StaticMeshImportData->bAutoGenerateCollision = true;
        ImportUI->StaticMeshImportData->bBuildNanite = false;
        ImportUI->StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
    }

    return ImportUI;
}

// 创建动画 FBX 导入选项。
UFbxImportUI* CreateAnimationImportOptions(USkeleton* Skeleton, const FRotator& ImportRotation, float ImportUniformScale)
{
    // 动画导入只导入动画轨道，不导入网格、材质或贴图。
    UFbxImportUI* ImportUI = NewObject<UFbxImportUI>();
    ImportUI->MeshTypeToImport = FBXIT_Animation;
    ImportUI->OriginalImportType = FBXIT_Animation;
    ImportUI->bAutomatedImportShouldDetectType = false;
    ImportUI->bImportAsSkeletal = true;
    ImportUI->bImportMesh = false;
    ImportUI->Skeleton = Skeleton;
    ImportUI->bCreatePhysicsAsset = false;
    ImportUI->bImportAnimations = true;
    ImportUI->bImportMaterials = false;
    ImportUI->bImportTextures = false;
    ImportUI->bOverrideFullName = true;

    // 动画导入设置：使用 FBX 导出的时间范围，保留骨骼轨道，避免自动删减关键帧。
    if (ImportUI->AnimSequenceImportData)
    {
        ImportUI->AnimSequenceImportData->ImportRotation = ImportRotation;
        ImportUI->AnimSequenceImportData->ImportUniformScale = ImportUniformScale;
        ImportUI->AnimSequenceImportData->AnimationLength = FBXALIT_ExportedTime;
        ImportUI->AnimSequenceImportData->bImportBoneTracks = true;
        ImportUI->AnimSequenceImportData->bUseDefaultSampleRate = false;
        ImportUI->AnimSequenceImportData->CustomSampleRate = 0;
        ImportUI->AnimSequenceImportData->bSnapToClosestFrameBoundary = false;
        ImportUI->AnimSequenceImportData->bPreserveLocalTransform = false;
        ImportUI->AnimSequenceImportData->bRemoveRedundantKeys = false;
        ImportUI->AnimSequenceImportData->bDoNotImportCurveWithZero = false;
        ImportUI->AnimSequenceImportData->bImportCustomAttribute = false;
    }

    return ImportUI;
}

// 用指定 ImportUI 创建 FBX 工厂，并关闭导入类型自动检测。
UFbxFactory* CreateFbxFactory(UFbxImportUI* ImportUI)
{
    UFbxFactory* Factory = NewObject<UFbxFactory>();
    Factory->ImportUI = ImportUI;
    Factory->SetDetectImportTypeOnImport(false);
    return Factory;
}

// 导入一个骨骼网格条目，并在导入后自动赋予匹配材质。
USkeletalMesh* ImportSkeletalMeshEntry(
    const FDota2ManifestEntry& Entry,
    const FString& DestPath,
    USkeleton* Skeleton,
    bool bCreatePhysicsAsset,
    bool bReplaceExisting,
    const TMap<FString, UMaterial*>& MaterialsByKey,
    const FRotator& ImportRotation,
    float ImportUniformScale)
{
    if (!FPaths::FileExists(Entry.File))
    {
        UE_LOG(LogDota2ImportCommandlet, Warning, TEXT("Missing skeletal FBX: %s"), *Entry.File);
        return nullptr;
    }

    // 使用传入 Skeleton 时导入为同骨架分体；Skeleton 为空时创建新的骨架资产。
    UFbxImportUI* ImportUI = CreateSkeletalImportOptions(Skeleton, bCreatePhysicsAsset, ImportRotation, ImportUniformScale);
    UFbxFactory* Factory = CreateFbxFactory(ImportUI);
    UAssetImportTask* Task = MakeImportTask(Entry.File, DestPath, CleanObjectName(Entry.Name), ImportUI, Factory, bReplaceExisting);
    RunImportTask(Task);

    // 优先按预期对象路径加载；如果导入器生成了不同对象名，则遍历任务返回对象兜底。
    USkeletalMesh* Mesh = LoadTypedAsset<USkeletalMesh>(ObjectPath(DestPath, CleanObjectName(Entry.Name)));
    if (!Mesh && Task->GetObjects().Num() > 0)
    {
        for (UObject* Obj : Task->GetObjects())
        {
            if ((Mesh = Cast<USkeletalMesh>(Obj)))
            {
                break;
            }
        }
    }

    AssignMaterialsToSkeletalMesh(Mesh, MaterialsByKey);
    return Mesh;
}

// 导入一个静态网格条目，并在导入后自动赋予匹配材质。
UStaticMesh* ImportStaticMeshEntry(
    const FDota2ManifestEntry& Entry,
    const FString& DestPath,
    bool bReplaceExisting,
    const TMap<FString, UMaterial*>& MaterialsByKey,
    const FRotator& ImportRotation,
    float ImportUniformScale)
{
    if (!FPaths::FileExists(Entry.File))
    {
        UE_LOG(LogDota2ImportCommandlet, Warning, TEXT("Missing static FBX: %s"), *Entry.File);
        return nullptr;
    }

    UFbxImportUI* ImportUI = CreateStaticImportOptions(ImportRotation, ImportUniformScale);
    UFbxFactory* Factory = CreateFbxFactory(ImportUI);
    UAssetImportTask* Task = MakeImportTask(Entry.File, DestPath, CleanObjectName(Entry.Name), ImportUI, Factory, bReplaceExisting);
    RunImportTask(Task);

    // 优先按预期对象路径加载；如果导入器生成了不同对象名，则遍历任务返回对象兜底。
    UStaticMesh* Mesh = LoadTypedAsset<UStaticMesh>(ObjectPath(DestPath, CleanObjectName(Entry.Name)));
    if (!Mesh && Task->GetObjects().Num() > 0)
    {
        for (UObject* Obj : Task->GetObjects())
        {
            if ((Mesh = Cast<UStaticMesh>(Obj)))
            {
                break;
            }
        }
    }

    AssignMaterialsToStaticMesh(Mesh, MaterialsByKey);
    return Mesh;
}

// 导入一个动画条目到指定 Skeleton。
UAnimSequence* ImportAnimationEntry(
    const FDota2ManifestEntry& Entry,
    const FString& DestPath,
    USkeleton* Skeleton,
    bool bReplaceExisting,
    const FRotator& ImportRotation,
    float ImportUniformScale)
{
    // 动画必须绑定到 Skeleton；找不到目标骨架时跳过，避免导入失败弹窗或产生无效资产。
    if (!Skeleton)
    {
        UE_LOG(LogDota2ImportCommandlet, Warning, TEXT("Skipping animation without skeleton: %s"), *Entry.Name);
        return nullptr;
    }

    // Blender/Source2 导出里可能包含只有一帧的姿势或静态条目，这里跳过它们。
    if (Entry.bHasFrameRange && Entry.FrameEnd - Entry.FrameStart < 1.0)
    {
        UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Skipping one-frame/static pose entry: %s (%0.3f-%0.3f)"), *Entry.Name, Entry.FrameStart, Entry.FrameEnd);
        return nullptr;
    }

    if (!FPaths::FileExists(Entry.File))
    {
        UE_LOG(LogDota2ImportCommandlet, Warning, TEXT("Missing animation FBX: %s"), *Entry.File);
        return nullptr;
    }

    UFbxImportUI* ImportUI = CreateAnimationImportOptions(Skeleton, ImportRotation, ImportUniformScale);
    UFbxFactory* Factory = CreateFbxFactory(ImportUI);
    UAssetImportTask* Task = MakeImportTask(Entry.File, DestPath, CleanObjectName(Entry.Name), ImportUI, Factory, bReplaceExisting);
    RunImportTask(Task);

    // 优先按预期对象路径加载；如果导入器生成了不同对象名，则遍历任务返回对象兜底。
    UAnimSequence* Sequence = LoadTypedAsset<UAnimSequence>(ObjectPath(DestPath, CleanObjectName(Entry.Name)));
    if (!Sequence && Task->GetObjects().Num() > 0)
    {
        for (UObject* Obj : Task->GetObjects())
        {
            if ((Sequence = Cast<UAnimSequence>(Obj)))
            {
                break;
            }
        }
    }
    if (Sequence)
    {
        // 动画本身没有额外重建逻辑，但导入后仍标记为需要保存。
        Sequence->MarkPackageDirty();
    }
    return Sequence;
}

// 设置 Skeleton 和导入动画的预览网格，方便在编辑器中直接查看动画。
void SetPreviewMeshes(USkeleton* MasterSkeleton, USkeletalMesh* PreviewMesh, const TArray<UAnimSequence*>& Animations)
{
    if (!PreviewMesh)
    {
        return;
    }

    // Skeleton 预览网格用于 Skeleton 编辑器和默认动画预览。
    if (MasterSkeleton)
    {
        UAnimationBlueprintLibrary::SetSkeletonPreviewMesh(MasterSkeleton, PreviewMesh, true);
        MasterSkeleton->MarkPackageDirty();
        UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Skeleton preview mesh set to %s"), *PreviewMesh->GetPathName());
    }

    // 每个 AnimSequence 也单独设置预览网格，避免打开动画时显示为空或错网格。
    int32 Updated = 0;
    for (UAnimSequence* Sequence : Animations)
    {
        if (!Sequence || Sequence->GetSkeleton() != MasterSkeleton)
        {
            continue;
        }
        Sequence->SetPreviewMesh(PreviewMesh, true);
        Sequence->MarkPackageDirty();
        Updated++;
    }

    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Animation preview mesh set on %d sequences."), Updated);
}

// 在蓝图组件树中查找指定 SkeletalMeshComponent 节点，不存在则创建。
USCS_Node* FindOrCreateSkeletalMeshNode(UBlueprint* Blueprint, const FName NodeName)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return nullptr;
    }

    if (USCS_Node* Existing = Blueprint->SimpleConstructionScript->FindSCSNode(NodeName))
    {
        return Existing;
    }

    // 创建的节点会出现在蓝图的 Simple Construction Script 组件树中。
    USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(USkeletalMeshComponent::StaticClass(), NodeName);
    Blueprint->SimpleConstructionScript->AddNode(Node);
    return Node;
}

// 从 SCS 节点取出组件模板，并转换为 USkeletalMeshComponent。
USkeletalMeshComponent* GetSkeletalComponentTemplate(USCS_Node* Node)
{
    return Node ? Cast<USkeletalMeshComponent>(Node->ComponentTemplate) : nullptr;
}

// 配置一个分体跟随组件：使用自己的网格，但姿势跟随主 Mesh 组件。
void ConfigureFollowerComponent(USkeletalMeshComponent* Component, USkeletalMeshComponent* Leader, USkeletalMesh* Mesh)
{
    if (!Component || !Leader)
    {
        return;
    }

    // 分体部件与主网格同原点、同旋转、同缩放，避免因组件偏移造成错位。
    Component->Modify();
    Component->SetRelativeLocation(FVector::ZeroVector);
    Component->SetRelativeRotation(FRotator::ZeroRotator);
    Component->SetRelativeScale3D(FVector::OneVector);

    // 分体外观组件不参与碰撞，碰撞交给角色主体或胶囊体处理。
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetSkeletalMesh(Mesh, false);
    Component->SetAnimationMode(EAnimationMode::AnimationBlueprint);

    // LeaderPose 让头、手臂、肩膀、翅膀等部件复用主体骨骼姿势。
    Component->SetLeaderPoseComponent(Leader, true, false);
    Component->bUseBoundsFromLeaderPoseComponent = true;
}

// 从 SkeletalMeshes 目录按固定资产名加载 ShadowFiend 分体网格。
USkeletalMesh* LoadShadowFiendMesh(const FString& SkeletalPath, const FString& Name)
{
    return LoadTypedAsset<USkeletalMesh>(ObjectPath(SkeletalPath, Name));
}

// 针对 ShadowFiend Arcana 的项目定制蓝图配置：设置主体网格、动画和分体跟随组件。
void ConfigureShadowFiendBlueprint(
    const FString& BlueprintObjectPath,
    const FString& SkeletalPath,
    const FString& AnimBlueprintObjectPath,
    const FString& IdleAnimObjectPath)
{
    // 命令行未传 -Blueprint 时跳过蓝图后处理，只完成资产导入。
    if (BlueprintObjectPath.IsEmpty())
    {
        return;
    }

    // BlueprintObjectPath 需要是完整对象路径，例如 /Game/.../BP_Name.BP_Name。
    UBlueprint* Blueprint = LoadTypedAsset<UBlueprint>(BlueprintObjectPath);
    if (!Blueprint || !Blueprint->SimpleConstructionScript || !Blueprint->GeneratedClass)
    {
        UE_LOG(LogDota2ImportCommandlet, Warning, TEXT("Could not load blueprint for ShadowFiend setup: %s"), *BlueprintObjectPath);
        return;
    }

    Blueprint->Modify();

    // 通过蓝图生成类的 CDO 拿到 ACharacter 自带的 Mesh 组件模板。
    ACharacter* CharacterCDO = Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject());
    USkeletalMeshComponent* MainMesh = CharacterCDO ? CharacterCDO->GetMesh() : nullptr;
    if (!MainMesh)
    {
        UE_LOG(LogDota2ImportCommandlet, Warning, TEXT("Blueprint has no native skeletal mesh component: %s"), *BlueprintObjectPath);
        return;
    }

    // 这些资产名来自当前 ShadowFiend 预处理/导入命名约定。
    USkeletalMesh* Body = LoadShadowFiendMesh(SkeletalPath, TEXT("SK_ShadowFiend_Arcana_Body"));
    USkeletalMesh* Head = LoadShadowFiendMesh(SkeletalPath, TEXT("SK_ShadowFiend_Arcana_Head"));
    USkeletalMesh* Arms = LoadShadowFiendMesh(SkeletalPath, TEXT("SK_ShadowFiend_Arcana_Arms"));
    USkeletalMesh* Shoulders = LoadShadowFiendMesh(SkeletalPath, TEXT("SK_ShadowFiend_Arcana_Shoulders"));
    USkeletalMesh* Wings = LoadShadowFiendMesh(SkeletalPath, TEXT("SK_ShadowFiend_Arcana_Wings"));
    UAnimBlueprint* AnimBlueprint = AnimBlueprintObjectPath.IsEmpty() ? nullptr : LoadTypedAsset<UAnimBlueprint>(AnimBlueprintObjectPath);
    UAnimationAsset* IdleAnim = IdleAnimObjectPath.IsEmpty() ? nullptr : LoadTypedAsset<UAnimationAsset>(IdleAnimObjectPath);

    // 主 Mesh 下移到 UE Character 胶囊体中常见的位置；具体偏移是当前角色资源的适配值。
    MainMesh->Modify();
    MainMesh->SetRelativeLocation(FVector(0.0, 0.0, -88.0));
    MainMesh->SetRelativeRotation(FRotator::ZeroRotator);
    MainMesh->SetRelativeScale3D(FVector(1.0, 1.0, 1.0));
    MainMesh->SetSkeletalMesh(Body, false);
    MainMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    // 优先使用动画蓝图；没有动画蓝图时可以退回单个 Idle 动画预览。
    if (AnimBlueprint && AnimBlueprint->GeneratedClass)
    {
        MainMesh->SetAnimInstanceClass(AnimBlueprint->GeneratedClass);
        MainMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    }
    else if (IdleAnim)
    {
        MainMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        MainMesh->SetAnimation(IdleAnim);
    }

    // ShadowFiend 分体部件列表：每个部件会在蓝图里创建一个跟随主 Mesh 的组件。
    TArray<TPair<FName, USkeletalMesh*>> Parts;
    Parts.Add(TPair<FName, USkeletalMesh*>(TEXT("Dota2Head"), Head));
    Parts.Add(TPair<FName, USkeletalMesh*>(TEXT("Dota2Arms"), Arms));
    Parts.Add(TPair<FName, USkeletalMesh*>(TEXT("Dota2Shoulders"), Shoulders));
    Parts.Add(TPair<FName, USkeletalMesh*>(TEXT("Dota2Wings"), Wings));

    // 确保每个分体组件存在，并把组件挂到主 Mesh 下。
    for (const TPair<FName, USkeletalMesh*>& Part : Parts)
    {
        USCS_Node* Node = FindOrCreateSkeletalMeshNode(Blueprint, Part.Key);
        if (!Node)
        {
            continue;
        }
        Node->SetParent(MainMesh);
        ConfigureFollowerComponent(GetSkeletalComponentTemplate(Node), MainMesh, Part.Value);
    }

    // 组件树变化属于蓝图结构变化，需要标记并重新编译蓝图。
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    Blueprint->MarkPackageDirty();
    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Configured modular ShadowFiend blueprint: %s"), *BlueprintObjectPath);
}

// VerifyOnly 的单条检查结果。失败项会让 Commandlet 返回非 0，便于命令行自动化使用。
void AddVerifyCheck(TArray<TSharedPtr<FJsonValue>>& Checks, int32& ErrorCount, const FString& Name, bool bPassed, const FString& Details)
{
    TSharedPtr<FJsonObject> Check = MakeShared<FJsonObject>();
    Check->SetStringField(TEXT("name"), Name);
    Check->SetBoolField(TEXT("passed"), bPassed);
    Check->SetStringField(TEXT("details"), Details);
    Checks.Add(MakeShared<FJsonValueObject>(Check));

    if (bPassed)
    {
        UE_LOG(LogDota2ImportCommandlet, Display, TEXT("VERIFY OK: %s - %s"), *Name, *Details);
    }
    else
    {
        ErrorCount++;
        UE_LOG(LogDota2ImportCommandlet, Error, TEXT("VERIFY FAILED: %s - %s"), *Name, *Details);
    }
}

// 加载指定资产并把加载结果写入验证报告。
template <typename TObject>
TObject* VerifyLoadAsset(TArray<TSharedPtr<FJsonValue>>& Checks, int32& ErrorCount, const FString& Label, const FString& PackagePath, const FString& AssetName)
{
    const FString Path = ObjectPath(PackagePath, AssetName);
    TObject* Asset = LoadTypedAsset<TObject>(Path);
    AddVerifyCheck(Checks, ErrorCount, Label, Asset != nullptr, Path);
    return Asset;
}

FString PathOrNone(const UObject* Object)
{
    return Object ? Object->GetPathName() : TEXT("<none>");
}

void AddSkeletonBindingCheck(
    TArray<TSharedPtr<FJsonValue>>& Checks,
    int32& ErrorCount,
    const FString& Name,
    USkeletalMesh* Mesh,
    USkeleton* ExpectedSkeleton)
{
    const bool bPassed = Mesh && ExpectedSkeleton && Mesh->GetSkeleton() == ExpectedSkeleton;
    const FString Details = FString::Printf(
        TEXT("mesh=%s mesh_skeleton=%s expected=%s"),
        *PathOrNone(Mesh),
        Mesh && Mesh->GetSkeleton() ? *Mesh->GetSkeleton()->GetPathName() : TEXT("<none>"),
        *PathOrNone(ExpectedSkeleton));
    AddVerifyCheck(Checks, ErrorCount, Name, bPassed, Details);
}

void AddBoneCountCheck(
    TArray<TSharedPtr<FJsonValue>>& Checks,
    int32& ErrorCount,
    const FString& Name,
    USkeleton* Skeleton,
    int32 ExpectedBones)
{
    const int32 BoneCount = Skeleton ? Skeleton->GetReferenceSkeleton().GetNum() : 0;
    const bool bPassed = Skeleton && BoneCount == ExpectedBones;
    const FString Details = FString::Printf(TEXT("skeleton=%s bones=%d expected=%d"), *PathOrNone(Skeleton), BoneCount, ExpectedBones);
    AddVerifyCheck(Checks, ErrorCount, Name, bPassed, Details);
}

void AddSkeletonPreviewCheck(
    TArray<TSharedPtr<FJsonValue>>& Checks,
    int32& ErrorCount,
    const FString& Name,
    USkeleton* Skeleton,
    USkeletalMesh* ExpectedPreviewMesh)
{
    USkeletalMesh* PreviewMesh = Skeleton ? Skeleton->GetPreviewMesh(false) : nullptr;
    const bool bPassed = Skeleton && ExpectedPreviewMesh && PreviewMesh == ExpectedPreviewMesh;
    const FString Details = FString::Printf(
        TEXT("skeleton=%s preview=%s expected=%s"),
        *PathOrNone(Skeleton),
        *PathOrNone(PreviewMesh),
        *PathOrNone(ExpectedPreviewMesh));
    AddVerifyCheck(Checks, ErrorCount, Name, bPassed, Details);
}

void AddAnimationCheck(
    TArray<TSharedPtr<FJsonValue>>& Checks,
    int32& ErrorCount,
    const FString& Name,
    UAnimSequence* Sequence,
    USkeleton* ExpectedSkeleton,
    USkeletalMesh* ExpectedPreviewMesh)
{
    const bool bSkeletonOk = Sequence && ExpectedSkeleton && Sequence->GetSkeleton() == ExpectedSkeleton;
    const bool bPreviewOk = !ExpectedPreviewMesh || (Sequence && Sequence->GetPreviewMesh(false) == ExpectedPreviewMesh);
    const bool bLengthOk = Sequence && Sequence->GetPlayLength() > 0.0f;
    const bool bPassed = bSkeletonOk && bPreviewOk && bLengthOk;
    const FString Details = FString::Printf(
        TEXT("sequence=%s skeleton=%s expected_skeleton=%s preview=%s expected_preview=%s length=%.3f"),
        *PathOrNone(Sequence),
        Sequence && Sequence->GetSkeleton() ? *Sequence->GetSkeleton()->GetPathName() : TEXT("<none>"),
        *PathOrNone(ExpectedSkeleton),
        Sequence && Sequence->GetPreviewMesh(false) ? *Sequence->GetPreviewMesh(false)->GetPathName() : TEXT("<none>"),
        *PathOrNone(ExpectedPreviewMesh),
        Sequence ? Sequence->GetPlayLength() : 0.0f);
    AddVerifyCheck(Checks, ErrorCount, Name, bPassed, Details);
}

void AddTextureSettingsCheck(
    TArray<TSharedPtr<FJsonValue>>& Checks,
    int32& ErrorCount,
    const FString& Name,
    UTexture2D* Texture,
    bool bExpectedSRGB,
    TextureCompressionSettings ExpectedCompression)
{
    const bool bPassed = Texture
        && Texture->SRGB == bExpectedSRGB
        && Texture->CompressionSettings == ExpectedCompression;
    const FString Details = FString::Printf(
        TEXT("texture=%s sRGB=%s expected_sRGB=%s compression=%d expected_compression=%d"),
        *PathOrNone(Texture),
        Texture && Texture->SRGB ? TEXT("true") : TEXT("false"),
        bExpectedSRGB ? TEXT("true") : TEXT("false"),
        Texture ? static_cast<int32>(Texture->CompressionSettings.GetValue()) : -1,
        static_cast<int32>(ExpectedCompression));
    AddVerifyCheck(Checks, ErrorCount, Name, bPassed, Details);
}

bool IsAssetUnderContentSubdir(const FAssetData& Asset, const FString& DestRoot, const FString& Subdir)
{
    const FString PackagePath = Asset.PackagePath.ToString();
    const FString ShortPath = CombineContentPath(DestRoot, Subdir);
    return PackagePath == ShortPath || PackagePath.StartsWith(ShortPath + TEXT("/"));
}

// 不依赖 UE Python 的资产导入验证入口。
int32 RunImportVerification(const FString& DestRoot, const FString& ReportPath)
{
    TArray<TSharedPtr<FJsonValue>> Checks;
    int32 ErrorCount = 0;

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    TArray<FString> ScanPaths;
    ScanPaths.Add(DestRoot);
    AssetRegistry.ScanPathsSynchronous(ScanPaths, true);

    FARFilter Filter;
    Filter.PackagePaths.Add(*DestRoot);
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    AssetRegistry.GetAssets(Filter, Assets);

    TMap<FString, int32> ClassCounts;
    TMap<FString, int32> DirectoryCounts;
    int32 ShortSummonDirAssets = 0;
    for (const FAssetData& Asset : Assets)
    {
        ClassCounts.FindOrAdd(Asset.AssetClassPath.GetAssetName().ToString())++;

        FString RelativePath = Asset.PackagePath.ToString();
        RelativePath.RemoveFromStart(DestRoot);
        RelativePath.RemoveFromStart(TEXT("/"));
        DirectoryCounts.FindOrAdd(RelativePath.IsEmpty() ? TEXT(".") : RelativePath)++;

        if (IsAssetUnderContentSubdir(Asset, DestRoot, TEXT("Spirits"))
            || IsAssetUnderContentSubdir(Asset, DestRoot, TEXT("Dragon")))
        {
            ShortSummonDirAssets++;
        }
    }

    AddVerifyCheck(Checks, ErrorCount, TEXT("asset_registry_scan"), Assets.Num() > 0, FString::Printf(TEXT("assets=%d root=%s"), Assets.Num(), *DestRoot));
    AddVerifyCheck(Checks, ErrorCount, TEXT("no_short_spirits_or_dragon_dirs"), ShortSummonDirAssets == 0, FString::Printf(TEXT("assets_under_/Spirits_or_/Dragon=%d"), ShortSummonDirAssets));

    const FString TexturePath = CombineContentPath(DestRoot, TEXT("Textures"));
    const FString MaterialPath = CombineContentPath(DestRoot, TEXT("Materials"));
    const FString SkeletalPath = CombineContentPath(DestRoot, TEXT("SkeletalMeshes"));
    const FString AnimationPath = CombineContentPath(DestRoot, TEXT("Animations"));
    const FString TrainerSpiritsPath = CombineContentPath(DestRoot, TEXT("TrainerSpirits"));
    const FString TrainerSpiritAnimationPath = CombineContentPath(TrainerSpiritsPath, TEXT("Animations"));
    const FString TrainerDragonPath = CombineContentPath(DestRoot, TEXT("TrainerDragon"));
    const FString TrainerDragonAnimationPath = CombineContentPath(TrainerDragonPath, TEXT("Animations"));

    USkeletalMesh* MainMesh = VerifyLoadAsset<USkeletalMesh>(Checks, ErrorCount, TEXT("load_main_mesh"), SkeletalPath, TEXT("SK_InvokerKid_Modular_AllParts"));
    USkeleton* MainSkeleton = VerifyLoadAsset<USkeleton>(Checks, ErrorCount, TEXT("load_main_skeleton"), SkeletalPath, TEXT("SK_InvokerKid_Modular_AllParts_Skeleton"));
    AddSkeletonBindingCheck(Checks, ErrorCount, TEXT("main_mesh_uses_main_skeleton"), MainMesh, MainSkeleton);
    AddBoneCountCheck(Checks, ErrorCount, TEXT("main_skeleton_bone_count"), MainSkeleton, 101);
    AddSkeletonPreviewCheck(Checks, ErrorCount, TEXT("main_skeleton_preview_mesh"), MainSkeleton, MainMesh);

    const TArray<FString> MainParts =
    {
        TEXT("SK_InvokerKid_BodyHead"),
        TEXT("SK_InvokerKid_Cape"),
        TEXT("SK_InvokerKid_Shoulder"),
        TEXT("SK_InvokerKid_Sleeves"),
        TEXT("SK_InvokerKid_Hair")
    };
    for (const FString& PartName : MainParts)
    {
        USkeletalMesh* PartMesh = VerifyLoadAsset<USkeletalMesh>(Checks, ErrorCount, TEXT("load_") + PartName, SkeletalPath, PartName);
        AddSkeletonBindingCheck(Checks, ErrorCount, TEXT("shared_main_skeleton_") + PartName, PartMesh, MainSkeleton);
    }

    auto VerifyStandalone = [&](const FString& Label, const FString& MeshName, int32 ExpectedBones, const TArray<FString>& SampleAnimations)
    {
        const bool bIsDragon = Label == TEXT("TrainerDragon");
        const FString& MeshPath = bIsDragon ? TrainerDragonPath : TrainerSpiritsPath;
        const FString& AnimPath = bIsDragon ? TrainerDragonAnimationPath : TrainerSpiritAnimationPath;
        USkeletalMesh* Mesh = VerifyLoadAsset<USkeletalMesh>(Checks, ErrorCount, TEXT("load_") + MeshName, MeshPath, MeshName);
        USkeleton* Skeleton = VerifyLoadAsset<USkeleton>(Checks, ErrorCount, TEXT("load_") + MeshName + TEXT("_Skeleton"), MeshPath, MeshName + TEXT("_Skeleton"));
        AddSkeletonBindingCheck(Checks, ErrorCount, TEXT("standalone_skeleton_binding_") + Label, Mesh, Skeleton);
        AddBoneCountCheck(Checks, ErrorCount, TEXT("standalone_bone_count_") + Label, Skeleton, ExpectedBones);
        AddSkeletonPreviewCheck(Checks, ErrorCount, TEXT("standalone_preview_mesh_") + Label, Skeleton, Mesh);
        for (const FString& AnimName : SampleAnimations)
        {
            UAnimSequence* Sequence = VerifyLoadAsset<UAnimSequence>(Checks, ErrorCount, TEXT("load_") + AnimName, AnimPath, AnimName);
            AddAnimationCheck(Checks, ErrorCount, TEXT("animation_binding_") + AnimName, Sequence, Skeleton, Mesh);
        }
    };

    const TArray<FString> MainAnimationSamples =
    {
        TEXT("AN_InvokerKid_idle_anim"),
        TEXT("AN_InvokerKid_run_anim"),
        TEXT("AN_InvokerKid_attack_anim"),
        TEXT("AN_InvokerKid_cast"),
        TEXT("AN_InvokerKid_death2")
    };
    for (const FString& AnimName : MainAnimationSamples)
    {
        UAnimSequence* Sequence = VerifyLoadAsset<UAnimSequence>(Checks, ErrorCount, TEXT("load_") + AnimName, AnimationPath, AnimName);
        AddAnimationCheck(Checks, ErrorCount, TEXT("animation_binding_") + AnimName, Sequence, MainSkeleton, MainMesh);
    }

    VerifyStandalone(TEXT("Exort"), TEXT("SK_InvokerKid_Exort"), 8, { TEXT("AN_InvokerKid_Exort_invoker_exort_idle") });
    VerifyStandalone(TEXT("Quas"), TEXT("SK_InvokerKid_Quas"), 8, { TEXT("AN_InvokerKid_Quas_invoker_quas_idle") });
    VerifyStandalone(TEXT("Wex"), TEXT("SK_InvokerKid_Wex"), 8, { TEXT("AN_InvokerKid_Wex_invoker_wex_idle") });
    VerifyStandalone(TEXT("TrainerDragon"), TEXT("SK_InvokerKid_TrainerDragon"), 48, { TEXT("AN_InvokerKid_TrainerDragon_forge_spirit_idle") });

    AddTextureSettingsCheck(
        Checks,
        ErrorCount,
        TEXT("texture_color_srgb"),
        LoadTypedAsset<UTexture2D>(ObjectPath(TexturePath, TEXT("T_invoker_kid_body_color_psd_77bd4cde"))),
        true,
        TC_Default);
    AddTextureSettingsCheck(
        Checks,
        ErrorCount,
        TEXT("texture_normal_linear_normalmap"),
        LoadTypedAsset<UTexture2D>(ObjectPath(TexturePath, TEXT("T_invoker_kid_body_normal_psd_3d471306"))),
        false,
        TC_Normalmap);
    AddTextureSettingsCheck(
        Checks,
        ErrorCount,
        TEXT("texture_orm_linear_masks"),
        LoadTypedAsset<UTexture2D>(ObjectPath(TexturePath, TEXT("T_invoker_kid_body_diffusemask_psd_5a526a8_orm_2773339141"))),
        false,
        TC_Masks);
    AddTextureSettingsCheck(
        Checks,
        ErrorCount,
        TEXT("texture_selfillum_linear_masks"),
        LoadTypedAsset<UTexture2D>(ObjectPath(TexturePath, TEXT("T_invoker_kid_head_selfillummask_psd_33b482d2"))),
        false,
        TC_Masks);
    AddTextureSettingsCheck(
        Checks,
        ErrorCount,
        TEXT("texture_tmasks_linear_masks"),
        LoadTypedAsset<UTexture2D>(ObjectPath(TexturePath, TEXT("T_invoker_sprite_exort_vmat_g_tmasks2_86b58cd6"))),
        false,
        TC_Masks);

    VerifyLoadAsset<UMaterial>(Checks, ErrorCount, TEXT("load_material_body"), MaterialPath, TEXT("M_InvokerKid_invoker_kid_body"));
    VerifyLoadAsset<UMaterial>(Checks, ErrorCount, TEXT("load_material_head"), MaterialPath, TEXT("M_InvokerKid_invoker_kid_head"));
    VerifyLoadAsset<UMaterial>(Checks, ErrorCount, TEXT("load_material_exort"), MaterialPath, TEXT("M_InvokerKid_invoker_sprite_exort"));
    VerifyLoadAsset<UMaterial>(Checks, ErrorCount, TEXT("load_material_trainer_dragon"), MaterialPath, TEXT("M_InvokerKid_invoker_trainer_dragon"));

    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("dest_root"), DestRoot);
    Report->SetNumberField(TEXT("asset_count"), Assets.Num());
    Report->SetNumberField(TEXT("error_count"), ErrorCount);
    Report->SetBoolField(TEXT("passed"), ErrorCount == 0);
    Report->SetArrayField(TEXT("checks"), Checks);

    TSharedPtr<FJsonObject> ClassCountsObject = MakeShared<FJsonObject>();
    for (const TPair<FString, int32>& Pair : ClassCounts)
    {
        ClassCountsObject->SetNumberField(Pair.Key, Pair.Value);
    }
    Report->SetObjectField(TEXT("class_counts"), ClassCountsObject);

    TSharedPtr<FJsonObject> DirectoryCountsObject = MakeShared<FJsonObject>();
    for (const TPair<FString, int32>& Pair : DirectoryCounts)
    {
        DirectoryCountsObject->SetNumberField(Pair.Key, Pair.Value);
    }
    Report->SetObjectField(TEXT("directory_counts"), DirectoryCountsObject);

    if (!ReportPath.IsEmpty())
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
        FString Json;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
        FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
        if (!FFileHelper::SaveStringToFile(Json, *ReportPath))
        {
            UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Could not write verification report: %s"), *ReportPath);
            return 1;
        }
        UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Verification report written: %s"), *ReportPath);
    }

    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Dota2 verify finished. Assets=%d Errors=%d"), Assets.Num(), ErrorCount);
    return ErrorCount == 0 ? 0 : 1;
}

// 通用 manifest 驱动验证：适合加载包、替换件、补丁导入等非固定 InvokerKid 命名的资产。
int32 RunManifestVerification(
    const FString& ManifestPath,
    const FString& DestRoot,
    const FString& ExpectedSkeletonPath,
    const FString& ReportPath)
{
    TSharedPtr<FJsonObject> Manifest;
    if (!LoadManifest(ManifestPath, Manifest))
    {
        return 1;
    }

    TArray<FDota2ManifestEntry> SkeletalEntries;
    TArray<FDota2ManifestEntry> AnimationEntries;
    TArray<FDota2ManifestEntry> PropEntries;
    ParseManifestArray(Manifest, TEXT("skeletal_meshes"), SkeletalEntries);
    ParseManifestArray(Manifest, TEXT("animations"), AnimationEntries);
    ParseManifestArray(Manifest, TEXT("props"), PropEntries);

    TArray<TSharedPtr<FJsonValue>> Checks;
    int32 ErrorCount = 0;

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    AssetRegistry.ScanPathsSynchronous({ DestRoot }, true);

    FARFilter Filter;
    Filter.PackagePaths.Add(*DestRoot);
    Filter.bRecursivePaths = true;

    TArray<FAssetData> Assets;
    AssetRegistry.GetAssets(Filter, Assets);
    AddVerifyCheck(Checks, ErrorCount, TEXT("asset_registry_scan"), Assets.Num() > 0, FString::Printf(TEXT("assets=%d root=%s"), Assets.Num(), *DestRoot));

    USkeleton* ExpectedSkeleton = ExpectedSkeletonPath.IsEmpty() ? nullptr : LoadTypedAsset<USkeleton>(ExpectedSkeletonPath);
    if (!ExpectedSkeletonPath.IsEmpty())
    {
        AddVerifyCheck(
            Checks,
            ErrorCount,
            TEXT("load_expected_skeleton"),
            ExpectedSkeleton != nullptr,
            ExpectedSkeleton ? ExpectedSkeleton->GetPathName() : ExpectedSkeletonPath);
    }

    const FString DefaultSkeletalPath = CombineContentPath(DestRoot, TEXT("SkeletalMeshes"));
    for (const FDota2ManifestEntry& Entry : SkeletalEntries)
    {
        const FString EntryPath = EntryDestPath(DefaultSkeletalPath, DestRoot, Entry);
        const FString AssetName = CleanObjectName(Entry.Name);
        USkeletalMesh* Mesh = VerifyLoadAsset<USkeletalMesh>(Checks, ErrorCount, TEXT("load_") + AssetName, EntryPath, AssetName);
        if (ExpectedSkeleton)
        {
            AddSkeletonBindingCheck(Checks, ErrorCount, TEXT("skeleton_binding_") + AssetName, Mesh, ExpectedSkeleton);
        }
    }

    const FString DefaultAnimationPath = CombineContentPath(DestRoot, TEXT("Animations"));
    for (const FDota2ManifestEntry& Entry : AnimationEntries)
    {
        const FString EntryPath = EntryDestPath(DefaultAnimationPath, DestRoot, Entry);
        const FString AssetName = CleanObjectName(Entry.Name);
        UAnimSequence* Sequence = VerifyLoadAsset<UAnimSequence>(Checks, ErrorCount, TEXT("load_") + AssetName, EntryPath, AssetName);
        if (ExpectedSkeleton)
        {
            AddAnimationCheck(Checks, ErrorCount, TEXT("animation_binding_") + AssetName, Sequence, ExpectedSkeleton, nullptr);
        }
    }

    const FString DefaultPropPath = CombineContentPath(DestRoot, TEXT("Props"));
    for (const FDota2ManifestEntry& Entry : PropEntries)
    {
        if (Entry.Type.Equals(TEXT("static"), ESearchCase::IgnoreCase)
            || Entry.Type.Contains(TEXT("static"), ESearchCase::IgnoreCase))
        {
            const FString EntryPath = EntryDestPath(DefaultPropPath, DestRoot, Entry);
            const FString AssetName = CleanObjectName(Entry.Name);
            VerifyLoadAsset<UStaticMesh>(Checks, ErrorCount, TEXT("load_") + AssetName, EntryPath, AssetName);
        }
    }

    TSharedPtr<FJsonObject> Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("manifest"), ManifestPath);
    Report->SetStringField(TEXT("dest_root"), DestRoot);
    Report->SetStringField(TEXT("expected_skeleton"), ExpectedSkeletonPath);
    Report->SetNumberField(TEXT("asset_count"), Assets.Num());
    Report->SetNumberField(TEXT("skeletal_entry_count"), SkeletalEntries.Num());
    Report->SetNumberField(TEXT("animation_entry_count"), AnimationEntries.Num());
    Report->SetNumberField(TEXT("prop_entry_count"), PropEntries.Num());
    Report->SetNumberField(TEXT("error_count"), ErrorCount);
    Report->SetBoolField(TEXT("passed"), ErrorCount == 0);
    Report->SetArrayField(TEXT("checks"), Checks);

    if (!ReportPath.IsEmpty())
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
        FString Json;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
        FJsonSerializer::Serialize(Report.ToSharedRef(), Writer);
        if (!FFileHelper::SaveStringToFile(Json, *ReportPath))
        {
            UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Could not write manifest verification report: %s"), *ReportPath);
            return 1;
        }
        UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Manifest verification report written: %s"), *ReportPath);
    }

    UE_LOG(
        LogDota2ImportCommandlet,
        Display,
        TEXT("Dota2 manifest verify finished. Assets=%d SkeletalEntries=%d Errors=%d"),
        Assets.Num(),
        SkeletalEntries.Num(),
        ErrorCount);
    return ErrorCount == 0 ? 0 : 1;
}

struct FSocketPreviewMapping
{
    FName SocketName;
    FString MeshPath;
};

FString DescribePreviewAttachments(const FPreviewAssetAttachContainer& Container, const TArray<FName>& SocketNames)
{
    TArray<FString> Parts;
    for (const FName& SocketName : SocketNames)
    {
        TArray<FString> Objects;
        for (int32 Index = 0; Index < Container.Num(); ++Index)
        {
            const FPreviewAttachedObjectPair& Pair = Container[Index];
            if (Pair.AttachedTo == SocketName)
            {
                UObject* Object = Pair.GetAttachedObject();
                Objects.Add(Object ? Object->GetPathName() : TEXT("<missing>"));
            }
        }
        Parts.Add(FString::Printf(TEXT("%s=[%s]"), *SocketName.ToString(), *FString::Join(Objects, TEXT(", "))));
    }
    return FString::Join(Parts, TEXT("; "));
}

int32 RemoveSocketPreviewAttachments(FPreviewAssetAttachContainer& Container, const TSet<FName>& SocketNames)
{
    int32 Removed = 0;
    for (int32 Index = Container.Num() - 1; Index >= 0; --Index)
    {
        if (SocketNames.Contains(Container[Index].AttachedTo))
        {
            Container.RemoveAtSwap(Index, EAllowShrinking::No);
            ++Removed;
        }
    }
    return Removed;
}

int32 RunInvokerKidOrbPreviewFix()
{
    const FString SkeletonPath = TEXT("/Game/Assets/Characters/Dota2/Invoker_kid/SkeletalMeshes/SK_InvokerKid_Modular_AllParts_Skeleton.SK_InvokerKid_Modular_AllParts_Skeleton");
    const FString MainMeshPath = TEXT("/Game/Assets/Characters/Dota2/Invoker_kid/SkeletalMeshes/SK_InvokerKid_Modular_AllParts.SK_InvokerKid_Modular_AllParts");
    const TArray<FSocketPreviewMapping> Mappings =
    {
        { TEXT("SO_Orb1_Quas"), TEXT("/Game/Assets/Characters/Dota2/Invoker_kid/TrainerSpirits/SK_InvokerKid_Quas.SK_InvokerKid_Quas") },
        { TEXT("SO_Orb2_Exort"), TEXT("/Game/Assets/Characters/Dota2/Invoker_kid/TrainerSpirits/SK_InvokerKid_Exort.SK_InvokerKid_Exort") },
        { TEXT("SO_Orb3_Wex"), TEXT("/Game/Assets/Characters/Dota2/Invoker_kid/TrainerSpirits/SK_InvokerKid_Wex.SK_InvokerKid_Wex") }
    };

    USkeleton* Skeleton = LoadTypedAsset<USkeleton>(SkeletonPath);
    if (!Skeleton)
    {
        UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Could not load Invoker Kid skeleton: %s"), *SkeletonPath);
        return 1;
    }

    USkeletalMesh* MainMesh = LoadTypedAsset<USkeletalMesh>(MainMeshPath);
    if (!MainMesh)
    {
        UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Could not load Invoker Kid main preview mesh: %s"), *MainMeshPath);
        return 1;
    }

    TArray<FName> SocketNames;
    TSet<FName> SocketNameSet;
    for (const FSocketPreviewMapping& Mapping : Mappings)
    {
        if (!Skeleton->FindSocket(Mapping.SocketName))
        {
            UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Missing socket on skeleton: %s"), *Mapping.SocketName.ToString());
            return 1;
        }
        SocketNames.Add(Mapping.SocketName);
        SocketNameSet.Add(Mapping.SocketName);
    }

    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Skeleton previews before: %s"), *DescribePreviewAttachments(Skeleton->PreviewAttachedAssetContainer, SocketNames));
    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Main mesh previews before: %s"), *DescribePreviewAttachments(MainMesh->GetPreviewAttachedAssetContainer(), SocketNames));

    Skeleton->Modify();
    MainMesh->Modify();

    const int32 RemovedFromSkeleton = RemoveSocketPreviewAttachments(Skeleton->PreviewAttachedAssetContainer, SocketNameSet);
    const int32 RemovedFromMainMesh = RemoveSocketPreviewAttachments(MainMesh->GetPreviewAttachedAssetContainer(), SocketNameSet);

    for (const FSocketPreviewMapping& Mapping : Mappings)
    {
        USkeletalMesh* PreviewMesh = LoadTypedAsset<USkeletalMesh>(Mapping.MeshPath);
        if (!PreviewMesh)
        {
            UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Could not load preview mesh for %s: %s"), *Mapping.SocketName.ToString(), *Mapping.MeshPath);
            return 1;
        }
        Skeleton->PreviewAttachedAssetContainer.AddAttachedObject(PreviewMesh, Mapping.SocketName);
        UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Set preview %s -> %s"), *Mapping.SocketName.ToString(), *PreviewMesh->GetPathName());
    }

    Skeleton->MarkPackageDirty();
    MainMesh->MarkPackageDirty();

    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Removed preview attachments from skeleton=%d main_mesh=%d"), RemovedFromSkeleton, RemovedFromMainMesh);
    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Skeleton previews after: %s"), *DescribePreviewAttachments(Skeleton->PreviewAttachedAssetContainer, SocketNames));
    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Main mesh previews after: %s"), *DescribePreviewAttachments(MainMesh->GetPreviewAttachedAssetContainer(), SocketNames));

    SaveAllDirtyContent();
    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Invoker Kid orb preview assets fixed. Socket transforms were not modified."));
    return 0;
}
}

// Commandlet 构造函数：声明它运行在编辑器命令行环境，并把日志输出到控制台。
UDota2ImportCommandlet::UDota2ImportCommandlet()
{
    // 不是客户端或服务器运行时命令，而是编辑器命令。
    IsClient = false;
    IsEditor = true;
    IsServer = false;

    // 让 UnrealEditor-Cmd.exe 执行时能在控制台看到日志。
    LogToConsole = true;
}

// Commandlet 主入口。Params 是 Unreal 传入的完整参数字符串。
int32 UDota2ImportCommandlet::Main(const FString& Params)
{
    // 必填参数：manifest 文件路径和 Content Browser 目标根目录。
    const FString ManifestPath = ParseArgValue(Params, TEXT("Manifest="));
    const FString DestRoot = ParseArgValue(Params, TEXT("Dest="));
    const bool bVerifyOnly = ParseBoolSwitch(Params, TEXT("VerifyOnly"));
    const bool bFixInvokerKidOrbPreviews = ParseBoolSwitch(Params, TEXT("FixInvokerKidOrbPreviews"));
    const FString VerifyManifestPath = ParseArgValue(Params, TEXT("VerifyManifest="));
    const FString VerifyReportPath = ParseArgValue(Params, TEXT("VerifyReport="));

    // 可选参数：导入完成后要自动配置的角色蓝图、动画蓝图和 Idle 动画。
    const FString BlueprintPath = ParseArgValue(Params, TEXT("Blueprint="));
    const FString SkeletonPath = ParseArgValue(Params, TEXT("Skeleton="));
    const FString AnimBlueprintPath = ParseArgValue(Params, TEXT("AnimBlueprint="));
    const FString IdleAnimPath = ParseArgValue(Params, TEXT("IdleAnim="));

    // Character 参与材质命名；ReplaceExisting 控制是否覆盖已有资产。
    FString Character = ParseArgValue(Params, TEXT("Character="));
    const bool bReplaceExisting = ParseBoolSwitch(Params, TEXT("ReplaceExisting"));

    // FBX 导入旋转修正只作为诊断/兜底参数保留。
    // 标准 Dota2 Blender 预处理流程应在 FBX 数据层烘焙轴向，UE 侧保持零旋转导入。
    const FRotator ImportRotation(
        ParseFloatArg(Params, TEXT("ImportRotateY="), 0.0f),
        ParseFloatArg(Params, TEXT("ImportRotateZ="), 0.0f),
        ParseFloatArg(Params, TEXT("ImportRotateX="), 0.0f));

    // FBX 统一导入缩放，默认 1.0 表示不缩放；用于处理 Dota/Blender/UE 单位差异。
    const float ImportUniformScale = ParseFloatArg(Params, TEXT("ImportUniformScale="), 1.0f);

    if (bFixInvokerKidOrbPreviews)
    {
        return RunInvokerKidOrbPreviewFix();
    }

    // 缺少核心参数时直接失败，并打印最小用法。
    if (DestRoot.IsEmpty() || (!bVerifyOnly && ManifestPath.IsEmpty()))
    {
        UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Usage: -run=Dota2Import -Manifest=\"...json\" -Dest=\"/Game/...\" [-Character=Name] [-ReplaceExisting] [-ImportUniformScale=1.0] [-Blueprint=/Game/...BP.BP] or -run=Dota2Import -Dest=\"/Game/...\" -VerifyOnly [-VerifyManifest=\"...json\"] [-Skeleton=/Game/...Skeleton.Skeleton] [-VerifyReport=\"...json\"] or -run=Dota2Import -FixInvokerKidOrbPreviews"));
        return 1;
    }

    if (bVerifyOnly)
    {
        if (!VerifyManifestPath.IsEmpty())
        {
            return RunManifestVerification(VerifyManifestPath, DestRoot, SkeletonPath, VerifyReportPath);
        }
        return RunImportVerification(DestRoot, VerifyReportPath);
    }

    // 清理角色名，确保它可以安全参与 UE 资产命名。
    Character = CleanObjectName(Character.IsEmpty() ? TEXT("Dota2Character") : Character);
    TSharedPtr<FJsonObject> Manifest;
    if (!LoadManifest(ManifestPath, Manifest))
    {
        return 1;
    }

    if (!SkeletonPath.IsEmpty())
    {
        UE_LOG(
            LogDota2ImportCommandlet,
            Error,
            TEXT("Refusing to import skeletal meshes against an existing Skeleton with -Skeleton=%s. This can dirty or rewrite the Skeleton package. Build a new superset Skeleton or use -VerifyOnly for checks."),
            *SkeletonPath);
        return 1;
    }

    UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Dota2 import started. Manifest=%s Dest=%s Character=%s"), *ManifestPath, *DestRoot, *Character);

    // 从 manifest 中拆出三类资源条目。
    TArray<FDota2ManifestEntry> SkeletalEntries;
    TArray<FDota2ManifestEntry> AnimationEntries;
    TArray<FDota2ManifestEntry> PropEntries;
    ParseManifestArray(Manifest, TEXT("skeletal_meshes"), SkeletalEntries);
    ParseManifestArray(Manifest, TEXT("animations"), AnimationEntries);
    ParseManifestArray(Manifest, TEXT("props"), PropEntries);

    // 至少需要一个骨骼网格；第一个骨骼网格会被当作主网格并创建主 Skeleton。
    if (SkeletalEntries.Num() == 0)
    {
        UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Manifest contains no skeletal_meshes entries."));
        return 1;
    }

    // 在目标根目录下固定建立分类子目录，便于 Content Browser 管理。
    const FString TexturePath = CombineContentPath(DestRoot, TEXT("Textures"));
    const FString MaterialPath = CombineContentPath(DestRoot, TEXT("Materials"));
    const FString SkeletalPath = CombineContentPath(DestRoot, TEXT("SkeletalMeshes"));
    const FString AnimationPath = CombineContentPath(DestRoot, TEXT("Animations"));
    const FString PropPath = CombineContentPath(DestRoot, TEXT("Props"));

    // 先导入贴图，再根据贴图组创建材质；网格导入后会用这些材质自动匹配槽位。
    TMap<FString, FTextureSet> TextureSets;
    ImportTextures(Manifest, TexturePath, TextureSets, bReplaceExisting);
    TMap<FString, UMaterial*> MaterialsByKey = CreateMaterials(MaterialPath, Character, TextureSets, bReplaceExisting);

    // 主骨架默认来自第一个 skeletal_meshes 条目；如果传入 -Skeleton，则严格复用现有 Skeleton。
    USkeleton* MasterSkeleton = SkeletonPath.IsEmpty() ? nullptr : LoadTypedAsset<USkeleton>(SkeletonPath);
    if (!SkeletonPath.IsEmpty() && !MasterSkeleton)
    {
        UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Failed to load existing skeleton: %s"), *SkeletonPath);
        return 1;
    }
    if (MasterSkeleton)
    {
        UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Using existing master skeleton: %s"), *MasterSkeleton->GetPathName());
    }
    USkeletalMesh* MasterMesh = nullptr;
    TMap<FString, USkeleton*> SkeletonByTarget;
    TMap<FString, USkeletalMesh*> PreviewMeshByTarget;
    TMap<FString, TArray<UAnimSequence*>> AnimationsByTarget;

    // 逐个导入骨骼网格。Index==0 的网格创建 PhysicsAsset 和主 Skeleton。
    for (int32 Index = 0; Index < SkeletalEntries.Num(); ++Index)
    {
        const FDota2ManifestEntry& Entry = SkeletalEntries[Index];
        const bool bCreatePhysicsAsset = Index == 0;
        const FString EntrySkeletalPath = EntryDestPath(SkeletalPath, DestRoot, Entry);
        USkeletalMesh* Mesh = ImportSkeletalMeshEntry(Entry, EntrySkeletalPath, MasterSkeleton, bCreatePhysicsAsset, bReplaceExisting, MaterialsByKey, ImportRotation, ImportUniformScale);
        if (!Mesh)
        {
            UE_LOG(LogDota2ImportCommandlet, Warning, TEXT("Skeletal mesh failed to import: %s"), *Entry.Name);
            continue;
        }

        // 第一个骨骼网格是主网格，后续动画默认绑定到它的 Skeleton。
        if (Index == 0)
        {
            MasterMesh = Mesh;
            MasterSkeleton = MasterSkeleton ? MasterSkeleton : Mesh->GetSkeleton();
            if (!MasterSkeleton)
            {
                UE_LOG(LogDota2ImportCommandlet, Error, TEXT("Master skeletal mesh imported without a skeleton: %s"), *Entry.Name);
                return 1;
            }
            SkeletonByTarget.Add(Entry.Name, MasterSkeleton);
            SkeletonByTarget.Add(MasterSkeleton->GetName(), MasterSkeleton);
            SkeletonByTarget.Add(Entry.Name + TEXT("_Skeleton"), MasterSkeleton);
            PreviewMeshByTarget.Add(Entry.Name, Mesh);
            PreviewMeshByTarget.Add(MasterSkeleton->GetName(), Mesh);
            PreviewMeshByTarget.Add(Entry.Name + TEXT("_Skeleton"), Mesh);
            UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Master skeleton: %s"), *MasterSkeleton->GetPathName());
        }
        else if (MasterSkeleton)
        {
            SkeletonByTarget.Add(Entry.Name, MasterSkeleton);
        }
    }

    // props 可以是静态网格，也可以是独立骨骼 FX；两类分别计数。
    int32 ImportedStaticProps = 0;
    int32 ImportedStandaloneFx = 0;
    TMap<FString, USkeleton*> StandaloneFxSkeletons;

    // 导入 props。部分 FX 占位资源 UE FBX 无法按网格/骨架导入，按命名规则跳过。
    for (const FDota2ManifestEntry& Entry : PropEntries)
    {
        if (Entry.Name.Contains(TEXT("rocks")))
        {
            UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Skipping FX placeholder that UE FBX cannot import as mesh/skeleton: %s"), *Entry.Name);
            continue;
        }

        // type 标为 static 或名称以 SM_ 开头时按静态网格导入。
        if (Entry.Type.Contains(TEXT("static")) || Entry.Name.StartsWith(TEXT("SM_")))
        {
            const FString EntryPropPath = EntryDestPath(PropPath, DestRoot, Entry);
            if (ImportStaticMeshEntry(Entry, EntryPropPath, bReplaceExisting, MaterialsByKey, ImportRotation, ImportUniformScale))
            {
                ImportedStaticProps++;
            }
        }
        else
        {
            // 其他 prop 当作独立骨骼 FX 导入，并记录自己的 Skeleton，供目标动画匹配。
            const FString EntryPropPath = EntryDestPath(PropPath, DestRoot, Entry);
            USkeletalMesh* FxMesh = ImportSkeletalMeshEntry(Entry, EntryPropPath, nullptr, true, bReplaceExisting, MaterialsByKey, ImportRotation, ImportUniformScale);
            if (FxMesh && FxMesh->GetSkeleton())
            {
                StandaloneFxSkeletons.Add(Entry.Name, FxMesh->GetSkeleton());
                SkeletonByTarget.Add(Entry.Name, FxMesh->GetSkeleton());
                SkeletonByTarget.Add(FxMesh->GetSkeleton()->GetName(), FxMesh->GetSkeleton());
                SkeletonByTarget.Add(Entry.Name + TEXT("_Skeleton"), FxMesh->GetSkeleton());
                PreviewMeshByTarget.Add(Entry.Name, FxMesh);
                PreviewMeshByTarget.Add(FxMesh->GetSkeleton()->GetName(), FxMesh);
                PreviewMeshByTarget.Add(Entry.Name + TEXT("_Skeleton"), FxMesh);
                ImportedStandaloneFx++;
            }
        }
    }

    // 导入动画。默认绑定主 Skeleton；manifest.target 存在时尝试绑定目标骨架。
    int32 ImportedAnimations = 0;
    TArray<UAnimSequence*> ImportedAnimationAssets;
    for (const FDota2ManifestEntry& Entry : AnimationEntries)
    {
        // 每条动画可以指定 target；找不到 target 时保留主 Skeleton 作为兜底。
        USkeleton* TargetSkeleton = MasterSkeleton;
        FString ResolvedTarget = Entry.Target.IsEmpty() && MasterMesh ? MasterMesh->GetName() : Entry.Target;
        if (!Entry.Target.IsEmpty())
        {
            if (USkeleton** Found = SkeletonByTarget.Find(Entry.Target))
            {
                TargetSkeleton = *Found;
            }
            else if (Entry.Target.Contains(TEXT("arcana_hand")))
            {
                // ShadowFiend arcana_hand 这类 FX 命名可能和 prop 条目不完全一致，做一次包含匹配兜底。
                for (const TPair<FString, USkeleton*>& Pair : StandaloneFxSkeletons)
                {
                    if (Pair.Key.Contains(TEXT("arcana_hand")))
                    {
                        TargetSkeleton = Pair.Value;
                        ResolvedTarget = Pair.Key;
                        break;
                    }
                }
            }

            if (TargetSkeleton == MasterSkeleton && !SkeletonByTarget.Contains(Entry.Target))
            {
                UE_LOG(LogDota2ImportCommandlet, Display, TEXT("Skipping animation with unresolved non-master target %s: %s"), *Entry.Target, *Entry.Name);
                continue;
            }
        }

        const FString EntryAnimationPath = EntryDestPath(AnimationPath, DestRoot, Entry);
        if (UAnimSequence* Sequence = ImportAnimationEntry(Entry, EntryAnimationPath, TargetSkeleton, bReplaceExisting, ImportRotation, ImportUniformScale))
        {
            ImportedAnimationAssets.Add(Sequence);
            if (!ResolvedTarget.IsEmpty())
            {
                AnimationsByTarget.FindOrAdd(ResolvedTarget).Add(Sequence);
            }
            ImportedAnimations++;
        }
    }

    // 资产导入完成后做编辑器体验后处理：设置预览网格、配置蓝图、保存所有脏包。
    SetPreviewMeshes(MasterSkeleton, MasterMesh, ImportedAnimationAssets);
    for (const TPair<FString, USkeleton*>& Pair : StandaloneFxSkeletons)
    {
        USkeletalMesh** PreviewMesh = PreviewMeshByTarget.Find(Pair.Key);
        TArray<UAnimSequence*>* TargetAnimations = AnimationsByTarget.Find(Pair.Key);
        if (PreviewMesh && *PreviewMesh)
        {
            const TArray<UAnimSequence*> EmptyAnimations;
            SetPreviewMeshes(Pair.Value, *PreviewMesh, TargetAnimations ? *TargetAnimations : EmptyAnimations);
        }
    }
    ConfigureShadowFiendBlueprint(BlueprintPath, SkeletalPath, AnimBlueprintPath, IdleAnimPath);

    SaveAllDirtyContent();

    // 输出本次导入汇总，便于命令行和 CI 日志确认结果。
    UE_LOG(
        LogDota2ImportCommandlet,
        Display,
        TEXT("Dota2 import finished. MasterMesh=%s Skeleton=%s Materials=%d StaticProps=%d StandaloneFx=%d Animations=%d"),
        MasterMesh ? *MasterMesh->GetPathName() : TEXT("<none>"),
        MasterSkeleton ? *MasterSkeleton->GetPathName() : TEXT("<none>"),
        MaterialsByKey.Num(),
        ImportedStaticProps,
        ImportedStandaloneFx,
        ImportedAnimations);

    // 返回 0 表示 Commandlet 成功完成。
    return 0;
}
