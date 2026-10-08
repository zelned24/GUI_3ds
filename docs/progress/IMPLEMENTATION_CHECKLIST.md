# Checklist de implementación — PokéRogue para Old 3DS XL

## Cómo medir el avance

- Referencia actual: rama `codex/pokerogue-3ds-migration`, último commit publicado `7345b78` y cambios locales posteriores sin publicar. Los resultados históricos no verifican estos cambios locales.
- Objetivo: PokéRogue jugable y fiel al snapshot upstream en Old 3DS, con ambas pantallas, progreso exportable y actualizaciones desde consola.
- Cada ID es estable para reportar avances: `MOV-07`, `GUI-03`, etc. No equivale a un movimiento/habilidad individual.
- Una casilla sin marcar puede tener código parcial; el resumen de cada área indica lo existente. Marcarla solo con integración completa y evidencia ejecutada pertinente.
- Estados para reportes: **pendiente**, **parcial**, **implementado sin verificar**, **verificado**. Registrar commit, archivos, prueba/resultado y limitaciones.
- No hay porcentaje global fiable todavía: falta medir la cobertura por ID/atributo/contexto. Contar estas tareas como pesos iguales distorsionaría el avance.
- Pins: juego `8555c08c823b856cbec4eb99ca84ea52a955836d`; assets `056a1f408f26a3be4fef243f7462cb43608c7928`; locales `23aea1cb0da5a0b15b836f3c243791cc42303`.
- Fuente de detalle: [estado](../MIGRATION_STATUS.md), [inventario anterior](POKEROGUE_3DS_REMAINING_WORK.md), `project/` y scripts actuales. El inventario anterior incluye notas históricas, no resultados vigentes.
- GUI significa interfaz **del juego**; no reconstruir el editor eliminado. El código actual usa gameplay C++ y bridge QuickJS opcional: su presencia no demuestra un port completo de la web.

## Resumen del checklist

Estos son criterios de cierre, no cantidades de ataques o habilidades pendientes. Las casillas se cierran con evidencia de su alcance exacto; las abiertas pueden tener implementación parcial.

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
| Interfaz del juego en dos pantallas | 13 |
| Assets, animación y audio | 8 |
| Guardado, continuar y exportación | 8 |
| Actualización desde la consola | 8 |
| Memoria y rendimiento Old 3DS XL | 6 |
| Validación y entrega final | 10 |
| **Total** | **128** |

Prioridad de gameplay pendiente: completar el segundo Pokémon activo del jugador y el campo de cuatro actores (**TUR-05**), checks previos y cola dinámica de acciones (**TUR-01–04**), ampliar habilidades/movimientos (**HAB / MOV**) y cerrar Eternatus con persistencia (**FLU-09 / SAV-03**). Struggle por PP agotados y guardado del campo actual de tres actores ya tienen rutas conectadas; no cubren todos los contextos. Las regresiones de replay y restauración se verifican en la suite FirstRunRuntime; consultar resultados vigentes en MIGRATION_STATUS.md.

