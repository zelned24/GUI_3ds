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
    return 0;
}
