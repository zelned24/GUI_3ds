# Checklist de implementación — PokéRogue para Old 3DS XL

## Cómo medir el avance

- Referencia actual: rama `codex/pokerogue-3ds-migration`, último commit de implementación publicado confirmado `0e6f187`; conversión física ampliada terminada: 10682 atlas y 10746 páginas `.t3x`. Validación nativa y visual pendiente. Los resultados históricos no verifican estos cambios locales.
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

Prioridad visual actual: validar en runtime el catálogo completo convertido y su selección de apariencias desbloqueadas; comprobar tipografía, recortes, cursor y distribución de todos los submenús. Fondos de batalla/título y bases estáticas/animadas preparados offline con nearest y dibujados 1:1; quedan validación GPU de esos rasters y composición del entrenador; intro completa paginada implementada sin validación nativa. La generación/conversión de assets continúa autorizada. Compilación, tests que compilan y Azahar están aplazados por la última instrucción del usuario.

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

### Presentación: evidencia vigente y siguiente integración

1. **Implementado sin ejecución nativa:** submenu de nueve opciones, geometría compartida de filas/confirmaciones/carga, nombres de movimientos hasta dos líneas, barras PP enteras y ajustes HP/EXP persistidos en preferencias v4.
2. **Implementado sin prueba GPU:** iconos de apariencia en equipo/party/captura, cache acotada de seis páginas compactas y retiro sincronizado; trainers/items rechazan imágenes inválidas y recuerdan fallos de carga.
3. **Pendiente de integrar:** validación nativa del selector de naturaleza, selector de teratipo y aparición detallada; selección de habilidad conectada sin validación nativa, iconos de apariencia del grid/Pokédex, funciones completas de logros/huevos/gacha/comunidad/sesión, audio y animaciones ligadas a todas las fases.
4. **Pendiente de verificar:** todos los harness C++ recientes, fuente/cursor/recortes y cada menú en Azahar; consumo de memoria, latencia y rendimiento en Old 3DS física. Continúa la prohibición de compilar/abrir Azahar.
5. **No cerrado:** GUI-01–12/14 y AST-01–08 conservan su alcance completo; ningún guard estático sustituye su aceptación visual o funcional.

Comprobaciones sin compilación repetidas sobre los archivos actuales:

- [x] Cobertura física de glifos UI y alpha binaria de fuentes: `python test/ui_font_coverage_tests.py`, 2 tests PASS. Solo assets; no prueba lectura/dibujo en consola.
- [x] Ambos índices de iconos regenerados dos veces y comparados byte a byte con los publicados: `python test/appearance_icon_generation_tests.py` PASS. No convierte texturas ni ejecuta C++.

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
  - [ ] Catálogo ampliado: 7570 identidades derivadas del árbol/masterlist pinned, incluyendo formas, género y front/back. Generador integrado antes del staging completo; reporte determinista con SHA del masterlist y generador, ausencias upstream separadas de INVALID_IMPORT/NOT_YET_SUPPORTED_BY_IMPORTER. Materialización final exit 0: 7570 materializadas, cero ausencias y cero unsupported. Conversión e índice físico ampliados verificados; carga/dibujo nativo pendientes.
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
- [ ] Convertir e integrar las 7.570 apariencias shiny en el catálogo físico `.t3x`; materialización, staging, conversión e índice físico completos; carga/dibujo nativo pendientes. El índice vigente contiene 210 apariencias femeninas normales; las tres shiny convertidas inicialmente ya no forman parte del inventario normal vigente.
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

### Controles del catálogo completo y retorno de submenús

- [ ] Catálogo shiny completo integrado: el staging exige reporte final, conteos completos, cero registros inválidos/no soportados, identidad única y hash PNG coincidente con el registro materializado, hash del reporte compatible con Python y ausencias upstream sin duplicados/conflictos. Guard JS PASS; las 7.570 apariencias ya están materializadas y conversión e índice físico completos; ejecución runtime pendiente.
- [ ] Retorno de submenús verificado en consola: la navegación C++ conserva la fila de origen al volver desde cualquiera de las nueve opciones. Casos nativos escritos para las nueve rutas y ajustes anidados; ejecución aplazada por instrucción de no compilar.

### Estadísticas de descubrimiento del perfil

- [ ] Pantalla nativa de estadísticas completa: cuatro métricas de catálogo conectadas al perfil (iniciales capturados, iniciales shiny, especies vistas y capturadas), con denominadores canónicos y etiquetas de locales pinned. Reutiliza `FirstRunRuntime::profileCatalogStats`, semántica de `game-stats-ui-handler.ts::displayStats`. No deriva combates, capturas repetidas ni tiempo jugado a partir de flags. Prueba nativa de perfil ausente/fresco/shiny escrita sin ejecutar; guard de wiring/localización y cobertura de glifos PASS. Historial, demás métricas y validación visual siguen pendientes.

### Carga fallida de texturas

- [ ] Verificación nativa de caché de errores: `PokemonAtlasPresenter` recuerda páginas físicas que fallaron (hasta cuatro por atlas) y `TrainerPresenter` recuerda la última identidad fallida. Cambio de identidad/invalidate/clear permite reintentar. Rechaza keys truncadas y mantiene liberación mediante el renderer al fallar metadata de entrenador. Harness de entrenador añadido a la suite: carga fallida repetida, clear/identidad para recuperar, metadata faltante, key larga y liberación diferida. No ejecutado. Guards estáticos JS PASS; ejecución de I/O, carga GPU y comportamiento visual pendientes.

### Habilidad mostrada en la selección

- [ ] Verificar ficha y actor de starters: la ficha consume `FirstRunRuntime::setupStarterAbilityId`, resolviendo slot desbloqueado del perfil con `nativeStarterDefaultAbility` y habilidad de la forma seleccionada. Ya no fuerza ability1 en UI. Comparación nativa con la habilidad del actor preparada, pendiente de ejecución; guard estático JS PASS. Selección manual de habilidad/naturaleza y preferencia persistente siguen pendientes.

### Discrepancia detectada: slots de habilidades y guardado

- [ ] Corregir y verificar slots reales de habilidades: en juego pinned `8555c08c823b856cbec4eb99ca84ea52a955836d`, `src/data/pokemon-species.ts::PokemonSpeciesForm.constructor` convierte ability2=NONE en ability1; `getAbility` usa índices 0/1/2 fijos. `starter-select-ui-utils.ts::getStarterDefaultAbilityIndex` recibe esa instancia efectiva. El helper local usa el NONE crudo y devuelve slot 1 para habilidad oculta cuando no existe segunda regular: discrepancia real, no paridad demostrada.
- [ ] Revisar consumidores de la corrección: `NativeStarterCandyProfile.hpp::nativeStarterDefaultAbility`, preparación y forma de starter en `FirstRunRuntime.cpp`, asignación de abilityAttr al capturar, reconstrucción por identidad en `PokemonBattleState.cpp`, replay y compatibilidad de saves con actores históricos. El constructor de batalla por identidad ya trata slot 1 como segunda regular/alias y slot 2 como oculta. Agregar regresiones para especies con una/dos regulares, formas, captura y save/load antes de dar esta corrección por verificada.

- [ ] Corrección escrita de slots: default oculto usa índice 2; segundo regular ausente se resuelve como alias de ability1. Captura registra bits 1<<índice sin reinterpretar slot 1 como oculto. Ficha y preparación comparten `nativeStarterFormAbility`. Regresiones nativas añadidas para todas las especies starter y formas, perfil hidden-only y concordancia ficha/actor; ejecución y compatibilidad de guardados pendientes. `NativeRunSave.cpp::restoreNativePokemonActorSave` valida abilityId frente a índices fijos y ya rechaza actor histórico incoherente (slot 1 con habilidad oculta), no se agregó una migración inferida.

- [ ] Round-trip de actor hidden-only: capture → encode → decode → restore conserva slot 2 y abilityId; caso incoherente slot 1/habilidad oculta distinta debe rechazarse. Regresión C++ escrita, no ejecutada.

- [ ] Catálogo completo contra fuente real: staging vuelve a enumerar las identidades del árbol/masterlist pinned reutilizando `enumerate_appearances`, verifica SHA del masterlist/generador y exige cobertura exacta del reporte (materializado o ausencia explícita). Tests JS de cobertura faltante, identidad sustituta y ausencia legítima PASS; staging y conversión ampliada sobre el catálogo final PASS; integración nativa sin ejecutar.

### Resultado completo de materialización pinned