Prioridad visual actual: integrar el catálogo completo de apariencias físicas; completar sprites femeninos normales y selección de variantes desbloqueadas; comprobar tipografía, recortes, cursor y distribución de todos los submenús. La generación/conversión de assets continúa autorizada. Compilación, tests que compilan y Azahar están aplazados por la última instrucción del usuario.

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
| HP-05 / MOV-07 | Struggle virtual conectado a selección jugador/IA, orden, locales, daño/retroceso boss y campo actual de tres actores | Segundo activo jugador, modifiers, restricciones por otros tags y validación ejecutada |
| HP-01–08 | HP/PP/status, daño/curación, EXP parcial persistida, checkpoint de derrota simultánea y residual de Poison/Toxic/Burn para ambos enemigos | Composición completa, fases tras faint/summon, otros tags/callbacks y feedback visual; falta ejecución de pruebas |
| FLU-05 / SAV-03 | Checkpoint v22: bioma, actores explícitos, jefes, RNG global, trainer resuelto y segundo enemigo del campo doble | Segundo activo jugador, fase final, decisiones pendientes y recorrido completo; casos de trainer posterior/dobles sin verificar |
| GUI-01–14 / AST-01–08 | Presentación nativa, índices, filtrado GPU nearest-neighbor para sprites nítidos y assets convertidos | Todas las pantallas, HUD HP/PP/EXP, animación/audio, controles y comparación visual |
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
  - [x] Filtrar starters canónicos por generación con Y/táctil y compartir catálogo entre navegación/dibujo. Cuadrícula de 18 iconos, equipo y botones inferiores con zonas táctiles separadas.
  - [ ] Perfil P3CANDY9 añade apariencia observada/capturada (DexAttr 1/2/16/32/64), lee v1–v8 sin inferir shiny; casos de roundtrip y migración escritos. Actor y registro de observado/capturado conectados con estado appearanceResolved explícito; Codec de actor pokemon=e conserva apariencia resuelta y lee antiguos como desconocidos; casos escritos. Resolver de apariencia predeterminada reproduce `GameData.getSpeciesDefaultDexAttrProps` (shiny capturado y variante más alta; metadata antigua desconocida), con casos escritos. Pendientes integración de todos los snapshots, generación, selección, assets y ejecución. Envelope v27 conserva apariencia del enemigo principal y equipo de entrenador, migra v26 como desconocida y exige snapshot del jugador cuando tiene apariencia resuelta. Captura/restauración y casos de migración conectados; ejecución pendiente. Generación shiny, selección y referencias de assets todavía pendientes.
  - [ ] Filtro táctil Todos/Capturados/Sin capturar combinado con generación/tipo implementado; pruebas escritas, ejecución y distribución visual pendientes.
  - [ ] Costes canónicos y reducciones guardadas dibujados en cada celda; casos de fracciones/buffer escritos, ejecución y comparación visual pendientes.
  - [ ] Mostrar el catálogo elegible con estados capturado, visto, desconocido y variantes shiny según el progreso; El selector ya permite recorrer todos los starters elegibles y proyecta capturado/visto/desconocido desde el perfil existente; Registro de encuentros salvajes y equipos de entrenadores conectado al perfil existente, con validación previa del lote; pruebas de aislamiento y save/restore escritas, ejecución pendiente. Eternamax sincroniza la forma del actor y registra la observación; evolución registra visto y actualiza metadata de captura solo cuando la raíz ya estaba capturada, sin recompensas; pendientes ejecución, otros cambios de forma, estadísticas seenCount y variantes shiny persistentes. Comparación visual pendiente.
  - [x] Filtrar por tipos canónicos disponibles con X/táctil, combinar con generación y resolver etiquetas originales españolas; catálogo vacío/tipo desconocido y combinación de filtros probados.
  - [x] Rasterizar el TTF pinned en monocromo sin eliminar trazos de alfa bajo; conservar avances/baselines y ampliar el bitmap cuando el hinting lo requiere. Tests de mapas, métricas, bounds y conversión repetida.
  - [x] Evitar recargas por frame de páginas de iconos; liberar el conjunto del selector tras sincronización al salir. Prueba host de todas las páginas en dos frames.
  - [ ] Confirmar en Azahar/Old 3DS las nuevas fuentes, patrón nativo del fondo, cursor y coste de residencia de iconos (hasta 8 MiB del snapshot actual).
  - [ ] Completar filtros restantes de upstream (coste/atributos disponibles), decisiones de starters y verificación visual de fuentes.
- [ ] **GUI-05.** Completar battle, cambio/objetivos, captura y mensajes/feedback de fallo.
- [ ] **GUI-06.** Completar mapa/bioma, rewards, tienda, party/items y decisiones de aprendizaje/evolución.
- [ ] **GUI-07.** Completar settings, idioma, audio, exportación y actualización de contenido.
  - [ ] Submenú global basado en `MenuUiHandler` pinned y captura: nueve opciones localizadas en panel original, cursor compartido y sombreado superior. X o táctil en pie del título lo abren; D-pad/táctil recorren todas las opciones en 320×240 sin desplazamiento (filas de 20 px). Ajustes y retorno anidado conectados; las demás opciones abren destinos informativos que detallan la integración pendiente, sin fabricar datos ni sesiones. Casos C++ escritos, sin ejecutar; compilación y comparación visual pendientes.
  - [ ] Ajustes: categorías y preferencias realmente aplicadas/persistidas.
  - [ ] Logros: catálogo, desbloqueos, detalles y progreso persistente.
  - [ ] Estadísticas: contadores reales del perfil, sin valores ficticios.
  - [ ] Lista de Huevos: inventario real, tiempos/progreso y detalle.
  - [ ] Gacha de Huevos: vouchers, selección y resultados deterministas del motor.
  - [ ] Pokédex: capturado/visto/desconocido, formas y shiny del perfil. Vista C++ conectada al perfil real y todas las especies canónicas; iconos originales 1× con estados, nombre desconocido oculto, páginas L/R/táctil y retorno al submenú. Casos de navegación escritos, sin ejecutar. Filtros de generación y capturado/visto/desconocido añadidos mediante X/Y o táctil; información conocida (tipos y estadísticas base canónicas) en pantalla superior. Filtros restantes, detalles completos, formas, shiny y verificación visual pendientes.
  - [ ] Gestionar datos: importar/exportar/respaldar perfil y partidas con confirmación y errores claros.
  - [ ] Comunidad: enlaces/servicios compatibles con 3DS y disponibilidad explícita.
  - [ ] Cerrar sesión: solo conectado a una sesión real; no simular autenticación.
  - [ ] Etiquetas `es-ES/menu-ui-handler.json` añadidas al generador y tabla C++ regenerada sin compilar. Preparación de glifos y validación visual pendientes.

