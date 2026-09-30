# PokéRogue para Old Nintendo 3DS XL — Pendientes y coordinación

## 1. Objetivo y estado de este documento

1. Completar Classic de inicio a victoria/derrota y resumen, con reglas del snapshot upstream, C++ y presentación adaptada a ambas pantallas. Después, completar los demás modos y la actualización de contenido desde la consola.
2. Esta lista es un inventario de trabajo y evidencia pendiente; no certifica que exista una run Classic completa. Una clase, contrato o prueba escrita no demuestra integración jugable.
3. Base de coordinación: rama `codex/pokerogue-3ds-migration`, repositorio `https://github.com/zelned24/GUI_3ds`, checkpoint publicado antes de este cierre `2d10ce6`. El commit que contiene este documento también cierra la parte de IA descrita abajo.
4. Checkout de publicación usado por este agente: `D:/Proyectos/3ds_gui/GUI_3DS-publish`. No confundirlo con otros checkouts locales. Consultar HEAD y cambios locales antes de integrar trabajo externo.
5. Fuentes de requisitos: instrucciones del usuario, AGENTS.md, GUI_3DS_NORTH_STAR.md y el estado consolidado `docs/MIGRATION_STATUS.md`. Incluye requisitos posteriores del usuario: estética web, doble pantalla, C++, OTA y exportación de progreso.
6. Tests y compilación del programa están aplazados por instrucción del usuario. Se permite escribir pruebas y convertir PNG a `.t3x`. No describir funcionalidad como verificada en Azahar/Old 3DS hasta realizar esa validación final.
7. Los documentos históricos de progreso/paridad contienen estados anteriores. El código y los informes de conversión actuales son la referencia para resolver discrepancias. Actualizar esos documentos al completar cada bloque.

## 2. Reparto inmediato entre las dos IA

### 2.1. Parte de este agente — cerrada con este documento

1. Conectar clima resuelto al cálculo de daño previsto usado por el filtro de movimientos capaces de debilitar al rival.
2. Corregir velocidad para la puntuación de entrenadores: efectiva para el activo/oponente; sin modificadores para reservas, según `Pokemon.getMatchupScore`.
3. Reutilizar puntuaciones resueltas al elegir reemplazo, evitando recalcularlas con un campo neutral.
4. Añadir una regresión escrita para reservas con etapas de velocidad opuestas.
5. Revisar diff, registrar trazabilidad y publicar el checkpoint; no ejecutar tests ni compilar.
6. Archivos de este bloque: `project/src/game/FirstRunRuntime.cpp`, `project/include/game/PokemonTrainerAi.hpp`, `test/native/pokemon_battle_state_harness.cpp` y `docs/MIGRATION_STATUS.md`.
7. Esto no habilita todavía el combate de entrenador ni completa su semántica. No continuar modificando estos archivos simultáneamente desde ambas IA sin nuevo reparto.

### 2.2. Parte recomendada para la otra IA — presentación y assets

1. Leer este documento y los contratos; inspeccionar `project/include/runtime`, `project/src/runtime`, `project/src/screens`, `project/src/gfx` y los scripts de assets existentes antes de crear sistemas.
2. Completar los bloques 8 y 9 de esta lista: presentación web en ambas pantallas, controles, carga de sprites/formas, animaciones, fondos, HUD y audio.
3. Consumir estados y eventos del dominio; no añadir reglas de combate, listas de Pokémon ni datos de ataques a la UI.
4. Reutilizar `RuntimeAssetManager`, `PokemonAtlasPresenter`, `ScenePlayer`, AssetIndex y los bindings existentes. No reconstruir el Studio ni crear otro motor de presentación.
5. Trabajar en una rama propia `codex/...` o un worktree separado desde el checkpoint acordado. No compartir un checkout con escrituras concurrentes.
6. No editar inicialmente `FirstRunRuntime.cpp`, `PokemonBattleState.*`, `PokemonTrainerAi.hpp`, `NativeRunSave.*`, el generador central o el contenido generado: solicitar coordinación cuando una integración requiera esos archivos.
7. Registrar conexiones que necesite del dominio mediante un contrato de datos/eventos y un diff revisable. No sustituir estados ausentes por fixtures.
8. Entregar commit, lista de archivos, componentes conectados, dependencias pendientes y pruebas escritas/no ejecutadas. Avisar al usuario al terminar para reunir ambos avances.

