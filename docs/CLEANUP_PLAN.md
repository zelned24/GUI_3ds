# Plan revisable de limpieza — aprobado por el usuario

## 1. Alcance

1. Retirar el Studio web; mantener íntegra la interfaz C++ del juego y sus assets para Azahar/Old 3DS.
2. Trasladar las dependencias headless identificadas a tools/js y actualizar imports, scripts y workflow.
3. Retirar suites monolíticas del Studio; conservar contenido/RNG/battle/native y sustituir el registro de npm test sin cambiar sus asserts.
4. Consolidar documentos históricos en docs/MIGRATION_STATUS.md; conservar la lista completa de pendientes y el contrato de contenido actualizable.
5. Reescribir README.md, AGENTS.md y GUI_3DS_NORTH_STAR.md para reflejar C++/Azahar en lugar del Studio. Conservar pins, separación de capas, procedencia, determinismo y restricción de tests/compilación.
6. No borrar historial Git, project/src, project/include, datos canónicos, assets físicos, clones upstream, node_modules ni trabajo de otra IA. No ejecutar tests/compilación.

## 2. Eliminar interfaz web (archivos exactos)

1. `public/css/game.css`
2. `public/css/main.css`
3. `public/css/preview.css`
4. `public/index.html`
5. `public/js/animation/ClipLibrary.js`
6. `public/js/app.js`
7. `public/js/battle/BattleCommand.js`
8. `public/js/battle/BattleEngine.js`
9. `public/js/battle/BattleEvents.js`
10. `public/js/battle/BattleLabUI.js`
11. `public/js/battle/BattlePhases.js`
12. `public/js/battle/BattleSession.js`
13. `public/js/core/DependencyGraph.js`
14. `public/js/core/HistoryManager.js`
15. `public/js/core/PresetManager.js`
16. `public/js/core/ProjectDocument.js`
17. `public/js/core/ProjectModel.js`
18. `public/js/core/Validator.js`
19. `public/js/data/DataStudioUI.js`
20. `public/js/editor/AssetBrowser.js`
21. `public/js/editor/BuildOutputPanel.js`
22. `public/js/editor/CanvasRenderer.js`
23. `public/js/editor/CommandPalette.js`
24. `public/js/editor/CompositionNavigator.js`
25. `public/js/editor/DragResizeManager.js`
26. `public/js/editor/GlobalSearch.js`
27. `public/js/editor/Hierarchy.js`
28. `public/js/editor/Inspector.js`
29. `public/js/editor/KeyboardShortcuts.js`
30. `public/js/editor/SceneTabs.js`
31. `public/js/editor/SelectionManager.js`
32. `public/js/editor/TimelineUI.js`
33. `public/js/generator/BatchExporter.js`
34. `public/js/generator/CodeGenerator.js`
35. `public/js/generator/ExportReport.js`
36. `public/js/generator/ProjectValidator.js`
37. `public/js/preview/PreviewRuntime.js`
38. `public/js/screens/BaseScreen.js`
39. `public/js/screens/BattleResultScreen.js`
40. `public/js/screens/BattleScreen.js`
41. `public/js/screens/OptionsScreen.js`
42. `public/js/screens/RunSetupScreen.js`
43. `public/js/screens/RunSummaryScreen.js`
44. `public/js/screens/TitleScreen.js`
45. `public/js/screens/WaveIntroScreen.js`
46. `public/js/shell/AppShell.js`
47. `public/js/shell/InputManager.js`
48. `public/js/shell/ScreenManager.js`
49. `public/js/wave/WaveManager.js`

## 3. Trasladar módulos necesarios, conservando su implementación

