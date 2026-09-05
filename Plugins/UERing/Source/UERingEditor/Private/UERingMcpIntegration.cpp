#include "UERingMcpIntegration.h"

#include "Async/Async.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "HAL/FileManager.h"
#include "IModelContextProtocolModule.h"
#include "IModelContextProtocolTool.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ModelContextProtocolToolResults.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SQLiteDatabase.h"
#include "SQLitePreparedStatement.h"
#include "UERingCppIndexer.h"
#include "UERingExportManager.h"
#include "UERingIndexManager.h"
#include "UERingSettings.h"
#include "UERingValidator.h"

DEFINE_LOG_CATEGORY_STATIC(LogUERingMcp, Log, All);

namespace UERingMcp
{
    TSharedRef<FJsonObject> StringProperty(const FString& Description)
    {
        const TSharedRef<FJsonObject> Property = MakeShared<FJsonObject>();
        Property->SetStringField(TEXT("type"), TEXT("string"));
        Property->SetStringField(TEXT("description"), Description);
        return Property;
    }

    TSharedRef<FJsonObject> BooleanProperty(const FString& Description, const bool bDefault)
    {
        const TSharedRef<FJsonObject> Property = MakeShared<FJsonObject>();
        Property->SetStringField(TEXT("type"), TEXT("boolean"));
        Property->SetStringField(TEXT("description"), Description);
        Property->SetBoolField(TEXT("default"), bDefault);
        return Property;
    }

    TSharedRef<FJsonObject> IntegerProperty(
        const FString& Description,
        const int64 Default,
        const int64 Minimum,
        const int64 Maximum)
    {
        const TSharedRef<FJsonObject> Property = MakeShared<FJsonObject>();
        Property->SetStringField(TEXT("type"), TEXT("integer"));
        Property->SetStringField(TEXT("description"), Description);
        Property->SetNumberField(TEXT("default"), Default);
        Property->SetNumberField(TEXT("minimum"), Minimum);
        Property->SetNumberField(TEXT("maximum"), Maximum);
        return Property;
    }

