#include "storage/NativeRunSave.hpp"
#include "storage/NativeStarterCandyProfile.hpp"
#include "storage/NativeStarterCandyStore.hpp"
#include "storage/NativeProgressStore.hpp"
#include "storage/NativeProgressBundle.hpp"
#include "storage/IntegritySha256.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cstring>

// Freestanding WASM harness: fixtures live on the stack; accidental dynamic
// destruction is a test failure rather than an unimplemented allocator call.
void operator delete(void*, size_t) noexcept { __builtin_trap(); }
extern "C" int memcmp(const void* a, const void* b, size_t size) {
    const auto* left = static_cast<const unsigned char*>(a);
    const auto* right = static_cast<const unsigned char*>(b);
    for (size_t i = 0; i < size; ++i) if (left[i] != right[i]) return left[i] < right[i] ? -1 : 1;
    return 0;
}

namespace {
using namespace Pokerogue3DS;
class MemoryStorage : public NativeSaveStorage {
public:
    char slots[2][kNativeSaveMaxBytes]{};
    size_t sizes[2]{};
    char exported[kNativeSaveMaxBytes]{};
    size_t exportSize = 0;
    bool interrupt = false;
    NativeSaveResult readSlot(unsigned i, char* out, size_t cap, size_t& size) override {
        size = sizes[i];
        if (!size) return NativeSaveResult::NotFound;
        if (size > cap) return NativeSaveResult::TooLarge;
        std::memcpy(out, slots[i], size); return NativeSaveResult::Ok;
    }
    NativeSaveResult writeSlot(unsigned i, const char* in, size_t size) override {
        sizes[i] = interrupt ? size / 2 : size;
        std::memcpy(slots[i], in, sizes[i]);
        return interrupt ? NativeSaveResult::IoError : NativeSaveResult::Ok;
    }
    NativeSaveResult readExport(char* out, size_t cap, size_t& size) override {
        size = exportSize;
        if (!size) return NativeSaveResult::NotFound;
        if (size > cap) return NativeSaveResult::TooLarge;
        std::memcpy(out, exported, size); return NativeSaveResult::Ok;
    }
    NativeSaveResult writeExport(const char* in, size_t size) override {
        exportSize = size; std::memcpy(exported, in, size); return NativeSaveResult::Ok;
    }
};
class MemoryBundleStorage : public NativeProgressBundleStorage {
public:
    char bytes[kNativeProgressBundleMaxBytes]{};
    size_t size = 0;
    bool interrupt = false;
    NativeSaveResult readBundle(char* out, size_t capacity, size_t& read) override {
        read = 0;
        if (!size) return NativeSaveResult::NotFound;
        if (size > capacity) return NativeSaveResult::TooLarge;
        std::memcpy(out, bytes, size); read = size; return NativeSaveResult::Ok;
    }
    NativeSaveResult writeBundle(const char* input, size_t length) override {
        if (length > sizeof(bytes)) return NativeSaveResult::TooLarge;
        size = interrupt ? length / 2 : length;
        std::memcpy(bytes, input, size);
        return interrupt ? NativeSaveResult::IoError : NativeSaveResult::Ok;
    }
};
}
extern "C" int runNativeSaveChecks() {
    char digest[65];
    IntegritySha256::hashHex("abc", 3, digest);
    if (std::strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0) return 1;
    IntegritySha256 streaming;
    const char* multiBlock = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    streaming.update(multiBlock, 13);
    uint8_t intermediate[32]; streaming.finish(intermediate);
    streaming.update(multiBlock + 13, 43);
    streaming.finish(intermediate); IntegritySha256::toHex(intermediate, digest);
    if (std::strcmp(digest, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") != 0) return 11;
    MemoryStorage disk; NativeRunSaveStore store(disk); NativeRunSave original{}, restored{};
    if (makeNativeRunSetupSave(123, 1, original) != NativeSaveResult::Ok) return 2;
    uint16_t nonProfileStarterDex = 0;
    for (std::size_t i = 0; i < PokerogueContent::kSpeciesCount; ++i) {
        const auto& species = PokerogueContent::kSpecies[i];
        if (species.starterEligible && !species.freshProfileStarter) {
            nonProfileStarterDex = species.dex;
            break;
        }
    }
    // Codec validates canonical eligibility. Runtime regressions 618–620
    // separately require caught-profile authorization for non-default starters.
    NativeRunSave unlockedStarter{};
    if (!nonProfileStarterDex || makeNativeRunSetupSave(123, nonProfileStarterDex, unlockedStarter) !=
            NativeSaveResult::Ok || unlockedStarter.starterDex != nonProfileStarterDex ||
        validateNativeRunSave(unlockedStarter, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 12;
    uint16_t ineligibleStarterDex = 0;
    for (const auto& species : PokerogueContent::kSpecies)
        if (!species.starterEligible) { ineligibleStarterDex = species.dex; break; }
    NativeRunSave invalidStarter{};
    if (!ineligibleStarterDex || makeNativeRunSetupSave(123, ineligibleStarterDex, invalidStarter) !=
            NativeSaveResult::InvalidRecord) return 115;
    if (store.save(original) != NativeSaveResult::Ok || store.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok || restored.seed != 123) return 3;
    original.seed = 456; disk.interrupt = true;
    if (store.save(original) != NativeSaveResult::IoError) return 4;
    if (store.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok || restored.seed != 123) return 5;
    disk.interrupt = false;
    if (store.save(original) != NativeSaveResult::Ok || store.exportLatest(PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 6;
    MemoryStorage other; NativeRunSaveStore imported(other);
    other.writeExport(disk.exported, disk.exportSize);
    if (imported.importExport(PokerogueContent::kContentHash) != NativeSaveResult::Ok || imported.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok || restored.seed != 456) return 7;
    other.exported[other.exportSize / 2] ^= 1;
    if (imported.importExport(PokerogueContent::kContentHash) == NativeSaveResult::Ok) return 8;
    if (imported.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok || restored.seed != 456) return 9;
    char wrongHash[65]; std::memcpy(wrongHash, PokerogueContent::kContentHash, 65); wrongHash[0] = wrongHash[0] == '0' ? '1' : '0';
    if (imported.load(wrongHash, restored) != NativeSaveResult::ContentMismatch) return 10;
    NativeRunSave trainerSave = original;
    trainerSave.wave = 5;
    trainerSave.stage = NativeSaveStage::BattleActive;
    trainerSave.battleTurn = 1;
    trainerSave.playerHp = 20;
    trainerSave.encounterDex = 19;
    trainerSave.enemyHp = 12;
    trainerSave.playerMoveCount = trainerSave.enemyMoveCount = 1;
    trainerSave.playerMoveIds[0] = trainerSave.enemyMoveIds[0] = 33;
    trainerSave.playerPp[0] = trainerSave.enemyPp[0] = 25;
    trainerSave.playerStatStages[0] = -6;
    trainerSave.enemyStatStages[4] = 6;
    trainerSave.enemySwitchCounter = 2;
    trainerSave.trainerTypeId = 63;
    trainerSave.trainerPartyCount = 2;
    trainerSave.activeTrainerMember = 0;
    trainerSave.trainerParty[0] = {19, 12, 1, {33, 0, 0, 0}, {25, 0, 0, 0}};
    trainerSave.trainerParty[1] = {504, 9, 1, {33, 0, 0, 0}, {17, 0, 0, 0}};
    trainerSave.trainerParty[0].statStages[4] = 6;
    trainerSave.trainerParty[1].statStages[5] = -2;
    trainerSave.weatherType = 2;
    trainerSave.weatherTurnsLeft = 3;
    trainerSave.weatherMaxDuration = 5;
    char partyBytes[kNativeSaveMaxBytes]{};
    size_t partySize = 0;
    if (encodeNativeRunSave(trainerSave, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, partySize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.weatherType != 2 || restored.weatherTurnsLeft != 3 || restored.weatherMaxDuration != 5 ||
        restored.playerStatStages[0] != -6 || restored.enemyStatStages[4] != 6 ||
        restored.trainerParty[1].statStages[5] != -2 || restored.enemySwitchCounter != 2 || restored.trainerPartyCount != 2 || restored.trainerParty[1].hp != 9 ||
        restored.trainerParty[1].pp[0] != 17 || restored.activeTrainerMember != 0) return 13;
    NativeRunSave roomSave = trainerSave;
    roomSave.trickRoomTurnsLeft = 3;
    roomSave.trickRoomMaxDuration = 5;
    roomSave.trickRoomSourceMoveId = 433;
    roomSave.trickRoomSourcePokemonId = 123;
    char roomBytes[kNativeSaveMaxBytes]{};
    size_t roomSize = 0;
    if (encodeNativeRunSave(roomSave, roomBytes, sizeof(roomBytes), roomSize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(roomBytes, roomSize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.trickRoomTurnsLeft != 3 || restored.trickRoomMaxDuration != 5 ||
        restored.trickRoomSourceMoveId != 433 || restored.trickRoomSourcePokemonId != 123) return 38;
    roomSave.trickRoomSourceMoveId = 33;
    if (validateNativeRunSave(roomSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 39;
    char versionSevenBytes[kNativeSaveMaxBytes]{};
    std::memcpy(versionSevenBytes, roomBytes, roomSize);
    size_t versionSevenSize = 0;
    for (size_t n = 0; n < roomSize; ++n) {
        if (n + 18 <= roomSize && std::memcmp(versionSevenBytes + n, "trickRoomTurnsLeft=", 18) == 0) {
            versionSevenSize = n; break;
        }
        if (n + 16 <= roomSize && std::memcmp(versionSevenBytes + n, "saveVersion=000d", 16) == 0)
            versionSevenBytes[n + 15] = '7';
        if (n + 19 <= roomSize && std::memcmp(versionSevenBytes + n, "runtimeVersion=000d", 19) == 0)
            versionSevenBytes[n + 18] = '7';
    }
    if (!versionSevenSize) return 40;
    IntegritySha256::hashHex(versionSevenBytes, versionSevenSize, digest);
    std::memcpy(versionSevenBytes + versionSevenSize, "sha256=", 7);
    std::memcpy(versionSevenBytes + versionSevenSize + 7, digest, 64);
    versionSevenBytes[versionSevenSize + 71] = '\n';
    if (decodeNativeRunSave(versionSevenBytes, versionSevenSize + 72, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.saveVersion != kNativeSaveVersion || restored.weatherType != 2 ||
        restored.trickRoomTurnsLeft || restored.trickRoomMaxDuration ||
        restored.trickRoomSourceMoveId || restored.trickRoomSourcePokemonId) return 41;
    NativeRunSave invalidWeather = trainerSave;
    invalidWeather.weatherTurnsLeft = 6;
    if (validateNativeRunSave(invalidWeather, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 34;
    invalidWeather = trainerSave;
    invalidWeather.weatherType = 7; // Immutable weather cannot carry a timer.
    if (validateNativeRunSave(invalidWeather, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 35;
    char versionSixBytes[kNativeSaveMaxBytes]{};
    std::memcpy(versionSixBytes, partyBytes, partySize);
    size_t versionSixSize = 0;
    for (size_t n = 0; n < partySize; ++n) {
        if (n + 12 <= partySize && std::memcmp(versionSixBytes + n, "weatherType=", 12) == 0) {
            versionSixSize = n; break;
        }
        if (n + 16 <= partySize && std::memcmp(versionSixBytes + n, "saveVersion=000d", 16) == 0)
            versionSixBytes[n + 15] = '6';
        if (n + 19 <= partySize && std::memcmp(versionSixBytes + n, "runtimeVersion=000d", 19) == 0)
            versionSixBytes[n + 18] = '6';
    }
    if (!versionSixSize) return 36;
    IntegritySha256::hashHex(versionSixBytes, versionSixSize, digest);
    std::memcpy(versionSixBytes + versionSixSize, "sha256=", 7);
    std::memcpy(versionSixBytes + versionSixSize + 7, digest, 64);
    versionSixBytes[versionSixSize + 71] = '\n';
    if (decodeNativeRunSave(versionSixBytes, versionSixSize + 72, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.saveVersion != kNativeSaveVersion || restored.weatherType || restored.weatherTurnsLeft ||
        restored.weatherMaxDuration || restored.playerStatStages[0] != -6) return 37;
    char legacyTrainerBytes[kNativeSaveMaxBytes]{};
    size_t legacyTrainerSize = 0;
    const size_t trainerBodyEnd = partySize - 72;
    for (size_t n = 0; n < trainerBodyEnd;) {
        if (n + 13 <= trainerBodyEnd && std::memcmp(partyBytes + n, "playerStages=", 13) == 0) break;
        if (n + 28 <= trainerBodyEnd &&
            std::memcmp(partyBytes + n, "enemySwitchCounter=", 19) == 0) { n += 28; continue; }
        legacyTrainerBytes[legacyTrainerSize++] = partyBytes[n++];
    }
    for (size_t n = 0; n < legacyTrainerSize; ++n) {
        if (n + 16 <= legacyTrainerSize && std::memcmp(legacyTrainerBytes + n, "saveVersion=000d", 16) == 0)
            legacyTrainerBytes[n + 15] = '4';
        if (n + 19 <= legacyTrainerSize && std::memcmp(legacyTrainerBytes + n, "runtimeVersion=000d", 19) == 0)
            legacyTrainerBytes[n + 18] = '4';
    }
    IntegritySha256::hashHex(legacyTrainerBytes, legacyTrainerSize, digest);
    std::memcpy(legacyTrainerBytes + legacyTrainerSize, "sha256=", 7);
    std::memcpy(legacyTrainerBytes + legacyTrainerSize + 7, digest, 64);
    legacyTrainerBytes[legacyTrainerSize + 71] = '\n';
    if (decodeNativeRunSave(legacyTrainerBytes, legacyTrainerSize + 72,
            PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.saveVersion != kNativeSaveVersion || restored.enemySwitchCounter ||
        restored.trainerPartyCount != 2 || restored.trainerParty[1].pp[0] != 17) return 22;
    trainerSave.trainerPartyCount = 6;
    for (uint8_t member = 2; member < 6; ++member)
        trainerSave.trainerParty[member] = trainerSave.trainerParty[1];
    if (encodeNativeRunSave(trainerSave, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, partySize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.trainerPartyCount != 6 || restored.trainerParty[5].pp[0] != 17) return 21;
    trainerSave.trainerPartyCount = 2;
    for (uint8_t member = 2; member < 6; ++member) trainerSave.trainerParty[member] = {};
    trainerSave.activeTrainerMember = 2;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 14;
    trainerSave.activeTrainerMember = 0;
    trainerSave.trainerParty[0].hp = 11;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 15;
    trainerSave.trainerParty[0].hp = trainerSave.enemyHp = 0;
    trainerSave.stage = NativeSaveStage::BattleWon;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 16;
    trainerSave.trainerParty[1].hp = 0;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 17;
    if (encodeNativeRunSave(original, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok) return 18;
    size_t legacyBodySize = 0;
    for (size_t n = 0; n < partySize; ++n) {
        if (n + 19 <= partySize && std::memcmp(partyBytes + n, "enemySwitchCounter=", 19) == 0) {
            legacyBodySize = n; break;
        }
        if (n + 16 <= partySize && std::memcmp(partyBytes + n, "saveVersion=000d", 16) == 0)
            partyBytes[n + 15] = '3';
        if (n + 19 <= partySize && std::memcmp(partyBytes + n, "runtimeVersion=000d", 19) == 0)
            partyBytes[n + 18] = '3';
    }
    if (!legacyBodySize) return 19;
    IntegritySha256::hashHex(partyBytes, legacyBodySize, digest);
    std::memcpy(partyBytes + legacyBodySize, "sha256=", 7);
    std::memcpy(partyBytes + legacyBodySize + 7, digest, 64);
    partyBytes[legacyBodySize + 71] = '\n';
    if (decodeNativeRunSave(partyBytes, legacyBodySize + 72, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.saveVersion != kNativeSaveVersion || restored.runtimeVersion != kNativeSaveRuntimeVersion ||
        restored.seed != original.seed || restored.trainerPartyCount || restored.activeTrainerMember != 0xFF) return 20;
    NativeRunSave inventorySave = trainerSave;
    inventorySave.pokeballCounts[0] = 0;
    inventorySave.pokeballCounts[1] = 3;
    inventorySave.pokeballCounts[2] = 99;
    inventorySave.pokeballCounts[3] = 1;
    inventorySave.pokeballCounts[4] = 2;
    if (encodeNativeRunSave(inventorySave, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, partySize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok)
        return 40;
    for (uint8_t ball = 0; ball < 5; ++ball)
        if (inventorySave.pokeballCounts[ball] != restored.pokeballCounts[ball]) return 41;
    inventorySave.pokeballCounts[2] = 100;
    if (validateNativeRunSave(inventorySave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 42;
    // v8 has Trick Room but no inventory; its supported production stock was initial.
    char versionEightBytes[kNativeSaveMaxBytes]{};
    std::memcpy(versionEightBytes, roomBytes, roomSize);
    size_t versionEightSize = 0;
    for (size_t n = 0; n < roomSize; ++n) {
        if (n + 14 <= roomSize && std::memcmp(versionEightBytes + n, "pokeballCount=", 14) == 0) {
            versionEightSize = n; break;
        }
        if (n + 16 <= roomSize && std::memcmp(versionEightBytes + n, "saveVersion=000d", 16) == 0)
            versionEightBytes[n + 15] = '8';
        if (n + 19 <= roomSize && std::memcmp(versionEightBytes + n, "runtimeVersion=000d", 19) == 0)
            versionEightBytes[n + 18] = '8';
    }
    if (!versionEightSize) return 43;
    IntegritySha256::hashHex(versionEightBytes, versionEightSize, digest);
    std::memcpy(versionEightBytes + versionEightSize, "sha256=", 7);
    std::memcpy(versionEightBytes + versionEightSize + 7, digest, 64);
    versionEightBytes[versionEightSize + 71] = '\n';
    if (decodeNativeRunSave(versionEightBytes, versionEightSize + 72, PokerogueContent::kContentHash,
            restored) != NativeSaveResult::Ok || restored.saveVersion != kNativeSaveVersion ||
        restored.trickRoomTurnsLeft != 3 || restored.pokeballCounts[0] != 5) return 44;
    for (uint8_t ball = 1; ball < 5; ++ball) if (restored.pokeballCounts[ball]) return 45;
    NativeRunSave invalidSetup = original;
    invalidSetup.pokeballCounts[1] = 1;
    if (validateNativeRunSave(invalidSetup, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 46;
    NativeStarterCandyRecord candyRecords[] = {{1, 7, 24}, {4, 8, 12}};
    char candyBytes[256]{};
    size_t candySize = 0;
    if (encodeNativeStarterCandyProfile(candyRecords, 2, 1, PokerogueContent::kContentHash,
            9999, candyBytes, sizeof(candyBytes), candySize) != NativeSaveResult::Ok) return 47;
    NativeStarterCandyRecord restoredCandy[2]{};
    size_t candyCount = 0;
    uint32_t candyGeneration = 0;
    if (decodeNativeStarterCandyProfile(candyBytes, candySize, PokerogueContent::kContentHash, 9999,
            restoredCandy, 2, candyCount, candyGeneration) != NativeSaveResult::Ok || candyCount != 2 ||
        candyGeneration != 1 || restoredCandy[0].candyCount != 7 || restoredCandy[1].friendship != 12)
        return 48;
    candyBytes[80] ^= 1;
    if (decodeNativeStarterCandyProfile(candyBytes, candySize, PokerogueContent::kContentHash, 9999,
            restoredCandy, 2, candyCount, candyGeneration) != NativeSaveResult::ChecksumMismatch ||
        restoredCandy[0].candyCount != 7 || candyCount != 2) return 49;
    candyRecords[1].speciesDex = 1;
    if (encodeNativeStarterCandyProfile(candyRecords, 2, 1, PokerogueContent::kContentHash, 9999,
            candyBytes, sizeof(candyBytes), candySize) != NativeSaveResult::InvalidRecord || candySize) return 50;
    static char profileScratch[2 * kStarterCandyProfileMaxBytes]{};
    other.sizes[0] = other.sizes[1] = 0;
    other.interrupt = false;
    NativeStarterCandyStore candyStore(other, profileScratch, sizeof(profileScratch));
    candyRecords[1].speciesDex = 4;
    if (candyStore.save(candyRecords, 2, PokerogueContent::kContentHash, 9999, candyGeneration) !=
            NativeSaveResult::Ok || candyGeneration != 1) return 51;
    candyRecords[0].candyCount = 10;
    other.interrupt = true;
    if (candyStore.save(candyRecords, 2, PokerogueContent::kContentHash, 9999, candyGeneration) !=
            NativeSaveResult::IoError || candyGeneration != 1) return 52;
    if (candyStore.load(PokerogueContent::kContentHash, 9999, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || restoredCandy[0].candyCount != 7 || candyGeneration != 1) return 53;
    other.interrupt = false;
    if (candyStore.save(candyRecords, 2, PokerogueContent::kContentHash, 9999, candyGeneration) !=
            NativeSaveResult::Ok || candyGeneration != 2 ||
        candyStore.exportLatest(PokerogueContent::kContentHash, 9999) != NativeSaveResult::Ok ||
        decodeNativeStarterCandyProfile(other.exported, other.exportSize, PokerogueContent::kContentHash,
            9999, restoredCandy, 2, candyCount, candyGeneration) != NativeSaveResult::Ok ||
        restoredCandy[0].candyCount != 10) return 54;
    other.slots[1][80] ^= 1;
    if (candyStore.load(PokerogueContent::kContentHash, 9999, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || restoredCandy[0].candyCount != 7 || candyGeneration != 1) return 55;
    if (encodeNativeStarterCandyProfile(candyRecords, 2, 1, PokerogueContent::kContentHash, 9999,
            other.slots[1], sizeof(other.slots[1]), other.sizes[1]) != NativeSaveResult::Ok ||
        candyStore.load(PokerogueContent::kContentHash, 9999, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::AmbiguousJournal || restoredCandy[0].candyCount != 7) return 56;
    const auto* candySpecies = PokerogueContent::findSpeciesByDex(1);
    if (!candySpecies || PokerogueContent::kMaxStarterCandyCount != 9999 ||
        PokerogueContent::kClassicCandyFriendshipMultiplier != 3) return 57;
    uint32_t rootCap = 0;
    for (const auto& entry : PokerogueContent::kStarterCandyFriendshipCaps)
        if (entry.cost == candySpecies->starterCost) rootCap = entry.value;
    if (!rootCap) return 58;
    NativeStarterCandyRecord awarding{1, 7, rootCap - 1};
    StarterCandyAwardEvent award{};
    if (applyNativeStarterCandyFriendship(awarding, rootCap + 2, award) != StarterCandyApplyResult::Applied ||
        awarding.candyCount != 9 || awarding.friendship != 1 || award.requestedAward != 2 ||
        award.appliedAward != 2) return 59;
    awarding.candyCount = PokerogueContent::kMaxStarterCandyCount;
    if (applyNativeStarterCandyFriendship(awarding, rootCap, award) != StarterCandyApplyResult::Applied ||
        awarding.friendship != rootCap - 1 || award.requestedAward || award.appliedAward) return 60;
    awarding.candyCount = PokerogueContent::kMaxStarterCandyCount - 1;
    if (applyNativeStarterCandyFriendship(awarding, rootCap * 3, award) != StarterCandyApplyResult::Applied ||
        awarding.candyCount != PokerogueContent::kMaxStarterCandyCount || award.requestedAward != 3 ||
        award.appliedAward != 1) return 61;
    awarding.friendship = 0xffffffffU;
    if (applyNativeStarterCandyFriendship(awarding, 1, award) != StarterCandyApplyResult::Overflow ||
        awarding.friendship != 0xffffffffU || award.appliedAward != 1) return 62;
    other.sizes[0] = other.sizes[1] = 0;
    if (candyStore.save(candyRecords, 2, PokerogueContent::kContentHash, candyGeneration) != NativeSaveResult::Ok ||
        candyStore.load(PokerogueContent::kContentHash, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || restoredCandy[0].candyCount != 10 ||
        candyStore.exportLatest(PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 63;
    disk.sizes[0] = disk.sizes[1] = 0;
    disk.interrupt = false;
    disk.exportSize = other.exportSize;
    std::memcpy(disk.exported, other.exported, disk.exportSize);
    NativeStarterCandyStore profileImporter(disk, profileScratch, sizeof(profileScratch));
    size_t importedCandyCount = 0;
    uint32_t importedCandyGeneration = 0;
    NativeStarterCandyRecord importStaging[2]{};
    if (profileImporter.importExport(PokerogueContent::kContentHash, importStaging, 2,
            importedCandyCount, importedCandyGeneration) != NativeSaveResult::Ok || importedCandyCount != 2 ||
        importedCandyGeneration != 1 ||
        profileImporter.load(PokerogueContent::kContentHash, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || restoredCandy[0].candyCount != 10) return 64;
    if (profileImporter.importExport(PokerogueContent::kContentHash, importStaging, 2,
            importedCandyCount, importedCandyGeneration) != NativeSaveResult::Ok || importedCandyGeneration != 2)
        return 65;
    disk.exported[80] ^= 1;
    if (profileImporter.importExport(PokerogueContent::kContentHash, importStaging, 2,
            importedCandyCount, importedCandyGeneration) != NativeSaveResult::ChecksumMismatch ||
        importedCandyGeneration != 2 || importedCandyCount != 2 ||
        profileImporter.load(PokerogueContent::kContentHash, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || candyGeneration != 2 || restoredCandy[0].candyCount != 10) return 66;
    other.sizes[0] = other.sizes[1] = 0;
    candyRecords[0].candyCount = 7;
    if (candyStore.save(candyRecords, 2, PokerogueContent::kContentHash, candyGeneration) != NativeSaveResult::Ok ||
        candyGeneration != 1) return 67;
    candyRecords[0].candyCount = 10;
    uint32_t preparedGeneration = 0;
    if (candyStore.prepareFromCommitted(candyRecords, 2, PokerogueContent::kContentHash, 1,
            preparedGeneration) != NativeSaveResult::Ok || preparedGeneration != 2) return 68;
    // Simulate power loss before the run stores its new profile generation.
    if (candyStore.loadGeneration(PokerogueContent::kContentHash, 1, restoredCandy, 2,
            candyCount, candyGeneration) != NativeSaveResult::Ok || restoredCandy[0].candyCount != 7)
        return 69;
    if (candyStore.prepareFromCommitted(candyRecords, 2, PokerogueContent::kContentHash, 1,
            preparedGeneration) != NativeSaveResult::Ok || preparedGeneration != 3 ||
        candyStore.loadGeneration(PokerogueContent::kContentHash, 1, restoredCandy, 2,
            candyCount, candyGeneration) != NativeSaveResult::Ok || restoredCandy[0].candyCount != 7)
        return 70;
    if (candyStore.loadGeneration(PokerogueContent::kContentHash, 3, restoredCandy, 2,
            candyCount, candyGeneration) != NativeSaveResult::Ok || restoredCandy[0].candyCount != 10 ||
        candyGeneration != 3) return 71;
    if (candyStore.loadGeneration(PokerogueContent::kContentHash, 2, restoredCandy, 2,
            candyCount, candyGeneration) != NativeSaveResult::NotFound || candyGeneration != 3 ||
        restoredCandy[0].candyCount != 10) return 72;
    original.starterProfileGeneration = 3;
    size_t linkedSize = 0;
    if (encodeNativeRunSave(original, partyBytes, sizeof(partyBytes), linkedSize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, linkedSize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.starterProfileGeneration != 3) return 73;
    size_t oldPayloadSize = 0;
    for (size_t n = 0; n < linkedSize; ++n) {
        if (n + 25 <= linkedSize && std::memcmp(partyBytes + n, "starterProfileGeneration=", 25) == 0) {
            oldPayloadSize = n; break;
        }
    }
    if (!oldPayloadSize) return 74;
    for (size_t n = 0; n < oldPayloadSize; ++n) {
        if (n + 16 <= oldPayloadSize && std::memcmp(partyBytes + n, "saveVersion=000d", 16) == 0)
            partyBytes[n + 15] = 'b';
        if (n + 19 <= oldPayloadSize && std::memcmp(partyBytes + n, "runtimeVersion=000d", 19) == 0)
            partyBytes[n + 18] = 'b';
    }
    IntegritySha256::hashHex(partyBytes, oldPayloadSize, digest);
    std::memcpy(partyBytes + oldPayloadSize, "sha256=", 7);
    std::memcpy(partyBytes + oldPayloadSize + 7, digest, 64);
    partyBytes[oldPayloadSize + 71] = '\n';
    if (decodeNativeRunSave(partyBytes, oldPayloadSize + 72, PokerogueContent::kContentHash, restored) !=
            NativeSaveResult::Ok || restored.starterProfileGeneration != 0 ||
        restored.saveVersion != kNativeSaveVersion || restored.runtimeVersion != kNativeSaveRuntimeVersion)
        return 75;
    disk.sizes[0] = disk.sizes[1] = 0;
    other.sizes[0] = other.sizes[1] = 0;
    NativeProgressStore progress(store, candyStore);
    NativeRunSave committedRun{};
    if (makeNativeRunSetupSave(123, 1, committedRun) != NativeSaveResult::Ok) return 76;
    candyRecords[0].candyCount = 7;
    if (progress.commit(committedRun, candyRecords, 2) != NativeSaveResult::Ok ||
        committedRun.starterProfileGeneration != 1 || committedRun.generation != 1) return 77;
    candyRecords[0].candyCount = 10;
    disk.interrupt = true;
    if (progress.commit(committedRun, candyRecords, 2) != NativeSaveResult::IoError ||
        committedRun.starterProfileGeneration != 1) return 78;
    disk.interrupt = false;
    if (progress.load(PokerogueContent::kContentHash, restored, restoredCandy, 2, candyCount) !=
            NativeSaveResult::Ok || restored.starterProfileGeneration != 1 ||
        restoredCandy[0].candyCount != 7) return 79;
    if (progress.commit(committedRun, candyRecords, 2) != NativeSaveResult::Ok ||
        committedRun.starterProfileGeneration != 3 ||
        progress.load(PokerogueContent::kContentHash, restored, restoredCandy, 2, candyCount) !=
            NativeSaveResult::Ok || restoredCandy[0].candyCount != 10) return 80;
    NativeRunSave staleRun = committedRun;
    staleRun.starterProfileGeneration = 1;
    if (progress.commit(staleRun, candyRecords, 2) != NativeSaveResult::InvalidRecord) return 81;
    other.interrupt = true;
    candyRecords[0].candyCount = 11;
    if (progress.commit(committedRun, candyRecords, 2) != NativeSaveResult::IoError) return 82;
    other.interrupt = false;
    if (progress.load(PokerogueContent::kContentHash, restored, restoredCandy, 2, candyCount) !=
            NativeSaveResult::Ok || restored.starterProfileGeneration != 3 ||
        restoredCandy[0].candyCount != 10) return 83;
    candyRecords[0].candyCount = 12;
    if (candyStore.prepareFromCommitted(candyRecords, 2, PokerogueContent::kContentHash, 3,
            preparedGeneration) != NativeSaveResult::Ok ||
        candyStore.exportGeneration(PokerogueContent::kContentHash, 3) != NativeSaveResult::Ok) return 84;
    if (decodeNativeStarterCandyProfile(other.exported, other.exportSize, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, restoredCandy, 2, candyCount, candyGeneration) !=
            NativeSaveResult::Ok || candyGeneration != 3 || restoredCandy[0].candyCount != 10) return 85;
    if (candyStore.exportGeneration(PokerogueContent::kContentHash, 2) != NativeSaveResult::NotFound)
        return 86;
    if (candyStore.inspectGeneration(PokerogueContent::kContentHash, 3) != NativeSaveResult::Ok ||
        candyStore.inspectGeneration(PokerogueContent::kContentHash, 2) != NativeSaveResult::NotFound)
        return 87;
    const uint32_t unchangedSeed = restored.seed;
    other.sizes[0] = other.sizes[1] = 0;
    if (store.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::InvalidRecord ||
        restored.seed != unchangedSeed ||
        store.save(committedRun) != NativeSaveResult::InvalidRecord) return 88;
    NativeRunSaveStore unboundStore(disk);
    if (unboundStore.load(PokerogueContent::kContentHash, restored) != NativeSaveResult::InvalidRecord)
        return 89;
    trainerSave.participantHistoryResolved = true;
    trainerSave.participantCount = 2;
    trainerSave.participantIds[0] = 0;
    trainerSave.participantIds[1] = 123;
    size_t historySize = 0;
    if (encodeNativeRunSave(trainerSave, partyBytes, sizeof(partyBytes), historySize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, historySize, PokerogueContent::kContentHash, restored) !=
            NativeSaveResult::Ok || !restored.participantHistoryResolved || restored.participantCount != 2 ||
        restored.participantIds[0] != 0 || restored.participantIds[1] != 123) return 90;
    trainerSave.participantIds[1] = 0;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 91;
    trainerSave.participantIds[1] = 123;
    trainerSave.participantHistoryResolved = false;
    if (validateNativeRunSave(trainerSave, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 92;
    size_t profilePayloadSize = 0;
    for (size_t n = 0; n < historySize; ++n) {
        if (n + 27 <= historySize && std::memcmp(partyBytes + n, "participantHistoryResolved=", 27) == 0) {
            profilePayloadSize = n; break;
        }
        if (n + 16 <= historySize && std::memcmp(partyBytes + n, "saveVersion=000d", 16) == 0)
            partyBytes[n + 15] = 'c';
        if (n + 19 <= historySize && std::memcmp(partyBytes + n, "runtimeVersion=000d", 19) == 0)
            partyBytes[n + 18] = 'c';
    }
    if (!profilePayloadSize) return 93;
    IntegritySha256::hashHex(partyBytes, profilePayloadSize, digest);
    std::memcpy(partyBytes + profilePayloadSize, "sha256=", 7);
    std::memcpy(partyBytes + profilePayloadSize + 7, digest, 64);
    partyBytes[profilePayloadSize + 71] = '\n';
    if (decodeNativeRunSave(partyBytes, profilePayloadSize + 72, PokerogueContent::kContentHash, restored) !=
            NativeSaveResult::Ok || restored.participantHistoryResolved || restored.participantCount ||
        restored.saveVersion != kNativeSaveVersion) return 94;
    static char bundleProfile[kStarterCandyProfileMaxBytes]{};
    static char bundleBytes[kNativeProgressBundleMaxBytes]{};
    static char repeatedBundle[kNativeProgressBundleMaxBytes]{};
    original.starterProfileGeneration = 17;
    size_t bundleRunSize = 0, bundleProfileSize = 0, bundleSize = 0, repeatedSize = 0;
    if (encodeNativeRunSave(original, partyBytes, sizeof(partyBytes), bundleRunSize) != NativeSaveResult::Ok ||
        encodeNativeStarterCandyProfile(candyRecords, 2, 17, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, bundleProfile, sizeof(bundleProfile), bundleProfileSize) !=
            NativeSaveResult::Ok || encodeNativeProgressBundle(partyBytes, bundleRunSize, bundleProfile,
            bundleProfileSize, PokerogueContent::kContentHash, restored, bundleBytes, sizeof(bundleBytes), bundleSize) !=
            NativeSaveResult::Ok) return 95;
    NativeProgressBundleView bundleView{};
    if (inspectNativeProgressBundle(bundleBytes, bundleSize, PokerogueContent::kContentHash,
            restored, bundleView) != NativeSaveResult::Ok || bundleView.sourceProfileGeneration != 17 ||
        bundleView.runSize != bundleRunSize || bundleView.profileSize != bundleProfileSize) return 96;
    if (encodeNativeProgressBundle(partyBytes, bundleRunSize, bundleProfile, bundleProfileSize,
            PokerogueContent::kContentHash, restored, repeatedBundle, sizeof(repeatedBundle), repeatedSize) !=
            NativeSaveResult::Ok || bundleSize != repeatedSize || std::memcmp(bundleBytes, repeatedBundle, bundleSize))
        return 97;
    bundleBytes[16 + bundleRunSize + 80] ^= 1;
    if (inspectNativeProgressBundle(bundleBytes, bundleSize, PokerogueContent::kContentHash, restored,
            bundleView) != NativeSaveResult::ChecksumMismatch || bundleView.sourceProfileGeneration != 17)
        return 98;
    bundleBytes[16 + bundleRunSize + 80] ^= 1;
    if (inspectNativeProgressBundle(bundleBytes, bundleSize, wrongHash, restored, bundleView) !=
            NativeSaveResult::ContentMismatch) return 99;
    if (encodeNativeStarterCandyProfile(candyRecords, 2, 18, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, bundleProfile, sizeof(bundleProfile), bundleProfileSize) !=
            NativeSaveResult::Ok || encodeNativeProgressBundle(partyBytes, bundleRunSize, bundleProfile,
            bundleProfileSize, PokerogueContent::kContentHash, restored, repeatedBundle, sizeof(repeatedBundle), repeatedSize) !=
            NativeSaveResult::InvalidRecord || repeatedSize) return 100;
    bundleBytes[7] = '2';
    if (inspectNativeProgressBundle(bundleBytes, bundleSize, PokerogueContent::kContentHash, restored,
            bundleView) != NativeSaveResult::UnsupportedVersion) return 101;
    bundleBytes[7] = '1';
    StarterCandyProfileCodec::put(0xffffffffU, bundleBytes + 8, 4);
    if (inspectNativeProgressBundle(bundleBytes, bundleSize, PokerogueContent::kContentHash, restored,
            bundleView) != NativeSaveResult::InvalidFormat) return 102;
    // Real paired journal -> SD-like bundle transport -> foreign generation rebase.
    disk.sizes[0] = disk.sizes[1] = other.sizes[0] = other.sizes[1] = 0;
    NativeRunSave portable{};
    if (makeNativeRunSetupSave(321, 1, portable) != NativeSaveResult::Ok ||
        progress.commit(portable, candyRecords, 2) != NativeSaveResult::Ok) return 103;
    static MemoryBundleStorage transport;
    static char bundleWorkspace[2 * kNativeProgressBundleMaxBytes]{};
    if (progress.exportBundle(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restoredCandy, 2) != NativeSaveResult::Ok) return 104;
    size_t importedCount = 0;
    if (progress.readBundleCandidate(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restored, restoredCandy, 2, importedCount) != NativeSaveResult::Ok ||
        restored.seed != 321 || importedCount != 2 || restored.starterProfileGeneration != 1) return 105;
    // Advance local generation independently; the portable reference stays at one.
    portable.seed = 654;
    if (progress.commit(portable, candyRecords, 2) != NativeSaveResult::Ok ||
        portable.starterProfileGeneration != 2 ||
        progress.commitImported(restored, restoredCandy, importedCount) != NativeSaveResult::Ok ||
        restored.seed != 321 || restored.starterProfileGeneration != 3) return 106;
    transport.bytes[16] ^= 1;
    importedCount = 999;
    if (progress.readBundleCandidate(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restored, restoredCandy, 2, importedCount) != NativeSaveResult::ChecksumMismatch ||
        importedCount != 999 || progress.load(PokerogueContent::kContentHash, portable,
            restoredCandy, 2, candyCount) != NativeSaveResult::Ok || portable.starterProfileGeneration != 3)
        return 107;
    transport.bytes[16] ^= 1;
    transport.interrupt = true;
    if (progress.exportBundle(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restoredCandy, 2) != NativeSaveResult::IoError ||
        progress.load(PokerogueContent::kContentHash, portable, restoredCandy, 2, candyCount) !=
            NativeSaveResult::Ok || portable.starterProfileGeneration != 3) return 108;
    transport.interrupt = false;
    if (progress.exportBundle(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restoredCandy, 2) != NativeSaveResult::Ok ||
        progress.readBundleCandidate(transport, PokerogueContent::kContentHash, bundleWorkspace,
            sizeof(bundleWorkspace), restored, restoredCandy, 2, importedCount) != NativeSaveResult::Ok)
        return 109;
    disk.interrupt = true;
    if (progress.commitImported(restored, restoredCandy, importedCount) != NativeSaveResult::IoError)
        return 110;
    disk.interrupt = false;
    if (progress.load(PokerogueContent::kContentHash, portable, restoredCandy, 2, candyCount) !=
            NativeSaveResult::Ok || portable.starterProfileGeneration != 3 || portable.seed != 321)
        return 111;
    NativeStarterCandyRecord caughtProfile[1]{{1, 0, 0, true}};
    char caughtEncoded[256]{};
    size_t caughtWritten = 0;
    if (encodeNativeStarterCandyProfile(caughtProfile, 1, 1, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtEncoded, sizeof(caughtEncoded), caughtWritten) !=
            NativeSaveResult::Ok) return 112;
    NativeStarterCandyRecord caughtDecoded[1]{};
    size_t caughtCount = 0;
    uint32_t caughtGeneration = 0;
    if (decodeNativeStarterCandyProfile(caughtEncoded, caughtWritten, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtDecoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || caughtCount != 1 || !caughtDecoded[0].caught) return 113;
    NativeStarterCandyRecord purchased{1, 999, 321, true};
    const PokerogueContent::StarterCandyPrice* starterPrice = nullptr;
    const auto* purchasedSpecies = PokerogueContent::findSpeciesByDex(1);
    for (const auto& row : PokerogueContent::kStarterCandyPrices)
        if (row.cost == purchasedSpecies->starterCost) { starterPrice = &row; break; }
    if (!starterPrice || applyNativeStarterCostReduction(purchased) != StarterCostPurchaseResult::Applied ||
        purchased.costReduction != 1 || purchased.candyCount != 999 - starterPrice->costReduction[0] ||
        purchased.friendship != 321 || !purchased.caught) return 117;
    if (applyNativeStarterCostReduction(purchased) != StarterCostPurchaseResult::Applied ||
        purchased.costReduction != 2 || purchased.candyCount !=
            999 - starterPrice->costReduction[0] - starterPrice->costReduction[1]) return 118;
    purchased.observedFormAttr = 128;
    purchased.unlockedFormAttr = 128;
    purchased.abilityAttr = 5;
    purchased.genderAttr = 12;
    purchased.natureAttr = (1u << 1) | (1u << 25);
    const uint8_t savedIvs[] = {31, 0, 7, 15, 22, 30};
    for (uint8_t i = 0; i < 6; ++i) purchased.dexIvs[i] = savedIvs[i];
    const uint16_t remainingCandy = purchased.candyCount;
    if (applyNativeStarterCostReduction(purchased) != StarterCostPurchaseResult::MaximumReduction ||
        purchased.candyCount != remainingCandy || purchased.costReduction != 2) return 119;
    NativeStarterCandyRecord poor{1, 0, 42, true};
    if (applyNativeStarterCostReduction(poor) != StarterCostPurchaseResult::InsufficientCandy ||
        poor.candyCount || poor.costReduction || poor.friendship != 42 || !poor.caught) return 120;
    if (encodeNativeStarterCandyProfile(&purchased, 1, 1, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtEncoded, sizeof(caughtEncoded), caughtWritten) !=
            NativeSaveResult::Ok || std::memcmp(caughtEncoded, "P3CANDY7", 8) ||
        decodeNativeStarterCandyProfile(caughtEncoded, caughtWritten, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtDecoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || caughtDecoded[0].costReduction != 2 || !caughtDecoded[0].caught ||
        caughtDecoded[0].candyCount != remainingCandy || caughtDecoded[0].friendship != 321) return 121;
    if (caughtDecoded[0].natureAttr != purchased.natureAttr || caughtDecoded[0].abilityAttr != 5 ||
        caughtDecoded[0].genderAttr != 12 || caughtDecoded[0].observedFormAttr != 128) return 125;
    for (uint8_t i = 0; i < 6; ++i) if (caughtDecoded[0].dexIvs[i] != savedIvs[i]) return 126;
    char v3Bytes[256]{};
    std::memcpy(v3Bytes, caughtEncoded, 89);
    std::memcpy(v3Bytes, "P3CANDY3", 8);
    const size_t v3Size = kStarterCandyProfileOverhead + 9;
    char v3Digest[65]{};
    IntegritySha256::hashHex(v3Bytes, v3Size - 64, v3Digest);
    std::memcpy(v3Bytes + v3Size - 64, v3Digest, 64);
    NativeStarterCandyRecord v3Decoded[1]{};
    if (decodeNativeStarterCandyProfile(v3Bytes, v3Size, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, v3Decoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || v3Decoded[0].costReduction != 2 || !v3Decoded[0].caught ||
        v3Decoded[0].natureAttr) return 130;
    char v4Bytes[256]{};
    std::memcpy(v4Bytes, caughtEncoded, 99);
    std::memcpy(v4Bytes, "P3CANDY4", 8);
    const size_t v4Size = kStarterCandyProfileOverhead + 19;
    char v4Digest[65]{};
    IntegritySha256::hashHex(v4Bytes, v4Size - 64, v4Digest);
    std::memcpy(v4Bytes + v4Size - 64, v4Digest, 64);
    NativeStarterCandyRecord v4Decoded[1]{};
    if (decodeNativeStarterCandyProfile(v4Bytes, v4Size, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, v4Decoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || v4Decoded[0].natureAttr != purchased.natureAttr ||
        v4Decoded[0].abilityAttr || v4Decoded[0].genderAttr || v4Decoded[0].dexIvs[0] != 31) return 131;
    auto invalidAttributes = purchased;
    invalidAttributes.abilityAttr = 8;
    if (StarterCandyProfileCodec::valid(invalidAttributes, 0, PokerogueContent::kMaxStarterCandyCount)) return 132;
    invalidAttributes = purchased;
    invalidAttributes.genderAttr = 1;
    if (StarterCandyProfileCodec::valid(invalidAttributes, 0, PokerogueContent::kMaxStarterCandyCount)) return 133;
    char v5Bytes[256]{};
    std::memcpy(v5Bytes, caughtEncoded, 101);
    std::memcpy(v5Bytes, "P3CANDY5", 8);
    const size_t v5Size = kStarterCandyProfileOverhead + 21;
    char v5Digest[65]{};
    IntegritySha256::hashHex(v5Bytes, v5Size - 64, v5Digest);
    std::memcpy(v5Bytes + v5Size - 64, v5Digest, 64);
    NativeStarterCandyRecord v5Decoded[1]{};
    if (decodeNativeStarterCandyProfile(v5Bytes, v5Size, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, v5Decoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || v5Decoded[0].abilityAttr != 5 || v5Decoded[0].genderAttr != 12 ||
        v5Decoded[0].natureAttr != purchased.natureAttr || v5Decoded[0].observedFormAttr) return 149;
    auto invalidObserved = purchased;
    char v6Bytes[256]{};
    const size_t v6Size = kStarterCandyProfileOverhead + 29;
    std::memcpy(v6Bytes, caughtEncoded, 109);
    std::memcpy(v6Bytes, "P3CANDY6", 8);
    char v6Digest[65]{};
    IntegritySha256::hashHex(v6Bytes, v6Size - 64, v6Digest);
    std::memcpy(v6Bytes + v6Size - 64, v6Digest, 64);
    NativeStarterCandyRecord v6Decoded[1]{};
    if (decodeNativeStarterCandyProfile(v6Bytes, v6Size, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, v6Decoded, 1, caughtCount, caughtGeneration) != NativeSaveResult::Ok ||
        v6Decoded[0].observedFormAttr != purchased.observedFormAttr || v6Decoded[0].unlockedFormAttr) return 161;
    if (caughtDecoded[0].unlockedFormAttr != purchased.unlockedFormAttr) return 162;
    auto invalidUnlock = purchased;
    invalidUnlock.unlockedFormAttr = 1;
    if (StarterCandyProfileCodec::valid(invalidUnlock, 0, PokerogueContent::kMaxStarterCandyCount)) return 163;
    invalidObserved.observedFormAttr = 1;
    if (StarterCandyProfileCodec::valid(invalidObserved, 0, PokerogueContent::kMaxStarterCandyCount)) return 150;
    invalidObserved.observedFormAttr = uint64_t(1) << 63;
    if (StarterCandyProfileCodec::valid(invalidObserved, 0, PokerogueContent::kMaxStarterCandyCount)) return 151;
    auto invalidDexRecord = purchased;
    invalidDexRecord.dexIvs[0] = 32;
    if (StarterCandyProfileCodec::valid(invalidDexRecord, 0, PokerogueContent::kMaxStarterCandyCount)) return 127;
    invalidDexRecord = purchased;
    invalidDexRecord.natureAttr |= 1u;
    if (StarterCandyProfileCodec::valid(invalidDexRecord, 0, PokerogueContent::kMaxStarterCandyCount)) return 128;
    PokemonNature defaultNature = PokemonNature::Unspecified;
    if (!nativeStarterDefaultNature(purchased, defaultNature) || defaultNature != PokemonNature::Hardy) return 134;
    auto missingNature = purchased;
    missingNature.natureAttr = 0;
    if (nativeStarterDefaultNature(missingNature, defaultNature) || defaultNature != PokemonNature::Hardy) return 135;
    uint8_t defaultAbilityIndex = 255;
    uint16_t defaultAbility = 65535;
    if (!nativeStarterDefaultAbility(purchased, defaultAbilityIndex, defaultAbility) ||
        defaultAbilityIndex != 0 || defaultAbility != purchasedSpecies->ability1) return 136;
    auto hiddenOnly = purchased;
    hiddenOnly.abilityAttr = 4;
    if (!nativeStarterDefaultAbility(hiddenOnly, defaultAbilityIndex, defaultAbility) ||
        defaultAbilityIndex != (purchasedSpecies->ability2 ? 2 : 1) || defaultAbility != purchasedSpecies->abilityHidden) return 137;
    hiddenOnly.abilityAttr = 0;
    const uint8_t retainedIndex = defaultAbilityIndex;
    const uint16_t retainedAbility = defaultAbility;
    if (nativeStarterDefaultAbility(hiddenOnly, defaultAbilityIndex, defaultAbility) ||
        defaultAbilityIndex != retainedIndex || defaultAbility != retainedAbility) return 138;
    PokemonGender defaultGender = PokemonGender::Unspecified;
    if (!nativeStarterDefaultGender(purchased, defaultGender) || defaultGender != PokemonGender::Male) return 139;
    auto femaleOnly = purchased;
    femaleOnly.genderAttr = 8;
    if (!nativeStarterDefaultGender(femaleOnly, defaultGender) || defaultGender != PokemonGender::Male) return 140;
    femaleOnly.genderAttr = 0;
    femaleOnly.abilityAttr = 0; // Legacy incomplete metadata, not a known zero-gender capture.
    if (nativeStarterDefaultGender(femaleOnly, defaultGender) || defaultGender != PokemonGender::Male) return 141;
    auto knownNoGenderBits = purchased;
    knownNoGenderBits.genderAttr = 0;
    if (!nativeStarterDefaultGender(knownNoGenderBits, defaultGender) || defaultGender != PokemonGender::Male) return 144;
    bool checkedGenderless = false;
    for (const auto& species : PokerogueContent::kSpecies) {
        if (species.malePercentTenths != 65534) continue;
        NativeStarterCandyRecord genderless{};
        genderless.speciesDex = species.dex;
        if (!nativeStarterDefaultGender(genderless, defaultGender) || defaultGender != PokemonGender::Genderless) return 142;
        checkedGenderless = true;
        break;
    }
    if (!checkedGenderless) return 143;
    purchased.costReduction = 3;
    size_t invalidWritten = 999;
    if (encodeNativeStarterCandyProfile(&purchased, 1, 1, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtEncoded, sizeof(caughtEncoded), invalidWritten) !=
            NativeSaveResult::InvalidRecord || invalidWritten) return 122;
    // A checksum-valid v6 record still rejects reserved bits and reduction three.
    const uint8_t invalidFlags[] = {8, 7};
    for (const uint8_t flags : invalidFlags) {
        caughtEncoded[88] = static_cast<char>(flags);
        char invalidDigest[65]{};
        IntegritySha256::hashHex(caughtEncoded, caughtWritten - 64, invalidDigest);
        std::memcpy(caughtEncoded + caughtWritten - 64, invalidDigest, 64);
        caughtDecoded[0].costReduction = 2;
        if (decodeNativeStarterCandyProfile(caughtEncoded, caughtWritten, PokerogueContent::kContentHash,
                PokerogueContent::kMaxStarterCandyCount, caughtDecoded, 1, caughtCount, caughtGeneration) !=
                NativeSaveResult::InvalidRecord || caughtDecoded[0].costReduction != 2) return 124;
    }
    // v2 carries only caught: importing it must not invent a purchased reduction.
    std::memcpy(caughtEncoded, "P3CANDY2", 8);
    caughtEncoded[88] = 1;
    caughtWritten = kStarterCandyProfileOverhead + 9;
    char v2Digest[65]{};
    IntegritySha256::hashHex(caughtEncoded, caughtWritten - 64, v2Digest);
    std::memcpy(caughtEncoded + caughtWritten - 64, v2Digest, 64);
    if (decodeNativeStarterCandyProfile(caughtEncoded, caughtWritten, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtDecoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || caughtDecoded[0].costReduction || !caughtDecoded[0].caught ||
        caughtDecoded[0].natureAttr) return 123;
    for (uint8_t iv : caughtDecoded[0].dexIvs) if (iv) return 129;
    // Legacy records retain candy/friendship but do not invent caught metadata.
    std::memcpy(caughtEncoded, "P3CANDY1", 8);
    --caughtWritten;
    char legacyDigest[65]{};
    IntegritySha256::hashHex(caughtEncoded, caughtWritten - 64, legacyDigest);
    std::memcpy(caughtEncoded + caughtWritten - 64, legacyDigest, 64);
    if (decodeNativeStarterCandyProfile(caughtEncoded, caughtWritten, PokerogueContent::kContentHash,
            PokerogueContent::kMaxStarterCandyCount, caughtDecoded, 1, caughtCount, caughtGeneration) !=
            NativeSaveResult::Ok || caughtDecoded[0].caught) return 114;
    if (std::strcmp(nativeSaveResultName(NativeSaveResult::MemoryUnavailable),
            "Insufficient memory for save transaction") ||
        !std::strcmp(nativeSaveResultName(NativeSaveResult::MemoryUnavailable),
            nativeSaveResultName(NativeSaveResult::InvalidRecord))) return 116;
    for (const auto& form : PokerogueContent::kForms) {
        uint16_t speciesDex = 0;
        for (const auto& species : PokerogueContent::kSpecies)
            if (!std::strcmp(species.id, form.speciesId)) { speciesDex = species.dex; break; }
        if (!speciesDex || PokerogueContent::findFormByUpstreamIndex(speciesDex, form.upstreamFormIndex) != &form) return 145;
        PokemonActorIdentity formActor{};
        formActor.formId = form.id;
        uint64_t observed = 0;
        if (pokemonObservedDexFormAttr(speciesDex, formActor, observed) != PokemonObservedFormResult::Ok ||
            observed != (uint64_t(128) << form.upstreamFormIndex)) return 147;
        NativeStarterCandyRecord observedRecord{};
        observedRecord.speciesDex = speciesDex;
        observedRecord.caught = true;
        observedRecord.observedFormAttr = observed;
        char observedBytes[256]{};
        size_t observedSize = 0, observedCount = 0;
        uint32_t observedGeneration = 0;
        NativeStarterCandyRecord restoredObservation[1]{};
        if (encodeNativeStarterCandyProfile(&observedRecord, 1, 1, PokerogueContent::kContentHash,
                PokerogueContent::kMaxStarterCandyCount, observedBytes, sizeof(observedBytes), observedSize) != NativeSaveResult::Ok ||
            decodeNativeStarterCandyProfile(observedBytes, observedSize, PokerogueContent::kContentHash,
                PokerogueContent::kMaxStarterCandyCount, restoredObservation, 1, observedCount, observedGeneration) != NativeSaveResult::Ok ||
            restoredObservation[0].observedFormAttr != observed || observedCount != 1) return 152;

        observed = 123;
        if (pokemonObservedDexFormAttr(0, formActor, observed) != PokemonObservedFormResult::MissingSpecies ||
            observed != 123) return 148;

    }
    for (const auto& form : PokerogueContent::kForms) {
        uint16_t dex = 0;
        for (const auto& species : PokerogueContent::kSpecies) {
            if (PokerogueContent::findFormByUpstreamIndex(species.dex, form.upstreamFormIndex) == &form) {
                dex = species.dex;
                break;
            }
        }
        const PokerogueContent::FormPermission* permission = nullptr;
        for (const auto& entry : PokerogueContent::kFormPermissions)
            if (std::strcmp(entry.formId, form.id) == 0) { permission = &entry; break; }
        if (!permission) return 153;
        const auto locked = pokemonValidateStarterForm(dex, form.upstreamFormIndex, 0);
        const auto unlocked = pokemonValidateStarterForm(dex, form.upstreamFormIndex,
            uint64_t(128) << form.upstreamFormIndex);
        if (permission->isStarterSelectable == 1) {
            if (locked != PokemonStarterFormResult::NotUnlocked ||
                unlocked != PokemonStarterFormResult::Ok) return 154;
        } else if (permission->isStarterSelectable == 0) {
            if (locked != PokemonStarterFormResult::NotSelectable ||
                unlocked != PokemonStarterFormResult::NotSelectable) return 155;
        } else if (locked != PokemonStarterFormResult::UnsupportedPermission) return 156;
    }
    if (pokemonValidateStarterForm(0, 0, 0) != PokemonStarterFormResult::MissingSpecies ||
        pokemonValidateStarterForm(1, 65535, 0) != PokemonStarterFormResult::MissingForm) return 157;
    for (const auto& species : PokerogueContent::kSpecies) {
        uint64_t mask = 123;
        if (pokemonObtainableFormMask(species.dex, mask) != PokemonFormUnlockMaskResult::Ok) return 158;
        size_t count = 0;
        uint64_t expected = 0;
        for (const auto& form : PokerogueContent::kForms) {
            if (std::strcmp(form.speciesId, species.id)) continue;
            ++count;
            for (const auto& permission : PokerogueContent::kFormPermissions) {
                if (!std::strcmp(permission.formId, form.id) && permission.isUnobtainable == 0)
                    expected |= uint64_t(128) << form.upstreamFormIndex;
            }
        }
        if (mask != (count > 1 ? expected : uint64_t(128))) return 159;
    }
    uint64_t missingMask = 123;
    if (pokemonObtainableFormMask(0, missingMask) != PokemonFormUnlockMaskResult::MissingSpecies ||
        missingMask != 123) return 160;
    struct FormCaptureCase { const char* original; const char* recipient; uint16_t index; uint64_t extra; };
    const FormCaptureCase cases[] = {
        {"venusaur", "venusaur", 1, 128}, {"pikachu", "pichu", 1, 128},
        {"urshifu", "urshifu", 2, 128}, {"urshifu", "urshifu", 3, 256},
        {"zygarde", "zygarde", 4, 512}, {"zygarde", "zygarde", 5, 1024}
    };
    for (const auto& sample : cases) {
        uint16_t original = 0, recipient = 0;
        for (const auto& species : PokerogueContent::kSpecies) {
            if (!std::strcmp(species.id, sample.original)) original = species.dex;
            if (!std::strcmp(species.id, sample.recipient)) recipient = species.dex;
        }
        const auto* form = PokerogueContent::findFormByUpstreamIndex(original, sample.index);
        if (!form) return 164;
        PokemonActorIdentity actor{};
        actor.formId = form->id;
        uint64_t unlocked = 0, allowed = 0;
        if (pokemonObtainableFormMask(recipient, allowed) != PokemonFormUnlockMaskResult::Ok ||
            pokemonCaptureFormUnlocks(original, actor, recipient, unlocked) != PokemonCaptureFormUnlockResult::Ok ||
            unlocked != (((uint64_t(128) << sample.index) & allowed) | sample.extra)) return 165;
    }
    uint64_t failedUnlock = 123;
    PokemonActorIdentity noActor{};
    if (pokemonCaptureFormUnlocks(0, noActor, 1, failedUnlock) != PokemonCaptureFormUnlockResult::InvalidSpecies ||
        failedUnlock != 123) return 166;
    for (const auto& species : PokerogueContent::kSpecies) {
        NativeStarterCandyRecord record{};
        record.speciesDex = species.dex;
        record.genderAttr = 8; // Only a female observed must not override species default.
        record.abilityAttr = 1;
        record.natureAttr = 2;
        PokemonGender actual = PokemonGender::Unspecified;
        const PokemonGender expected = species.malePercentTenths == 65534 ? PokemonGender::Genderless :
            species.malePercentTenths == 0 ? PokemonGender::Female : PokemonGender::Male;
        if (!nativeStarterDefaultGender(record, actual) || actual != expected) return 167;
    }
    if (PokerogueContent::findFormByUpstreamIndex(0, 0)) return 146;
    return 0;
}