1. `public/js/animation/AnimationClip.js` → `tools/js/animation/AnimationClip.js`
2. `public/js/animation/AnimationTrack.js` → `tools/js/animation/AnimationTrack.js`
3. `public/js/animation/Interpolation.js` → `tools/js/animation/Interpolation.js`
4. `public/js/animation/Keyframe.js` → `tools/js/animation/Keyframe.js`
5. `public/js/animation/TimelineEvaluator.js` → `tools/js/animation/TimelineEvaluator.js`
6. `public/js/battle/BattleState.js` → `tools/js/battle/BattleState.js`
7. `public/js/components/BaseComponent.js` → `tools/js/components/BaseComponent.js`
8. `public/js/components/ComponentRegistry.js` → `tools/js/components/ComponentRegistry.js`
9. `public/js/components/CompositionNode.js` → `tools/js/components/CompositionNode.js`
10. `public/js/components/GroupNode.js` → `tools/js/components/GroupNode.js`
11. `public/js/components/HealthBar.js` → `tools/js/components/HealthBar.js`
12. `public/js/components/ImageNode.js` → `tools/js/components/ImageNode.js`
13. `public/js/components/MoveButton.js` → `tools/js/components/MoveButton.js`
14. `public/js/components/PixelText.js` → `tools/js/components/PixelText.js`
15. `public/js/components/PokemonSpriteNode.js` → `tools/js/components/PokemonSpriteNode.js`
16. `public/js/components/RogueBox.js` → `tools/js/components/RogueBox.js`
17. `public/js/components/ShapeNode.js` → `tools/js/components/ShapeNode.js`
18. `public/js/components/TextNode.js` → `tools/js/components/TextNode.js`
19. `public/js/components/TouchButton.js` → `tools/js/components/TouchButton.js`
20. `public/js/core/DeterministicRNG.js` → `tools/js/core/DeterministicRNG.js`
21. `public/js/core/EffectModel.js` → `tools/js/core/EffectModel.js`
22. `public/js/core/PropertySystem.js` → `tools/js/core/PropertySystem.js`
23. `public/js/core/SceneLibrary.js` → `tools/js/core/SceneLibrary.js`
24. `public/js/core/SceneModel.js` → `tools/js/core/SceneModel.js`
25. `public/js/core/Transform.js` → `tools/js/core/Transform.js`
26. `public/js/core/UINode.js` → `tools/js/core/UINode.js`
27. `public/js/data/AssetIndex.js` → `tools/js/data/AssetIndex.js`
28. `public/js/data/AssetResolver.js` → `tools/js/data/AssetResolver.js`
29. `public/js/data/AudioResolver.js` → `tools/js/data/AudioResolver.js`
30. `public/js/data/CanonicalDataContract.js` → `tools/js/data/CanonicalDataContract.js`
31. `public/js/data/CanonicalModels.js` → `tools/js/data/CanonicalModels.js`
32. `public/js/data/DataManager.js` → `tools/js/data/DataManager.js`
33. `public/js/data/PokemonSpriteResolver.js` → `tools/js/data/PokemonSpriteResolver.js`
34. `public/js/data/PokerogueAdapter.js` → `tools/js/data/PokerogueAdapter.js`
35. `public/js/data/PokerogueBaseAtlasIndex.js` → `tools/js/data/PokerogueBaseAtlasIndex.js`
36. `public/js/data/PokerogueEnumParser.js` → `tools/js/data/PokerogueEnumParser.js`
37. `public/js/data/PokerogueImporter.js` → `tools/js/data/PokerogueImporter.js`
38. `public/js/data/PokerogueLocaleImporter.js` → `tools/js/data/PokerogueLocaleImporter.js`
39. `public/js/data/PokerogueManifest.js` → `tools/js/data/PokerogueManifest.js`
40. `public/js/data/PokerogueRepository.js` → `tools/js/data/PokerogueRepository.js`
41. `public/js/data/PokerogueSource.js` → `tools/js/data/PokerogueSource.js`
42. `public/js/editor/SpatialUtils.js` → `tools/js/geometry/SpatialUtils.js`
43. `public/js/fixtures/fallbackVerticalSlice.js` → `test/fixtures/fallbackVerticalSlice.js (consolidar duplicado)`
44. `public/js/game/FirstRunFlow.js` → `tools/js/game/FirstRunFlow.js`
45. `public/js/game/GameMode.js` → `tools/js/game/GameMode.js`
46. `public/js/game/ProgressionContent.js` → `tools/js/game/ProgressionContent.js`
47. `public/js/generator/AssetPackager.js` → `tools/js/generator/AssetPackager.js`
48. `public/js/generator/SceneCppExporter.js` → `tools/js/generator/SceneCppExporter.js`
49. `public/js/generator/SceneValidator.js` → `tools/js/generator/SceneValidator.js`

## 4. Retirar documentos históricos tras consolidar estado y límites

