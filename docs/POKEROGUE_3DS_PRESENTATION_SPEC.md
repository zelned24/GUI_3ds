# PokéRogue 3DS — Presentación, templates y bindings

Autoridad: [`GUI_3DS_NORTH_STAR.md`](../GUI_3DS_NORTH_STAR.md). El Composer describe cómo se presenta un estado resuelto; no contiene especies/reglas de gameplay.

## Estado actual y brecha

El repositorio ya tiene modelo de autoría `ProjectModel`/`SceneModel`, componentes, canvas dual, timeline/keyframes/evaluator, preview, validadores, exportador C++, `ScenePlayer` y `RuntimeAssetManager`. Scene JSON y C++ generated schemas son reutilizables. Las screens actuales (`BattleScreen`, `WaveIntroScreen`, etc.) son DOM screens independientes del Composer: consultan `WaveManager`/`BattleState`, construyen HTML y muestran campos de un slice. `PokemonSpriteNode` declara especie/form/shiny/facing y `PokemonSpriteResolver` resuelve sprite; no existe binding general de valores de run a slots ni template de gameplay nativo. Composer scenes son layout authoring, no runtime template dinámico.

## Template con slots

Una escena de batalla puede declarar slots tipados: `backgroundSlot`, `playerPokemonSlot`, `enemyPokemonSlot`, `trainerSlot`, `partyHudSlot`, `enemyHudSlot`, `heldItemsSlot`, `passiveItemsSlot`, `biomeLabelSlot`, `waveCounterSlot`, `messageSlot`, `commandMenuSlot`. Un slot tiene binding y presentación/fallback opcional, nunca contiene una especie fija.

```json
{
  "scene": "BattleForest",
  "slots": {
    "playerPokemon": { "binding": "activePlayerPokemon" },
    "enemyPokemon": { "binding": "resolvedEncounter" },
    "background": { "binding": "currentBiome.background" },
    "wave": { "binding": "run.wave" }
  }
}
```

Es ejemplo conceptual, no formato implementado. Evolucionar formatos `project.json` y `screens/*.json` con migración compatible y schema explícito.

## Resolución

Bindings tipados conectan `Biome → SceneTemplate`, `EncounterType → SceneTemplate`, `EventType → SceneTemplate`, `BossType → SceneTemplate`, `Transformation → VisualResolver`, `Species/Form → SpriteResolver`, `Item → ItemIconResolver`. Resolución visual encadena especie → forma → asset físico indexado → animación → escala → efectos. El asset manifest verifica archivo/provenance antes de mostrarlo; no sintetizar rutas como prueba de existencia.

Un slot proyecta una vista inmutable de datos (nombre/nivel/HP/status/money/items/biome/wave/menu). El input de UI emite Command; engine actualiza modelo y eventos; binding recibe eventos y refresca presentación. Ningún binding cambia `BattleState` directamente.

## Pokémon, items y transformaciones

Mantener separado contenido `Species/Form/ItemDefinition`, instancias y estado de runtime, y nodo/slot visual. El estado runtime futuro debe cubrir species/form/level/HP/status/moves/abilities/items/stats/modifiers/transformation/visual state. Resolver `TransformationState` (none, mega, primal, dynamax, gigantamax, tera y futuros) desde datos compartidos; no crear escena por species o transformación. `ItemDefinition`/`ItemInstance`/`ItemResolver` suministran nombres/iconos/visibilidad para held/passive/consumibles/rewards.

## Requisitos 3DS

Mantener salida para top 400×240 (sin touch) y bottom 320×240 (touch resistiva), navegación por botones, contraste/tamaño legible, coordenadas enteras, memoria/VRAM, límites de textura y formats soportados. Scene Composer autoriza title/menu/starter/map/biome/battle/event/reward/dialog/boss intro/win/lose/summary; datos son dinámicos y reutilizan templates. Runtime mantiene separados `ScenePlayer`, timeline, assets, game flow, battle y resolver de contenido.

## Sistemas reutilizables y gaps

Reutilizar timeline, `SceneModel`, validadores, exportador, `ScenePlayer`, manifests/resolvers de assets y cache. Mantener la UI actual como referencia/bridge durante migración, no duplicar engine runtime. Brechas: schema de slots, binding registry, view model/estado proyectado, resolved scene context, validación de binding/assets, resolución native equivalente y pruebas de paridad editor-preview-export-runtime. Evitar switches dispersos en UI.