- [ ] **GUI-08.** Completar win/lose/summary y recuperación de errores de carga.
- [ ] **GUI-09.** Completar D-pad/A/B/X/Y/L/R/Start/Select y táctil resistivo con foco coherente.
- [x] **GUI-13.** Convertir alpha antialias de la fuente A4 a alpha binario, conservar métricas, alinear coordenadas de texto a píxeles y preparar un perfil Azahar Old 3DS sin suavizado; pruebas de conversión/determinismo/corrupción ejecutadas.
- [ ] **GUI-14.** Verificar nitidez, tamaños y alineación de letras/sprites en todas las pantallas mediante Azahar y Old 3DS; contrastar capturas con la referencia web.

- [ ] **GUI-10.** Completar locales, glyphs, fallback y texto largo; retirar strings fijas donde exista localización.
- [ ] **GUI-11.** Mantener UI → comando → evento → binding; sin reglas ni especies hardcodeadas.
- [ ] **GUI-12.** Auditar rol QuickJS/Phaser→Citro2D y bundle: bridge no equivale a ejecutar todo PokéRogue upstream.

## 11. Assets, animación y audio

### Estado actual

Hay catálogos de atlases/t3x y presenter; el inventario histórico debe cotejarse con archivos físicos actuales.

### Pendientes y criterios de cierre

- [ ] **AST-01.** Auditar todos los PNG/t3x físicos, índices, hashes y provenance, sin reducir a especies de ejemplo.
- [ ] **AST-02.** Completar front/back, forms, shiny, variantes y resolución dinámica por ID.
  - [ ] Resolver de starters aplica la apariencia predeterminada desde caughtAppearanceAttr del perfil (shiny capturado/variante superior según helper upstream), sin nuevos sorteos. Registros legacy sin metadata mantienen apariencia desconocida. Guard estático JS PASS; casos C++ normal/epic/legacy y estabilidad PID/Tera escritos, sin ejecutar. Selección manual de apariencia y validación nativa pendientes.
  - [ ] Materialización completa de apariencias activa: 7570 identidades derivadas del árbol/masterlist pinned, incluyendo formas, género y front/back. Generador integrado antes del staging completo; reporte determinista con SHA del masterlist y generador, ausencias upstream separadas de INVALID_IMPORT/NOT_YET_SUPPORTED_BY_IMPORTER. Cinco tests Python PASS. Resultado final, conversión del catálogo ampliado e índice pendientes.
  - [ ] Importer/modelo preservan `genderDiffs` desde el constructor de especie upstream como booleano o desconocido; valores declarados no interpretables fallan claramente. Prueba JS con tres especies reales del pin PASS. Reimportación de especies terminada, dos imports con hash idéntico `0f3f4fc91c6b458f14b1b4f85c2d60923d24d1e1bfcd4155af3ddf08eff7d140`; tabla C++ generada, sin compilar. Importer de formas reutiliza el parser booleano y conserva ausencias como desconocidas: casos sobre los 609 fragmentos reales añadidos; 201 no declaran el campo. Parser de exclusiones de getBaseSpriteKey añadido y probado desde enum/fuente pinned, con provenance; Metadata de 1084 especies/609 formas y 10 exclusiones publicada, hash canónico `c797658f2c13c494bd83eeb1b6ea8177e9c25b73d09abe5d1c653b7655dadf2b`; generación C++ repetida byte idéntica SHA `843be89458a9d82e8be128030fb5158f16daff5d57f0a21f6190493851cea12d`. Renderer consulta tablas; compilación y prueba de consumo C++ pendientes.
  - [ ] Materializador `scripts/pokemon_variant_palette.py` reproduce paletas RGB exactas del shader pinned y resuelve atlas shiny/dedicados desde el masterlist; preserva alpha y SHA de fuentes. Cuatro tests Python PASS con especies reales, front/back y determinismo. Staging `--appearances` conectado al importador existente y preservado por la preparación completa del catálogo, verifica archivos derivados y SHA de cada fuente pinned; tres atlas reales incluidos en el inventario. Conversión física terminada: 2905 atlas, 2921 páginas `.t3x`, incluidos tres atlas shiny reales; inventario SHA `8f42de9f7806a3b5ad9c75913bb3115bd2f31bd4281fc1f94e299ca5c1b7f44e`. Índice C++ generado verificando SHA, identidad, pins y páginas/rutas del runtime; pruebas JS aisladas PASS y regeneración real determinista SHA `3c995e3490d2315c12f714f3005a06b296616a26db5e34cda7c247e3a0585f44`. Renderer conectado al índice mediante apariencia del actor, facing y reglas visuales importadas de género/forma; faltantes emiten diagnóstico sin sustituir shiny/female por normal/male. Guard JS estático PASS; casos nativos del índice escritos, sin ejecutar. Reglas de formas publicadas; imports deterministas y generación C++ byte idéntica comprobados. Faltan catálogo shiny/female completo, selector y verificación C++/GPU.
- [ ] **AST-03.** Completar páginas de atlas grandes, offsets/origen/recorte y cambios de frame.
  - [ ] Reemplazo de atlas retira texturas mediante Renderer2D hasta SYNCDRAW, conservando los presenters separados existentes. Invalidación por QuickJS y salida del runtime usa el mismo retiro diferido. Guard estático JS PASS; validación de GPU nativa pendiente.
