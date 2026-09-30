#include "storage/NativeRunSave.hpp"
#include "storage/IntegritySha256.hpp"
#include "game/PokemonExperience.hpp"
#include "game/PokerogueTurnOrder.hpp"
#include "content/PokerogueRuntimeContent.hpp"

namespace Pokerogue3DS {
namespace {
// Stable envelope marker; saveVersion carries the independently migrated
// payload schema (currently version 8).
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

const PokerogueContent::Species* freshStarter(uint16_t dex) {
    for (size_t i = 0; i < PokerogueContent::kSpeciesCount; ++i) {
        const auto& species = PokerogueContent::kSpecies[i];
        if (species.dex == dex && species.freshProfileStarter) return &species;
    }
    return nullptr;
}

bool starterExperienceAtLevelFive(uint16_t dex, uint32_t& experience) {
    const auto* starter = freshStarter(dex);
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

NativeSaveResult readJournal(NativeSaveStorage& storage, JournalSlot (&slots)[2], int& selected) {
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
    }
    return "Unknown save error";
}

NativeSaveResult makeNativeRunSetupSave(uint32_t seed, uint16_t starterDex, NativeRunSave& output) {
    NativeRunSave value{};
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

NativeSaveResult validateNativeRunSave(const NativeRunSave& save, const char* expectedContentHash) {
    if (save.saveVersion != kNativeSaveVersion) return NativeSaveResult::UnsupportedVersion;
    if (save.runtimeVersion != kNativeSaveRuntimeVersion) return NativeSaveResult::IncompatibleRuntime;
    const PokemonTrickRoomState room{save.trickRoomTurnsLeft, save.trickRoomMaxDuration,
        save.trickRoomSourceMoveId, save.trickRoomSourcePokemonId};
    if (!validPokemonTrickRoomState(room) ||
        (save.stage == NativeSaveStage::RunSetup && room.turnsLeft)) return NativeSaveResult::InvalidRecord;
    const bool immutableWeather = save.weatherType >= 7 && save.weatherType <= 9;
    if (save.weatherType > 9 || save.weatherTurnsLeft > save.weatherMaxDuration ||
        ((save.weatherType == 0 || immutableWeather) && (save.weatherTurnsLeft || save.weatherMaxDuration)) ||
        (save.stage == NativeSaveStage::RunSetup && save.weatherType != 0))
        return NativeSaveResult::InvalidRecord;
    if (!isHash(save.contentHash) || !isHash(expectedContentHash)) return NativeSaveResult::InvalidFormat;
    if (!equal(save.contentHash, expectedContentHash)) return NativeSaveResult::ContentMismatch;
    // Only the initial one-Pokemon, reward-skipping biome segment can be
    // replayed without a serialized party, modifier and biome history.
    if (!save.seed || !save.wave || save.wave > 9
        || !isId(save.modeId, sizeof(save.modeId))
        || !isId(save.biomeId, sizeof(save.biomeId)) || !equal(save.modeId, "classic"))
        return NativeSaveResult::InvalidRecord;
    if (!equal(save.biomeId, PokerogueContent::kStartingBiomeId))
        return NativeSaveResult::InvalidRecord;
    const auto* starter = freshStarter(save.starterDex);
    if (!starter) return NativeSaveResult::InvalidRecord;
    uint32_t initialExperience = 0, currentThreshold = 0;
    if (!starterExperienceAtLevelFive(save.starterDex, initialExperience)
        || save.playerLevel < 5 || save.playerLevel > classicExperienceLevelCap(save.wave)
        || pokemonTotalExperienceForLevel(starter->growthRate, save.playerLevel, currentThreshold)
            != PokemonExperienceResult::Ok
        || save.playerExperience < currentThreshold) return NativeSaveResult::InvalidRecord;
    if (save.playerLevel < classicExperienceLevelCap(save.wave)) {
        uint32_t nextThreshold = 0;
        if (pokemonTotalExperienceForLevel(starter->growthRate,
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
            ? hasLivingTrainerMember : !hasLivingTrainerMember)) return NativeSaveResult::InvalidRecord;
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
            (!save.playerHp || !save.enemyHp)) || (save.stage == NativeSaveStage::BattleWon &&
            (save.enemyHp || !save.playerHp)) || (save.stage == NativeSaveStage::ExperienceGranted &&
            (save.enemyHp || !save.playerHp)) || (save.stage == NativeSaveStage::BattleLost &&
            (save.playerHp || !save.enemyHp))) return NativeSaveResult::InvalidRecord;

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
            if (!move || move->pp < 0 || save.playerPp[i] > move->pp) return NativeSaveResult::InvalidRecord;
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
    NativeRunSave value{};
    size_t payloadStart = 0;
    auto status = inspectEnvelope(bytes, length, value, payloadStart);
    if (status != NativeSaveResult::Ok) return status;
    if (value.saveVersion > kNativeSaveVersion) return NativeSaveResult::UnsupportedVersion;
    if (value.saveVersion != 1 && value.saveVersion != 2 && value.saveVersion != 3 && value.saveVersion != 4 && value.saveVersion != 5 && value.saveVersion != 6 && value.saveVersion != 7 &&
        value.saveVersion != kNativeSaveVersion) return NativeSaveResult::UnsupportedVersion;
    const bool legacySetup = value.saveVersion == 1 && value.runtimeVersion == 1;
    const bool legacyBattle = value.saveVersion == 2 && value.runtimeVersion == 2;
    const bool legacyProgress = value.saveVersion == 3 && value.runtimeVersion == 3;
    const bool legacyTrainer = value.saveVersion == 4 && value.runtimeVersion == 4;
    const bool legacySwitch = value.saveVersion == 5 && value.runtimeVersion == 5;
    const bool legacyStages = value.saveVersion == 6 && value.runtimeVersion == 6;
    const bool legacyWeather = value.saveVersion == 7 && value.runtimeVersion == 7;
    const bool currentPayload = value.saveVersion == kNativeSaveVersion &&
        value.runtimeVersion == kNativeSaveRuntimeVersion;
    if (!currentPayload && !legacySetup && !legacyBattle && !legacyProgress && !legacyTrainer && !legacySwitch && !legacyStages && !legacyWeather)
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
        if (reader.position != reader.end) return NativeSaveResult::InvalidFormat;
        // Version-one records represented setup only. Upgrade in memory; the
        // next journal write emits the current version-seven format.
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
        if (reader.position != reader.end) return NativeSaveResult::InvalidFormat;
        if (legacyBattle || legacyProgress || legacyTrainer || legacySwitch || legacyStages || legacyWeather) {
            value.saveVersion = kNativeSaveVersion;
            value.runtimeVersion = kNativeSaveRuntimeVersion;
        }
    }
    status = validateNativeRunSave(value, expectedContentHash);
    if (status == NativeSaveResult::Ok) output = value;
    return status;
}

NativeSaveResult NativeRunSaveStore::load(const char* contentHash, NativeRunSave& output) {
    JournalSlot slots[2];
    int selected = -1;
    const auto status = readJournal(m_storage, slots, selected);
    if (status != NativeSaveResult::Ok) return status;
    return decodeNativeRunSave(slots[selected].bytes, slots[selected].size, contentHash, output);
}

NativeSaveResult NativeRunSaveStore::save(const NativeRunSave& value) {
    auto status = validateNativeRunSave(value, value.contentHash);
    if (status != NativeSaveResult::Ok) return status;
    JournalSlot slots[2];
    int selected = -1;
    status = readJournal(m_storage, slots, selected);
    if (status != NativeSaveResult::Ok && status != NativeSaveResult::NotFound) return status;
    NativeRunSave next = value;
    next.generation = 1;
    if (selected >= 0) {
        NativeRunSave previous{};
        status = decodeNativeRunSave(slots[selected].bytes, slots[selected].size, value.contentHash, previous);
        if (status != NativeSaveResult::Ok) return status;
        if (previous.generation == 0xffffffffu) return NativeSaveResult::SequenceExhausted;
        next.generation = previous.generation + 1;
    }
    const unsigned target = selected == 0 ? 1 : 0;
    char bytes[kNativeSaveMaxBytes];
    size_t size = 0;
    status = encodeNativeRunSave(next, bytes, sizeof(bytes), size);
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
    NativeRunSave value{};
    auto status = load(contentHash, value);
    if (status != NativeSaveResult::Ok) return status;
    char bytes[kNativeSaveMaxBytes];
    size_t size = 0;
    status = encodeNativeRunSave(value, bytes, sizeof(bytes), size);
    if (status != NativeSaveResult::Ok) return status;
    status = m_storage.writeExport(bytes, size);
    if (status != NativeSaveResult::Ok) return status;
    char verified[kNativeSaveMaxBytes];
    size_t verifiedSize = 0;
    status = m_storage.readExport(verified, sizeof(verified), verifiedSize);
    if (status != NativeSaveResult::Ok) return status;
    if (verifiedSize != size) return NativeSaveResult::IoError;
    for (size_t i = 0; i < size; ++i) if (verified[i] != bytes[i]) return NativeSaveResult::IoError;
    return NativeSaveResult::Ok;
}

NativeSaveResult NativeRunSaveStore::importExport(const char* contentHash) {
    char bytes[kNativeSaveMaxBytes];
    size_t size = 0;
    auto status = m_storage.readExport(bytes, sizeof(bytes), size);
    if (status != NativeSaveResult::Ok) return status;
    NativeRunSave value{};
    status = decodeNativeRunSave(bytes, size, contentHash, value);
    if (status != NativeSaveResult::Ok) return status;
    return save(value);
}

} // namespace Pokerogue3DS