    TSharedPtr<FJsonObject> ObjectSchema(
        const TMap<FString, TSharedPtr<FJsonValue>>& Properties,
        const TArray<FString>& Required = {})
    {
        const TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
        Schema->SetStringField(TEXT("type"), TEXT("object"));
        const TSharedRef<FJsonObject> JsonProperties = MakeShared<FJsonObject>();
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Property : Properties)
        {
            JsonProperties->SetField(Property.Key, Property.Value);
        }
        Schema->SetObjectField(TEXT("properties"), JsonProperties);
        TArray<TSharedPtr<FJsonValue>> JsonRequired;
        for (const FString& Field : Required)
        {
            JsonRequired.Add(MakeShared<FJsonValueString>(Field));
        }
        Schema->SetArrayField(TEXT("required"), JsonRequired);
        Schema->SetBoolField(TEXT("additionalProperties"), false);
        return Schema;
    }

    FModelContextProtocolToolResult Structured(const TSharedRef<FJsonObject>& Object)
    {
        const TSharedPtr<FJsonValue> Value = MakeShared<FJsonValueObject>(Object);
        return UE::ModelContextProtocol::MakeStructuredContentResult(Value);
    }

    FString RelativeToProject(FString Filename)
    {
        FPaths::MakePathRelativeTo(Filename, *FPaths::ProjectDir());
        FPaths::NormalizeFilename(Filename);
        return Filename;
    }

    FString ExportStatusName(const EUERingExportStatus Status)
    {
        switch (Status)
        {
        case EUERingExportStatus::Exported:
            return TEXT("exported");
        case EUERingExportStatus::Unchanged:
            return TEXT("unchanged");
        case EUERingExportStatus::Unsupported:
            return TEXT("unsupported");
        default:
            return TEXT("failed");
        }
    }

    int64 SerializedUtf8Bytes(const TSharedRef<FJsonObject>& Object)
    {
        const FModelContextProtocolToolResult ToolResult = Structured(Object);
        if (!ToolResult.JsonObject.IsValid())
        {
            return MAX_int64;
        }
        FString Json;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
        if (!FJsonSerializer::Serialize(ToolResult.JsonObject.ToSharedRef(), Writer))
        {
            return MAX_int64;
        }
        return FTCHARToUTF8(*Json).Length();
    }

    class FGameThreadTool : public IModelContextProtocolTool
    {
    public:
        virtual FModelContextProtocolToolResult Run(const TSharedPtr<FJsonObject>& Params) override final
        {
            if (!IsInGameThread())
            {
                return UE::ModelContextProtocol::MakeErrorResult(
                    TEXT("UE Ring MCP operations must execute on the Unreal game thread."));
            }
            return Execute(Params);
        }

        virtual void RunAsync(
            const FModelContextProtocolToolRequestId& RequestId,
            const TSharedPtr<FJsonObject>& Params,
            const FResultCallback& OnComplete) override final
        {
            const TSharedRef<IModelContextProtocolTool> Self = AsShared();
            auto Work = [Self, Params, OnComplete]()
            {
                OnComplete(Self->Run(Params));
            };
            if (IsInGameThread())
            {
                Work();
            }
            else
            {
                AsyncTask(ENamedThreads::GameThread, MoveTemp(Work));
            }
        }

    protected:
        virtual FModelContextProtocolToolResult Execute(const TSharedPtr<FJsonObject>& Params) const = 0;
    };

    class FGetSemanticTool final : public FGameThreadTool
    {
    public:
        virtual FString GetName() const override { return TEXT("ue_ring_get_semantic"); }
        virtual FString GetDescription() const override
        {
            return TEXT("Read an existing UE Ring semantic sidecar for a project content package. max_bytes limits the serialized structured response payload.");
        }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override
        {
            const int64 DefaultBytes = static_cast<int64>(GetDefault<UUERingSettings>()->MaxMcpSemanticMiB) * 1024 * 1024;
            return ObjectSchema({
                { TEXT("package_name"), MakeShared<FJsonValueObject>(StringProperty(TEXT("Long package name such as /Game/Blueprints/BP_Door."))) },
                { TEXT("include_content"), MakeShared<FJsonValueObject>(BooleanProperty(TEXT("Include parsed USEM JSON when it fits max_bytes."), true)) },
                { TEXT("max_bytes"), MakeShared<FJsonValueObject>(IntegerProperty(TEXT("Maximum serialized structured response payload size."), DefaultBytes, 1024, 64 * 1024 * 1024)) }
            }, { TEXT("package_name") });
        }

    protected:
        virtual FModelContextProtocolToolResult Execute(const TSharedPtr<FJsonObject>& Params) const override
        {
            FString PackageName;
            if (!Params.IsValid()
                || !Params->TryGetStringField(TEXT("package_name"), PackageName)
                || !FUERingExportManager::Get().IsSupportedPackageName(PackageName))
            {
                return UE::ModelContextProtocol::MakeErrorResult(
                    TEXT("package_name must be a valid project content long package name."));
            }
            FString SemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(PackageName, false);
            if (!IFileManager::Get().FileExists(*SemanticFile))
            {
                SemanticFile = FUERingExportManager::Get().GetSemanticFileForPackage(PackageName, true);
            }
            const int64 Bytes = IFileManager::Get().FileSize(*SemanticFile);
            if (Bytes < 0)
            {
                return UE::ModelContextProtocol::MakeErrorResult(
                    FString::Printf(TEXT("No semantic sidecar exists for %s. Export it first."), *PackageName));
            }

            bool bIncludeContent = true;
            Params->TryGetBoolField(TEXT("include_content"), bIncludeContent);
            double RequestedMaxBytes = static_cast<double>(GetDefault<UUERingSettings>()->MaxMcpSemanticMiB) * 1024.0 * 1024.0;
            Params->TryGetNumberField(TEXT("max_bytes"), RequestedMaxBytes);
            const int64 MaxBytes = FMath::Clamp<int64>(static_cast<int64>(RequestedMaxBytes), 1024, 64 * 1024 * 1024);

            const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("packageName"), PackageName);
            Result->SetStringField(TEXT("semanticFile"), RelativeToProject(SemanticFile));
            Result->SetNumberField(TEXT("semanticBytes"), Bytes);
            Result->SetNumberField(TEXT("maxResponseBytes"), MaxBytes);
            Result->SetNumberField(TEXT("responsePayloadBytes"), 0);
            const bool bOmitted = !bIncludeContent || Bytes > MaxBytes;
            Result->SetBoolField(TEXT("contentOmitted"), bOmitted);
            if (!bOmitted)
            {
                FString Json;
                TSharedPtr<FJsonObject> Semantic;
                if (!FFileHelper::LoadFileToString(Json, *SemanticFile)
                    || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Semantic)
                    || !Semantic.IsValid())
                {
                    return UE::ModelContextProtocol::MakeErrorResult(
                        FString::Printf(TEXT("Semantic sidecar is not valid JSON: %s"), *PackageName));
                }
                Result->SetObjectField(TEXT("semantic"), Semantic);
                const int64 PayloadBytes = SerializedUtf8Bytes(Result);
                if (PayloadBytes > MaxBytes)
                {
                    Result->RemoveField(TEXT("semantic"));
                    Result->SetBoolField(TEXT("contentOmitted"), true);
                    Result->SetStringField(
                        TEXT("omissionReason"),
                        FString::Printf(
                            TEXT("Serialized response payload exceeds max_bytes (%lld > %lld)."),
                            PayloadBytes,
                            MaxBytes));
                }
            }
            else if (bIncludeContent && Bytes > MaxBytes)
            {
                Result->SetStringField(
                    TEXT("omissionReason"),
                    FString::Printf(TEXT("Sidecar exceeds max_bytes (%lld > %lld)."), Bytes, MaxBytes));
            }
            int64 ResponseBytes = SerializedUtf8Bytes(Result);
            Result->SetNumberField(TEXT("responsePayloadBytes"), ResponseBytes);
            ResponseBytes = SerializedUtf8Bytes(Result);
            Result->SetNumberField(TEXT("responsePayloadBytes"), ResponseBytes);
            if (ResponseBytes > MaxBytes && Result->HasField(TEXT("semantic")))
            {
                Result->RemoveField(TEXT("semantic"));
                Result->SetBoolField(TEXT("contentOmitted"), true);
                Result->SetStringField(
                    TEXT("omissionReason"),
                    FString::Printf(
                        TEXT("Serialized response payload exceeds max_bytes (%lld > %lld)."),
                        ResponseBytes,
                        MaxBytes));
                Result->SetNumberField(TEXT("responsePayloadBytes"), 0);
                ResponseBytes = SerializedUtf8Bytes(Result);
                Result->SetNumberField(TEXT("responsePayloadBytes"), ResponseBytes);
            }
            return Structured(Result);
        }
    };

    class FExportAssetTool final : public FGameThreadTool
    {
    public:
        virtual FString GetName() const override { return TEXT("ue_ring_export_asset"); }
        virtual FString GetDescription() const override
        {
            return TEXT("Export or refresh UE Ring semantics for one saved project content package and optionally rebuild project indexes.");
        }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override
        {
            return ObjectSchema({
                { TEXT("package_name"), MakeShared<FJsonValueObject>(StringProperty(TEXT("Long package name such as /Game/Blueprints/BP_Door."))) },
                { TEXT("rebuild_indexes"), MakeShared<FJsonValueObject>(BooleanProperty(TEXT("Rebuild asset/dependency and C++ indexes after export."), true)) }
            }, { TEXT("package_name") });
        }

    protected:
        virtual FModelContextProtocolToolResult Execute(const TSharedPtr<FJsonObject>& Params) const override
        {
            FString PackageName;
            if (!Params.IsValid()
                || !Params->TryGetStringField(TEXT("package_name"), PackageName)
                || !FUERingExportManager::Get().IsSupportedPackageName(PackageName))
            {
                return UE::ModelContextProtocol::MakeErrorResult(
                    TEXT("package_name must be a valid project content long package name."));
            }

            IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
            TArray<FAssetData> Assets;
            Registry.GetAssetsByPackageName(FName(*PackageName), Assets, false);
            Assets.RemoveAll([](const FAssetData& Candidate)
            {
                return !FUERingExportManager::Get().CanExport(Candidate);
            });
            FUERingExportManager::Get().CanonicalizeAssetsByPackage(Assets);
            if (Assets.IsEmpty())
            {
                return UE::ModelContextProtocol::MakeErrorResult(
                    FString::Printf(TEXT("No supported asset was found in %s."), *PackageName));
            }

            const FUERingExportResult Export = FUERingExportManager::Get().ExportAsset(Assets[0]);
            if (!Export.IsSuccess())
            {
                return UE::ModelContextProtocol::MakeErrorResult(Export.Error);
            }
            bool bRebuildIndexes = true;
            Params->TryGetBoolField(TEXT("rebuild_indexes"), bRebuildIndexes);
            if (bRebuildIndexes)
            {
                FString Error;
                if (!FUERingIndexManager::UpdatePackages({ FName(*PackageName) }, Error)
                    || (Export.Status == EUERingExportStatus::Exported
                        && GetDefault<UUERingSettings>()->bIncludeCppIndex
                        && !FUERingCppIndexer::UpdatePackages({ FName(*PackageName) }, Error)))
                {
                    return UE::ModelContextProtocol::MakeErrorResult(Error);
                }
            }

            const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("packageName"), PackageName);
            Result->SetStringField(TEXT("status"), ExportStatusName(Export.Status));
            Result->SetStringField(TEXT("exporter"), Export.ExporterName);
            Result->SetStringField(TEXT("semanticFile"), RelativeToProject(Export.OutputFile));
            Result->SetBoolField(TEXT("indexesRebuilt"), bRebuildIndexes);
            return Structured(Result);
        }
    };

    class FValidateTool final : public FGameThreadTool
    {
    public:
        virtual FString GetName() const override { return TEXT("ue_ring_validate_semantics"); }
        virtual FString GetDescription() const override
        {
            return TEXT("Validate UE Ring sidecars for missing, stale, orphaned, or invalid semantic files.");
        }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override
        {
            return ObjectSchema(TMap<FString, TSharedPtr<FJsonValue>>());
        }

    protected:
        virtual FModelContextProtocolToolResult Execute(const TSharedPtr<FJsonObject>& Params) const override
        {
            const FUERingValidationReport Report = FUERingValidator::Validate();
            const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("valid"), Report.IsValid());
            Result->SetNumberField(TEXT("checked"), Report.Checked);
            Result->SetNumberField(TEXT("missing"), Report.Missing);
            Result->SetNumberField(TEXT("stale"), Report.Stale);
            Result->SetNumberField(TEXT("orphan"), Report.Orphan);
            Result->SetNumberField(TEXT("invalid"), Report.Invalid);
            TArray<TSharedPtr<FJsonValue>> Messages;
            for (const FString& Message : Report.Messages)
            {
                Messages.Add(MakeShared<FJsonValueString>(Message));
            }
            Result->SetArrayField(TEXT("messages"), Messages);
            return Structured(Result);
        }
    };

    class FQueryGraphTool final : public FGameThreadTool
    {
    public:
        virtual FString GetName() const override { return TEXT("ue_ring_query_graph"); }
        virtual FString GetDescription() const override
        {
            return TEXT("Query incoming or outgoing edges from the UE Ring unified project semantic graph SQLite index.");
        }
        virtual TSharedPtr<FJsonObject> GetInputJsonSchema() const override
        {
            const TSharedRef<FJsonObject> Direction = StringProperty(
                TEXT("Edge direction: outgoing, incoming, or both."));
            Direction->SetArrayField(TEXT("enum"), {
                MakeShared<FJsonValueString>(TEXT("outgoing")),
                MakeShared<FJsonValueString>(TEXT("incoming")),
                MakeShared<FJsonValueString>(TEXT("both"))
            });
            Direction->SetStringField(TEXT("default"), TEXT("both"));
            return ObjectSchema({
                { TEXT("node_id"), MakeShared<FJsonValueObject>(StringProperty(TEXT("Exact unified graph node ID."))) },
                { TEXT("direction"), MakeShared<FJsonValueObject>(Direction) },
                { TEXT("relation"), MakeShared<FJsonValueObject>(StringProperty(TEXT("Optional exact relation filter."))) },
                { TEXT("limit"), MakeShared<FJsonValueObject>(IntegerProperty(TEXT("Maximum returned edges."), 100, 1, 500)) }
            }, { TEXT("node_id") });
        }

    protected:
        virtual FModelContextProtocolToolResult Execute(const TSharedPtr<FJsonObject>& Params) const override
        {
            FString NodeId;
            if (!Params.IsValid()
                || !Params->TryGetStringField(TEXT("node_id"), NodeId)
                || NodeId.IsEmpty()
                || NodeId.Len() > 4096)
            {
                return UE::ModelContextProtocol::MakeErrorResult(TEXT("node_id must be a non-empty unified graph node ID."));
            }
            FString Direction = TEXT("both");
            Params->TryGetStringField(TEXT("direction"), Direction);
            if (Direction != TEXT("outgoing") && Direction != TEXT("incoming") && Direction != TEXT("both"))
            {
                return UE::ModelContextProtocol::MakeErrorResult(
                    TEXT("direction must be outgoing, incoming, or both."));
            }
            FString Relation;
            Params->TryGetStringField(TEXT("relation"), Relation);
            double RequestedLimit = 100.0;
            Params->TryGetNumberField(TEXT("limit"), RequestedLimit);
            const int64 Limit = FMath::Clamp<int64>(static_cast<int64>(RequestedLimit), 1, 500);

            const FString DatabaseFile = FPaths::Combine(
                FUERingExportManager::Get().GetOutputRoot(),
                TEXT("index/project.uesem.sqlite"));
            FSQLiteDatabase Database;
            if (!Database.Open(*DatabaseFile, ESQLiteDatabaseOpenMode::ReadOnly))
            {
                return UE::ModelContextProtocol::MakeErrorResult(
                    TEXT("The project semantic SQLite index is unavailable. Run a full export first."));
            }
            ON_SCOPE_EXIT
            {
                Database.Close();
            };

            bool bNodeExists = false;
            FSQLitePreparedStatement NodeQuery(
                Database,
                TEXT("SELECT 1 FROM graph_nodes WHERE node_id = ? LIMIT 1"));
            if (!NodeQuery.IsValid() || !NodeQuery.SetBindingValueByIndex(1, NodeId))
            {
                return UE::ModelContextProtocol::MakeErrorResult(TEXT("Could not query graph_nodes."));
            }
            const int64 NodeRows = NodeQuery.Execute([&bNodeExists](const FSQLitePreparedStatement&)
            {
                bNodeExists = true;
                return ESQLitePreparedStatementExecuteRowResult::Stop;
            });
            if (NodeRows == INDEX_NONE)
            {
                return UE::ModelContextProtocol::MakeErrorResult(TEXT("Could not query graph_nodes."));
            }

            FString DirectionClause;
            if (Direction == TEXT("outgoing")) DirectionClause = TEXT("source_node = ?");
            else if (Direction == TEXT("incoming")) DirectionClause = TEXT("target_node = ?");
            else DirectionClause = TEXT("(source_node = ? OR target_node = ?)");
            const FString Sql = FString::Printf(
                TEXT("SELECT source_node, target_node, relation, qualifier, confidence, evidence_source, "
                     "evidence_pointer, source_node_id, source_title, source_pin_id, target_pin_id "
                     "FROM graph_edges WHERE %s AND (? = '' OR relation = ?) "
                     "ORDER BY relation, source_node, target_node, qualifier LIMIT ?"),
                *DirectionClause);
            FSQLitePreparedStatement EdgeQuery(Database, *Sql);
            if (!EdgeQuery.IsValid())
            {
                return UE::ModelContextProtocol::MakeErrorResult(TEXT("Could not prepare graph edge query."));
            }
            int32 Binding = 1;
            if (!EdgeQuery.SetBindingValueByIndex(Binding++, NodeId)
                || (Direction == TEXT("both") && !EdgeQuery.SetBindingValueByIndex(Binding++, NodeId))
                || !EdgeQuery.SetBindingValueByIndex(Binding++, Relation)
                || !EdgeQuery.SetBindingValueByIndex(Binding++, Relation)
                || !EdgeQuery.SetBindingValueByIndex(Binding, Limit))
            {
                return UE::ModelContextProtocol::MakeErrorResult(TEXT("Could not bind graph edge query."));
            }

            TArray<TSharedPtr<FJsonValue>> Edges;
            const int64 EdgeRows = EdgeQuery.Execute([&Edges](const FSQLitePreparedStatement& Row)
            {
                FString Source;
                FString Target;
                FString EdgeRelation;
                FString Qualifier;
                double Confidence = 0.0;
                FString EvidenceSource;
                FString EvidencePointer;
                FString SourceNodeId;
                FString SourceTitle;
                FString SourcePinId;
                FString TargetPinId;
                Row.GetColumnValueByIndex(0, Source);
                Row.GetColumnValueByIndex(1, Target);
                Row.GetColumnValueByIndex(2, EdgeRelation);
                Row.GetColumnValueByIndex(3, Qualifier);
                Row.GetColumnValueByIndex(4, Confidence);
                Row.GetColumnValueByIndex(5, EvidenceSource);
                Row.GetColumnValueByIndex(6, EvidencePointer);
                Row.GetColumnValueByIndex(7, SourceNodeId);
                Row.GetColumnValueByIndex(8, SourceTitle);
                Row.GetColumnValueByIndex(9, SourcePinId);
                Row.GetColumnValueByIndex(10, TargetPinId);
                const TSharedRef<FJsonObject> Edge = MakeShared<FJsonObject>();
                Edge->SetStringField(TEXT("from"), Source);
                Edge->SetStringField(TEXT("to"), Target);
                Edge->SetStringField(TEXT("relation"), EdgeRelation);
                Edge->SetNumberField(TEXT("confidence"), Confidence);
                Edge->SetStringField(TEXT("evidenceSource"), EvidenceSource);
                if (!Qualifier.IsEmpty()) Edge->SetStringField(TEXT("qualifier"), Qualifier);
                if (!EvidencePointer.IsEmpty()) Edge->SetStringField(TEXT("evidencePointer"), EvidencePointer);
                if (!SourceNodeId.IsEmpty()) Edge->SetStringField(TEXT("sourceNodeId"), SourceNodeId);
                if (!SourceTitle.IsEmpty()) Edge->SetStringField(TEXT("sourceTitle"), SourceTitle);
                if (!SourcePinId.IsEmpty()) Edge->SetStringField(TEXT("sourcePinId"), SourcePinId);
                if (!TargetPinId.IsEmpty()) Edge->SetStringField(TEXT("targetPinId"), TargetPinId);
                Edges.Add(MakeShared<FJsonValueObject>(Edge));
                return ESQLitePreparedStatementExecuteRowResult::Continue;
            });
            if (EdgeRows == INDEX_NONE)
            {
                return UE::ModelContextProtocol::MakeErrorResult(TEXT("Could not execute graph edge query."));
            }

            const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("nodeId"), NodeId);
            Result->SetBoolField(TEXT("nodeExists"), bNodeExists);
            Result->SetStringField(TEXT("direction"), Direction);
            if (!Relation.IsEmpty()) Result->SetStringField(TEXT("relation"), Relation);
            Result->SetNumberField(TEXT("count"), Edges.Num());
            Result->SetArrayField(TEXT("edges"), Edges);
            return Structured(Result);
        }
    };
}