- [ ] **AST-04.** Completar animación de idle/ataque/daño/faint/captura/cambio/evolución.
- [ ] **AST-05.** Completar fondos de bioma, plataformas, trainers y texturas/iconos de interfaz.
  - [ ] Trainers retiran texturas con el renderer al reemplazarse; una clave sin mapping limpia el sprite anterior. Guard estático JS PASS; regresión nativa y visual pendientes.
- [ ] **AST-06.** Completar efectos de movimientos/habilidades y referencias a animaciones.
- [ ] **AST-07.** Completar música, cries, SFX, conversión, streaming y volumen.
- [ ] **AST-08.** Validar empaquetado/carga/liberación y capturas comparables con web en ambas pantallas.
  - [x] Retirar del PokemonSpriteResolver heredado seis registros ficticios y sus claims de verificación. Rechaza fixtures, metadata incompleta y `physicalVerified` declarado por el llamador; guard JS PASS.
  - [ ] Conectar consumidores heredados del resolver al índice físico real. CanonicalModels y PokerogueAdapter ya retiran fallbacks de rutas, frame cero y claims de shiny/género: campos ausentes quedan null, metadata explícita se conserva; guard JS PASS. Reimportación de esta corrección activa; otros consumidores y verificación completa pendientes.
  - [x] Verificación de archivos de presentación actuales: cuatro fuentes con alpha binario y métricas/hashes físicos, ventanas/tipos/HUD/items/trainers/cinemática. `verify_presentation_media.py` PASS y `pixel_font_tests.py` 12 PASS, sin compilar ni abrir Azahar.
  - [ ] Comparación visual y prueba de carga/liberación C++ de los últimos cambios, incluida selección de apariencia; no cubiertas por la verificación de archivos.

## 12. Guardado, continuar y exportación

### Estado actual

NativeRunSave/runtime v22: RNG global, segundo enemigo, escudos, objetivo y máscara de EXP parcial; codec de actor v11 cuando hay tag Sturdy, v10 para confusión y compatibilidad anterior. Hay journals y bundles. Segundo jugador activo, decisiones pendientes y cobertura completa de estados aún faltan; regresiones sin ejecutar.

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

Tests y compilación están autorizados. Solo se marcan como verificadas las pruebas ejecutadas; Azahar y hardware requieren evidencia propia.

### Pendientes y criterios de cierre

- [x] **VAL-01.** Ejecutar npm test y registrar todos los fallos conocidos/nuevos.
- [x] **VAL-02.** Ejecutar npm run native-parity y comprobar cobertura real de harnesses.
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
| HP-05 / MOV-07 / TUR-05 | Parcial | beb74ef → a8d8f1f | Inspección estática; regresiones escritas sin ejecutar | Struggle: segundo jugador, tags, modifiers y validación |
| SAV-02 | Parcial | 5e7b5e5 | Codec v21 inspeccionado; regresiones sin ejecutar | Todas las fronteras de RNG y compatibilidad histórica |
| SAV-03 / TUR-05 | Parcial | f8ee639 | Codec v22 y capture/restore inspeccionados; regresiones sin ejecutar | Campo de cuatro actores, decisiones y fase final |
| SAV-03 / HP-03 / HP-06 | Parcial | 7c64484 | Revisión estática de EXP parcial y derrota simultánea | Fases tras faint y ejecución de regresiones |
| CAM-03 / HP-07 / TUR-05 | Parcial | ddcb4ff | Residual doble y dos colas de orden inspeccionados | Callbacks, faint/summon, segundo jugador y pruebas |
| GUI-01 / GUI-14 | Parcial | Arranque del renderer | Harness C++ con siete fallos de inicialización, cleanup/retry y pipeline de fuente requerida; no hay sustitución silenciosa por font del sistema | Comparación de todas las pantallas con web en Azahar y Old 3DS física |
| GUI-01 / GUI-05 / GUI-14 | Parcial | Ancho real del HUD y cursores | Renderer host devuelve ancho ajustado; HUD reserva nivel/género/HP y cursores usan escala dibujada; build ARM | Tamaños mínimos, glifos y comparación visual conjunta |
| GUI-02 / GUI-14 | Parcial | Tipos canónicos del HUD | Prueba host: 1084 especies y 609 formas; rechazo de formas cruzadas, ausencia de segundo tipo y límites de texto | Tipos temporales, Teracristalización y comparación visual |
| GUI-07 | Parcial | Cambio de marcos nativos | Cinco IDs/símbolos upstream y PNG convertidos; selector aplica/persiste marco y táctil con confirmación; diario v2 y migración v1 probados en host | Interacción SD real, otros ajustes, audio, idioma, exportación y OTA |
| GUI-01–05, 08–09 | Parcial | 760b65b y corrección de carga | Navegación C++ host y guards de dispatch; build ARM. Continuar/Cargar releen SD y muestran errores específicos | Menús completos, comparación visual conjunta en Azahar, interacción SD y Old 3DS |
| TUR-07 / FLU-09 | Parcial | b18388c | FirstRunRuntime: 50/50 PASS; huida conserva RNG/avance sin EXP y hay casos de wave 200 | Trampas, callbacks, cuatro actores y recorrido Classic completo en Azahar |
| VAL-01–02 | Verificado en su alcance | 760b65b y corrección de carga | npm test: 34 suites PASS; native-parity: 126/126 PASS | No demuestra VAL-03–05: ejecución final, reproducibilidad de 3DSX y paridad completa con upstream siguen abiertas |

