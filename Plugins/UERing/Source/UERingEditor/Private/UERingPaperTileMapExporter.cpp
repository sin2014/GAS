#include "UERingPaperTileMapExporter.h"

#include "Dom/JsonValue.h"
#include "Materials/MaterialInterface.h"
#include "PaperTileLayer.h"
#include "PaperTileMap.h"
#include "PaperTileSet.h"
#include "UObject/UnrealType.h"

namespace UERingPaperTileMapExporter
{
    TSharedRef<FJsonObject> ObjectReference(const UObject* Object)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("path"), Object != nullptr ? Object->GetPathName() : FString());
        Json->SetStringField(TEXT("class"), Object != nullptr ? Object->GetClass()->GetPathName() : FString());
        return Json;
    }

    TSharedRef<FJsonObject> Vector(const FVector& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("x"), Value.X);
        Json->SetNumberField(TEXT("y"), Value.Y);
        Json->SetNumberField(TEXT("z"), Value.Z);
        return Json;
    }

    TSharedRef<FJsonObject> Color(const FLinearColor& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("r"), Value.R);
        Json->SetNumberField(TEXT("g"), Value.G);
        Json->SetNumberField(TEXT("b"), Value.B);
        Json->SetNumberField(TEXT("a"), Value.A);
        return Json;
    }

    bool ReadBoolProperty(const UObject& Object, const TCHAR* Name, bool& OutValue)
    {
        const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object.GetClass(), Name);
        if (Property == nullptr)
        {
            return false;
        }
        OutValue = Property->GetPropertyValue_InContainer(&Object);
        return true;
    }

    bool ReadFloatProperty(const UObject& Object, const TCHAR* Name, double& OutValue)
    {
        const FFloatProperty* Property = FindFProperty<FFloatProperty>(Object.GetClass(), Name);
        if (Property == nullptr)
        {
            return false;
        }
        OutValue = Property->GetPropertyValue_InContainer(&Object);
        return true;
    }

    TSharedRef<FJsonObject> CellSegment(
        const int32 StartX,
        const TArray<int32>& PackedTileIndices,
        const UPaperTileSet* TileSet,
        const TMap<const UPaperTileSet*, int32>& TileSetIndices)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("x"), StartX);
        Json->SetNumberField(TEXT("length"), PackedTileIndices.Num());
        Json->SetNumberField(TEXT("tileSetIndex"), TileSetIndices.FindChecked(TileSet));
        TArray<TSharedPtr<FJsonValue>> Values;
        Values.Reserve(PackedTileIndices.Num());
        for (const int32 PackedTileIndex : PackedTileIndices)
        {
            Values.Add(MakeShared<FJsonValueNumber>(PackedTileIndex));
        }
        Json->SetArrayField(TEXT("packedTileIndices"), Values);
        return Json;
    }
}

FName FUERingPaperTileMapExporter::GetName() const
{
    return TEXT("PaperTileMap");
}

bool FUERingPaperTileMapExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.IsInstanceOf(UPaperTileMap::StaticClass());
}

