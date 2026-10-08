#pragma once

#include <cstddef>
#include <cstdint>
#include "game/PokerogueModifierReward.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"

namespace Pokerogue3DS {

inline constexpr uint16_t kNativeSaveVersion = 28;
inline constexpr uint16_t kNativeSaveRuntimeVersion = 28;
// Bounded text envelope including six trainer members and field/inventory state.
inline constexpr size_t kNativeSaveMaxBytes = 8192;
inline constexpr size_t kNativeHeldModifierCapacity = 32;

enum class NativeSaveStage : uint16_t {
    RunSetup = 1,
    BattleActive = 2,
    BattleWon = 3,
    BattleLost = 4,
    ExperienceGranted = 5,
};

// Unknown legacy appearance remains distinct from a resolved normal Pokemon.
struct NativeAppearanceSave {
    bool resolved = false;
    bool shiny = false;
    uint8_t variant = 0;
};
inline bool nativeAppearanceSaveValid(const NativeAppearanceSave& value) {
    return value.variant <= 2 && (value.resolved || (!value.shiny && !value.variant)) &&
        (value.shiny || !value.variant);
}

// Identity, form and IVs are reconstructed from the pinned seed. Mutable
// per-member state and explicit appearance must survive switches.
struct NativeTrainerMemberSave {
    uint16_t speciesDex = 0;
    uint16_t hp = 0;
    uint8_t moveCount = 0;
    uint16_t moveIds[4]{};
    uint8_t pp[4]{};
    int8_t statStages[7]{};
    PokemonStatusState status{};
    PokemonConfusionTagState confusion{};
    bool sturdyTag = false; // Run envelope v19.
    uint8_t berryCriticalBoostStages = 0; // Run envelope v24.
    bool hasEatenBerry = false; // Run envelope v25.
    NativeAppearanceSave appearance{}; // Run envelope v27.
};

// Explicit actor snapshot: canonical IDs only; computed stats are reconstructed.
// Kept separate from seed-replayed trainer records for captured/evolved players.
struct NativePokemonSave {
    uint16_t speciesDex = 0;
    char formId[96]{};
    uint16_t level = 0;
    uint32_t pokemonId = 0;
    uint16_t abilityId = 0;
    uint8_t gender = 0;
    uint8_t nature = 255;
    uint8_t ivs[6]{};
    uint16_t hp = 0;
    uint32_t experience = 0;
    uint8_t moveCount = 0;
    uint16_t moveIds[4]{};
    uint8_t pp[4]{};
    int8_t statStages[7]{};
    PokemonStatusState status{}; // Actor payload v7; older payloads contain no status.
    PokemonConfusionTagState confusion{}; // Actor payload v10, summon data.
    bool sturdyTag = false; // Actor payload v11; absent in older records.
    bool ivsDerivedFromId = false;
    bool pauseEvolutions = false;
    uint8_t maxPp[4]{};
    bool maxPpResolved = false;
    uint8_t friendship = 0;
    bool friendshipResolved = false; // Legacy payloads resolve to pinned species base.
    bool unburdenTag = false;
    uint8_t berryCriticalBoostStages = 0; // Actor payload v12; absent in earlier payloads.
    bool hasEatenBerry = false; // Actor payload v13; per-battle consumption history.
    bool actorIdentityResolved = false;
    uint8_t abilityIndex = 0;
    char initialTeraType[16]{}; // Empty only for legacy actor payloads.
    uint8_t initialTeraTypeIndex = 0;
    bool initialTeraTypeResolved = false;
    bool appearanceResolved = false;
    bool shiny = false;
    uint8_t shinyVariant = 0;
};
struct PokemonBattleState;
struct PokemonActorIdentity;
// Output is published only after full canonical validation succeeds.
bool captureNativePokemonSave(const PokemonBattleState& state, uint32_t experience,
    NativePokemonSave& output);
bool restoreNativePokemonSave(const NativePokemonSave& saved, PokemonBattleState& output);
bool captureNativePokemonActorSave(const PokemonBattleState& state,
    const PokemonActorIdentity& identity, uint32_t experience, NativePokemonSave& output);
bool restoreNativePokemonActorSave(const NativePokemonSave& saved,
    PokemonBattleState& state, PokemonActorIdentity& identity);

// Current checkpoint actor slots: player party 0..5, trainer party 6..11,
// legacy standalone player 12, wild primary enemy 13, second enemy 14.
// Histories share one pool bounded by the serialized run envelope, not by species
// or Berry catalog counts. Pool exhaustion is explicit; no history is truncated.
inline constexpr size_t kNativeBerryActorSlots = 15;
inline constexpr size_t kNativeBerryHistoryValues = kNativeSaveMaxBytes / 5;
struct NativeBerryHistoryRecord {
    uint32_t ownerPokemonId = 0;
    uint8_t actorSlot = 0;
    uint16_t offset = 0;
    uint16_t counts[3]{};
};
struct NativeBerryHistoryStore {
    bool resolved = false; // Legacy runs did not retain ordered Berry history.
    uint8_t recordCount = 0;
    uint16_t valueCount = 0;
    NativeBerryHistoryRecord records[kNativeBerryActorSlots]{};
    uint16_t values[kNativeBerryHistoryValues]{};
};

struct NativeRunSave {
    uint32_t generation = 0;
    // Zero denotes a legacy/unlinked run; otherwise load this exact profile generation.
    uint32_t starterProfileGeneration = 0;
    uint32_t eggProgressGeneration = 0; // v28; zero means no committed egg component.
    // Sorted Pokemon identity IDs; legacy history remains explicitly unknown.
    bool participantHistoryResolved = false;
    uint8_t participantCount = 0;
    uint32_t participantIds[6]{};
    uint16_t saveVersion = kNativeSaveVersion;
    uint16_t runtimeVersion = kNativeSaveRuntimeVersion;
    char contentHash[65]{};
    NativeSaveStage stage = NativeSaveStage::RunSetup;
    uint32_t seed = 0;
    uint16_t wave = 1;
    uint16_t starterDex = 0;
    uint8_t setupStarterCount = 0; // Zero retains the legacy single-starter setup.
    uint16_t setupStarterDexes[6]{};
    uint16_t playerLevel = 5;
    uint32_t playerExperience = 0;
    uint16_t encounterDex = 0;
    uint16_t playerHp = 0;
    uint16_t enemyHp = 0;
    PokemonStatusState playerStatus{};
    PokemonStatusState enemyStatus{};
    PokemonConfusionTagState playerConfusion{};
    PokemonConfusionTagState enemyConfusion{};
    PokemonBossState enemyBoss{}; // Run envelope v20; actor form remains reconstructed.
    bool playerHasEatenBerry = false;
    bool enemyHasEatenBerry = false;
    NativeAppearanceSave enemyAppearance{}; // Run envelope v27.
    uint8_t playerBerryCriticalBoostStages = 0;
    uint8_t enemyBerryCriticalBoostStages = 0;
    bool enemySturdyTag = false; // Player tags remain in explicit actor payloads.
    bool globalRngResolved = false; // v21; legacy snapshots cannot recover shield RNG history.
    PokerogueRngState globalRng{};
    bool doubleBattle = false; // v22: current field has one player and two enemies.
    NativePokemonSave secondEnemy{};
    PokemonBossState secondEnemyBoss{};
    uint8_t doubleExperienceGrantedMask = 0;
    uint8_t selectedTarget = 0;
    uint32_t battleTurn = 0;
    uint8_t playerMoveCount = 0;
    uint8_t enemyMoveCount = 0;
    uint16_t playerMoveIds[4]{};
    uint16_t enemyMoveIds[4]{};
    uint8_t playerPp[4]{};
    uint8_t enemyPp[4]{};
    int8_t playerStatStages[7]{};
    int8_t enemyStatStages[7]{};
    uint32_t enemySwitchCounter = 0;
    uint8_t weatherType = 0; // Pinned WeatherType ID; zero is NONE.
    uint16_t weatherTurnsLeft = 0;
    uint16_t weatherMaxDuration = 0;
    uint16_t trickRoomTurnsLeft = 0;
    uint16_t trickRoomMaxDuration = 0;
    uint16_t trickRoomSourceMoveId = 0;
    uint32_t trickRoomSourcePokemonId = 0;
    uint16_t trainerTypeId = 0;
    uint8_t trainerPartyCount = 0;
    uint8_t activeTrainerMember = 0xFF;
    NativeTrainerMemberSave trainerParty[6]{};
    // src/enums/pokeball.ts: IDs 0..4; unused LUXURY_BALL is not inventory.
    uint16_t pokeballCounts[5]{5, 0, 0, 0, 0};
    uint8_t playerPartyCount = 0; // Zero retains the legacy seed-replayed actor path.
    uint8_t activePlayerMember = 0xFF;
    NativePokemonSave playerParty[6]{};
    uint8_t heldModifierCount = 0;
    NativeHeldModifierInstance heldModifiers[kNativeHeldModifierCapacity]{};
    uint16_t persistentModifierCount = 0; // v23: ordered team-wide modifiers.
    NativePersistentModifierInstance persistentModifiers[kNativePersistentModifierCapacity]{};
    NativeBerryHistoryStore berryHistories{}; // Run envelope v26.
    char modeId[32]{};
    char biomeId[48]{};
};

enum class NativeSaveResult : uint8_t {
    Ok = 0, NotFound, InvalidFormat, UnsupportedVersion, IncompatibleRuntime,
    ContentMismatch, UnsupportedStage, InvalidRecord, TooLarge,
    ChecksumMismatch, IoError, SequenceExhausted, AmbiguousJournal, MemoryUnavailable
};

const char* nativeSaveResultName(NativeSaveResult result);
// Bounded member payload; enclosing run journal supplies version/hash/checksum.
struct NativeHeldModifierInstance;
// Component payload; enclosing run journal owns checksum/content hash/version.
NativeSaveResult encodeNativePersistentModifier(const NativePersistentModifierInstance& instance,
    char* output, size_t capacity, size_t& written);
NativeSaveResult decodeNativePersistentModifier(const char* bytes, size_t length,
    NativePersistentModifierInstance& output);
NativeSaveResult encodeNativeHeldModifier(const NativeHeldModifierInstance& instance,
    char* output, size_t capacity, size_t& written);
NativeSaveResult decodeNativeHeldModifier(const char* bytes, size_t length,
    NativeHeldModifierInstance& output);

struct PokemonBerryHistoryView;
NativeSaveResult captureNativeBerryHistory(NativeBerryHistoryStore& store,
    uint8_t actorSlot, const PokemonBerryHistoryView& history);
bool nativeBerryHistoryView(NativeBerryHistoryStore& store, size_t index,
    PokemonBerryHistoryView& output);
NativeSaveResult retainNativeBerryHistoryActors(NativeBerryHistoryStore& store, uint16_t actorSlotMask);
NativeSaveResult resetNativeBerrySummonHistory(NativeBerryHistoryStore& store, uint32_t ownerPokemonId);
NativeSaveResult removeNativeBerryHistoryOwner(NativeBerryHistoryStore& store, uint32_t ownerPokemonId);
NativeSaveResult transferNativeBerryHistoryToParty(NativeBerryHistoryStore& store, uint32_t ownerPokemonId, uint8_t partySlot);

// Versioned component; the enclosing run envelope owns content hash/checksum.
// Lists are variable length and keep order/duplicates. Caller supplies storage.
NativeSaveResult encodeNativeBerryHistory(const PokemonBerryHistoryView& history,
    char* output, size_t capacity, size_t& written);
NativeSaveResult decodeNativeBerryHistory(const char* bytes, size_t length,
    PokemonBerryHistoryView& output);
// Restore against the reconstructed actor before publishing caller-owned lists.
// A nonempty history requires its per-arena hasEatenBerry flag.
NativeSaveResult restoreNativePokemonBerryHistory(const char* bytes, size_t length,
    const PokemonBattleState& actor, PokemonBerryHistoryView& output);

NativeSaveResult encodeNativePokemonSave(const NativePokemonSave& saved, char* output,
    size_t capacity, size_t& written);
NativeSaveResult decodeNativePokemonSave(const char* bytes, size_t length,
    NativePokemonSave& output);


// Construct a bounded snapshot from canonical IDs. No UI or pointer data enters
// the file. Loading replays the supported setup deterministically from its seed.
NativeSaveResult makeNativeRunSetupSave(uint32_t seed, uint16_t starterDex,
                                      NativeRunSave& output);
NativeSaveResult validateNativeRunSave(const NativeRunSave& save, const char* expectedContentHash);
NativeSaveResult encodeNativeRunSave(const NativeRunSave& save, char* output,
                                    size_t capacity, size_t& written);
NativeSaveResult decodeNativeRunSave(const char* bytes, size_t length,
                                    const char* expectedContentHash, NativeRunSave& output);

// Journal logic has no filesystem dependency. The platform implementation must
// durably finish an inactive-slot write before reporting success. A failed write
// may damage that slot, but must never touch the other slot.
class NativeSaveStorage {
public:
    virtual ~NativeSaveStorage() = default;
    virtual NativeSaveResult readSlot(unsigned slot, char* output, size_t capacity, size_t& read) = 0;
    virtual NativeSaveResult writeSlot(unsigned slot, const char* bytes, size_t length) = 0;
    virtual NativeSaveResult deleteSlot(unsigned slot) { (void)slot; return NativeSaveResult::Ok; }
    virtual NativeSaveResult readExport(char* output, size_t capacity, size_t& read) = 0;
    virtual NativeSaveResult writeExport(const char* bytes, size_t length) = 0;
};

// Portable paired progress transport, independent of the authoritative journals.
class NativeProgressBundleStorage {
public:
    virtual ~NativeProgressBundleStorage() = default;
    virtual NativeSaveResult readBundle(char* output, size_t capacity, size_t& read) = 0;
    virtual NativeSaveResult writeBundle(const char* bytes, size_t length) = 0;
};

class NativeStarterCandyStore;
class NativeEggProgressStore;

class NativeRunSaveStore {
public:
    explicit NativeRunSaveStore(NativeSaveStorage& storage) : m_storage(storage) {}
    // Borrowed profile journal; host binds before SD/QuickJS operations.
    void bindStarterProfiles(NativeStarterCandyStore& profiles) { m_profiles = &profiles; }
    void bindEggProgress(NativeEggProgressStore& eggs) { m_eggs = &eggs; }
    NativeEggProgressStore* eggProgressStore() const { return m_eggs; }
    NativeSaveResult load(const char* contentHash, NativeRunSave& output);
    NativeSaveResult save(const NativeRunSave& value);
    NativeSaveResult deleteSave();
    NativeSaveResult exportLatest(const char* contentHash);
    NativeSaveResult importExport(const char* contentHash);

private:
    NativeSaveStorage& m_storage;
    NativeStarterCandyStore* m_profiles = nullptr;
    NativeEggProgressStore* m_eggs = nullptr;
};

// The production backend stores only these fixed files, without user-controlled
// paths. Paired .p3progress exports include the referenced starter profile.
class SdNativeSaveStorage final : public NativeSaveStorage, public NativeProgressBundleStorage {
public:
    static constexpr const char* kDirectory = "sdmc:/3ds/pokerogue/saves";
    static constexpr const char* kExportPath = "sdmc:/3ds/pokerogue/exports/progress.p3save";
    static constexpr const char* kBundlePath = "sdmc:/3ds/pokerogue/exports/progress.p3progress";
    NativeSaveResult readBundle(char* output, size_t capacity, size_t& read) override;
    NativeSaveResult writeBundle(const char* bytes, size_t length) override;
    NativeSaveResult readSlot(unsigned slot, char* output, size_t capacity, size_t& read) override;
    NativeSaveResult writeSlot(unsigned slot, const char* bytes, size_t length) override;
    NativeSaveResult deleteSlot(unsigned slot) override;
    NativeSaveResult readExport(char* output, size_t capacity, size_t& read) override;
    NativeSaveResult writeExport(const char* bytes, size_t length) override;
};

} // namespace Pokerogue3DS
