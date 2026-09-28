# PokéRogue 3DS — Game flow y modos

Autoridad: [`GUI_3DS_NORTH_STAR.md`](../GUI_3DS_NORTH_STAR.md). El flujo es una capacidad objetivo; las reglas específicas son contenido semántico upstream que debe estudiarse y fijarse antes de portarse.

## Estado actual y brecha

`AppShell` registra screens de título, setup, intro de wave, batalla, resultado y resumen. `RunSetupScreen` pasa species/level; `WaveIntroScreen` y `BattleScreen` leen un `WaveDefinition` de facto producido por `WaveManager`. Este manager tiene `totalWaves = 10`, clamp global, especie/nivel/reward por fórmula, mini-boss en 5 y final en 10. UI muestra `/ 10` y nombres/descripciones de Golem. No hay `GameMode`, run save, route graph, biome/wave/encounter catalog ni policy resolver. Es un demo de integración, no implementación de Classic upstream.

## Flow destino

```text
BOOT → TITLE → CONTINUE | NEW GAME
  → RUN SETUP → STARTER SELECTION
  → (MAP → NODE RESOLUTION)?
  → BIOME / WAVE → ENCOUNTER → BATTLE | EVENT
  → REWARD → NEXT NODE / NEXT WAVE / NEXT BIOME
  → WIN | LOSE → RUN SUMMARY
```

La transición es conducida por un `RunFlow` y commands/results; escenas visuales reciben contexto resuelto. Modos pueden omitir/variar map, encuentros, progression, eventos y condición terminal. No forzar todos a Classic.

## Modo y política

Modelo destino `GameModeDefinition`: `id`, config, reglas declarativas/estrategias registradas, condiciones de finalización, política de mapa, biome, progression, encounter, rewards y metadatos/provenance. Engine consulta reglas del modo; nunca asume un máximo global.

Como mínimo, el modelo debe admitir Classic, Endless, Daily, Spliced Endless y Challenge aunque no se implementen juntos. `mode.rules.maxWave` sólo expresa límite de contenido cuando la modalidad realmente lo tiene. Un límite de configuración/contenido no limita capacidad numérica del runtime.

## Contratos de contenido destino

- `WaveDefinition`: número, mode, biome, encounter type/pool, trainer/boss/event/reward, scene binding y special rules.
- `BiomeDefinition`: id, nombres localizados, visual template, background/music, pools, routes/transitions, metadata y provenance.
- `MapDefinition`, `MapNode`, `MapEdge`: grafo de tipos extensibles (battle, elite, boss, event, shop, transition, mode-specific); no lista rígida de waves.
- `EncounterPool`, `EncounterRule`, `EncounterDefinition`: selección contextual por mode/wave/biome/seed/rarity/type/modifiers y resultado `ResolvedEncounter` con species/form/level/trainer/modifiers.

Classic debe provenir de catálogo de definiciones y bindings, no de 200 escenas/JSON individuales ni de código de UI. Endless tiene política propia y no hereda Classic por accidente. La cantidad de biomas y waves procede del contenido upstream, nunca de constantes de interfaz. Revalidar cualquier cifra mutable en el commit fijado durante implementación.

## Ownership y dependencias

`GameFlow` coordina transitions; mode policy decide progresión; content resolvers producen nodos/waves/encounters; BattleEngine recibe encuentro resuelto y devuelve eventos/resultado; reward resolver actualiza run; presentation binding proyecta estado. `SceneModel`, `TimelineUI` y `CanvasRenderer` no deciden biome/encounter/progression. Seeds viajan explícitamente y un PRNG determinista gobierna resultados aleatorios.

## Reutilización y porting

Reutilizar `AppShell` como cascarón de navegación sólo después de separar el state flow; `WaveManager` sirve como prototipo/bridge, no política final. Investigar upstream game modes, phases, biome transitions, encounter generation, reward/save semantics en pinned revision. Añadir evidencia/pruebas de paridad por dominio al portar; no inventar comportamiento para completar el flujo.
