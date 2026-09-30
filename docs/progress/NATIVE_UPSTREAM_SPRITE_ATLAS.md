# Native PokéRogue sprite atlas migration

Pinned assets revision: `pagefaultgames/pokerogue-assets@056a1f408f26a3be4fef243f7462cb43608c7928`.

Pokémon sprites use TexturePacker manifests such as `images/pokemon/1.json`.
The frame array is at `textures[0].frames`; the existing importer previously
recorded zero frames from `parsed.frames`. The production importer now retains
the atlas image path and dimensions, every frame rectangle, its original size
and trim offset, in source order. It validates bounds and rejects rotations,
multiple textures and malformed records until those formats have native support.
The generated C++ content gains compact atlas/frame tables keyed by canonical
species ID, with manifest path and hash.

Source inspection found 102 frames in `1.json`, 144 in `6.json` and 116 in
`25.json`. Charizard includes both trimmed and untrimmed frames. These are
manifest facts, not a claim that any corresponding texture is installed.

`verified`/`manifestVerified` mean the pinned JSON was read and hashed.
`imageVerified` stays false and `imageHash` null until the PNG is fetched and
checked. The current asset index includes missing entries and the repository
does not contain converted `.t3x` textures. Native atlas subtexture drawing,
conversion, full species/form coverage, palette/layout parity, animation
timing and Old 3DS hardware memory measurements remain pending. The existing
text presentation remains provisional. Compilation and tests are deferred
until implementation is complete as requested.

The RomFS packager now rejects test fixture paths by default and no longer
searches `test/fixtures/assets` implicitly. Offline fixture packaging must
opt in with `allowTestFixtures: true`. For both textures and raw assets, the
RomFS manifest records the SHA-256 of the actual input bytes; it does not
substitute the converted `.t3x` hash or a path-derived catalog value. Pinned
PNG staging now verifies source bytes for the base and named-form inventory.
Atlas conversion still needs to be connected before sprites render on hardware.

The first three species now have pinned front and back PNGs and atlas manifests in a committed source
lock at `project/data/assets/pokerogue-sprite-lock.json`. The staging script
`scripts/stage_pokerogue_sprite_assets.mjs` fetches only the pinned revision,
checks both SHA-256 values and PNG/atlas dimensions, then writes an ignored
`build/upstream-assets/staged-sprite-assets.json` with package inputs. The
front PNGs are 181×181 (species 1), 612×612 (species 6), and 315×315
(species 25); the back PNGs are 156×156, 627×627, and 302×302.
Upstream `getBattleSpriteId(back)` selects the separate back atlas for the
player Pokémon. Staging succeeded for all six without invoking `tex3ds` or
compiling C++. `AssetPackager` can consume the staged entries by honoring their
explicit source paths and expected SHA-256 values. Connecting this staging to
the 3DS build, converting texture data, and rendering atlas frames remain open.

The pinned PokéRogue, pokerogue-assets and pokerogue-locales Git revisions are
also available under ignored `build/upstream/`. `scripts/import_pokerogue_content.mjs`
now uses `PinnedLocalRepository`: it verifies each local HEAD against the
configured revision, reads local files, and can resolve a non-materialized
file from the sparse assets tree. If a local clone is absent, it continues to
fetch the configured pinned raw URL. A mismatched local revision fails clearly.
The canonical JSON has since been regenerated from these pinned local clones.

The pinned asset tree contains 1,051 complete numeric base atlas pairs and
400 named-form atlas pairs in each facing (front and back).
`scripts/generate_pokerogue_base_atlas_index.mjs`
derives `public/js/data/PokerogueBaseAtlasIndex.js` from the pinned Git tree;
the script rejects a local revision mismatch. The six locked PNGs are an
initial independently hashed subset. The canonical species catalog has 1,084 records,
so numeric base atlas coverage alone does not explain every species or form.
The importer now projects the full pinned base/named-form path inventory into
species asset references, distinguishing `INDEXED_BASE_ATLAS`,
`INDEXED_NAMED_FORM_ATLASES`, and `MISSING_IN_PINNED_ASSET_TREE`. An indexed
path is not marked physically verified in canonical JSON. Shiny/variant sprites
and `.t3x` conversion still need indexing and staged conversion. The
current importer still deeply parses three atlas manifests, and expanding it
must avoid embedding every animation frame in the C++ header on Old 3DS.
Against the current 1,084-species canonical catalog, 1,051 have a numeric
base front atlas and the other 33 have at least one named-form front atlas;
none lacks a known front path in the pinned tree.

