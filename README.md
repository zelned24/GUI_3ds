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

Tests y compilación están autorizados; consultar los resultados actuales en docs/MIGRATION_STATUS.md. No hay npm start ni servidor/editor web.

Mapa de estructura: `npm run brain-map` regenera [el brain map](docs/BRAIN_MAP.md).

## Documentación mínima

1. [Reglas](AGENTS.md).
2. [Estado y límites](docs/MIGRATION_STATUS.md).
3. [Pendientes y coordinación](docs/progress/POKEROGUE_3DS_REMAINING_WORK.md).
4. [Catálogo actualizable](docs/progress/NATIVE_RUNTIME_CONTENT_PACK_CONTRACT.md).

El historial conserva documentos y suites retirados. Retirarlos por cambio de alcance no demuestra que sus antiguos fallos fueran corregidos.

Checklist de avance por áreas: [implementación pendiente](docs/progress/IMPLEMENTATION_CHECKLIST.md).

## Assets de presentación nativa

1. Con los repositorios pinned disponibles en `build/upstream/`, instalar `python -m pip install -r scripts/presentation-requirements.txt`.
2. Ejecutar `python scripts/prepare_native_presentation.py` (requiere tex3ds y mkbcfnt de devkitPro; rutas Windows actuales en el script).
3. Ejecutar `python scripts/verify_presentation_media.py` para comprobar archivos físicos. Los manifests de procedencia se generan bajo `build/native-presentation/`.
4. Ejecutar el build después de preparar sprites, presentación y bundle QuickJS. El build no descarga esos catálogos automáticamente.

Los binarios y assets convertidos bajo `build/` no se suben a Git; se generan desde las revisiones fijadas.
