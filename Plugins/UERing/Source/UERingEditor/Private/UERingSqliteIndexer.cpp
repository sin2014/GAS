#include "UERingSqliteIndexer.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "SQLiteDatabase.h"
#include "SQLitePreparedStatement.h"

namespace UERingSqliteIndexer
{
    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FInsertMetadata,
        "INSERT INTO metadata(key, value) VALUES (?, ?)",
        SQLITE_PREPARED_STATEMENT_BINDINGS(FString, FString));

    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FUpsertMetadata,
        "INSERT OR REPLACE INTO metadata(key, value) VALUES (?, ?)",
        SQLITE_PREPARED_STATEMENT_BINDINGS(FString, FString));

    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FInsertAsset,
        "INSERT INTO assets(package_name, object_path, asset_class, status, semantic_kind, exporter, "
        "source_file, semantic_file, source_hash, semantic_hash, owner_module, primary_asset_id, "
        "recoverability, reconstruction_confidence, dependency_count, referencer_count, "
        "semantic_bytes, omission_count, exported_at_utc, search_text) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        SQLITE_PREPARED_STATEMENT_BINDINGS(
            FString,
            FString,
            FString,
            FString,
            FString,
            FString,
            FString,
            FString,
            FString,
            FString,
            FString,
            FString,
            FString,
            double,
            int64,
            int64,
            int64,
            int64,
            FString,
            FString));

    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FInsertTag,
        "INSERT OR IGNORE INTO asset_tags(package_name, tag) VALUES (?, ?)",
        SQLITE_PREPARED_STATEMENT_BINDINGS(FString, FString));

    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FInsertDomain,
        "INSERT OR IGNORE INTO asset_domains(package_name, domain) VALUES (?, ?)",
        SQLITE_PREPARED_STATEMENT_BINDINGS(FString, FString));

    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FInsertDependency,
        "INSERT OR IGNORE INTO dependencies(source_package, target_package, kind) VALUES (?, ?, ?)",
        SQLITE_PREPARED_STATEMENT_BINDINGS(FString, FString, FString));

    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FInsertGraphNode,
        "INSERT INTO graph_nodes(node_id, kind, subtype, label, package_name, graph_id, raw_node_id, search_text) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        SQLITE_PREPARED_STATEMENT_BINDINGS(
            FString, FString, FString, FString, FString, FString, FString, FString));

    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FUpsertGraphNode,
        "INSERT OR IGNORE INTO graph_nodes(node_id, kind, subtype, label, package_name, graph_id, raw_node_id, search_text) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        SQLITE_PREPARED_STATEMENT_BINDINGS(
            FString, FString, FString, FString, FString, FString, FString, FString));

    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FInsertGraphEdge,
        "INSERT INTO graph_edges(source_node, target_node, relation, qualifier, confidence, "
        "evidence_source, evidence_pointer, contributor_package, source_node_id, source_title, "
        "source_pin_id, target_pin_id) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        SQLITE_PREPARED_STATEMENT_BINDINGS(
            FString, FString, FString, FString, double, FString, FString, FString, FString, FString, FString, FString));

    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FDeleteAsset,
        "DELETE FROM assets WHERE package_name = ?",
        SQLITE_PREPARED_STATEMENT_BINDINGS(FString));
    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FDeleteTags,
        "DELETE FROM asset_tags WHERE package_name = ?",
        SQLITE_PREPARED_STATEMENT_BINDINGS(FString));
    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FDeleteDomains,
        "DELETE FROM asset_domains WHERE package_name = ?",
        SQLITE_PREPARED_STATEMENT_BINDINGS(FString));
    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FDeleteDependencies,
        "DELETE FROM dependencies WHERE source_package = ?",
        SQLITE_PREPARED_STATEMENT_BINDINGS(FString));
    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FDeleteGraphEdges,
        "DELETE FROM graph_edges WHERE contributor_package = ?",
        SQLITE_PREPARED_STATEMENT_BINDINGS(FString));
    SQLITE_PREPARED_STATEMENT_BINDINGS_ONLY(
        FDeleteGraphNodes,
        "DELETE FROM graph_nodes WHERE package_name = ?",
        SQLITE_PREPARED_STATEMENT_BINDINGS(FString));

    FString StringField(const TSharedRef<FJsonObject>& Object, const TCHAR* Field)
    {
        FString Value;
        Object->TryGetStringField(Field, Value);
        return Value;
    }

    int64 IntegerField(const TSharedRef<FJsonObject>& Object, const TCHAR* Field)
    {
        double Value = 0.0;
        Object->TryGetNumberField(Field, Value);
        return static_cast<int64>(Value);
    }

    double NumberField(const TSharedRef<FJsonObject>& Object, const TCHAR* Field)
    {
        double Value = 0.0;
        Object->TryGetNumberField(Field, Value);
        return Value;
    }

    bool Fail(FSQLiteDatabase& Database, const FString& Prefix, FString& OutError)
    {
        const FString DatabaseError = Database.GetLastError();
        Database.Execute(TEXT("ROLLBACK"));
        OutError = FString::Printf(TEXT("%s: %s"), *Prefix, *DatabaseError);
        Database.Close();
        return false;
    }
}