El registro describe implementación parcial y avances verificados. No se calcula porcentaje contando casillas de distinto alcance.

Para cada entrega actualizar esta tabla con IDs, commit, alcance conectado, exclusiones y resultado ejecutado. Las notas siguientes son históricas; la tabla consolidada y el código de referencia prevalecen cuando una limitación antigua ya fue abordada.

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

### Avance HP-05 / MOV-07: selección automática de Struggle

- Singles conectan PP agotados → ID virtual → orden → checks previos → comando → recoil → feedback. Jefes usan dispatcher también para retroceso; moveset persistente intacto.
- Replay de ambos actores con PP cero y regresiones de usuario boss escritos sin ejecutar. Dobles, modifiers, tags que bloquean movimientos y contextos de habilidades pendientes.

### Avance HP-05 / TUR-05: Struggle en campo de tres actores

- PP agotados de jugador/ambos enemigos usan acción virtual; objetivo aleatorio pinned se prepara antes de IA, prioridad usa Struggle y slots persistentes se conservan.
- Regresiones de RNG/objetivo y campo real escritas sin ejecutar. Segundo activo jugador, todos los checks de status dobles, persistencia y contextos pendientes impiden cerrar dobles completo.

### Avance SAV-02 / HP-02: continuidad del RNG global

- Envelope v21 y runtime capture/restore conservan Alea global para boosts de escudos. Codec usa fracciones enteras exactas, rechaza estados inválidos y mantiene decodificación legacy con historial explícitamente ausente.
- Regresiones de roundtrip/extracciones/bytes/migración y checkpoint de jefe escritas sin ejecutar. Persistencia doble sigue pendiente; no se cierra continuidad completa ni compatibilidad/paridad de runs legacy.

### Avance SAV-03 / TUR-05: persistencia del campo doble actual

- Envelope v22 conserva segundo enemigo, estado mutable, escudos, objetivo y máscara de EXP parcial. Capture/codec/restore/recapture conectados con validación del encuentro regenerado y snapshots explícitos de jugador.
- Regresiones de encuentro real/replay/rechazo y migración v21 escritas sin ejecutar. Segundo jugador activo, derrotas parciales/EXP, estados/contextos completos y evidencia final siguen pendientes.

### Avance SAV-03 / HP-03 / HP-06: derrotas parciales y simultáneas

- Checkpoints de derrota permiten ambos campos sin HP; captura de derrota usa actores explícitos para conservar EXP, sin replay ficticio de premios trainer.
- Regresión de derrota parcial real → EXP → checkpoint → continuar sin repetir premio, y fixtures de pérdida simultánea escritas sin ejecutar. Resto de fases post-faint y validación final pendientes.

### Avance CAM-03 / HP-07 / TUR-05: status residual doble

- Secuencia de colas pinned conectada a fin de turno; Poison/Toxic/Burn usan políticas exactas y dispatcher propio de cada boss. Velocidad usa clima del snapshot posterior a weather.
- Regresiones de orden y segundo enemigo Toxic → tick/HP → checkpoint → restore escritas sin ejecutar. Reacciones posteriores a faint/summon y contextos completos siguen pendientes.

### Avance TUR-03 / TUR-05 / HP-07: checks previos de un objetivo

- Sueño/congelación/parálisis conectados al dispatcher de acciones de un objetivo en el campo doble actual, con el mismo resolver de singles. Ataques de área mantienen gate para no repetir checks/RNG.
- Regresión de sueño con Struggle virtual y replay de checkpoint escrita sin ejecutar. Confusión doble, segundo jugador, callbacks y cobertura completa pendientes.

### Avance TUR-03 / TUR-05 / MOV-09 / HP-05: acción de área compartida

- Estado transitorio compartido conserva checks/cancelación/ID entre objetivos. Último PP no convierte el segundo efecto en Struggle. Cambios de etapas status en dobles se admiten solo con perfiles neutrales y callbacks resueltos en todo el campo vivo.
- Confusión se conecta a acciones de un objetivo y al dispatcher compartido con gates de clima/modifiers/habilidades. Regresiones Growl/sueño/confusión/último PP escritas sin ejecutar. Daño de área, reacciones de etapas y segundo jugador pendientes.

### Avance TUR-05 / MOV-03 / HP-05: daño de área always-hit