- [x] Materializar las 7.570 apariencias del árbol/masterlist pinned: proceso terminado exit 0, 7.570 materializadas, cero ausencias y cero registros no soportados. Reporte `build/upstream-assets/appearances/catalog-report.json`, hash `8782d7d9edaeae4eafddc3af23e42d4d73afe4f9bee390e8b157161fd74377be`; conserva PNG/manifest/provenance. Esto no completa su conversión a `.t3x` ni la integración runtime.
- [x] Staging `--all --appearances`: exit 0, 10.682 identidades totales; 10.679 directas y tres recuperadas con padding transparente (Dustox normal y variantes shiny 1/2 de espalda). Las variantes conservan el PNG materializado y la procedencia pinned.
- [x] Conversión e índice físico de las 10.682 identidades: 10.746 páginas y 215.222.942 bytes en disco; cada textura/metadata verificada por SHA-256. Índice generado con 7.780 apariencias: 7.570 shiny y 210 femeninas normales. Inventario SHA-256 `3320f75cd8323e651fd7372bbe01894d4c9089f3d5b3e0e357e55e5ccdd3f02b`. Reporte reproducible `docs/generated/POKEMON_APPEARANCE_CONVERSION_REPORT.json`.
- [ ] Carga/dibujo del catálogo completo en runtime y presupuesto RAM/VRAM verificados en consola; compilación y Azahar aplazados.
- [x] Cobertura real de conversión: `python test/converted_appearance_catalog_tests.py` PASS (2 pruebas), conserva exactamente las 7.570 identidades/hashes materializados y verifica concordancia de reporte/inventario/header. No ejecuta carga de GPU.
- [x] Verificar valores de referencia del slot por defecto mediante función upstream inspeccionada: `test/starter_ability_upstream_tests.mjs` PASS, 7.588 casos y 517 especies con ability2 raw NONE. Esta referencia no ejecuta el C++.

- [ ] Intro de entrenador por identidad completa: ArenaPresenter recarga por ID, género y nombre; recuerda intentos fallidos hasta cambio de identidad/clear, y entrega al renderer la retirada de texturas al recargar. Casos nativos de mismo sprite, cambio de género/nombre, fallo repetido y recuperación añadidos sin ejecutar; guard estático PASS.

- [x] Finalización ampliada verificable: rechaza duplicados/conteos inconsistentes, valida archivos/hashes y escribe un reporte temporal completo antes de reemplazar `converted-sprite-assets.json`. Guard estático JS PASS; ejecución final sobre los 10.682 atlas PASS. La conversión de texturas no se declara publicación transaccional de un paquete.

- [ ] Render de imágenes válido: `drawImageDirect` rechaza coordenadas/tamaños/rotación/opacidad no finitos y dimensiones no positivas; limita alfa a 1 sin cambiar escalas ni flip. Casos C++ escritos para NaN/infinito/cero/negativo y dibujo válido 1×, sin ejecutar; guard estático JS PASS.

### Identidad visual del retrato de selección

- [ ] Retrato del cursor concordante con la aventura: `FirstRunRuntime::setupStarterVisual` resuelve especie/forma y apariencia/género desde el perfil; `main.cpp` consume esa identidad en lugar de crear un actor sin metadata. Pruebas nativas de concordancia normal/shiny épico/legacy escritas y pendientes de ejecución. Guard de conexión JS PASS. La conversión ampliada está físicamente verificada; la ejecución y comprobación visual siguen pendientes.

- [x] Intro sin interpolación espacial: 16 muestras reales convertidas con nearest y verificadas píxel a píxel, timestamps y hashes de origen/conversión. Reproducción por timestamps escrita sin mezcla de samples.
- [ ] Intro completa y fiel al vídeo de 101 fotogramas, reproducción nativa, memoria/VRAM y fluidez verificadas.

- [ ] Modal de formas y caramelos: el panel de formas cubre los botones anteriores, comparte límites táctiles/dibujo y tiene regreso explícito. Nombres y textos largos usan ancho acotado y el cursor sigue el tamaño realmente dibujado. Guard JS PASS; prueba nativa exhaustiva de coordenadas escrita sin ejecutar. Validación visual pendiente.

- [ ] Foco/paginación de formas: abre en la preferencia real, muestra posición/total y permite L/R por páginas, sin IDs adicionales ni cambios al perfil al navegar. Resolver de desbloqueo reutiliza búsqueda por ID del perfil. Guard JS PASS; casos nativos de navegación reversible/vacío escritos y sin ejecutar; interacción visual pendiente.

- [ ] Tienda de caramelos conectada: botón táctil desde Formas y X abren el modal; D-pad/Circle Pad y táctil eligen opciones, volver regresa a Formas. Compras pasan por gameplay y transacción `NativeProgressStore`; sin almacenamiento no hay éxito simulado ni cambios sobre copias descartadas. Entrada del modal aislada del bridge. Guard JS PASS; geometría nativa escrita, interacción/SD sin verificar.

- [ ] Feedback de compras: distingue especie bloqueada, precio canónico ausente, saldo insuficiente, máximo/ya desbloqueado, perfil inválido y error de guardado. Resultados provienen de los enums gameplay; no inventa precios ni concede compras desde UI. Guard JS PASS; transacción/interacción nativa sin verificar.

- [x] Pasivas reales normalizadas: corrige `passives` plural; 894 definiciones compartidas y 190 mapas por forma, con IDs de enum/provenance/raw conservados. Importación pinned repetida produce el mismo hash `20e7cef5a58d28f31254ab5dfaab0815be92c6a4e7c1e10a3c12c05ebbc9fe27`; test JS de las 1.084 especies y declaraciones inválidas PASS.
- [ ] Resolver nativo de pasivas por forma y nombre en tienda: tabla generada de overrides con fallback a forma cero conforme a `SpeciesDataRegistry.getPassive`; UI consume gameplay y locale importado. Casos C++ escritos sin ejecutar. Activación, supresión y triggers de las pasivas en combate pendientes.

### Presentación: cambios locales pendientes de validación nativa

- [x] Comprobaciones de fuente: nombres de entrenador ajustados a su ventana de 164 px; coordenadas no finitas rechazadas por las tres rutas de texto.
- [x] Submenú global: área de retorno táctil acotada; historial identificado como pendiente en lugar de afirmar ausencia de partidas.
- [ ] Ejecutar casos C++ y comprobar estas pantallas en Azahar cuando el usuario lo autorice. Los guards de fuente no prueban el renderizado.

- [x] Intro de entrenador: dibujado nativo `1×` en lugar de `1.5×`; validación de coordenadas, tamaño y escala antes de cargar el frame para dibujarlo. Comprobado por guards de fuente.
- [ ] Validar visualmente sprites de entrenador altos y animados con el anclaje actual; no se afirma ausencia de recortes para todo el catálogo.

- [ ] Animación de entrenadores: reloj a 24 FPS implementado conforme a `src/data/trainers/trainer-config.ts` pinned (`frameRate: 24`, `repeat: -1`). Pokémon conserva 10 FPS. Casos nativos de límites temporales y overflow escritos, sin ejecutar; falta revisar el límite upstream de nombres 0001–0128 y los nombres extendidos en atlas.

- [x] Empaquetado de entrenadores: 303 metadatos comparados registro a registro con fuente pinned; 378 registros extendidos de tres atlas conservados íntegros en `docs/generated/TRAINER_PRESENTATION_REPORT.json`. Blue conserva 80 frames compatibles; ya no se truncan nombres inválidos. Conversión y verificador físico PASS, sin compilación del juego.
- [ ] Parser C++ y reproducción visual de entrenadores: reloj 24 FPS y límite 0001–0128 implementados; pruebas nativas escritas sin ejecutar.

- [ ] Reloj de entrenador: inicio en timestamp cero conserva su origen con flag explícito; clear reinicia el flag. Código corregido y guards PASS; comportamiento C++ pendiente de ejecución. Harness de entrenador actualizado para la nueva ruta de texto ajustado.

- [x] Intro: 101/101 frames pinned empaquetados en seis páginas; pixels nearest y tiempos completos verificados por frame; raster 200×100 con ampliación entera 2×. Reporte reproducible `docs/generated/INTRO_PRESENTATION_REPORT.json`.
- [ ] Reproducción de intro paginada C++: una página activa, cambio con retirement GPU y último frame completo implementados; pruebas nativas, latencia de I/O, memoria y comparación visual pendientes por prohibición de compilación/Azahar. Estimación: 2 MiB de textura activa y 4 MiB de texturas durante cambio, sin afirmar pico real del proceso.

- [ ] Finalización de intro conectada a `isFinished()` del reproductor en lugar del reloj global. Saltar usa retirement GPU, también cubierto en harness nativo escrito; guards de conexión PASS, ejecución nativa pendiente.

- [x] Ruta de intro: retirada la carga/dibujado de arena oculta detrás de la cinemática. Guard de conexión PASS; sin medición de ahorro real.