void FUERingMcpIntegration::Initialize()
{
    IModelContextProtocolModule* Module = IModelContextProtocolModule::Get();
    if (Module == nullptr)
    {
        UE_LOG(LogUERingMcp, Warning, TEXT("Official ModelContextProtocol module is unavailable; UE Ring MCP tools were not registered."));
        return;
    }
    Tools = {
        MakeShared<UERingMcp::FGetSemanticTool>(),
        MakeShared<UERingMcp::FExportAssetTool>(),
        MakeShared<UERingMcp::FValidateTool>(),
        MakeShared<UERingMcp::FQueryGraphTool>()
    };
    RefreshToolsHandle = Module->OnRefreshTools().AddRaw(this, &FUERingMcpIntegration::RegisterTools);
    RegisterTools();
}

void FUERingMcpIntegration::Shutdown()
{
    if (IModelContextProtocolModule* Module = IModelContextProtocolModule::Get())
    {
        if (RefreshToolsHandle.IsValid())
        {
            Module->OnRefreshTools().Remove(RefreshToolsHandle);
        }
        for (const TSharedRef<IModelContextProtocolTool>& Tool : Tools)
        {
            Module->RemoveTool(Tool);
        }
    }
    RefreshToolsHandle.Reset();
    Tools.Reset();
}

void FUERingMcpIntegration::RegisterTools()
{
    IModelContextProtocolModule* Module = IModelContextProtocolModule::Get();
    if (Module == nullptr)
    {
        return;
    }
    for (const TSharedRef<IModelContextProtocolTool>& Tool : Tools)
    {
        if (!Module->AddTool(Tool))
        {
            UE_LOG(LogUERingMcp, Warning, TEXT("Could not register MCP tool '%s'; the name is already in use."), *Tool->GetName());
        }
    }
}
