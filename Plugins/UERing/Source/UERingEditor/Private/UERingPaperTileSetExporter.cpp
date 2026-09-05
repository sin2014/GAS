#include "UERingPaperTileSetExporter.h"

#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "PaperTileSet.h"

namespace UERingPaperTileSetExporter
{
    TSharedRef<FJsonObject> IntPoint(const FIntPoint& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("x"), Value.X);
        Json->SetNumberField(TEXT("y"), Value.Y);
        return Json;
    }

    TSharedRef<FJsonObject> Vector2D(const FVector2D& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("x"), Value.X);
        Json->SetNumberField(TEXT("y"), Value.Y);
        return Json;
    }

    TSharedRef<FJsonObject> Margin(const FIntMargin& Value)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("left"), Value.Left);
        Json->SetNumberField(TEXT("top"), Value.Top);
        Json->SetNumberField(TEXT("right"), Value.Right);
        Json->SetNumberField(TEXT("bottom"), Value.Bottom);
        return Json;
    }

    TSharedRef<FJsonObject> ObjectReference(const UObject* Object)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("objectPath"), Object != nullptr ? Object->GetPathName() : FString());
        Json->SetStringField(
            TEXT("packageName"),
            Object != nullptr && Object->GetPackage() != nullptr ? Object->GetPackage()->GetName() : FString());
        Json->SetStringField(
            TEXT("class"),
            Object != nullptr && Object->GetClass() != nullptr ? Object->GetClass()->GetPathName() : FString());
        return Json;
    }

    TSharedRef<FJsonObject> SerializeCollision(const FSpriteGeometryCollection& Collision)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("geometryType"), UEnum::GetValueAsString(Collision.GeometryType.GetValue()));
        Json->SetNumberField(TEXT("pixelsPerSubdivisionX"), Collision.PixelsPerSubdivisionX);
        Json->SetNumberField(TEXT("pixelsPerSubdivisionY"), Collision.PixelsPerSubdivisionY);
        Json->SetBoolField(TEXT("avoidVertexMerging"), Collision.bAvoidVertexMerging);
        Json->SetNumberField(TEXT("alphaThreshold"), Collision.AlphaThreshold);
        Json->SetNumberField(TEXT("detailAmount"), Collision.DetailAmount);
        Json->SetNumberField(TEXT("simplifyEpsilon"), Collision.SimplifyEpsilon);

        TArray<TSharedPtr<FJsonValue>> Shapes;
        Shapes.Reserve(Collision.Shapes.Num());
        for (const FSpriteGeometryShape& Shape : Collision.Shapes)
        {
            const TSharedRef<FJsonObject> JsonShape = MakeShared<FJsonObject>();
            JsonShape->SetStringField(TEXT("shapeType"), UEnum::GetValueAsString(Shape.ShapeType));
            JsonShape->SetObjectField(TEXT("boxSize"), Vector2D(Shape.BoxSize));
            JsonShape->SetObjectField(TEXT("boxPosition"), Vector2D(Shape.BoxPosition));
            JsonShape->SetNumberField(TEXT("rotation"), Shape.Rotation);
            JsonShape->SetBoolField(TEXT("negativeWinding"), Shape.bNegativeWinding);
            TArray<TSharedPtr<FJsonValue>> Vertices;
            Vertices.Reserve(Shape.Vertices.Num());
            for (const FVector2D& Vertex : Shape.Vertices)
            {
                Vertices.Add(MakeShared<FJsonValueObject>(Vector2D(Vertex)));
            }
            JsonShape->SetArrayField(TEXT("vertices"), Vertices);
            Shapes.Add(MakeShared<FJsonValueObject>(JsonShape));
        }
        Json->SetArrayField(TEXT("shapes"), Shapes);
        return Json;
    }

    bool HasTerrainMembership(const FPaperTileMetadata& Metadata)
    {
        return Metadata.TerrainMembership[0] != 0xff
            || Metadata.TerrainMembership[1] != 0xff
            || Metadata.TerrainMembership[2] != 0xff
            || Metadata.TerrainMembership[3] != 0xff;
    }

    bool IsMeaningfulOverride(const FPaperTileMetadata& Metadata)
    {
        return Metadata.HasMetaData() || Metadata.HasCollision() || HasTerrainMembership(Metadata);
    }

    TSharedRef<FJsonObject> SerializeMetadata(const FPaperTileMetadata& Metadata)
    {
        const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("userDataName"), Metadata.UserDataName.ToString());
        Json->SetBoolField(TEXT("hasCollision"), Metadata.HasCollision());
        Json->SetObjectField(TEXT("collision"), SerializeCollision(Metadata.CollisionData));
        TArray<TSharedPtr<FJsonValue>> TerrainMembership;
        for (const uint8 Membership : Metadata.TerrainMembership)
        {
            TerrainMembership.Add(MakeShared<FJsonValueNumber>(Membership));
        }
        Json->SetArrayField(TEXT("terrainMembership"), TerrainMembership);
        return Json;
    }
}

FName FUERingPaperTileSetExporter::GetName() const
{
    return TEXT("PaperTileSet");
}

