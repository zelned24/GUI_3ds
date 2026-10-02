# Checklist de implementación — PokéRogue para Old 3DS XL

## Cómo medir el avance

- Referencia inspeccionada: commit `165a23d`, rama `codex/pokerogue-3ds-migration`.
- Objetivo: PokéRogue jugable y fiel al snapshot upstream en Old 3DS, con ambas pantallas, progreso exportable y actualizaciones desde consola.
- Cada ID es estable para reportar avances: `MOV-07`, `GUI-03`, etc. No equivale a un movimiento/habilidad individual.
- Una casilla sin marcar puede tener código parcial; el resumen de cada área indica lo existente. Marcarla solo con integración completa y evidencia ejecutada pertinente.
- Estados para reportes: **pendiente**, **parcial**, **implementado sin verificar**, **verificado**. Registrar commit, archivos, prueba/resultado y limitaciones.
- No hay porcentaje global fiable todavía: falta medir la cobertura por ID/atributo/contexto. Contar estas tareas como pesos iguales distorsionaría el avance.
- Pins: juego `8555c08c823b856cbec4eb99ca84ea52a955836d`; assets `056a1f408f26a3be4fef243f7462cb43608c7928`; locales `23aea1cb0da5a0b15b836f3c243791cc42303`.
- Fuente de detalle: [estado](../MIGRATION_STATUS.md), [inventario anterior](POKEROGUE_3DS_REMAINING_WORK.md), `project/` y scripts actuales. El inventario anterior incluye notas históricas, no resultados vigentes.
- GUI significa interfaz **del juego**; no reconstruir el editor eliminado. El código actual usa gameplay C++ y bridge QuickJS opcional: su presencia no demuestra un port completo de la web.

## Orden de avance

1. Medir cobertura real y cerrar combate/HP/habilidades/movimientos que impiden runs.
2. Cerrar progresión, trainers/bosses, items/capturas, decisiones y persistencia de Classic.
3. Integrar interfaz/animación/audio en ambas pantallas y asegurar recursos Old 3DS.
4. Completar catálogo actualizable, exportación y restantes modos.
5. Ejecutar tests, compilación, Azahar y hardware en la etapa final.

## 1. Contenido canónico y catálogo

### Estado actual

Hay importación pinned, modelos canónicos, provenance y tablas C++ generadas. Importado no significa ejecutado.

### Pendientes y criterios de cierre

- [ ] **DAT-01.** Auditar cada especie, forma, movimiento, habilidad, modificador y modo del snapshot y sus referencias.
- [ ] **DAT-02.** Crear inventario por ID/atributo: importado → generado → ejecutado → conectado → verificado; contabilizar exclusiones del runtime.
- [ ] **DAT-03.** Completar datos consumibles de evoluciones, formas, pasivas, entrenadores y encuentros especiales todavía conservados como raw.
- [ ] **DAT-04.** Conservar campos desconocidos, hashes, revisiones, símbolos y overrides 3DS sin modificar upstream.
- [ ] **DAT-05.** Validar locales, learnsets, assets y referencias cruzadas; rechazar fixtures en producción.
- [ ] **DAT-06.** Demostrar dos importaciones/generaciones idénticas y compatibilidad frente a contenido nuevo.

## 2. Inicio, modos y progresión

### Estado actual

FirstRunRuntime contiene setup, equipo, progresión y decisiones; Classic completo no está demostrado.

### Pendientes y criterios de cierre

