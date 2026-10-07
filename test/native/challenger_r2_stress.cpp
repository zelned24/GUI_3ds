#include "game/FirstRunRuntime.hpp"
#include "storage/NativeRunSave.hpp"
#include "storage/NativeStarterCandyStore.hpp"
#include "storage/NativeStarterCandyProfile.hpp"
#include "runtime/FrontendMenuPresenter.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <algorithm>

namespace {
using namespace Pokerogue3DS;

// Dummy C2D symbol needed for FrontendMenuPresenter
void C2D_SpriteSheetFree(C2D_SpriteSheet) {}

// ============================================================================
// Section 1: Mathematical Empirical Simulation of Flee Formula
// ============================================================================
static unsigned calculateFleeChance(float playerSpeed, float enemySpeed, uint8_t attempts, bool isBoss) {
    const float pSpeed = std::max(1.0f, playerSpeed);
    const float eSpeed = std::max(1.0f, enemySpeed);
    const float speedRatio = pSpeed / eSpeed;
    const float speedCap = isBoss ? 6.0f : 4.0f;
    const float minChance = 5.0f;
    const float maxChance = isBoss ? 25.0f : 95.0f;
    const float escapeBonus = isBoss ? 2.0f : 10.0f;
    const float escapeSlope = (maxChance - minChance) / speedCap;
    float calcChance = escapeSlope * speedRatio + minChance + escapeBonus * float(attempts);
    if (calcChance < minChance) calcChance = minChance;
    if (calcChance > maxChance) calcChance = maxChance;
    return unsigned(std::round(calcChance));
}

static int testFleeMathematicalFormula() {
    std::printf("[TEST 1] Mathematical Flee Formula & Extreme Ratios\n");

    // 1.1 Non-boss extreme low speed (playerSpeed << enemySpeed)
    {
        unsigned chance = calculateFleeChance(1.0f, 500.0f, 0, false);
        if (chance != 5) {
            std::printf("FAIL 1.1: Extreme low speed ratio expected 5%%, got %u%%\n", chance);
            return 1101;
        }
    }

    // 1.2 Non-boss extreme high speed (playerSpeed >> enemySpeed)
    {
        unsigned chance = calculateFleeChance(500.0f, 1.0f, 0, false);
        if (chance != 95) {
            std::printf("FAIL 1.2: Extreme high speed ratio expected 95%%, got %u%%\n", chance);
            return 1102;
        }
    }

    // 1.3 Zero speed protection (0 speed input must clamp to 1 and not NaN/Inf)
    {
        unsigned chance0 = calculateFleeChance(0.0f, 0.0f, 0, false);
        // speedRatio = 1/1 = 1.0. Slope = 22.5. 22.5*1 + 5 = 27.5 -> 28%
        if (chance0 != 28) {
            std::printf("FAIL 1.3: Zero speed protection expected 28%%, got %u%%\n", chance0);
            return 1103;
        }
    }

    // 1.4 Attempt bonus scaling (non-boss at speedRatio = 1.0)
    // attempts: 0 -> 28%, 1 -> 38%, 2 -> 48%, 3 -> 58%, 4 -> 68%, 5 -> 78%, 6 -> 88%, 7 -> 95% (clamped)
    {
        const unsigned expectedScaling[] = { 28, 38, 48, 58, 68, 78, 88, 95, 95 };
        for (uint8_t att = 0; att < 9; ++att) {
            unsigned c = calculateFleeChance(50.0f, 50.0f, att, false);
            if (c != expectedScaling[att]) {
                std::printf("FAIL 1.4: Scaling attempt %u expected %u%%, got %u%%\n", att, expectedScaling[att], c);
                return 1104;
            }
        }
    }

    // 1.5 Boss escape chance bounds (min 5%, max 25%, speedCap 6.0)
    {
        unsigned bossMin = calculateFleeChance(1.0f, 500.0f, 0, true);
        if (bossMin != 5) {
            std::printf("FAIL 1.5a: Boss min chance expected 5%%, got %u%%\n", bossMin);
            return 1105;
        }
        unsigned bossMax = calculateFleeChance(500.0f, 1.0f, 0, true);
        if (bossMax != 25) {
            std::printf("FAIL 1.5b: Boss max chance expected 25%%, got %u%%\n", bossMax);
            return 1106;
        }
        // At speedRatio = 1.0: slope = 20/6 = 3.333333. Base = 3.333333 + 5 = 8.333333 -> 8%
        // attempt 1: 8.333333 + 2.0 = 10.333333 -> 10%
        // attempt 5: 8.333333 + 10.0 = 18.333333 -> 18%
        // attempt 10: 8.333333 + 20.0 = 28.333333 -> clamped to 25%
        if (calculateFleeChance(50.0f, 50.0f, 0, true) != 8) return 1107;
        if (calculateFleeChance(50.0f, 50.0f, 1, true) != 10) return 1108;
        if (calculateFleeChance(50.0f, 50.0f, 5, true) != 18) return 1109;
        if (calculateFleeChance(50.0f, 50.0f, 10, true) != 25) return 1110;
    }

    std::printf("PASS: Mathematical flee formula verified.\n");
    return 0;
}

// ============================================================================
// Section 2: FirstRunRuntime Live Combat Flee Empirical Testing
// ============================================================================
static int testFirstRunRuntimeFleeMechanics() {
    std::printf("[TEST 2] FirstRunRuntime Live Flee Mechanics\n");

    // 2.1 Flee before run starts must be rejected
    {
        FirstRunRuntime game(42);
        if (game.fleeBattle()) {
            std::printf("FAIL 2.1: fleeBattle succeeded before run started\n");
            return 2101;
        }
    }

    // 2.2 Setup run and test wild encounter flee
    {
        FirstRunRuntime game(100);
        if (!game.restoreSetup(100, 1)) return 2102;
        if (!game.startRun()) return 2103;
        if (!game.runStarted() || game.battleFinished()) return 2104;

        const uint16_t initialWave = game.run().wave;
        if (initialWave != 1) return 2105;

        // Verify flee against wild enemy
        const bool fleeResult = game.fleeBattle();
        if (!fleeResult) {
            std::printf("FAIL 2.2: fleeBattle returned false on valid wild encounter\n");
            return 2106;
        }

        if (game.battleFinished()) {
            // SUCCESS PATH
            if (game.playerWon()) {
                std::printf("FAIL 2.2a: Success path incorrectly marked playerWon = true\n");
                return 2107;
            }
            if (game.experienceGranted()) {
                std::printf("FAIL 2.2b: Success path incorrectly marked experienceGranted = true\n");
                return 2108;
            }
            if (game.run().wave != initialWave + 1) {
                std::printf("FAIL 2.2c: Success path wave did not advance (expected %u, got %u)\n",
                    initialWave + 1, game.run().wave);
                return 2109;
            }
            if (game.battleFeedback() != "¡Escapaste sin problemas!") {
                std::printf("FAIL 2.2d: Success path battleFeedback incorrect: '%s'\n",
                    game.battleFeedback().c_str());
                return 2110;
            }
        } else {
            // FAILURE PATH
            if (game.battleFeedback() != "¡No pudiste escapar!") {
                std::printf("FAIL 2.2e: Failure path battleFeedback incorrect: '%s'\n",
                    game.battleFeedback().c_str());
                return 2111;
            }
        }
    }

    // 2.3 Boss flee restrictions (wave % 10 == 0)
    // Create game at wave 10
    {
        NativeRunSave bossSave{};
        FirstRunRuntime game(200);
        if (!game.restoreSetup(200, 1)) return 2120;
        game.captureNativeRunSave(bossSave);
        bossSave.stage = NativeSaveStage::BattleActive;
        bossSave.wave = 10; // Major boss wave
        bossSave.encounterDex = 143; // Snorlax
        bossSave.playerHp = 20;
        bossSave.enemyHp = 100;
        bossSave.battleTurn = 1;
        bossSave.playerMoveCount = 1;
        bossSave.playerMoveIds[0] = 33;
        bossSave.playerPp[0] = 35;
        bossSave.enemyMoveCount = 1;
        bossSave.enemyMoveIds[0] = 33;
        bossSave.enemyPp[0] = 35;
        bossSave.enemyBoss.segmentCount = 2; // Boss segments

        if (game.restoreNativeRunSave(bossSave)) {
            // Check flee restriction
            if (game.fleeBattle()) {
                std::printf("FAIL 2.3: Flee succeeded on wave 10 boss!\n");
                return 2121;
            }
            if (game.battleFeedback() != "No puedes huir de un combate contra un jefe.") {
                std::printf("FAIL 2.3b: Boss flee feedback incorrect: '%s'\n", game.battleFeedback().c_str());
                return 2122;
            }
        }
    }

    std::printf("PASS: Live FirstRunRuntime flee mechanics verified.\n");
    return 0;
}

// ============================================================================
// Section 3: SD Save Deletion Lifecycle & Starter Isolation (R5.2)
// ============================================================================
class MockStorage final : public NativeSaveStorage {
public:
    char slots[2][kNativeSaveMaxBytes]{};
    size_t sizes[2]{};
    bool deleted[2]{false, false};

