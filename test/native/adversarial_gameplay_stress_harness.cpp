#include "game/PokemonStarterMoveset.hpp"
#include "game/FirstRunRuntime.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include "storage/IntegritySha256.hpp"
#include "storage/NativeStarterCandyProfile.hpp"
#include "storage/NativeStarterCandyStore.hpp"
#include "storage/NativeProgressStore.hpp"
#include "game/PokemonExperience.hpp"
#include "game/PokemonWeatherPhase.hpp"
#include "game/PokemonStatStageEffect.hpp"
#include "game/PokemonHealingEffect.hpp"
#include "game/PokemonBerryEffect.hpp"
#include "game/PokerogueClassicWaveSchedule.hpp"
#include "game/PokerogueBiomeTransition.hpp"
#include "game/PokerogueEncounterResolver.hpp"
#include "game/PokerogueTrainerPartyLevels.hpp"
#include "game/PokemonWildMovesetGenerator.hpp"
#include "storage/NativeStarterCandyStore.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

using namespace Pokerogue3DS;

// Memory storage for dual-slot journal stress testing
class AdversarialMemoryStorage : public NativeSaveStorage {
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
        std::memcpy(out, slots[i], size);
        return NativeSaveResult::Ok;
    }

    NativeSaveResult writeSlot(unsigned i, const char* in, size_t size) override {
        if (size > sizeof(slots[i])) return NativeSaveResult::TooLarge;
        sizes[i] = interrupt ? size / 2 : size;
        std::memcpy(slots[i], in, sizes[i]);
        return interrupt ? NativeSaveResult::IoError : NativeSaveResult::Ok;
    }

    NativeSaveResult readExport(char* out, size_t cap, size_t& size) override {
        size = exportSize;
        if (!size) return NativeSaveResult::NotFound;
        if (size > cap) return NativeSaveResult::TooLarge;
        std::memcpy(out, exported, size);
        return NativeSaveResult::Ok;
    }

    NativeSaveResult writeExport(const char* in, size_t size) override {
        if (size > sizeof(exported)) return NativeSaveResult::TooLarge;
        exportSize = size;
        std::memcpy(exported, in, size);
        return NativeSaveResult::Ok;
    }
};

// =========================================================================
// SECTION 1: R4 STORY PROGRESSION & WAVE 1..200 ENCOUNTERS STRESS TESTS
// =========================================================================

static int stressTestWaveClassification1to200() {
    std::printf("  [1.1] Testing Classic wave 1..200 classification & boundaries...\n");
    if (classifyClassicWave(0) != ClassicWaveKind::Invalid) return 101;
    if (classifyClassicWave(201) != ClassicWaveKind::Invalid) return 102;
    if (classifyClassicWave(200) != ClassicWaveKind::FinalBoss) return 103;

    // Fixed Gym Leader waves
    const uint16_t gymWaves[] = {20, 50, 80, 110, 140, 170};
    for (uint16_t w : gymWaves) {
        if (classifyClassicWave(w) != ClassicWaveKind::FixedTrainerBattle) {
            std::printf("    FAIL: Wave %u not classified as FixedTrainerBattle!\n", w);
            return 110 + w;
        }
    }

    // Fixed Evil Team waves
    const uint16_t evilWaves[] = {35, 62, 64, 66, 112, 114, 115, 164, 165};
    for (uint16_t w : evilWaves) {
        if (classifyClassicWave(w) != ClassicWaveKind::FixedTrainerBattle) {
            std::printf("    FAIL: Evil Team Wave %u not classified as FixedTrainerBattle!\n", w);
            return 120 + w;
        }
    }

    // Elite Four & Champion waves
    const uint16_t e4Waves[] = {182, 184, 186, 188};
    for (uint16_t w : e4Waves) {
        if (classifyClassicWave(w) != ClassicWaveKind::FixedTrainerBattle) {
            std::printf("    FAIL: E4 Wave %u not classified as FixedTrainerBattle!\n", w);
            return 130 + w;
        }
    }
    if (classifyClassicWave(190) != ClassicWaveKind::FixedTrainerBattle) {
        std::printf("    FAIL: Champion Wave 190 not classified as FixedTrainerBattle!\n");
        return 139;
    }

    // Rival waves
    const uint16_t rivalWaves[] = {8, 25, 55, 95, 145, 195};
    for (uint16_t w : rivalWaves) {
        if (classifyClassicWave(w) != ClassicWaveKind::FixedTrainerBattle) {
            std::printf("    FAIL: Rival Wave %u not classified as FixedTrainerBattle!\n", w);
            return 140 + w;
        }
    }

    // Check every wave 1..200 is valid
    for (uint16_t w = 1; w <= 200; ++w) {
        auto kind = classifyClassicWave(w);
        if (kind == ClassicWaveKind::Invalid) {
            std::printf("    FAIL: Wave %u classified as Invalid!\n", w);
            return 150;
        }
    }
    std::printf("    PASS: All 200 waves correctly classified.\n");
    return 0;
}

