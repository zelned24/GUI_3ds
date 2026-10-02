# Checklist de implementación — PokéRogue para Old 3DS XL

## Cómo medir el avance

- Referencia de seguimiento: commit `d145425` más cambios locales parciales de Struggle, rama `codex/pokerogue-3ds-migration`.
- Objetivo: PokéRogue jugable y fiel al snapshot upstream en Old 3DS, con ambas pantallas, progreso exportable y actualizaciones desde consola.
- Cada ID es estable para reportar avances: `MOV-07`, `GUI-03`, etc. No equivale a un movimiento/habilidad individual.
- Una casilla sin marcar puede tener código parcial; el resumen de cada área indica lo existente. Marcarla solo con integración completa y evidencia ejecutada pertinente.
- Estados para reportes: **pendiente**, **parcial**, **implementado sin verificar**, **verificado**. Registrar commit, archivos, prueba/resultado y limitaciones.
- No hay porcentaje global fiable todavía: falta medir la cobertura por ID/atributo/contexto. Contar estas tareas como pesos iguales distorsionaría el avance.
- Pins: juego `8555c08c823b856cbec4eb99ca84ea52a955836d`; assets `056a1f408f26a3be4fef243f7462cb43608c7928`; locales `23aea1cb0da5a0b15b836f3c243791cc42303`.
- Fuente de detalle: [estado](../MIGRATION_STATUS.md), [inventario anterior](POKEROGUE_3DS_REMAINING_WORK.md), `project/` y scripts actuales. El inventario anterior incluye notas históricas, no resultados vigentes.
- GUI significa interfaz **del juego**; no reconstruir el editor eliminado. El código actual usa gameplay C++ y bridge QuickJS opcional: su presencia no demuestra un port completo de la web.

## Resumen del checklist

Estos son criterios de cierre, no cantidades de ataques o habilidades pendientes. Todas las casillas siguen abiertas hasta aportar evidencia de integración y validación; muchas tienen implementación parcial.

| Área | Criterios abiertos |
|---|---:|
| Contenido canónico y catálogo | 6 |
| Inicio, modos y progresión | 10 |
| Turnos y comandos | 8 |
| Movimientos y cálculo de daño | 13 |
| Habilidades y pasivas | 10 |
| HP, PP, EXP y estados | 8 |
| Campo, clima y transformaciones | 6 |
| Entrenadores e IA | 6 |
| Captura, items, recompensas y perfil | 8 |
| Interfaz del juego en dos pantallas | 12 |
| Assets, animación y audio | 8 |
| Guardado, continuar y exportación | 8 |
| Actualización desde la consola | 8 |
| Memoria y rendimiento Old 3DS XL | 6 |
| Validación y entrega final | 12 |
| **Total** | **129** |

Prioridad inmediata: conectar el fallback de Struggle al agotarse PP (**HP-05 / MOV-07**), ampliar efectos y contextos de combate, y completar dobles y segunda fase de Eternatus con su persistencia (**TUR-05 / FLU-05 / SAV-03**). Daño fijo ya tiene rutas de selección, comando y predicción, pendientes de validación. Los tests escritos permanecen sin ejecutar.

## Estado consolidado para seguimiento

Esta tabla describe código inspeccionado, no resultados de ejecución. Las notas cronológicas posteriores registran pasos anteriores; este resumen indica el estado actual.