### 2.3. Integración conjunta posterior

1. Comparar los commits de ambas IA y resolver primero contratos compartidos; conservar historial y cambios ajenos.
2. Revisar solapamientos de generadores, assets, esquema de guardado y bindings antes de integrar.
3. Revalidar esta lista con el estado integrado y acordar el siguiente reparto. Este documento no autoriza mensajes a otra conversación ni supone que la otra IA ya esté trabajando.

## 3. Contenido canónico y cobertura upstream

### 3.1. Infraestructura existente, pendiente de cobertura final

1. Conservar pins: PokéRogue `8555c08c823b856cbec4eb99ca84ea52a955836d`, assets `056a1f408f26a3be4fef243f7462cb43608c7928`, locales `23aea1cb0da5a0b15b836f3c243791591cc42303`.
2. Hay catálogos importados y generador C++; completar auditoría de campos, referencias y consumidores. El catálogo contiene 1084 especies, 609 formas, 920 movimientos, 320 habilidades, 109 modificadores y 5 modos; son conteos del snapshot, no límites del engine.
3. Mantener SourceSnapshot → importer → normalizer → canonical → overrides → runtime, con repository/revision/path/symbol/SHA-256/schemaVersion y raw desconocido.
4. Completar las representaciones consumibles de evoluciones, condiciones de formas, learnsets, pasivas, recompensas, entrenadores, encuentros especiales y efectos todavía conservados solo como metadata.
5. Crear una matriz por familia de atributos: importado, normalizado, generado, ejecutado, conectado al flujo y verificado. No confundir esos estados.
6. Validar aislamiento de fixtures, referencias locales/assets, registros nuevos y hashes deterministas con dos importaciones al final.
7. Mantener overrides 3DS explícitos y reversibles; distinguir faltante upstream, capacidad no soportada e importación inválida.

## 4. Run, starter y progresión Classic

### 4.1. Flujo de inicio y equipo

1. Completar boot/title/new game/continue/setup y selección de starters con catálogo real, presupuesto/reglas upstream, formas, habilidades y desbloqueos del perfil.
2. Sustituir el estado de un solo starter por equipo completo y reservas con identidad, HP, PP, status, EXP, formas, objetos y pasivas persistentes.
3. Resolver shiny/variantes, género/naturaleza/IVs, movimientos iniciales, herencia relevante y personalización conforme al perfil upstream.
4. Integrar aprendizaje/reemplazo de movimientos, evolución y selección de forma al subir de nivel. La ganancia básica de EXP existente no completa estos flujos.

### 4.2. Waves, biomas y mapa

1. Conectar `PokerogueBiomeTransition.hpp` y los enlaces canónicos a la run real, incluida elección por Map Modifier.
2. Completar transición cada segmento, reloj/time-of-day, semillas y continuidad del campo según upstream. No crear una tabla inventada wave por wave.
3. Completar encuentros salvajes, dobles, jefes, entrenadores aleatorios/fijos, rivales y encuentros especiales/mystery del snapshot.
4. Completar niveles, rarezas, variantes, escalado, composición de grupos, movimientos y modificadores de encuentros para toda la progresión.
5. Conectar recuperación/interludios/transiciones especiales y entrada a End según reglas canónicas.
6. Eliminar límites actuales de guardado a waves 1–9/bioma inicial solo después de migrar el estado y la restauración correspondientes.
7. Completar waves 1–200, segmentos de jefes y combate final de Classic con victoria/derrota auténticas.
8. Demostrar continuidad encounter → battle → result → reward → next wave → next biome; contratos aislados no bastan.

## 5. Dominio de combate C++

### 5.1. Fases y comandos

1. Completar cola/fases de entrada, turnos, selección de objetivos, comandos de movimiento/cambio/objeto/captura/huida y resultados.
2. Integrar estados que bloquean o alteran movimientos antes de consumir PP y distinguir cancelación, fallo previo, fallo de efecto y objetivo inválido.
3. Completar orden multi-actor, prioridad por habilidad/objeto, empates, cambios y acciones forzadas con RNG upstream.
4. Mantener eventos tipados y UI → command → engine → event → binding; separar fases/resolvers/effects.
5. Integrar política de ignorar habilidades, habilidad suprimida/cambiada, pasivas, objetivos aliados y visibilidad de habilidades en simulaciones de IA.
6. Completar dobles y resolución multiobjetivo; el bucle jugable actual continúa limitado a un oponente.
7. Auditar transacciones: fallos no deben dejar HP/PP/campo/RNG parcialmente aplicados; cerrar guardados únicamente en estados coherentes.