- [ ] **FLU-01.** Cerrar título → nueva partida → selección de modo → starters → run real.
- [ ] **FLU-02.** Completar selección de varios starters, presupuesto, formas, habilidad/pasiva, variantes y desbloqueos.
- [ ] **FLU-03.** Cerrar gestión del equipo de seis, reservas, orden, cambios voluntarios y reemplazos forzados.
- [ ] **FLU-04.** Completar aprendizaje/reemplazo de movimientos, evoluciones y condiciones de formas especiales.
- [ ] **FLU-05.** Cerrar waves 1–200, transiciones cada diez waves y continuidad del RNG/campo.
- [ ] **FLU-06.** Completar elección de bioma con Map, reloj, clima y rutas/entrada a End.
- [ ] **FLU-07.** Completar encuentros salvajes, dobles, jefes, rivales, entrenadores fijos y aleatorios.
- [ ] **FLU-08.** Completar mystery encounters y eventos especiales del snapshot.
- [ ] **FLU-09.** Completar Eternatus/fases finales, victoria, derrota, resumen y retorno al título.
- [ ] **FLU-10.** Completar Endless, Spliced Endless, Daily y Challenge, sus reglas y persistencia.

## 3. Turnos y comandos

### Estado actual

Existen comandos, gates de capacidad y transacciones; numerosos contextos siguen restringidos.

### Pendientes y criterios de cierre

- [ ] **TUR-01.** Completar cola ordenada de fases de entrada, acción, daño, callbacks, faint y fin de turno.
- [ ] **TUR-02.** Completar prioridad de movimientos/habilidades/items, empates y RNG upstream.
- [ ] **TUR-03.** Completar interrupciones: sueño, congelación, parálisis, confusión, flinch, infatuación y desobediencia cuando aplique.
- [ ] **TUR-04.** Completar disable, encore, taunt, torment, imprison, heal block, throat chop, gravedad y bloqueo por items.
- [ ] **TUR-05.** Completar dobles/multiobjetivo, aliados, redirección y ejecución única de checks/PP por acción.
- [ ] **TUR-06.** Completar protección, sustituto, reflexión de movimientos y acciones indirectas.
- [ ] **TUR-07.** Completar comandos de objetos, captura, huida y cambios con sus restricciones.
- [ ] **TUR-08.** Auditar rollback HP/PP/status/etapas/campo/RNG ante fallos tardíos y límites de guardado.

## 4. Movimientos y cálculo de daño

### Estado actual

Daño básico y varias familias de críticos, status, etapas, curación, drenaje y recoil tienen código. Overheat/Scald/Flare Blitz se ampliaron recientemente; pruebas pendientes.

### Pendientes y criterios de cierre

- [ ] **MOV-01.** Completar todos los atributos y builders excluidos por supportsActiveBattleMove; no habilitar ataques omitiendo efectos.
- [ ] **MOV-02.** Completar daño fijo/variable, potencia condicional y cálculo con estadísticas alternativas.
- [ ] **MOV-03.** Completar tipos/categorías variables, STAB especial, inmunidades, absorciones y redondeos.
- [ ] **MOV-04.** Completar precisión/evasión, always-hit, críticos y bypass de etapas en todos los contextos.
- [ ] **MOV-05.** Completar ataques múltiples, repeticiones, cadenas y efectos por hit/último hit.
- [ ] **MOV-06.** Completar carga, recarga, preparación, movimientos de dos turnos y semiinvulnerabilidad.
- [ ] **MOV-07.** Completar recoil, drenaje, Liquid Ooze, auto-KO y composición de varios efectos.
- [ ] **MOV-08.** Completar curación/status cure, Rest, curación variable y objetivos aliados/enemigos.
- [ ] **MOV-09.** Completar boosts/drops, reflexión/copia, reacciones y White Herb conectado a inventario.
- [ ] **MOV-10.** Completar estados secundarios, confusión, trampas, tags y probabilidad modificada.
- [ ] **MOV-11.** Completar intercambio/copia de movimientos, habilidades, estadísticas y cambio forzado.
- [ ] **MOV-12.** Completar flags de contacto, sonido, polvo, mordisco, puño, bala, viento, dance, reckless y sus callbacks.
- [ ] **MOV-13.** Verificar los casos recientemente escritos: KO por retroceso, descongelación previa, cura del rival y burn posterior.