bool FUERingPaperTileSetExporter::CanExport(const FAssetData& AssetData) const
{
    return AssetData.IsInstanceOf(UPaperTileSet::StaticClass());
}

bool FUERingPaperTileSetExporter::BuildPayload(
    const FUERingExportContext& Context,
    FUERingSemanticPayload& OutPayload,
    FString& OutError) const
{
    using namespace UERingPaperTileSetExporter;

    const UPaperTileSet* TileSet = Cast<UPaperTileSet>(Context.Asset.Get());
    if (TileSet == nullptr)
    {
        OutError = TEXT("The loaded object is not a PaperTileSet.");
        return false;
    }

    const TSharedRef<FJsonObject> Semantics = MakeShared<FJsonObject>();
    Semantics->SetStringField(TEXT("kind"), TEXT("PaperTileSet"));
    Semantics->SetStringField(TEXT("metadataEncoding"), TEXT("sparse-default-v1"));
    Semantics->SetObjectField(TEXT("tileSize"), IntPoint(TileSet->GetTileSize()));
    Semantics->SetObjectField(TEXT("borderMargin"), Margin(TileSet->GetMargin()));
    Semantics->SetObjectField(TEXT("perTileSpacing"), IntPoint(TileSet->GetPerTileSpacing()));
    Semantics->SetObjectField(TEXT("drawingOffset"), IntPoint(TileSet->GetDrawingOffset()));
    Semantics->SetNumberField(TEXT("tileCount"), TileSet->GetTileCount());
    Semantics->SetNumberField(TEXT("tileCountX"), TileSet->GetTileCountX());
    Semantics->SetNumberField(TEXT("tileCountY"), TileSet->GetTileCountY());
    Semantics->SetObjectField(TEXT("tileSheet"), ObjectReference(TileSet->GetTileSheetTexture()));
    Semantics->SetObjectField(
        TEXT("tileSheetAuthoredSize"),
        TileSet->GetTileSheetTexture() != nullptr
            ? IntPoint(TileSet->GetTileSheetAuthoredSize())
            : IntPoint(FIntPoint::ZeroValue));

    TArray<TSharedPtr<FJsonValue>> AdditionalTextures;
    for (const UTexture* Texture : TileSet->GetAdditionalTextures())
    {
        AdditionalTextures.Add(MakeShared<FJsonValueObject>(ObjectReference(Texture)));
    }
    Semantics->SetArrayField(TEXT("additionalTextures"), AdditionalTextures);

    TArray<TSharedPtr<FJsonValue>> Terrains;
    for (int32 Index = 0; Index < TileSet->GetNumTerrains(); ++Index)
    {
        const FPaperTileSetTerrain Terrain = TileSet->GetTerrain(Index);
        const TSharedRef<FJsonObject> JsonTerrain = MakeShared<FJsonObject>();
        JsonTerrain->SetNumberField(TEXT("index"), Index);
        JsonTerrain->SetStringField(TEXT("name"), Terrain.TerrainName);
        JsonTerrain->SetNumberField(TEXT("centerTileIndex"), Terrain.CenterTileIndex);
        Terrains.Add(MakeShared<FJsonValueObject>(JsonTerrain));
    }
    Semantics->SetArrayField(TEXT("terrains"), Terrains);

    const FPaperTileMetadata DefaultMetadata;
    Semantics->SetObjectField(TEXT("defaultMetadata"), SerializeMetadata(DefaultMetadata));
    TArray<TSharedPtr<FJsonValue>> Overrides;
    for (int32 TileIndex = 0; TileIndex < TileSet->GetTileCount(); ++TileIndex)
    {
        const FPaperTileMetadata* Metadata = TileSet->GetTileMetadata(TileIndex);
        if (Metadata == nullptr || !IsMeaningfulOverride(*Metadata))
        {
            continue;
        }
        const TSharedRef<FJsonObject> Override = MakeShared<FJsonObject>();
        Override->SetNumberField(TEXT("tileIndex"), TileIndex);
        Override->SetObjectField(TEXT("metadata"), SerializeMetadata(*Metadata));
        Overrides.Add(MakeShared<FJsonValueObject>(Override));
    }
    Semantics->SetNumberField(TEXT("overrideCount"), Overrides.Num());
    Semantics->SetArrayField(TEXT("overrides"), Overrides);
#if WITH_EDITORONLY_DATA
    const FLinearColor BackgroundColor = TileSet->GetBackgroundColor();
    const TSharedRef<FJsonObject> JsonBackgroundColor = MakeShared<FJsonObject>();
    JsonBackgroundColor->SetNumberField(TEXT("r"), BackgroundColor.R);
    JsonBackgroundColor->SetNumberField(TEXT("g"), BackgroundColor.G);
    JsonBackgroundColor->SetNumberField(TEXT("b"), BackgroundColor.B);
    JsonBackgroundColor->SetNumberField(TEXT("a"), BackgroundColor.A);
    Semantics->SetObjectField(TEXT("editorBackgroundColor"), JsonBackgroundColor);
#endif
    OutPayload.Semantics = Semantics;
    return true;
}