### 5.2. Movimientos y daño

1. Completar las familias de efectos todavía excluidas por el filtro de movimientos del runtime, con cobertura por atributo real upstream.
2. Migrar daño fijo/variable, ataques múltiples, carga/recarga, retroceso/drenaje, auto-KO, prioridad condicional, cambios de objetivo, sustituto, protección y movimientos reflejados.
3. Completar status principales/secundarios, confusión, trampas, tags, inmunidades, curación, cambios de etapas y efectos de cambio.
4. Conectar el resolver de etapas existente al flujo real, incluida reacción de habilidades y objetos.
5. Completar potencia, tipo/categoría variables, STAB especial, efectividad modificada y redondeos de todos los modificadores.
6. Extender precisión/críticos/velocidad/PP a items, pasivas, aliados y tags; los perfiles actuales cubren solo familias migradas.
7. Pressure y Trick Room ya tienen rutas conectadas limitadas; falta validarlas y ampliar sus condiciones al estado completo de combate.

### 5.3. Habilidades, campo y efectos de fin de turno

1. Completar triggers de entrada, salida, antes/después de ataque/daño, contacto, KO, cambios de status, etapas, turno y bioma.
2. Integrar climas no neutrales en la run: generación, invocación, duración, supresión, daño residual, forma y guardado. Actualmente el codec almacena clima, pero el runtime rechaza restaurar clima no neutral.
3. Completar terreno, hazards, pantallas y arena tags restantes; integrar interludio sin aplicar lapsos/efectos que upstream omite.
4. Completar condiciones de inmunidad/absorción, protección de daño, habilidades de cambio de tipo/estadística y copias/supresión.
5. Integrar Mega, Gigantamax, Terastal, fusión y otras transformaciones presentes en el snapshot mediante datos y estado, no escenas por especie.
6. Completar efectos y curaciones de fin de turno, faint simultáneo y reglas de victoria/derrota después de todos los efectos pertinentes.

## 6. Entrenadores e IA

### 6.1. Equipos y decisiones

1. El catálogo/equipos y partes de la IA existen; el combate de entrenador sigue bloqueado en `battleInputSupported`. Resolver dependencias antes de quitar ese bloqueo.
2. Completar todos los generadores de equipo, plantillas/callbacks, firmas, variantes, roles, dificultad, objetos y habilidades del snapshot.
3. Completar puntuación de matchup con inmunidades/absorciones, habilidades, illusion, movimientos utilizables, campo y visibilidad upstream.
4. Completar valoración de atributos de movimientos, condiciones, objetivos múltiples y todos los modos de decisión de IA.
5. Completar cambio voluntario con queue/trap/hazards/efectos de entrada y selección tras faint; preservar HP/PP/status/EXP por miembro.
6. Integrar entrada del actor y efectos de habilidad/objeto antes de resolver el ataque contra un entrante.
7. Conectar entrenador completo → todos sus miembros → recompensa y siguiente encuentro, incluido rival/jefes/final.
8. Revisar secuencia de RNG constructor/selección/switch/turno contra casos reales pinned, no contra resultados inventados.

## 7. Capturas, items, recompensas y perfil

### 7.1. Acciones y economía

1. Completar inventario, clases/instancias/modificadores, acumulación, duración, límites, objetivos y elegibilidad canónicos.
2. Completar Poké Balls y cálculo de captura, bloqueo en encuentros no capturables, incorporación al equipo/reserva y desbloqueos.
3. Completar uso de curas, revivir, recuperación de PP, berries, held items, transferencia y retirada.
4. Completar dinero, compra, costes, rerolls, tiers/pools/pesos de recompensa y restricciones por equipo/run.
5. Conectar selección de recompensa real: el flujo existente de saltar recompensa no satisface el requisito de recompensa completa.
6. Completar EXP de equipo, participantes, EXP Share/modificadores y recompensas especiales; evitar repetir EXP al restaurar/cambiar.
7. Completar pasivas/desbloqueos, candies, vouchers/eggs y demás progreso del perfil aplicable al snapshot, con persistencia separada de la run.

## 8. Presentación web adaptada a dos pantallas — otra IA

