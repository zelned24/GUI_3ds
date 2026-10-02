#include "storage/NativeRunSave.hpp"
#include "storage/NativeStarterCandyStore.hpp"
#include "storage/IntegritySha256.hpp"
#include "game/PokemonExperience.hpp"
#include "game/PokemonStarterMoveset.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokerogueModifierReward.hpp"
#include "game/PokerogueTurnOrder.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <memory>
#include <new>
#include <cmath>

namespace Pokerogue3DS {
namespace {
// Stable envelope marker; saveVersion carries the independently migrated
// payload schema (currently version 22).
constexpr char kMagic[] = "POKEROGUE-3DS-SAVE 1\n";
constexpr size_t kDigestLineLength = 72;

uint32_t packStages(const int8_t* stages) {
    uint32_t packed = 0;
    for (uint8_t stat = 0; stat < 7; ++stat)
        packed |= static_cast<uint32_t>(stages[stat] + 6) << (stat * 4);
    return packed;
}

bool unpackStages(uint32_t packed, int8_t* stages) {
    if (packed >> 28) return false;
    for (uint8_t stat = 0; stat < 7; ++stat) {
        const uint8_t value = (packed >> (stat * 4)) & 15;
        if (value > 12) return false;
        stages[stat] = static_cast<int8_t>(value) - 6;
    }
    return true;
}

bool equal(const char* first, const char* second) {
    if (!first || !second) return false;
    while (*first && *first == *second) { ++first; ++second; }
    return *first == *second;
}

const PokerogueContent::Species* canonicalStarter(uint16_t dex) {
    for (size_t i = 0; i < PokerogueContent::kSpeciesCount; ++i) {
        const auto& species = PokerogueContent::kSpecies[i];
        if (species.dex == dex && species.starterEligible && species.starterCost >= 1 &&
            species.starterCost <= kClassicStarterValueLimit) return &species;
    }
    return nullptr;
}

bool starterExperienceAtLevelFive(uint16_t dex, uint32_t& experience) {
    const auto* starter = canonicalStarter(dex);
    return starter && pokemonTotalExperienceForLevel(starter->growthRate, 5, experience)
        == PokemonExperienceResult::Ok;
}

bool copyText(char* destination, size_t capacity, const char* source) {
    if (!source) return false;
    for (size_t i = 0; i < capacity; ++i) {
        destination[i] = source[i];
        if (!source[i]) return true;
    }
    return false;
}

bool isId(const char* text, size_t capacity) {
    if (!text[0]) return false;
    for (size_t i = 0; i < capacity; ++i) {
        const char ch = text[i];
        if (!ch) return true;
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')) return false;
    }
    return false;
}

int hexValue(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    return -1;
}

bool isHash(const char* text) {
    if (!text) return false;
    for (unsigned i = 0; i < 64; ++i) if (hexValue(text[i]) < 0) return false;
    return text[64] == '\0';
}

struct Writer {
    char* bytes;
    size_t capacity;
    size_t position = 0;
    bool valid = true;

    void character(char ch) {
        if (position >= capacity) { valid = false; return; }
        bytes[position++] = ch;
    }
    void text(const char* value) { while (*value) character(*value++); }
    void hex(uint32_t value, unsigned digits) {
        constexpr char alphabet[] = "0123456789abcdef";
        for (unsigned i = 0; i < digits; ++i) character(alphabet[(value >> ((digits - i - 1) * 4)) & 15]);
        character('\n');
    }
};

struct Reader {
    const char* bytes;
    size_t end;
    size_t position = 0;