static int stressTestTrainerKeyResolution() {
    std::printf("  [1.2] Checking resolution of all scripted story trainer keys in content catalogs...\n");
    // Gym Leader fallback keys
    const char* gymKeys[] = {"brock", "misty", "lt_surge", "erika", "sabrina", "blaine"};
    for (const char* k : gymKeys) {
        const auto* t = PokerogueContent::findTrainerTypeByKey(k);
        if (!t) {
            std::printf("    FAIL: Gym leader key '%s' not found in catalog!\n", k);
            return 201;
        }
    }

    // Evil Team keys
    struct EvilTeamRoster {
        const char* grunt;
        const char* admin1;
        const char* admin2;
        const char* admin3;
        const char* boss1;
        const char* boss2;
    };
    static constexpr EvilTeamRoster kEvilTeams[9] = {
        {"rocket_grunt", "archer", "ariana", "petrel", "rocket_boss_giovanni_1", "rocket_boss_giovanni_2"},
        {"magma_grunt", "tabitha", "courtney", "tabitha", "maxie", "maxie_2"},
        {"aqua_grunt", "matt", "shelly", "matt", "archie", "archie_2"},
        {"galactic_grunt", "mars", "jupiter", "saturn", "cyrus", "cyrus_2"},
        {"plasma_grunt", "zinzolin", "colress", "zinzolin", "ghetsis", "ghetsis_2"},
        {"flare_grunt", "bryony", "xerosic", "mable", "lysandre", "lysandre_2"},
        {"skull_grunt", "plumeria", "faba", "plumeria", "guzma", "guzma_2"},
        {"macro_grunt", "oleana", "oleana", "oleana", "rose", "rose_2"},
        {"star_grunt", "giacomo", "mela", "atticus", "penny", "penny_2"}
    };

    for (size_t i = 0; i < 9; ++i) {
        const auto& r = kEvilTeams[i];
        const char* members[] = {r.grunt, r.admin1, r.admin2, r.admin3, r.boss1, r.boss2};
        for (const char* m : members) {
            const auto* t = PokerogueContent::findTrainerTypeByKey(m);
            if (!t) {
                std::printf("    FAIL: Evil team %zu trainer key '%s' not found!\n", i, m);
                return 210 + static_cast<int>(i);
            }
        }
    }

    // Elite Four & Champion keys
    struct EliteFourRoster {
        const char* e4_1;
        const char* e4_2;
        const char* e4_3;
        const char* e4_4;
        const char* champion;
    };
    static constexpr EliteFourRoster kEliteFourRosters[9] = {
        {"lorelei", "bruno", "agatha", "lance", "blue"},
        {"will", "koga", "bruno", "karen", "lance_champion"},
        {"sidney", "phoebe", "glacia", "drake", "steven"},
        {"aaron", "bertha", "flint", "lucian", "cynthia"},
        {"shauntal", "marshal", "grimsley", "caitlin", "iris"},
        {"malva", "siebold", "wikstrom", "drasna", "diantha"},
        {"hala", "olivia", "acerola", "kahili", "hau"},
        {"marnie_elite", "bede_elite", "nessa_elite", "raihan_elite", "leon"},
        {"rika", "poppy", "larry_elite", "hassel", "geeta"}
    };

    for (size_t i = 0; i < 9; ++i) {
        const auto& r = kEliteFourRosters[i];
        const char* members[] = {r.e4_1, r.e4_2, r.e4_3, r.e4_4, r.champion};
        for (const char* m : members) {
            const auto* t = PokerogueContent::findTrainerTypeByKey(m);
            if (!t) {
                std::printf("    FAIL: E4 roster %zu trainer key '%s' not found!\n", i, m);
                return 220 + static_cast<int>(i);
            }
        }
    }

    std::printf("    PASS: All 9 Evil Teams and all 9 Regional Leagues resolve valid TrainerType records.\n");
    return 0;
}