| Área / IDs | Ya existe | Qué falta implementar o integrar |
|---|---|---|
| DAT-01–06 | Catálogo pinned, generador C++, provenance e inventario por declaraciones | Matriz real de ejecución por ID/contexto, resto de datos raw y referencias completas |
| MOV-02 / IA-02 | Daño constante, nivel, mitad de HP y Psywave conectados a selección, predicción y comando | Potencia variable restante, composición, dobles, modifiers y verificación |
| MOV-04 / MOV-12 | Bypass de etapas y slicing con varios secundarios | Resto de flags, callbacks y combinaciones; slicing no implica Sharpness completa |
| HAB-03 | Sharpness tiene perfil, cálculo de potencia, proveedor y regresión de replay Gallade escritos | Contextos adicionales, pasivas/supresión y validación ejecutada |
| HAB-04 / HP-03 / CAM-03 | Sturdy: tag, daño ordinario/fijo, escudos, confusión/clima y persistencia jugador/enemigo/trainer | Otras supervivencias, pasivas/supresión, dobles y validación ejecutada |
| HAB-01–10 / MOV-01–13 | Familias parciales y gates explícitos | Cobertura de todas las habilidades/movimientos del snapshot; no basta importar metadata |
| HP-05 / MOV-07 | Declaración pinned y comando virtual de Struggle escritos en cambios locales | Selección automática jugador/IA, restricciones, jefes/dobles, feedback y regresiones; todavía no es fallback jugable |
| HP-01–08 | HP/PP/status, daño/curación, EXP y casos de límites | Composición completa, segmentos, faint simultáneo, persistencia y feedback visual |
| FLU-05 / SAV-03 | Checkpoint v20: bioma, actores explícitos, jefes individuales y trainer con estados resueltos | Dobles, fase final, decisiones pendientes y recorrido completo; casos de trainer posterior sin verificar |
| GUI-01–12 / AST-01–08 | Presentación nativa, índices y assets convertidos parciales | Todas las pantallas, HUD HP/PP/EXP, animación/audio, controles y comparación visual |
| SAV-01–08 | Codecs, journals y bundles | Todos los estados de run/perfil, export/import conectado a UI y compatibilidad de contenido |
| OTA-01–08 | Infraestructura de packs | Catálogo de gameplay cargable, firma, descarga e instalación desde consola |
| 3DS-01–06 / VAL-01–12 | Pruebas escritas y pipeline | Ejecución final, build, Azahar y medición en Old 3DS XL física |

### Actualización de cada entrega

1. Indicar los IDs afectados y el commit o cambios locales.
2. Registrar qué ruta está conectada y sus contextos excluidos.
3. Distinguir código escrito de evidencia ejecutada.
4. Marcar una casilla solo cuando se cumpla su criterio completo.
5. Mantener separado el avance de gameplay del de presentación/assets de la otra IA.

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

NativeRunSave/runtime v20; codec de actor v11 cuando hay tag Sturdy, v10 para confusión y compatibilidad anterior. Hay journals y bundles; cobertura de estados completa pendiente.

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

### Avance MOV-02: cálculo de daño fijo tras precisión

- resolveStandardPokemonMoveDamage usa el perfil constante/nivel tras inmunidad y precisión, sin crítico, STAB, burn ni variación de daño. Rechaza capacidades de habilidad aún desconocidas.
- Sigue pendiente habilitar selección/IA/predicción en FirstRunRuntime y pruebas finales. Esta ruta de cálculo no equivale todavía a comando jugable completo.

### Avance MOV-08 / MOV-13: efecto implícito del constructor AttackMove

- Todos los ataques Fuego incluyen cura FREEZE del objetivo por el constructor upstream, aunque no figure en su raw individual. El dispatcher aplica esa cura y la IA cuenta su atributo implícito.
- Regresiones Ember y beneficio Overheat actualizadas desde esta evidencia. El inventario de declaraciones raw no enumera efectos heredados/implícitos; DAT-02 requiere también esa auditoría.

### Avance MOV-02: predicción de daño fijo

- calculatePokemonDamageCore devuelve constante/nivel tras inmunidad, sin multiplicar resistencias/STAB/burn. Comparte gate de capacidades con ejecución, evitando que IA prediga una ruta no soportada.
- Regresiones de predicción/ejecución escritas; falta admisión y puntuación de selección en FirstRunRuntime y validación final. MOV-02 sigue parcial.