- Swift canónico atraviesa acción compartida, multiplicador de objetivos vivos, daño a ambos enemigos y PP único; política también disponible en predicción. No se habilitan efectos adicionales ni área con checks aleatorios de precisión.
- Regresiones de políticas/daño/checkpoint escritas sin ejecutar. Campo de cuatro actores, batching general de precisión y callbacks pendientes.

### Avance TUR-05 / MOV-04: lote de precisión antes de daño

- Tipo/inmunidad/bloqueo/precisión se preparan para todos los objetivos vivos antes de críticos/daño. Resultados ligados a move/target se consumen sin reroll. Petal Blizzard plano se conecta al dispatcher del jugador.
- Regresión de hit/miss mixto y orden RNG escrita sin ejecutar. ALL_NEAR_OTHERS enemigo, segundo jugador, protecciones y efectos adicionales pendientes.

### Avance TUR-05 / IA-02 / MOV-03: daño enemigo a aliado

- Dispatcher plano de área compartido resuelve oponentes → aliado, precisión previa, daño/PP y multiplicidad viva. Valoración de IA considera aliado y predicción de KO utiliza factor de área.
- Regresión Petal Blizzard enemigo → jugador + aliado escrita sin ejecutar. Segundo activo jugador, effects/flags/callbacks completos y verificación pendientes.

### Avance TUR-05 / MOV-09: cambios de estadísticas de área enemigos

- Growl enemigo usa acción compartida también en respuestas a cambio/captura; aliado queda fuera de ALL_NEAR_ENEMIES. Gates de perfiles neutrales conservados.
- Regresión de comando real/último PP/HP escrita sin ejecutar. Campo de cuatro actores y reacciones de etapas pendientes.

### Avance HAB-05 / MOV-09 / TUR-05: etapas locales en dobles

- Multiplicadores Simple/Contrary y protección propia se componen por objetivo en ataques status dobles, reutilizando el resolver de singles. Reflexión/copia/reacciones posteriores permanecen gated en todo el campo vivo.
- Regresiones de perfiles canónicos en campo real escritas sin ejecutar; callbacks de campo, pasivas/supresión y validación final pendientes.

### Avance HAB-05 / MOV-09: reacciones locales de un objetivo

- Comandos status de un objetivo en dobles admiten reacciones de etapas canónicas como Defiant/Competitive mediante resolver existente. Reacciones de área, reflexión/copia y callbacks desconocidos siguen gated.
- Regresiones Screech → bajada → reacción escritas sin ejecutar; no se cierra cobertura completa de habilidades.

### Avance HAB-05 / MOV-09: Mirror Armor de un objetivo

- Reflexión de un objetivo admite dobles; fase reflejada compone protección propia de bajadas antes de mutar. Simple/Contrary/Clear Body y ausencia de reflexión recursiva tienen regresiones escritas sin ejecutar.
- Copia, reflexión de área, pasivas/supresión y validación final siguen pendientes.

### Avance TUR-08 / MOV-04: identidad de hit checks

- Resultados preparados se ligan a atacante, movimiento y objetivo; se rechaza reutilización por otro atacante sin consumir RNG. Regresión escrita sin ejecutar; cola general y campo de cuatro actores pendientes.

### Avance TUR-01 / TUR-02 / TUR-05: cola dinámica de acciones

- Runtime doble reordena acciones pendientes por velocidad/clima/Trick Room y prioridad en cada extracción, conservando su orden restante y movimientos elegidos.
- Regresiones del resolver escritas sin ejecutar; timing modifiers, prioridad por abilities/items, segundo activo jugador y prueba end-to-end con cambios de velocidad pendientes.

### Avance TUR-02: regresión de turno dinámico

- Caso completo Scary Face → cambio de velocidad → dos acciones Swift pendientes compara HP/PP/etapas con comandos ordenados y exige diferenciarse del orden inicial. Setup de velocidades/slots exclusivo de test.
- Escrito sin ejecutar; evidencia upstream ejecutada, timing/priority modifiers y segundo jugador pendientes.

### Avance HAB-05 / HP-04 / CAM-03: Poison Heal doble

- Capacidad de acción reconoce la declaración exacta de Poison Heal; curación local post-turno en dobles conecta bloqueo de residual y HP sin eliminar status.
- Regresión de segundo enemigo real con capacidad canónica escrita sin ejecutar; pasivas/supresión, callbacks de curación y campo completo pendientes.

| GUI-01 / GUI-10 / AST-05 | Parcial | Etiquetas originales de tipos | Atlas es-ES pinned, 20 frames, bounds/hashes físicos y harness renderer; coverage de tipos canónicos | Idiomas, iconos compactos upstream del HUD y comparación visual conjunta |

| GUI-02 / AST-05 / AST-08 | Parcial | Tipos compactos del HUD | Seis atlas upstream, 120 frames, trim/canvas nativos y escala 1×; validación de datos/renderer/archivos físicos | Estado/captura originales, escala de paneles/barras, comparación visual y rendimiento real |