The pinned sparse checkout materialized all 5,806 PNG/JSON files for the
2,902 base and named-form front/back pairs (about 76 MB). Running
`stage_pokerogue_sprite_assets.mjs --all` examined each pair and emitted
SHA-256, source path, physical dimensions, atlas format and frame count for
2,901 usable atlases directly. Twenty pinned manifests name another image or
omit the name; their matching keyed PNGs are accepted only after frame bounds
validation and the mismatch is recorded. Eight frames in species 256 exceed
the declared source canvas by one pixel and retain that geometry explicitly.
The remaining Dustox back PNG is one pixel smaller than its manifest;
`pad_pokerogue_sprite_atlas.py` adds transparent padding to a derived PNG,
preserves the original source hash, and completes the 2,902-atlas inventory.
The report also records 147 declared-size differences and 90 extruded-frame
border adjustments. The staged atlases
have per-atlas `.p3a` metadata files with source hashes, physical dimensions,
frame rectangle, source canvas, trim offset and duration. The 189,096 staged
frames occupy about 6.29 MB of `.p3a` files, at 32 bytes each plus headers.
The C++ `PokemonAtlasMetadata` loader validates one file at a time
and bounds its allocation to 1,024 frames per atlas; the complete catalog is
not placed in ARM11 RAM or a C++ header.

`Renderer2D` now crops one unrotated atlas frame using Citro2D subtexture UVs
and scales its trimmed rectangle into the original canvas. Its previous call
to a nonexistent `C2D_DrawImageAtRotatedScaled` was replaced with the real
`C2D_DrawImageAtRotated` API, with scale factors derived from image pixel
dimensions. `PokemonAtlasPresenter` now pairs a resolved canonical form key
with one `.p3a` metadata file and one current `.t3x` page, keeping at most one
front and one back sheet loaded. `main.cpp` draws them behind the top-screen
scene text. The 2,902 `.p3a` files and 2,903 converted `.t3x` pages are
staged in `build/romfs`. One source atlas (`642-therian:back`) is 1175×1175;
its two pages use P3ATLAS2 frame-to-page metadata.

The local canonical import was regenerated twice from the pinned snapshots
with identical content hash
`1bc4b6c4979a464b7681fab362e773b514398dcf9ceebae01d83e155fbbc8365`.
Its 1,084 species asset references now contain 3 deeply parsed manifests,
1,048 indexed base paths, and 33 species resolved to named-form candidates.
The importer also resolves all 609 forms using pinned `SpeciesFormKey` and
the upstream `formSpriteKey` override. All 609 have an indexed front path;
all 609 have a staged front atlas after explicit image-reference pairing.
The canonical report still says zero verified images because image staging is
tracked in the separate build output and has not been integrated into the
canonical importer. The generated C++ header is about 2.5 MB with only three
embedded atlases. The `.p3a` files provide a streamed runtime format for
the 2,902 staged atlases. Shiny, gender and variant coverage, dynamic sprite
replacement, and hardware memory measurements remain open. Native 10 FPS
battle sprite frame selection is now present but unverified in Azahar/hardware.
The current `ContentUpdateStore` accepts only 512 entries in one pack, so it
cannot yet deliver this full 2,902-atlas inventory as a single console update.
Sharded or indexed content packs and signed metadata delivery must be added
before claiming that every sprite can be updated from the console.

`scripts/convert_pokerogue_sprite_atlases.mjs` now derives a deterministic
conversion plan from the staged pinned inventory. Its default invocation
rechecks every PNG SHA-256 and physical dimension without converting anything.
The current plan finds 2,901 of 2,902 staged atlases within a 1,024-pixel
texture edge. `642-therian:back` is 1,175×1,175 and requires multiple pages.
The explicit `--convert` mode invokes devkitPro `tex3ds` with one untrimmed
RGBA4 sheet per atlas and records the converted file hashes. Running
`npm run convert:pokerogue-sprites` with local `tex3ds` 2.3.0 completed the
full 2,902-atlas inventory as 2,903 `.t3x` pages, totaling 40,127,787 bytes.
The conversion command requires Python with Pillow for the two explicit
source-derived geometry repairs; `TEX3DS` and `POKEROGUE_PYTHON` can select
their installed executables.
The deterministic inventory SHA-256 is
`056d0183a0088d5cda2247f685881b54a0a956cdc0fadf0de333af7182a5a838`.
The command resumes from verified hashes after interruption. Converted-sheet
Citro2D loading, VRAM pressure and visuals in Azahar/hardware remain unverified
until the later program build.

The exceptional Thundurus Therian back atlas is an animation sheet, not a
background. `scripts/split_pokerogue_sprite_atlas.py` checks the pinned PNG
and staged metadata hashes, repacks its 193 frames in playback-name order into
two transparent 1,024×1,024 PNG pages with one-pixel gutters, converts each
page to `.t3x`, and writes P3ATLAS2 metadata with the page encoded per frame.
The original PNG/JSON and SHA-256 provenance remain intact. The split report
is `build/upstream-assets/split-642-therian-back.json` and the page textures
are `build/romfs/sprites/pokemon/back/642-therian-p{0,1}.t3x`.
The native atlas loader accepts P3ATLAS1 and P3ATLAS2 and the presenter loads
only the page needed for the current frame. The pinned `PokemonSpecies.loadAssets`
and `Pokemon.loadAssets` enumerate `0001.png` through `0400.png` and play the
available frames at 10 FPS. Native presentation now follows that name order
and rate, independently of TexturePacker storage order. Hardware performance
and image output are not yet measured.