- [x] Fondo de selector adaptado offline: 320×180 original → 400×225 nearest, raster final 1:1 y provenance/hash preservados; comparación pixel a pixel y hashes físicos PASS. No implica píxeles originales uniformes: la adaptación fuente sigue siendo 1.25×.
- [ ] Revisión visual del fondo adaptado y retiro GPU de arena/capas: reemplazo y limpieza usan `retireSpriteSheet` con renderer; harness actualizado sin ejecutar. Fondos de batalla y capas todavía tienen escalas fraccionales pendientes.

- [ ] HUD PS: tween visual conectado al reloj y a la identidad/lado de cada actor; duración `clamp(abs(lastHp-hp)*5,250,5000)`, `Sine.easeOut`, números `ceil(ratio*maxHp)` y barra recortada a píxeles enteros. Fuente pinned: `src/ui/battle-info/battle-info.ts::updatePokemonHp`, `player-battle-info.ts::onHpTweenUpdate`, `src/system/settings/default-settings.ts` (DEFAULT). Casos de daño/curación/interrupción y cadencias 15/30/60 escritos, sin ejecutar. Faltan espera de fases y comparación visual; el ajuste persistente de velocidad está implementado con ejecución nativa pendiente; no modifica los PS de gameplay.

- [ ] Selector/HUD: limpieza explícita e iconos liberados mediante retirement GPU, drenado por SYNCDRAW antes de finalizar renderer. Guard de main/presenters PASS; caso de ownership del HUD escrito, sin ejecutar.

- [ ] Etiquetas de tipos: raster original siempre 1:1, rechaza ventanas menores de su tamaño nativo y coordenadas/tamaños no finitos antes de cargar textura. Todos los consumidores actuales reservan al menos 32×14; casos de half-scale y límites reescritos para el nuevo contrato explícito, sin ejecutar. Guard de fuente PASS; apariencia nativa pendiente.

- [x] Cobertura física de fuentes ampliada: ASCII imprimible completo y caracteres no ASCII de UI/catálogos tienen tinta en las cuatro fuentes `.bcfnt`; test de assets PASS. No prueba legibilidad, posiciones ni salida GPU.

- [ ] Ajuste de velocidad PS conectado: cuatro valores pinned, navegación A/táctil/izquierda/derecha, valor localizado y consumidor HUD. Preferencias v3 almacenan velocidad en dos bits del envelope con SHA-256 y journal; v1/v2 preservan marco/táctil y migran a DEFAULT. Cambiar marco/táctil conserva velocidad. Cases de roundtrip, flags, versión futura y conflicto por velocidad escritos sin ejecutar. Guard de conexión PASS; persistencia SD, ejecución C++ y validación visual pendientes.

- [ ] EXP por niveles: `ExperienceBarTimeline` conectada al HUD y reloj; curvas canónicas, duración/multiplicadores `expSegmentTiming`, Sine.easeIn y pausa de nivel upstream. Nivel visible se sincroniza con el tramo, sin modificar gameplay. Casos de varios niveles, curvas reales, instantáneo, límites y cadencias escritos sin ejecutar; guards de conexión PASS. Audio, settings de EXP, coordinación de fases y prueba visual pendientes. La anterior expectativa por frame se sustituyó por proyección instantánea cuando no se proporciona reloj y casos temporales explícitos.

- [ ] EXP actualizada durante animación: conserva la pausa del nivel y el tramo de llenado activo al recibir una ganancia posterior; consume el objetivo actualizado en el siguiente tramo. Casos nativos para actualización durante pausa y llenado escritos, sin ejecutar. Coordinación de fases y comparación visual siguen pendientes.

- [ ] Capas animadas de arena: índice de frame calculado sin multiplicación del timestamp completo, conservando 12 FPS y orden actual. Casos de límites y UINT64_MAX escritos sin ejecutar. Raster de fondos/bases a tamaño nativo pendiente; hoy permanecen escalas fraccionales de 1.25×.

- [x] Fondos de batalla preparados offline a ancho 400 con nearest y altura proporcional; raster de batalla dibujado 1:1. Fuentes pinned, pixels y hashes físicos comprobados en `test/arena_background_assets_tests.py`; reporte `docs/generated/ARENA_BACKGROUND_REPORT.json`. Esto conserva replicación desigual de pixels de origen a 1.25×.
- [ ] Fondos: ejecución GPU y composición visual pendientes; título y bases/capas animadas aún se escalan en runtime. No compilado ni abierto Azahar.

- [x] Raster de título: 39 fondos con recorte centrado proporcional a 400×240 nearest, ruta física y hashes propios; comparación pixel a pixel PASS. Presenter cambia de recurso al cambiar entre título y batalla, retirando el anterior mediante renderer.
- [ ] Validación nativa del cambio de raster y composición de título pendiente. Bases/capas siguen con escala fraccional; el nearest offline no convierte el escalado original en una escala entera.

- [x] Bases estáticas de arena: 72 rasters nearest adaptados offline a 1.25× y dibujados 1:1; fuentes pinned, pixels y hashes físicos PASS (`test/arena_layer_assets_tests.py`). Pipeline separado en `scripts/prepare_arena_layers.py`, reporte `docs/generated/ARENA_LAYER_REPORT.json`.
- [ ] Dos atlas de capas animadas conservan fuente, metadata y escala 1.25×; falta adaptación por frame y validación GPU de todas las bases. Nearest offline conserva replicación no uniforme de pixels originales; no implica una escala entera de origen.

- [x] Capas animadas de arena: 32/32 frames originales de `end_a` y `end_b` reconstruidos, adaptados nearest, recortados y reempaquetados; metadata P3ATLAS1 regenerada y provenance de frames originales conservada. Comparación pixel a pixel por frame y metadata binaria PASS. Todos los fondos y bases de arena ahora se dibujan a raster 1:1.
- [ ] Comparación visual y ejecución GPU de los nuevos atlas pendiente; ritmo 12 FPS conservado, sin compilación ni Azahar. El remuestreo offline 1.25× conserva pixels de origen no uniformes.

- [x] Reproducibilidad de arena: segunda conversión de fondos y capas compara 158 hashes de reportes, cabeceras, texturas y metadata; todos idénticos (`test/arena_conversion_determinism_tests.py`). Guards de raster 1:1 y selección de recursos PASS. Esto no valida salida Citro2D.

- [x] Fuentes físicas: las cuatro hojas BCFNT A4 contienen exclusivamente alpha 0/15; cobertura ASCII/UI/catálogos y hashes de cada archivo PASS. Fuente TTF pinned y métricas publicadas en `docs/generated/NATIVE_FONT_REPORT.json`; pipeline conserva el reporte versionado.
- [x] Integridad física de los glifos: las cuatro fuentes BCFNT coinciden byte a byte al rasterizar nuevamente todos sus glifos mapeados desde el TTF upstream fijado, con versiones Pillow/FreeType verificadas. `test/ui_font_coverage_tests.py`: 3 PASS. Comprueba conversión y cobertura, no renderizado GPU ni apariencia en consola.
- [ ] Legibilidad y alineación visual de fuentes siguen pendientes: alpha binario y cobertura no prueban apariencia en pantalla ni alineación de todos los submenús. Sin compilación ni Azahar.

- [ ] Submenú de nueve opciones: harness de navegación actualizado para la ruta real de Gestionar datos (exportar/importar, cancelar confirmación), entrada táctil y retorno a la misma fila en todas las opciones. Corrige una expectativa obsoleta de ServiceInfo; pruebas C++ escritas sin ejecutar. Logros/huevos/gacha/comunidad/sesión siguen sin servicio funcional completo.

- [ ] Composición de submenú: atenuación del título mantenida en ajustes anidados, información y gestión de datos; Pokédex usa vista superior propia. Selector `overlaysTitle()` conectado a main, casos nativos escritos sin ejecutar; revisión visual pendiente.

- [ ] EXP tras intervalo largo: vuelve a validar el nivel/total recibido después de avanzar la animación anterior; correcciones decrecientes proyectan el valor real sin underflow del piso de EXP. Casos de nivel menor y menor total en el mismo nivel escritos sin ejecutar. No modifica gameplay ni afirma coordinación de fases completa.

- [x] Empaquetado de capas animadas minimiza área de textura potencia de dos entre distribuciones regulares válidas: `end_b` baja de 1024×256 (1 MiB RGBA8 estimado) a 512×256 (512 KiB); `end_a` permanece 1 MiB con dimensiones máximas menores. Frames/pixels/metadata verificados, bytes estimados registrados por capa.
- [ ] Memoria residente y pico real de arena en Old 3DS siguen pendientes; estimaciones de textura no incluyen metadata, CPU, retirement ni otros presenters.

- [ ] Movimientos en combate doble: selección de objetivo trasladada a franja y=210..233, separada de instrucciones y PP; paneles acortados y barra PP ajustada para evitar solapamiento. Nombres de objetivo y ayuda usan texto limitado a su región. Geometría nativa escrita sin ejecutar; guards de conexión PASS, comparación visual pendiente.