FString FUERingSqliteIndexer::GetDatabaseFile(const FString& IndexDirectory)
{
    return FPaths::Combine(IndexDirectory, TEXT("project.uesem.sqlite"));
}

bool FUERingSqliteIndexer::IsIncrementalDatabaseCompatible(const FString& IndexDirectory)
{
    FSQLiteDatabase Database;
    int32 UserVersion = 0;
    const bool bCompatible = Database.Open(
            *GetDatabaseFile(IndexDirectory), ESQLiteDatabaseOpenMode::ReadOnly)
        && Database.GetUserVersion(UserVersion)
        && UserVersion == 6;
    Database.Close();
    return bCompatible;
}

bool FUERingSqliteIndexer::FindContributorsTargetingAssets(
    const FString& IndexDirectory,
    const TSet<FString>& PackageNames,
    TSet<FString>& OutContributors)
{
    if (PackageNames.IsEmpty()) return true;
    FSQLiteDatabase Database;
    int32 UserVersion = 0;
    if (!Database.Open(*GetDatabaseFile(IndexDirectory), ESQLiteDatabaseOpenMode::ReadOnly)
        || !Database.GetUserVersion(UserVersion)
        || UserVersion != 6)
    {
        Database.Close();
        return false;
    }
    for (const FString& PackageName : PackageNames)
    {
        FSQLitePreparedStatement Query(
            Database,
            TEXT("SELECT DISTINCT contributor_package FROM graph_edges WHERE target_node = ?"));
        if (!Query.IsValid() || !Query.SetBindingValueByIndex(1, TEXT("asset:") + PackageName))
        {
            Database.Close();
            return false;
        }
        const int64 Rows = Query.Execute(
            [&OutContributors](const FSQLitePreparedStatement& Row)
            {
                FString Contributor;
                if (!Row.GetColumnValueByIndex(0, Contributor))
                {
                    return ESQLitePreparedStatementExecuteRowResult::Error;
                }
                if (!Contributor.IsEmpty()) OutContributors.Add(Contributor);
                return ESQLitePreparedStatementExecuteRowResult::Continue;
            });
        Query.Destroy();
        if (Rows < 0)
        {
            Database.Close();
            return false;
        }
    }
    return Database.Close();
}

