#include "storage/NativeRunSave.hpp"
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
    trainerSave.trainerTypeId = 63;
    trainerSave.trainerPartyCount = 2;
    trainerSave.activeTrainerMember = 0;
    trainerSave.trainerParty[0] = {19, 12, 1, {33, 0, 0, 0}, {25, 0, 0, 0}};
    trainerSave.trainerParty[1] = {504, 9, 1, {33, 0, 0, 0}, {17, 0, 0, 0}};
    char partyBytes[kNativeSaveMaxBytes]{};
    size_t partySize = 0;
    if (encodeNativeRunSave(trainerSave, partyBytes, sizeof(partyBytes), partySize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(partyBytes, partySize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.trainerPartyCount != 2 || restored.trainerParty[1].hp != 9 ||
        restored.trainerParty[1].pp[0] != 17 || restored.activeTrainerMember != 0) return 13;
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
        if (n + 12 <= partySize && std::memcmp(partyBytes + n, "trainerType=", 12) == 0) {
            legacyBodySize = n; break;
        }
        if (n + 16 <= partySize && std::memcmp(partyBytes + n, "saveVersion=0004", 16) == 0)
            partyBytes[n + 15] = '3';
        if (n + 19 <= partySize && std::memcmp(partyBytes + n, "runtimeVersion=0004", 19) == 0)
            partyBytes[n + 18] = '3';
    }
    if (!legacyBodySize) return 19;
    IntegritySha256::hashHex(partyBytes, legacyBodySize, digest);
    std::memcpy(partyBytes + legacyBodySize, "sha256=", 7);
    std::memcpy(partyBytes + legacyBodySize + 7, digest, 64);
    partyBytes[legacyBodySize + 71] = '\n';
    if (decodeNativeRunSave(partyBytes, legacyBodySize + 72, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.saveVersion != 4 || restored.runtimeVersion != 4 ||
        restored.seed != original.seed || restored.trainerPartyCount || restored.activeTrainerMember != 0xFF) return 20;
    return 0;
}
