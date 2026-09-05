# unreal-blueprint-reader

C++ Unreal Engine editor plugin that exposes Blueprint graph structure as JSON for AI tooling via Python scripting.

Gives AI assistants structural access to Blueprint graphs — nodes, pins, connections, execution flow, and variables — through `UFUNCTION`s callable from the editor's Python environment.

## Why?

Blueprint graphs are opaque binary assets. AI assistants can't read `.uasset` files, so they're blind to any logic implemented in Blueprints. This plugin serializes the graph data to JSON, making it accessible through the UE Python scripting API.

**Companion MCP server:** [unreal-blueprint-mcp](https://github.com/tumourlove/unreal-blueprint-mcp) wraps these functions as MCP tools for AI assistants.

**Complements** (does not replace):
- [unreal-source-mcp](https://github.com/tumourlove/unreal-source-mcp) — Engine-level source intelligence (full UE C++ and HLSL)
- [unreal-project-mcp](https://github.com/tumourlove/unreal-project-mcp) — Project-level source intelligence (your C++ code)
- [unreal-editor-mcp](https://github.com/tumourlove/unreal-editor-mcp) — Build diagnostics and editor log tools (Live Coding, error parsing, log search)
- [unreal-blueprint-mcp](https://github.com/tumourlove/unreal-blueprint-mcp) — Blueprint graph reading (nodes, pins, connections, execution flow)
- [unreal-material-mcp](https://github.com/tumourlove/unreal-material-mcp) — Material graph intelligence, editing, and procedural creation (46 tools: expressions, parameters, instances, graph building, templates, C++ plugin)
- [unreal-config-mcp](https://github.com/tumourlove/unreal-config-mcp) — Config/INI intelligence (resolve inheritance chains, search settings, diff from defaults, explain CVars)
- [unreal-animation-mcp](https://github.com/tumourlove/unreal-animation-mcp) — Animation data inspector and editor (sequences, montages, blend spaces, ABPs, skeletons, 62 tools)
- [unreal-niagara-mcp](https://github.com/tumourlove/unreal-niagara-mcp) — Niagara VFX intelligence and editing (emitters, modules, HLSL generation, procedural creation, 70 tools)
- [unreal-api-mcp](https://github.com/nicobailon/unreal-api-mcp) by [Nico Bailon](https://github.com/nicobailon) — API surface lookup (signatures, #include paths, deprecation warnings)

Together these servers give AI agents full-stack UE understanding: engine internals, API surface, your project code, build/runtime feedback, Blueprint graph data, config/INI intelligence, material graph inspection + editing, animation data inspection + editing, and Niagara VFX inspection + creation.

## Installation

Copy or symlink this plugin to your project's `Plugins/` folder:

```
MyProject/
  Plugins/
    BlueprintReader/
      BlueprintReader.uplugin
      Source/
        BlueprintReader/
          ...
```

Recompile the project or restart the editor.

## Prerequisites

- **Unreal Engine 5.x** (tested on 5.7)
- **Python Remote Execution** enabled in Project Settings (for AI tooling access)

## Python-Callable Functions

All functions are static methods on `UBlueprintReaderLibrary`:

| Function | Description |
|----------|-------------|
| `get_blueprint_graph_list(asset_path)` | List all graphs (event graphs, functions, macros) in a Blueprint |
| `get_graph_data(asset_path, graph_name)` | Full node/pin/connection serialization for a graph |
| `get_blueprint_variables(asset_path)` | All variables with types, defaults, and property flags |
| `get_execution_flow(asset_path, entry_point)` | Linearized execution trace from an entry event/function |
| `search_nodes(asset_path, query)` | Search nodes by title, class, or function name |

### Example (editor Python console)

```python
import unreal
result = unreal.BlueprintReaderLibrary.get_blueprint_graph_list('/Game/Characters/BP_Hero')
print(result)
```

## License

MIT
