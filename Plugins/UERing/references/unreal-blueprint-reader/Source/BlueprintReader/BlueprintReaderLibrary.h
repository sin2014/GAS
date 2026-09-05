#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintReaderLibrary.generated.h"

UCLASS()
class UBlueprintReaderLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * List all graphs in a Blueprint asset.
     * Returns JSON: {"asset_path", "class", "parent_class", "graphs": [{"name", "type", "node_count"}]}
     *
     * @param AssetPath  Full asset path, e.g. "/Game/Characters/BP_Hero"
     * @return JSON string with graph listing, or error JSON if asset not found
     */
    UFUNCTION(BlueprintCallable, Category = "Blueprint Reader")
    static FString GetBlueprintGraphList(const FString& AssetPath);

    /**
     * Get full graph data: all nodes with pins, types, connections, defaults.
     * Returns JSON with nodes array containing pin details.
     *
     * @param AssetPath  Full asset path, e.g. "/Game/Characters/BP_Hero"
     * @param GraphName  Name of the graph (e.g. "EventGraph", "SetupHUD"). Empty = first UbergraphPage.
     * @return JSON string with full graph data
     */
    UFUNCTION(BlueprintCallable, Category = "Blueprint Reader")
    static FString GetGraphData(const FString& AssetPath, const FString& GraphName);

    /**
     * Get all variables defined in a Blueprint.
     * Returns JSON with variable name, type, default value, and flags.
     *
     * @param AssetPath  Full asset path
     * @return JSON string with variables array
     */
    UFUNCTION(BlueprintCallable, Category = "Blueprint Reader")
    static FString GetBlueprintVariables(const FString& AssetPath);

    /**
     * Get linearized execution flow from an entry point.
     * Follows exec pins recursively, producing a tree structure.
     *
     * @param AssetPath   Full asset path
     * @param EntryPoint  Name of the entry event/function (e.g. "ReceiveBeginPlay", "SetupHUD")
     * @return JSON string with execution flow tree
     */
    UFUNCTION(BlueprintCallable, Category = "Blueprint Reader")
    static FString GetExecutionFlow(const FString& AssetPath, const FString& EntryPoint);

    /**
     * Search for nodes in a Blueprint by title or function name.
     *
     * @param AssetPath  Full asset path
     * @param Query      Search string (matched against node title and function name, case-insensitive)
     * @return JSON string with matching nodes
     */
    UFUNCTION(BlueprintCallable, Category = "Blueprint Reader")
    static FString SearchNodes(const FString& AssetPath, const FString& Query);
};
