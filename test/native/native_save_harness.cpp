#include "storage/NativeRunSave.hpp"
#include "storage/NativeStarterCandyProfile.hpp"
#include "storage/NativeStarterCandyStore.hpp"
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
    NativeRunSave invalidStarter{};
    if (!nonProfileStarterDex || makeNativeRunSetupSave(123, nonProfileStarterDex, invalidStarter) !=
            NativeSaveResult::InvalidRecord) return 12;
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
        if (n + 16 <= roomSize && std::memcmp(versionSevenBytes + n, "saveVersion=000b", 16) == 0)
            versionSevenBytes[n + 15] = '7';
        if (n + 19 <= roomSize && std::memcmp(versionSevenBytes + n, "runtimeVersion=000b", 19) == 0)
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
        if (n + 16 <= partySize && std::memcmp(versionSixBytes + n, "saveVersion=000b", 16) == 0)
            versionSixBytes[n + 15] = '6';
        if (n + 19 <= partySize && std::memcmp(versionSixBytes + n, "runtimeVersion=000b", 19) == 0)
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
        if (n + 16 <= legacyTrainerSize && std::memcmp(legacyTrainerBytes + n, "saveVersion=000b", 16) == 0)
            legacyTrainerBytes[n + 15] = '4';
        if (n + 19 <= legacyTrainerSize && std::memcmp(legacyTrainerBytes + n, "runtimeVersion=000b", 19) == 0)
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
        if (n + 16 <= partySize && std::memcmp(partyBytes + n, "saveVersion=000b", 16) == 0)
            partyBytes[n + 15] = '3';
        if (n + 19 <= partySize && std::memcmp(partyBytes + n, "runtimeVersion=000b", 19) == 0)
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
        if (n + 16 <= roomSize && std::memcmp(versionEightBytes + n, "saveVersion=000b", 16) == 0)
            versionEightBytes[n + 15] = '8';
        if (n + 19 <= roomSize && std::memcmp(versionEightBytes + n, "runtimeVersion=000b", 19) == 0)
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
    return 0;
}