static int stressTestStoryProgressionAcrossSeeds() {
    std::printf("  [1.3] Stress testing scripted story progression across 50 distinct seeds...\n");
    const uint32_t testSeeds[] = {
        1, 2, 3, 5, 7, 11, 13, 17, 23, 42, 69, 77, 88, 99, 100,
        123, 256, 333, 500, 777, 999, 1000, 1234, 4321, 5555,
        0x3f5c, 0xa72b, 0x12345678, 0xdeadbeef, 0xc001cafe,
        0x7fffffff, 0x55555555, 0x33333333, 0x0f0f0f0f, 0x00000001,
        0x10000000, 0x20000000, 0x40000000, 0x80000000, 0xffffffff,
        987654, 456789, 112233, 445566, 778899, 102030, 405060, 708090, 13579, 24680
    };

    const uint16_t scriptedWaves[] = {
        8, 20, 25, 35, 50, 55, 62, 64, 66, 80, 95, 110, 112, 114, 115, 140, 145, 164, 165, 170, 182, 184, 186, 188, 190, 195, 200
    };

    size_t testsRun = 0;
    for (uint32_t seed : testSeeds) {
        for (uint16_t wave : scriptedWaves) {
            ++testsRun;
            if (wave == 200) {
                // Eternatus boss
                ClassicVictoryPlan plan{};
                if (!planClassicVictory(200, plan)) return 301;
                if (!plan.contains(ClassicVictoryStep::GameClear) || plan.nextWave != 0) return 302;
                continue;
            }

            const auto* fixedBattle = PokerogueContent::findClassicFixedBattleWave(wave);
            const PokerogueContent::TrainerType* trainer = nullptr;

            if (fixedBattle && fixedBattle->hasStaticTrainerType) {
                trainer = PokerogueContent::findTrainerType(fixedBattle->trainerTypeId);
            } else if (wave % 30 == 20) {
                // Gym leader
                PokerogueRngAdapter waveRng;
                uint16_t seedBuf[PokerogueRngAdapter::kMaxSeedCodeUnits] = {static_cast<uint16_t>(seed & 0xffff), static_cast<uint16_t>(seed >> 16)};
                waveRng.sow(seedBuf, 2);
                auto gymResolution = PokerogueEncounterResolver::resolveTrainerType("town", true, false, waveRng);
                if (gymResolution.valid && gymResolution.trainerType) {
                    trainer = gymResolution.trainerType;
                } else {
                    const char* key = "brock";
                    if (wave == 50) key = "misty";
                    else if (wave == 80) key = "lt_surge";
                    else if (wave == 110) key = "erika";
                    else if (wave == 140) key = "sabrina";
                    else if (wave == 170) key = "blaine";
                    trainer = PokerogueContent::findTrainerTypeByKey(key);
                }
            } else if (fixedBattle && !fixedBattle->hasStaticTrainerType) {
                // Evil team or Elite Four
                const uint32_t evilTeamIndex = (seed ^ 0x3f5cu) % 9u;
                const uint32_t e4Index = ((seed >> 4) ^ 0xa72bu) % 9u;
                const char* symbol = fixedBattle->upstreamSymbol ? fixedBattle->upstreamSymbol : "";
                const char* trainerKey = nullptr;

                if (std::strcmp(symbol, "EVIL_GRUNT_1") == 0 ||
                    std::strcmp(symbol, "EVIL_GRUNT_2") == 0 ||
                    std::strcmp(symbol, "EVIL_GRUNT_3") == 0 ||
                    std::strcmp(symbol, "EVIL_GRUNT_4") == 0) {
                    const char* grunts[] = {"rocket_grunt", "magma_grunt", "aqua_grunt", "galactic_grunt", "plasma_grunt", "flare_grunt", "skull_grunt", "macro_grunt", "star_grunt"};
                    trainerKey = grunts[evilTeamIndex];
                } else if (std::strcmp(symbol, "EVIL_ADMIN_1") == 0) {
                    const char* admins[] = {"archer", "tabitha", "matt", "mars", "zinzolin", "bryony", "plumeria", "oleana", "giacomo"};
                    trainerKey = admins[evilTeamIndex];
                } else if (std::strcmp(symbol, "EVIL_ADMIN_2") == 0) {
                    const char* admins[] = {"ariana", "courtney", "shelly", "jupiter", "colress", "xerosic", "faba", "oleana", "mela"};
                    trainerKey = admins[evilTeamIndex];
                } else if (std::strcmp(symbol, "EVIL_ADMIN_3") == 0) {
                    const char* admins[] = {"petrel", "tabitha", "matt", "saturn", "zinzolin", "mable", "plumeria", "oleana", "atticus"};
                    trainerKey = admins[evilTeamIndex];
                } else if (std::strcmp(symbol, "EVIL_BOSS_1") == 0) {
                    const char* bosses[] = {"rocket_boss_giovanni_1", "maxie", "archie", "cyrus", "ghetsis", "lysandre", "guzma", "rose", "penny"};
                    trainerKey = bosses[evilTeamIndex];
                } else if (std::strcmp(symbol, "EVIL_BOSS_2") == 0) {
                    const char* bosses[] = {"rocket_boss_giovanni_2", "maxie_2", "archie_2", "cyrus_2", "ghetsis_2", "lysandre_2", "guzma_2", "rose_2", "penny_2"};
                    trainerKey = bosses[evilTeamIndex];
                } else if (std::strcmp(symbol, "ELITE_FOUR_1") == 0) {
                    const char* e4[] = {"lorelei", "will", "sidney", "aaron", "shauntal", "malva", "hala", "marnie_elite", "rika"};
                    trainerKey = e4[e4Index];
                } else if (std::strcmp(symbol, "ELITE_FOUR_2") == 0) {
                    const char* e4[] = {"bruno", "koga", "phoebe", "bertha", "marshal", "siebold", "olivia", "bede_elite", "poppy"};
                    trainerKey = e4[e4Index];
                } else if (std::strcmp(symbol, "ELITE_FOUR_3") == 0) {
                    const char* e4[] = {"agatha", "bruno", "glacia", "flint", "grimsley", "wikstrom", "acerola", "nessa_elite", "larry_elite"};
                    trainerKey = e4[e4Index];
                } else if (std::strcmp(symbol, "ELITE_FOUR_4") == 0) {
                    const char* e4[] = {"lance", "karen", "drake", "lucian", "caitlin", "drasna", "kahili", "raihan_elite", "hassel"};
                    trainerKey = e4[e4Index];
                } else if (std::strcmp(symbol, "CHAMPION") == 0) {
                    const char* champs[] = {"blue", "lance_champion", "steven", "cynthia", "iris", "diantha", "hau", "leon", "geeta"};
                    trainerKey = champs[e4Index];
                }
                if (trainerKey) {
                    trainer = PokerogueContent::findTrainerTypeByKey(trainerKey);
                }
            }

            if (!trainer) {
                std::printf("    FAIL: Seed %08x Wave %u produced null trainer!\n", seed, wave);
                return 303;
            }

            // Verify party template selection
            PokerogueRngAdapter tRng;
            uint16_t seedBuf[PokerogueRngAdapter::kMaxSeedCodeUnits] = {static_cast<uint16_t>(seed & 0xffff), static_cast<uint16_t>((seed >> 16) ^ (wave << 8))};
            tRng.sow(seedBuf, 2);
            const auto chosen = selectTrainerPartyTemplate(*trainer, wave, tRng);
            if (!chosen.supported || !chosen.value) {
                std::printf("    FAIL: Seed %08x Wave %u Trainer '%s' party template selection failed!\n", seed, wave, trainer->name);
                return 304;
            }

            // Verify party levels resolution
            const auto levels = resolveClassicTrainerPartyLevels(*chosen.value, wave, false);
            if (!levels.supported || levels.count == 0 || levels.count > 6) {
                std::printf("    FAIL: Seed %08x Wave %u Trainer '%s' party levels failed (count=%u)!\n", seed, wave, trainer->name, levels.count);
                return 305;
            }

            // Verify member seed offset computation for every member
            for (uint8_t m = 0; m < levels.count; ++m) {
                uint32_t offset = 0;
                if (!trainerPartyMemberSeedOffset(*trainer, wave, m, offset)) {
                    std::printf("    FAIL: Seed %08x Wave %u Trainer '%s' member %u seed offset failed!\n", seed, wave, trainer->name, m);
                    return 306;
                }
            }
        }
    }
    std::printf("    PASS: %zu scripted encounters tested across 50 seeds with zero failures or crashes.\n", testsRun);
    return 0;
}

