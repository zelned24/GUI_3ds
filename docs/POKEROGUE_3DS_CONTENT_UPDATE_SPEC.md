# PokéRogue 3DS — Contenido y actualizaciones

Autoridad arquitectónica: [`GUI_3DS_NORTH_STAR.md`](../GUI_3DS_NORTH_STAR.md). Este contrato describe el destino; el repositorio sólo tiene hoy un importador vertical slice y no implementa un updater instalable.

## Pipeline

```text
Upstream repositories
 → pinned revision(s)
 → acquisition and integrity check
 → importer / normalizer
 → canonical content + preserved extensions
 → explicit 3DS overrides
 → deterministic generated content
 → diff and ContentPackage
 → signature/hash verification
 → compatibility check
 → atomic install / rollback-safe activation
```

No copiar código Phaser/TypeScript al runtime. La licencia/procedencia de contenido y assets se debe respetar y conservar según cada fuente.

## Estado actual y brecha

`PokerogueSource.js` fija URL, branch y revisions de `pokerogue`, `pokerogue-assets` y `pokerogue-locales`; `PokerogueRepository` cachea por repo/revision/path. `PokerogueImporter` parsea slices de species/moves/abilities, enums y locales; `PokerogueManifest` produce entradas con repository, revision, sourcePath, hash y schemaVersion. Existen resolvers/index de assets con provenance. El alcance observado no constituye espejo completo ni pipeline de releases: no se encontraron canonical biome/wave/item/mode imports, preservación general de claves desconocidas, override registry, content diff/package, firma o instalador.

El importer sintetiza declaraciones offline y `DataManager` inicializa un baseline vertical slice. Esas rutas están marcadas como fallback en parte del código, pero no deben confundirse con producción/upstream. `CanonicalModels` completa campos ausentes con valores por defecto y no ofrece una extensión raw general; algunos paths de sprite se sintetizan en el modelo, por lo que no son prueba de archivo físico.

## Modelos destino

```text
ContentManifest {
  packageVersion, contentVersion, minimumRuntimeVersion,
  schemaVersion, entries[], payloadHash, signature, signingKeyId
}
ContentPackage { manifest, payload }
ContentEntry { id, kind, path, hash, size, dependencies, provenance }
Provenance { repository, revision, sourcePath, sha256, schemaVersion }
```

`runtimeVersion`, `contentVersion` y `saveVersion` son versiones separadas. Cada fuente importada guarda la revisión exacta y checksum SHA-256 del contenido crudo; generación serializa de forma estable. El formato de firma y confianza de clave requiere decisión de implementación y amenaza/operación definidas; no se debe fingir autenticidad con un hash sin firma.

## Compatibilidad e instalación

Antes de activar un paquete, verificar firma, hash, schema soportado, `minimumRuntimeVersion`, dependencias y espacio. Aplicar en staging; activar de forma atómica y conservar ruta de rollback. Un contenido rechazado debe dar error localizado/diagnóstico claro. Especies, sprites, biomas, movimientos, items y diálogo que usen capabilities existentes no deberían exigir un `.3dsx`; nuevos esquemas/capacidades sí pueden requerir runtime.

Los saves declaran `saveVersion`, `contentVersion` y rango/identificador de compatibilidad runtime. Migraciones son explícitas, deterministas, idempotentes cuando corresponda, y el juego informa claramente si no puede abrir un save. Usar como referencia la política upstream de rechazar saves de versión más nueva que cliente, sin copiar implementación antes de revisar el contrato vigente.

## Preservación, overrides y determinismo

- No descartar silenciosamente campos desconocidos; retenerlos en `extensions`/raw metadata acotada y validada, o fallar con diagnóstico si no es seguro.
- Aplicar `upstream canonical + local override = final data`; guardar origen y alcance del override.
- Ordenar keys/entries/dependencias y excluir reloj, datos volátiles e IDs aleatorios de outputs.
- No tratar fixtures offline como import upstream exitoso; el estado/source del dataset debe ser explícito.
- Mantener compatibilidad hacia atrás con formatos de proyectos y escenas existentes.

## Gate de integración futuro

Para cada entidad, demostrar fuente → revision pin → import → modelo canónico → override → output determinista → test/resultado. Separar pruebas con fixtures de red/credenciales para no hacer que `npm test` dependa de disponibilidad upstream. Actualizaciones de contenido deben incluir pruebas de desconocidos, hashes, determinismo, incompatibilidad y rollback antes de habilitar publicación/instalación.