## 5. Habilidades y pasivas

### Estado actual

Hay perfiles y resolvers parciales; Simple/Contrary, Mirror Armor, Opportunist, Defiant/Competitive y familias de status tienen rutas. No existe cobertura total.

### Pendientes y criterios de cierre

- [ ] **HAB-01.** Inventariar cada habilidad y atributo: soportado por contexto, parcial o sin implementar.
- [ ] **HAB-02.** Completar triggers de entrada/salida, cambio, turno, bioma y formas.
- [ ] **HAB-03.** Completar potencia/daño/STAB/tipo/precisión/crítico/velocidad/PP y sus condiciones.
- [ ] **HAB-04.** Completar inmunidad/absorción, contacto, PostDefend/PostAttack/PostDamage y KO.
- [ ] **HAB-05.** Completar reacciones de status, etapas, curación, tags y fin de turno.
- [ ] **HAB-06.** Completar pasivas y coexistencia de componentes sin duplicar aplicación.
- [ ] **HAB-07.** Completar suppression, Mold Breaker/ignore abilities, cambios/copias e interacción de aliados.
- [ ] **HAB-08.** Completar bypassFaint y actividad de callbacks después de faint, con RNG correcto.
- [ ] **HAB-09.** Completar habilidades condicionadas por forma/clima/terreno/items y capacidades no reconocidas.
- [ ] **HAB-10.** Contrastar simulación de IA y revelación de habilidades con ejecución real.

## 6. HP, PP, EXP y estados

### Estado actual

Existen fórmulas, daños/curas, etapas, estados y persistencia. Falta cobertura completa y ejecución final.

### Pendientes y criterios de cierre

- [ ] **HP-01.** Validar HP máximo/actual, daño aplicado frente a solicitado y clamps en todos los efectos.
- [ ] **HP-02.** Completar segmentos/escudos de jefes con recoil, residual, drenaje y daño indirecto.
- [ ] **HP-03.** Completar faint simultáneo, equipo sin activos, cambio obligatorio y resultado final tras todos los efectos.
- [ ] **HP-04.** Completar revivir, curaciones, regeneración, bloqueo de curación y modificadores.
- [ ] **HP-05.** Completar PP/PP Up/Pressure, uso virtual/indirecto, agotamiento y Struggle.
- [ ] **HP-06.** Completar EXP por participantes/equipo, modificadores, límites de nivel y decisiones pendientes.
- [ ] **HP-07.** Completar duración/origen/lapsos de todos los status y tags; guardar sin pérdidas.
- [ ] **HP-08.** Conectar HP/EXP/PP/status del dominio a barras, números, iconos y mensajes de presentación.

## 7. Campo, clima y transformaciones

### Estado actual

Hay clima, ciclo de vida y Trick Room parciales. El restore permite clima cuando weatherBattleSupported resuelve sus actores; no hay rechazo universal de clima no neutral.

### Pendientes y criterios de cierre

- [ ] **CAM-01.** Completar generación/invocación/duración/supresión de todos los climas y sus efectos.
- [ ] **CAM-02.** Completar terreno, hazards, pantallas, arena tags y orden de lapsos.
- [ ] **CAM-03.** Completar daño/curación residual y reglas de interludio entre biomas.
- [ ] **CAM-04.** Completar efectos de entrada sobre un Pokémon que cambia y contraataque posterior.
- [ ] **CAM-05.** Completar Mega, Gigantamax, Terastal, fusión y otras transformaciones del snapshot.
- [ ] **CAM-06.** Completar persistencia/reversión de tipos/formas y callbacks asociados.

## 8. Entrenadores e IA

### Estado actual

Hay equipos, SMART AI y cambios; puntuación de secundarios/drenaje/recoil se corrigió, sin validación ejecutada.

### Pendientes y criterios de cierre