| GUI-02 / GUI-05 / AST-05 / AST-08 | Parcial | Estado y owned originales | Atlas es-ES de ocho estados preservados, seis proyectados, PNG owned 7×7; renderer host prueba escala/errores/liberación | Pokérus/faint, demás indicadores, comparación de ambas pantallas y hardware |

| GUI-02 / GUI-05 / GUI-14 / AST-05 | Parcial | Paneles y barras nativos | Paneles 1×, barras originales recortadas, columnas acotadas, separación de feedback, override de dimensiones pinned; tests renderer/import | Animación temporal, labels/dígitos originales y comparación visual/Old 3DS |

| GUI-02 / GUI-10 / GUI-14 / AST-05 | Parcial | Dígitos y labels del HUD | Números originales 8×8 y cuatro etiquetas es-ES, scale 1×, source/provenance y tests renderer/import | Nivel capped, nombres largos, animación y comparación visual completa |

| GUI-02 / GUI-10 / GUI-14 | Parcial | Abreviación de nombres con fuente constante | Semántica updateNameText, recorte UTF-8 seguro, medición C2D en buffer independiente; regresiones de casos Unicode y arranque | Métricas de la fuente real, perfil CPU y comparación visual completa |

| GUI-02 / GUI-14 | Parcial | Color de nivel capped Classic | Runtime expone política EXP existente; HUD selecciona numbers_red; tests de umbral/enemigo/no resuelto y fórmula en 200 waves | Cambios durante partida y comparación visual/Old 3DS; otros modos |

### Nitidez restante: revisión de escalas y formatos

1. [x] Eliminar reducción fraccionaria automática de Pokémon por altura. Canvas común, adaptación nearest offline explícita y dibujo automático 1×/2×; selección conserva proporciones. Verificados los 2902 atlas y 189096 frames desde las fuentes pinned, incluido el lector C++.
   - [ ] Comparar la composición de Pokémon pequeños/grandes y formas en selección, combate simple/doble y animación mediante Azahar/Old 3DS. La prueba de datos no demuestra fidelidad visual final.
2. [x] Generar y conectar rasters de fuente 8/10/12/16 con transform efectivo entero, alpha binario, medición y cursor coherentes.
   - [ ] Comprobar legibilidad/composición de todas las pantallas y paginación de mensajes en Azahar/Old 3DS.
   - [x] Paginar el banner de progreso y el panel de combate activo con anchos medidos, fuente nativa y límites de líneas/UTF-8. SELECT navega; en combate también funciona el área táctil del pie, separada de los comandos. Pruebas de layout y renderer ejecutadas.
   - [ ] Extender el tratamiento de texto largo a setup, recompensas, decisiones y ajustes; verificar eventos repetidos, navegación conjunta y capturas. Texto progresivo/audio/prompts upstream aún pendientes.
     - [x] Conectar cajas medidas de dos líneas para habilidades en setup y tres líneas para nombres de recompensas. Fuente nativa fija, sin dibujo parcial si excede la caja; casos restantes usan fitted y requieren detalle completo posterior.
     - [x] Usar etiquetas originales es-ES de tipos a 1× en setup, resueltas desde especie/forma canónica.
     - [x] Conectar cajas de dos líneas a confirmación de starters/evolución y tres líneas a movimientos reemplazables, con cursor alineado al primer renglón. Acotar al panel los mensajes de error de setup/formas.
     - [x] Dibujar el logo original de título a 2× (300×66) centrado; eliminar la escala anterior de 1,8×. Comparación visual pendiente.
     - [x] Dibujar iconos de recompensas y Poké Balls a 32×32 (1×); validar canvas/recortes de los 528 frames de objetos y unificar dibujo/táctil de las cinco filas de Poké Balls sin solapamientos.
   - [x] Compactar las cuatro sheets A4 conservando glifos/métricas; 2 MiB → 288 KiB.
   - [x] Generar márgenes/altura de tinta de mayúsculas desde BCFNT pinned; descontar margen transparente al dibujar texto y centrar cursores con altura visible. Pruebas de offsets, UTF-8 y métricas ejecutadas; el verificador compara A4 físico, provenance y header C++; capturas pendientes.
   - [ ] Perfilar el presupuesto total y validar las fuentes compactadas en Azahar/Old 3DS.
3. [ ] Evaluar RGBA5551/RGBA4 por clase de asset con comparación de colores/alpha y memoria residente. Pokémon ya usa RGBA4, fuentes A4 y presentación RGBA8; no dar ETC1A4 por visualmente equivalente sin comparación.
4. [ ] Medir CPU/GPU/memoria en Old 3DS: nearest y scale 1 no prueban un incremento de FPS.
5. [ ] Portar la animación EXP por tramos de nivel de `src/ui/battle-info/player-battle-info.ts` (pin del juego): base 1650ms, Sine.easeIn, multiplicadores por nivel/ganancia/configuración y pausa tras subir nivel. El HUD actual avanza por frame; distinguir miembros de igual especie, continuar/reiniciar y cambios de actor. Conectar eventos temporales, probar cadencias distintas y comparar captura; no reemplazarlo con un lerp genérico.
   - [x] Conectar reloj visual libctru de ticks transcurridos: retirar el supuesto de 60 frames por segundo en los timestamps de sprites/intro. Harness comprueba cadencias de 15/30/60 con error máximo de truncamiento de 1ms; simulación/RNG no consumen este reloj.
   - [x] Identificar la caché EXP por pokemonId, tratar cero como un valor válido y reiniciarla al volver al título; prueba C++ host de dos actores de igual especie, EXP cero y reinicio pasó.
   - [ ] Integrar los cálculos EXP temporales preparados en BattleHudGeometry; el HUD aún usa interpolación por frame.
   - [x] Retirar GPU_LINEAR de la cinemática inicial y comprobar GPU_NEAREST; escala fraccionaria/crossfade y fidelidad de vídeo siguen pendientes.