- [ ] Movimientos táctiles: Struggle por PP agotados se activa tocando el primer rectángulo mediante el mismo comando de turno de A; otros slots permanecen inactivos. Nombres/PP derivan posición y ancho del rectángulo compartido con hit-test, evitando texto fuera de su botón. Guards de conexión PASS; comportamiento nativo y comparación visual pendientes.

- [ ] Volver táctil en movimientos: región compartida con etiqueta B, acotada a x=100..187; funciona también en combate doble y no consume toques sobre PP/detalles. Instrucciones A/B separadas, botones de objetivo no se solapan. Barrido de geometría escrito sin ejecutar; guards PASS, ejecución/visual pendientes.

- [ ] Equipo: columnas separadas para nombre/nivel/estado/PS; números PS y pie limitados al ancho de pantalla, cursor usa el tamaño real del nombre. Barra PS limita hp a maxHp y ancho a píxeles enteros, color amarillo hasta 25% como HUD upstream. Guards de conexión PASS; prueba nativa y apariencia pendientes. Iconos de esta lista siguen a 0.5×, adaptación pendiente.

- [x] Iconos compactos de equipo: 1500 iconos 40×30 reducidos offline a 20×15 nearest en ocho páginas; identidad y fuente/hash de cada icono conservados. Comparación con PNG pinned por icono PASS (`test/compact_icon_assets_tests.py`).
- [ ] Equipo consume páginas compactas a 1:1; native rendering pendiente. Ocho texturas 256×256 RGBA8 estiman 2 MiB frente a 8 MiB de páginas originales; es un catálogo de recursos, no memoria real medida. Otros consumidores conservan tamaño original.

- [ ] Ruta nativa de iconos compactos: harness renderer cubre ocho páginas compactas, geometría reducida, escala 1×, caché sin recarga por frame, IDs ausentes y retirement hasta fence GPU. Pruebas escritas y registradas en harness existente, sin ejecutar por instrucción de no compilar.

- [ ] Equipo: género real del battleState presentado junto al nombre, reservando ancho y usando tamaño raster del nombre; no inventa símbolo para género desconocido/genderless. Guards PASS; validación visual y variantes shiny en esta lista pendientes.

- [ ] Equipo: indicador shiny original 15×14 a 1:1 con tint de variante 0/1/2, solo para identidad/apariencia resueltas del actor; no infiere shiny de datos ausentes. Hoja física existente reutilizada, carga diferida y retiro GPU conectado al cierre. Guards PASS; composición/ejecución nativa pendientes.

- [ ] Decisiones de captura/aprendizaje: filas de reemplazo usan rectángulos compartidos de input, PS e instrucciones limitados al ancho, iconos compactos a 1:1 en lugar de escala 0.5×. Guards PASS; integración nativa y composición visual pendientes.

- [ ] Aprendizaje de movimientos: máximo de líneas calculado con ink height y line feed reales dentro del botón, evitando tercera línea fuera de la región. Fallback fitted cuando no cabe; casos de límites/no finitos escritos sin ejecutar, guards PASS. Comparación visual pendiente.

- [ ] Recompensas sobre movimientos: nombres localizados mediante ID canónico (antes nombre upstream sin locale), cursor usa raster real tras ajuste de ancho. Cabecera/destinatario/instrucciones limitados a su región. Guards PASS; prueba nativa y visual pendientes.

- [ ] Iconos: coordenadas/opacity/escala no finitas y escalas no positivas rechazadas antes de cargar texturas; opacity limitada a 1. Casos nativos escritos para ausencia de I/O en entradas inválidas, sin ejecutar.

- [ ] Memoria de menús: páginas compactas de equipo/captura/destinatario retiradas cuando dejan de usarse y al volver al título, mediante renderer después de beginFrame. Conserva navegación y selección; cierre de decisiones usa retirement. Guards PASS, ejecución GPU y pico real pendientes.

- [ ] Pausa: draw e input comparten tres filas de 36px; corrige zonas táctiles desplazadas respecto del tercer texto y límites x/y. Etiquetas limitadas al panel y cursor alineado al raster elegido. Barrido de 320×240 escrito sin ejecutar; guards PASS, prueba visual pendiente.

- [ ] Confirmación de movimientos: menú se cierra solo si `advanceBattleTurn()` acepta el comando, tanto A como táctil/Struggle. Rechazos (PP/políticas/runtime) conservan selección y feedback del motor; no altera reglas ni publica medio turno. Guard PASS; verificación nativa pendiente.

- [ ] Navegación doble: D-Pad/circle pad recorren ambas columnas de movimientos; L/R cambian objetivo con ayuda visible. Salida táctil del command presenter usa región B compartida y ya no intercepta botones de objetivo y=210. Guard PASS; interacción nativa pendiente.

- [ ] Controles de combate doble: harness de navegación cubre D-Pad en dos columnas, hombros L/R, toques de ambos objetivos sin cerrar, toque fuera de pantalla y B táctil; combate individual conserva navegación y hombros inactivos. Casos escritos en suite nativa existente, sin ejecutar.

- [ ] Cierre de menús: el command presenter libera también el cursor estático de movimientos antes de finalizar Citro2D, mediante retirement del renderer. Guard de ownership/orden sin compilación; ejecución nativa pendiente.

- [ ] Objetos/balls: posición de atlas redondeada a píxeles físicos; conserva canvas original de 32px en consumidores actuales. Valida coordenadas/tamaño/opacity antes de I/O e índice de página; cierre de recompensas y combate retira texturas mediante renderer. Guards sin compilación; prueba nativa/visual pendiente.

- [ ] Pruebas nativas de objetos: casos escritos para coordenadas/tamaño/opacity inválidos sin I/O, error de carga y reintento, caché, trim upstream intacto, destino redondeado, alpha limitado y limpieza idempotente con retirement. Integrados en harness existente; sin compilar ni ejecutar por indicación del usuario.

- [ ] Pokédex: filtros, celdas y navegación de páginas comparten geometría táctil/presentación; etiquetas L/R limitadas a sus botones. Barrido nativo completo 320×240 escrito para celdas sin solapes y entradas fuera de pantalla, pendiente de ejecución. Guards de conexión PASS; sin compilación ni Azahar.

- [ ] Pokédex/shiny: estrellas originales a escala nativa para todas las variantes capturadas, derivadas de SHINY + DEFAULT_VARIANT/VARIANT_2/VARIANT_3 según upstream pinned `src/ui/handlers/pokedex-ui-handler.ts`. Perfil ausente/legacy/inválido no inventa desbloqueos; lifecycle mediante renderer. Casos nativos para siete máscaras escritos sin ejecutar; sprite Pokémon shiny específico aún pendiente en icon index.

- [ ] Iconos de apariencias: inventario pinned completo de 5054 PNG (incluye shiny/género/formas y cinco iconos de huevo), ordenado por ruta, dimensiones nativas, SHA-256 y referencia a `PokemonSpecies.getIconId/getIconAtlasKey`. Dos inventarios idénticos PASS sin compilación. Índice runtime actual tiene 1500 identidades normales; aún falta resolver variantData/reemplazos de evento, convertir las apariencias adicionales y conectar consumidores. Reporte: `docs/generated/POKEMON_ICON_SOURCE_INVENTORY.json`.

- [ ] Apariencias/iconos: 5054 PNG reales convertidos a 27 páginas `.t3x` RGBA8 sin resampling, integrados en pipeline de presentación. Verificación de todos los píxeles empaquetados y hashes de conversión PASS. Cada página estima 1 MiB residente; no cargar todas a la vez. Falta índice de identidad canónica, cache acotada/consumidores y ejecución GPU. Reporte `docs/generated/APPEARANCE_ICON_CONVERSION_REPORT.json`; assets físicos generados bajo build, no versionados.

- [ ] Índice físico C++ de apariencias: 5054 claves fuente ordenadas y búsqueda binaria sin fallback, enlazadas a páginas convertidas y geometría exacta. Python verifica índice completo contra reporte y límites físicos PASS; casos C++ escritos sin ejecutar. Aún falta resolver identidad de actor y cache acotada antes de conectar UI.

- [ ] Resolver de iconos base: 12000 combinaciones especie/forma/género/shiny/variante, 7680 referencias físicas exactas y 4320 fallback normal explícito según `PokedexMonContainer.checkIconId` pinned. Usa formSpriteKey canónico, masterlist real y excepciones de género/formas de getIconId; reporte preserva hashes de ambas fuentes. Determinismo/referencias/casos base Python PASS. Reemplazos de eventos/exp sprites explícitamente no soportados; falta generar enlaces C++ y consumidor/cache.

- [ ] Enlace C++ de iconos: 12000 identidades base ordenadas, referencias numéricas a iconos físicos y marca explícita de fallback upstream. Búsqueda binaria rechaza variantes inválidas y combinaciones ausentes. Índice completo validado contra reporte Python PASS; recorrido nativo y casos shiny escritos sin ejecutar. Consumidor y cache aún pendientes.