bool FUERingSqliteIndexer::Rebuild(
    const FString& IndexDirectory,
    const TSharedRef<FJsonObject>& ProjectIndex,
    const TSharedRef<FJsonObject>& DependencyGraph,
    const TSharedRef<FJsonObject>& ProjectGraph,
    FString& OutError)
{
    using namespace UERingSqliteIndexer;

    if (!IFileManager::Get().MakeDirectory(*IndexDirectory, true))
    {
        OutError = FString::Printf(TEXT("Could not create SQLite index directory: %s"), *IndexDirectory);
        return false;
    }

    const FString DatabaseFile = GetDatabaseFile(IndexDirectory);
    const FString TempFile = DatabaseFile + TEXT(".tmp");
    IFileManager::Get().Delete(*TempFile, false, true);

    FSQLiteDatabase Database;
    if (!Database.Open(*TempFile, ESQLiteDatabaseOpenMode::ReadWriteCreate))
    {
        OutError = FString::Printf(TEXT("Could not open SQLite index %s: %s"), *TempFile, *Database.GetLastError());
        return false;
    }

    const TArray<const TCHAR*> SchemaStatements = {
        TEXT("PRAGMA journal_mode=DELETE"),
        TEXT("PRAGMA synchronous=FULL"),
        TEXT("CREATE TABLE metadata(key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
        TEXT("CREATE TABLE assets("
        "package_name TEXT PRIMARY KEY, object_path TEXT NOT NULL, asset_class TEXT NOT NULL, "
        "status TEXT NOT NULL, semantic_kind TEXT NOT NULL, exporter TEXT NOT NULL, "
        "source_file TEXT NOT NULL, semantic_file TEXT NOT NULL, source_hash TEXT NOT NULL, "
        "semantic_hash TEXT NOT NULL, owner_module TEXT NOT NULL, primary_asset_id TEXT NOT NULL, "
        "recoverability TEXT NOT NULL, reconstruction_confidence REAL NOT NULL, "
        "dependency_count INTEGER NOT NULL, referencer_count INTEGER NOT NULL, "
        "semantic_bytes INTEGER NOT NULL, omission_count INTEGER NOT NULL, "
        "exported_at_utc TEXT NOT NULL, search_text TEXT NOT NULL)"),
        TEXT("CREATE TABLE asset_tags("
        "package_name TEXT NOT NULL, tag TEXT NOT NULL, PRIMARY KEY(package_name, tag))"),
        TEXT("CREATE TABLE asset_domains("
        "package_name TEXT NOT NULL, domain TEXT NOT NULL, PRIMARY KEY(package_name, domain))"),
        TEXT("CREATE TABLE dependencies("
        "source_package TEXT NOT NULL, target_package TEXT NOT NULL, kind TEXT NOT NULL, "
        "PRIMARY KEY(source_package, target_package, kind))"),
        TEXT("CREATE TABLE graph_nodes("
        "node_id TEXT PRIMARY KEY, kind TEXT NOT NULL, subtype TEXT NOT NULL, label TEXT NOT NULL, "
        "package_name TEXT NOT NULL, graph_id TEXT NOT NULL, raw_node_id TEXT NOT NULL, "
        "search_text TEXT NOT NULL)"),
        TEXT("CREATE TABLE graph_edges("
        "source_node TEXT NOT NULL, target_node TEXT NOT NULL, relation TEXT NOT NULL, "
        "qualifier TEXT NOT NULL, confidence REAL NOT NULL, evidence_source TEXT NOT NULL, "
        "evidence_pointer TEXT NOT NULL, contributor_package TEXT NOT NULL, "
        "source_node_id TEXT NOT NULL, source_title TEXT NOT NULL, "
        "source_pin_id TEXT NOT NULL, target_pin_id TEXT NOT NULL, "
        "PRIMARY KEY(source_node, target_node, relation, qualifier, evidence_pointer, "
        "source_node_id, source_pin_id, target_pin_id))"),
        TEXT("CREATE INDEX assets_by_class ON assets(asset_class, package_name)"),
        TEXT("CREATE INDEX assets_by_kind ON assets(semantic_kind, package_name)"),
        TEXT("CREATE INDEX assets_by_status ON assets(status, package_name)"),
        TEXT("CREATE INDEX assets_by_module ON assets(owner_module, package_name)"),
        TEXT("CREATE INDEX assets_by_primary_id ON assets(primary_asset_id, package_name)"),
        TEXT("CREATE INDEX assets_by_recoverability ON assets(recoverability, reconstruction_confidence, package_name)"),
        TEXT("CREATE INDEX assets_by_semantic_bytes ON assets(semantic_bytes DESC, package_name)"),
        TEXT("CREATE INDEX tags_by_tag ON asset_tags(tag, package_name)"),
        TEXT("CREATE INDEX domains_by_domain ON asset_domains(domain, package_name)"),
        TEXT("CREATE INDEX dependencies_by_target ON dependencies(target_package, kind, source_package)"),
        TEXT("CREATE INDEX graph_nodes_by_kind ON graph_nodes(kind, subtype, node_id)"),
        TEXT("CREATE INDEX graph_nodes_by_package ON graph_nodes(package_name, kind, node_id)"),
        TEXT("CREATE INDEX graph_edges_by_source ON graph_edges(source_node, relation, target_node)"),
        TEXT("CREATE INDEX graph_edges_by_target ON graph_edges(target_node, relation, source_node)"),
        TEXT("CREATE INDEX graph_edges_by_contributor ON graph_edges(contributor_package)"),
        TEXT("BEGIN IMMEDIATE")
    };
    for (const TCHAR* Statement : SchemaStatements)
    {
        if (!Database.Execute(Statement))
        {
            return Fail(Database, TEXT("Could not create SQLite schema"), OutError);
        }
    }

    FInsertMetadata InsertMetadata(Database, ESQLitePreparedStatementFlags::Persistent);
    FInsertAsset InsertAsset(Database, ESQLitePreparedStatementFlags::Persistent);
    FInsertTag InsertTag(Database, ESQLitePreparedStatementFlags::Persistent);
    FInsertDomain InsertDomain(Database, ESQLitePreparedStatementFlags::Persistent);
    FInsertDependency InsertDependency(Database, ESQLitePreparedStatementFlags::Persistent);
    FInsertGraphNode InsertGraphNode(Database, ESQLitePreparedStatementFlags::Persistent);
    FInsertGraphEdge InsertGraphEdge(Database, ESQLitePreparedStatementFlags::Persistent);
    auto FailWithStatements = [&](const FString& Prefix)
    {
        InsertMetadata.Destroy();
        InsertAsset.Destroy();
        InsertTag.Destroy();
        InsertDomain.Destroy();
        InsertDependency.Destroy();
        InsertGraphNode.Destroy();
        InsertGraphEdge.Destroy();
        return Fail(Database, Prefix, OutError);
    };
    if (!InsertMetadata.IsValid() || !InsertAsset.IsValid() || !InsertTag.IsValid()
        || !InsertDomain.IsValid() || !InsertDependency.IsValid()
        || !InsertGraphNode.IsValid() || !InsertGraphEdge.IsValid())
    {
        return FailWithStatements(TEXT("Could not prepare SQLite statements"));
    }

    const TSharedPtr<FJsonObject>* ProjectPtr = nullptr;
    const TSharedPtr<FJsonObject>* EnginePtr = nullptr;
    ProjectIndex->TryGetObjectField(TEXT("project"), ProjectPtr);
    ProjectIndex->TryGetObjectField(TEXT("engine"), EnginePtr);
    const FString ProjectName = ProjectPtr != nullptr ? StringField((*ProjectPtr).ToSharedRef(), TEXT("name")) : FString();
    const FString EngineVersion = EnginePtr != nullptr ? StringField((*EnginePtr).ToSharedRef(), TEXT("version")) : FString();
    if (!InsertMetadata.BindAndExecute(TEXT("schema"), StringField(ProjectIndex, TEXT("schema")))
        || !InsertMetadata.BindAndExecute(TEXT("schema_version"), StringField(ProjectIndex, TEXT("schemaVersion")))
        || !InsertMetadata.BindAndExecute(TEXT("generated_at_utc"), StringField(ProjectIndex, TEXT("generatedAtUtc")))
        || !InsertMetadata.BindAndExecute(TEXT("project_name"), ProjectName)
        || !InsertMetadata.BindAndExecute(TEXT("engine_version"), EngineVersion))
    {
        return FailWithStatements(TEXT("Could not insert SQLite metadata"));
    }

    const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
    if (ProjectIndex->TryGetArrayField(TEXT("assets"), Assets) && Assets != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Assets)
        {
            const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(EntryPtr) || EntryPtr == nullptr)
            {
                continue;
            }
            const TSharedRef<FJsonObject> Entry = (*EntryPtr).ToSharedRef();
            const FString PackageName = StringField(Entry, TEXT("packageName"));
            const FString ObjectPath = StringField(Entry, TEXT("objectPath"));
            const FString AssetClass = StringField(Entry, TEXT("assetClass"));
            const FString Status = StringField(Entry, TEXT("status"));
            const FString SemanticKind = StringField(Entry, TEXT("semanticKind"));
            const FString Exporter = StringField(Entry, TEXT("exporter"));
            const FString OwnerModule = StringField(Entry, TEXT("ownerModule"));
            const FString PrimaryAssetId = StringField(Entry, TEXT("primaryAssetId"));
            FString SearchText = FString::Printf(
                TEXT("%s %s %s %s %s %s %s"),
                *PackageName,
                *ObjectPath,
                *AssetClass,
                *SemanticKind,
                *Exporter,
                *OwnerModule,
                *PrimaryAssetId);

            const TArray<TSharedPtr<FJsonValue>>* Domains = nullptr;
            if (Entry->TryGetArrayField(TEXT("domains"), Domains) && Domains != nullptr)
            {
                for (const TSharedPtr<FJsonValue>& DomainValue : *Domains)
                {
                    FString Domain;
                    if (DomainValue.IsValid() && DomainValue->TryGetString(Domain))
                    {
                        SearchText += TEXT(" ") + Domain;
                        if (!InsertDomain.BindAndExecute(PackageName, Domain))
                        {
                            return FailWithStatements(TEXT("Could not insert SQLite asset domain"));
                        }
                    }
                }
            }

            const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
            if (Entry->TryGetArrayField(TEXT("assetTags"), Tags) && Tags != nullptr)
            {
                for (const TSharedPtr<FJsonValue>& TagValue : *Tags)
                {
                    FString Tag;
                    if (TagValue.IsValid() && TagValue->TryGetString(Tag))
                    {
                        SearchText += TEXT(" ") + Tag;
                        if (!InsertTag.BindAndExecute(PackageName, Tag))
                        {
                            return FailWithStatements(TEXT("Could not insert SQLite asset tag"));
                        }
                    }
                }
            }

            if (!InsertAsset.BindAndExecute(
                PackageName,
                ObjectPath,
                AssetClass,
                Status,
                SemanticKind,
                Exporter,
                StringField(Entry, TEXT("sourceFile")),
                StringField(Entry, TEXT("semanticFile")),
                StringField(Entry, TEXT("sourceHash")),
                StringField(Entry, TEXT("semanticHash")),
                OwnerModule,
                PrimaryAssetId,
                StringField(Entry, TEXT("recoverability")),
                NumberField(Entry, TEXT("reconstructionConfidence")),
                IntegerField(Entry, TEXT("dependencyCount")),
                IntegerField(Entry, TEXT("referencerCount")),
                IntegerField(Entry, TEXT("semanticBytes")),
                IntegerField(Entry, TEXT("omissionCount")),
                StringField(Entry, TEXT("exportedAtUtc")),
                SearchText))
            {
                return FailWithStatements(TEXT("Could not insert SQLite asset"));
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Edges = nullptr;
    if (DependencyGraph->TryGetArrayField(TEXT("edges"), Edges) && Edges != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Edges)
        {
            const TSharedPtr<FJsonObject>* EdgePtr = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(EdgePtr) || EdgePtr == nullptr)
            {
                continue;
            }
            const TSharedRef<FJsonObject> Edge = (*EdgePtr).ToSharedRef();
            if (!InsertDependency.BindAndExecute(
                StringField(Edge, TEXT("from")),
                StringField(Edge, TEXT("to")),
                StringField(Edge, TEXT("type"))))
            {
                return FailWithStatements(TEXT("Could not insert SQLite dependency"));
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* GraphNodes = nullptr;
    if (ProjectGraph->TryGetArrayField(TEXT("nodes"), GraphNodes) && GraphNodes != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *GraphNodes)
        {
            const TSharedPtr<FJsonObject>* NodePtr = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(NodePtr) || NodePtr == nullptr) continue;
            const TSharedRef<FJsonObject> Node = (*NodePtr).ToSharedRef();
            const FString NodeId = StringField(Node, TEXT("id"));
            const FString Kind = StringField(Node, TEXT("kind"));
            const FString Subtype = StringField(Node, TEXT("subtype"));
            const FString Label = StringField(Node, TEXT("label"));
            const FString PackageName = StringField(Node, TEXT("packageName"));
            const FString SearchText = NodeId + TEXT(" ") + Kind + TEXT(" ") + Subtype
                + TEXT(" ") + Label + TEXT(" ") + PackageName;
            if (!InsertGraphNode.BindAndExecute(
                    NodeId,
                    Kind,
                    Subtype,
                    Label,
                    PackageName,
                    StringField(Node, TEXT("graphId")),
                    StringField(Node, TEXT("rawNodeId")),
                    SearchText))
            {
                return FailWithStatements(TEXT("Could not insert unified graph node"));
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* GraphEdges = nullptr;
    if (ProjectGraph->TryGetArrayField(TEXT("edges"), GraphEdges) && GraphEdges != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *GraphEdges)
        {
            const TSharedPtr<FJsonObject>* EdgePtr = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(EdgePtr) || EdgePtr == nullptr) continue;
            const TSharedRef<FJsonObject> Edge = (*EdgePtr).ToSharedRef();
            if (!InsertGraphEdge.BindAndExecute(
                StringField(Edge, TEXT("from")),
                StringField(Edge, TEXT("to")),
                StringField(Edge, TEXT("relation")),
                StringField(Edge, TEXT("qualifier")),
                NumberField(Edge, TEXT("confidence")),
                StringField(Edge, TEXT("evidenceSource")),
                StringField(Edge, TEXT("evidencePointer")),
                StringField(Edge, TEXT("contributorPackage")),
                StringField(Edge, TEXT("sourceNodeId")),
                StringField(Edge, TEXT("sourceTitle")),
                StringField(Edge, TEXT("sourcePinId")),
                StringField(Edge, TEXT("targetPinId"))))
            {
                return FailWithStatements(TEXT("Could not insert unified graph edge"));
            }
        }
    }

    if (!Database.Execute(TEXT("COMMIT")) || !Database.PerformQuickIntegrityCheck())
    {
        return FailWithStatements(TEXT("Could not finalize SQLite index"));
    }
    if (!Database.SetUserVersion(6))
    {
        return FailWithStatements(TEXT("Could not set SQLite index version"));
    }
    InsertMetadata.Destroy();
    InsertAsset.Destroy();
    InsertTag.Destroy();
    InsertDomain.Destroy();
    InsertDependency.Destroy();
    InsertGraphNode.Destroy();
    InsertGraphEdge.Destroy();
    if (!Database.Close())
    {
        OutError = FString::Printf(TEXT("Could not close SQLite index: %s"), *Database.GetLastError());
        IFileManager::Get().Delete(*TempFile, false, true);
        return false;
    }
    if (!IFileManager::Get().Move(*DatabaseFile, *TempFile, true, true, false, true))
    {
        IFileManager::Get().Delete(*TempFile, false, true);
        OutError = FString::Printf(TEXT("Could not replace SQLite index: %s"), *DatabaseFile);
        return false;
    }
    return true;
}

bool FUERingSqliteIndexer::Update(
    const FString& IndexDirectory,
    const TSharedRef<FJsonObject>& ProjectIndex,
    const TSharedRef<FJsonObject>& DependencyGraph,
    const TSharedRef<FJsonObject>& ProjectGraph,
    const TSet<FString>& IndexPackages,
    const TSet<FString>& ContributorPackages,
    FString& OutError)
{
    using namespace UERingSqliteIndexer;

    if (IndexPackages.IsEmpty() && ContributorPackages.IsEmpty()) return true;
    const FString DatabaseFile = GetDatabaseFile(IndexDirectory);
    FSQLiteDatabase Database;
    int32 UserVersion = 0;
    if (!Database.Open(*DatabaseFile, ESQLiteDatabaseOpenMode::ReadWrite)
        || !Database.GetUserVersion(UserVersion)
        || UserVersion != 6)
    {
        Database.Close();
        OutError = TEXT("SQLite index is not compatible with package-scoped updates; run a full index rebuild.");
        return false;
    }
    if (!Database.Execute(TEXT("PRAGMA synchronous=FULL"))
        || !Database.Execute(TEXT("BEGIN IMMEDIATE")))
    {
        return Fail(Database, TEXT("Could not begin incremental SQLite update"), OutError);
    }

    FUpsertMetadata UpsertMetadata(Database, ESQLitePreparedStatementFlags::Persistent);
    FInsertAsset InsertAsset(Database, ESQLitePreparedStatementFlags::Persistent);
    FInsertTag InsertTag(Database, ESQLitePreparedStatementFlags::Persistent);
    FInsertDomain InsertDomain(Database, ESQLitePreparedStatementFlags::Persistent);
    FInsertDependency InsertDependency(Database, ESQLitePreparedStatementFlags::Persistent);
    FUpsertGraphNode UpsertGraphNode(Database, ESQLitePreparedStatementFlags::Persistent);
    FInsertGraphEdge InsertGraphEdge(Database, ESQLitePreparedStatementFlags::Persistent);
    FDeleteAsset DeleteAsset(Database, ESQLitePreparedStatementFlags::Persistent);
    FDeleteTags DeleteTags(Database, ESQLitePreparedStatementFlags::Persistent);
    FDeleteDomains DeleteDomains(Database, ESQLitePreparedStatementFlags::Persistent);
    FDeleteDependencies DeleteDependencies(Database, ESQLitePreparedStatementFlags::Persistent);
    FDeleteGraphEdges DeleteGraphEdges(Database, ESQLitePreparedStatementFlags::Persistent);
    FDeleteGraphNodes DeleteGraphNodes(Database, ESQLitePreparedStatementFlags::Persistent);

    auto DestroyStatements = [&]()
    {
        UpsertMetadata.Destroy();
        InsertAsset.Destroy();
        InsertTag.Destroy();
        InsertDomain.Destroy();
        InsertDependency.Destroy();
        UpsertGraphNode.Destroy();
        InsertGraphEdge.Destroy();
        DeleteAsset.Destroy();
        DeleteTags.Destroy();
        DeleteDomains.Destroy();
        DeleteDependencies.Destroy();
        DeleteGraphEdges.Destroy();
        DeleteGraphNodes.Destroy();
    };
    auto FailUpdate = [&](const FString& Prefix)
    {
        DestroyStatements();
        return Fail(Database, Prefix, OutError);
    };
    if (!UpsertMetadata.IsValid() || !InsertAsset.IsValid() || !InsertTag.IsValid()
        || !InsertDomain.IsValid() || !InsertDependency.IsValid() || !UpsertGraphNode.IsValid()
        || !InsertGraphEdge.IsValid() || !DeleteAsset.IsValid() || !DeleteTags.IsValid()
        || !DeleteDomains.IsValid() || !DeleteDependencies.IsValid()
        || !DeleteGraphEdges.IsValid() || !DeleteGraphNodes.IsValid())
    {
        return FailUpdate(TEXT("Could not prepare incremental SQLite statements"));
    }

    TArray<FString> SortedPackages = IndexPackages.Array();
    SortedPackages.Sort();
    for (const FString& PackageName : SortedPackages)
    {
        if (!DeleteAsset.BindAndExecute(PackageName)
            || !DeleteTags.BindAndExecute(PackageName)
            || !DeleteDomains.BindAndExecute(PackageName)
            || !DeleteDependencies.BindAndExecute(PackageName))
        {
            return FailUpdate(TEXT("Could not remove stale incremental SQLite rows"));
        }
    }
    TArray<FString> SortedContributors = ContributorPackages.Array();
    SortedContributors.Sort();
    for (const FString& PackageName : SortedContributors)
    {
        if (!DeleteGraphEdges.BindAndExecute(PackageName)
            || !DeleteGraphNodes.BindAndExecute(PackageName))
        {
            return FailUpdate(TEXT("Could not remove stale incremental SQLite graph rows"));
        }
    }

    const TSharedPtr<FJsonObject>* ProjectPtr = nullptr;
    const TSharedPtr<FJsonObject>* EnginePtr = nullptr;
    ProjectIndex->TryGetObjectField(TEXT("project"), ProjectPtr);
    ProjectIndex->TryGetObjectField(TEXT("engine"), EnginePtr);
    if (!UpsertMetadata.BindAndExecute(TEXT("schema"), StringField(ProjectIndex, TEXT("schema")))
        || !UpsertMetadata.BindAndExecute(TEXT("schema_version"), StringField(ProjectIndex, TEXT("schemaVersion")))
        || !UpsertMetadata.BindAndExecute(TEXT("generated_at_utc"), StringField(ProjectIndex, TEXT("generatedAtUtc")))
        || !UpsertMetadata.BindAndExecute(
            TEXT("project_name"),
            ProjectPtr != nullptr ? StringField((*ProjectPtr).ToSharedRef(), TEXT("name")) : FString())
        || !UpsertMetadata.BindAndExecute(
            TEXT("engine_version"),
            EnginePtr != nullptr ? StringField((*EnginePtr).ToSharedRef(), TEXT("version")) : FString()))
    {
        return FailUpdate(TEXT("Could not update incremental SQLite metadata"));
    }

    const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
    if (ProjectIndex->TryGetArrayField(TEXT("assets"), Assets) && Assets != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Assets)
        {
            const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(EntryPtr) || EntryPtr == nullptr) continue;
            const TSharedRef<FJsonObject> Entry = (*EntryPtr).ToSharedRef();
            const FString PackageName = StringField(Entry, TEXT("packageName"));
            if (!IndexPackages.Contains(PackageName)) continue;
            const FString ObjectPath = StringField(Entry, TEXT("objectPath"));
            const FString AssetClass = StringField(Entry, TEXT("assetClass"));
            const FString SemanticKind = StringField(Entry, TEXT("semanticKind"));
            const FString Exporter = StringField(Entry, TEXT("exporter"));
            const FString OwnerModule = StringField(Entry, TEXT("ownerModule"));
            const FString PrimaryAssetId = StringField(Entry, TEXT("primaryAssetId"));
            FString SearchText = FString::Printf(
                TEXT("%s %s %s %s %s %s %s"),
                *PackageName, *ObjectPath, *AssetClass, *SemanticKind,
                *Exporter, *OwnerModule, *PrimaryAssetId);

            const TArray<TSharedPtr<FJsonValue>>* Domains = nullptr;
            if (Entry->TryGetArrayField(TEXT("domains"), Domains) && Domains != nullptr)
            {
                for (const TSharedPtr<FJsonValue>& DomainValue : *Domains)
                {
                    FString Domain;
                    if (DomainValue.IsValid() && DomainValue->TryGetString(Domain))
                    {
                        SearchText += TEXT(" ") + Domain;
                        if (!InsertDomain.BindAndExecute(PackageName, Domain))
                        {
                            return FailUpdate(TEXT("Could not insert incremental SQLite domain"));
                        }
                    }
                }
            }
            const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
            if (Entry->TryGetArrayField(TEXT("assetTags"), Tags) && Tags != nullptr)
            {
                for (const TSharedPtr<FJsonValue>& TagValue : *Tags)
                {
                    FString Tag;
                    if (TagValue.IsValid() && TagValue->TryGetString(Tag))
                    {
                        SearchText += TEXT(" ") + Tag;
                        if (!InsertTag.BindAndExecute(PackageName, Tag))
                        {
                            return FailUpdate(TEXT("Could not insert incremental SQLite tag"));
                        }
                    }
                }
            }
            if (!InsertAsset.BindAndExecute(
                PackageName,
                ObjectPath,
                AssetClass,
                StringField(Entry, TEXT("status")),
                SemanticKind,
                Exporter,
                StringField(Entry, TEXT("sourceFile")),
                StringField(Entry, TEXT("semanticFile")),
                StringField(Entry, TEXT("sourceHash")),
                StringField(Entry, TEXT("semanticHash")),
                OwnerModule,
                PrimaryAssetId,
                StringField(Entry, TEXT("recoverability")),
                NumberField(Entry, TEXT("reconstructionConfidence")),
                IntegerField(Entry, TEXT("dependencyCount")),
                IntegerField(Entry, TEXT("referencerCount")),
                IntegerField(Entry, TEXT("semanticBytes")),
                IntegerField(Entry, TEXT("omissionCount")),
                StringField(Entry, TEXT("exportedAtUtc")),
                SearchText))
            {
                return FailUpdate(TEXT("Could not insert incremental SQLite asset"));
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* DependencyEdges = nullptr;
    if (DependencyGraph->TryGetArrayField(TEXT("edges"), DependencyEdges) && DependencyEdges != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *DependencyEdges)
        {
            const TSharedPtr<FJsonObject>* EdgePtr = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(EdgePtr) || EdgePtr == nullptr) continue;
            const TSharedRef<FJsonObject> Edge = (*EdgePtr).ToSharedRef();
            const FString From = StringField(Edge, TEXT("from"));
            if (IndexPackages.Contains(From)
                && !InsertDependency.BindAndExecute(
                    From, StringField(Edge, TEXT("to")), StringField(Edge, TEXT("type"))))
            {
                return FailUpdate(TEXT("Could not insert incremental SQLite dependency"));
            }
        }
    }

    TSet<FString> GraphNodeIds;
    const TArray<TSharedPtr<FJsonValue>>* GraphEdges = nullptr;
    if (ProjectGraph->TryGetArrayField(TEXT("edges"), GraphEdges) && GraphEdges != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *GraphEdges)
        {
            const TSharedPtr<FJsonObject>* EdgePtr = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(EdgePtr) || EdgePtr == nullptr) continue;
            const TSharedRef<FJsonObject> Edge = (*EdgePtr).ToSharedRef();
            if (!ContributorPackages.Contains(StringField(Edge, TEXT("contributorPackage")))) continue;
            GraphNodeIds.Add(StringField(Edge, TEXT("from")));
            GraphNodeIds.Add(StringField(Edge, TEXT("to")));
            if (!InsertGraphEdge.BindAndExecute(
                StringField(Edge, TEXT("from")),
                StringField(Edge, TEXT("to")),
                StringField(Edge, TEXT("relation")),
                StringField(Edge, TEXT("qualifier")),
                NumberField(Edge, TEXT("confidence")),
                StringField(Edge, TEXT("evidenceSource")),
                StringField(Edge, TEXT("evidencePointer")),
                StringField(Edge, TEXT("contributorPackage")),
                StringField(Edge, TEXT("sourceNodeId")),
                StringField(Edge, TEXT("sourceTitle")),
                StringField(Edge, TEXT("sourcePinId")),
                StringField(Edge, TEXT("targetPinId"))))
            {
                return FailUpdate(TEXT("Could not insert incremental SQLite graph edge"));
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* GraphNodes = nullptr;
    if (ProjectGraph->TryGetArrayField(TEXT("nodes"), GraphNodes) && GraphNodes != nullptr)
    {
        for (const TSharedPtr<FJsonValue>& Value : *GraphNodes)
        {
            const TSharedPtr<FJsonObject>* NodePtr = nullptr;
            if (!Value.IsValid() || !Value->TryGetObject(NodePtr) || NodePtr == nullptr) continue;
            const TSharedRef<FJsonObject> Node = (*NodePtr).ToSharedRef();
            const FString NodeId = StringField(Node, TEXT("id"));
            const FString PackageName = StringField(Node, TEXT("packageName"));
            if (!GraphNodeIds.Contains(NodeId) && !ContributorPackages.Contains(PackageName)) continue;
            const FString Kind = StringField(Node, TEXT("kind"));
            const FString Subtype = StringField(Node, TEXT("subtype"));
            const FString Label = StringField(Node, TEXT("label"));
            const FString SearchText = NodeId + TEXT(" ") + Kind + TEXT(" ") + Subtype
                + TEXT(" ") + Label + TEXT(" ") + PackageName;
            if (!UpsertGraphNode.BindAndExecute(
                    NodeId,
                    Kind,
                    Subtype,
                    Label,
                    PackageName,
                    StringField(Node, TEXT("graphId")),
                    StringField(Node, TEXT("rawNodeId")),
                    SearchText))
            {
                return FailUpdate(TEXT("Could not upsert incremental SQLite graph node"));
            }
        }
    }

    if (!Database.Execute(
            TEXT("DELETE FROM graph_nodes WHERE kind != 'asset' "
                 "AND NOT EXISTS (SELECT 1 FROM graph_edges WHERE source_node = node_id OR target_node = node_id)")))
    {
        return FailUpdate(TEXT("Could not prune incremental SQLite graph nodes"));
    }
    FSQLitePreparedStatement DanglingQuery(
        Database,
        TEXT("SELECT COUNT(*) FROM graph_edges e "
             "LEFT JOIN graph_nodes s ON s.node_id = e.source_node "
             "LEFT JOIN graph_nodes t ON t.node_id = e.target_node "
             "WHERE s.node_id IS NULL OR t.node_id IS NULL"));
    int64 DanglingCount = -1;
    const int64 DanglingRows = DanglingQuery.Execute(
        [&DanglingCount](const FSQLitePreparedStatement& Row)
        {
            return Row.GetColumnValueByIndex(0, DanglingCount)
                ? ESQLitePreparedStatementExecuteRowResult::Continue
                : ESQLitePreparedStatementExecuteRowResult::Error;
        });
    DanglingQuery.Destroy();
    if (DanglingRows != 1 || DanglingCount != 0
        || !Database.Execute(TEXT("COMMIT"))
        || !Database.PerformQuickIntegrityCheck())
    {
        return FailUpdate(TEXT("Could not finalize incremental SQLite update"));
    }
    DestroyStatements();
    if (!Database.Close())
    {
        OutError = FString::Printf(TEXT("Could not close incremental SQLite index: %s"), *Database.GetLastError());
        return false;
    }
    return true;
}
