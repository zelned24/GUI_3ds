# PokéRogue 3DS — Especificación maestra

La autoridad arquitectónica compartida es [`GUI_3DS_NORTH_STAR.md`](../GUI_3DS_NORTH_STAR.md). Este documento define la dirección, resume la auditoría `CURRENT / TARGET / GAP` y enlaza contratos especializados. No es autorización para implementar mecánicas sin investigación.

## Propósito y frontera

GUI_3DS adapta PokéRogue a Nintendo 3DS. El Composer es una herramienta de autoría, preview, validación y exportación de presentación; PokéRogue upstream es la referencia canónica para semántica de juego, contenido y save. La UI no se convierte en fuente de reglas. No crear un segundo producto/repositorio ni un motor PokéRogue alternativo.

## Arquitectura objetivo

```text
UPSTREAM ──> PINNED IMPORT ──> NORMALIZER ──> CANONICAL CONTENT
                                                + 3DS OVERRIDES
                                                       │
                                  ┌────────────────────┴─────────────────┐
                                  v                                      v
                         GAME FLOW / DATA                       CONTENT PACKAGE
                                  │                                      │
                         RESOLVED GAME STATE                 VERIFY / INSTALL
                                  │
                    PRESENTATION BINDINGS / TEMPLATES
                                  │
                COMPOSER ──> VALIDATE / EXPORT
                                  │
                     C++ 3DS RUNTIME / ROMFS
```

Game data, presentation, runtime y content update tienen modelos y ownership propios. Una run produce estado resuelto; bindings tipados proyectan ese estado sobre slots. Runtime de combate no importa DOM/editor. Interacciones generan Commands; engine procesa y emite Events; bindings actualizan la UI.

## Hallazgos de auditoría

| Sistema | CURRENT | TARGET | GAP |
|---|---|---|---|
| Flow/modes | `AppShell` y screens; no mode policy integrado | Flow común gobernado por modo | Falta máquina de estados y policies |
| Classic/Endless | slice de 10 waves y encuentros fijos | definiciones de contenido y políticas propias | No existe contenido o resolver de modos |
| Map/biome/wave/encounter | no hay modelos integrados; `WaveManager` fija datos | definiciones canónicas y selección seeded | Dominio no modelado |
| Pokémon/items/transforms | estado parcial; item model mínimo; sin transformación runtime | estado separado/extensible y resolvers | falta runtime/domain integration |
| Battle | bridge modular por fases/events/commands | portar semántica upstream con paridad definida | prototipo no canónico, reglas parciales |
| Scene/bindings | modelo de escena/timeline y export existentes, sin bindings de run | templates dinámicos con slots | separación editorial existe, conexión a gameplay no |
| Import/content | pipeline parcial, manifest y provenance en slices | import íntegro preservando campos + overrides | parser/fallback insuficientes para producción |
| Update/save | sin packages ni save gameplay | versionado, verificación, migración | infraestructura ausente |

El inventario detallado, reuso, duplicaciones y límites 3DS vive en [North Star](../GUI_3DS_NORTH_STAR.md). Mantener los sistemas de autoría/export/runtime reutilizables y el BattleEngine sólo como bridge hasta estudiar upstream.

## Criterios para decisiones futuras

- Preguntar “¿cómo funciona?” implica consultar una revisión oficial fijada antes de implementar.
- Clasificar el cambio como datos, presentación, capability runtime o ambos.
- Nuevas especies/items/biomas/moves deben entrar por contenido importado y bindings/resolvers, no listas UI o escenas duplicadas.
- Nuevas capacidades 3DS son específicas de runtime y deben explicitar memoria, renderer/input, formatos y compatibilidad.
- Contenido importado conserva desconocidos seguros y su provenance. Overrides 3DS se aplican por separado.
- No declarar integración por la existencia de una clase: verificar fuente → import → datos → test → resultado.

## Referencias upstream examinadas

Las rutas de `beta` son referencias navegables, no una pin. Para cada trabajo se debe contrastar con el commit exacto configurado en `PokerogueSource.js` y actualizar/registrar el pin sólo mediante una tarea de import deliberada.

- [Repositorio oficial pagefaultgames/pokerogue](https://github.com/pagefaultgames/pokerogue)
- [Pokémon runtime state](https://github.com/pagefaultgames/pokerogue/blob/beta/src/field/pokemon.ts) y [datos persistentes/por-wave/por-turno](https://github.com/pagefaultgames/pokerogue/blob/beta/src/data/pokemon/pokemon-data.ts)
- [Datos de especie upstream](https://github.com/pagefaultgames/pokerogue/blob/beta/src/data/balance/species/generation-01.ts)
- [Reglas de Daily run](https://github.com/pagefaultgames/pokerogue/blob/beta/src/data/daily-seed/daily-run.ts)
- [Release notes sobre rechazo de saves incompatibles](https://github.com/pagefaultgames/pokerogue/releases)

No se infiere del límite de una modalidad que el engine tenga un máximo global. La auditoría no valida la afirmación de documentación “Endless ~5850”; se considera dato de contenido mutable que se debe verificar contra upstream al implementar.

## Documentos relacionados

- [Contenido y actualizaciones](POKEROGUE_3DS_CONTENT_UPDATE_SPEC.md)
- [Game flow y modos](POKEROGUE_3DS_GAME_FLOW_SPEC.md)
- [Presentación y bindings](POKEROGUE_3DS_PRESENTATION_SPEC.md)
