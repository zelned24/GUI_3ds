#pragma once

#include <cstddef>
#include <cstdint>
#include "game/PokerogueModifierReward.hpp"

namespace Pokerogue3DS {

inline constexpr uint16_t kNativeSaveVersion = 12;
inline constexpr uint16_t kNativeSaveRuntimeVersion = 12;
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

// Identity, form and IVs are reconstructed from the pinned seed. Mutable
// per-member state must survive switches and cannot be inferred from that seed.
struct NativeTrainerMemberSave {
    uint16_t speciesDex = 0;
    uint16_t hp = 0;
    uint8_t moveCount = 0;
    uint16_t moveIds[4]{};
    uint8_t pp[4]{};
    int8_t statStages[7]{};
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
    bool ivsDerivedFromId = false;
    bool pauseEvolutions = false;
    uint8_t maxPp[4]{};
    bool maxPpResolved = false;
    uint8_t friendship = 0;
    bool friendshipResolved = false; // Legacy payloads resolve to pinned species base.
    bool unburdenTag = false;
    bool actorIdentityResolved = false;
    uint8_t abilityIndex = 0;
    char initialTeraType[16]{}; // Empty only for legacy actor payloads.
    uint8_t initialTeraTypeIndex = 0;
    bool initialTeraTypeResolved = false;
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

struct NativeRunSave {
    uint32_t generation = 0;
    // Zero denotes a legacy/unlinked run; otherwise load this exact profile generation.
    uint32_t starterProfileGeneration = 0;
    uint16_t saveVersion = kNativeSaveVersion;
    uint16_t runtimeVersion = kNativeSaveRuntimeVersion;
    char contentHash[65]{};
    NativeSaveStage stage = NativeSaveStage::RunSetup;
    uint32_t seed = 0;
    uint16_t wave = 1;
    uint16_t starterDex = 0;
    uint16_t playerLevel = 5;
    uint32_t playerExperience = 0;
    uint16_t encounterDex = 0;
    uint16_t playerHp = 0;
    uint16_t enemyHp = 0;
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
    char modeId[32]{};
    char biomeId[48]{};
};

enum class NativeSaveResult : uint8_t {
    Ok = 0, NotFound, InvalidFormat, UnsupportedVersion, IncompatibleRuntime,
    ContentMismatch, UnsupportedStage, InvalidRecord, TooLarge,
    ChecksumMismatch, IoError, SequenceExhausted, AmbiguousJournal
};

const char* nativeSaveResultName(NativeSaveResult result);
// Bounded member payload; enclosing run journal supplies version/hash/checksum.
struct NativeHeldModifierInstance;
// Component payload; enclosing run journal owns checksum/content hash/version.
NativeSaveResult encodeNativeHeldModifier(const NativeHeldModifierInstance& instance,
    char* output, size_t capacity, size_t& written);
NativeSaveResult decodeNativeHeldModifier(const char* bytes, size_t length,
    NativeHeldModifierInstance& output);

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
    virtual NativeSaveResult readExport(char* output, size_t capacity, size_t& read) = 0;
    virtual NativeSaveResult writeExport(const char* bytes, size_t length) = 0;
};

class NativeRunSaveStore {
public:
    explicit NativeRunSaveStore(NativeSaveStorage& storage) : m_storage(storage) {}
    NativeSaveResult load(const char* contentHash, NativeRunSave& output);
    NativeSaveResult save(const NativeRunSave& value);
    NativeSaveResult exportLatest(const char* contentHash);
    NativeSaveResult importExport(const char* contentHash);

private:
    NativeSaveStorage& m_storage;
};

// The production backend stores only these fixed files, without user-controlled
// paths. Export is a portable UTF-8 .p3save file copied from the SD to another 3DS.
class SdNativeSaveStorage final : public NativeSaveStorage {
public:
    static constexpr const char* kDirectory = "sdmc:/3ds/pokerogue/saves";
    static constexpr const char* kExportPath = "sdmc:/3ds/pokerogue/exports/progress.p3save";
    NativeSaveResult readSlot(unsigned slot, char* output, size_t capacity, size_t& read) override;
    NativeSaveResult writeSlot(unsigned slot, const char* bytes, size_t length) override;
    NativeSaveResult readExport(char* output, size_t capacity, size_t& read) override;
    NativeSaveResult writeExport(const char* bytes, size_t length) override;
};

} // namespace Pokerogue3DS