- [ ] **IA-01.** Completar templates/callbacks, firmas, variantes, roles, dificultad y generación de todos los entrenadores.
- [ ] **IA-02.** Completar valoración de todos los atributos, condiciones, inmunidades y objetivos múltiples.
- [ ] **IA-03.** Completar matchup con habilidades/campo/modificadores e información visible.
- [ ] **IA-04.** Completar cambio voluntario/forzado, traps/hazards y efectos antes del ataque al entrante.
- [ ] **IA-05.** Completar todos los modos de decisión upstream y selección aleatoria determinista.
- [ ] **IA-06.** Cerrar equipo completo → derrota del entrenador → recompensa → siguiente encuentro.

## 9. Captura, items, recompensas y perfil

### Estado actual

Existen fórmulas de captura, modificadores y recompensas parciales; no representan todos los efectos del catálogo.

### Pendientes y criterios de cierre

- [ ] **OBJ-01.** Completar bolas, sacudidas, bloqueos y restricciones por encuentro/jefe/final.
- [ ] **OBJ-02.** Completar incorporación de capturas con equipo lleno, sustitución y desbloqueos.
- [ ] **OBJ-03.** Completar inventario, stacks, duración, límites, transferencia y retirada.
- [ ] **OBJ-04.** Completar berries, held items, curas, revivir, PP y todos los efectos del catálogo.
- [ ] **OBJ-05.** Completar selección/objetivos de rewards, pools/pesos/tiers y restricciones canónicas.
- [ ] **OBJ-06.** Completar dinero, tienda, compras, rerolls y costes.
- [ ] **OBJ-07.** Completar candies, pasivas, huevos, vouchers, shiny/unlocks y progreso persistente.
- [ ] **OBJ-08.** Evitar duplicar items/rewards/EXP al guardar, cargar o repetir decisiones.

## 10. Interfaz del juego en dos pantallas

### Estado actual

Se conserva presentación C++/ScenePlayer y bridge QuickJS opcional. El editor/Studio web está retirado. Fidelidad visual sin demostrar.

### Pendientes y criterios de cierre

- [ ] **GUI-01.** Reproducir estilo web: pixel art, tipografía, paleta, marcos, cursores, HUD y diálogos.
- [ ] **GUI-02.** Cerrar pantalla superior 400×240: campo, sprites, fondos, HUD y animaciones.
- [ ] **GUI-03.** Cerrar pantalla inferior 320×240: acciones, movimientos, equipo e inventario.
- [ ] **GUI-04.** Completar title/new/continue/setup/starters y selección de modos.
- [ ] **GUI-05.** Completar battle, cambio/objetivos, captura y mensajes/feedback de fallo.
- [ ] **GUI-06.** Completar mapa/bioma, rewards, tienda, party/items y decisiones de aprendizaje/evolución.
- [ ] **GUI-07.** Completar settings, idioma, audio, exportación y actualización de contenido.
- [ ] **GUI-08.** Completar win/lose/summary y recuperación de errores de carga.
- [ ] **GUI-09.** Completar D-pad/A/B/X/Y/L/R/Start/Select y táctil resistivo con foco coherente.
- [ ] **GUI-10.** Completar locales, glyphs, fallback y texto largo; retirar strings fijas donde exista localización.
- [ ] **GUI-11.** Mantener UI → comando → evento → binding; sin reglas ni especies hardcodeadas.
- [ ] **GUI-12.** Auditar rol QuickJS/Phaser→Citro2D y bundle: bridge no equivale a ejecutar todo PokéRogue upstream.

## 11. Assets, animación y audio

### Estado actual

Hay catálogos de atlases/t3x y presenter; el inventario histórico debe cotejarse con archivos físicos actuales.

### Pendientes y criterios de cierre