    bool literal(const char* expected) {
        while (*expected) {
            if (position >= end || bytes[position++] != *expected++) return false;
        }
        return true;
    }
    bool hex(unsigned digits, uint32_t& value) {
        value = 0;
        for (unsigned i = 0; i < digits; ++i) {
            if (position >= end) return false;
            const int digit = hexValue(bytes[position++]);
            if (digit < 0) return false;
            value = (value << 4) | static_cast<unsigned>(digit);
        }
        return literal("\n");
    }
    bool line(char* value, size_t capacity) {
        size_t i = 0;
        while (position < end) {
            const char ch = bytes[position++];
            if (ch == '\n') {
                if (i >= capacity) return false;
                value[i] = '\0'; return true;
            }
            if (i + 1 >= capacity) return false;
            if (ch == '\0' || ch == '\r') return false;
            value[i++] = ch;
        }
        return false;
    }
};

bool sameStatus(const PokemonStatusState& a, const PokemonStatusState& b) {
    return a.present == b.present && a.effect == b.effect && a.toxicTurnCount == b.toxicTurnCount &&
        a.sleepTurnsRemaining == b.sleepTurnsRemaining && a.freezeTurnsRemaining == b.freezeTurnsRemaining &&
        a.hasSleepTurnsRemaining == b.hasSleepTurnsRemaining && a.hasFreezeTurnsRemaining == b.hasFreezeTurnsRemaining;
}
void writeStatus(Writer& writer, const PokemonStatusState& status) {
    writer.hex((status.present ? 1 : 0) | (status.hasSleepTurnsRemaining ? 2 : 0) |
        (status.hasFreezeTurnsRemaining ? 4 : 0), 2);
    writer.hex(static_cast<uint8_t>(status.effect), 2);
    writer.hex(status.toxicTurnCount, 8);
    writer.hex(status.sleepTurnsRemaining, 8);
    writer.hex(status.freezeTurnsRemaining, 8);
}
bool readStatus(Reader& reader, PokemonStatusState& output) {
    PokemonStatusState status{};
    uint32_t value = 0;
    if (!reader.hex(2, value) || value > 7) return false;
    status.present = (value & 1) != 0;
    status.hasSleepTurnsRemaining = (value & 2) != 0;
    status.hasFreezeTurnsRemaining = (value & 4) != 0;
    if (!reader.hex(2, value) || value > 7) return false;
    status.effect = static_cast<PokemonStatusEffect>(value);
    if (!reader.hex(8, status.toxicTurnCount) || !reader.hex(8, status.sleepTurnsRemaining) ||
        !reader.hex(8, status.freezeTurnsRemaining) || !pokemonStatusStateValid(status)) return false;
    output = status;
    return true;
}

// The common envelope is inspected before version-specific fields. This makes a
// valid newer save block silent fallback or overwrite by an older runtime.
NativeSaveResult inspectEnvelope(const char* bytes, size_t length, NativeRunSave& header,
                                size_t& payloadStart) {
    if (!bytes || length < kDigestLineLength || length > kNativeSaveMaxBytes) {
        return length > kNativeSaveMaxBytes ? NativeSaveResult::TooLarge : NativeSaveResult::InvalidFormat;
    }
    const size_t bodyLength = length - kDigestLineLength;
    Reader digestLine{bytes, length, bodyLength};
    char expectedDigest[65];
    if (!digestLine.literal("sha256=") || !digestLine.line(expectedDigest, sizeof(expectedDigest))
        || digestLine.position != length || !isHash(expectedDigest)) return NativeSaveResult::InvalidFormat;
    char actualDigest[65];
    IntegritySha256::hashHex(bytes, bodyLength, actualDigest);
    if (!equal(expectedDigest, actualDigest)) return NativeSaveResult::ChecksumMismatch;

    Reader reader{bytes, bodyLength};
    uint32_t parsed = 0;
    if (!reader.literal(kMagic) || !reader.literal("generation=") || !reader.hex(8, header.generation)
        || !reader.literal("saveVersion=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
    header.saveVersion = static_cast<uint16_t>(parsed);
    if (!reader.literal("runtimeVersion=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
    header.runtimeVersion = static_cast<uint16_t>(parsed);
    if (!reader.literal("contentHash=") || !reader.line(header.contentHash, sizeof(header.contentHash))
        || !isHash(header.contentHash)) return NativeSaveResult::InvalidFormat;
    payloadStart = reader.position;
    return NativeSaveResult::Ok;
}

struct JournalSlot {
    char bytes[kNativeSaveMaxBytes];
    size_t size = 0;
    NativeRunSave header{};
    NativeSaveResult status = NativeSaveResult::NotFound;
};

NativeSaveResult readJournal(NativeSaveStorage& storage, JournalSlot* slots, int& selected) {
    selected = -1;
    for (unsigned i = 0; i < 2; ++i) {
        slots[i].status = storage.readSlot(i, slots[i].bytes, sizeof(slots[i].bytes), slots[i].size);
        if (slots[i].status == NativeSaveResult::IoError) return slots[i].status;
        if (slots[i].status == NativeSaveResult::Ok) {
            size_t payloadStart = 0;
            slots[i].status = inspectEnvelope(slots[i].bytes, slots[i].size, slots[i].header, payloadStart);
        }
        if (slots[i].status == NativeSaveResult::Ok
            && (selected < 0 || slots[i].header.generation > slots[selected].header.generation)) selected = static_cast<int>(i);
    }
    if (selected < 0) {
        return slots[0].status == NativeSaveResult::NotFound ? slots[1].status : slots[0].status;
    }
    if (slots[0].status == NativeSaveResult::Ok && slots[1].status == NativeSaveResult::Ok
        && slots[0].header.generation == slots[1].header.generation) {
        if (slots[0].size != slots[1].size) return NativeSaveResult::AmbiguousJournal;
        for (size_t i = 0; i < slots[0].size; ++i) {
            if (slots[0].bytes[i] != slots[1].bytes[i]) return NativeSaveResult::AmbiguousJournal;
        }
    }
    return NativeSaveResult::Ok;
}
} // namespace

const char* nativeSaveResultName(NativeSaveResult result) {
    switch (result) {
    case NativeSaveResult::Ok: return "OK";
    case NativeSaveResult::NotFound: return "No saved progress";
    case NativeSaveResult::InvalidFormat: return "Invalid save format";
    case NativeSaveResult::UnsupportedVersion: return "Unsupported save version";
    case NativeSaveResult::IncompatibleRuntime: return "Save needs another runtime version";
    case NativeSaveResult::ContentMismatch: return "Save needs its matching content version";
    case NativeSaveResult::UnsupportedStage: return "Save stage is not supported";
    case NativeSaveResult::InvalidRecord: return "Invalid saved canonical reference";
    case NativeSaveResult::TooLarge: return "Save exceeds the supported size";
    case NativeSaveResult::ChecksumMismatch: return "Save checksum mismatch";
    case NativeSaveResult::IoError: return "SD read or write failed";
    case NativeSaveResult::SequenceExhausted: return "Save generation exhausted";
    case NativeSaveResult::AmbiguousJournal: return "Conflicting save generations";
    case NativeSaveResult::MemoryUnavailable: return "Insufficient memory for save transaction";
    }
    return "Unknown save error";
}

NativeSaveResult makeNativeRunSetupSave(uint32_t seed, uint16_t starterDex, NativeRunSave& output) {
    std::unique_ptr<NativeRunSave> valueStorage(new (std::nothrow) NativeRunSave{});
    if (!valueStorage) return NativeSaveResult::MemoryUnavailable;
    auto& value = *valueStorage;
    value.seed = seed;
    value.starterDex = starterDex;
    if (!starterExperienceAtLevelFive(starterDex, value.playerExperience))
        return NativeSaveResult::InvalidRecord;
    copyText(value.contentHash, sizeof(value.contentHash), PokerogueContent::kContentHash);
    copyText(value.modeId, sizeof(value.modeId), "classic");
    copyText(value.biomeId, sizeof(value.biomeId), PokerogueContent::kStartingBiomeId);
    const auto status = validateNativeRunSave(value, PokerogueContent::kContentHash);
    if (status == NativeSaveResult::Ok) output = value;
    return status;
}

bool restoreNativePokemonSave(const NativePokemonSave& saved, PokemonBattleState& output) {
    if (!pokemonStatusStateValid(saved.status) ||
        !validPokemonConfusionTag(saved.confusion)) return false;
    if (saved.nature > 24 || saved.gender > static_cast<uint8_t>(PokemonGender::Female) ||
        !saved.moveCount || saved.moveCount > 4 || !saved.formId[0]) return false;
    bool terminated = false;
    for (size_t i = 0; i < sizeof(saved.formId); ++i)
        if (!saved.formId[i]) { terminated = true; break; }
    if (!terminated) return false;
    const auto* form = PokerogueContent::findFormById(saved.formId);
    const auto* species = PokerogueContent::findSpeciesByDex(saved.speciesDex);
    if (!form || !species) return false;
    PokemonBattleInit input{};
    input.speciesDex = saved.speciesDex;
    input.formId = form->id; // Catalog owns the pointer, never the save buffer.
    input.level = saved.level;
    input.pokemonId = saved.pokemonId;
    input.abilityId = saved.abilityId;
    input.gender = static_cast<PokemonGender>(saved.gender);
    input.nature = static_cast<PokemonNature>(saved.nature);
    input.deriveIvsFromPokemonId = saved.ivsDerivedFromId;
    input.moveCount = saved.moveCount;
    for (uint8_t i = 0; i < 6; ++i) input.ivs[i] = saved.ivs[i];
    for (uint8_t i = 0; i < 4; ++i) {
        if (i >= saved.moveCount && (saved.moveIds[i] || saved.pp[i] || (saved.maxPpResolved && saved.maxPp[i]))) return false;
        input.moveIds[i] = saved.moveIds[i];
        for (uint8_t prior = 0; prior < i && i < saved.moveCount; ++prior)
            if (saved.moveIds[prior] == saved.moveIds[i]) return false;
    }
    PokemonBattleState actor{};
    if (initializePokemonBattleState(input, actor) != PokemonBattleInitResult::Ok ||
        saved.hp > actor.maxHp) return false;
    for (uint8_t i = 0; i < 6; ++i)
        if (saved.ivs[i] != actor.ivs[i]) return false;
    uint32_t threshold = 0;
    if (pokemonTotalExperienceForLevel(species->growthRate, saved.level, threshold) !=
            PokemonExperienceResult::Ok) return false;
    // PokemonLevelIncrementModifier keeps EXP when an explicit uncapped-limit
    // override is exceeded. Actor storage has no such policy: preserve both
    // values; the enclosing run validates its resolved progression capability.
    // EXP may also exceed the next threshold while the wave cap is active.
    if (saved.friendshipResolved) actor.friendship = saved.friendship;
    actor.pauseEvolutions = saved.pauseEvolutions;
    actor.heldItemLostTags.unburden = saved.unburdenTag;
    actor.hp = saved.hp;
    actor.status = saved.status;
    actor.confusion = saved.confusion;
    actor.sturdy.present = saved.sturdyTag;
    for (uint8_t i = 0; i < saved.moveCount; ++i) {
        if (saved.maxPpResolved) {
            if (!pokemonPermanentMaxPpSupported(saved.moveIds[i], saved.maxPp[i])) return false;
            actor.moves[i].maxPp = saved.maxPp[i];
        }
        if (saved.pp[i] > actor.moves[i].maxPp) return false;
        actor.moves[i].pp = saved.pp[i];
    }
    for (uint8_t i = 0; i < 7; ++i) {
        if (saved.statStages[i] < -6 || saved.statStages[i] > 6) return false;
        actor.statStages[i] = saved.statStages[i];
    }
    output = actor;
    return true;
}

bool captureNativePokemonSave(const PokemonBattleState& state, uint32_t experience,
    NativePokemonSave& output) {
    // Mid-turn snapshots require turnData serialization; checkpoints reset it.
    if (!state.statsAreBaseFormulaOnly || !state.formId || state.turnDamageDealt ||
        state.pendingStatus != PokemonStatusEffect::None ||
        !validPokemonConfusionTag(state.confusion)) return false;
    NativePokemonSave saved{};
    if (!copyText(saved.formId, sizeof(saved.formId), state.formId)) return false;
    saved.speciesDex = state.speciesDex;
    saved.level = state.level;
    saved.pokemonId = state.pokemonId;
    saved.abilityId = state.abilityId;
    saved.gender = static_cast<uint8_t>(state.gender);
    saved.nature = static_cast<uint8_t>(state.nature);
    saved.ivsDerivedFromId = state.ivsWereDerivedFromPokemonId;
    saved.pauseEvolutions = state.pauseEvolutions;
    saved.maxPpResolved = true;
    for (uint8_t slot = 0; slot < state.moveCount; ++slot) saved.maxPp[slot] = state.moves[slot].maxPp;
    saved.friendship = state.friendship;
    saved.friendshipResolved = true;
    saved.unburdenTag = state.heldItemLostTags.unburden;
    saved.hp = state.hp;
    saved.status = state.status;
    saved.confusion = state.confusion;
    saved.sturdyTag = state.sturdy.present;
    saved.experience = experience;
    saved.moveCount = state.moveCount;
    for (uint8_t i = 0; i < 6; ++i) saved.ivs[i] = state.ivs[i];
    for (uint8_t i = 0; i < 4; ++i) {
        saved.moveIds[i] = state.moves[i].moveId;
        saved.pp[i] = state.moves[i].pp;
    }
    for (uint8_t i = 0; i < 7; ++i) saved.statStages[i] = state.statStages[i];
    PokemonBattleState restored{};
    if (!restoreNativePokemonSave(saved, restored) || restored.maxHp != state.maxHp) return false;
    for (uint8_t i = 0; i < 6; ++i) if (restored.stats[i] != state.stats[i]) return false;
    output = saved;
    return true;
}

bool restoreNativePokemonActorSave(const NativePokemonSave& saved,
    PokemonBattleState& state, PokemonActorIdentity& identity) {
    if (!saved.actorIdentityResolved || saved.abilityIndex > 2 ||
        !saved.initialTeraTypeResolved || saved.initialTeraTypeIndex > 1) return false;
    PokemonBattleState restored{};
    if (!restoreNativePokemonSave(saved, restored)) return false;
    const auto* form = PokerogueContent::findFormById(restored.formId);
    const auto* species = PokerogueContent::findSpeciesByDex(restored.speciesDex);
    if (!form || !species) return false;
    const uint16_t first = form->ability1 ? form->ability1 : species->ability1;
    const uint16_t second = form->ability2 ? form->ability2 : first;
    const uint16_t hidden = form->abilityHidden ? form->abilityHidden : species->abilityHidden;
    if (restored.abilityId != (saved.abilityIndex == 2 ? hidden : saved.abilityIndex == 1 ? second : first))
        return false;
    // Legacy actors lack the concrete type. Resolve their former ordinal once;
    // new snapshots preserve the original type independently of current form.
    bool typeTerminated = false;
    for (char ch : saved.initialTeraType) if (!ch) { typeTerminated = true; break; }
    if (!typeTerminated) return false;
    const char* initialType = resolvePokemonTypeSymbol(saved.initialTeraType[0]
        ? saved.initialTeraType : saved.initialTeraTypeIndex ? form->type2 : form->type1);
    if (!initialType) return false;
    PokemonActorIdentity actor{};
    actor.pokemonId = restored.pokemonId;
    actor.abilityIndex = saved.abilityIndex;
    actor.gender = restored.gender;
    actor.nature = restored.nature;
    actor.formId = restored.formId;
    actor.initialTeraType = initialType;
    actor.initialTeraTypeIndex = saved.initialTeraTypeIndex;
    actor.initialTeraTypeResolved = true;
    for (uint8_t i = 0; i < 6; ++i) actor.ivs[i] = restored.ivs[i];
    state = restored;
    identity = actor;
    return true;
}

bool captureNativePokemonActorSave(const PokemonBattleState& state,
    const PokemonActorIdentity& identity, uint32_t experience, NativePokemonSave& output) {
    if (identity.pokemonId != state.pokemonId || identity.gender != state.gender ||
        identity.nature != state.nature || !equal(identity.formId, state.formId)) return false;
    for (uint8_t i = 0; i < 6; ++i) if (identity.ivs[i] != state.ivs[i]) return false;
    NativePokemonSave saved{};
    if (!captureNativePokemonSave(state, experience, saved)) return false;
    saved.actorIdentityResolved = true;
    saved.abilityIndex = identity.abilityIndex;
    saved.initialTeraTypeIndex = identity.initialTeraTypeIndex;
    saved.initialTeraTypeResolved = identity.initialTeraTypeResolved;
    if (identity.initialTeraType && (!resolvePokemonTypeSymbol(identity.initialTeraType) ||
        !copyText(saved.initialTeraType, sizeof(saved.initialTeraType), identity.initialTeraType))) return false;
    PokemonBattleState restored{};
    PokemonActorIdentity restoredIdentity{};
    if (!restoreNativePokemonActorSave(saved, restored, restoredIdentity)) return false;
    if (!copyText(saved.initialTeraType, sizeof(saved.initialTeraType), restoredIdentity.initialTeraType)) return false;
    output = saved;
    return true;
}

NativeSaveResult encodeNativeHeldModifier(const NativeHeldModifierInstance& instance,
    char* output, size_t capacity, size_t& written) {
    written = 0;
    if (!validateHeldModifierInstance(instance)) return NativeSaveResult::InvalidRecord;
    if (!output) return NativeSaveResult::InvalidFormat;
    const auto* definition = heldModifierDefinition(instance);
    Writer writer{output, capacity};
    writer.text("held=1\n");
    writer.text(definition->id); writer.character('\n');
    writer.hex(instance.ownerPokemonId, 8);
    writer.hex(instance.stackCount, 4);
    writer.hex(instance.transferable ? 1 : 0, 2);
    size_t argumentBytes = 0;
    while (instance.rawArguments[argumentBytes]) ++argumentBytes;
    writer.hex(static_cast<uint32_t>(argumentBytes), 2);
    for (size_t i = 0; i < argumentBytes; ++i)
        writer.hex(static_cast<unsigned char>(instance.rawArguments[i]), 2);
    if (!writer.valid) return NativeSaveResult::TooLarge;
    written = writer.position;
    return NativeSaveResult::Ok;
}

NativeSaveResult decodeNativeHeldModifier(const char* bytes, size_t length,
    NativeHeldModifierInstance& output) {
    if (!bytes || !length || length > 1024) return NativeSaveResult::InvalidFormat;
    Reader reader{bytes, length};
    char canonicalId[128]{};
    char arguments[128]{};
    uint32_t owner = 0, stack = 0, transferable = 0, argumentBytes = 0;
    if (!reader.literal("held=1\n") || !reader.line(canonicalId, sizeof(canonicalId)) ||
        !reader.hex(8, owner) || !reader.hex(4, stack) || !reader.hex(2, transferable) ||
        transferable > 1 || !reader.hex(2, argumentBytes) || argumentBytes >= sizeof(arguments))
        return NativeSaveResult::InvalidFormat;
    for (uint32_t i = 0; i < argumentBytes; ++i) {
        uint32_t value = 0;
        if (!reader.hex(2, value) || !value) return NativeSaveResult::InvalidFormat;
        arguments[i] = static_cast<char>(value);
    }
    if (reader.position != reader.end) return NativeSaveResult::InvalidFormat;
    NativeHeldModifierInstance next{};
    if (initializeHeldModifierInstance(canonicalId, owner, static_cast<uint16_t>(stack),
            transferable != 0, arguments, next) != HeldModifierStorageResult::Ok)
        return NativeSaveResult::InvalidRecord;
    output = next;
    return NativeSaveResult::Ok;
}

NativeSaveResult encodeNativePokemonSave(const NativePokemonSave& saved, char* output,
    size_t capacity, size_t& written) {
    written = 0;
    PokemonBattleState state{};
    PokemonActorIdentity identity{};
    if (!restoreNativePokemonActorSave(saved, state, identity)) return NativeSaveResult::InvalidRecord;
    if (!output) return NativeSaveResult::InvalidFormat;
    Writer writer{output, capacity};
    writer.text(saved.sturdyTag ? "pokemon=b\n" : saved.confusion.present ? "pokemon=a\n" : saved.status.present ? "pokemon=7\n" : "pokemon=6\n");
    writer.hex(saved.speciesDex, 4);
    writer.text(saved.formId); writer.character('\n');
    writer.hex(saved.level, 4);
    writer.hex(saved.pokemonId, 8);
    writer.hex(saved.abilityId, 4);
    writer.hex(saved.gender, 2);
    writer.hex(saved.nature, 2);
    for (uint8_t iv : saved.ivs) writer.hex(iv, 2);
    writer.hex(saved.hp, 4);
    writer.hex(saved.experience, 8);
    writer.hex(saved.moveCount, 2);
    for (uint8_t slot = 0; slot < 4; ++slot) {
        writer.hex(saved.moveIds[slot], 4);
        writer.hex(saved.pp[slot], 2);
    }
    writer.hex(packStages(saved.statStages), 8);
    writer.hex(saved.ivsDerivedFromId ? 1 : 0, 2);
    writer.hex(saved.abilityIndex, 2);
    writer.hex(saved.initialTeraTypeIndex, 2);
    writer.hex(saved.pauseEvolutions ? 1 : 0, 2);
    writer.text(identity.initialTeraType); writer.character('\n');
    writer.hex(saved.unburdenTag ? 1 : 0, 2);
    writer.hex(state.friendship, 2);
    for (uint8_t slot = 0; slot < 4; ++slot) writer.hex(state.moves[slot].maxPp, 2);
    if (saved.sturdyTag) {
        writeStatus(writer, saved.status);
        writer.hex(saved.confusion.present ? 1 : 0, 1);
    }
    if (saved.confusion.present) {
        if (!saved.sturdyTag) writeStatus(writer, saved.status);
        writer.hex(saved.confusion.turns, 8);
        writer.hex(saved.confusion.sourceMoveResolved ? 1 : 0, 1);
        writer.hex(saved.confusion.sourceMoveId, 4);
        writer.hex(saved.confusion.sourcePokemonResolved ? 1 : 0, 1);
        writer.hex(saved.confusion.sourcePokemonId, 8);
    } else if (saved.status.present && !saved.sturdyTag) {
        writer.hex(static_cast<uint8_t>(saved.status.effect), 2);
        writer.hex((saved.status.hasSleepTurnsRemaining ? 1 : 0) |
            (saved.status.hasFreezeTurnsRemaining ? 2 : 0), 2);
        writer.hex(saved.status.toxicTurnCount, 8);
        writer.hex(saved.status.sleepTurnsRemaining, 8);
        writer.hex(saved.status.freezeTurnsRemaining, 8);
    }
    if (saved.sturdyTag) writer.hex(1, 1);
    if (!writer.valid) return NativeSaveResult::TooLarge;
    written = writer.position;
    return NativeSaveResult::Ok;
}

NativeSaveResult decodeNativePokemonSave(const char* bytes, size_t length,
    NativePokemonSave& output) {
    if (!bytes || !length || length > 512) return NativeSaveResult::InvalidFormat;
    Reader reader{bytes, length};
    NativePokemonSave saved{};
    uint32_t value = 0;
    if (!reader.literal("pokemon=") || !reader.hex(1, value) || (value < 1 || value > 11))
        return NativeSaveResult::InvalidFormat;
    const bool hasSurvival = value >= 11;
    const bool hasConfusionActor = value >= 10;
    const bool hasConfusionSource = value >= 9;
    bool hasConfusion = value >= 8;
    const bool hasStatus = value >= 7;
    const bool hasMaxPp = value >= 6;
    const bool hasFriendship = value >= 5;
    const bool hasUnburdenTag = value >= 4;
    const bool hasConcreteTeraType = value >= 3;
    const bool hasEvolutionPause = value >= 2;
    if (!reader.hex(4, value)) return NativeSaveResult::InvalidFormat;
    saved.speciesDex = static_cast<uint16_t>(value);
    if (!reader.line(saved.formId, sizeof(saved.formId)) || !reader.hex(4, value))
        return NativeSaveResult::InvalidFormat;
    saved.level = static_cast<uint16_t>(value);
    if (!reader.hex(8, saved.pokemonId) || !reader.hex(4, value)) return NativeSaveResult::InvalidFormat;
    saved.abilityId = static_cast<uint16_t>(value);
    if (!reader.hex(2, value)) return NativeSaveResult::InvalidFormat;
    saved.gender = static_cast<uint8_t>(value);
    if (!reader.hex(2, value)) return NativeSaveResult::InvalidFormat;
    saved.nature = static_cast<uint8_t>(value);
    for (auto& iv : saved.ivs) {
        if (!reader.hex(2, value)) return NativeSaveResult::InvalidFormat;
        iv = static_cast<uint8_t>(value);
    }
    if (!reader.hex(4, value)) return NativeSaveResult::InvalidFormat;
    saved.hp = static_cast<uint16_t>(value);
    if (!reader.hex(8, saved.experience) || !reader.hex(2, value)) return NativeSaveResult::InvalidFormat;
    saved.moveCount = static_cast<uint8_t>(value);
    for (uint8_t slot = 0; slot < 4; ++slot) {
        if (!reader.hex(4, value)) return NativeSaveResult::InvalidFormat;
        saved.moveIds[slot] = static_cast<uint16_t>(value);
        if (!reader.hex(2, value)) return NativeSaveResult::InvalidFormat;
        saved.pp[slot] = static_cast<uint8_t>(value);
    }
    if (!reader.hex(8, value) || !unpackStages(value, saved.statStages)) return NativeSaveResult::InvalidFormat;
    if (!reader.hex(2, value) || value > 1) return NativeSaveResult::InvalidFormat;
    saved.ivsDerivedFromId = value != 0;
    if (!reader.hex(2, value)) return NativeSaveResult::InvalidFormat;
    saved.abilityIndex = static_cast<uint8_t>(value);
    if (!reader.hex(2, value)) return NativeSaveResult::InvalidFormat;
    saved.initialTeraTypeIndex = static_cast<uint8_t>(value);
    if (hasEvolutionPause) {
        if (!reader.hex(2, value) || value > 1) return NativeSaveResult::InvalidFormat;
        saved.pauseEvolutions = value != 0;
    }
    if (hasConcreteTeraType && (!reader.line(saved.initialTeraType, sizeof(saved.initialTeraType)) ||
        !saved.initialTeraType[0])) return NativeSaveResult::InvalidFormat;
    if (hasUnburdenTag) {
        if (!reader.hex(2, value) || value > 1) return NativeSaveResult::InvalidFormat;
        saved.unburdenTag = value != 0;
    }
    if (hasFriendship) {
        if (!reader.hex(2, value)) return NativeSaveResult::InvalidFormat;
        saved.friendship = static_cast<uint8_t>(value);
        saved.friendshipResolved = true;
    }
    if (hasMaxPp) {
        for (uint8_t slot = 0; slot < 4; ++slot) {
            if (!reader.hex(2, value)) return NativeSaveResult::InvalidFormat;
            saved.maxPp[slot] = static_cast<uint8_t>(value);
        }
        saved.maxPpResolved = true;
    }
    if (hasSurvival) {
        if (!readStatus(reader, saved.status) || !reader.hex(1, value) || value > 1)
            return NativeSaveResult::InvalidFormat;
        hasConfusion = value != 0;
    }
    if (hasConfusion) {
        if ((!hasSurvival && !readStatus(reader, saved.status)) || !reader.hex(8, saved.confusion.turns) ||
            !saved.confusion.turns) return NativeSaveResult::InvalidFormat;
        saved.confusion.present = true;
        if (hasConfusionSource) {
            if (!reader.hex(1, value) || value > 1) return NativeSaveResult::InvalidFormat;
            saved.confusion.sourceMoveResolved = value != 0;
            if (!reader.hex(4, value)) return NativeSaveResult::InvalidFormat;
            saved.confusion.sourceMoveId = static_cast<uint16_t>(value);
        }
        if (hasConfusionActor) {
            if (!reader.hex(1, value) || value > 1) return NativeSaveResult::InvalidFormat;
            saved.confusion.sourcePokemonResolved = value != 0;
            if (!reader.hex(8, saved.confusion.sourcePokemonId)) return NativeSaveResult::InvalidFormat;
        }
    } else if (hasStatus && !hasSurvival) {
        if (!reader.hex(2, value) || value > 7) return NativeSaveResult::InvalidFormat;
        saved.status.effect = static_cast<PokemonStatusEffect>(value);
        saved.status.present = true;
        if (!reader.hex(2, value) || value > 3) return NativeSaveResult::InvalidFormat;
        saved.status.hasSleepTurnsRemaining = (value & 1) != 0;
        saved.status.hasFreezeTurnsRemaining = (value & 2) != 0;
        if (!reader.hex(8, saved.status.toxicTurnCount) ||
            !reader.hex(8, saved.status.sleepTurnsRemaining) ||
            !reader.hex(8, saved.status.freezeTurnsRemaining)) return NativeSaveResult::InvalidFormat;
    }
    if (hasSurvival) {
        if (!reader.hex(1, value) || value > 1) return NativeSaveResult::InvalidFormat;
        saved.sturdyTag = value != 0;
    }
    saved.actorIdentityResolved = saved.initialTeraTypeResolved = true;
    if (reader.position != reader.end) return NativeSaveResult::InvalidFormat;
    PokemonBattleState state{};
    PokemonActorIdentity identity{};
    if (!restoreNativePokemonActorSave(saved, state, identity)) return NativeSaveResult::InvalidRecord;
    output = saved;
    return NativeSaveResult::Ok;
}

NativeSaveResult validateNativeRunSave(const NativeRunSave& save, const char* expectedContentHash) {
    if (save.doubleBattle) {
        PokemonBattleState restoredSecond{};
        PokemonActorIdentity identity{};
        const auto& boss = save.secondEnemyBoss;
        if (save.stage == NativeSaveStage::RunSetup || save.trainerPartyCount || !save.playerPartyCount ||
            !save.globalRngResolved || save.selectedTarget > 1 || save.doubleExperienceGrantedMask > 3 ||
            !restoreNativePokemonActorSave(save.secondEnemy, restoredSecond, identity) ||
            (save.doubleExperienceGrantedMask & 1 && save.enemyHp) ||
            (save.doubleExperienceGrantedMask & 2 && save.secondEnemy.hp) ||
            (save.stage == NativeSaveStage::BattleActive && save.doubleExperienceGrantedMask !=
                static_cast<uint8_t>((save.enemyHp ? 0 : 1) | (save.secondEnemy.hp ? 0 : 2))) ||
            (save.stage == NativeSaveStage::ExperienceGranted && save.doubleExperienceGrantedMask != 3) ||
            (!boss.segmentCount && (boss.segmentIndex || boss.classicFinalBossFirstPhase || boss.hasTrainer)) ||
            (boss.segmentCount && (boss.segmentIndex >= boss.segmentCount || boss.classicFinalBossFirstPhase || boss.hasTrainer)))
            return NativeSaveResult::InvalidRecord;
    } else {
        const auto& actor = save.secondEnemy;
        if (save.doubleExperienceGrantedMask || save.selectedTarget || actor.speciesDex || actor.formId[0] ||
            actor.level || actor.pokemonId || actor.abilityId || actor.gender || actor.nature != 255 || actor.hp ||
            actor.experience || actor.moveCount || actor.status.present || actor.status.effect != PokemonStatusEffect::None ||
            actor.status.toxicTurnCount || actor.status.sleepTurnsRemaining || actor.status.freezeTurnsRemaining ||
            actor.status.hasSleepTurnsRemaining || actor.status.hasFreezeTurnsRemaining || actor.confusion.present ||
            actor.confusion.turns || actor.confusion.sourceMoveId || actor.confusion.sourceMoveResolved ||
            actor.confusion.sourcePokemonId || actor.confusion.sourcePokemonResolved || actor.sturdyTag ||
            actor.ivsDerivedFromId || actor.pauseEvolutions || actor.maxPpResolved || actor.friendship ||
            actor.friendshipResolved || actor.unburdenTag || actor.actorIdentityResolved || actor.abilityIndex ||
            actor.initialTeraType[0] || actor.initialTeraTypeIndex || actor.initialTeraTypeResolved ||
            save.secondEnemyBoss.segmentCount || save.secondEnemyBoss.segmentIndex ||
            save.secondEnemyBoss.classicFinalBossFirstPhase || save.secondEnemyBoss.hasTrainer)
            return NativeSaveResult::InvalidRecord;
        for (uint8_t i = 0; i < 4; ++i)
            if (actor.moveIds[i] || actor.pp[i] || actor.maxPp[i]) return NativeSaveResult::InvalidRecord;
        for (uint8_t i = 0; i < 6; ++i) if (actor.ivs[i]) return NativeSaveResult::InvalidRecord;
        for (uint8_t i = 0; i < 7; ++i) if (actor.statStages[i]) return NativeSaveResult::InvalidRecord;
    }
    const bool livingEnemy = save.enemyHp || (save.doubleBattle && save.secondEnemy.hp);
    const auto& random = save.globalRng;
    const auto fractionValid = [](double value) {
        return std::isfinite(value) && value >= 0 && value < 1 &&
            std::floor(value * 4294967296.0) == value * 4294967296.0;
    };
    if (save.globalRngResolved) {
        if (save.stage == NativeSaveStage::RunSetup || !std::isfinite(random.carry) || random.carry < 0 ||
            random.carry > 2091639 || std::floor(random.carry) != random.carry ||
            !fractionValid(random.s0) || !fractionValid(random.s1) || !fractionValid(random.s2))
            return NativeSaveResult::InvalidRecord;
    } else if (random.carry != 0 || random.s0 != 0 || random.s1 != 0 || random.s2 != 0)
        return NativeSaveResult::InvalidRecord;

    if (save.playerPartyCount > 6 || save.trainerPartyCount > 6) return NativeSaveResult::InvalidRecord;
    if (save.stage == NativeSaveStage::RunSetup && save.enemySturdyTag) return NativeSaveResult::InvalidRecord;
    for (uint8_t i = 0; i < 6; ++i)
        if (i >= save.trainerPartyCount && save.trainerParty[i].sturdyTag) return NativeSaveResult::InvalidRecord;
    if (save.trainerPartyCount && save.activeTrainerMember < save.trainerPartyCount &&
        save.enemySturdyTag != save.trainerParty[save.activeTrainerMember].sturdyTag)
        return NativeSaveResult::InvalidRecord;
    const auto& boss = save.enemyBoss;
    if ((!boss.segmentCount && (boss.segmentIndex || boss.classicFinalBossFirstPhase || boss.hasTrainer)) ||
        (boss.segmentCount && (boss.segmentIndex >= boss.segmentCount || save.stage == NativeSaveStage::RunSetup ||
            save.trainerPartyCount || boss.hasTrainer)) ||
        (boss.classicFinalBossFirstPhase && save.wave != PokerogueContent::kClassicFinalWave))
        return NativeSaveResult::InvalidRecord;
    const auto validConfusion = [](const PokemonConfusionTagState& tag) {
        return validPokemonConfusionTag(tag);
    };
    const auto sameConfusion = [](const PokemonConfusionTagState& a, const PokemonConfusionTagState& b) {
        return a.present == b.present && a.turns == b.turns &&
            a.sourceMoveResolved == b.sourceMoveResolved && a.sourceMoveId == b.sourceMoveId &&
            a.sourcePokemonResolved == b.sourcePokemonResolved && a.sourcePokemonId == b.sourcePokemonId;
    };
    if (!validConfusion(save.playerConfusion) || !validConfusion(save.enemyConfusion) ||
        (save.stage == NativeSaveStage::RunSetup && (save.playerConfusion.present || save.enemyConfusion.present)))
        return NativeSaveResult::InvalidRecord;
    for (uint8_t i = 0; i < 6; ++i) {
        if (!validConfusion(save.playerParty[i].confusion) || !validConfusion(save.trainerParty[i].confusion) ||
            (i >= save.playerPartyCount && (save.playerParty[i].confusion.present || save.playerParty[i].sturdyTag)) ||
            (i >= save.trainerPartyCount && save.trainerParty[i].confusion.present)) return NativeSaveResult::InvalidRecord;
    }
    if (save.playerPartyCount && save.activePlayerMember < save.playerPartyCount &&
        !sameConfusion(save.playerConfusion, save.playerParty[save.activePlayerMember].confusion))
        return NativeSaveResult::InvalidRecord;
    if (save.trainerPartyCount && save.activeTrainerMember < save.trainerPartyCount &&
        !sameConfusion(save.enemyConfusion, save.trainerParty[save.activeTrainerMember].confusion))
        return NativeSaveResult::InvalidRecord;
    if (!pokemonStatusStateValid(save.playerStatus) || !pokemonStatusStateValid(save.enemyStatus) ||
        (save.stage == NativeSaveStage::RunSetup && (save.playerStatus.present || save.enemyStatus.present)))
        return NativeSaveResult::InvalidRecord;
    for (uint8_t i = 0; i < 6; ++i)
        if (!pokemonStatusStateValid(save.trainerParty[i].status) ||
            (i >= save.trainerPartyCount && save.trainerParty[i].status.present)) return NativeSaveResult::InvalidRecord;
    if (save.playerPartyCount && save.activePlayerMember < save.playerPartyCount &&
        !sameStatus(save.playerStatus, save.playerParty[save.activePlayerMember].status)) return NativeSaveResult::InvalidRecord;
    if (save.trainerPartyCount && save.activeTrainerMember < save.trainerPartyCount &&
        !sameStatus(save.enemyStatus, save.trainerParty[save.activeTrainerMember].status)) return NativeSaveResult::InvalidRecord;
    if (save.saveVersion != kNativeSaveVersion) return NativeSaveResult::UnsupportedVersion;
    if (save.runtimeVersion != kNativeSaveRuntimeVersion) return NativeSaveResult::IncompatibleRuntime;
    const PokemonTrickRoomState room{save.trickRoomTurnsLeft, save.trickRoomMaxDuration,
        save.trickRoomSourceMoveId, save.trickRoomSourcePokemonId};
    if (!validPokemonTrickRoomState(room) ||
        (save.stage == NativeSaveStage::RunSetup && room.turnsLeft)) return NativeSaveResult::InvalidRecord;
    // Pinned src/data/pokeball.ts MAX_PER_TYPE_POKEBALLS = 99.
    for (uint8_t ball = 0; ball < 5; ++ball)
        if (save.pokeballCounts[ball] > 99 ||
            (save.stage == NativeSaveStage::RunSetup && save.pokeballCounts[ball] != (ball ? 0 : 5)))
            return NativeSaveResult::InvalidRecord;
    if (save.heldModifierCount > kNativeHeldModifierCapacity ||
        (save.heldModifierCount && (save.stage == NativeSaveStage::RunSetup || !save.playerPartyCount)))
        return NativeSaveResult::InvalidRecord;
    for (uint8_t i = 0; i < save.heldModifierCount; ++i)
        if (!validateHeldModifierInstance(save.heldModifiers[i])) return NativeSaveResult::InvalidRecord;
    if (save.setupStarterCount > 6 ||
        (save.setupStarterCount && (save.stage != NativeSaveStage::RunSetup ||
            save.setupStarterDexes[0] != save.starterDex))) return NativeSaveResult::InvalidRecord;
    for (uint8_t i = 0; i < 6; ++i) {
        if (i >= save.setupStarterCount) {
            if (save.setupStarterDexes[i]) return NativeSaveResult::InvalidRecord;
            continue;
        }
        if (!canonicalStarter(save.setupStarterDexes[i])) return NativeSaveResult::InvalidRecord;
        for (uint8_t prior = 0; prior < i; ++prior)
            if (save.setupStarterDexes[prior] == save.setupStarterDexes[i]) return NativeSaveResult::InvalidRecord;
    }
    if (save.playerPartyCount > 6 ||
        (!save.playerPartyCount && save.activePlayerMember != 0xFF) ||
        (save.playerPartyCount && (save.stage == NativeSaveStage::RunSetup ||
            save.activePlayerMember >= save.playerPartyCount))) return NativeSaveResult::InvalidRecord;
    if (save.participantCount > 6 || (!save.participantHistoryResolved && save.participantCount) ||
        (save.stage == NativeSaveStage::RunSetup && save.participantCount)) return NativeSaveResult::InvalidRecord;
    for (uint8_t i = 0; i < 6; ++i) {
        if (i >= save.participantCount) {
            if (save.participantIds[i]) return NativeSaveResult::InvalidRecord;
            continue;
        }
        if (i && save.participantIds[i - 1] >= save.participantIds[i]) return NativeSaveResult::InvalidRecord;
        if (save.playerPartyCount) {
            bool found = false;
            for (uint8_t member = 0; member < save.playerPartyCount; ++member)
                found |= save.playerParty[member].pokemonId == save.participantIds[i];
            if (!found) return NativeSaveResult::InvalidRecord;
        }
    }
    bool livingPlayerMember = false;
    for (uint8_t member = 0; member < save.playerPartyCount; ++member) {
        PokemonBattleState actor{};
        PokemonActorIdentity identity{};
        if (!restoreNativePokemonActorSave(save.playerParty[member], actor, identity))
            return NativeSaveResult::InvalidRecord;
        livingPlayerMember |= actor.hp != 0;
        const auto* memberSpecies = PokerogueContent::findSpeciesByDex(actor.speciesDex);
        const uint16_t cap = classicExperienceLevelCap(save.wave);
        if (!memberSpecies || !cap || actor.level > cap) return NativeSaveResult::InvalidRecord;
        if (actor.level < cap) {
            uint32_t nextExperience = 0;
            if (pokemonTotalExperienceForLevel(memberSpecies->growthRate, actor.level + 1, nextExperience) !=
                    PokemonExperienceResult::Ok || save.playerParty[member].experience >= nextExperience)
                return NativeSaveResult::InvalidRecord;
        }
        for (uint8_t prior = 0; prior < member; ++prior)
            if (save.playerParty[prior].pokemonId == actor.pokemonId) return NativeSaveResult::InvalidRecord;
    }
    if (save.playerPartyCount) {
        if ((save.stage == NativeSaveStage::BattleLost && livingPlayerMember) ||
            (save.stage != NativeSaveStage::BattleLost && !livingPlayerMember))
            return NativeSaveResult::InvalidRecord;
        const auto& active = save.playerParty[save.activePlayerMember];
        if (active.level != save.playerLevel || active.experience != save.playerExperience ||
            active.hp != save.playerHp || active.moveCount != save.playerMoveCount)
            return NativeSaveResult::InvalidRecord;
        for (uint8_t slot = 0; slot < 4; ++slot)
            if (active.moveIds[slot] != save.playerMoveIds[slot] || active.pp[slot] != save.playerPp[slot])
                return NativeSaveResult::InvalidRecord;
        for (uint8_t stat = 0; stat < 7; ++stat)
            if (active.statStages[stat] != save.playerStatStages[stat]) return NativeSaveResult::InvalidRecord;
    }
    const bool immutableWeather = save.weatherType >= 7 && save.weatherType <= 9;
    if (save.weatherType > 9 || save.weatherTurnsLeft > save.weatherMaxDuration ||
        ((save.weatherType == 0 || immutableWeather) && (save.weatherTurnsLeft || save.weatherMaxDuration)) ||
        (save.stage == NativeSaveStage::RunSetup && save.weatherType != 0))
        return NativeSaveResult::InvalidRecord;
    if (!isHash(save.contentHash) || !isHash(expectedContentHash)) return NativeSaveResult::InvalidFormat;
    if (!equal(save.contentHash, expectedContentHash)) return NativeSaveResult::ContentMismatch;
    // Classic progression validation across waves and registered biomes.
    if (!save.seed || !save.wave || save.wave > PokerogueContent::kClassicFinalWave
        || !isId(save.modeId, sizeof(save.modeId))
        || !isId(save.biomeId, sizeof(save.biomeId)) || !equal(save.modeId, "classic"))
        return NativeSaveResult::InvalidRecord;
    if (!PokerogueContent::findBiomeById(save.biomeId))
        return NativeSaveResult::InvalidRecord;
    const auto* starter = canonicalStarter(save.starterDex);
    if (!starter) return NativeSaveResult::InvalidRecord;
    uint32_t initialExperience = 0, currentThreshold = 0;
    const auto* experienceSpecies = save.playerPartyCount
        ? PokerogueContent::findSpeciesByDex(save.playerParty[save.activePlayerMember].speciesDex) : starter;
    if (!experienceSpecies) return NativeSaveResult::InvalidRecord;
    if (!starterExperienceAtLevelFive(save.starterDex, initialExperience)
        || save.playerLevel < (save.playerPartyCount ? 1 : 5) || save.playerLevel > classicExperienceLevelCap(save.wave)
        || pokemonTotalExperienceForLevel(experienceSpecies->growthRate, save.playerLevel, currentThreshold)
            != PokemonExperienceResult::Ok
        || save.playerExperience < currentThreshold) return NativeSaveResult::InvalidRecord;
    if (save.playerLevel < classicExperienceLevelCap(save.wave)) {
        uint32_t nextThreshold = 0;
        if (pokemonTotalExperienceForLevel(experienceSpecies->growthRate,
                static_cast<uint16_t>(save.playerLevel + 1), nextThreshold) != PokemonExperienceResult::Ok
            || save.playerExperience >= nextThreshold) return NativeSaveResult::InvalidRecord;
    }
    if (save.trainerPartyCount > 6) return NativeSaveResult::InvalidRecord;
    if (!save.trainerPartyCount) {
        if (save.enemySwitchCounter || save.trainerTypeId || save.activeTrainerMember != 0xFF)
            return NativeSaveResult::InvalidRecord;
    } else {
        if (save.stage == NativeSaveStage::RunSetup ||
            save.activeTrainerMember >= save.trainerPartyCount ||
            !PokerogueContent::findTrainerType(save.trainerTypeId))
            return NativeSaveResult::InvalidRecord;
        const auto& active = save.trainerParty[save.activeTrainerMember];
        if (active.speciesDex != save.encounterDex || active.hp != save.enemyHp ||
            active.moveCount != save.enemyMoveCount) return NativeSaveResult::InvalidRecord;
        for (uint8_t i = 0; i < 4; ++i)
            if (active.moveIds[i] != save.enemyMoveIds[i] || active.pp[i] != save.enemyPp[i])
                return NativeSaveResult::InvalidRecord;
    }
    for (uint8_t stat = 0; stat < 7; ++stat) {
        if (save.playerStatStages[stat] < -6 || save.playerStatStages[stat] > 6 ||
            save.enemyStatStages[stat] < -6 || save.enemyStatStages[stat] > 6 ||
            (save.stage == NativeSaveStage::RunSetup &&
             (save.playerStatStages[stat] || save.enemyStatStages[stat])))
            return NativeSaveResult::InvalidRecord;
        if (save.trainerPartyCount && save.enemyStatStages[stat] !=
            save.trainerParty[save.activeTrainerMember].statStages[stat])
            return NativeSaveResult::InvalidRecord;
    }
    bool hasLivingTrainerMember = false;
    for (uint8_t member = 0; member < 6; ++member) {
        const auto& record = save.trainerParty[member];
        for (int8_t stage : record.statStages)
            if (stage < -6 || stage > 6 || (member >= save.trainerPartyCount && stage))
                return NativeSaveResult::InvalidRecord;
        if (member >= save.trainerPartyCount) {
            if (record.speciesDex || record.hp || record.moveCount)
                return NativeSaveResult::InvalidRecord;
        } else {
            if (!PokerogueContent::findSpeciesByDex(record.speciesDex) ||
                !record.moveCount || record.moveCount > 4) return NativeSaveResult::InvalidRecord;
            hasLivingTrainerMember |= record.hp != 0;
        }
        for (uint8_t slot = 0; slot < 4; ++slot) {
            if (member >= save.trainerPartyCount || slot >= record.moveCount) {
                if (record.moveIds[slot] || record.pp[slot]) return NativeSaveResult::InvalidRecord;
            } else {
                const auto* move = PokerogueContent::findMoveById(record.moveIds[slot]);
                if (!move || move->pp < 1 || record.pp[slot] > move->pp)
                    return NativeSaveResult::InvalidRecord;
            }
        }
    }
    if (save.trainerPartyCount &&
        ((save.stage == NativeSaveStage::BattleWon || save.stage == NativeSaveStage::ExperienceGranted)
            ? hasLivingTrainerMember : (save.stage == NativeSaveStage::BattleActive && !hasLivingTrainerMember)))
        return NativeSaveResult::InvalidRecord;
    if (save.stage == NativeSaveStage::RunSetup) {
        if (save.wave != 1 || !equal(save.biomeId, PokerogueContent::kStartingBiomeId)
            || save.playerLevel != 5 || save.playerExperience != initialExperience)
            return NativeSaveResult::InvalidRecord;
        if (save.encounterDex || save.playerHp || save.enemyHp || save.battleTurn ||
            save.playerMoveCount || save.enemyMoveCount) return NativeSaveResult::InvalidRecord;
        for (uint8_t i = 0; i < 4; ++i) {
            if (save.playerMoveIds[i] || save.enemyMoveIds[i] || save.playerPp[i] || save.enemyPp[i])
                return NativeSaveResult::InvalidRecord;
        }
        return NativeSaveResult::Ok;
    }
    if (save.stage != NativeSaveStage::BattleActive && save.stage != NativeSaveStage::BattleWon &&
        save.stage != NativeSaveStage::BattleLost &&
        save.stage != NativeSaveStage::ExperienceGranted) return NativeSaveResult::UnsupportedStage;
    if (!save.battleTurn || !save.encounterDex || !save.playerMoveCount || save.playerMoveCount > 4 ||
        !save.enemyMoveCount || save.enemyMoveCount > 4 || (save.stage == NativeSaveStage::BattleActive &&
            (!save.playerHp || !livingEnemy)) || (save.stage == NativeSaveStage::BattleWon &&
            (livingEnemy || !save.playerHp)) || (save.stage == NativeSaveStage::ExperienceGranted &&
            (livingEnemy || !save.playerHp)) || (save.stage == NativeSaveStage::BattleLost &&
            save.playerHp)) return NativeSaveResult::InvalidRecord;

    bool validEncounter = false;
    for (size_t i = 0; i < PokerogueContent::kSpeciesCount; ++i) {
        if (PokerogueContent::kSpecies[i].dex == save.encounterDex) { validEncounter = true; break; }
    }
    if (!validEncounter) return NativeSaveResult::InvalidRecord;
    for (uint8_t i = 0; i < 4; ++i) {
        if (i >= save.playerMoveCount) {
            if (save.playerMoveIds[i] || save.playerPp[i]) return NativeSaveResult::InvalidRecord;
        } else {
            const auto* move = PokerogueContent::findMoveById(save.playerMoveIds[i]);
            if (!move || move->pp < 0) return NativeSaveResult::InvalidRecord;
            const auto* active = save.playerPartyCount ? &save.playerParty[save.activePlayerMember] : nullptr;
            const uint16_t maximum = active && active->maxPpResolved ? active->maxPp[i] : move->pp;
            if (save.playerPp[i] > maximum) return NativeSaveResult::InvalidRecord;
        }
        if (i >= save.enemyMoveCount) {
            if (save.enemyMoveIds[i] || save.enemyPp[i]) return NativeSaveResult::InvalidRecord;
        } else {
            const auto* move = PokerogueContent::findMoveById(save.enemyMoveIds[i]);
            if (!move || move->pp < 0 || save.enemyPp[i] > move->pp) return NativeSaveResult::InvalidRecord;
        }
    }
    return NativeSaveResult::Ok;
}

NativeSaveResult encodeNativeRunSave(const NativeRunSave& save, char* output, size_t capacity, size_t& written) {
    written = 0;
    const auto validity = validateNativeRunSave(save, save.contentHash);
    if (validity != NativeSaveResult::Ok) return validity;
    if (!output) return NativeSaveResult::InvalidFormat;
    Writer writer{output, capacity};
    writer.text(kMagic);
    writer.text("generation="); writer.hex(save.generation, 8);
    writer.text("saveVersion="); writer.hex(save.saveVersion, 4);
    writer.text("runtimeVersion="); writer.hex(save.runtimeVersion, 4);
    writer.text("contentHash="); writer.text(save.contentHash); writer.character('\n');
    writer.text("stage="); writer.hex(static_cast<uint16_t>(save.stage), 4);
    writer.text("seed="); writer.hex(save.seed, 8);
    writer.text("wave="); writer.hex(save.wave, 4);
    writer.text("starterDex="); writer.hex(save.starterDex, 4);
    writer.text("playerLevel="); writer.hex(save.playerLevel, 4);
    writer.text("playerExperience="); writer.hex(save.playerExperience, 8);
    writer.text("mode="); writer.text(save.modeId); writer.character('\n');
    writer.text("biome="); writer.text(save.biomeId); writer.character('\n');
    writer.text("encounterDex="); writer.hex(save.encounterDex, 4);
    writer.text("playerHp="); writer.hex(save.playerHp, 4);
    writer.text("enemyHp="); writer.hex(save.enemyHp, 4);
    writer.text("battleTurn="); writer.hex(save.battleTurn, 8);
    writer.text("playerMoveCount="); writer.hex(save.playerMoveCount, 2);
    writer.text("enemyMoveCount="); writer.hex(save.enemyMoveCount, 2);
    for (uint8_t i = 0; i < 4; ++i) {
        writer.text("playerMove="); writer.hex(save.playerMoveIds[i], 4);
        writer.text("playerPp="); writer.hex(save.playerPp[i], 2);
    }
    for (uint8_t i = 0; i < 4; ++i) {
        writer.text("enemyMove="); writer.hex(save.enemyMoveIds[i], 4);
        writer.text("enemyPp="); writer.hex(save.enemyPp[i], 2);
    }
    writer.text("enemySwitchCounter="); writer.hex(save.enemySwitchCounter, 8);
    writer.text("trainerType="); writer.hex(save.trainerTypeId, 4);
    writer.text("trainerPartyCount="); writer.hex(save.trainerPartyCount, 2);
    writer.text("activeTrainerMember="); writer.hex(save.activeTrainerMember, 2);
    for (uint8_t member = 0; member < save.trainerPartyCount; ++member) {
        const auto& record = save.trainerParty[member];
        writer.text("memberSpecies="); writer.hex(record.speciesDex, 4);
        writer.text("memberHp="); writer.hex(record.hp, 4);
        writer.text("memberMoveCount="); writer.hex(record.moveCount, 2);
        for (uint8_t slot = 0; slot < 4; ++slot) {
            writer.text("memberMove="); writer.hex(record.moveIds[slot], 4);
            writer.text("memberPp="); writer.hex(record.pp[slot], 2);
        }
    }
    writer.text("playerStages="); writer.hex(packStages(save.playerStatStages), 8);
    writer.text("enemyStages="); writer.hex(packStages(save.enemyStatStages), 8);
    for (uint8_t member = 0; member < save.trainerPartyCount; ++member) {
        writer.text("memberStages="); writer.hex(packStages(save.trainerParty[member].statStages), 8);
    }
    writer.text("weatherType="); writer.hex(save.weatherType, 2);
    writer.text("weatherTurnsLeft="); writer.hex(save.weatherTurnsLeft, 4);
    writer.text("weatherMaxDuration="); writer.hex(save.weatherMaxDuration, 4);
    writer.text("trickRoomTurnsLeft="); writer.hex(save.trickRoomTurnsLeft, 4);
    writer.text("trickRoomMaxDuration="); writer.hex(save.trickRoomMaxDuration, 4);
    writer.text("trickRoomSourceMoveId="); writer.hex(save.trickRoomSourceMoveId, 4);
    writer.text("trickRoomSourcePokemonId="); writer.hex(save.trickRoomSourcePokemonId, 8);
    for (uint8_t ball = 0; ball < 5; ++ball) {
        writer.text("pokeballCount="); writer.hex(save.pokeballCounts[ball], 4);
    }
    writer.text("playerPartyCount="); writer.hex(save.playerPartyCount, 2);
    writer.text("activePlayerMember="); writer.hex(save.activePlayerMember, 2);
    for (uint8_t member = 0; member < save.playerPartyCount; ++member) {
        char payload[512]{};
        size_t payloadSize = 0;
        const auto status = encodeNativePokemonSave(save.playerParty[member], payload, sizeof(payload), payloadSize);
        if (status != NativeSaveResult::Ok) return status;
        writer.text("playerMemberBytes="); writer.hex(static_cast<uint32_t>(payloadSize), 4);
        for (size_t byte = 0; byte < payloadSize; ++byte) writer.character(payload[byte]);
    }
    writer.text("heldModifierCount="); writer.hex(save.heldModifierCount, 2);
    for (uint8_t i = 0; i < save.heldModifierCount; ++i) {
        char payload[1024]{};
        size_t payloadSize = 0;
        const auto status = encodeNativeHeldModifier(save.heldModifiers[i], payload, sizeof(payload), payloadSize);
        if (status != NativeSaveResult::Ok) return status;
        writer.text("heldModifierBytes="); writer.hex(static_cast<uint32_t>(payloadSize), 4);
        for (size_t byte = 0; byte < payloadSize; ++byte) writer.character(payload[byte]);
    }
    writer.text("starterProfileGeneration="); writer.hex(save.starterProfileGeneration, 8);
    writer.text("participantHistoryResolved="); writer.hex(save.participantHistoryResolved ? 1 : 0, 2);
    writer.text("participantCount="); writer.hex(save.participantCount, 2);
    for (uint8_t i = 0; i < save.participantCount; ++i) {
        writer.text("participantId="); writer.hex(save.participantIds[i], 8);
    }
    writer.text("setupStarterCount="); writer.hex(save.setupStarterCount, 2);
    for (uint8_t i = 0; i < save.setupStarterCount; ++i) {
        writer.text("setupStarterDex="); writer.hex(save.setupStarterDexes[i], 4);
    }
    writer.text("playerStatus="); writeStatus(writer, save.playerStatus);
    writer.text("enemyStatus="); writeStatus(writer, save.enemyStatus);
    for (uint8_t i = 0; i < save.trainerPartyCount; ++i) {
        writer.text("memberStatus="); writeStatus(writer, save.trainerParty[i].status);
    }
    writer.text("playerConfusion="); writer.hex(save.playerConfusion.turns, 8);
    writer.text("enemyConfusion="); writer.hex(save.enemyConfusion.turns, 8);
    for (uint8_t i = 0; i < save.trainerPartyCount; ++i) {
        writer.text("memberConfusion="); writer.hex(save.trainerParty[i].confusion.turns, 8);
    }
    writer.text("playerConfusionSource=");
    writer.hex(save.playerConfusion.sourceMoveResolved ? 1 : 0, 1);
    writer.hex(save.playerConfusion.sourceMoveId, 4);
    writer.text("enemyConfusionSource=");
    writer.hex(save.enemyConfusion.sourceMoveResolved ? 1 : 0, 1);
    writer.hex(save.enemyConfusion.sourceMoveId, 4);
    for (uint8_t i = 0; i < save.trainerPartyCount; ++i) {
        writer.text("memberConfusionSource=");
        writer.hex(save.trainerParty[i].confusion.sourceMoveResolved ? 1 : 0, 1);
        writer.hex(save.trainerParty[i].confusion.sourceMoveId, 4);
    }
    writer.text("playerConfusionActor=");
    writer.hex(save.playerConfusion.sourcePokemonResolved ? 1 : 0, 1);
    writer.hex(save.playerConfusion.sourcePokemonId, 8);
    writer.text("enemyConfusionActor=");
    writer.hex(save.enemyConfusion.sourcePokemonResolved ? 1 : 0, 1);
    writer.hex(save.enemyConfusion.sourcePokemonId, 8);
    for (uint8_t i = 0; i < save.trainerPartyCount; ++i) {
        writer.text("memberConfusionActor=");
        writer.hex(save.trainerParty[i].confusion.sourcePokemonResolved ? 1 : 0, 1);
        writer.hex(save.trainerParty[i].confusion.sourcePokemonId, 8);
    }
    writer.text("enemySturdy="); writer.hex(save.enemySturdyTag ? 1 : 0, 1);
    for (uint8_t i = 0; i < save.trainerPartyCount; ++i) {
        writer.text("memberSturdy="); writer.hex(save.trainerParty[i].sturdyTag ? 1 : 0, 1);
    }
    writer.text("enemyBoss=");
    writer.hex(save.enemyBoss.segmentCount, 4);
    writer.hex(save.enemyBoss.segmentIndex, 4);
    writer.hex(save.enemyBoss.classicFinalBossFirstPhase ? 1 : 0, 1);
    writer.hex(save.enemyBoss.hasTrainer ? 1 : 0, 1);
    writer.text("globalRng="); writer.hex(save.globalRngResolved ? 1 : 0, 1);
    if (save.globalRngResolved) {
        writer.hex(static_cast<uint32_t>(save.globalRng.carry), 8);
        writer.hex(static_cast<uint32_t>(save.globalRng.s0 * 4294967296.0), 8);
        writer.hex(static_cast<uint32_t>(save.globalRng.s1 * 4294967296.0), 8);
        writer.hex(static_cast<uint32_t>(save.globalRng.s2 * 4294967296.0), 8);
    }
    writer.text("doubleBattle="); writer.hex(save.doubleBattle ? 1 : 0, 1);
    if (save.doubleBattle) {
        char payload[512]{};
        size_t payloadSize = 0;
        const auto status = encodeNativePokemonSave(save.secondEnemy, payload, sizeof(payload), payloadSize);
        if (status != NativeSaveResult::Ok) return status;
        writer.text("secondEnemyBytes="); writer.hex(static_cast<uint32_t>(payloadSize), 4);
        for (size_t i = 0; i < payloadSize; ++i) writer.character(payload[i]);
        writer.text("secondEnemyBoss=");
        writer.hex(save.secondEnemyBoss.segmentCount, 4);
        writer.hex(save.secondEnemyBoss.segmentIndex, 4);
        writer.hex(save.secondEnemyBoss.classicFinalBossFirstPhase ? 1 : 0, 1);
        writer.hex(save.secondEnemyBoss.hasTrainer ? 1 : 0, 1);
        writer.text("doubleExperience="); writer.hex(save.doubleExperienceGrantedMask, 1);
        writer.text("selectedTarget="); writer.hex(save.selectedTarget, 1);
    }
    if (!writer.valid) return NativeSaveResult::TooLarge;
    char hash[65];
    IntegritySha256::hashHex(output, writer.position, hash);
    writer.text("sha256="); writer.text(hash); writer.character('\n');
    if (!writer.valid || writer.position > kNativeSaveMaxBytes) return NativeSaveResult::TooLarge;
    written = writer.position;
    return NativeSaveResult::Ok;
}

NativeSaveResult decodeNativeRunSave(const char* bytes, size_t length, const char* expectedContentHash,
                                    NativeRunSave& output) {
    std::unique_ptr<NativeRunSave> valueStorage(new (std::nothrow) NativeRunSave{});
    if (!valueStorage) return NativeSaveResult::MemoryUnavailable;
    auto& value = *valueStorage;
    size_t payloadStart = 0;
    auto status = inspectEnvelope(bytes, length, value, payloadStart);
    if (status != NativeSaveResult::Ok) return status;
    if (value.saveVersion > kNativeSaveVersion) return NativeSaveResult::UnsupportedVersion;
    if (value.saveVersion != 1 && value.saveVersion != 2 && value.saveVersion != 3 && value.saveVersion != 4 && value.saveVersion != 5 && value.saveVersion != 6 && value.saveVersion != 7 && value.saveVersion != 8 && value.saveVersion != 9 && value.saveVersion != 10 && value.saveVersion != 11 && value.saveVersion != 12 && value.saveVersion != 13 && value.saveVersion != 14 && value.saveVersion != 15 && value.saveVersion != 16 && value.saveVersion != 17 && value.saveVersion != 18 && value.saveVersion != 19 && value.saveVersion != 20 && value.saveVersion != 21 &&
        value.saveVersion != kNativeSaveVersion) return NativeSaveResult::UnsupportedVersion;
    const bool legacySetup = value.saveVersion == 1 && value.runtimeVersion == 1;
    const bool legacyBattle = value.saveVersion == 2 && value.runtimeVersion == 2;
    const bool legacyProgress = value.saveVersion == 3 && value.runtimeVersion == 3;
    const bool legacyTrainer = value.saveVersion == 4 && value.runtimeVersion == 4;
    const bool legacySwitch = value.saveVersion == 5 && value.runtimeVersion == 5;
    const bool legacyStages = value.saveVersion == 6 && value.runtimeVersion == 6;
    const bool legacyWeather = value.saveVersion == 7 && value.runtimeVersion == 7;
    const bool legacyRoom = value.saveVersion == 8 && value.runtimeVersion == 8;
    const bool legacyInventory = value.saveVersion == 9 && value.runtimeVersion == 9;
    const bool legacyParty = value.saveVersion == 10 && value.runtimeVersion == 10;
    const bool legacyHeld = value.saveVersion == 11 && value.runtimeVersion == 11;
    const bool legacyProfile = value.saveVersion == 12 && value.runtimeVersion == 12;
    const bool legacyParticipants = value.saveVersion == 13 && value.runtimeVersion == 13;
    const bool legacySetupParty = value.saveVersion == 14 && value.runtimeVersion == 14;
    const bool legacyStatus = value.saveVersion == 15 && value.runtimeVersion == 15;
    const bool legacyConfusion = value.saveVersion == 16 && value.runtimeVersion == 16;
    const bool legacyConfusionSource = value.saveVersion == 17 && value.runtimeVersion == 17;
    const bool legacyConfusionActor = value.saveVersion == 18 && value.runtimeVersion == 18;
    const bool legacySurvival = value.saveVersion == 19 && value.runtimeVersion == 19;
    const bool legacyBoss = value.saveVersion == 20 && value.runtimeVersion == 20;
    const bool legacyGlobalRng = value.saveVersion == 21 && value.runtimeVersion == 21;
    const bool currentPayload = value.saveVersion == kNativeSaveVersion &&
        value.runtimeVersion == kNativeSaveRuntimeVersion;
    if (!currentPayload && !legacySetup && !legacyBattle && !legacyProgress && !legacyTrainer && !legacySwitch && !legacyStages && !legacyWeather && !legacyRoom && !legacyInventory && !legacyParty && !legacyHeld && !legacyProfile && !legacyParticipants && !legacySetupParty && !legacyStatus && !legacyConfusion && !legacyConfusionSource && !legacyConfusionActor && !legacySurvival && !legacyBoss && !legacyGlobalRng)
        return NativeSaveResult::IncompatibleRuntime;
    if (!isHash(expectedContentHash)) return NativeSaveResult::InvalidFormat;
    if (!equal(value.contentHash, expectedContentHash)) return NativeSaveResult::ContentMismatch;
    Reader reader{bytes, length - kDigestLineLength, payloadStart};
    uint32_t parsed = 0;
    if (!reader.literal("stage=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
    value.stage = static_cast<NativeSaveStage>(parsed);
    if (!reader.literal("seed=") || !reader.hex(8, value.seed)
        || !reader.literal("wave=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
    value.wave = static_cast<uint16_t>(parsed);
    if (!reader.literal("starterDex=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
    value.starterDex = static_cast<uint16_t>(parsed);
    if (value.saveVersion >= 3) {
        if (!reader.literal("playerLevel=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
        value.playerLevel = static_cast<uint16_t>(parsed);
        if (!reader.literal("playerExperience=") || !reader.hex(8, value.playerExperience))
            return NativeSaveResult::InvalidFormat;
    } else if (!starterExperienceAtLevelFive(value.starterDex, value.playerExperience)) {
        return NativeSaveResult::InvalidRecord;
    }
    if (!reader.literal("mode=") || !reader.line(value.modeId, sizeof(value.modeId))
        || !reader.literal("biome=") || !reader.line(value.biomeId, sizeof(value.biomeId)))
        return NativeSaveResult::InvalidFormat;
    if (value.saveVersion == 1) {
        if (value.stage != NativeSaveStage::RunSetup) return NativeSaveResult::UnsupportedStage;
        if (value.saveVersion >= 14) {
            if (!reader.literal("setupStarterCount=") || !reader.hex(2, parsed) || parsed > 6)
                return NativeSaveResult::InvalidFormat;
            value.setupStarterCount = static_cast<uint8_t>(parsed);
            for (uint8_t i = 0; i < value.setupStarterCount; ++i) {
                if (!reader.literal("setupStarterDex=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
                value.setupStarterDexes[i] = static_cast<uint16_t>(parsed);
            }
        }
        if (reader.position != reader.end) return NativeSaveResult::InvalidFormat;
        // Version-one records represented setup only. Upgrade in memory; the
        // next journal write emits the current payload format.
        value.saveVersion = kNativeSaveVersion;
        value.runtimeVersion = kNativeSaveRuntimeVersion;
    } else {
        if (!reader.literal("encounterDex=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
        value.encounterDex = static_cast<uint16_t>(parsed);
        if (!reader.literal("playerHp=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
        value.playerHp = static_cast<uint16_t>(parsed);
        if (!reader.literal("enemyHp=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
        value.enemyHp = static_cast<uint16_t>(parsed);
        if (!reader.literal("battleTurn=") || !reader.hex(8, value.battleTurn) ||
            !reader.literal("playerMoveCount=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
        value.playerMoveCount = static_cast<uint8_t>(parsed);
        if (!reader.literal("enemyMoveCount=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
        value.enemyMoveCount = static_cast<uint8_t>(parsed);
        for (uint8_t i = 0; i < 4; ++i) {
            if (!reader.literal("playerMove=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
            value.playerMoveIds[i] = static_cast<uint16_t>(parsed);
            if (!reader.literal("playerPp=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
            value.playerPp[i] = static_cast<uint8_t>(parsed);
        }
        for (uint8_t i = 0; i < 4; ++i) {
            if (!reader.literal("enemyMove=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
            value.enemyMoveIds[i] = static_cast<uint16_t>(parsed);
            if (!reader.literal("enemyPp=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
            value.enemyPp[i] = static_cast<uint8_t>(parsed);
        }
        if (value.saveVersion >= 5 && (!reader.literal("enemySwitchCounter=") ||
            !reader.hex(8, value.enemySwitchCounter))) return NativeSaveResult::InvalidFormat;
        if (value.saveVersion >= 4) {
            if (!reader.literal("trainerType=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
            value.trainerTypeId = static_cast<uint16_t>(parsed);
            if (!reader.literal("trainerPartyCount=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
            value.trainerPartyCount = static_cast<uint8_t>(parsed);
            if (value.trainerPartyCount > 6) return NativeSaveResult::InvalidRecord;
            if (!reader.literal("activeTrainerMember=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
            value.activeTrainerMember = static_cast<uint8_t>(parsed);
            for (uint8_t member = 0; member < value.trainerPartyCount; ++member) {
                auto& record = value.trainerParty[member];
                if (!reader.literal("memberSpecies=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
                record.speciesDex = static_cast<uint16_t>(parsed);
                if (!reader.literal("memberHp=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
                record.hp = static_cast<uint16_t>(parsed);
                if (!reader.literal("memberMoveCount=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
                record.moveCount = static_cast<uint8_t>(parsed);
                for (uint8_t slot = 0; slot < 4; ++slot) {
                    if (!reader.literal("memberMove=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
                    record.moveIds[slot] = static_cast<uint16_t>(parsed);
                    if (!reader.literal("memberPp=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
                    record.pp[slot] = static_cast<uint8_t>(parsed);
                }
            }
        }
        if (value.saveVersion >= 6) {
            if (!reader.literal("playerStages=") || !reader.hex(8, parsed) ||
                !unpackStages(parsed, value.playerStatStages) ||
                !reader.literal("enemyStages=") || !reader.hex(8, parsed) ||
                !unpackStages(parsed, value.enemyStatStages)) return NativeSaveResult::InvalidFormat;
            for (uint8_t member = 0; member < value.trainerPartyCount; ++member)
                if (!reader.literal("memberStages=") || !reader.hex(8, parsed) ||
                    !unpackStages(parsed, value.trainerParty[member].statStages))
                    return NativeSaveResult::InvalidFormat;
        }
        if (value.saveVersion >= 7) {
            if (!reader.literal("weatherType=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
            value.weatherType = static_cast<uint8_t>(parsed);
            if (!reader.literal("weatherTurnsLeft=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
            value.weatherTurnsLeft = static_cast<uint16_t>(parsed);
            if (!reader.literal("weatherMaxDuration=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
            value.weatherMaxDuration = static_cast<uint16_t>(parsed);
        }
        if (value.saveVersion >= 8) {
            if (!reader.literal("trickRoomTurnsLeft=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
            value.trickRoomTurnsLeft = static_cast<uint16_t>(parsed);
            if (!reader.literal("trickRoomMaxDuration=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
            value.trickRoomMaxDuration = static_cast<uint16_t>(parsed);
            if (!reader.literal("trickRoomSourceMoveId=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
            value.trickRoomSourceMoveId = static_cast<uint16_t>(parsed);
            if (!reader.literal("trickRoomSourcePokemonId=") || !reader.hex(8, parsed)) return NativeSaveResult::InvalidFormat;
            value.trickRoomSourcePokemonId = parsed;
        }
        if (value.saveVersion >= 9) {
            for (uint8_t ball = 0; ball < 5; ++ball) {
                if (!reader.literal("pokeballCount=") || !reader.hex(4, parsed))
                    return NativeSaveResult::InvalidFormat;
                value.pokeballCounts[ball] = static_cast<uint16_t>(parsed);
            }
        }
        if (value.saveVersion >= 10) {
            if (!reader.literal("playerPartyCount=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
            value.playerPartyCount = static_cast<uint8_t>(parsed);
            if (value.playerPartyCount > 6) return NativeSaveResult::InvalidRecord;
            if (!reader.literal("activePlayerMember=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
            value.activePlayerMember = static_cast<uint8_t>(parsed);
            for (uint8_t member = 0; member < value.playerPartyCount; ++member) {
                if (!reader.literal("playerMemberBytes=") || !reader.hex(4, parsed) ||
                    parsed > reader.end - reader.position) return NativeSaveResult::InvalidFormat;
                const auto memberStatus = decodeNativePokemonSave(reader.bytes + reader.position, parsed,
                    value.playerParty[member]);
                if (memberStatus != NativeSaveResult::Ok) return memberStatus;
                reader.position += parsed;
            }
        }
        if (value.saveVersion >= 11) {
            if (!reader.literal("heldModifierCount=") || !reader.hex(2, parsed)) return NativeSaveResult::InvalidFormat;
            if (parsed > kNativeHeldModifierCapacity) return NativeSaveResult::InvalidRecord;
            value.heldModifierCount = static_cast<uint8_t>(parsed);
            for (uint8_t i = 0; i < value.heldModifierCount; ++i) {
                if (!reader.literal("heldModifierBytes=") || !reader.hex(4, parsed) ||
                    !parsed || parsed > 1024 || parsed > reader.end - reader.position)
                    return NativeSaveResult::InvalidFormat;
                const auto component = decodeNativeHeldModifier(reader.bytes + reader.position, parsed, value.heldModifiers[i]);
                if (component != NativeSaveResult::Ok) return component;
                reader.position += parsed;
            }
        }
        if (value.saveVersion >= 12 &&
            (!reader.literal("starterProfileGeneration=") ||
             !reader.hex(8, value.starterProfileGeneration))) return NativeSaveResult::InvalidFormat;
        if (value.saveVersion >= 13) {
            if (!reader.literal("participantHistoryResolved=") || !reader.hex(2, parsed) || parsed > 1)
                return NativeSaveResult::InvalidFormat;
            value.participantHistoryResolved = parsed != 0;
            if (!reader.literal("participantCount=") || !reader.hex(2, parsed) || parsed > 6)
                return NativeSaveResult::InvalidFormat;
            value.participantCount = static_cast<uint8_t>(parsed);
            for (uint8_t i = 0; i < value.participantCount; ++i)
                if (!reader.literal("participantId=") || !reader.hex(8, value.participantIds[i]))
                    return NativeSaveResult::InvalidFormat;
        }
        if (value.saveVersion >= 14) {
            if (!reader.literal("setupStarterCount=") || !reader.hex(2, parsed) || parsed > 6)
                return NativeSaveResult::InvalidFormat;
            value.setupStarterCount = static_cast<uint8_t>(parsed);
            for (uint8_t i = 0; i < value.setupStarterCount; ++i) {
                if (!reader.literal("setupStarterDex=") || !reader.hex(4, parsed)) return NativeSaveResult::InvalidFormat;
                value.setupStarterDexes[i] = static_cast<uint16_t>(parsed);
            }
        }
        if (value.saveVersion >= 15) {
            if (!reader.literal("playerStatus=") || !readStatus(reader, value.playerStatus) ||
                !reader.literal("enemyStatus=") || !readStatus(reader, value.enemyStatus)) return NativeSaveResult::InvalidFormat;
            for (uint8_t i = 0; i < value.trainerPartyCount; ++i)
                if (!reader.literal("memberStatus=") || !readStatus(reader, value.trainerParty[i].status))
                    return NativeSaveResult::InvalidFormat;
        }
        if (value.saveVersion >= 16) {
            if (!reader.literal("playerConfusion=") || !reader.hex(8, value.playerConfusion.turns) ||
                !reader.literal("enemyConfusion=") || !reader.hex(8, value.enemyConfusion.turns))
                return NativeSaveResult::InvalidFormat;
            value.playerConfusion.present = value.playerConfusion.turns != 0;
            value.enemyConfusion.present = value.enemyConfusion.turns != 0;
            for (uint8_t i = 0; i < value.trainerPartyCount; ++i) {
                if (!reader.literal("memberConfusion=") || !reader.hex(8, value.trainerParty[i].confusion.turns))
                    return NativeSaveResult::InvalidFormat;
                value.trainerParty[i].confusion.present = value.trainerParty[i].confusion.turns != 0;
            }
        }
        if (value.saveVersion >= 17) {
            const auto readSource = [&reader](const char* field, PokemonConfusionTagState& tag) {
                uint32_t parsedSource = 0;
                if (!reader.literal(field) || !reader.hex(1, parsedSource) || parsedSource > 1) return false;
                tag.sourceMoveResolved = parsedSource != 0;
                if (!reader.hex(4, parsedSource)) return false;
                tag.sourceMoveId = static_cast<uint16_t>(parsedSource);
                return validPokemonConfusionTag(tag);
            };
            if (!readSource("playerConfusionSource=", value.playerConfusion) ||
                !readSource("enemyConfusionSource=", value.enemyConfusion)) return NativeSaveResult::InvalidFormat;
            for (uint8_t i = 0; i < value.trainerPartyCount; ++i)
                if (!readSource("memberConfusionSource=", value.trainerParty[i].confusion))
                    return NativeSaveResult::InvalidFormat;
        }
        if (value.saveVersion >= 18) {
            const auto readActor = [&reader](const char* field, PokemonConfusionTagState& tag) {
                uint32_t resolved = 0;
                if (!reader.literal(field) || !reader.hex(1, resolved) || resolved > 1) return false;
                tag.sourcePokemonResolved = resolved != 0;
                return reader.hex(8, tag.sourcePokemonId) && validPokemonConfusionTag(tag);
            };
            if (!readActor("playerConfusionActor=", value.playerConfusion) ||
                !readActor("enemyConfusionActor=", value.enemyConfusion)) return NativeSaveResult::InvalidFormat;
            for (uint8_t i = 0; i < value.trainerPartyCount; ++i)
                if (!readActor("memberConfusionActor=", value.trainerParty[i].confusion))
                    return NativeSaveResult::InvalidFormat;
        }
        if (value.saveVersion >= 19) {
            const auto readSurvival = [&reader](const char* field, bool& tag) {
                uint32_t parsedTag = 0;
                if (!reader.literal(field) || !reader.hex(1, parsedTag) || parsedTag > 1) return false;
                tag = parsedTag != 0;
                return true;
            };
            if (!readSurvival("enemySturdy=", value.enemySturdyTag)) return NativeSaveResult::InvalidFormat;
            for (uint8_t i = 0; i < value.trainerPartyCount; ++i)
                if (!readSurvival("memberSturdy=", value.trainerParty[i].sturdyTag))
                    return NativeSaveResult::InvalidFormat;
        }
        if (value.saveVersion >= 20) {
            uint32_t count = 0, index = 0, firstPhase = 0, trainer = 0;
            if (!reader.literal("enemyBoss=") || !reader.hex(4, count) || !reader.hex(4, index) ||
                !reader.hex(1, firstPhase) || !reader.hex(1, trainer) || firstPhase > 1 || trainer > 1)
                return NativeSaveResult::InvalidFormat;
            value.enemyBoss = {static_cast<uint16_t>(count), static_cast<uint16_t>(index), firstPhase != 0, trainer != 0};
        }
        if (value.saveVersion >= 21) {
            if (!reader.literal("globalRng=") || !reader.hex(1, parsed) || parsed > 1)
                return NativeSaveResult::InvalidFormat;
            value.globalRngResolved = parsed != 0;
            if (value.globalRngResolved) {
                uint32_t carry = 0, s0 = 0, s1 = 0, s2 = 0;
                if (!reader.hex(8, carry) || !reader.hex(8, s0) || !reader.hex(8, s1) || !reader.hex(8, s2))
                    return NativeSaveResult::InvalidFormat;
                constexpr double unit = 1.0 / 4294967296.0;
                value.globalRng = {static_cast<double>(carry), s0 * unit, s1 * unit, s2 * unit};
            }
        }
        if (value.saveVersion >= 22) {
            if (!reader.literal("doubleBattle=") || !reader.hex(1, parsed) || parsed > 1)
                return NativeSaveResult::InvalidFormat;
            value.doubleBattle = parsed != 0;
            if (value.doubleBattle) {
                if (!reader.literal("secondEnemyBytes=") || !reader.hex(4, parsed) || !parsed ||
                    parsed > 512 || parsed > reader.end - reader.position) return NativeSaveResult::InvalidFormat;
                const auto status = decodeNativePokemonSave(reader.bytes + reader.position, parsed, value.secondEnemy);
                if (status != NativeSaveResult::Ok) return status;
                reader.position += parsed;
                uint32_t count = 0, index = 0, first = 0, trainer = 0;
                if (!reader.literal("secondEnemyBoss=") || !reader.hex(4, count) || !reader.hex(4, index) ||
                    !reader.hex(1, first) || !reader.hex(1, trainer) || first > 1 || trainer > 1)
                    return NativeSaveResult::InvalidFormat;
                value.secondEnemyBoss = {static_cast<uint16_t>(count), static_cast<uint16_t>(index), first != 0, trainer != 0};
                if (!reader.literal("doubleExperience=") || !reader.hex(1, parsed) || parsed > 3)
                    return NativeSaveResult::InvalidFormat;
                value.doubleExperienceGrantedMask = static_cast<uint8_t>(parsed);
                if (!reader.literal("selectedTarget=") || !reader.hex(1, parsed) || parsed > 1)
                    return NativeSaveResult::InvalidFormat;
                value.selectedTarget = static_cast<uint8_t>(parsed);
            }
        }
        // All version-specific fields, including confusion source metadata, must be
        // consumed before checking for trailing or missing payload bytes.
        if (reader.position != reader.end) return NativeSaveResult::InvalidFormat;
        if (legacyBattle || legacyProgress || legacyTrainer || legacySwitch || legacyStages || legacyWeather || legacyRoom || legacyInventory || legacyParty || legacyHeld || legacyProfile || legacyParticipants || legacySetupParty || legacyStatus || legacyConfusion || legacyConfusionSource || legacyConfusionActor || legacySurvival || legacyBoss || legacyGlobalRng) {
            value.saveVersion = kNativeSaveVersion;
            value.runtimeVersion = kNativeSaveRuntimeVersion;
        }
    }
    status = validateNativeRunSave(value, expectedContentHash);
    if (status == NativeSaveResult::Ok) output = value;
    return status;
}

NativeSaveResult NativeRunSaveStore::load(const char* contentHash, NativeRunSave& output) {
    std::unique_ptr<JournalSlot[]> slotsStorage(new (std::nothrow) JournalSlot[2]);
    if (!slotsStorage) return NativeSaveResult::MemoryUnavailable;
    auto* slots = slotsStorage.get();
    int selected = -1;
    const auto status = readJournal(m_storage, slots, selected);
    if (status != NativeSaveResult::Ok) return status;
    std::unique_ptr<NativeRunSave> candidateStorage(new (std::nothrow) NativeRunSave{});
    if (!candidateStorage) return NativeSaveResult::MemoryUnavailable;
    auto& candidate = *candidateStorage;
    auto decoded = decodeNativeRunSave(slots[selected].bytes, slots[selected].size, contentHash, candidate);
    if (decoded != NativeSaveResult::Ok) return decoded;
    if (candidate.starterProfileGeneration) {
        if (!m_profiles) return NativeSaveResult::InvalidRecord;
        decoded = m_profiles->inspectGeneration(contentHash, candidate.starterProfileGeneration);
        if (decoded != NativeSaveResult::Ok)
            return decoded == NativeSaveResult::NotFound ? NativeSaveResult::InvalidRecord : decoded;
    }
    output = candidate;
    return NativeSaveResult::Ok;
}

NativeSaveResult NativeRunSaveStore::save(const NativeRunSave& value) {
    auto status = validateNativeRunSave(value, value.contentHash);
    if (status == NativeSaveResult::Ok && value.starterProfileGeneration) {
        if (!m_profiles) return NativeSaveResult::InvalidRecord;
        status = m_profiles->inspectGeneration(value.contentHash, value.starterProfileGeneration);
        if (status == NativeSaveResult::NotFound) status = NativeSaveResult::InvalidRecord;
    }
    if (status != NativeSaveResult::Ok) return status;
    std::unique_ptr<JournalSlot[]> slotsStorage(new (std::nothrow) JournalSlot[2]);
    if (!slotsStorage) return NativeSaveResult::MemoryUnavailable;
    auto* slots = slotsStorage.get();
    int selected = -1;
    status = readJournal(m_storage, slots, selected);
    if (status != NativeSaveResult::Ok && status != NativeSaveResult::NotFound) return status;
    std::unique_ptr<NativeRunSave> nextStorage(new (std::nothrow) NativeRunSave(value));
    if (!nextStorage) return NativeSaveResult::MemoryUnavailable;
    auto& next = *nextStorage;
    next.generation = 1;
    if (selected >= 0) {
        std::unique_ptr<NativeRunSave> previousStorage(new (std::nothrow) NativeRunSave{});
        if (!previousStorage) return NativeSaveResult::MemoryUnavailable;
        auto& previous = *previousStorage;
        status = decodeNativeRunSave(slots[selected].bytes, slots[selected].size, value.contentHash, previous);
        if (status != NativeSaveResult::Ok) return status;
        if (previous.generation == 0xffffffffu) return NativeSaveResult::SequenceExhausted;
        next.generation = previous.generation + 1;
    }
    const unsigned target = selected == 0 ? 1 : 0;
    std::unique_ptr<char[]> bytesStorage(new (std::nothrow) char[kNativeSaveMaxBytes]);
    if (!bytesStorage) return NativeSaveResult::MemoryUnavailable;
    auto* bytes = bytesStorage.get();
    size_t size = 0;
    status = encodeNativeRunSave(next, bytes, kNativeSaveMaxBytes, size);
    if (status != NativeSaveResult::Ok) return status;
    status = m_storage.writeSlot(target, bytes, size);
    if (status != NativeSaveResult::Ok) return status;
    size_t verifiedSize = 0;
    status = m_storage.readSlot(target, slots[target].bytes, sizeof(slots[target].bytes), verifiedSize);
    if (status != NativeSaveResult::Ok) return status;
    if (verifiedSize != size) return NativeSaveResult::IoError;
    for (size_t i = 0; i < size; ++i) if (bytes[i] != slots[target].bytes[i]) return NativeSaveResult::IoError;
    return NativeSaveResult::Ok;
}

NativeSaveResult NativeRunSaveStore::exportLatest(const char* contentHash) {
    std::unique_ptr<NativeRunSave> valueStorage(new (std::nothrow) NativeRunSave{});
    if (!valueStorage) return NativeSaveResult::MemoryUnavailable;
    auto& value = *valueStorage;
    auto status = load(contentHash, value);
    if (status != NativeSaveResult::Ok) return status;
    std::unique_ptr<char[]> bytesStorage(new (std::nothrow) char[kNativeSaveMaxBytes]);
    if (!bytesStorage) return NativeSaveResult::MemoryUnavailable;
    auto* bytes = bytesStorage.get();
    size_t size = 0;
    status = encodeNativeRunSave(value, bytes, kNativeSaveMaxBytes, size);
    if (status != NativeSaveResult::Ok) return status;
    std::unique_ptr<char[]> verifiedStorage(new (std::nothrow) char[kNativeSaveMaxBytes]);
    if (!verifiedStorage) return NativeSaveResult::MemoryUnavailable;
    auto* verified = verifiedStorage.get();
    status = m_storage.writeExport(bytes, size);
    if (status != NativeSaveResult::Ok) return status;
    size_t verifiedSize = 0;
    status = m_storage.readExport(verified, kNativeSaveMaxBytes, verifiedSize);
    if (status != NativeSaveResult::Ok) return status;
    if (verifiedSize != size) return NativeSaveResult::IoError;
    for (size_t i = 0; i < size; ++i) if (verified[i] != bytes[i]) return NativeSaveResult::IoError;
    return NativeSaveResult::Ok;
}

NativeSaveResult NativeRunSaveStore::importExport(const char* contentHash) {
    std::unique_ptr<char[]> bytesStorage(new (std::nothrow) char[kNativeSaveMaxBytes]);
    if (!bytesStorage) return NativeSaveResult::MemoryUnavailable;
    auto* bytes = bytesStorage.get();
    size_t size = 0;
    auto status = m_storage.readExport(bytes, kNativeSaveMaxBytes, size);
    if (status != NativeSaveResult::Ok) return status;
    std::unique_ptr<NativeRunSave> valueStorage(new (std::nothrow) NativeRunSave{});
    if (!valueStorage) return NativeSaveResult::MemoryUnavailable;
    auto& value = *valueStorage;
    status = decodeNativeRunSave(bytes, size, contentHash, value);
    if (status != NativeSaveResult::Ok) return status;
    // A foreign profile sequence is not a local identity; paired import must rebase it.
    if (value.starterProfileGeneration) return NativeSaveResult::InvalidRecord;
    return save(value);
}

} // namespace Pokerogue3DS
