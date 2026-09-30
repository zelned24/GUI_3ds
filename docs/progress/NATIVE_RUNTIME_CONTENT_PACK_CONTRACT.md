# Catálogo actualizable en Old 3DS — contrato de integración

## 1. Estado y alcance

1. Este documento define el formato y las fronteras para la siguiente implementación; no afirma que exista un lector de catálogo ni una actualización instalable.
2. `ContentUpdateStore` ABI 1 admite manifiestos de presentación con hasta 512 entradas y hasta 64 MiB. El inventario convertido de Pokémon tiene 2903 páginas: no cabe en un único manifiesto ABI 1.
3. Las estructuras `m_active` y `m_pending` mantienen dos manifiestos completos en memoria. Aumentar el array hasta cubrir todos los assets aumentaría memoria residente; no hacerlo como sustituto de un índice paginado.
4. El runtime consume hoy tablas compiladas de `PokerogueRuntimeContent.hpp`. Descargar texturas no cambia especies, learnsets, biomas ni movimientos. Hace falta conectar un lector versionado a los consumidores existentes.
5. Conservar ABI 1 para releases ya soportadas. El formato nuevo tendrá su propia versión; no reinterpretar bytes antiguos como un catálogo nuevo.

## 2. Unidad de release

### 2.1. Descriptor firmado

1. Un release declara ID monotónico, versión del formato, versión de esquema canónico, capabilities mínimas de runtime, hash de catálogo, revisiones de los tres upstream y hash/tamaño de cada índice/payload.
2. Los IDs del contenido siguen siendo los IDs upstream/canónicos. El release ID no participa en resultados de simulación ni modifica semillas.
3. La firma cubre el descriptor completo y, mediante sus hashes, todos los índices y payloads. El reloj de importación queda fuera del contenido determinista.
4. La verificación depende de una clave confiable provisionada en el ejecutable. Una clave enviada por el mismo paquete no puede autorizar ese paquete.
5. El algoritmo y key ID deben ser explícitos; reutilizar la frontera `ContentSignatureVerifier` sin fingir que SHA-256 proporciona autenticidad.

### 2.2. Archivos indexados

1. Separar índice de catálogos, payload de catálogos, índice de assets y payloads de assets por shards. El descriptor enumera shards; no enumera cada frame o especie en un array residente.
2. Usar offsets y tamaños enteros de ancho fijo, little-endian, sin serializar structs C++ ni punteros ni padding de compilador.
3. Cada cabecera incluye magic, formatVersion, headerBytes, fileBytes, recordCount, indexOffset y stringTableOffset. El lector rechaza versiones/capabilities desconocidas antes de activar.
4. Las entradas del índice contienen kind, ID canónico, offset, tamaño, schema y hash del registro. El orden es estable por kind e ID; IDs duplicados se rechazan.
5. Las rutas físicas se derivan de hashes/IDs de shards verificados; no utilizar rutas arbitrarias del manifiesto remoto para escribir en SD.
6. Arrays de formas, abilities, learnsets, rutas, atributos y locales usan offset/count verificados. No fijar máximos de catálogo como capacidades del engine.
7. IDs numéricos y string IDs no se convierten en posiciones de array como identidad persistente. Un cambio de orden no cambia la entidad.
8. Mantener provenance y extensiones raw en el paquete canónico; cargar metadata extensa bajo demanda. Preservación no exige tener toda la metadata en RAM durante combate.

## 3. Lector y consumidores

### 3.1. Reutilización del sistema existente

1. El lector alimentará RuntimeContent; no crear otra base de Pokémon o managers paralelos.
2. Introducir una frontera de lookup por ID que pueda usar tablas compiladas o release SD verificado. El contenido compilado seguirá siendo baseline explícito; un error SD no sustituirá silenciosamente una run guardada por otro catálogo.
3. Migrar por consumidores: lookup de especies/formas → moves/abilities → learnsets/evolutions → encounters/routes/trainers → items/locales/assets. Durante la transición, cada run usa un único catálogo coherente.
4. No mezclar una especie del paquete nuevo con movimientos del catálogo compilado salvo que el descriptor declare y valide una dependencia exacta.
5. Sustituir recorridos de `kSpeciesCount`, `kMoves`, `kLocales` y demás arrays por accesos indexados en los consumidores que deban actualizarse. El generador seguirá construyendo baseline y paquetes desde el mismo CanonicalContent.
6. La validez de referencias y capabilities se comprueba antes de activar; un movimiento con metadata conservada pero comportamiento no migrado conserva un estado explícito de no soportado.