- [ ] **AST-01.** Auditar todos los PNG/t3x físicos, índices, hashes y provenance, sin reducir a especies de ejemplo.
- [ ] **AST-02.** Completar front/back, forms, shiny, variantes y resolución dinámica por ID.
- [ ] **AST-03.** Completar páginas de atlas grandes, offsets/origen/recorte y cambios de frame.
- [ ] **AST-04.** Completar animación de idle/ataque/daño/faint/captura/cambio/evolución.
- [ ] **AST-05.** Completar fondos de bioma, plataformas, trainers y texturas/iconos de interfaz.
- [ ] **AST-06.** Completar efectos de movimientos/habilidades y referencias a animaciones.
- [ ] **AST-07.** Completar música, cries, SFX, conversión, streaming y volumen.
- [ ] **AST-08.** Validar empaquetado/carga/liberación y capturas comparables con web en ambas pantallas.

## 12. Guardado, continuar y exportación

### Estado actual

NativeRunSave/runtime v18; codecs de actor v10 y compatibilidad anterior. Hay journals y bundles; cobertura de estados completa pendiente.

### Pendientes y criterios de cierre

- [ ] **SAV-01.** Persistir todo el equipo, modificadores, status/tags, formas, campo y decisiones pendientes.
- [ ] **SAV-02.** Persistir RNG de cada frontera soportada sin reconstrucción por suposiciones.
- [ ] **SAV-03.** Completar restauración atómica para todos los estados de run/mode/profile.
- [ ] **SAV-04.** Completar migraciones de esquema y compatibilidad de hash/catálogo tras updates.
- [ ] **SAV-05.** Completar Continue y protección del save previo ante errores de lectura/escritura.
- [ ] **SAV-06.** Cerrar export/import desde UI y SD, entre consolas, con validación del bundle.
- [ ] **SAV-07.** Documentar formato nativo y compatibilidad web; no afirmar importación de cuenta web inexistente.
- [ ] **SAV-08.** Verificar corrupción, cortes de escritura, rollback y ausencia de recompensas duplicadas.

## 13. Actualización desde la consola

### Estado actual

ContentUpdateStore ofrece infraestructura; sus packs de presentación no sustituyen un catálogo de gameplay cargable.

### Pendientes y criterios de cierre

- [ ] **OTA-01.** Completar formato de catálogo versionado y loader runtime para añadir datos sin reinstalar.
- [ ] **OTA-02.** Completar packager/publicación reproducibles de datos/locales/assets/índices.
- [ ] **OTA-03.** Completar HTTPS 3DS, certificados, firma y clave confiable; rechazar contenido sin autenticar.
- [ ] **OTA-04.** Completar descarga streaming/reanudación, espacio SD, hashes y cancelación.
- [ ] **OTA-05.** Completar instalación/activación atómicas y rollback tras corte o corrupción.
- [ ] **OTA-06.** Conectar release activo a datos/assets/locales y migraciones compatibles del save.
- [ ] **OTA-07.** Completar UI de buscar/descargar/activar y errores/progreso.
- [ ] **OTA-08.** Distinguir actualización de contenido de nuevas capacidades que requieren otro ejecutable.

## 14. Memoria y rendimiento Old 3DS XL

### Estado actual

Sin mediciones concluyentes de hardware; Azahar tampoco las reemplaza.

### Pendientes y criterios de cierre

- [ ] **3DS-01.** Medir RAM/VRAM/heap/stack/frame time y tiempos de lectura SD en Old 3DS.
- [ ] **3DS-02.** Cerrar presupuestos de caches, eviction, carga de páginas y liberación.
- [ ] **3DS-03.** Evitar carga simultánea de catálogo/atlases completos y acotar picos.
- [ ] **3DS-04.** Completar carga escalonada y recuperación por asset faltante/error SD.
- [ ] **3DS-05.** Verificar dobles, efectos simultáneos y atlas multipartes bajo presupuesto.
- [ ] **3DS-06.** Verificar audio, controles, suspensión/reanudación y estabilidad prolongada.

## 15. Validación y entrega final

### Estado actual