### 8.1. Pantallas y navegación

1. Reproducir identidad visual upstream: pixel art, tipografía, colores, paneles, HUD, barras, iconos, diálogos y selección.
2. Pantalla superior 400×240: campo, Pokémon, fondos, animaciones y HUD principal. Inferior 320×240: acciones, movimientos, equipo, inventario y navegación contextual.
3. Completar title/setup/starter/map/battle/rewards/party/items/settings/continue/win/lose/summary, con modelos y bindings dinámicos.
4. Implementar foco y navegación D-pad/A/B/X/Y/L/R/Start/Select, táctil resistivo y feedback coherente sin dependencia exclusiva del táctil.
5. Integrar HP/EXP/PP/status, cambio de Pokémon, selección de objetivos, mensajes de combate y efectos mediante eventos del dominio.
6. Resolver idiomas mediante locale IDs, fallback explícito, fuentes/glyphs y límites de texto; eliminar presentación inglesa fija donde exista contenido localizado.
7. Mantener presentación C++ y ScenePlayer separados del dominio; Studio web retirado.

### 8.2. Sprites, formas, animaciones y audio

1. Reutilizar el inventario convertido: informe existente registra 2902 atlases → 2903 páginas `.t3x`, incluida espalda de Thundurus Therian en dos páginas. No volver a limitar contenido a tres especies de ejemplo.
2. Verificar inventario físico/hashes actual antes de empacar; conectar índice de especies/formas/front/back/shiny/variantes a assets reales disponibles.
3. Completar recorte, origen, offsets, escalado pixel-perfect, orden de frames, velocidad y transiciones de animación; no cargar todos los atlases simultáneamente.
4. Completar fondos de bioma/campo, UI textures, iconos de items/tipos/status y entrenadores con procedencia y conversión.
5. Completar efectos de movimientos/habilidades/captura/faint/cambio y sus referencias. No inventar assets para disimular faltantes.
6. Completar música por contexto, cries y efectos sonoros, conversión, streaming/caché y volumen/configuración.
7. Registrar toda adaptación visual por límites 3DS y capturas comparables con web; fidelidad visual y rendimiento aún no demostrados en consola.

## 9. Old 3DS: memoria, almacenamiento y rendimiento

### 9.1. Presupuestos y carga

1. Medir RAM/VRAM/texturas/stack/heap/frame time/tiempos SD en Old 3DS; no asumir resultados de PC o New 3DS.
2. Definir presupuestos, cachés y eviction; cargar páginas visibles y liberar assets al salir de contexto.
3. Completar carga asíncrona o escalonada, fallos de lectura, mensajes de carga y recuperación sin bloquear indefinidamente.
4. Evitar monolitos de assets en memoria y revisar coste de catálogos generados/índices; diseñar packs indexados compatibles con updates.
5. Verificar cambios de páginas del atlas grande, swaps de equipo, sprites simultáneos y efectos en dobles.
6. Medir estabilidad prolongada, suspensión/reanudación, fugas, fragmentación y recuperación tras fallos.

## 10. Guardado, continuar y exportación

### 10.1. Estado persistente completo

1. Reutilizar journal dual, checksums, schema v8 y export `sdmc:/3ds/pokerogue/exports/progress.p3save`; no crear un segundo save system.
2. Extender estado a equipo completo, status/tags, objetos/modificadores, rewards, perfil/unlocks, bioma/ruta/wave 200, campos y formas.
3. Guardar estado RNG necesario para cada frontera de fase; dejar claro si se permiten puntos entre turnos o fases y no reconstruir por supuestos inválidos.
4. Restaurar de forma atómica todos los estados soportados, validar referencias y evitar replay con recompensas/EXP/objetos duplicados.
5. Completar migraciones de esquema y compatibilidad de hash/catálogo después de updates; conservar guardado anterior y reportar incompatibilidad.
6. Completar Continue desde title y resumen de victoria/derrota con estadísticas reales; no sobrescribir una run por fallo de lectura.
7. Verificar export/import entre consolas y documentar su formato. El archivo nativo actual no equivale a un save de cuenta web; compatibilidad web exige contrato y migración propios si se solicita.

## 11. Actualizaciones desde la propia consola

### 11.1. Contenido sin reinstalar el ejecutable