bool FUERingPaperTileMapExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    using namespace UERingPaperTileMapExporter;
    const UPaperTileMap* TileMap = Cast<UPaperTileMap>(Context.Asset.Get());
    if (TileMap == nullptr)
    {
        OutError = TEXT("The loaded object is not a PaperTileMap.");
        return false;
    }

    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("PaperTileMap"));
    Semantics->SetStringField(TEXT("cellEncoding"), TEXT("sparse-row-segments-v1"));
    const TSharedRef<FJsonObject> PackedTileIndexLayout = MakeShared<FJsonObject>();
    PackedTileIndexLayout->SetNumberField(TEXT("tileIndexMask"), static_cast<uint32>(EPaperTileFlags::TileIndexMask));
    PackedTileIndexLayout->SetNumberField(TEXT("flipDiagonalMask"), static_cast<uint32>(EPaperTileFlags::FlipDiagonal));
    PackedTileIndexLayout->SetNumberField(TEXT("flipVerticalMask"), static_cast<uint32>(EPaperTileFlags::FlipVertical));
    PackedTileIndexLayout->SetNumberField(TEXT("flipHorizontalMask"), static_cast<uint32>(EPaperTileFlags::FlipHorizontal));
    Semantics->SetObjectField(TEXT("packedTileIndexLayout"), PackedTileIndexLayout);
    Semantics->SetNumberField(TEXT("mapWidth"), TileMap->MapWidth);
    Semantics->SetNumberField(TEXT("mapHeight"), TileMap->MapHeight);
    Semantics->SetNumberField(TEXT("tileWidth"), TileMap->TileWidth);
    Semantics->SetNumberField(TEXT("tileHeight"), TileMap->TileHeight);
    Semantics->SetNumberField(TEXT("pixelsPerUnrealUnit"), TileMap->PixelsPerUnrealUnit);
    Semantics->SetNumberField(TEXT("unrealUnitsPerPixel"), TileMap->GetUnrealUnitsPerPixel());
    Semantics->SetNumberField(TEXT("separationPerTileX"), TileMap->SeparationPerTileX);
    Semantics->SetNumberField(TEXT("separationPerTileY"), TileMap->SeparationPerTileY);
    Semantics->SetNumberField(TEXT("separationPerLayer"), TileMap->SeparationPerLayer);
    Semantics->SetNumberField(TEXT("projectionMode"), static_cast<int32>(TileMap->ProjectionMode.GetValue()));
    Semantics->SetNumberField(TEXT("hexSideLength"), TileMap->HexSideLength);
    Semantics->SetNumberField(TEXT("collisionDomain"), static_cast<int32>(TileMap->GetSpriteCollisionDomain()));
    Semantics->SetNumberField(TEXT("collisionThickness"), TileMap->GetCollisionThickness());
    Semantics->SetObjectField(TEXT("material"), ObjectReference(TileMap->Material.Get()));
    Semantics->SetStringField(TEXT("selectedTileSet"), TileMap->SelectedTileSet.ToSoftObjectPath().ToString());

    FVector Corner;
    FVector StepX;
    FVector StepY;
    FVector OffsetYFactor;
    TileMap->GetTileToLocalParameters(Corner, StepX, StepY, OffsetYFactor);
    const TSharedRef<FJsonObject> LocalGeometry = MakeShared<FJsonObject>();
    LocalGeometry->SetObjectField(TEXT("corner"), Vector(Corner));
    LocalGeometry->SetObjectField(TEXT("stepX"), Vector(StepX));
    LocalGeometry->SetObjectField(TEXT("stepY"), Vector(StepY));
    LocalGeometry->SetObjectField(TEXT("offsetYFactor"), Vector(OffsetYFactor));
    Semantics->SetObjectField(TEXT("localGeometry"), LocalGeometry);

    TArray<const UPaperTileSet*> TileSets;
    for (const UPaperTileLayer* Layer : TileMap->TileLayers)
    {
        if (Layer == nullptr)
        {
            continue;
        }
        for (int32 Y = 0; Y < Layer->GetLayerHeight(); ++Y)
        {
            for (int32 X = 0; X < Layer->GetLayerWidth(); ++X)
            {
                const FPaperTileInfo Cell = Layer->GetCell(X, Y);
                if (Cell.IsValid())
                {
                    TileSets.AddUnique(Cell.TileSet);
                }
            }
        }
    }
    TileSets.Sort([](const UPaperTileSet& A, const UPaperTileSet& B)
    {
        return A.GetPathName() < B.GetPathName();
    });
    TMap<const UPaperTileSet*, int32> TileSetIndices;
    TArray<TSharedPtr<FJsonValue>> JsonTileSets;
    for (int32 Index = 0; Index < TileSets.Num(); ++Index)
    {
        TileSetIndices.Add(TileSets[Index], Index);
        const TSharedRef<FJsonObject> JsonTileSet = ObjectReference(TileSets[Index]);
        JsonTileSet->SetNumberField(TEXT("index"), Index);
        JsonTileSets.Add(MakeShared<FJsonValueObject>(JsonTileSet));
    }
    Semantics->SetArrayField(TEXT("tileSets"), JsonTileSets);

    int32 TotalOccupiedCells = 0;
    int32 TotalSegments = 0;
    TArray<TSharedPtr<FJsonValue>> Layers;
    for (int32 LayerIndex = 0; LayerIndex < TileMap->TileLayers.Num(); ++LayerIndex)
    {
        const UPaperTileLayer* Layer = TileMap->TileLayers[LayerIndex];
        if (Layer == nullptr)
        {
            continue;
        }
        const TSharedRef<FJsonObject> JsonLayer = MakeShared<FJsonObject>();
        JsonLayer->SetNumberField(TEXT("index"), LayerIndex);
        JsonLayer->SetStringField(TEXT("name"), Layer->LayerName.ToString());
        JsonLayer->SetNumberField(TEXT("width"), Layer->GetLayerWidth());
        JsonLayer->SetNumberField(TEXT("height"), Layer->GetLayerHeight());
        JsonLayer->SetBoolField(TEXT("renderInGame"), Layer->ShouldRenderInGame());
#if WITH_EDITORONLY_DATA
        JsonLayer->SetBoolField(TEXT("renderInEditor"), Layer->ShouldRenderInEditor());
#endif
        JsonLayer->SetBoolField(TEXT("collides"), Layer->GetLayerCollides());
        JsonLayer->SetObjectField(TEXT("color"), Color(Layer->GetLayerColor()));
        bool bOverride = false;
        double OverrideValue = 0.0;
        if (ReadBoolProperty(*Layer, TEXT("bOverrideCollisionThickness"), bOverride))
        {
            JsonLayer->SetBoolField(TEXT("overridesCollisionThickness"), bOverride);
        }
        if (ReadFloatProperty(*Layer, TEXT("CollisionThicknessOverride"), OverrideValue))
        {
            JsonLayer->SetNumberField(TEXT("collisionThicknessOverride"), OverrideValue);
        }
        if (ReadBoolProperty(*Layer, TEXT("bOverrideCollisionOffset"), bOverride))
        {
            JsonLayer->SetBoolField(TEXT("overridesCollisionOffset"), bOverride);
        }
        if (ReadFloatProperty(*Layer, TEXT("CollisionOffsetOverride"), OverrideValue))
        {
            JsonLayer->SetNumberField(TEXT("collisionOffsetOverride"), OverrideValue);
        }

        TArray<TSharedPtr<FJsonValue>> Rows;
        int32 LayerSegments = 0;
        for (int32 Y = 0; Y < Layer->GetLayerHeight(); ++Y)
        {
            TArray<TSharedPtr<FJsonValue>> Segments;
            int32 X = 0;
            while (X < Layer->GetLayerWidth())
            {
                const FPaperTileInfo Cell = Layer->GetCell(X, Y);
                if (!Cell.IsValid())
                {
                    ++X;
                    continue;
                }
                const int32 StartX = X;
                const UPaperTileSet* SegmentTileSet = Cell.TileSet;
                TArray<int32> PackedTileIndices;
                PackedTileIndices.Add(Cell.PackedTileIndex);
                while (++X < Layer->GetLayerWidth())
                {
                    const FPaperTileInfo NextCell = Layer->GetCell(X, Y);
                    if (!NextCell.IsValid() || NextCell.TileSet != SegmentTileSet)
                    {
                        break;
                    }
                    PackedTileIndices.Add(NextCell.PackedTileIndex);
                }
                Segments.Add(MakeShared<FJsonValueObject>(
                    CellSegment(StartX, PackedTileIndices, SegmentTileSet, TileSetIndices)));
                ++LayerSegments;
            }
            if (!Segments.IsEmpty())
            {
                const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
                Row->SetNumberField(TEXT("y"), Y);
                Row->SetArrayField(TEXT("segments"), Segments);
                Rows.Add(MakeShared<FJsonValueObject>(Row));
            }
        }
        const int32 OccupiedCells = Layer->GetNumOccupiedCells();
        JsonLayer->SetStringField(TEXT("cellEncoding"), TEXT("sparse-row-segments-v1"));
        JsonLayer->SetNumberField(TEXT("occupiedCellCount"), OccupiedCells);
        JsonLayer->SetNumberField(TEXT("segmentCount"), LayerSegments);
        JsonLayer->SetArrayField(TEXT("rows"), Rows);
        TotalOccupiedCells += OccupiedCells;
        TotalSegments += LayerSegments;
        Layers.Add(MakeShared<FJsonValueObject>(JsonLayer));
    }
    Semantics->SetNumberField(TEXT("layerCount"), Layers.Num());
    Semantics->SetNumberField(TEXT("occupiedCellCount"), TotalOccupiedCells);
    Semantics->SetNumberField(TEXT("segmentCount"), TotalSegments);
    Semantics->SetArrayField(TEXT("layers"), Layers);

    OutPayload.Semantics = Semantics;
    return true;
}