Por instrucción del usuario, tests y compilación del programa están aplazados. Pruebas escritas no se marcan como pasadas.

### Pendientes y criterios de cierre

- [ ] **VAL-01.** Ejecutar npm test y registrar todos los fallos conocidos/nuevos.
- [ ] **VAL-02.** Ejecutar npm run native-parity y comprobar cobertura real de harnesses.
- [ ] **VAL-03.** Ejecutar npm run native-test y npm run 3ds-test en la etapa final.
- [ ] **VAL-04.** Ejecutar npm run 3ds-build; producir .3dsx reproducible con assets reales.
- [ ] **VAL-05.** Verificar paridad de reglas/RNG con pinned upstream, además de replay nativo.
- [ ] **VAL-06.** Probar Classic completo en Azahar: inicio → wave 200 → final → resumen.
- [ ] **VAL-07.** Probar derrotas, reservas, save/continue/export/import y fallos de SD.
- [ ] **VAL-08.** Probar OTA firma/red/corrupción/rollback y compatibilidad de save.
- [ ] **VAL-09.** Verificar fidelidad visual con capturas web/dual-screen.
- [ ] **VAL-10.** Probar Old 3DS XL física y registrar recursos, rendimiento y estabilidad.
- [ ] **VAL-11.** Revisar git diff --check, licencias/créditos, instalación SD y release.
- [ ] **VAL-12.** Entregar matriz de requisitos con evidencia, límites y checklist cerrado; no declarar éxito con gates pendientes.

## 16. Registro de avance

| ID | Estado | Commit | Evidencia ejecutada | Falta para cerrar |
|---|---|---|---|---|
| Ejemplo: MOV-07 | Parcial | 1cd6400 / efb650e | Revisión estática; tests no ejecutados | Resto de composiciones, paridad y runtime |
| Ejemplo: SAV-04 | Parcial | Consultar historial de NativeRunSave | Codec v18 inspeccionado; sin gate final | Cobertura de estados y compatibilidad de catálogo |
| Ejemplo: GUI-12 | Parcial | Consultar historial de QuickJSBridge | Bridge y bindRuntime inspeccionados | Alcance completo y ejecución ARM |

Los ejemplos describen infraestructura y límites; no son tareas cerradas. Actualizar filas con cada bloque y mantener las casillas abiertas hasta reunir su evidencia.

### Avance DAT-02: inventario reproducible de declaraciones

- Generación: `node scripts/generate_content_coverage_inventory.mjs`.
- [Resumen por familia](../generated/CONTENT_COVERAGE_INVENTORY.md) y [registros por ID](../generated/CONTENT_COVERAGE_INVENTORY.json).
- Incluye movimientos/habilidades, builders y provenance; no duplica raw TypeScript. Todos los registros comienzan NOT_AUDITED para ejecución: el inventario no infiere soporte por existencia de código.
- DAT-02 permanece parcial: falta conectar matriz de capacidades y evidencia por contexto del runtime, además del resto de dominios.

### Avance HAB-08 / MOV-10: chance tras retroceso

- Resolver de probabilidad acepta actividad explícita de ambas habilidades. POST_APPLY pasa HP restante; Serene Grace no duplica chance tras faint y Shield Dust solo bloquea cuando está activo.
- Regresiones con Flare Blitz 394, Serene Grace 32 y Shield Dust 19 escritas, sin ejecutar. Las dos tareas siguen parciales; no se extiende a pasivas/supresión todavía.

### Avance MOV-02: perfiles de daño fijo/nivel

- Generador produce MoveFixedDamageProfile de FixedDamageAttr literal y LevelDamageAttr exacto con provenance. Sonic Boom/Dragon Rage y Seismic Toss/Night Shade quedan representados.
- Falta conectarlos al cálculo/comando, inmunidades, precisión, jefes e IA con orden RNG upstream. Generar el perfil no habilita el movimiento ni cierra MOV-02.