### 3.2. Memoria y vida útil

1. Leer páginas de índice y registros con buffers acotados; no cargar catálogos JSON completos ni todos los manifiestos/texturas en Old 3DS.
2. Especificar ownership de strings/records: una vista prestada no puede sobrevivir a una lectura que reutilice su buffer. El estado de batalla copia campos necesarios; presentación conserva o resuelve textos con una vida útil clara.
3. Un release queda fijado para la run activa. Descargar otro release no invalida punteros ni cambia reglas durante un turno.
4. Los presupuestos de caché/texturas se definirán con medición de Old 3DS. No declarar que 64 MiB de paquete equivalen a 64 MiB disponibles en RAM.
5. Validar offset <= fileBytes y size <= fileBytes - offset antes de cada lectura, así como count/stride sin overflow. Una lectura incompleta es un error, nunca un registro cero válido.

## 4. Instalación y guardados

### 4.1. Secuencia de activación

1. Descargar descriptor → verificar firma/compatibilidad → estimar espacio → descargar índices/payloads en staging → verificar hashes y referencias → escribir activación inactiva → leer y validar → activar al entrar en un punto seguro.
2. Reutilizar la activación dual y almacenamiento de `ContentUpdateStore`; ampliar formato y lector por versión, conservando el release previo hasta completar la operación.
3. Una interrupción puede dejar staging incompleto; no puede cambiar el release activo ni dañar el export de progreso.
4. Soportar cancelación/reanudación con límites de bytes y validación final. Los payloads obtenidos de una URL no verificada no se ejecutan ni activan.

### 4.2. Compatibilidad de progreso

1. Los saves fijan hash/schema/capabilities de contenido y versión de save. Su migración es explícita y determinista, con copia recuperable del anterior.
2. Conservar release antiguo si hay un save que lo necesita o proporcionar una migración demostrada. No aceptar compatibilidad solo porque el número de especies coincide.
3. Exportar progreso incluye identificador de contenido requerido. No empaquetar assets completos dentro del archivo de save.
4. Actualizaciones de datos dentro de capabilities existentes pueden evitar reinstalar `.3dsx`. Nuevos efectos, formato o capacidades nativas pueden requerir ejecutable nuevo; la UI debe distinguir ambos casos.

## 5. Implementación pendiente y entrega a otra IA

1. Generador determinista del descriptor, índices y payloads desde el canonical existente, con provenance y overrides aplicados.
2. Lector C++ con validaciones, lookup y vida útil explícita; adaptador RuntimeContent compartido con baseline compilado.
3. Extensión versionada de ContentUpdateStore, verificador real y backend HTTPS/SD.
4. Migración de todos los consumidores del catálogo y AssetIndex al release activo, incluida compatibilidad de save.
5. UI de actualización podrá desarrollarse por la IA de presentación mediante eventos/estado de progreso; no mostrar instalación exitosa mientras la cadena anterior falte.
6. Pruebas a escribir: offset/count overflow, truncado, IDs duplicados, referencias rotas, firma incorrecta, capabilities faltantes, hashes, interrupción, rollback y export compatible. Ejecutarlas en la etapa final acordada, junto a compilación y hardware.
7. Evidencia final: nueva entidad importada → paquete firmado → descarga real en consola → activación → consumidor de catálogo/asset → save/load/export, sin reinstalar el ejecutable cuando no cambian capabilities.

## 6. Primer código del lector: límites de tablas

1. ContentUpdateStore.hpp expone contentRangeValid, contentTableRecordOffset y readContentTableRecord sobre ContentStorage existente. Validan rango, tabla completa, stride/count/index, multiplicación sin overflow, capacidad de salida y lectura exacta, sin cargar el índice completo.
2. El caller aporta la ruta local confiable y tamaño verificado; estas funciones no autentican archivos ni interpretan catálogos. Un backend puede llenar parcialmente el buffer antes de fallar; el caller descarta el resultado en ese caso.
3. Se añadió test/native/content_index_bounds_harness.cpp para límites, overflow, capacidad y lecturas truncadas. Es un harness host pendiente de incluir/ejecutar en la validación final. No se compiló.
4. El formato binario concreto, decodificación de cabeceras/records, generador, lookup canónico y consumidores siguen pendientes; esto no habilita OTA.