### Avance MOV-02 / IA-02: admisión y selección de daño fijo

- Selección y ejecución FirstRunRuntime conectadas para los cuatro perfiles exactos, con gates de capacidades/contexto. IA usa potencia efectiva cero, no el daño constante como potencia.
- Builder metadata incluida y regresiones escritas; validación ejecutada y contextos adicionales siguen pendientes. Ambas casillas permanecen abiertas.

### Avance MOV-02 / SAV-02: replay de comandos tras restore

- Añadidos casos de los cuatro ataques de daño fijo en FirstRunRuntime: dos restores, comandos reales, comparación de HP/PP/stage/turno y battle RNG.
- Son snapshots exclusivamente de test; casos escritos sin ejecución. No se marcan casillas como verificadas.

### Avance MOV-02: daño de mitad de HP

- Perfiles y cálculo conectados para Super Fang, Nature’s Madness y Ruination: floor(HP/2), mínimo uno, sin modificadores ordinarios.
- Multi Lens/contextos adicionales no habilitados; límites y RNG tienen regresiones escritas sin ejecutar.

### Avance MOV-02 / IA-02: Psywave

- Perfil canónico, ejecución y predicción conectados al RNG de batalla: el KO simulado upstream también consume la tirada de nivel aleatoria.
- Regresiones de orden RNG y restore escritas, sin ejecutar. No se declara cobertura completa ni validación de consola.

### Avance HAB-04 / HP-03: Sturdy

- Componentes de preparación/consumo/lapse y proveedor canónico con provenance implementados. Simulación no crea tag; preventEndure no lo consume.
- Actor, codec individual, comando, rutas indirectas y lapse están conectados en contextos limitados. Guardar una run con tag activo sigue rechazado; jefes y contextos adicionales siguen pendientes. Regresiones escritas sin ejecutar.

### Avance MOV-02 / IA-02: False Swipe y Hold Back

- SurviveDamageAttr canónico conectado a predicción, comando y selección; límite HP-1 aplicado después del redondeo, incluido daño cero a HP uno.
- Regresiones de HP/PP/RNG escritas; contextos amplios y validación ejecutada permanecen pendientes.

### Avance MOV-04: ignorar etapas del rival

- Chip Away/Darkest Lariat conectados a daño, precisión y selección con perfiles exactos. Se conserva la precisión y el boost ofensivo propio.
- Regresiones escritas sin ejecutar; atributos compuestos y validación final pendientes.

### Avance HAB-03 / SAV-02: replay Gallade/Sharpness

- Perfil, cálculo de potencia y proveedor exacto conectados en contextos simples. Replay con actor real Gallade y Sacred Sword escrito, compara HP/PP/ability/RNG tras dos restores.
- Caso sin ejecutar; dobles/modifiers/pasivas/supresión y verificación final siguen pendientes. No se cierra HAB-03 ni SAV-02.

### Avance SAV-01 / HP-07: Sturdy en checkpoint del jugador

- Captura usa actor v11 y restore reaplica summon tag después de reconstrucción del encuentro. Se conservan tags de todos los miembros de jugador representados.
- Enemy/trainer siguen protegidos por rechazo; no hay ampliación de waves/biomas. Regresiones de roundtrip/consumo/RNG escritas, sin ejecutar.

### Avance SAV-01 / SAV-04 / HP-07: tags enemigos y trainer

- Envelope v19, captura y restore conservan Sturdy enemigo y de miembros trainer; v18 migra sin tags nuevos. Validación rechaza slots/activo inconsistentes y booleanos inválidos.
- Regresiones escritas sin ejecutar; estas tareas continúan parciales. Persistencia de bioma/ruta, otros tags y resto de fronteras siguen pendientes.

### Avance SAV-03 / FLU-06: reconstrucción del bioma guardado