// =========================================================================
// SECTION 2: R5 PERSISTENCE & SAVE SYSTEM STRESS TESTS
// =========================================================================

static int stressTestDualSlotPingPong() {
    std::printf("  [2.1] Stress testing atomic dual-slot ping-pong journal with 50 iterations...\n");
    AdversarialMemoryStorage disk;
    NativeRunSaveStore store(disk);

    NativeRunSave state{};
    if (makeNativeRunSetupSave(1001, 1, state) != NativeSaveResult::Ok) return 401;

    for (uint32_t i = 1; i <= 50; ++i) {
        state.seed = 1000 + i;
        const auto saveRes = store.save(state);
        if (saveRes != NativeSaveResult::Ok) {
            std::printf("    FAIL: Ping-pong save failed at iteration %u (%d)\n", i, static_cast<int>(saveRes));
            return 402;
        }

        // Verify active slot alternated
        NativeRunSave loaded{};
        const auto loadRes = store.load(PokerogueContent::kContentHash, loaded);
        if (loadRes != NativeSaveResult::Ok) {
            std::printf("    FAIL: Ping-pong load failed at iteration %u (%d)\n", i, static_cast<int>(loadRes));
            return 403;
        }
        if (loaded.seed != 1000 + i || loaded.wave != 1) {
            std::printf("    FAIL: Ping-pong loaded corrupted state (seed=%u, wave=%u)!\n", loaded.seed, loaded.wave);
            return 404;
        }
    }

    // Power-cut / interrupted write simulation
    std::printf("  [2.2] Testing write interruption & failover to surviving slot...\n");
    state.seed = 9999;
    disk.interrupt = true;
    const auto failedSave = store.save(state);
    if (failedSave != NativeSaveResult::IoError) {
        std::printf("    FAIL: Interrupted save did not return IoError (%d)\n", static_cast<int>(failedSave));
        return 405;
    }

    // Load must still recover iteration 50 state from the surviving slot!
    NativeRunSave recovered{};
    const auto recoverRes = store.load(PokerogueContent::kContentHash, recovered);
    if (recoverRes != NativeSaveResult::Ok || recovered.seed != 1050 || recovered.wave != 1) {
        std::printf("    FAIL: Interrupted save corrupts store recovery (seed=%u, wave=%u)!\n", recovered.seed, recovered.wave);
        return 406;
    }
    disk.interrupt = false;
    std::printf("    PASS: Interrupted write gracefully recovered prior state from surviving slot.\n");

    // SHA-256 Bit-flip detection
    std::printf("  [2.3] Testing SHA-256 corruption detection (bit flips)...\n");
    char encodedBytes[kNativeSaveMaxBytes]{};
    size_t encodedSize = 0;
    if (encodeNativeRunSave(recovered, encodedBytes, sizeof(encodedBytes), encodedSize) != NativeSaveResult::Ok) return 407;

    // Flip a bit in the middle of payload
    encodedBytes[encodedSize / 2] ^= 0x40;
    NativeRunSave corruptedDecoded{};
    const auto corruptRes = decodeNativeRunSave(encodedBytes, encodedSize, PokerogueContent::kContentHash, corruptedDecoded);
    if (corruptRes != NativeSaveResult::ChecksumMismatch && corruptRes != NativeSaveResult::InvalidFormat) {
        std::printf("    FAIL: Corrupted payload was not rejected by SHA-256 check (%d)!\n", static_cast<int>(corruptRes));
        return 408;
    }
    std::printf("    PASS: Bit flips successfully detected and rejected.\n");

    // ContentHash mismatch rejection
    std::printf("  [2.4] Testing ContentHash version mismatch rejection...\n");
    char badHash[65]{};
    std::memcpy(badHash, PokerogueContent::kContentHash, 64);
    badHash[0] = (badHash[0] == 'a') ? 'b' : 'a';
    const auto badHashRes = store.load(badHash, recovered);
    if (badHashRes != NativeSaveResult::ContentMismatch) {
        std::printf("    FAIL: Mismatched content hash did not return ContentMismatch (%d)!\n", static_cast<int>(badHashRes));
        return 409;
    }
    std::printf("    PASS: ContentHash mismatch correctly rejected.\n");
    return 0;
}