- [ ] Apariencias compactas: 27 páginas físicas 256×256 RGBA8, iconos 20×15 nearest preparados offline para equipo. Los 5054 recortes, páginas y hashes verificados PASS; índice físico incluye rutas compactas. Estimación por página 256 KiB, no medición GPU; siguiente integración debe preparar como máximo las páginas del equipo visible y retirar las anteriores tras sincronización.

- [ ] Equipo/apariencias: consumidor C++ conectado al índice de identidad para actores con género/appearance resueltos; páginas preparadas en lote antes de dibujar, cache fija de seis páginas, sin I/O/evicción durante los draws. Compactos 20×15 a escala nativa; metadata legacy mantiene ruta normal y formas inválidas muestran ausencia. Fallo de carga no sustituye silenciosamente el icono; reintento después de clear. Casos nativos escritos para seis páginas, reutilización, sustitución y recuperación; sin ejecutar. Estimación cache de apariencias 1.5 MiB, transición hasta 3 MiB más cache legacy/cursor; medición GPU pendiente.

- [ ] Captura/equipo: decisiones de sustitución conectadas a apariencia de actor con lote acotado; usa el estado activo actualizado para icono, nombre y HP. Resolver compartido con equipo valida ownership de formas canónicas y distingue metadata legacy de identidad inválida, sin fallback local silencioso. Casos nativos por todas las formas, owner incorrecto y variante inválida escritos sin ejecutar; source guards PASS.

- [ ] Carga de páginas de apariencias: requiere textura/subtexture válidas y dimensiones físicas 256×256 compacta / 512×512 nativa, sin atlas rotado no soportado. Página malformada se retira y no dibuja ni reintenta continuamente; clear habilita recuperación. Guard de código PASS; casos de fallo/retirement/reintento escritos en harness nativo sin ejecutar.

- [ ] Índices de iconos: regeneración aislada dos veces, ambos headers idénticos byte a byte al generado publicado PASS. Brain map incorpora subgrafo `appearanceIcons`: fuente pinned → conversión → identidad → índices → presenter → equipo/captura → pruebas. El mapa y estos checks no prueban ejecución nativa. Reordenamiento por número de especie descartado: muestra de 46 páginas de Pokédex requería 72 referencias de página frente a 69 con orden actual; no aporta mejora demostrada.

- [ ] Slots del equipo en starters: seis iconos compactos 20×15 nativos centrados en sus ventanas, apariencia del actor preparado y estrellas shiny reales; lote máximo seis páginas y limpieza al abandonar setup. Grid del catálogo conserva iconos nativos 40×30. Resolver compartido sin generar RNG ni mutar progreso. Guards de conexión PASS; comparación visual/nativa pendiente.

- [ ] Texto paginado: espacios/tabuladores finales no consumen otra fila ni provocan fallback abreviado al completar el máximo de líneas. Separadores antes de otra palabra y saltos explícitos conservan paginación. Casos de layout y renderer escritos en harness nativo, sin ejecutar; no se afirma corrección visual por lectura del código.

- [ ] Confirmaciones: inicio de starters usa rectángulos compartidos 105×32 y ya no acepta toda la franja horizontal; borrar guardado dibuja dos botones coincidentes con input half-open compartido. Casos nativos de límites/huecos/fuera de pantalla y barrido completo de starters escritos sin ejecutar. Source guards PASS; compilación y visual pendientes.

- [ ] Cargar partida: botones separados de cargar/reintentar y eliminar, con geometría común 132×34 y texto acotado. Touch de eliminar abre confirmación; perfil sin guardado no ofrece eliminación y sí reintento de lectura. Límites y huecos escritos en pruebas nativas sin ejecutar; guards PASS, prueba visual pendiente.

- [ ] Velocidad EXP: ajuste `settings:expGainsSpeed` conectado a comandos, HUD y expSegmentTiming pinned (default/fast/faster/skip). Preferencias v4 guardan bits 19–20; leen v1–v3 con EXP default, preservan el valor desde callers legacy, rechazan versiones futuras/flags desconocidos. Audio y espera de fase siguen pendientes. Casos de 32 combinaciones, v3 legacy, navegación y velocidad visual escritos sin ejecutar; guards PASS, sin compilación/Azahar.

  Referencia EXP fijada: `8555c08c823b856cbec4eb99ca84ea52a955836d`, `src/enums/exp-gains-speed.ts::ExpGainsSpeed` y `src/ui/battle-info/player-battle-info.ts::doUpdateExpAnimation`; divide duración por 2^speed y conserva pausa de nivel. El cambio v4 mueve el caso de versión futura a v5 y reserva flags desde bit 21; no elimina comprobaciones de formato.

- [ ] Submenú global: conserva las nueve entradas upstream de la referencia; filas 272×20 comparten geometría entre dibujo e input. Barrido de todos los píxeles de la pantalla inferior y bordes escrito en harness nativo, pendiente de ejecución. Logros, huevos/gacha, comunidad y sesión siguen sin implementación completa; no se presentan como funciones terminadas. Sin compilación ni Azahar.

- [ ] Compatibilidad de preferencias EXP: regresiones escritas para conflicto exclusivo de EXP sin sobrescritura, recuperación tras escritura interrumpida/corrupta, migración v3→v4 conservando HP/táctil y todos los bits reservados de v1–v4 con checksum válido. Verifican salida intacta ante rechazo; ejecución nativa pendiente por indicación de no compilar.

- [ ] Lista de movimientos: nombres localizados usan hasta dos líneas de raster nativo antes del fallback acotado; PP debajo sin invadir el nombre. Barra de PP cuantizada a columnas enteras, clamp y producto de 64 bits. Casos de límites escritos sin ejecución nativa; revisión visual y compilación pendientes.

- [ ] Entrenadores: carga rechaza hojas sin textura/región o dimensiones nulas, retira recursos mediante renderer y evita reintentos por frame hasta clear/cambio de identidad. Dibujo directo alinea posiciones a píxeles enteros; encuentro conserva escala 1×. Casos nativos de tres fallos físicos escritos sin ejecutar; revisión visual pendiente.

- [ ] Iconos de objetos: únicamente rutas del índice físico generado; eliminado fallback ui/items-0.t3x que no produce el pipeline actual. Fallos de página recuerdan estado hasta clear/cambio de página; imágenes inválidas se retiran y devuelven false. Validación de rectángulo/trim antes de dibujar. Regresiones de recuperación explícita y hoja malformada escritas, sin ejecutar C++; sin compilación/Azahar.

- [ ] Etiquetas de tipo: atlas conserva raster 32×14 nativo; fallback textual alinea rectángulo a píxeles y centra según altura real de tinta, rechaza valores no finitos y evita desbordar etiquetas bajas. Casos nativos de invalidación/centrado escritos sin ejecutar. Compilación y visual pendientes.

- [ ] Selector manual de habilidad: validación compartida de opciones desbloqueadas escrita desde StarterSelectUiHandler pinned (duplica slot normal solo si slot 0 no está desbloqueado; oculta requiere habilidad física). Resolver de selección rechaza índices/flags inválidos sin mutar salida; resolver de forma rechaza especie ajena. Casos de todos los starters×16 máscaras×4 slots escritos, sin ejecutar. Botón/selección persistida y preparación del equipo aún pendientes; no se declara selector integrado.

- [ ] Preferencia de habilidad: perfil v10 (`P3CANDYA`, registro 42 bytes) conserva slot explícito o 255 default. v1–v9 migran sin fabricar elección; validación exige starter capturado y slot realmente desbloqueado, incluida excepción duplicate legacy. Casos roundtrip/v9/rechazos escritos sin ejecutar. UI/transacción de preparación pendientes; no se declara selector completo.

- [ ] Selección de habilidad transaccional: comando de gameplay prepara copia/equipo y guarda antes de publicar; ficha y actor consumen preferencia compartida, 255 vuelve al default. Regresión de Bulbasaur real normal/oculta, PID estable, slot inválido, escritura fallida y restauración escrita sin ejecutar. Controles visibles aún pendientes; no se afirma paridad de triggers por elegir la habilidad.

- [ ] Control de habilidad conectado: submenú de formas muestra botón 272×18 con habilidad localizada actual, Y/táctil emiten ciclo al runtime. Slots duplicados/bloqueados se omiten mediante validación compartida; SD falla sin publicar cambios. Casos de ciclo directo/inverso y guard de conexión escritos. Guards JS PASS; C++/visual pendientes, sin compilar/Azahar.

- [ ] Habilidad en equipo de varios starters: regresión de Bulbasaur líder/Squirtle reserva exige preferencia oculta del cursor, actor reserva correcto, PID/orden/líder intactos y restauración SD del equipo. Fallo de escritura de run tras guardar perfil exige estado en memoria intacto y carga de generación compatible. Casos escritos sin ejecutar; no prueba triggers ni runtime GPU.

