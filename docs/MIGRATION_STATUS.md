# Estado actual y evidencia pendiente

## Código existente, pendiente de validación final

1. Pins: PokéRogue 8555c08c823b856cbec4eb99ca84ea52a955836d; assets 056a1f408f26a3be4fef243f7462cb43608c7928; locales 23aea1cb0da5a0b15b836f3c243791591cc42303. Autoridad: tools/js/data/PokerogueSource.js.
2. Conteos, hash y provenance: project/data/pokerogue/import-report.json y canonical-content.json. No son límites del engine.
3. Runtime limitado: encounters, RNG, daño, etapas, clima/críticos/precisión/PP y familias migradas. Self-target stat buffs y Trick Room conectados; inmunidades sonora/polvo con metadata upstream y callbacks desconocidos explícitos.
4. Guardado v8, journal dual y export .p3save. Validator limitado a waves 1–9/bioma inicial; restaurar clima no neutral permanece bloqueado.
5. Conversión reportada de 2902 atlases a 2903 páginas t3x, incluida espalda de Thundurus Therian en dos páginas. Inventario físico en build/upstream-assets y build/romfs; animaciones upstream 10 FPS. Hardware no verificado.
6. ContentUpdateStore y lecturas indexadas acotadas existen; catálogos siguen compilados. OTA completa no disponible.

## No completado

1. Equipos completos, todos los efectos/status/abilities/items, dobles y entrenadores jugables. battleInputSupported mantiene bloqueo de entrenadores.
2. Rewards, captura, mapa/biomas, progresión hasta wave 200, combate final y resumen.
3. Presentación web adaptada a ambas pantallas, audio y rendimiento Old 3DS.
4. Save completo, migraciones de contenido y actualización firmada desde consola.
5. Ejecución final de pruebas, compilación 3dsx, Azahar y hardware.

## Limpieza aprobada por el usuario

1. Retirados servidor, HTML/CSS y editor/shell/preview web. Dependencias del pipeline trasladadas a tools/js, sin reconstruir el Studio.
2. Retiradas suites monolíticas antiguas del Studio; conservadas migración/RNG/battle/native. Nuevo ejecutor las registra sin modificar sus asserts; no se ejecutó.
3. Documentos históricos consolidados en este estado, lista de pendientes y contrato OTA; mapa semántico obsoleto retirado. Provenance sigue en los datos canónicos y código.
4. Fallos históricos BETA-UI-8.19/8.34 pertenecen al Studio retirado, no se declaran arreglados. Suites actuales sin resultado validado todavía.
5. Historial Git, assets, clones upstream y runtime/presentación C++ permanecen. Plan y lista exacta de retiros disponibles en el commit de limpieza.