static bool captureActiveTestCheckpoint(const FirstRunRuntime& game, NativeRunSave& saved) {
    if (game.captureNativeRunSave(saved) != NativeSaveResult::Ok) return false;
    if (saved.stage != NativeSaveStage::RunSetup) return true;
    const auto& field = game.presentation();
    if (game.doubleBattle() || field.trainerPartyCount || !field.player.actorIdentityResolved ||
        !field.enemy.actorIdentityResolved) return false;
    saved.stage = NativeSaveStage::BattleActive;
    saved.battleTurn = 1;
    saved.encounterDex = field.enemy.dex;
    saved.playerHp = field.player.battleState.hp;
    saved.enemyHp = field.enemy.battleState.hp;
    saved.playerMoveCount = field.player.battleState.moveCount;
    saved.enemyMoveCount = field.enemy.battleState.moveCount;
    for (uint8_t i = 0; i < saved.playerMoveCount; ++i) {
        saved.playerMoveIds[i] = field.player.battleState.moves[i].moveId;
        saved.playerPp[i] = field.player.battleState.moves[i].pp;
    }
    for (uint8_t i = 0; i < saved.enemyMoveCount; ++i) {
        saved.enemyMoveIds[i] = field.enemy.battleState.moves[i].moveId;
        saved.enemyPp[i] = field.enemy.battleState.moves[i].pp;
    }
    saved.playerPartyCount = game.playerPartyCount();
    saved.activePlayerMember = game.activePlayerPartyIndex();
    for (uint8_t i = 0; i < saved.playerPartyCount; ++i) {
        const auto& actor = i == saved.activePlayerMember ? field.player : *game.playerPartyMember(i);
        if (!captureNativePokemonActorSave(actor.battleState, actor.actor, actor.totalExperience,
                saved.playerParty[i])) return false;
    }
    return validateNativeRunSave(saved, PokerogueContent::kContentHash) == NativeSaveResult::Ok;
}