- [ ] Ficha superior starters: habilidad y pasiva con encabezados localizados, dos áreas de 220×23 para nombres a raster nativo; pasiva bloqueada identificada [X] y atenuada. Disponibilidad/estadísticas/BST reubicados y texto acotado al panel. Guards JS PASS; geometría nativa escrita sin ejecutar y comparación visual pendiente. Mostrar pasiva no implementa sus triggers de combate.

- [ ] Selector de naturaleza: resolución/ciclo enum 0–24 desde bit n+1 de GameData.getNaturesForAttr y StarterSelectUiHandler pinned. Rechaza máscara/índice/dirección inválidos sin mutar salida; naturaleza única no ofrece cambio. Casos de 25 bits individuales, todos los pares y ciclo completo escritos sin ejecutar. Preferencia guardada, comandos, texto localizado/estadísticas y controles aún pendientes.

- [ ] Preferencia de naturaleza: perfil v11 (`P3CANDYB`, 43 bytes/registro) conserva enum elegido o 255 default y habilidad independiente; v1–v10 leen sin elección fabricada. Validación exige naturaleza desbloqueada de starter capturado. Roundtrip de 25 naturalezas, v10 legacy, índices bloqueados y versión futura sin mutar salida escritos sin ejecutar. Comando/UI/estadísticas todavía pendientes.

- [ ] Naturaleza aplicada: habilidad/naturaleza comparten transacción de setup y conservan la otra preferencia. Resolver preparado lleva naturaleza a actor y fórmula de estadísticas; 255 vuelve al default, ciclo recorre solo desbloqueadas. Bulbasaur Adamant con habilidad oculta, PID estable, rechazo/SD fallido y restauración de stats escritos sin ejecutar. Controles/nombre localizado aún pendientes.

- [ ] Controles de naturaleza conectados al submenú de formas: START y táctil comparten comando persistente; botones separados de 132×18 para habilidad y naturaleza. Los 25 nombres españoles se generan desde enum/locales pinned con SHA-256, sin strings de naturaleza inventados. Generación repetida y provenance: 2 tests Python PASS; cobertura de glifos: 2 PASS; guards JS PASS. Barrido de geometría actualizado en harness C++ sin ejecutar. Compilación, interacción y aspecto en Azahar pendientes por instrucción del usuario.

- [ ] Ficha superior de starter: muestra la naturaleza preparada con etiqueta y nombre españoles pinned en un renglón de 220×15, entre pasiva y estadísticas. Los starters bloqueados conservan la etiqueta de bloqueo y no muestran una naturaleza elegida ficticia. Límites contra pasiva/estadísticas y exclusión táctil de ambas preferencias escritos en harness C++, sin ejecución. Cobertura física de glifos y guards de texto ejecutados; aspecto y ejecución nativa pendientes.

- [ ] Renderer compartido: sprites sin rotación ajustan el origen a píxeles enteros, también desde ScenePlayer/QuickJS. Centros de sprites con dimensiones impares conservan el medio píxel correcto; tamaño, flips y posiciones de imágenes rotadas se conservan. Casos negativos, dimensiones impares, flips y rotación escritos en harness nativo sin ejecutar. Elimina una fuente de shimmering por posición; no sustituye la validación visual ni corrige escalas fraccionarias explícitas del contenido.

- [ ] Iconos normales de grid/Pokédex: carga rechaza sheets con tamaño distinto de 512×512 (256×256 compactas), UV no finitas, orientación rotada o textura/subtexture ausente; retirement seguro y fallo recordado hasta clear evitan relectura por frame. Harness cubre ancho erróneo, orientación, NaN, repetición y recuperación, sin ejecutar. Grid shiny/género aún pendiente: necesita residencia acotada para 18 iconos visibles, sin aumentar indiscriminadamente la caché de equipo de seis páginas.

- [ ] Resolver visual de starter por dex: el catálogo puede consultar forma/género/shiny/variante sin mover el cursor ni reconstruir equipo, reutilizando exactamente el resolver de la ficha superior. Comparación con actor preparado en apariencias normal/épica/legacy, cursor intacto e ID inválido escritos en harness sin ejecución. Consumidor grid y presupuesto de páginas de apariencias aún pendientes.

- [ ] Grid de starters conectado al resolver visual por dex y al índice físico de apariencias: forma/género/shiny/variante del perfil, fallback normal solo para metadata desconocida, sin reemplazar variantes faltantes. Páginas compactas 256×256 RGBA8 y escala entera 2× mantienen área 40×30. Límite explícito 18 entradas/páginas para grid compacto; equipo mantiene seis. Techo teórico grid: 4.5 MiB apariencias + 2 MiB páginas normales; transición de apariencias hasta 9 MiB antes de liberar GPU, sin medición de heap/VRAM. Pruebas de 18 páginas, rechazo de 19, reutilización, dimensiones/posiciones y escala inválida escritas sin ejecutar. Calidad de compactación, GPU, carga y pico real pendientes.

- [ ] Residencia del selector: al entrar al título se retiran las páginas normales/de apariencias de grid y equipo; cada grid retiene solo páginas normales de sus celdas visibles antes de dibujar. Las páginas fallidas conservan el intento mientras siguen visibles. Casos de batch válido/null/sobre límite y reutilización escritos sin ejecutar; retirement GPU y pico real requieren validación nativa. No se compila ni abre Azahar.

- [ ] Corregir pérdida de detalle del grid compacto ampliado: pipeline de tiles nativos 40×30 con padding transparente 64×32 RGBA8, sin resize. Catálogo pinned completo e índice físico con hashes; 18 texturas estiman 144 KiB activas (288 KiB durante reemplazo), sin contar overhead/otras capas. 5.054 tiles convertidos: píxeles originales, padding, provenance y hashes PASS; generación repetida byte a byte PASS. Consumidor C++ todavía usa grid compacto hasta conectar tiles nativos. No afirmar paridad visual ni medición de memoria.

- [ ] Grid capturado conectado a tiles nativos: identidad física del índice determina el tile real, carga limitada a 18, dimensiones 64×32 validadas, recorte original 40×30 a escala 1× y posiciones enteras. Equipo conserva sus páginas compactas; metadata legacy/desconocida conserva icono normal. Harness de carga/recorte/reutilización escrito sin ejecutar. Píxeles físicos verificados; GPU, aspecto y memoria real pendientes.

- [ ] Grid visto/desconocido: resuelve tile normal original solo como silueta de presentación, mantiene tint gris/negro y no cambia perfil, género/shiny desbloqueado ni variante conocida. Variante conocida faltante no cae a normal. Celdas con tile ya no retienen atlas normal redundante. Comparación de resolución unknown/known/invalid escrita sin ejecutar; guards estáticos PASS. GPU/apariencia/memoria pendientes.

- [ ] Pokédex grid conectado a tiles originales 40×30: base form explícita, apariencia shiny/variante y género por defaults capturados; metadata desconocida usa silueta normal con tint gris/negro. Caché exclusiva nativa de 24 celdas (192 KiB texturas, hasta 384 KiB reemplazo estimado) y retirement al salir. Selección de formas/atributos del Pokédex completo y paridad con intersección de unlocks upstream siguen pendientes; no declarar funcionalidad completa. C++/GPU/visual sin ejecutar.

- [ ] Integridad de generación de tiles: caché reutiliza conversión solo si snapshot/inventario coinciden, PNG coincide y SHA-256 del t3x coincide con reporte anterior. Tiles corruptos/faltantes y fuente cambiada fuerzan conversión; casos temporales de caché cubiertos en prueba Python. RomFS incluye icon-tiles; ContentUpdateStore.assetPath todavía carece de consumidores runtime y manifest de catálogo completo: actualización desde consola no terminada.

- [ ] Equipo del selector: seis iconos con tiles originales 40×30 a escala 1× centrados en slots 44×32. Legacy usa tile normal solo de presentación; shiny/forma/género conocidos conservan su identidad. Texturas activas estimadas 48 KiB (96 KiB durante reemplazo), frente a seis atlas compactos de hasta 1.5 MiB; memoria real sin medir. Harness verifica seis rectángulos y slot inválido, sin ejecutar. Guards estáticos y diff-check PASS; validación visual pendiente.

- [ ] Menú de equipo en aventura: fallo de carga de icono normal ahora muestra indicador de asset faltante igual que apariencia conocida, sin espacio silenciosamente vacío. Guard de ambos caminos PASS. Tiles nativos en equipo/captura aún requieren filas mayores: 30px originales no caben en filas actuales 25px; mantener pendiente el rediseño compartido de dibujo/táctil. Prueba de carga y visual nativa pendiente.