| GUI-01 / GUI-10 / GUI-14 | Parcial | Raster nativo de letras | Cuatro fuentes pinned, compensación Citro2D, anchos/cursor reales, determinismo y gates host/ARM | Comparación conjunta, paginación y presupuesto en Old 3DS |

### Catálogo de apariencias: estado observado

- [x] Materializar 7.569 apariencias desde las revisiones fijadas, conservando hashes y procedencia.
- [x] Identificar el único rechazo del catálogo: Bouffalant (`626`), frente, variante 1; el archivo upstream contiene el color inválido `9e655cx`. No se sustituyó por un color inventado.
- [x] Reproducir la regla upstream `src/utils/color-utils.ts::rgbHexToRgba`: hex inválido se convierte en negro; primera coincidencia del shader conserva prioridad. Test Python dedicado y caso real de Bouffalant pasan. Regeneración del catálogo pendiente de terminar.
- [ ] Convertir e integrar las 7.570 apariencias shiny en el catálogo físico `.t3x`; regeneración en curso tras corregir Bouffalant. El índice vigente contiene 210 apariencias femeninas normales; las tres shiny convertidas inicialmente ya no forman parte del inventario normal vigente.
- [ ] Verificar visualmente todas las rutas de selección de apariencia en Azahar y comprobar recursos en Old 3DS. Compilación y lanzamiento aplazados por instrucción del usuario.

La búsqueda del índice generado usa búsqueda binaria sobre identidades ordenadas; esto no constituye una medición de rendimiento en hardware.

### Sprites femeninos normales

- [x] Convertir y verificar hashes del catálogo normal: 3.112 atlas, 3.129 páginas `.t3x`, 61.629.673 bytes en disco; inventario SHA-256 `4d9f568d6df2c6f89e66d821de9a20b20c9f526d37f058089245e505e919cac7`. Es tamaño de catálogo, no memoria residente.

- [ ] Integración completa: el staging enumera los archivos reales `images/pokemon/female` y `images/pokemon/back/female` en el pin de assets; conserva hashes y el índice distingue normal/shiny. Test JS de identidad exacta pasa. Conversión física terminada: 210 apariencias femeninas normales en el índice vigente; carga y dibujo nativo sin verificar.

### Gestionar datos: exportación desde el menú

- [ ] Verificación completa de exportación: el submenú emite `ExportProgress`; `main.cpp` ejecuta `NativeProgressStore::exportBundle` sobre la pareja guardada de run/perfil y muestra el resultado. No guarda la vista de título como partida. Prueba nativa de navegación añadida, pendiente de ejecución por restricción de compilación. Importación desde este submenú añadida: confirmación con opción No inicial, izquierda/derecha y A/B o botones táctiles visibles, lectura del bundle, validación mediante replay antes del commit y recarga del progreso. Pruebas nativas de cancelar/confirmar escritas pero no ejecutadas; integración visual y almacenamiento completos siguen sin verificar.

### Texto y errores de importación

- [x] Verificar cobertura de glifos especiales en las cuatro fuentes físicas para literales de interfaz y nombres canónicos de especies, formas, movimientos, habilidades e items; `ui_font_coverage_tests.py` pasa. Cursor de texto de respaldo usa un glifo disponible; cursor original mantiene su asset.
- [x] Corregir cadenas UTF-8 dañadas del presenter del menú y textos de `main.cpp`; guard JS estático pasa. Esto no demuestra dibujo correcto de todos los glifos.
- [ ] Verificar que una importación rechazada recargue la autoridad del guardado local: ruta de recuperación escrita, ejecución nativa pendiente.

### Indicadores shiny del selector

- [x] Importar y convertir tres frames originales, tints de `getVariantTint` e IDs de `VariantTier` con el parser de enums existente; tres tests Python pasan (hashes upstream/físicos, regeneración idéntica, guard de binding).

- [ ] Verificación visual completa: atlas original de tres indicadores convertido y generado, colores importados de `getVariantTint`; cuadrícula conectada al nivel shiny mayor acreditado por `caughtAppearanceAttr`. No infiere desbloqueos de perfiles legacy sin metadata. Dibujo 1:1 y `GPU_NEAREST`; carga lazy con un intento por sesión. Pendiente de compilación y Azahar.