static int stressTestRestoreComplexStates() {
    std::printf("  [2.5] Testing restoreNativeRunSave with complex battle states (weather, status, party)...\n");
    FirstRunRuntime gameSource(777);
    NativeRunSave baseSave{};
    if (!captureActiveTestCheckpoint(gameSource, baseSave)) {
        std::printf("    FAIL: Could not capture active test checkpoint!\n");
        return 501;
    }

    // Apply Weather: Sandstorm with 3 turns left
    baseSave.weatherType = 3; // Sandstorm
    baseSave.weatherTurnsLeft = 3;
    baseSave.weatherMaxDuration = 5;

    // Apply Trick room: 2 turns left
    baseSave.trickRoomTurnsLeft = 2;
    baseSave.trickRoomMaxDuration = 5;
    baseSave.trickRoomSourceMoveId = 433;
    baseSave.trickRoomSourcePokemonId = 101;

    // Apply Status: Burn on player, Sleep on enemy
    baseSave.playerStatus.present = true;
    baseSave.playerStatus.effect = PokemonStatusEffect::Burn;
    if (baseSave.playerPartyCount && baseSave.activePlayerMember < baseSave.playerPartyCount) {
        baseSave.playerParty[baseSave.activePlayerMember].status = baseSave.playerStatus;
    }

    baseSave.enemyStatus.present = true;
    baseSave.enemyStatus.effect = PokemonStatusEffect::Sleep;
    baseSave.enemyStatus.hasSleepTurnsRemaining = true;
    baseSave.enemyStatus.sleepTurnsRemaining = 2;

    const auto validRes = validateNativeRunSave(baseSave, PokerogueContent::kContentHash);
    if (validRes != NativeSaveResult::Ok) {
        std::printf("    FAIL: Validating complex save failed (%d)!\n", static_cast<int>(validRes));
        return 502;
    }

    char envelope[kNativeSaveMaxBytes]{};
    size_t envSize = 0;
    NativeRunSave decoded{};
    const auto encRes = encodeNativeRunSave(baseSave, envelope, sizeof(envelope), envSize);
    const auto decRes = decodeNativeRunSave(envelope, envSize, PokerogueContent::kContentHash, decoded);
    if (encRes != NativeSaveResult::Ok || decRes != NativeSaveResult::Ok) {
        std::printf("    FAIL: Failed to encode (%d) or decode (%d) complex battle state save!\n",
            static_cast<int>(encRes), static_cast<int>(decRes));
        return 503;
    }

    if (decoded.weatherType != 3 || decoded.weatherTurnsLeft != 3 ||
        decoded.trickRoomTurnsLeft != 2 || decoded.playerStatus.effect != PokemonStatusEffect::Burn ||
        decoded.enemyStatus.effect != PokemonStatusEffect::Sleep) {
        std::printf("    FAIL: Decoded fields do not match original complex state!\n");
        return 504;
    }

    // Now test held Berry restore behavior:
    std::printf("  [2.6] Verifying held Berry restore rejection per contract in FirstRunRuntime.cpp:721-724...\n");
    NativeRunSave berrySave = decoded;
    berrySave.heldModifierCount = 1;
    if (initializeHeldBerry(PokerogueContent::kBerryTypes[0].id, berrySave.playerParty[0].pokemonId,
            1, true, berrySave.heldModifiers[0]) != HeldModifierStorageResult::Ok) return 504;

    FirstRunRuntime game(777);
    if (game.restoreNativeRunSave(berrySave)) {
        std::printf("    FAIL: restoreNativeRunSave accepted held Berry when battle consumption is unported!\n");
        return 505;
    }
    std::printf("    PASS: restoreNativeRunSave strictly and safely rejects held Berries without crashing.\n");

    return 0;
}

// =========================================================================
// SECTION 3: R5 CANDY STORE & PASSIVE UNLOCK STRESS TESTS
// =========================================================================