- Captura registra arena actual; resolve de checkpoint usa destino canónico sin repetir transición y sin punteros al buffer de entrada.
- Replay legacy comprueba coincidencia del bioma. Regresiones escritas; no elimina aún gates después de wave 9 ni cierra persistencia completa de ruta/campo.

### Avance SAV-03 / FLU-05: wild singles posteriores

- Captura fuerza equipo explícito después de wave 9; restauración usa bioma del checkpoint, sin replay ficticio de rewards/evoluciones. Legacy posterior sin actores rechazado.
- Segmentos boss, dobles y trainers adicionales aún no representados. Caso wave 11 escrito, pero su recorrido depende del save del jefe de wave 10 pendiente; no es evidencia ejecutada.

### Avance SAV-01 / SAV-03 / HP-02: segmentos de jefe

- Envelope v20 conserva count/index/fase de jefe individual; captura/restore conectados con validación de estructura canónica y migración histórica.
- Recorrido de test wave 10–11 actualizado, aún sin ejecutar. Segunda fase Eternatus, dobles y trainers adicionales requieren persistencia/integración propia; Classic completo permanece pendiente.

### Avance SAV-03 / IA-06: entrenador con snapshot explícito

- Capture/restore acepta parties generadas con estados resueltos fuera de wave 5 cuando el equipo jugador/EXP es explícito; verifica identidad del trainer y sus miembros contra generación.
- Replay legacy permanece restringido. Casos de ruta explícita wave 5 escritos; casos posteriores y gates ejecutados aún pendientes. No implica todos los entrenadores ni todas las reglas de combate.

### Avance TUR-02 / TUR-05: orden de campo

- Dobles actuales usan resolver de hasta cuatro actores; Trick Room invierte también empates según pinned upstream. Prioridad se aplica después, conservando orden estable.
- Casos escritos sin ejecutar. Segundo jugador, selección de sus comandos/objetivos, acciones agrupadas y forced order siguen pendientes.

### Avance HP-02 / CAM-03: status residual y segmentos

- Jefes normales singles conectan poison/toxic/burn al dispatcher existente de segmentos con preventEndure. HP/stages/status/shields/RNG se publican conjuntamente.
- Callbacks desconocidos, rangos no representables, dobles y fase final siguen protegidos. Regresiones escritas; validación ejecutada pendiente.

### Avance TUR-03 / HP-02: confusión del jefe

- Comando del jefe normal conecta autogolpe a escudos y conserva RNG actor/global, cancelación y PP. Rechazo tardío no publica mutaciones.
- Sturdy combinado con escudos, fase final y dobles siguen pendientes. Regresiones escritas, ejecución aplazada.

### Avance HAB-04 / HP-02: Sturdy con escudos

- Daño boss consume tag después de ajuste por segmentos; comando activa Sturdy canónica y registra supervivencia. Residual mantiene preventEndure; confusión usa el tag original con dispatcher.
- Casos escritos y generación realizada; ejecución pendiente. Weather, otras supervivencias/flags, dobles y fase final siguen pendientes.

### Avance CAM-03 / HP-02 / TUR-08: clima con supervivencia

- Dispatcher boss recibe requested damage y tag previo; ignoreSegments calcula escudos desde HP final. Eventos de fase y estados se publican conjuntamente.
- Regresiones de daño cero, KO siguiente y rollback escritas sin ejecutar. Resto de callbacks/campo doble y fase final siguen pendientes.

### Avance HP-05 / MOV-07: Struggle en preparación

- Cambios locales reconocen la declaración pinned y preparan un comando virtual sin sustituir el moveset persistente.
- Falta conectar la selección automática en FirstRunRuntime, completar composición de políticas y ejecutar regresiones. No hay evidencia ejecutada ni se cierra el criterio.

- Revisión adicional: chart/STAB omiten el perfil exacto typeless; regresiones 10210–10216 escritas sobre Ghost real, recoil, slots intactos y rechazo atómico. Sin ejecutar; selección automática aún pendiente.