- [ ] Equipo/captura: filas compartidas 304×32 entre y24–216, iconos originales 40×30 a 1× con caché de seis tiles. Nombre de equipo desplazado para evitar icono; pie inferior y224 ajustado a 8px nativos. Captura nombre/pie ajustados; dibujo e input conservan misma geometría. Barrido táctil 320×240 y límites de filas/iconos actualizados en harness sin ejecutar. GPU/lectura/layout visual pendientes, sin compilación/Azahar.

- [ ] Selección de destinatario de recompensas: encabezado y pie reutilizan rectángulos de equipo y dejan libres las seis filas ampliadas; feedback del motor visible en encabezado y controles en pie de 8px nativos. Corrige solapamiento heredado y204 con última fila. Guards estáticos PASS; límites de encabezado/filas/pie añadidos al harness nativo sin ejecutar. Revisión visual pendiente.

- [ ] Presenter de sprites Pokémon: valida coordenadas/tamaño y anchors/scale antes de seleccionar metadata o leer textures. Rechaza NaN/infinito, dimensiones no positivas y escala negativa; cero sigue siendo escala automática. Guards estáticos comprueban orden previo a I/O, sin afirmar prueba de ejecución C++. Escalas del consumidor principal de combate son automáticas 1×/2×; revisión visual de sprites grandes sigue pendiente.

- [ ] Recompensa dirigida a movimiento: nombre usa hasta dos líneas nativas de 10px antes de abreviar; PP conserva tamaño nativo 8px y columna independiente. Cursor usa la misma altura/tamaño resultante. Límites de dos líneas y columnas añadidos al harness sin ejecutar; guards JS y fuentes físicas PASS. Paridad visual/nativa pendiente.

- [ ] Sustitución tras captura: nombres usan hasta dos líneas nativas de 10px en columna 159px, separada de icono original 40px y PS 67px. Cursor se coloca antes del icono para no taparlo, alineado al texto. Límites de columnas/filas añadidos al harness sin ejecución; guards y fuentes físicas PASS. Aspecto y navegación nativos pendientes.

- [ ] Nombres de equipo en aventura: columna ampliada a 112px nativos; nivel/género pasan a segunda línea y17 con fuente 8px, separados del nombre y del estado/PS. Evita reducir prematuramente nombres a la antigua columna de 54px. Límites horizontales y verticales escritos sin ejecutar; guards JS/cobertura de glifos PASS. Composición visual nativa pendiente.

- [x] Índice físico de tiles: prueba Python compara orden exacto de 5.054 sourcePath/SHA-256 con AppearanceIcons, revisión/inventario y orden de paths C++; PASS. Prueba correspondencia de metadata/píxeles, no carga/dibujo C++.
- [x] Brain map actualizado con subgrafo nativeIconTiles: pipeline, índices, consumidores, geometría y pruebas; 265 archivos/577 dependencias. Estado actualizado distingue implementación y validación nativa pendiente.

- [ ] UV inválidas: renderer directo rechaza UV NaN/infinito; atlas rechaza también regiones degeneradas/rotadas. Loader de apariencias aplica validación antes de registrar textura disponible y recuerda el fallo hasta clear. Casos directos/atlas añadidos al harness sin ejecutar; guards estáticos PASS. Sin prueba GPU ni compilación.

- [ ] Nombre de modo en el selector: limitado a 124 píxeles en la columna izquierda (x=16..140), separado del panel de detalles que empieza en x=151; usa ajuste de fuente nativa. Guard de fuente añadido; validación C++/Azahar pendiente por indicación del usuario.

- [x] Egg identifiers and incubation constants imported from pinned upstream into EggContentPolicy.hpp: EggTier, EggSourceType, VoucherType; tier waves and Manaphy exception constant. SHA-256/source symbols preserved; Python provenance and repeated generation checks pass. Scope: data only.
- [ ] Connect imported egg policy to persistent inventory, wave lapse, hatching and gacha commands. Species generation, unlocks and animations remain pending; no native validation or compilation performed.

- [ ] EggIncubation.hpp: C++ batch validation and victory-boundary lapse preserve inventory order, reject duplicate IDs/invalid enums/species/overlapping buffers and insufficient output capacity before mutation. Ready eggs are retained for the future hatching consumer; legacy species zero is explicit. Native harness written, not compiled or executed. Not yet connected to victory, inventory persistence or UI.

- [ ] EggData metadata retained in the incubation record: timestamp, imported VariantTier, shiny, eggMoveIndex and hidden-ability override. Lapse changes only hatchWaves; metadata preservation and invalid-field cases added to the unexecuted native harness. Persistence and frontend integration remain pending.

- [ ] NativeEggInventory.hpp: versioned P3EGGS01 component codec with explicit little-endian 24-byte records, preserving all imported EggData attributes and inventory order. Full validation precedes decode; rejects duplicates, malformed flags/reserved bytes, wrong lengths and buffer overlaps. Native round-trip/determinism/corruption harness written but not compiled. No standalone file integrity claim: outer progress journal checksum, atomic persistence and bundle integration remain pending.

- [ ] Existing native save gate now includes egg codec and incubation cases (round trip, deterministic bytes, metadata, malformed records, duplicate IDs, failed-call preservation, ready ordering and empty inventory). Added eggInventory brain-map subgraph. Native gate remains unexecuted; durable journals/export still do not contain eggs.

- [ ] Fitted-text raster selection now validates and bounds UTF-8 before every measurement; long labels cannot overflow the 256-glyph measurement scratch. Renderer harness includes 1023-byte labels and malformed UTF-8, pending native execution. No visual or hardware proof yet.

- [ ] Default egg incubation resolver uses imported tier durations and special-species IDs parsed from the pinned Egg method/SpeciesId enum. No IDs are hardcoded in UI. Python provenance/determinism passes; native save gate covers every tier, both special species and invalid inputs, unexecuted. Species egg-tier resolution and gacha creation remain pending.

- [x] Canonical egg-tier import corrected: all 1084 species checked against their preserved pinned upstream declarations. Counts: COMMON 830, RARE 130, EPIC 98, LEGENDARY 26; omitted eggTier uses upstream COMMON fallback. Regression failed on the old Mankey record and passes on the corrected catalogue. Two full imports produced hash 1c5aca75dab6630c32a847b96cc4fbc3f26cf9433ee628ccfa599c181ea5e220.
- [ ] C++ speciesEggTier binary lookup now consumes a generated canonical species-tier index; full-index native checks written, not executed. Existing saves tied to the previous content hash require explicit content/save migration; no automatic compatibility claim.

- [ ] Egg-tier candidate catalog preserves declared-vs-fallback state. speciesForEggTier matches the pinned registry by excluding species without a declared eggTier, even when their lookup fallback is COMMON. Full declared-field comparison passes in Python; native tier lists/order and failed-capacity checks written, unexecuted. This is the candidate catalog, not weighted rollSpecies or completed gacha.

- [ ] Egg.rollEggTier decision ported to eggTierForRoll using imported pinned thresholds. Exhaustive 256-value distributions for all five source types added to native save gate, unexecuted. Function consumes an explicit supplied draw; does not substitute battle RNG for upstream randInt. Gacha RNG, pity, vouchers and complete transaction remain pending.

- [ ] EggGachaPolicy.hpp: pinned rarity decisions and pure pity plan separated from incubation. Pity increments before COMMON-only promotion, legendary/epic/rare priority, legendary-up increment and reset of only final tier follow upstream. Overflow is explicit; native gate cases written but not run. Generated thresholds/provenance/determinism Python checks pass. Live gacha transaction, RNG and persistent pity counters remain pending.

- [ ] Voucher pull planning: five literal offers and 99-egg admission limit imported from pinned EggGachaUiHandler. C++ prepares remaining vouchers/pulls without mutation, preserves inventory-before-balance failure ordering, cancel and explicit free-pull override. Native gate covers offers, exact fit, full inventory, insufficient balance and cancellation, unexecuted. Voucher/egg/pity durable transaction and actual UI remain pending.

- [ ] P3EGGP01 egg-progress envelope binds inventory, vouchers, pity, generation and canonical hash under SHA-256, reusing the existing inventory codec and integrity utilities. Borrowed-view inspection publishes only after full validation. Native round-trip/repeat/corruption/content-mismatch cases registered, unexecuted. SD journaling and linkage to run/profile generations are still pending; current exports do not include this envelope.

- [ ] Egg-progress journal/backend: NativeSaveStorage reused with bounded caller-owned two-slot scratch, exact generation reads, committed-slot preservation during prepare/retry and readback checksum. SD methods use existing durable write helpers for eggs0/eggs1.p3eggs. Interruption/retry/exact-generation cases added to native save gate, unexecuted. Main/gameplay and global run/profile generation coordination are not connected yet; no SD execution or portable export proof.