1. `docs/BETA_UI_8_ARCHITECTURE_AUDIT.md`
2. `docs/BETA_UI_8_CANONICAL_DATA_CONTRACT.md`
3. `docs/BETA_UI_8_GAMEMODE_CONTRACT.md`
4. `docs/BETA_UI_8_WAVE_BIOME_MAP_CONTRACT.md`
5. `docs/BETA_UI_9A_FIRST_PLAYABLE_VERTICAL_SLICE.md`
6. `docs/generated/BETA_UI_8C_UPSTREAM_TRACEABILITY.md`
7. `docs/generated/POKEROGUE_BATTLE_PARITY.md`
8. `docs/generated/POKEROGUE_CLASSIC_TRACEABILITY.md`
9. `docs/POKEROGUE_3DS_CONTENT_UPDATE_SPEC.md`
10. `docs/POKEROGUE_3DS_GAME_FLOW_SPEC.md`
11. `docs/POKEROGUE_3DS_MASTER_SPEC.md`
12. `docs/POKEROGUE_3DS_PRESENTATION_SPEC.md`
13. `docs/progress/BETA_UI_9A.md`
14. `docs/progress/BETA_UI_9B_ENCOUNTER_POOL.md`
15. `docs/progress/BETA_UI_9C_POKEROGUE_RNG_ADAPTER.md`
16. `docs/progress/BETA_UI_9D_NATIVE_WILD_ACTOR.md`
17. `docs/progress/BETA_UI_9E_MOVE_DECLARATIVE_METADATA.md`
18. `docs/progress/BETA_UI_9F_SINGLE_BATTLE_COMMANDS.md`
19. `docs/progress/NATIVE_CLASSIC_TRAINER_CHANCE_DATA.md`
20. `docs/progress/NATIVE_CLASSIC_VICTORY_PLAN.md`
21. `docs/progress/NATIVE_CLASSIC_WAVE_SCHEDULE.md`
22. `docs/progress/NATIVE_EXPERIENCE_AND_LEVELS.md`
23. `docs/progress/NATIVE_MOVESET_SELECTION.md`
24. `docs/progress/NATIVE_STORAGE_AND_CONTENT_UPDATES.md`
25. `docs/progress/NATIVE_TRAINER_CATALOG.md`
26. `docs/progress/NATIVE_UPSTREAM_SPRITE_ATLAS.md`
27. `docs/SEMANTIC_BRAIN_MAP.md`

## 5. Otros retiros y cambios

1. Eliminar server.js y .env.example (servidor/editor).
2. Eliminar astra.md, Continue.md y Plan_de_trabajo.md (planes históricos/contradictorios).
3. Eliminar .ai/architect.md, .ai/implementer.md y .ai/reviewer.md; las reglas vigentes quedan consolidadas en AGENTS.md.
4. Eliminar test/beta_ui_8_tests.js y sustituir test/run_tests.js (suite histórica del Studio) por registro de suites actuales. Sus fallos históricos no se declaran corregidos.
5. Conservar/renombrar beta_ui_9a_tests.js → migration_content_tests.js, beta_ui_9c_rng_tests.js → rng_tests.js y beta_ui_9d_tests.js → battle_tests.js. Conservar test/native/* y fixtures necesarios.
6. Eliminar docs/semantic-brain-map.json y scripts/semantic_brain_map_query.mjs, que quedan sin consumidor al retirar las suites antiguas.
7. Actualizar package.json/package-lock.json/metadata.json: nombre y descripción nativos; quitar start/dev y build que ejecutaba tests. Mantener comandos de importación, generación, conversión y validación final.
8. Actualizar .github/workflows/build-3ds.yml y scripts para las rutas tools/js; no lanzar workflow/build manualmente.

## 6. Riesgos y prueba de preservación

1. La lista de módulos trasladados procede del cierre de dependencias relativas de scripts y suites nativas actuales. Antes del commit comprobar estáticamente imports y rutas restantes.
2. No reutilizar tests del Studio como evidencia de combate. No reducir ni modificar asserts de las suites conservadas.
3. Revisar git diff --check y lista de archivos, sin ejecutar tests/compilación por restricción del usuario.
4. Todos los archivos retirados siguen recuperables en Git. La revisión automática requirió aprobación específica; el usuario la concedió y se aplicó este plan.
5. Aprobar este plan autoriza exactamente la eliminación, reubicación y consolidación descritas; la funcionalidad nativa de combate/presentación permanece.
