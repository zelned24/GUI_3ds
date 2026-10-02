# PokéRogue para Old Nintendo 3DS XL

Migración de PokéRogue a C++ con devkitARM, libctru y Citro2D. Interfaz adaptada a ambas pantallas; validación en Azahar y en consola. **Classic completo todavía está pendiente y no está verificado.**

## Estructura

- project/: runtime C++, datos canónicos, assets y presentación del juego.
- tools/js/: importadores/modelos/packagers/generadores sin editor web; modelos de escena usados por build/parity.
- scripts/: adquisición pinned, importación, generación y conversión.
- test/: suites relevantes de contenido, RNG, combate, assets y runtime; fixtures aisladas.
- docs/: estado consolidado, pendientes y contrato de catálogo actualizable.

## Comandos

1. npm ci: herramientas Node del pipeline, no runtime del juego.
2. npm run import:pokerogue: importación de fuentes pinned.
3. npm run generate:3ds-content: tablas C++ deterministas.
4. npm run convert:pokerogue-sprites: conversión de sprites reales.
5. Etapa final: npm test, npm run native-parity, npm run native-test, npm run 3ds-test y npm run 3ds-build.

Tests y compilación del programa están aplazados por el usuario. No hay npm start ni servidor/editor web.

Mapa de estructura: `npm run brain-map` regenera [el brain map](docs/BRAIN_MAP.md).

## Documentación mínima

1. [Reglas](AGENTS.md).
2. [Estado y límites](docs/MIGRATION_STATUS.md).
3. [Pendientes y coordinación](docs/progress/POKEROGUE_3DS_REMAINING_WORK.md).
4. [Catálogo actualizable](docs/progress/NATIVE_RUNTIME_CONTENT_PACK_CONTRACT.md).

El historial conserva documentos y suites retirados. Retirarlos por cambio de alcance no demuestra que sus antiguos fallos fueran corregidos.

Checklist de avance por áreas: [implementación pendiente](docs/progress/IMPLEMENTATION_CHECKLIST.md).