- [ ] Egg journal exact-generation export now writes/reads back and checks the exported envelope; generation zero is rejected, avoiding accidental export of prepared state. Journal verifies checksum before treating unknown format as authoritative, recovering committed state from a corrupt alternate slot. Cases registered in native save gate, unexecuted. Global bundle/run generation binding remains pending.

- [ ] Battle command presentation: draw/touch now share kCommandButtonRects, all four labels resolve imported pinned command-ui-handler strings, shadow/foreground use matching bounded native font selection, and cursor uses the displayed label size. Source guards added; C++ execution and visual confirmation remain pending under the no-build instruction. Pinned Spanish ball label is upstream "Balls", preserved without inventing a translation.

- [ ] Starter confirmation/candy presentation: confirmation buttons derive label/cursor positions from shared touch rectangles, bound localized yes/no labels to the available width, and align cursor to the actual selected native raster. Candy headings/cost label are bounded and both purchase cursors follow fitted text size. Source guards added; native/visual validation remains pending without compilation or Azahar.

- [ ] Direct sprite renderer now rejects degenerate/reversed subtexture UV ranges consistently with atlas crops. Mirroring remains via flipX/flipY scales. Native regression cases cover zero/reversed UV extents and are unexecuted; source guards pass, GPU/visual validation remains pending. This defensive fix does not prove the screenshot font/smoothing issues resolved.

- [ ] Frontend confirmations: import selection highlight/text/cursor share existing touch rectangles and imported yes/no strings; touch-disable confirmation now draws visible windows matching both hit areas, with bounded localized button labels. Source guards pass; native interaction/render validation remains pending under no-build instruction.

- [ ] Native font bitmap metrics: conversion replaces stale grayscale bitmap widths with the monochrome ink extent rather than taking max(old,new). This removes oversized zero-glyph UV widths (up to 36px for an 8px advance); advances/bearings remain upstream. Four fonts regenerated and provenance hashes updated. Physical/converter tests pass; native text rendering and visual confirmation remain pending.

- [ ] Native font bearing: monochrome conversion now stores the matching raster left bearing, preserving source advance/baseline. Verified mismatch for digit 1 at 12/16 points corrected; invalid signed-byte bearings reject conversion. Four font assets regenerated, metrics policy recorded in provenance. 14 converter tests + 3 physical font tests PASS; GPU/visual validation remains pending without build/Azahar.

- [ ] Shared nine-slice windows: reject nonfinite geometry/invalid texture ranges and snap origin/dimensions together to whole pixels before deriving patches. Source guards pass; native harness covers invalid inputs and shared outer edges but is unexecuted. No visual/GPU seam claim without the deferred native validation.

- [x] Regresión offline de anclaje de animaciones: frames con canvases/trim distintos se reconstruyen en canvas común inferior/centrado antes de nearest; comparación píxel a píxel, duración e importación repetida verificadas. Cinco casos Python de native_sprite_pixel_tests PASS mediante selección explícita de casos que no compilan. El caso C++ y la animación visible siguen pendientes.

- [x] Egg.rollSpecies pinned policy data: imported tier starter-cost bounds (COMMON 1..3, RARE 4..5, EPIC 6..7, LEGENDARY 8..9) and actual general-pool exclusions PHIONE/MANAPHY/ETERNATUS, resolving IDs through upstream enum. Provenance includes rollSpecies; repeated header/report generation and data checks PASS. Weighted draw, focus/special branches, unlock pity/variant filters and runtime/UI connection remain pending.

- [ ] Egg species weight C++: imported/validated actual Math.floor expression constants, canonical tier cost bounds, double arithmetic matching JS, clamping before weight calculation, explicit invalid tier/nonfinite input and unchanged output on failure. Existing native save gate has boundary/fractional/nonfinite regression cases, unexecuted. Two pinned generation/provenance/determinism tests PASS. Actual weighted pool/draw and persistent gacha commands remain pending.

- [x] Requested ARM preview build: make -f Makefile.3ds 3ds completed with exit 0 at commit f6f772d; build/GUI_3DS.3dsx is 266877972 bytes, magic 3DSX, SHA-256 3e7c9b412f7ebaf431af2b14d44f777c77a0cb1798a9ea5fc54218524786abdd. Includes regenerated fonts/RomFS. This proves compilation/packaging only; no Azahar or Old 3DS execution.
- [ ] UTF-8 abbreviation warning: trailing-space scan initializes codepoint and checks decoder success before classification. Existing UTF-8 regression harness remains pending execution; this source change is newer than the requested preview binary and has not been rebuilt.

- [ ] Egg weighted species draw: C++ validates a caller-owned filtered pool against canonical species/base costs, rejects duplicates/missing species/overflow, computes total before accepting roll, then selects first cumulative threshold strictly greater than roll in supplied order. No RNG/live mutation; outputs remain unchanged on failure. Boundary and malformed-tail cases added to existing native save gate, unexecuted. Upstream-order pool construction, pity/variant/focus filters and runtime transaction remain pending.

- [ ] General egg species pool C++: declared canonical tier membership followed by imported PHIONE/MANAPHY/ETERNATUS exclusion; ascending numeric SpeciesId order matches Object.values registry integer keys. Preflight capacity/alias checks before output publication; native gate checks all tiers/order/exclusions/failure preservation, unexecuted. Unlock-pity/variant filters and special/focus branches remain pending.

- [ ] Egg unlock pity decision: threshold/cap imported from actual pinned rollSpecies predicates. C++ uses locked pool only at threshold with nonempty candidates; counter resets for newly unlocked species or increments/saturates for caught/in-inventory species. Native boundary cases written, unexecuted. Building locked/variant pools from persistent profile/inventory and durable gacha integration remain pending.

- [ ] Weighted general egg pool validation: reject undeclared tier fallbacks, species belonging to another egg tier and imported excluded species before publishing totals/selection. Native malformed-pool cases written, unexecuted. Variant eligibility still requires species/form variantData semantics, not inferred converted sprite presence.

- [ ] Egg variant eligibility: pinned masterlist root species/form keys imported with asset revision/SHA; C++ follows hasVariants presence semantics with base-species fallback. Nested female/back sprite namespaces remain in source provenance, not species eligibility. Physical/provenance/determinism data tests pass; C++ pool filter integration/validation remains pending.

- [ ] Egg variant lookup: binary search over deterministic lexically sorted pinned keys avoids full scans per candidate. Existing native gate now covers every imported key, base fallback, absent keys and form-only Koraidon eligibility. Native cases unexecuted; runtime filter/transaction still pending.

- [ ] Combined egg pool filter: validate canonical pool, apply nonempty locked-species guarantee before RARE/EPIC variant eligibility, preserve supplied order and explicitly reject empty final pools without fabricating fallback. Pure predicates consume stable caller snapshots; preflight capacity/overlap before output. Native cases prove intended order/failure preservation when executed; execution and persistent-profile/inventory adapter remain pending.

- [x] Nueva compilación ARM solicitada sobre 45edb47: make -f Makefile.3ds 3ds -j2 terminó con código 0 y empaquetó build/GUI_3DS.3dsx con RomFS. Incluye la corrección del decodificador UTF-8 posterior al preview anterior. No prueba ejecución en Azahar ni en Old 3DS.
- [ ] Advertencias de indentación del preview: previousUtf8 y dexAt separados en instrucciones explícitas conservando el recorrido y selección. Este ajuste posterior al binario requiere la próxima compilación autorizada; revisión de fuentes solamente.

- [ ] Elegibilidad de variantes por forma canónica: eggSpeciesFormHasVariants resuelve el índice upstream mediante el catálogo existente y adapta la clave de enum a la clave de assets sin alterar datos importados. Caso Koraidon apex-build y rechazo de índices/especies inexistentes añadidos al gate nativo, pendiente de ejecución. No conecta todavía el perfil ni la transacción gacha.

- [ ] Adaptador de selección de huevos a registros persistentes: filterNativeEggSpeciesPool consume capturas exactas del perfil e inventario EggIncubationRecord; valida orden, duplicados, referencias y solapamientos antes de publicar. Formas de variantes usan índice cero del registro upstream, no preferencias de starter. Gate nativo añadido para especie pendiente, garantía agotada e inventario inválido; ejecución y conexión de la transacción/UI siguen pendientes.

- [ ] Presentación de recompensas: todos los textos restantes pasan por ajuste al ancho; botones confirmar/omitir comparten geometría táctil y los cursores siguen el raster mostrado. Guards de fuentes añadidos; compilación/ejecución y prueba visual de las dos pantallas pendientes.

- [ ] Menú del equipo: estados consumen atlas localizado statuses mediante drawHudIndicator (nearest, escala nativa) como PartySlot upstream; si falta el frame se muestra desconocido explícito. Cabecera, género, indicadores y PS limitan ancho; cabecera usa raster pequeño dentro de su zona. Guards de fuentes PASS, validación ARM/visual pendiente.
