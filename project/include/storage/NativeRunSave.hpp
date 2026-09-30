#pragma once

#include <cstddef>
#include <cstdint>

namespace Pokerogue3DS {

inline constexpr uint16_t kNativeSaveVersion = 4;
inline constexpr uint16_t kNativeSaveRuntimeVersion = 4;
inline constexpr size_t kNativeSaveMaxBytes = 2048;

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
};

struct NativeRunSave {
    uint32_t generation = 0;
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
    uint16_t trainerTypeId = 0;
    uint8_t trainerPartyCount = 0;
    uint8_t activeTrainerMember = 0xFF;
    NativeTrainerMemberSave trainerParty[6]{};
    char modeId[32]{};
    char biomeId[48]{};
};

enum class NativeSaveResult : uint8_t {
    Ok = 0, NotFound, InvalidFormat, UnsupportedVersion, IncompatibleRuntime,
    ContentMismatch, UnsupportedStage, InvalidRecord, TooLarge,
    ChecksumMismatch, IoError, SequenceExhausted, AmbiguousJournal
};

const char* nativeSaveResultName(NativeSaveResult result);

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