    NativeSaveResult readSlot(unsigned i, char* out, size_t cap, size_t& size) override {
        if (i > 1) return NativeSaveResult::InvalidRecord;
        if (deleted[i] || !sizes[i]) return NativeSaveResult::NotFound;
        if (sizes[i] > cap) return NativeSaveResult::TooLarge;
        std::memcpy(out, slots[i], sizes[i]);
        size = sizes[i];
        return NativeSaveResult::Ok;
    }

    NativeSaveResult writeSlot(unsigned i, const char* in, size_t size) override {
        if (i > 1) return NativeSaveResult::InvalidRecord;
        if (size > kNativeSaveMaxBytes) return NativeSaveResult::TooLarge;
        std::memcpy(slots[i], in, size);
        sizes[i] = size;
        deleted[i] = false;
        return NativeSaveResult::Ok;
    }

    NativeSaveResult deleteSlot(unsigned i) override {
        if (i > 1) return NativeSaveResult::InvalidRecord;
        sizes[i] = 0;
        deleted[i] = true;
        std::memset(slots[i], 0, kNativeSaveMaxBytes);
        return NativeSaveResult::Ok;
    }

    NativeSaveResult readExport(char*, size_t, size_t&) override { return NativeSaveResult::NotFound; }
    NativeSaveResult writeExport(const char*, size_t) override { return NativeSaveResult::Ok; }
};

static int testSdSaveDeletionLifecycle() {
    std::printf("[TEST 3] SD Save Deletion Lifecycle & Starter Profile Isolation\n");

    MockStorage disk;
    NativeRunSaveStore saveStore(disk);

    MockStorage starterDisk;
    char scratch[2 * kStarterCandyProfileMaxBytes]{};
    NativeStarterCandyStore candyStore(starterDisk, scratch, sizeof(scratch));

    // 3.1 Setup initial game save
    NativeRunSave runSave{};
    if (makeNativeRunSetupSave(12345, 1, runSave) != NativeSaveResult::Ok) return 3101;
    if (saveStore.save(runSave) != NativeSaveResult::Ok) return 3102;

    // Verify slots populated on disk
    if (disk.sizes[0] == 0) return 3103;

    // Verify load succeeds
    NativeRunSave loadedSave{};
    if (saveStore.load(PokerogueContent::kContentHash, loadedSave) != NativeSaveResult::Ok) return 3104;

    // 3.2 Setup initial starter candy profile
    NativeStarterCandyRecord record{1, 50, 5}; // Bulbasaur with 5 candies
    uint32_t generation = 0;
    if (candyStore.save(&record, 1, PokerogueContent::kContentHash,
            PokerogueContent::kSpeciesCount, generation) != NativeSaveResult::Ok) return 3105;

    // Verify starter profile exists on disk
    if (starterDisk.sizes[0] == 0 && starterDisk.sizes[1] == 0) return 3106;

    // 3.3 Execute deleteSave() on the run store
    const auto deleteResult = saveStore.deleteSave();
    if (deleteResult != NativeSaveResult::Ok) {
        std::printf("FAIL 3.3: deleteSave() returned non-Ok: %u\n", (unsigned)deleteResult);
        return 3107;
    }

    // 3.4 Verify both slot 0 and slot 1 removed from run disk
    if (!disk.deleted[0] || disk.sizes[0] != 0) {
        std::printf("FAIL 3.4a: Slot 0 was not deleted\n");
        return 3108;
    }
    if (!disk.deleted[1] || disk.sizes[1] != 0) {
        std::printf("FAIL 3.4b: Slot 1 was not deleted\n");
        return 3109;
    }

    // 3.5 Subsequent load must return NotFound
    NativeRunSave postDeleteSave{};
    const auto loadResult = saveStore.load(PokerogueContent::kContentHash, postDeleteSave);
    if (loadResult != NativeSaveResult::NotFound) {
        std::printf("FAIL 3.5: load() after delete did not return NotFound (returned %u)\n",
            (unsigned)loadResult);
        return 3110;
    }

    // 3.6 Calling deleteSave() again must be idempotent (return Ok even when slots already deleted)
    const auto reDeleteResult = saveStore.deleteSave();
    if (reDeleteResult != NativeSaveResult::Ok) {
        std::printf("FAIL 3.6: Second deleteSave() was not idempotent (returned %u)\n",
            (unsigned)reDeleteResult);
        return 3111;
    }

    // 3.7 Verify Starter Profile was PRESERVED on its storage and NOT deleted!
    if (starterDisk.sizes[0] == 0 && starterDisk.sizes[1] == 0) {
        std::printf("FAIL 3.7a: Starter profile was unexpectedly cleared!\n");
        return 3112;
    }
    NativeStarterCandyRecord loadedRecords[PokerogueContent::kSpeciesCount]{};
    size_t loadedCount = 0;
    uint32_t loadedGen = 0;
    if (candyStore.load(PokerogueContent::kContentHash, PokerogueContent::kSpeciesCount,
            loadedRecords, PokerogueContent::kSpeciesCount, loadedCount, loadedGen) != NativeSaveResult::Ok) {
        std::printf("FAIL 3.7b: Starter profile failed to load after save deletion!\n");
        return 3113;
    }
    if (loadedCount != 1 || loadedRecords[0].speciesDex != 1 || loadedRecords[0].candyCount != 5) {
        std::printf("FAIL 3.7c: Starter profile data corrupted! Count: %zu, candy: %u\n",
            loadedCount, loadedRecords[0].candyCount);
        return 3114;
    }

    // 3.8 Frontend Menu Delete Confirmation State Machine
    {
        FrontendMenuPresenter presenter(true); // Has save
        // Navigate to Load page
        presenter.input(KEY_TOUCH, 25, 48 + 2 * 29);
        presenter.input(KEY_A);
        if (presenter.page() != FrontendPage::Load) return 3120;
        if (presenter.isConfirmingDelete()) return 3121;

        // Press X -> enter confirmation mode
        auto cmd = presenter.input(KEY_X);
        if (cmd != FrontendCommand::None || !presenter.isConfirmingDelete()) {
            std::printf("FAIL 3.8a: KEY_X did not activate delete confirmation mode\n");
            return 3122;
        }

        // Press B -> cancel confirmation mode
        cmd = presenter.input(KEY_B);
        if (cmd != FrontendCommand::None || presenter.isConfirmingDelete()) {
            std::printf("FAIL 3.8b: KEY_B did not cancel delete confirmation mode\n");
            return 3123;
        }

        // Press X again, then confirm with KEY_A
        presenter.input(KEY_X);
        cmd = presenter.input(KEY_A);
        if (cmd != FrontendCommand::DeleteSave) {
            std::printf("FAIL 3.8c: Confirmed KEY_A did not dispatch DeleteSave command\n");
            return 3124;
        }
        if (presenter.isConfirmingDelete()) {
            std::printf("FAIL 3.8d: Confirmation state not cleared after DeleteSave\n");
            return 3125;
        }
    }

    std::printf("PASS: SD save deletion lifecycle and starter profile isolation verified.\n");
    return 0;
}

} // namespace

int main() {
    std::printf("=== Challenger 1 (Round 2) Stress Test Harness ===\n");
    int res = testFleeMathematicalFormula();
    if (res) return res;

    res = testFirstRunRuntimeFleeMechanics();
    if (res) return res;

    res = testSdSaveDeletionLifecycle();
    if (res) return res;

    std::printf("=== ALL CHALLENGER 1 STRESS TESTS PASSED (0) ===\n");
    return 0;
}