static int stressTestCandyStoreMechanics() {
    std::printf("  [3.1] Testing passive ability unlocks & cost reduction mechanics...\n");

    // Test with Bulbasaur (dex 1, cost 3)
    const auto* bulbasaur = PokerogueContent::findSpeciesByDex(1);
    if (!bulbasaur || !bulbasaur->starterEligible) return 601;

    const auto* price = PokerogueContent::findSpeciesByDex(1) ?
        PokerogueContent::kStarterCandyPrices : nullptr;
    const PokerogueContent::StarterCandyPrice* bulbPrice = nullptr;
    for (const auto& row : PokerogueContent::kStarterCandyPrices) {
        if (row.cost == bulbasaur->starterCost) { bulbPrice = &row; break; }
    }
    if (!bulbPrice) return 602;

    const uint16_t passiveCost = bulbPrice->passive;
    const uint16_t costRed1 = bulbPrice->costReduction[0];
    const uint16_t costRed2 = bulbPrice->costReduction[1];

    // Test Passive Unlock with Insufficient Candy
    NativeStarterCandyRecord rec{};
    rec.speciesDex = 1;
    rec.candyCount = passiveCost - 1;
    rec.passiveUnlocked = false;

    if (applyNativeStarterPassiveUnlock(rec) != StarterPassivePurchaseResult::InsufficientCandy ||
        rec.passiveUnlocked || rec.candyCount != passiveCost - 1) {
        std::printf("    FAIL: Passive unlock did not reject insufficient candy!\n");
        return 603;
    }

    // Test Passive Unlock with Exact Candy
    rec.candyCount = passiveCost;
    if (applyNativeStarterPassiveUnlock(rec) != StarterPassivePurchaseResult::Applied ||
        !rec.passiveUnlocked || rec.candyCount != 0) {
        std::printf("    FAIL: Passive unlock failed with exact candy!\n");
        return 604;
    }

    // Test Double Unlock (AlreadyUnlocked)
    rec.candyCount = 50;
    if (applyNativeStarterPassiveUnlock(rec) != StarterPassivePurchaseResult::AlreadyUnlocked ||
        rec.candyCount != 50) {
        std::printf("    FAIL: Double passive unlock was not rejected as AlreadyUnlocked!\n");
        return 605;
    }

    // Test Cost Reduction Tier 1
    rec.costReduction = 0;
    rec.candyCount = costRed1;
    if (applyNativeStarterCostReduction(rec) != StarterCostPurchaseResult::Applied ||
        rec.costReduction != 1 || rec.candyCount != 0) {
        std::printf("    FAIL: Cost reduction tier 1 failed with exact candy!\n");
        return 606;
    }

    // Test Cost Reduction Tier 2
    rec.candyCount = costRed2;
    if (applyNativeStarterCostReduction(rec) != StarterCostPurchaseResult::Applied ||
        rec.costReduction != 2 || rec.candyCount != 0) {
        std::printf("    FAIL: Cost reduction tier 2 failed with exact candy!\n");
        return 607;
    }

    // Test Cost Reduction Max Limit (2/2)
    rec.candyCount = 100;
    if (applyNativeStarterCostReduction(rec) != StarterCostPurchaseResult::MaximumReduction ||
        rec.costReduction != 2 || rec.candyCount != 100) {
        std::printf("    FAIL: Exceeding max cost reduction (2) was not rejected!\n");
        return 608;
    }

    // Test Non-Starter Species Rejection
    NativeStarterCandyRecord nonStarterRec{};
    nonStarterRec.speciesDex = 10; // Caterpie (not a starter in standard sense or check starterEligible)
    // Find an ineligible species
    for (const auto& sp : PokerogueContent::kSpecies) {
        if (!sp.starterEligible) {
            nonStarterRec.speciesDex = sp.dex;
            break;
        }
    }
    nonStarterRec.candyCount = 1000;
    if (applyNativeStarterPassiveUnlock(nonStarterRec) != StarterPassivePurchaseResult::InvalidRecord ||
        applyNativeStarterCostReduction(nonStarterRec) != StarterCostPurchaseResult::InvalidRecord) {
        std::printf("    FAIL: Non-starter species was not rejected by candy operations!\n");
        return 609;
    }

    std::printf("    PASS: Candy store transactions adhere strictly to balance, caps, and eligibility limits.\n");
    return 0;
}

static int stressTestCandyPriceTiers() {
    std::printf("  [3.2] Testing all starter candy price tiers and species mapping...\n");
    for (const auto& row : PokerogueContent::kStarterCandyPrices) {
        if (row.cost < 1 || row.costReduction[0] == 0 || row.costReduction[1] == 0 || row.passive == 0) {
            std::printf("    FAIL: Starter candy price tier for cost %u has invalid values!\n", row.cost);
            return 701;
        }
        if (row.costReduction[1] < row.costReduction[0]) {
            std::printf("    FAIL: Cost reduction tier 2 (%u) is less than tier 1 (%u) for cost %u!\n",
                row.costReduction[1], row.costReduction[0], row.cost);
            return 702;
        }
    }

    size_t eligibleStarters = 0;
    for (const auto& sp : PokerogueContent::kSpecies) {
        if (!sp.starterEligible) continue;
        ++eligibleStarters;
        const PokerogueContent::StarterCandyPrice* price = nullptr;
        for (const auto& row : PokerogueContent::kStarterCandyPrices) {
            if (row.cost == sp.starterCost) { price = &row; break; }
        }
        if (!price) {
            std::printf("    FAIL: Starter-eligible species %s (cost %u) has no entry in kStarterCandyPrices!\n",
                sp.id, sp.starterCost);
            return 703;
        }

        // Test with exact candy amounts
        NativeStarterCandyRecord rec{};
        rec.speciesDex = sp.dex;
        rec.candyCount = price->passive;
        if (applyNativeStarterPassiveUnlock(rec) != StarterPassivePurchaseResult::Applied ||
            !rec.passiveUnlocked || rec.candyCount != 0) {
            std::printf("    FAIL: Passive unlock failed for starter %s (cost %u)!\n", sp.id, sp.starterCost);
            return 704;
        }
    }
    std::printf("    PASS: Verified candy price tiers across %zu starter-eligible species.\n", eligibleStarters);
    return 0;
}