1. Reutilizar `ContentUpdateStore`: existe infraestructura de manifiesto/integridad/activación, pero OTA completa aún no está disponible.
2. Crear un formato versionado de catálogo cargable en runtime; los catálogos compilados actuales no permiten añadir gameplay data solo descargando texturas.
3. Completar empaquetado/publicación reproducible de catálogos, locales, índices, assets y provenance desde pins upstream.
4. Completar backend HTTPS 3DS, certificados/verificación, clave de editor confiable y verificador de firma; nunca activar un fallback sin firma.
5. Conectar descarga fragmentada/streaming, reanudación, espacio SD, hashes, versiones/capacidades y fallos de red.
6. Activar releases de forma atómica, soportar rollback y preservar el contenido anterior ante interrupción/corrupción.
7. Conectar loader de datos/assets/locales al release activo y migraciones de save compatibles.
8. Añadir UI de buscar/descargar/activar actualización y mensajes de progreso/error. Cambios de capacidad C++ pueden requerir ejecutable nuevo; diferenciar esos casos de actualizaciones de contenido.

## 12. Pruebas y entrega — al final de la implementación

### 12.1. Cobertura automatizada

1. Ejecutar `npm test`, `npm run native-parity`, regresiones nativas/host y golden cases relevantes. Mantener evidencia completa de fallos conocidos y nuevos.
2. Verificar que harnesses escritos estén realmente incluidos en los comandos finales; una prueba sin ejecución no es evidencia de éxito.
3. Cubrir importación real/determinismo/provenance/raw/fixtures/referencias y pipeline assets físico → conversión → runtime.
4. Cubrir estados/status/efectos/IA/RNG/fases/transacciones, equipos, trainers/bosses/doubles y captura.
5. Cubrir reward → progression → biome → wave 200 → final → win/lose → summary.
6. Cubrir save/load/export/import/migración, journals dañados/escritura interrumpida y rechazo atómico.
7. Cubrir OTA firma/red/corrupción/espacio/interrupción/rollback y compatibilidad de catálogo/save.
8. Actualizar trazabilidad y matriz de paridad con evidencia ejecutada y límites reales. No arreglar asserts solo para conseguir PASS.

### 12.2. Compilación, Azahar y hardware

1. Compilar y enlazar devkitARM/libctru/Citro2D, producir `.3dsx` y paquetes físicos necesarios cuando el usuario autorice la etapa final ya aplazada.
2. Verificar configuración/toolchain/CI y reproducibilidad del paquete; registrar versiones y hashes.
3. Probar flujo real en Azahar con assets completos; recopilar capturas, logs, errores y diferencias visuales.
4. Probar en Old Nintendo 3DS XL: controles, SD, audio, render, memoria, FPS, carga, save/export/continue y updates.
5. Demostrar una run Classic end-to-end y derrotas/restauración, no solo un combate o compilación exitosa.
6. Registrar hardware status explícito: verificado o pendiente. Azahar no demuestra memoria/rendimiento de Old 3DS.
7. Entregar reporte final de requisitos, commits, branch/status, pruebas, hardware, evidencia visual, límites y releases. No declarar objetivo completo con gates pendientes.

## 13. Después de Classic y migración completa

### 13.1. Modos y mantenimiento

1. Completar Endless, Daily, Spliced Endless y Challenge desde sus políticas/datos upstream, después de estabilizar Classic.
2. Completar retos/condiciones especiales, seed diaria, progresión/rewards específicas y persistencia por modo.
3. Endurecer save/content update, sincronización de upstream y detección de cambios de esquema/atributos no soportados.
4. Preparar release candidate con licencias/créditos/procedencia, instrucciones SD/instalación/actualización/exportación y límites de soporte.

## 14. Forma de reportar avances y coordinar

### 14.1. Entrega de cada IA

1. Informar commit/branch y archivos modificados; describir qué cadena fuente → dato → ejecución → presentación quedó conectada.
2. Marcar por tarea: pendiente, código parcial, integración pendiente, validación pendiente o verificada con evidencia.
3. Enumerar archivos compartidos y dependencias que necesitan al otro agente; no ocultar restricciones con warnings genéricos.
4. Notificar al usuario al terminar el bloque acordado y esperar su coordinación antes de entrar en los archivos que está modificando la otra IA.
5. Este agente ha cerrado el bloque 2.1. La siguiente asignación de gameplay debe acordarse después de conocer los commits y contratos de la otra IA; el objetivo global sigue pendiente.
