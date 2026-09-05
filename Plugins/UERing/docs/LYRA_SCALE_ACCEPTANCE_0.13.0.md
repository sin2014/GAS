# Lyra Scale Acceptance 0.13.0

Date: 2026-08-01  
Engine: Unreal Engine 5.8.0  
Project: `D:\GameDev\Unreal_Projects\LyraStarterGame`  
Plugin semantic revision: 24

## Scope

This iteration adds compact role-specific v2 projections for GAS, Lyra Experience,
PawnData, and GameFeature assets. It also connects native `/Script/...` class references
to class-symbol nodes and removes duplicate logical project-graph edges that differed only
by evidence pointer.

## Export result

- Exported assets: 3,876
- Unchanged assets: 0
- Failed assets: 0
- Assets at semantic revision 24: 3,876
- Unified graph nodes: 47,178
- Unified graph edges: 118,084
- Asset semantic JSON bytes: 123,471,620
- Complete `.uesem` bytes: 431,973,639
- Complete `.uesem` files: 13,432
- Project graph JSON bytes: 77,807,121
- SQLite bytes: 223,911,936

Compared with 0.12.0, the complete export is 15,737,447 bytes smaller (3.5%).
Logical graph-edge deduplication removed 13,107 edges (10.0%) while retaining one
authoritative evidence source for each relation. The small increase in asset semantic JSON
is intentional: it contains previously missing GameplayEffect behavior and 37 instanced
Experience/ActionSet GameFeatureAction configurations.

## Domain coverage

| Domain | Assets | Roles |
| --- | ---: | --- |
| GAS | 122 | ability 43, abilitySet 12, effect 42, gameplayCue 24, tagRelationshipMapping 1 |
| Lyra Experience | 29 | experience 10, actionSet 5, playlist 8, pawnData 6 |
| GameFeature | 5 | featureData 5 |
| Enhanced Input | 35 | action 26, mappingContext 4, inputConfig 5 |
| StateTree | 1 | tree 1 |
| DataRegistry | 1 | registry 1 |

GAS properties are selected by exact role allowlists. AttributeSet values are selected by
`FGameplayAttributeData` type. Current Lyra facts include 16 `Executions`, 11
`DurationPolicy`, 9 `GameplayCues`, 7 `Modifiers`, 5 `Period`, 2 `DurationMagnitude`, and
31 `GEComponents` fields. Domain projections contain no legacy `assetProperties` or
`classDefaultOverrides` arrays.

## Golden relationships

The project-scale acceptance suite verifies these representative flows:

- Default Experience -> Simple PawnData.
- HeroData ShooterGame -> AbilitySet, InputConfig, PawnClass, TagRelationship, CameraMode.
- AbilitySet ShooterHero -> Dash ability with `InputTag.Ability.Dash`.
- AbilitySet ShooterHero -> native `LyraGameplayAbility_Reset` class symbol.
- AbilitySet Arena -> native `TopDownArenaAttributeSet` class symbol.
- ShooterCore GameFeature actions -> input mapping, DataRegistry, and native component symbols.
- GameplayEffect duration, executions, cues, and instanced component configuration.

## Verification

- `BuildPlugin` for Win64: passed.
- Python unit tests: 48 passed.
- Strict asset index validation: passed.
- Strict unified project graph validation: passed.
- Lyra golden acceptance: passed.
- UE Automation `UERing.`: 11 passed.
- SQLite quick check, JSON/SQLite count parity, and dangling endpoint checks: passed.

Logs:

- `Saved/Logs/UERing-0.13.0-Revision24-FullExport.log`
- `Saved/Logs/UERing-0.13.0-Revision24-AllTests.log`

Only LyraStarterGame was deployed and exported. The Start project was not modified.