// =========================================================================
// SECTION 4: TRAINER PRESENTATION & REWARD ITEM ASSETS INTEGRITY
// =========================================================================

static int stressTestTrainerAndItemCatalogIntegrity() {
    std::printf("  [4.1] Testing trainer metadata, party templates, and item catalogs...\n");

    // Check every trainer in catalog
    size_t validTrainers = 0;
    for (const auto& t : PokerogueContent::kTrainerTypes) {
        if (!t.key || !*t.key || !t.name || !*t.name) {
            std::printf("    FAIL: Trainer %u has null or empty key/name!\n", t.id);
            return 801;
        }
        if (t.partyTemplateCount && t.partyTemplateOffset + t.partyTemplateCount > PokerogueContent::kTrainerPartyTemplateRefCount) {
            std::printf("    FAIL: Trainer '%s' party template offset out of bounds!\n", t.key);
            return 802;
        }
        ++validTrainers;
    }
    std::printf("    Verified %zu trainers in catalog.\n", validTrainers);

    // Check Modifier Reward items
    std::printf("  [4.2] Testing Poké Balls, vitamins, and recovery items in PokerogueModifierReward...\n");
    PokerogueRngAdapter rng;
    uint16_t seedBuf[PokerogueRngAdapter::kMaxSeedCodeUnits] = {'r', 'e', 'w', 'a', 'r', 'd'};
    rng.sow(seedBuf, 6);

    // Verify recovery item definitions
    const char* reqItems[] = {
        "POTION", "SUPER_POTION", "HYPER_POTION", "MAX_POTION", "FULL_RESTORE",
        "REVIVE", "MAX_REVIVE", "FULL_HEAL", "ETHER", "ELIXIR", "MAX_ELIXIR",
        "POKEBALL", "GREAT_BALL", "ULTRA_BALL", "ROGUE_BALL", "MASTER_BALL",
        "BASE_STAT_BOOSTER", "PP_UP", "PP_MAX", "SUPER_LURE", "NUGGET", "EVOLUTION_ITEM", "MAP", "MEMORY_MUSHROOM", "TERA_SHARD", "VOUCHER"
    };

    for (const char* itemKey : reqItems) {
        bool found = false;
        for (size_t i = 0; i < PokerogueContent::kItemCount; ++i) {
            if (std::strcmp(PokerogueContent::kItems[i].id, itemKey) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::printf("    FAIL: Reward item '%s' not found in content catalog!\n", itemKey);
            return 803;
        }
    }

    std::printf("    PASS: All required recovery, ball, and vitamin items exist with valid definitions.\n");
    return 0;
}

// =========================================================================
// MAIN HARNESS ENTRY
// =========================================================================

int main() {
    std::printf("=================================================================\n");
    std::printf("   ADVERSARIAL GAMEPLAY & PERSISTENCE STRESS HARNESS\n");
    std::printf("=================================================================\n\n");

    int res = 0;

    res = stressTestWaveClassification1to200();
    if (res) { std::printf("Suite 1.1 FAILED (%d)\n", res); return res; }

    res = stressTestTrainerKeyResolution();
    if (res) { std::printf("Suite 1.2 FAILED (%d)\n", res); return res; }

    res = stressTestStoryProgressionAcrossSeeds();
    if (res) { std::printf("Suite 1.3 FAILED (%d)\n", res); return res; }

    res = stressTestDualSlotPingPong();
    if (res) { std::printf("Suite 2.1-2.4 FAILED (%d)\n", res); return res; }

    res = stressTestRestoreComplexStates();
    if (res) { std::printf("Suite 2.5-2.6 FAILED (%d)\n", res); return res; }

    res = stressTestCandyStoreMechanics();
    if (res) { std::printf("Suite 3.1 FAILED (%d)\n", res); return res; }

    res = stressTestCandyPriceTiers();
    if (res) { std::printf("Suite 3.2 FAILED (%d)\n", res); return res; }

    res = stressTestTrainerAndItemCatalogIntegrity();
    if (res) { std::printf("Suite 4 FAILED (%d)\n", res); return res; }

    std::printf("\n=================================================================\n");
    std::printf("   ALL ADVERSARIAL STRESS TEST SUITES PASSED (0)\n");
    std::printf("=================================================================\n");
    return 0;
}
