#include "game/PokemonStarterMoveset.hpp"
#include "game/FirstRunRuntime.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "storage/IntegritySha256.hpp"
#include "storage/NativeStarterCandyProfile.hpp"
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

class ProgressMemoryStorage final : public Pokerogue3DS::NativeSaveStorage {
public:
    using Result = Pokerogue3DS::NativeSaveResult;
    char slots[2][Pokerogue3DS::kStarterCandyProfileMaxBytes]{};
    size_t sizes[2]{};
    char exported[Pokerogue3DS::kStarterCandyProfileMaxBytes]{};
    size_t exportSize = 0;
    bool interrupt = false;
    Result readSlot(unsigned i, char* out, size_t cap, size_t& read) override {
        read = sizes[i];
        if (!read) return Result::NotFound;
        if (read > cap) return Result::TooLarge;
        std::memcpy(out, slots[i], read); return Result::Ok;
    }
    Result writeSlot(unsigned i, const char* in, size_t size) override {
        if (size > sizeof(slots[i])) return Result::TooLarge;
        sizes[i] = interrupt ? size / 2 : size;
        std::memcpy(slots[i], in, sizes[i]);
        return interrupt ? Result::IoError : Result::Ok;
    }
    Result deleteSlot(unsigned i) override {
        if (i < 2) {
            sizes[i] = 0;
            std::memset(slots[i], 0, sizeof(slots[i]));
        }
        return Result::Ok;
    }
    Result readExport(char* out, size_t cap, size_t& read) override {
        read = exportSize;
        if (!read) return Result::NotFound;
        if (read > cap) return Result::TooLarge;
        std::memcpy(out, exported, read); return Result::Ok;
    }
    Result writeExport(const char* in, size_t size) override {
        if (size > sizeof(exported)) return Result::TooLarge;
        exportSize = size; std::memcpy(exported, in, size); return Result::Ok;
    }
};

// Turn a real setup into a complete, between-turn single-battle test checkpoint.
// This fills required fields from resolved actors, never fabricated catalog data.
static bool captureActiveTestCheckpoint(const Pokerogue3DS::FirstRunRuntime& game,
    Pokerogue3DS::NativeRunSave& saved) {
    using namespace Pokerogue3DS;
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

static bool sceneNodesOwnedBy(const Pokerogue3DS::FirstRunRuntime& game) {
    const auto& scene = game.scene();
    const uintptr_t owner = reinterpret_cast<uintptr_t>(&game);
    const uintptr_t nodes = reinterpret_cast<uintptr_t>(scene.nodes);
    const size_t bytes = scene.nodeCount * sizeof(*scene.nodes);
    return scene.nodes && scene.nodeCount && nodes >= owner &&
        nodes - owner <= sizeof(game) && bytes <= sizeof(game) - (nodes - owner);
}

static int checkInitialStarterTeamSetup() {
    using namespace Pokerogue3DS;
    uint16_t dexes[2]{};
    size_t count = 0;
    for (const auto& species : PokerogueContent::kSpecies)
        if (species.freshProfileStarter && species.starterEligible && species.starterCost <= 3 && count < 2)
            dexes[count++] = species.dex;
    if (count != 2) return 642;
    FirstRunRuntime first(1), repeated(2);
    for(const auto& species:PokerogueContent::kSpecies)
        if(first.starterUnlocked(species.dex)!=first.starterUnlocked(species)) return 642;
    if (!first.restoreStarterTeamSetup(1, dexes, 2) || !repeated.restoreStarterTeamSetup(1, dexes, 2) ||
        first.playerPartyCount() != 2 || repeated.playerPartyCount() != 2 || first.runStarted()) return 643;
    for (uint8_t member = 0; member < 2; ++member) {
        const auto* actor = first.playerPartyMember(member);
        const auto* repeat = repeated.playerPartyMember(member);
        if (!actor || !repeat || actor->dex != dexes[member] || !actor->actorIdentityResolved ||
            !actor->movesetResolved || actor->battleState.pokemonId != repeat->battleState.pokemonId ||
            actor->battleState.hp != repeat->battleState.hp || actor->moveCount != repeat->moveCount) return 644;
        for (uint8_t slot = 0; slot < actor->moveCount; ++slot)
            if (actor->moveIds[slot] != repeat->moveIds[slot] ||
                actor->battleState.moves[slot].pp != repeat->battleState.moves[slot].pp) return 645;
    }
    if (first.playerPartyMember(0)->battleState.pokemonId ==
        first.playerPartyMember(1)->battleState.pokemonId) return 646;
    if (!sceneNodesOwnedBy(first) || !sceneNodesOwnedBy(repeated)) return 660;
    const uint16_t duplicate[] = {dexes[0], dexes[0]};
    const uint32_t reservePid = first.playerPartyMember(1)->battleState.pokemonId;
    if (first.restoreStarterTeamSetup(1, duplicate, 2) ||
        first.restoreStarterTeamSetup(0, dexes, 2) || first.playerPartyCount() != 2 ||
        first.playerPartyMember(1)->battleState.pokemonId != reservePid) return 647;
    NativeRunSave unsupportedSetup{};
    if (first.captureNativeRunSave(unsupportedSetup) != NativeSaveResult::Ok ||
        unsupportedSetup.setupStarterCount != 2 || unsupportedSetup.setupStarterDexes[1] != dexes[1]) return 648;
    char encodedSetup[kNativeSaveMaxBytes]{};
    size_t encodedSetupBytes = 0;
    NativeRunSave decodedSetup{};
    if (encodeNativeRunSave(unsupportedSetup, encodedSetup, sizeof(encodedSetup), encodedSetupBytes) != NativeSaveResult::Ok ||
        decodeNativeRunSave(encodedSetup, encodedSetupBytes, PokerogueContent::kContentHash, decodedSetup) != NativeSaveResult::Ok ||
        decodedSetup.setupStarterCount != 2 || decodedSetup.setupStarterDexes[1] != dexes[1]) return 650;
    FirstRunRuntime reloaded(3);
    if (!reloaded.restoreNativeRunSave(decodedSetup) || reloaded.playerPartyCount() != 2 ||
        reloaded.playerPartyMember(1)->battleState.pokemonId != reservePid) return 651;
    decodedSetup.setupStarterDexes[1] = dexes[0];
    if (validateNativeRunSave(decodedSetup, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord ||
        reloaded.restoreNativeRunSave(decodedSetup) || reloaded.playerPartyCount() != 2) return 652;
    // v13 had no selection fields and restores its original single starter.
    char* selection = std::strstr(encodedSetup, "setupStarterCount=");
    char* saveVersion = std::strstr(encodedSetup, "saveVersion=");
    char* runtimeVersion = std::strstr(encodedSetup, "runtimeVersion=");
    if (!selection || !saveVersion || !runtimeVersion) return 653;
    std::memcpy(saveVersion + sizeof("saveVersion=") - 1, "000d", 4);
    std::memcpy(runtimeVersion + sizeof("runtimeVersion=") - 1, "000d", 4);
    const size_t legacyBody = static_cast<size_t>(selection - encodedSetup);
    char legacyHash[65]{};
    IntegritySha256::hashHex(encodedSetup, legacyBody, legacyHash);
    std::memcpy(selection, "sha256=", 7);
    std::memcpy(selection + 7, legacyHash, 64);
    selection[71] = '\n';
    if (decodeNativeRunSave(encodedSetup, legacyBody + 72, PokerogueContent::kContentHash, decodedSetup) !=
            NativeSaveResult::Ok || decodedSetup.setupStarterCount || decodedSetup.saveVersion != kNativeSaveVersion ||
        !reloaded.restoreNativeRunSave(decodedSetup) || reloaded.playerPartyCount() != 1) return 654;
    if (!first.restoreStarterTeamSetup(1, dexes, 1) || first.playerPartyCount() != 1 ||
        first.captureNativeRunSave(unsupportedSetup) != NativeSaveResult::Ok) return 649;
    for (size_t i = 0; i < PokerogueContent::kSpeciesCount && first.selectedSetupStarterDex() != dexes[1]; ++i)
        if (!first.browseSetupStarter(1)) return 655;
    if (first.selectedSetupStarterDex() != dexes[1] || first.playerPartyCount() != 1 ||
        first.run().starterDex != dexes[0] || !first.toggleSetupStarter() || first.playerPartyCount() != 2 ||
        first.playerPartyMember(1)->dex != dexes[1]) return 656;
    if (!first.toggleSetupStarter() || first.playerPartyCount() != 1 || first.run().starterDex != dexes[0]) return 657;
    for (size_t i = 0; i < PokerogueContent::kSpeciesCount && first.selectedSetupStarterDex() != dexes[0]; ++i)
        if (!first.browseSetupStarter(1)) return 658;
    if (first.toggleSetupStarter() || first.playerPartyCount() != 1 || first.run().starterDex != dexes[0]) return 659;
    if (!sceneNodesOwnedBy(first) || !first.restoreSetup(1, dexes[0]) || !sceneNodesOwnedBy(first)) return 661;
    NativeStarterCandyRecord improved{dexes[0], 0, 0, true};
    if (!seedNativeFreshStarterDexMetadata(improved)) return 676;
    improved.genderAttr = 8; // Female-only capture metadata; species default remains male.
    improved.natureAttr |= 1u << 1; // Hardy is the first enum nature unlocked.
    improved.dexIvs[0] = 31;
    improved.dexIvs[3] = 27;
    PokemonFriendshipPolicy profilePolicy{};
    profilePolicy.resolved = true;
    profilePolicy.candyMultiplier = PokerogueContent::kClassicCandyFriendshipMultiplier;
    if (!first.restoreStarterCandyProfile(&improved, 1, 0, profilePolicy) || !first.restoreSetup(1, dexes[0]) ||
        first.presentation().player.actor.ivs[0] != 31 || first.presentation().player.battleState.ivs[3] != 27 ||
        first.presentation().player.actor.ivs[1] != 15 ||
        first.presentation().player.actor.nature != PokemonNature::Hardy ||
        first.presentation().player.battleState.nature != PokemonNature::Hardy ||
        first.presentation().player.actor.gender != PokemonGender::Male ||
        first.presentation().player.battleState.gender != PokemonGender::Male) return 677;
    NativeRunSave improvedSetup{};
    if (first.captureNativeRunSave(improvedSetup) != NativeSaveResult::Ok ||
        !reloaded.restoreNativeRunSave(improvedSetup, &improved, 1, &profilePolicy) ||
        reloaded.presentation().player.actor.ivs[0] != 31 || reloaded.presentation().player.battleState.ivs[3] != 27 ||
        reloaded.presentation().player.battleState.stats[0] != first.presentation().player.battleState.stats[0]) return 678;

    return 0;
}

static int checkInitialTeamFirstTurnRoundtrip() {
    using namespace Pokerogue3DS;
    uint16_t starters[2]{};
    size_t count = 0;
    for (const auto& species : PokerogueContent::kSpecies)
        if (species.freshProfileStarter && species.starterEligible && species.starterCost <= 3 && count < 2)
            starters[count++] = species.dex;
    if (count != 2) return 663;
    // Search only for a declared supported encounter/move, never skip a failed turn.
    for (uint32_t seed = 1; seed <= 128; ++seed) {
        FirstRunRuntime game(seed);
        if (!game.restoreStarterTeamSetup(seed, starters, 2)) return 664;
        for (uint8_t slot = 0; slot < game.presentation().player.battleState.moveCount; ++slot) {
            if (game.selectedBattleMove() != slot && !game.selectBattleMove(1)) return 665;
            if (!game.battleInputSupported()) continue;
            const auto reserve = *game.playerPartyMember(1);
            const uint32_t activeId = game.presentation().player.battleState.pokemonId;
            if (!game.advanceBattleTurn() || !game.runStarted() || game.playerPartyCount() != 2) return 666;
            const auto* after = game.playerPartyMember(1);
            if (!after || after->battleState.pokemonId != reserve.battleState.pokemonId ||
                after->battleState.hp != reserve.battleState.hp || after->totalExperience != reserve.totalExperience) return 667;
            for (uint8_t move = 0; move < reserve.moveCount; ++move)
                if (after->battleState.moves[move].pp != reserve.battleState.moves[move].pp) return 668;
            NativeRunSave checkpoint{};
            if (game.captureNativeRunSave(checkpoint) != NativeSaveResult::Ok || checkpoint.playerPartyCount != 2 ||
                checkpoint.setupStarterCount || checkpoint.participantCount != 1 ||
                checkpoint.participantIds[0] != activeId || checkpoint.playerParty[1].pokemonId != reserve.battleState.pokemonId)
                return 669;
            char encoded[kNativeSaveMaxBytes]{};
            size_t written = 0;
            NativeRunSave decoded{};
            if (encodeNativeRunSave(checkpoint, encoded, sizeof(encoded), written) != NativeSaveResult::Ok ||
                decodeNativeRunSave(encoded, written, PokerogueContent::kContentHash, decoded) != NativeSaveResult::Ok)
                return 670;
            FirstRunRuntime restored(seed + 1);
            if (!restored.restoreNativeRunSave(decoded) || restored.playerPartyCount() != 2 ||
                restored.presentation().player.battleState.pokemonId != activeId ||
                restored.playerPartyMember(1)->battleState.pokemonId != reserve.battleState.pokemonId ||
                restored.playerPartyMember(1)->battleState.hp != reserve.battleState.hp || !sceneNodesOwnedBy(restored)) return 671;
            return 0;
        }
    }
    return 672; // Missing supported real encounter is a failure, never an implicit skip.
}

static int checkStarterCostPurchasePersistence() {
    using namespace Pokerogue3DS;
    FirstRunRuntime game(1);
    const uint16_t dex = game.run().starterDex;
    NativeStarterCandyRecord profile{dex, 999, 0, true};
    PokemonFriendshipPolicy policy{};
    policy.resolved = true;
    policy.candyMultiplier = PokerogueContent::kClassicCandyFriendshipMultiplier;
    if (!game.restoreStarterCandyProfile(&profile, 1, 0, policy)) return 635;
    static ProgressMemoryStorage runDisk, profileDisk;
    static char scratch[2 * kStarterCandyProfileMaxBytes]{};
    NativeRunSaveStore runs(runDisk);
    NativeStarterCandyStore profiles(profileDisk, scratch, sizeof(scratch));
    NativeProgressStore store(runs, profiles);
    StarterCostPurchaseResult purchase{};
    if (game.purchaseStarterCostReduction(dex, store, &purchase) != NativeSaveResult::Ok ||
        purchase != StarterCostPurchaseResult::Applied || game.starterCostReduction(dex) != 1) return 636;
    if (!sceneNodesOwnedBy(game)) return 662;
    const uint16_t afterFirst = game.starterProfileRecords()[0].candyCount;
    profileDisk.interrupt = true;
    if (game.purchaseStarterCostReduction(dex, store, &purchase) != NativeSaveResult::IoError ||
        game.starterCostReduction(dex) != 1 || game.starterProfileRecords()[0].candyCount != afterFirst) return 637;
    profileDisk.interrupt = false;
    runDisk.interrupt = true;
    if (game.purchaseStarterCostReduction(dex, store, &purchase) != NativeSaveResult::IoError ||
        game.starterCostReduction(dex) != 1 || game.starterProfileRecords()[0].candyCount != afterFirst) return 638;
    runDisk.interrupt = false;
    FirstRunRuntime restored(2);
    static NativeStarterCandyRecord staging[PokerogueContent::kSpeciesCount]{};
    NativeRunSave loaded{};
    if (restored.loadNativeProgress(runs, profiles, staging, PokerogueContent::kSpeciesCount, policy, &loaded) !=
            NativeSaveResult::Ok || loaded.starterProfileGeneration != 1 ||
        restored.starterCostReduction(dex) != 1 || restored.starterProfileRecords()[0].candyCount != afterFirst) return 639;
    if (restored.purchaseStarterCostReduction(dex, store, &purchase) != NativeSaveResult::Ok ||
        restored.starterCostReduction(dex) != 2) return 640;
    const uint16_t afterSecond = restored.starterProfileRecords()[0].candyCount;
    if (restored.purchaseStarterCostReduction(dex, store, &purchase) != NativeSaveResult::InvalidRecord ||
        purchase != StarterCostPurchaseResult::MaximumReduction || restored.starterCostReduction(dex) != 2 ||
        restored.starterProfileRecords()[0].candyCount != afterSecond) return 641;
    return 0;
}

static int checkStarterFormPreferencePersistence() {
    using namespace Pokerogue3DS;
    const PokerogueContent::Species* chosen = nullptr;
    const PokerogueContent::Form* alternate = nullptr;
    for (const auto& species : PokerogueContent::kSpecies) {
        if (!species.starterEligible || species.starterCost > 10) continue;
        for (const auto& form : PokerogueContent::kForms) {
            if (!form.upstreamFormIndex || form.upstreamFormIndex > 56 ||
                std::strcmp(form.speciesId, species.id) || !std::strcmp(form.formKey, "FEMALE")) continue;
            const uint64_t unlocks = uint64_t(128) | (uint64_t(128) << form.upstreamFormIndex);
            if (pokemonValidateStarterForm(species.dex, 0, unlocks) != PokemonStarterFormResult::Ok ||
                pokemonValidateStarterForm(species.dex, form.upstreamFormIndex, unlocks) != PokemonStarterFormResult::Ok)
                continue;
            chosen = &species;
            alternate = &form;
            break;
        }
        if (chosen) break;
    }
    if (!chosen || !alternate) return 683;
    NativeStarterCandyRecord profile{chosen->dex, 0, 0, true};
    profile.natureAttr = 2;
    profile.abilityAttr = 1;
    profile.genderAttr = 12;
    for (uint8_t& iv : profile.dexIvs) iv = 15;
    profile.unlockedFormAttr = uint64_t(128) | (uint64_t(128) << alternate->upstreamFormIndex);
    PokemonFriendshipPolicy policy{};
    policy.resolved = true;
    policy.candyMultiplier = PokerogueContent::kClassicCandyFriendshipMultiplier;
    FirstRunRuntime game(1);
    if (!game.restoreStarterCandyProfile(&profile, 1, 0, policy) || !game.restoreSetup(1, chosen->dex)) return 684;
    static ProgressMemoryStorage runDisk, profileDisk;
    static char scratch[2 * kStarterCandyProfileMaxBytes]{};
    NativeRunSaveStore runs(runDisk);
    NativeStarterCandyStore profiles(profileDisk, scratch, sizeof(scratch));
    NativeProgressStore store(runs, profiles);
    if (game.selectSetupStarterForm(chosen->dex, alternate->upstreamFormIndex, store) != NativeSaveResult::Ok ||
        game.setupStarterFormIndex(chosen->dex) != alternate->upstreamFormIndex ||
        !game.presentation().player.formId || std::strcmp(game.presentation().player.formId, alternate->id) ||
        !sceneNodesOwnedBy(game)) return 685;
    uint16_t expectedMoves[4]{};
    uint8_t expectedCount = 0;
    if (selectPokemonStarterMoveset(chosen->dex, alternate->id, 0, nullptr, 0, expectedMoves, expectedCount) !=
            PokemonStarterMovesetResult::Ok || expectedCount != game.presentation().player.moveCount) return 686;
    for (uint8_t i = 0; i < expectedCount; ++i)
        if (game.presentation().player.moveIds[i] != expectedMoves[i]) return 687;
    const uint32_t pid = game.presentation().player.actor.pokemonId;
    const uint16_t hp = game.presentation().player.battleState.hp;
    profileDisk.interrupt = true;
    if (game.selectSetupStarterForm(chosen->dex, 0, store) != NativeSaveResult::IoError ||
        game.setupStarterFormIndex(chosen->dex) != alternate->upstreamFormIndex ||
        game.presentation().player.actor.pokemonId != pid || game.presentation().player.battleState.hp != hp)
        return 688;
    profileDisk.interrupt = false;
    runDisk.interrupt = true;
    if (game.selectSetupStarterForm(chosen->dex, 0, store) != NativeSaveResult::IoError ||
        game.setupStarterFormIndex(chosen->dex) != alternate->upstreamFormIndex ||
        !sceneNodesOwnedBy(game)) return 689;
    runDisk.interrupt = false;
    FirstRunRuntime restored(2);
    static NativeStarterCandyRecord staging[PokerogueContent::kSpeciesCount]{};
    NativeRunSave loaded{};
    if (restored.loadNativeProgress(runs, profiles, staging, PokerogueContent::kSpeciesCount, policy, &loaded) !=
            NativeSaveResult::Ok || loaded.starterProfileGeneration != 1 ||
        restored.setupStarterFormIndex(chosen->dex) != alternate->upstreamFormIndex ||
        !restored.presentation().player.formId ||
        std::strcmp(restored.presentation().player.formId, alternate->id) ||
        restored.presentation().player.actor.pokemonId != pid || restored.presentation().player.battleState.hp != hp)
        return 690;
    if (restored.selectSetupStarterForm(chosen->dex, 65534, store) != NativeSaveResult::InvalidRecord ||
        restored.setupStarterFormIndex(chosen->dex) != alternate->upstreamFormIndex) return 691;
    if (restored.selectSetupStarterForm(chosen->dex, 65535, store) != NativeSaveResult::Ok ||
        restored.setupStarterFormIndex(chosen->dex) ||
        restored.starterProfileRecords()[0].preferredFormIndex != 65535 || !sceneNodesOwnedBy(restored)) return 692;
    return 0;
}

// Synthetic KO checkpoints still represent a real active battle participant.
static void setSingleParticipantFixture(Pokerogue3DS::NativeRunSave& save,
        const Pokerogue3DS::FirstRunRuntime& game) {
    save.participantHistoryResolved = true;
    save.participantCount = 1;
    for (auto& id : save.participantIds) id = 0;
    save.participantIds[0] = game.presentation().player.battleState.pokemonId;
}

// Standalone host regression for atomic checkpoint application. This requires
// the standard C++ library; it is not the freestanding WASM parity harness.
static int checkStatusActionAdmission() {
    using namespace Pokerogue3DS;
    bool checkedRejection = false, checkedExecution = false;
    for (uint32_t seed = 1; seed <= 256; ++seed) {
        FirstRunRuntime game(seed);
        const auto& context = game.presentation();
        if (!context.enemy.actorIdentityResolved || context.secondEnemy.dex || context.trainerPartyCount) continue;
        bool enemyStatusCapability = false;
        for (const auto& profile : PokerogueContent::kStatusActionAbilityProfiles)
            if (profile.abilityId == context.enemy.battleState.abilityId) enemyStatusCapability = profile.resolved;
        NativeRunSave checkpoint{};
        if (game.captureNativeRunSave(checkpoint) != NativeSaveResult::Ok) return 9380;
        checkpoint.stage = NativeSaveStage::BattleActive;
        checkpoint.battleTurn = 1;
        checkpoint.encounterDex = context.enemy.dex;
        checkpoint.playerHp = context.player.battleState.hp;
        checkpoint.enemyHp = context.enemy.battleState.hp;
        checkpoint.playerMoveCount = 1;
        checkpoint.playerMoveIds[0] = 95; // Explicit test-only Hypnosis injection.
        checkpoint.playerPp[0] = 20;
        checkpoint.enemyMoveCount = context.enemy.battleState.moveCount;
        for (uint8_t slot = 0; slot < checkpoint.enemyMoveCount; ++slot) {
            checkpoint.enemyMoveIds[slot] = context.enemy.battleState.moves[slot].moveId;
            checkpoint.enemyPp[slot] = context.enemy.battleState.moves[slot].pp;
        }
        for (uint8_t slot = 1; slot < 4; ++slot) {
            checkpoint.playerMoveIds[slot] = 0;
            checkpoint.playerPp[slot] = 0;
        }
        auto injectedActor = context.player.battleState;
        injectedActor.moveCount = 1;
        injectedActor.moves[0] = {95, 20, 20};
        for (uint8_t slot = 1; slot < 4; ++slot) injectedActor.moves[slot] = {};
        checkpoint.playerPartyCount = 1;
        checkpoint.activePlayerMember = 0;
        if (!captureNativePokemonActorSave(injectedActor, context.player.actor,
                context.player.totalExperience, checkpoint.playerParty[0])) return 9385;
        if (!game.restoreNativeRunSave(checkpoint)) return 9381;
        NativeRunSave beforeAction{}, afterAction{};
        if (game.captureNativeRunSave(beforeAction) != NativeSaveResult::Ok) return 9384;
        if (!enemyStatusCapability) {
            if (game.battleInputSupported() || game.advanceBattleTurn() ||
                game.presentation().player.battleState.moves[0].pp != 20 ||
                game.presentation().enemy.battleState.status.present ||
                game.captureNativeRunSave(afterAction) != NativeSaveResult::Ok ||
                afterAction.battleTurn != beforeAction.battleTurn) return 9382;
            auto blockedGrowl = checkpoint;
            blockedGrowl.playerMoveIds[0] = blockedGrowl.playerParty[0].moveIds[0] = 45;
            blockedGrowl.playerPp[0] = blockedGrowl.playerParty[0].pp[0] =
                blockedGrowl.playerParty[0].maxPp[0] = 40;
            FirstRunRuntime rejectedGrowl(seed);
            NativeRunSave rejectedBefore{}, rejectedAfter{};
            if (!rejectedGrowl.restoreNativeRunSave(blockedGrowl) ||
                rejectedGrowl.captureNativeRunSave(rejectedBefore) != NativeSaveResult::Ok ||
                rejectedGrowl.battleInputSupported() || rejectedGrowl.advanceBattleTurn() ||
                rejectedGrowl.captureNativeRunSave(rejectedAfter) != NativeSaveResult::Ok ||
                rejectedAfter.playerPp[0] != 40 || rejectedAfter.battleTurn != rejectedBefore.battleTurn ||
                rejectedAfter.playerHp != rejectedBefore.playerHp || rejectedAfter.enemyHp != rejectedBefore.enemyHp ||
                rejectedAfter.enemyStatStages[0] != rejectedBefore.enemyStatStages[0]) return 9541;
            checkedRejection = true;
        } else {
            if (!game.battleInputSupported()) continue;
            FirstRunRuntime repeated(seed);
            if (!repeated.restoreNativeRunSave(checkpoint) || !game.advanceBattleTurn() ||
                !repeated.advanceBattleTurn()) return 9386;
            NativeRunSave repeatedAction{};
            if (game.captureNativeRunSave(afterAction) != NativeSaveResult::Ok ||
                repeated.captureNativeRunSave(repeatedAction) != NativeSaveResult::Ok ||
                afterAction.playerPp[0] != 19 || afterAction.battleTurn != beforeAction.battleTurn + 1 ||
                afterAction.playerHp != repeatedAction.playerHp || afterAction.enemyHp != repeatedAction.enemyHp ||
                afterAction.enemyStatus.present != repeatedAction.enemyStatus.present ||
                afterAction.enemyStatus.effect != repeatedAction.enemyStatus.effect ||
                afterAction.enemyStatus.sleepTurnsRemaining != repeatedAction.enemyStatus.sleepTurnsRemaining)
                return 9387;
            // Real Geodude/Sturdy actor, deterministic Dragon Rage opponent.
            // Moves are injected only into this test snapshot, never production learnsets.
            {
                const auto* species = PokerogueContent::findSpeciesByDex(74);
                if (!species || (species->ability1 != 5 && species->ability2 != 5)) return 9970;
                PokemonBattleInit input{};
                input.speciesDex = species->dex;
                input.formId = species->firstFormId[0] ? species->firstFormId : nullptr;
                input.level = 5;
                input.pokemonId = context.player.actor.pokemonId;
                input.abilityId = 5;
                input.gender = context.player.actor.gender;
                input.nature = context.player.actor.nature;
                for (uint8_t i = 0; i < 6; ++i) input.ivs[i] = context.player.actor.ivs[i];
                input.moveCount = 1;
                input.moveIds[0] = 45;
                PokemonBattleState actor{};
                if (initializePokemonBattleState(input, actor) != PokemonBattleInitResult::Ok ||
                    actor.maxHp >= 40) return 9971;
                auto identity = context.player.actor;
                identity.formId = actor.formId;
                identity.abilityIndex = species->ability1 == 5 ? 0 : 1;
                identity.initialTeraType = resolvePokemonTypeSymbol(species->type1);
                identity.initialTeraTypeIndex = 0;
                identity.initialTeraTypeResolved = true;
                uint32_t experience = 0;
                if (pokemonTotalExperienceForLevel(species->growthRate, 5, experience) !=
                        PokemonExperienceResult::Ok) return 9972;
                auto sturdyCheckpoint = checkpoint;
                if (!captureNativePokemonActorSave(actor, identity, experience,
                        sturdyCheckpoint.playerParty[0])) return 9973;
                sturdyCheckpoint.playerLevel = 5;
                sturdyCheckpoint.playerExperience = experience;
                sturdyCheckpoint.playerHp = actor.hp;
                sturdyCheckpoint.playerMoveIds[0] = 45;
                sturdyCheckpoint.playerPp[0] = 40;
                // Keep the pinned enemy moveset in the checkpoint. Controlled
                // command inputs are injected into the live test field after restore;
                // production restore must still reject a forged encounter moveset.
                const auto injectDragonRage = [](FirstRunRuntime& run) {
                    auto& enemy = const_cast<PresentationContext&>(run.presentation()).enemy.battleState;
                    enemy.moveCount = 1;
                    for (auto& move : enemy.moves) move = {};
                    enemy.moves[0] = {82, 10, 10};
                };
                FirstRunRuntime sturdyRun(seed), replay(seed);
                NativeRunSave after{}, repeatedAfter{};
                const bool restoreA = sturdyRun.restoreNativeRunSave(sturdyCheckpoint);
                const bool restoreB = replay.restoreNativeRunSave(sturdyCheckpoint);
                if (restoreA) injectDragonRage(sturdyRun);
                if (restoreB) injectDragonRage(replay);
                const bool supportsA = restoreA && sturdyRun.battleInputSupported();
                const bool supportsB = restoreB && replay.battleInputSupported();
                const bool turnA = supportsA && sturdyRun.advanceBattleTurn();
                const bool turnB = supportsB && replay.advanceBattleTurn();
                const auto savedA = sturdyRun.captureNativeRunSave(after);
                const auto savedB = replay.captureNativeRunSave(repeatedAfter);
                if (!restoreA || !restoreB || !supportsA || !supportsB || !turnA || !turnB ||
                    savedA != NativeSaveResult::Ok || savedB != NativeSaveResult::Ok ||
                    after.playerHp != 1 || after.playerPp[0] != 39 || after.enemyPp[0] != 9)
                    std::printf("Sturdy integration seed=%u restore=%u/%u support=%u/%u turn=%u/%u save=%u/%u hp=%u pp=%u/%u feedback=%s\n",
                        static_cast<unsigned>(seed), restoreA, restoreB, supportsA, supportsB, turnA, turnB,
                        static_cast<unsigned>(savedA), static_cast<unsigned>(savedB), after.playerHp,
                        after.playerPp[0], after.enemyPp[0], sturdyRun.battleFeedback().c_str());
                if (!restoreA || !restoreB || !supportsA || !supportsB || !turnA || !turnB ||
                    savedA != NativeSaveResult::Ok || savedB != NativeSaveResult::Ok ||
                    after.playerHp != 1 || after.playerPp[0] != 39 || after.enemyPp[0] != 9 ||
                    after.playerHp != repeatedAfter.playerHp || after.enemyHp != repeatedAfter.enemyHp ||
                    sturdyRun.presentation().player.battleState.sturdy.present) return 9974;
                const auto a = sturdyRun.battleRng().state(), b = replay.battleRng().state();
                if (a.carry != b.carry || a.s0 != b.s0 || a.s1 != b.s1 || a.s2 != b.s2) return 9975;
                // Existing player survival tag must persist through a run checkpoint.
                auto taggedCheckpoint = sturdyCheckpoint;
                taggedCheckpoint.playerParty[0].sturdyTag = true;
                FirstRunRuntime tagged(seed), taggedReplay(seed);
                NativeRunSave recaptured{};
                char payload[kNativeSaveMaxBytes]{};
                size_t payloadSize = 0;
                NativeRunSave decoded{};
                if (!tagged.restoreNativeRunSave(taggedCheckpoint) ||
                    !tagged.presentation().player.battleState.sturdy.present ||
                    tagged.captureNativeRunSave(recaptured) != NativeSaveResult::Ok ||
                    !recaptured.playerPartyCount || !recaptured.playerParty[0].sturdyTag ||
                    encodeNativeRunSave(recaptured, payload, sizeof(payload), payloadSize) != NativeSaveResult::Ok ||
                    decodeNativeRunSave(payload, payloadSize, PokerogueContent::kContentHash, decoded) != NativeSaveResult::Ok ||
                    !taggedReplay.restoreNativeRunSave(decoded) ||
                    !taggedReplay.presentation().player.battleState.sturdy.present) return 10080;
                injectDragonRage(tagged);
                injectDragonRage(taggedReplay);
                if (!tagged.advanceBattleTurn() || !taggedReplay.advanceBattleTurn() ||
                    tagged.presentation().player.battleState.hp != 1 ||
                    taggedReplay.presentation().player.battleState.hp != 1 ||
                    tagged.presentation().player.battleState.sturdy.present ||
                    taggedReplay.presentation().player.battleState.sturdy.present) return 10081;
                const auto tagRng = tagged.battleRng().state(), replayTagRng = taggedReplay.battleRng().state();
                if (tagRng.carry != replayTagRng.carry || tagRng.s0 != replayTagRng.s0 ||
                    tagRng.s1 != replayTagRng.s1 || tagRng.s2 != replayTagRng.s2) return 10082;
                auto invalidUnusedTag = recaptured;
                invalidUnusedTag.playerParty[5].sturdyTag = true;
                if (validateNativeRunSave(invalidUnusedTag, PokerogueContent::kContentHash) !=
                    NativeSaveResult::InvalidRecord) return 10083;
            }
            // Real Gallade/Sharpness actor, Sacred Sword command after restore.
            // Moves are injected only into this test snapshot, never production learnsets.
            {
                const auto* species = PokerogueContent::findSpeciesByDex(475);
                if (!species || (species->ability1 != 292 && species->ability2 != 292)) return 10070;
                PokemonBattleInit input{};
                input.speciesDex = species->dex;
                input.formId = species->firstFormId[0] ? species->firstFormId : nullptr;
                input.level = 5;
                input.pokemonId = context.player.actor.pokemonId;
                input.abilityId = 292;
                input.gender = PokemonGender::Male;
                input.nature = context.player.actor.nature;
                for (uint8_t i = 0; i < 6; ++i) input.ivs[i] = context.player.actor.ivs[i];
                input.moveCount = 1;
                input.moveIds[0] = 533;
                PokemonBattleState actor{};
                if (initializePokemonBattleState(input, actor) != PokemonBattleInitResult::Ok) return 10071;
                auto identity = context.player.actor;
                identity.formId = actor.formId;
                identity.gender = PokemonGender::Male;
                identity.abilityIndex = species->ability1 == 292 ? 0 : 1;
                identity.initialTeraType = resolvePokemonTypeSymbol(species->type1);
                identity.initialTeraTypeIndex = 0;
                identity.initialTeraTypeResolved = true;
                uint32_t experience = 0;
                if (pokemonTotalExperienceForLevel(species->growthRate, 5, experience) !=
                        PokemonExperienceResult::Ok) return 10072;
                auto sharpnessCheckpoint = checkpoint;
                if (!captureNativePokemonActorSave(actor, identity, experience,
                        sharpnessCheckpoint.playerParty[0])) return 10073;
                sharpnessCheckpoint.playerLevel = 5;
                sharpnessCheckpoint.playerExperience = experience;
                sharpnessCheckpoint.playerHp = actor.hp;
                sharpnessCheckpoint.playerMoveIds[0] = 533;
                sharpnessCheckpoint.playerPp[0] = 15;
                const auto restoreSharpnessField = [&](FirstRunRuntime& run) {
                    if (!run.restoreNativeRunSave(sharpnessCheckpoint)) return false;
                    auto& enemy = const_cast<PresentationContext&>(run.presentation()).enemy.battleState;
                    enemy.moveCount = 1;
                    for (auto& move : enemy.moves) move = {};
                    enemy.moves[0] = {45, 40, 40}; // Controlled test-only Growl after canonical restore.
                    return true;
                };
                FirstRunRuntime sharpnessRun(seed), replay(seed);
                NativeRunSave after{}, repeatedAfter{};
                const bool restoreA = restoreSharpnessField(sharpnessRun), restoreB = restoreSharpnessField(replay);
                const bool supportA = restoreA && sharpnessRun.battleInputSupported();
                const bool supportB = restoreB && replay.battleInputSupported();
                const bool turnA = supportA && sharpnessRun.advanceBattleTurn();
                const bool turnB = supportB && replay.advanceBattleTurn();
                const auto saveA = sharpnessRun.captureNativeRunSave(after), saveB = replay.captureNativeRunSave(repeatedAfter);
                if (!restoreA || !restoreB || !supportA || !supportB || !turnA || !turnB ||
                    saveA != NativeSaveResult::Ok || saveB != NativeSaveResult::Ok)
                    std::printf("Sharpness seed=%u restore=%u/%u support=%u/%u turn=%u/%u save=%u/%u feedback=%s\n",
                        seed, restoreA, restoreB, supportA, supportB, turnA, turnB,
                        static_cast<unsigned>(saveA), static_cast<unsigned>(saveB), sharpnessRun.battleFeedback().c_str());
                if (!restoreA || !restoreB || !supportA || !supportB || !turnA || !turnB ||
                    saveA != NativeSaveResult::Ok || saveB != NativeSaveResult::Ok ||
                    after.playerPp[0] != 14 || after.battleTurn != sharpnessCheckpoint.battleTurn + (sharpnessRun.battleFinished() ? 0 : 1) ||
                    after.playerHp != repeatedAfter.playerHp || after.enemyHp != repeatedAfter.enemyHp ||
                    after.playerParty[0].abilityId != 292 || repeatedAfter.playerParty[0].abilityId != 292 ||
                    after.enemyPp[0] != repeatedAfter.enemyPp[0] ||
                    after.playerPp[0] != repeatedAfter.playerPp[0]) return 10074;
                const auto a = sharpnessRun.battleRng().state(), b = replay.battleRng().state();
                if (a.carry != b.carry || a.s0 != b.s0 || a.s1 != b.s1 || a.s2 != b.s2) return 10075;
                auto enemyTaggedCheckpoint = sharpnessCheckpoint;
                enemyTaggedCheckpoint.enemySturdyTag = true;
                FirstRunRuntime enemyTagged(seed), enemyTaggedReplay(seed);
                NativeRunSave enemyTagSave{};
                if (!enemyTagged.restoreNativeRunSave(enemyTaggedCheckpoint) ||
                    !enemyTagged.presentation().enemy.battleState.sturdy.present ||
                    enemyTagged.captureNativeRunSave(enemyTagSave) != NativeSaveResult::Ok ||
                    !enemyTagSave.enemySturdyTag || !enemyTaggedReplay.restoreNativeRunSave(enemyTagSave) ||
                    !enemyTaggedReplay.presentation().enemy.battleState.sturdy.present) return 10100;

            }
            NativeRunSave emberCheckpoint = checkpoint;
            emberCheckpoint.playerMoveIds[0] = 52;
            emberCheckpoint.playerPp[0] = 25;
            emberCheckpoint.playerParty[0].moveIds[0] = 52;
            emberCheckpoint.playerParty[0].pp[0] = emberCheckpoint.playerParty[0].maxPp[0] = 25;
            FirstRunRuntime ember(seed), repeatedEmber(seed);
            NativeRunSave emberAfter{}, repeatedEmberAfter{};
            if (!ember.restoreNativeRunSave(emberCheckpoint) ||
                !repeatedEmber.restoreNativeRunSave(emberCheckpoint) || !ember.battleInputSupported() ||
                !ember.advanceBattleTurn() || !repeatedEmber.advanceBattleTurn() ||
                ember.captureNativeRunSave(emberAfter) != NativeSaveResult::Ok ||
                repeatedEmber.captureNativeRunSave(repeatedEmberAfter) != NativeSaveResult::Ok ||
                emberAfter.playerPp[0] != 24 || emberAfter.enemyHp >= checkpoint.enemyHp ||
                emberAfter.enemyHp != repeatedEmberAfter.enemyHp ||
                emberAfter.enemyStatus.effect != repeatedEmberAfter.enemyStatus.effect ||
                emberAfter.enemyStatus.present != repeatedEmberAfter.enemyStatus.present) return 9430;
            // Test-only save injection of real canonical special-damage moves.
            // Both restores execute the production turn command and shared RNG.
            for (const uint16_t id : {uint16_t(49), uint16_t(82), uint16_t(69), uint16_t(101), uint16_t(162), uint16_t(717), uint16_t(877), uint16_t(149), uint16_t(206), uint16_t(610)}) {
                const auto* fixedMove = PokerogueContent::findMoveById(id);
                if (!fixedMove || (!pokemonFixedDamageMoveProfile(id) && !pokemonSurviveDamageMoveResolved(id))) return 9870;
                auto fixedCheckpoint = checkpoint;
                fixedCheckpoint.playerMoveIds[0] = fixedCheckpoint.playerParty[0].moveIds[0] = id;
                fixedCheckpoint.playerPp[0] = fixedCheckpoint.playerParty[0].pp[0] =
                    fixedCheckpoint.playerParty[0].maxPp[0] = static_cast<uint8_t>(fixedMove->pp);
                FirstRunRuntime fixedAttack(seed), repeatedFixedAttack(seed);
                NativeRunSave fixedAfter{}, repeatedFixedAfter{};
                if (!fixedAttack.restoreNativeRunSave(fixedCheckpoint) ||
                    !repeatedFixedAttack.restoreNativeRunSave(fixedCheckpoint) ||
                    !fixedAttack.battleInputSupported() || !repeatedFixedAttack.battleInputSupported() ||
                    !fixedAttack.advanceBattleTurn() || !repeatedFixedAttack.advanceBattleTurn() ||
                    fixedAttack.captureNativeRunSave(fixedAfter) != NativeSaveResult::Ok ||
                    repeatedFixedAttack.captureNativeRunSave(repeatedFixedAfter) != NativeSaveResult::Ok ||
                    fixedAfter.playerPp[0] != fixedMove->pp - 1 ||
                    fixedAfter.playerHp != repeatedFixedAfter.playerHp ||
                    fixedAfter.enemyHp != repeatedFixedAfter.enemyHp ||
                    fixedAfter.stage != repeatedFixedAfter.stage ||
                    fixedAfter.battleTurn != repeatedFixedAfter.battleTurn) return 9871;
                const auto fixedRng = fixedAttack.battleRng().state();
                const auto replayRng = repeatedFixedAttack.battleRng().state();
                if (fixedRng.carry != replayRng.carry || fixedRng.s0 != replayRng.s0 ||
                    fixedRng.s1 != replayRng.s1 || fixedRng.s2 != replayRng.s2) return 9872;
            }
            // Actual thaw+burn compositions, restored with both actors frozen.
            for (const uint16_t id : {uint16_t(172), uint16_t(394), uint16_t(503), uint16_t(815)}) {
                const auto* thawMove = PokerogueContent::findMoveById(id);
                if (!thawMove || !pokemonDamageSecondaryAttributesResolved(*thawMove, "StatusEffectAttr") ||
                    !pokemonMoveSelfThawResolved(id)) return 9770;
                auto thawCheckpoint = checkpoint;
                thawCheckpoint.playerMoveIds[0] = thawCheckpoint.playerParty[0].moveIds[0] = id;
                thawCheckpoint.playerPp[0] = thawCheckpoint.playerParty[0].pp[0] =
                    thawCheckpoint.playerParty[0].maxPp[0] = static_cast<uint8_t>(thawMove->pp);
                PokemonStatusState frozen{};
                frozen.present = true;
                frozen.effect = PokemonStatusEffect::Freeze;
                frozen.hasFreezeTurnsRemaining = true;
                frozen.freezeTurnsRemaining = 3;
                thawCheckpoint.playerStatus = thawCheckpoint.playerParty[0].status = frozen;
                thawCheckpoint.enemyStatus = frozen;
                FirstRunRuntime thawAttack(seed), replayThaw(seed);
                NativeRunSave thawAfter{}, replayAfter{};
                if (!thawAttack.restoreNativeRunSave(thawCheckpoint) ||
                    !replayThaw.restoreNativeRunSave(thawCheckpoint) || !thawAttack.battleInputSupported() ||
                    !thawAttack.advanceBattleTurn() || !replayThaw.advanceBattleTurn() ||
                    thawAttack.captureNativeRunSave(thawAfter) != NativeSaveResult::Ok ||
                    replayThaw.captureNativeRunSave(replayAfter) != NativeSaveResult::Ok ||
                    thawAfter.playerPp[0] != thawMove->pp - 1 || thawAfter.playerStatus.present ||
                    thawAfter.playerParty[0].status.present || replayAfter.playerStatus.present ||
                    thawAfter.playerHp != replayAfter.playerHp || thawAfter.enemyHp != replayAfter.enemyHp ||
                    thawAfter.enemyStatus.present != replayAfter.enemyStatus.present ||
                    thawAfter.enemyStatus.effect != replayAfter.enemyStatus.effect ||
                    thawAfter.enemyStatus.freezeTurnsRemaining != replayAfter.enemyStatus.freezeTurnsRemaining ||
                    thawAfter.enemyStatus.hasFreezeTurnsRemaining != replayAfter.enemyStatus.hasFreezeTurnsRemaining)
                    return 9771;
            }
            // Real multi-attribute attacks use the same critical/damage/status
            // path. Moves are injected only in this native regression snapshot.
            for (const uint16_t id : {uint16_t(299), uint16_t(342)}) {
                const auto* combinedMove = PokerogueContent::findMoveById(id);
                if (!combinedMove || combinedMove->attributeCount != 2 ||
                    !pokemonDamageSecondaryAttributesResolved(*combinedMove, "StatusEffectAttr")) return 9530;
                const auto* hypnosis = PokerogueContent::findMoveById(95);
                const auto* twineedle = PokerogueContent::findMoveById(41);
                if (!hypnosis || !twineedle ||
                    pokemonDamageSecondaryAttributesResolved(*hypnosis, "StatusEffectAttr") ||
                    pokemonDamageSecondaryAttributesResolved(*twineedle, "StatusEffectAttr") ||
                    pokemonDamageSecondaryAttributesResolved(*combinedMove, "ConfuseAttr")) return 9533;
                uint8_t denominator = 0;
                if (!pokemonMoveCriticalDenominator(id, denominator) || denominator != 8) return 9531;
                auto combinedCheckpoint = checkpoint;
                combinedCheckpoint.playerMoveIds[0] = id;
                combinedCheckpoint.playerPp[0] = static_cast<uint8_t>(combinedMove->pp);
                combinedCheckpoint.playerParty[0].moveIds[0] = id;
                combinedCheckpoint.playerParty[0].pp[0] = combinedCheckpoint.playerParty[0].maxPp[0] =
                    static_cast<uint8_t>(combinedMove->pp);
                FirstRunRuntime combined(seed), repeatedCombined(seed);
                NativeRunSave combinedAfter{}, repeatedCombinedAfter{};
                if (!combined.restoreNativeRunSave(combinedCheckpoint) ||
                    !repeatedCombined.restoreNativeRunSave(combinedCheckpoint) || !combined.battleInputSupported() ||
                    !combined.advanceBattleTurn() || !repeatedCombined.advanceBattleTurn() ||
                    combined.captureNativeRunSave(combinedAfter) != NativeSaveResult::Ok ||
                    repeatedCombined.captureNativeRunSave(repeatedCombinedAfter) != NativeSaveResult::Ok ||
                    combinedAfter.playerPp[0] != combinedMove->pp - 1 ||
                    combinedAfter.enemyHp != repeatedCombinedAfter.enemyHp ||
                    combinedAfter.playerHp != repeatedCombinedAfter.playerHp ||
                    combinedAfter.enemyStatus.effect != repeatedCombinedAfter.enemyStatus.effect ||
                    combinedAfter.enemyStatus.present != repeatedCombinedAfter.enemyStatus.present) return 9532;
            }
            auto psychicCheckpoint = checkpoint;
            psychicCheckpoint.playerMoveIds[0] = psychicCheckpoint.playerParty[0].moveIds[0] = 94;
            psychicCheckpoint.playerPp[0] = psychicCheckpoint.playerParty[0].pp[0] =
                psychicCheckpoint.playerParty[0].maxPp[0] = 10;
            FirstRunRuntime psychic(seed), repeatedPsychic(seed);
            NativeRunSave psychicAfter{}, repeatedPsychicAfter{};
            if (!psychic.restoreNativeRunSave(psychicCheckpoint) ||
                !repeatedPsychic.restoreNativeRunSave(psychicCheckpoint) || !psychic.battleInputSupported() ||
                !psychic.advanceBattleTurn() || !repeatedPsychic.advanceBattleTurn() ||
                psychic.captureNativeRunSave(psychicAfter) != NativeSaveResult::Ok ||
                repeatedPsychic.captureNativeRunSave(repeatedPsychicAfter) != NativeSaveResult::Ok ||
                psychicAfter.playerPp[0] != 9 || psychicAfter.enemyHp != repeatedPsychicAfter.enemyHp ||
                psychicAfter.playerHp != repeatedPsychicAfter.playerHp ||
                psychicAfter.enemyStatStages[4] != repeatedPsychicAfter.enemyStatStages[4]) return 9563;
            for (const uint16_t id : {uint16_t(61), uint16_t(488), uint16_t(315), uint16_t(232), uint16_t(306), uint16_t(534)}) {
                const auto* stageMove = PokerogueContent::findMoveById(id);
                if (!stageMove) return 9602;
                auto stageCheckpoint = checkpoint;
                stageCheckpoint.playerMoveIds[0] = stageCheckpoint.playerParty[0].moveIds[0] = id;
                stageCheckpoint.playerPp[0] = stageCheckpoint.playerParty[0].pp[0] =
                    stageCheckpoint.playerParty[0].maxPp[0] = static_cast<uint8_t>(stageMove->pp);
                if (id == 315) {
                    // Test-only frozen snapshot; the production move/profile stays canonical.
                    PokemonStatusState frozen{};
                    frozen.present = true;
                    frozen.effect = PokemonStatusEffect::Freeze;
                    frozen.hasFreezeTurnsRemaining = true;
                    frozen.freezeTurnsRemaining = 3;
                    stageCheckpoint.playerStatus = stageCheckpoint.playerParty[0].status = frozen;
                }
                FirstRunRuntime stageAttack(seed), repeatedStageAttack(seed);
                NativeRunSave stageAfter{}, repeatedStageAfter{};
                if (!stageAttack.restoreNativeRunSave(stageCheckpoint) ||
                    !repeatedStageAttack.restoreNativeRunSave(stageCheckpoint) || !stageAttack.battleInputSupported() ||
                    !stageAttack.advanceBattleTurn() || !repeatedStageAttack.advanceBattleTurn() ||
                    stageAttack.captureNativeRunSave(stageAfter) != NativeSaveResult::Ok ||
                    repeatedStageAttack.captureNativeRunSave(repeatedStageAfter) != NativeSaveResult::Ok ||
                    stageAfter.playerPp[0] != stageMove->pp - 1 || stageAfter.playerHp != repeatedStageAfter.playerHp ||
                    stageAfter.enemyHp != repeatedStageAfter.enemyHp ||
                    stageAfter.playerStatStages[4] != repeatedStageAfter.playerStatStages[4] ||
                    stageAfter.playerStatStages[2] != repeatedStageAfter.playerStatStages[2] ||
                    stageAfter.enemyStatStages[4] != repeatedStageAfter.enemyStatStages[4] ||
                    stageAfter.enemyStatStages[1] != repeatedStageAfter.enemyStatStages[1] ||
                    stageAfter.playerStatStages[0] != repeatedStageAfter.playerStatStages[0]) { std::printf("Stage move=%u seed=%u pp=%u expected=%u feedback=%s\n", id, seed, stageAfter.playerPp[0], stageMove->pp - 1, stageAttack.battleFeedback().c_str()); return 9603; }
                if (id == 315 && (stageAfter.playerStatus.present ||
                    repeatedStageAfter.playerStatus.present || stageAfter.playerParty[0].status.present ||
                    stageAfter.playerStatus.freezeTurnsRemaining ||
                    stageAfter.playerStatus.hasFreezeTurnsRemaining)) return 9740;
            }
            auto growlCheckpoint = checkpoint;
            growlCheckpoint.playerMoveIds[0] = growlCheckpoint.playerParty[0].moveIds[0] = 45;
            growlCheckpoint.playerPp[0] = growlCheckpoint.playerParty[0].pp[0] =
                growlCheckpoint.playerParty[0].maxPp[0] = 40;
            growlCheckpoint.playerStatStages[5] = growlCheckpoint.playerParty[0].statStages[5] = -6;
            growlCheckpoint.enemyStatStages[6] = 6;
            FirstRunRuntime growl(seed), repeatedGrowl(seed);
            NativeRunSave growlAfter{}, repeatedGrowlAfter{};
            if (!growl.restoreNativeRunSave(growlCheckpoint) || !repeatedGrowl.restoreNativeRunSave(growlCheckpoint) ||
                !growl.battleInputSupported() || !repeatedGrowl.battleInputSupported() ||
                !growl.advanceBattleTurn() || !repeatedGrowl.advanceBattleTurn() ||
                growl.captureNativeRunSave(growlAfter) != NativeSaveResult::Ok ||
                repeatedGrowl.captureNativeRunSave(repeatedGrowlAfter) != NativeSaveResult::Ok ||
                growlAfter.playerPp[0] != 39 || growlAfter.enemyStatStages[0] != repeatedGrowlAfter.enemyStatStages[0] ||
                growlAfter.playerHp != repeatedGrowlAfter.playerHp || growlAfter.enemyHp != repeatedGrowlAfter.enemyHp)
                return 9540;
            auto confusedEmberCheckpoint = emberCheckpoint;
            confusedEmberCheckpoint.enemyConfusion = {3, true};
            FirstRunRuntime confusedEmber(seed), repeatedConfusedEmber(seed);
            NativeRunSave confusedEmberAfter{}, repeatedConfusedEmberAfter{};
            if (!confusedEmber.restoreNativeRunSave(confusedEmberCheckpoint) ||
                !repeatedConfusedEmber.restoreNativeRunSave(confusedEmberCheckpoint) ||
                !confusedEmber.advanceBattleTurn() || !repeatedConfusedEmber.advanceBattleTurn() ||
                confusedEmber.captureNativeRunSave(confusedEmberAfter) != NativeSaveResult::Ok ||
                repeatedConfusedEmber.captureNativeRunSave(repeatedConfusedEmberAfter) != NativeSaveResult::Ok ||
                confusedEmberAfter.playerPp[0] != 24 ||
                confusedEmberAfter.enemyConfusion.turns != repeatedConfusedEmberAfter.enemyConfusion.turns ||
                confusedEmberAfter.enemyHp != repeatedConfusedEmberAfter.enemyHp ||
                confusedEmberAfter.playerHp != repeatedConfusedEmberAfter.playerHp) return 9463;
            auto confusionCheckpoint = emberCheckpoint;
            confusionCheckpoint.playerMoveIds[0] = 93;
            confusionCheckpoint.playerParty[0].moveIds[0] = 93;
            double confusionEffectiveness = 1.0;
            if (calculatePokemonTypeEffectiveness(93, game.presentation().enemy.battleState,
                    confusionEffectiveness) != PokemonTypeEffectivenessResult::Ok) return 9481;
            FirstRunRuntime confusionAttack(seed), repeatedConfusionAttack(seed);
            NativeRunSave confusionAfter{}, repeatedConfusionAfter{};
            if (!confusionAttack.restoreNativeRunSave(confusionCheckpoint) ||
                !repeatedConfusionAttack.restoreNativeRunSave(confusionCheckpoint) ||
                !confusionAttack.battleInputSupported() || !confusionAttack.advanceBattleTurn() ||
                !repeatedConfusionAttack.advanceBattleTurn() ||
                confusionAttack.captureNativeRunSave(confusionAfter) != NativeSaveResult::Ok ||
                repeatedConfusionAttack.captureNativeRunSave(repeatedConfusionAfter) != NativeSaveResult::Ok ||
                confusionAfter.playerPp[0] != 24 ||
                (confusionEffectiveness > 0 && confusionAfter.enemyHp >= confusionCheckpoint.enemyHp) ||
                (confusionEffectiveness == 0 && (confusionAfter.enemyHp != confusionCheckpoint.enemyHp ||
                    confusionAfter.enemyConfusion.present)) ||
                confusionAfter.enemyHp != repeatedConfusionAfter.enemyHp ||
                confusionAfter.enemyConfusion.turns != repeatedConfusionAfter.enemyConfusion.turns) return 9480;
            auto rayCheckpoint = emberCheckpoint;
            rayCheckpoint.playerMoveIds[0] = 109;
            rayCheckpoint.playerPp[0] = 10;
            rayCheckpoint.playerParty[0].moveIds[0] = 109;
            rayCheckpoint.playerParty[0].pp[0] = rayCheckpoint.playerParty[0].maxPp[0] = 10;
            FirstRunRuntime confuseRay(seed), repeatedRay(seed);
            NativeRunSave rayAfter{}, repeatedRayAfter{};
            if (!confuseRay.restoreNativeRunSave(rayCheckpoint) || !repeatedRay.restoreNativeRunSave(rayCheckpoint) ||
                !confuseRay.battleInputSupported() || !confuseRay.advanceBattleTurn() || !repeatedRay.advanceBattleTurn() ||
                confuseRay.captureNativeRunSave(rayAfter) != NativeSaveResult::Ok ||
                repeatedRay.captureNativeRunSave(repeatedRayAfter) != NativeSaveResult::Ok || rayAfter.playerPp[0] != 9 ||
                rayAfter.enemyHp != repeatedRayAfter.enemyHp || rayAfter.playerHp != repeatedRayAfter.playerHp ||
                rayAfter.enemyConfusion.turns != repeatedRayAfter.enemyConfusion.turns) return 9490;
            checkedExecution = true;
        }
        if (checkedRejection && checkedExecution) return 0;
    }
    return 9383; // Missing real encounter is a failure, not skipped coverage.
}

static int checkTrainerExperienceReplay() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 256; ++seed) {
        FirstRunRuntime game(seed);
        bool eligible = true;
        for (uint16_t wave = 1; wave <= 4; ++wave) {
            const auto& context = game.presentation();
            if (classifyClassicWave(wave) == ClassicWaveKind::TrainerChanceRequired &&
                    context.trainerPartyCount && context.enemy.actorIdentityResolved) {
                if (!context.trainerPartyBattleStatesResolved ||
                    !context.trainerPartyMovesetsResolved || !context.trainerPartyIvsResolved ||
                    context.activeTrainerPartyIndex >= context.trainerPartyCount ||
                    context.enemy.dex != context.trainerParty[context.activeTrainerPartyIndex].dex) return 228;
                for (uint8_t member = 0; member < context.trainerPartyCount; ++member)
                    if (!context.trainerParty[member].actorIdentityResolved ||
                        !context.trainerParty[member].battleState.moveCount) return 229;
            }
            if (!context.enemy.actorIdentityResolved || context.secondEnemy.dex ||
                context.trainerPartyCount) { eligible = false; break; }
            NativeRunSave won{};
            game.captureNativeRunSave(won);
            won.stage = NativeSaveStage::BattleWon;
            setSingleParticipantFixture(won, game);
            won.encounterDex = context.enemy.dex;
            won.playerHp = context.player.battleState.hp;
            won.enemyHp = 0;
            won.battleTurn = 1;
            won.playerMoveCount = context.player.battleState.moveCount;
            won.enemyMoveCount = context.enemy.battleState.moveCount;
            for (uint8_t slot = 0; slot < won.playerMoveCount; ++slot) {
                won.playerMoveIds[slot] = context.player.battleState.moves[slot].moveId;
                won.playerPp[slot] = context.player.battleState.moves[slot].pp;
            }
            for (uint8_t slot = 0; slot < won.enemyMoveCount; ++slot) {
                won.enemyMoveIds[slot] = context.enemy.battleState.moves[slot].moveId;
                won.enemyPp[slot] = context.enemy.battleState.moves[slot].pp;
            }
            if (!game.restoreNativeRunSave(won) || !game.advanceBattleTurn() ||
                !game.skipVictoryReward()) { eligible = false; break; }
        }
        if (!eligible || game.presentation().trainerPartyCount != 2 ||
            !game.presentation().trainerPartyBattleStatesResolved) continue;
        NativeRunSave checkpoint{};
        game.captureNativeRunSave(checkpoint);
        if (checkpoint.stage != NativeSaveStage::BattleActive) continue;
        NativeRunSave confusedCheckpoint = checkpoint;
        confusedCheckpoint.playerConfusion = {3, true};
        confusedCheckpoint.enemyConfusion = {2, true};
        if (confusedCheckpoint.playerPartyCount)
            confusedCheckpoint.playerParty[confusedCheckpoint.activePlayerMember].confusion =
                confusedCheckpoint.playerConfusion;
        confusedCheckpoint.trainerParty[confusedCheckpoint.activeTrainerMember].confusion =
            confusedCheckpoint.enemyConfusion;
        if (!game.restoreNativeRunSave(confusedCheckpoint) ||
            game.presentation().player.battleState.confusion.turns != 3 ||
            game.presentation().enemy.battleState.confusion.turns != 2) return 9190;
        NativeRunSave recapturedConfusion{};
        if (game.captureNativeRunSave(recapturedConfusion) != NativeSaveResult::Ok ||
            recapturedConfusion.playerConfusion.turns != 3 || recapturedConfusion.enemyConfusion.turns != 2)
            return 9191;
        if (!game.restoreNativeRunSave(checkpoint)) return 9192;
        NativeRunSave memberWon = checkpoint;
        memberWon.stage = NativeSaveStage::BattleWon;
        setSingleParticipantFixture(memberWon, game);
        memberWon.enemyHp = 0;
        memberWon.trainerParty[memberWon.activeTrainerMember].hp = 0;
        if (!game.restoreNativeRunSave(memberWon) ||
            game.victoryPlan().contains(ClassicVictoryStep::SelectModifier) ||
            game.victoryPlan().contains(ClassicVictoryStep::NewBattle) ||
            game.skipVictoryReward() || game.run().wave != 5) return 28;
        if (!game.advanceBattleTurn() || game.battleFinished() ||
            game.presentation().activeTrainerPartyIndex != 1 || game.run().wave != 5) return 29;
        if (!game.restoreNativeRunSave(checkpoint)) return 30;
        const auto& context = game.presentation();
        const auto* defeated = PokerogueContent::findSpeciesByDex(context.trainerParty[0].dex);
        const auto* starter = PokerogueContent::findSpeciesByDex(context.player.dex);
        const auto* form = context.trainerParty[0].formId
            ? PokerogueContent::findFormById(context.trainerParty[0].formId) : nullptr;
        double raw = 0;
        uint32_t award = 0;
        PokemonExperienceProgress progress{};
        if (!defeated || !starter || pokemonExperienceForDefeat(*defeated,
                context.trainerParty[0].level, raw, form) != PokemonExperienceResult::Ok ||
            pokemonSingleParticipantExperience(raw, true, award) != PokemonExperienceResult::Ok ||
            applyPokemonExperience(starter->growthRate, checkpoint.playerLevel,
                checkpoint.playerExperience, award, classicExperienceLevelCap(5), progress) !=
                PokemonExperienceResult::Ok) return 8;
        checkpoint.trainerParty[0].hp = 0;
        checkpoint.activeTrainerMember = 1;
        checkpoint.encounterDex = checkpoint.trainerParty[1].speciesDex;
        checkpoint.enemyHp = checkpoint.trainerParty[1].hp;
        checkpoint.enemyMoveCount = checkpoint.trainerParty[1].moveCount;
        for (uint8_t slot = 0; slot < 4; ++slot) {
            checkpoint.enemyMoveIds[slot] = checkpoint.trainerParty[1].moveIds[slot];
            checkpoint.enemyPp[slot] = checkpoint.trainerParty[1].pp[slot];
        }
        checkpoint.playerLevel = progress.level;
        checkpoint.playerExperience = progress.totalExperience;
        checkpoint.playerHp = 1;
        checkpoint.battleTurn = 2;
        if (!game.restoreNativeRunSave(checkpoint)) return 9;
        NativeRunSave restored{};
        game.captureNativeRunSave(restored);
        if (restored.playerExperience != checkpoint.playerExperience ||
            restored.activeTrainerMember != 1 || restored.trainerParty[0].hp ||
            restored.battleTurn != 2) return 10;
        NativeRunSave won = restored;
        won.stage = NativeSaveStage::BattleWon;
        setSingleParticipantFixture(won, game);
        won.enemyHp = 0;
        won.trainerParty[1].hp = 0;
        if (!game.restoreNativeRunSave(won) || !game.advanceBattleTurn()) return 12;
        NativeRunSave finalExperience{};
        game.captureNativeRunSave(finalExperience);
        if (finalExperience.stage != NativeSaveStage::ExperienceGranted ||
            !game.restoreNativeRunSave(finalExperience)) return 13;
        NativeRunSave roundtrip{};
        game.captureNativeRunSave(roundtrip);
        if (roundtrip.playerExperience != finalExperience.playerExperience ||
            roundtrip.playerLevel != finalExperience.playerLevel ||
            roundtrip.stage != NativeSaveStage::ExperienceGranted) return 14;
        return 0;
    }
    return 11; // No reconstructable pinned party: do not silently skip.
}

static int checkResolvedActionFieldLifecycle() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 512; ++seed) {
        FirstRunRuntime game(seed);
        for (unsigned slot = 0; slot < 4 && !game.battleInputSupported(); ++slot)
            game.selectBattleMove(1);
        if (!game.battleInputSupported() || !game.advanceBattleTurn()) continue;
        NativeRunSave active{};
        game.captureNativeRunSave(active);
        if (active.stage != NativeSaveStage::BattleActive) continue;
        active.trickRoomTurnsLeft = 3;
        active.trickRoomMaxDuration = 5;
        active.trickRoomSourceMoveId = 433;
        active.trickRoomSourcePokemonId = game.presentation().player.battleState.pokemonId;
        if (!game.restoreNativeRunSave(active)) return 28;
        for (unsigned slot = 0; slot < 4 && !game.battleInputSupported(); ++slot)
            game.selectBattleMove(1);
        if (!game.battleInputSupported()) continue;
        const uint8_t moveSlot = game.selectedBattleMove();
        const auto& player = game.presentation().player.battleState;
        uint8_t cost = 0;
        if (!pokemonSingleOpponentPpCost(game.presentation().enemy.battleState.abilityId, cost)) return 29;
        const uint8_t initialPp = player.moves[moveSlot].pp;
        if (!game.advanceBattleTurn()) return 30;
        NativeRunSave after{};
        game.captureNativeRunSave(after);
        if (after.trickRoomTurnsLeft != 2 || after.trickRoomSourceMoveId != 433) return 31;
        if (after.weatherType != static_cast<uint8_t>(game.arenaWeather().type) ||
            after.weatherTurnsLeft != game.arenaWeather().turnsLeft ||
            after.weatherMaxDuration != game.arenaWeather().maxDuration) return 34;
        if (game.presentation().player.battleState.hp && after.playerPp[moveSlot] !=
                initialPp - (cost < initialPp ? cost : initialPp)) return 32;
        if (!pokemonWeatherLifecycleSupported(game.presentation().player.battleState.abilityId) ||
            !pokemonWeatherLifecycleSupported(game.presentation().enemy.battleState.abilityId)) continue;
        active.weatherType = 2;
        active.weatherTurnsLeft = active.weatherMaxDuration = 1;
        active.trickRoomTurnsLeft = active.trickRoomMaxDuration = 0;
        active.trickRoomSourceMoveId = active.trickRoomSourcePokemonId = 0;
        if (!game.restoreNativeRunSave(active)) return 37;
        for (unsigned slot = 0; slot < 4 && !game.battleInputSupported(); ++slot) game.selectBattleMove(1);
        if (!game.battleInputSupported()) continue;
        const auto* selected = PokerogueContent::findMoveById(
            game.presentation().player.battleState.moves[game.selectedBattleMove()].moveId);
        if (!selected || selected->category == PokerogueContent::MoveStatus) continue;
        if (!game.advanceBattleTurn()) return 38;
        NativeRunSave expired{};
        game.captureNativeRunSave(expired);
        if (game.arenaWeather().type != PokemonEffectiveWeather::None ||
            expired.weatherType || expired.weatherTurnsLeft || expired.weatherMaxDuration) return 39;
        return 0;
    }
    return 33; // No supported real encounter: never silently skip integration coverage.
}

static int checkTrainerInteractiveBattle() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 256; ++seed) {
        FirstRunRuntime game(seed);
        bool eligible = true;
        for (uint16_t wave = 1; wave <= 4; ++wave) {
            const auto& context = game.presentation();
            if (!context.enemy.actorIdentityResolved || context.secondEnemy.dex ||
                context.trainerPartyCount) { eligible = false; break; }
            NativeRunSave won{};
            game.captureNativeRunSave(won);
            won.stage = NativeSaveStage::BattleWon;
            setSingleParticipantFixture(won, game);
            won.encounterDex = context.enemy.dex;
            won.playerHp = context.player.battleState.hp;
            won.enemyHp = 0;
            won.battleTurn = 1;
            won.playerMoveCount = context.player.battleState.moveCount;
            won.enemyMoveCount = context.enemy.battleState.moveCount;
            for (uint8_t slot = 0; slot < won.playerMoveCount; ++slot) {
                won.playerMoveIds[slot] = context.player.battleState.moves[slot].moveId;
                won.playerPp[slot] = context.player.battleState.moves[slot].pp;
            }
            for (uint8_t slot = 0; slot < won.enemyMoveCount; ++slot) {
                won.enemyMoveIds[slot] = context.enemy.battleState.moves[slot].moveId;
                won.enemyPp[slot] = context.enemy.battleState.moves[slot].pp;
            }
            if (!game.restoreNativeRunSave(won) || !game.advanceBattleTurn() ||
                !game.skipVictoryReward()) { eligible = false; break; }
        }
        if (!eligible || game.presentation().trainerPartyCount != 2 ||
            !game.presentation().trainerPartyBattleStatesResolved) continue;

        // At wave 5, verify trainer battle is supported and interactive
        if (!game.trainerBattleSupported()) return 40;
        if (!game.battleInputSupported()) return 41;

        // Execute a battle turn interactively against the trainer
        const uint16_t initialPlayerHp = game.presentation().player.battleState.hp;
        const uint16_t initialEnemyHp = game.presentation().enemy.battleState.hp;
        if (!game.advanceBattleTurn()) return 42;
        const auto& activeAfter = game.presentation();
        if (activeAfter.enemy.battleState.hp == initialEnemyHp &&
            activeAfter.player.battleState.hp == initialPlayerHp &&
            activeAfter.activeTrainerPartyIndex == 0) return 43;

        // Explicit player snapshots use the same trainer reconstruction path
        // needed by later waves, without replaying earlier EXP/reward history.
        NativeRunSave explicitTrainer{};
        if (game.captureNativeRunSave(explicitTrainer) != NativeSaveResult::Ok) return 10150;
        explicitTrainer.playerPartyCount = activeAfter.playerPartyCount;
        explicitTrainer.activePlayerMember = activeAfter.activePlayerPartyIndex;
        for (uint8_t member = 0; member < activeAfter.playerPartyCount; ++member) {
            const auto& actor = member == activeAfter.activePlayerPartyIndex
                ? activeAfter.player : activeAfter.playerParty[member];
            if (!captureNativePokemonActorSave(actor.battleState, actor.actor, actor.totalExperience,
                explicitTrainer.playerParty[member])) return 10151;
        }
        FirstRunRuntime explicitTrainerRestore(seed);
        NativeRunSave explicitTrainerRecaptured{};
        if (!explicitTrainerRestore.restoreNativeRunSave(explicitTrainer) ||
            explicitTrainerRestore.captureNativeRunSave(explicitTrainerRecaptured) != NativeSaveResult::Ok ||
            explicitTrainerRecaptured.trainerTypeId != explicitTrainer.trainerTypeId ||
            explicitTrainerRecaptured.activeTrainerMember != explicitTrainer.activeTrainerMember ||
            explicitTrainerRecaptured.trainerPartyCount != explicitTrainer.trainerPartyCount ||
            explicitTrainerRecaptured.playerExperience != explicitTrainer.playerExperience ||
            explicitTrainerRecaptured.enemySwitchCounter != explicitTrainer.enemySwitchCounter) return 10152;
        for (uint8_t member = 0; member < explicitTrainer.trainerPartyCount; ++member) {
            const auto& expected = explicitTrainer.trainerParty[member];
            const auto& actual = explicitTrainerRecaptured.trainerParty[member];
            if (actual.speciesDex != expected.speciesDex || actual.hp != expected.hp ||
                actual.moveCount != expected.moveCount || actual.sturdyTag != expected.sturdyTag) return 10153;
            for (uint8_t slot = 0; slot < actual.moveCount; ++slot)
                if (actual.moveIds[slot] != expected.moveIds[slot] || actual.pp[slot] != expected.pp[slot]) return 10154;
        }

        // Defeat first trainer Pokemon and transition to second
        NativeRunSave firstDefeated{};
        game.captureNativeRunSave(firstDefeated);
        firstDefeated.stage = NativeSaveStage::BattleWon;
        setSingleParticipantFixture(firstDefeated, game);
        firstDefeated.enemyHp = 0;
        firstDefeated.trainerParty[firstDefeated.activeTrainerMember].hp = 0;
        if (!game.restoreNativeRunSave(firstDefeated)) return 44;
        if (!game.advanceBattleTurn()) return 45;
        // Active trainer member must now be the reserve (index 1)
        if (game.presentation().activeTrainerPartyIndex != 1) return 46;
        if (game.battleFinished()) return 47;
        if (!game.trainerBattleSupported() || !game.battleInputSupported()) return 48;

        // Execute interactive turn against the second Pokemon
        if (!game.advanceBattleTurn()) return 49;

        // Defeat the second Pokemon
        NativeRunSave secondDefeated{};
        game.captureNativeRunSave(secondDefeated);
        secondDefeated.stage = NativeSaveStage::BattleWon;
        setSingleParticipantFixture(secondDefeated, game);
        secondDefeated.enemyHp = 0;
        secondDefeated.trainerParty[secondDefeated.activeTrainerMember].hp = 0;
        if (!game.restoreNativeRunSave(secondDefeated)) return 50;
        if (!game.advanceBattleTurn()) return 51;
        // Both trainer Pokémon fainted: battle is won, experience granted
        if (!game.battleFinished() || !game.playerWon() || !game.experienceGranted()) return 52;
        // Advancing now generates rewards or allows skipping
        if (!game.skipVictoryReward()) return 53;
        // Wave progresses to wave 6
        if (game.run().wave != 6) return 54;
        return 0;
    }
    return 55;
}

static int checkModifierRewardGenerationAndClaim() {
    using namespace Pokerogue3DS;
    // Rejected real encounters must not publish partial combat state or RNG draws.
    unsigned rejectedTurns = 0;
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        FirstRunRuntime game(seed);
        const auto beforePlayer = game.presentation().player.battleState;
        const auto beforeEnemy = game.presentation().enemy.battleState;
        const auto beforeSecond = game.presentation().secondEnemy.battleState;
        const auto beforeRng = game.battleRng().state();
        if (game.advanceBattleTurn()) continue;
        ++rejectedTurns;
        const auto afterRng = game.battleRng().state();
        if (beforeRng.carry != afterRng.carry || beforeRng.s0 != afterRng.s0 ||
            beforeRng.s1 != afterRng.s1 || beforeRng.s2 != afterRng.s2) return 217;
        const PokemonBattleState* before[] = {&beforePlayer, &beforeEnemy, &beforeSecond};
        const PokemonBattleState* after[] = {&game.presentation().player.battleState,
            &game.presentation().enemy.battleState, &game.presentation().secondEnemy.battleState};
        for (unsigned actor = 0; actor < 3; ++actor) {
            if (before[actor]->hp != after[actor]->hp ||
                before[actor]->moveCount != after[actor]->moveCount) return 218;
            for (unsigned slot = 0; slot < before[actor]->moveCount; ++slot)
                if (before[actor]->moves[slot].pp != after[actor]->moves[slot].pp) return 219;
        }
    }
    if (!rejectedTurns) return 220; // The capability boundary must be exercised.
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        FirstRunRuntime game(seed);
        const auto& context = game.presentation();
        if (!context.enemy.actorIdentityResolved || context.secondEnemy.dex) continue;
        NativeRunSave won{};
        if (!captureActiveTestCheckpoint(game, won)) return 60;
        won.stage = NativeSaveStage::BattleWon;
        setSingleParticipantFixture(won, game);
        won.enemyHp = 0;
        if (!game.restoreNativeRunSave(won)) return 60;
        if (!game.advanceBattleTurn()) return 61;
        if (!game.experienceGranted() || !game.battleFinished() || !game.playerWon()) return 62;
        if (!game.advanceBattleTurn()) return 63; // Triggers reward generation
        if (!game.rewardsPending() || game.rewardChoiceCount() != 3) return 64;
        if (game.selectedRewardChoice() != 0) return 65;
        if (!game.selectBattleMove(1) || game.selectedRewardChoice() != 1) return 66;
        if (!game.selectBattleMove(-1) || game.selectedRewardChoice() != 0) return 67;
        // Check a canonically generated choice with no current native reward adapter.
        auto unsupportedRewardGame = game;
        for (uint8_t option = 0; option < unsupportedRewardGame.rewardChoiceCount(); ++option) {
            const auto* candidate = unsupportedRewardGame.rewardChoice(unsupportedRewardGame.selectedRewardChoice());
            const char* id = candidate && candidate->poolEntry ? candidate->poolEntry->itemId : nullptr;
            if (id && (std::strcmp(id, "BERRY") == 0 || std::strcmp(id, "RARE_CANDY") == 0)) {
                const auto beforeWave = unsupportedRewardGame.run().wave;
                const auto beforeHp = unsupportedRewardGame.presentation().player.battleState.hp;
                if (unsupportedRewardGame.claimRewardChoice() || !unsupportedRewardGame.rewardsPending() ||
                    unsupportedRewardGame.run().wave != beforeWave ||
                    unsupportedRewardGame.presentation().player.battleState.hp != beforeHp) return 447;
            }
            unsupportedRewardGame.selectRewardChoice(1);
        }
        bool claimed = false;
        for (uint8_t option = 0; option < game.rewardChoiceCount(); ++option) {
            auto probe = game;
            if (probe.claimRewardChoice()) {
                claimed = game.claimRewardChoice();
                break;
            }
            game.selectRewardChoice(1);
        }
        if (!claimed) {
            std::printf("Reward seed=%u choice0=%s choice1=%s choice2=%s feedback=%s\n",
                seed,
                game.rewardChoice(0) && game.rewardChoice(0)->poolEntry ? game.rewardChoice(0)->poolEntry->itemId : "null",
                game.rewardChoice(1) && game.rewardChoice(1)->poolEntry ? game.rewardChoice(1)->poolEntry->itemId : "null",
                game.rewardChoice(2) && game.rewardChoice(2)->poolEntry ? game.rewardChoice(2)->poolEntry->itemId : "null",
                game.battleFeedback().c_str());
            return 69;
        }
        if (game.run().wave != 2 || game.rewardsPending()) return 70;
        return 0;
    }
    return 71;
}

static int checkFirstRivalEncounterTraceability() {
    using namespace Pokerogue3DS;
    const auto* rival = PokerogueContent::findTrainerTypeByKey("rival");
    if (!rival) return 11000;
    const auto* laterRival = PokerogueContent::findTrainerTypeByKey("rival_2");
    const auto* laterSlot = laterRival ? PokerogueContent::findRivalPartySlot(laterRival->id, 2) : nullptr;
    if (!laterSlot || laterSlot->resolved) return 11014; // Balanced later callbacks remain explicit.
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        FirstRunRuntime game(seed);
        bool eligible = true;
        for (uint16_t wave = 1; wave < 8; ++wave) {
            NativeRunSave defeated{};
            if (!captureActiveTestCheckpoint(game, defeated)) { eligible = false; break; }
            defeated.stage = NativeSaveStage::BattleWon;
            setSingleParticipantFixture(defeated, game);
            defeated.enemyHp = 0;
            if (defeated.doubleBattle) defeated.secondEnemy.hp = 0;
            for (uint8_t member = 0; member < defeated.trainerPartyCount; ++member)
                defeated.trainerParty[member].hp = 0;
            if (!game.restoreNativeRunSave(defeated) || !game.advanceBattleTurn() ||
                !game.skipVictoryReward()) { eligible = false; break; }
        }
        if (!eligible || game.run().wave != 8) continue;
        const auto& field = game.presentation();
        if (field.trainerTypeId != rival->id || field.trainerPartyCount != 2 ||
            !field.trainerPartyBattleStatesResolved) return 11001;
        for (uint8_t member = 0; member < 2; ++member) {
            const auto* slot = PokerogueContent::findRivalPartySlot(rival->id, member);
            const auto& actor = field.trainerParty[member];
            if (!slot || !slot->resolved || !actor.actorIdentityResolved) return 11002;
            const PokerogueContent::RivalPartyChoice* chosen = nullptr;
            for (uint16_t choice = 0; choice < slot->choiceCount; ++choice) {
                const auto& row = PokerogueContent::kRivalPartyChoices[slot->choiceOffset + choice];
                if (std::strcmp(row.speciesId, actor.speciesId) == 0) chosen = &row;
            }
            if (!chosen || (chosen->forcedAbilityIndex >= 0 &&
                actor.actor.abilityIndex != chosen->forcedAbilityIndex)) return 11003;
            if (slot->teraPrimary) {
                const auto* species = PokerogueContent::findSpeciesByDex(actor.dex);
                if (!species || !actor.actor.initialTeraTypeResolved ||
                    actor.actor.initialTeraTypeIndex != 0 || std::strcmp(actor.actor.initialTeraType,
                        resolvePokemonTypeSymbol(species->type1))) return 11004;
            }
        }
        NativeRunSave saved{}, repeated{};
        FirstRunRuntime restored(seed);
        if (game.captureNativeRunSave(saved) != NativeSaveResult::Ok ||
            !restored.restoreNativeRunSave(saved) ||
            restored.captureNativeRunSave(repeated) != NativeSaveResult::Ok ||
            repeated.trainerTypeId != rival->id || repeated.trainerPartyCount != 2) return 11005;
        for (uint8_t member = 0; member < 2; ++member) {
            const auto& original = field.trainerParty[member];
            const auto& replay = restored.presentation().trainerParty[member];
            if (original.dex != replay.dex || original.actor.pokemonId != replay.actor.pokemonId ||
                original.actor.abilityIndex != replay.actor.abilityIndex ||
                original.battleState.abilityId != replay.battleState.abilityId ||
                original.moveCount != replay.moveCount) return 11006;
            for (uint8_t move = 0; move < original.moveCount; ++move)
                if (original.moveIds[move] != replay.moveIds[move]) return 11007;
        }
        if (!game.battleInputSupported() || !restored.battleInputSupported()) continue;
        if (!game.advanceBattleTurn() || !restored.advanceBattleTurn()) return 11009;
        NativeRunSave advanced{}, advancedReplay{};
        if (game.captureNativeRunSave(advanced) != NativeSaveResult::Ok ||
            restored.captureNativeRunSave(advancedReplay) != NativeSaveResult::Ok ||
            advanced.playerHp != advancedReplay.playerHp || advanced.enemyHp != advancedReplay.enemyHp ||
            advanced.battleTurn != advancedReplay.battleTurn) return 11010;
        for (uint8_t move = 0; move < advanced.playerMoveCount; ++move)
            if (advanced.playerPp[move] != advancedReplay.playerPp[move]) return 11011;
        for (uint8_t move = 0; move < advanced.enemyMoveCount; ++move)
            if (advanced.enemyPp[move] != advancedReplay.enemyPp[move]) return 11012;
        const auto a = game.battleRng().state(), b = restored.battleRng().state();
        if (a.carry != b.carry || a.s0 != b.s0 || a.s1 != b.s1 || a.s2 != b.s2) return 11013;
        return 0;
    }
    return 11008;
}

static int checkCanonicalBerryEffects() {
    using namespace Pokerogue3DS;
    PokemonBattleState actor{};
    actor.pokemonId = 123;
    actor.maxHp = 100;
    actor.hp = 20;
    actor.moveCount = 2;
    actor.moves[0].maxPp = 5;
    actor.moves[0].pp = 3;
    actor.moves[1].maxPp = 15;
    actor.moves[1].pp = 0;
    actor.status.present = true;
    actor.status.effect = PokemonStatusEffect::Poison;
    PokemonBerryEffectPolicy policy{};
    policy.callbacksResolved = true;
    policy.attackHistoryResolved = true;
    policy.superEffectiveHitReceived = true;
    policy.criticalTagResolved = true;
    PokerogueRngAdapter rng;
    const uint16_t seed[] = {'b','e','r','r','y'};
    rng.sow(seed, 5);
    const auto sameRng = [](const PokerogueRngState& a, const PokerogueRngState& b) {
        return a.carry == b.carry && a.s0 == b.s0 && a.s1 == b.s1 && a.s2 == b.s2;
    };
    {
    // Primary/passive Berry policies come from inspected class ancestry, not
    // species/ability IDs hardcoded into the presentation or effect engine.
    uint16_t ripen = 65535, gluttony = 65535, unnerve = 65535, neutral = 65535;
    for (const auto& row : PokerogueContent::kBerryAbilityProfiles) {
        if (!row.resolved) continue;
        if (row.effectMultiplier == 2 && row.thresholdMultiplier == 1 && !row.preventsUse && !row.healFraction)
            ripen = row.abilityId;
        if (row.thresholdMultiplier == 2) gluttony = row.abilityId;
        if (row.preventsUse) unnerve = row.abilityId;
        if (!row.abilityId) neutral = row.abilityId;
    }
    if (ripen == 65535 || gluttony == 65535 || unnerve == 65535 || neutral == 65535 ||
        !PokerogueContent::kBerryAbilitySource.sourceHash[0]) return 10601;
    uint16_t statBerry = 65535, healBerry = 65535;
    for (const auto& row : PokerogueContent::kBerryEffectProfiles) {
        if (!std::strcmp(row.effect, "STAT")) statBerry = row.id;
        if (!std::strcmp(row.effect, "HEAL") && row.doubledEffectCallback) healBerry = row.id;
    }
    auto berryPolicy = policy;
    const uint16_t composed[] = {gluttony, ripen};
    auto aboveThreshold = actor;
    aboveThreshold.hp = 40;
    if (resolvePokemonBerryAbilityEffects(statBerry, aboveThreshold, composed, 2, true, berryPolicy) != PokemonBerryEffectResult::Ready ||
        berryPolicy.thresholdMultiplier != 2 || berryPolicy.effectMultiplier != 2) return 10602;
    PokemonBerryEffectPlan boostedBerryPlan{};
    if (planPokemonBerryEffect(statBerry, aboveThreshold, berryPolicy, rng, boostedBerryPlan) != PokemonBerryEffectResult::Ready ||
        boostedBerryPlan.stagesRequested != 2) return 10603;
    if (resolvePokemonBerryAbilityEffects(statBerry, actor, composed, 2, true, berryPolicy) != PokemonBerryEffectResult::Ready ||
        berryPolicy.thresholdMultiplier != 1 || berryPolicy.effectMultiplier != 2) return 10604;
    berryPolicy.effectMultiplier = 99;
    if (resolvePokemonBerryAbilityEffects(healBerry, actor, &neutral, 1, false, berryPolicy) != PokemonBerryEffectResult::UnresolvedPolicy ||
        berryPolicy.effectMultiplier != 99) return 10605;
    const uint16_t unknownAbility = 65535;
    if (resolvePokemonBerryAbilityEffects(healBerry, actor, &unknownAbility, 1, true, berryPolicy) != PokemonBerryEffectResult::UnresolvedPolicy ||
        berryPolicy.effectMultiplier != 99) return 10606;
    bool blocksBerry = false;
    if (resolvePokemonOpponentBerryBlock(&unnerve, 1, true, blocksBerry) != PokemonBerryEffectResult::Ready || !blocksBerry ||
        resolvePokemonOpponentBerryBlock(&neutral, 1, true, blocksBerry) != PokemonBerryEffectResult::Ready || blocksBerry)
        return 10607;
    blocksBerry = true;
    if (resolvePokemonOpponentBerryBlock(&unknownAbility, 1, true, blocksBerry) != PokemonBerryEffectResult::UnresolvedPolicy || !blocksBerry)
        return 10608;
    }
    size_t count = 0;
    for (const auto& profile : PokerogueContent::kBerryEffectProfiles) {
        if (!profile.resolved || !profile.nameKey || !profile.effectKey) return 10301;
        PokemonBerryEffectPlan plan{};
        auto expected = rng;
        int32_t expectedStat = 0;
        if (!std::strcmp(profile.effect, "RANDOM_STAT"))
            expectedStat = expected.randSeedInt(5, 1);
        if (planPokemonBerryEffect(profile.id, actor, policy, rng, plan) != PokemonBerryEffectResult::Ready ||
            plan.ownerPokemonId != 123 || plan.berryType != profile.id || !sameRng(rng.state(), expected.state())) return 10302;
        if (!std::strcmp(profile.effect, "HEAL") && plan.healingRequested != 25) return 10303;
        if (!std::strcmp(profile.effect, "CURE_STATUS") && (!plan.cureStatus || !plan.cureConfusion)) return 10304;
        if (!std::strcmp(profile.effect, "STAT") && (plan.stat != profile.stat || plan.stagesRequested != 1)) return 10305;
        if (!std::strcmp(profile.effect, "CRIT_BOOST") && !plan.addCriticalBoost) return 10306;
        if (!std::strcmp(profile.effect, "RANDOM_STAT") && (plan.stat != expectedStat || plan.stagesRequested != 2)) return 10307;
        if (!std::strcmp(profile.effect, "RESTORE_PP") && (plan.ppSlot != 1 || plan.ppAfter != 10)) return 10308;
        // Planning cannot execute queued healing/stat/status phases early.
        if (actor.hp != 20 || actor.statStages[0] || actor.moves[1].pp || !actor.status.present) return 10309;
        auto unavailable = policy;
        unavailable.callbacksResolved = false;
        plan.ownerPokemonId = 987;
        const auto before = rng.state();
        if (planPokemonBerryEffect(profile.id, actor, unavailable, rng, plan) != PokemonBerryEffectResult::UnresolvedPolicy ||
            plan.ownerPokemonId != 987 || !sameRng(before, rng.state())) return 10310;
        if (!std::strcmp(profile.predicate, "LOW_HP") || !std::strcmp(profile.predicate, "LOW_HP_STAT") ||
            !std::strcmp(profile.predicate, "LOW_HP_NO_CRIT")) {
            auto boundary = actor;
            boundary.hp = static_cast<uint16_t>(100 * profile.hpThreshold);
            if (planPokemonBerryEffect(profile.id, boundary, policy, rng, plan) != PokemonBerryEffectResult::NoEffect) return 10311;
        }
        if (!std::strcmp(profile.predicate, "LOW_HP_STAT")) {
            auto capped = actor;
            capped.statStages[profile.stat - 1] = 6;
            if (planPokemonBerryEffect(profile.id, capped, policy, rng, plan) != PokemonBerryEffectResult::NoEffect) return 10312;
            auto enhanced = policy;
            enhanced.thresholdMultiplier = 2;
            enhanced.effectMultiplier = 2;
            auto higherHp = actor;
            higherHp.hp = 40;
            if (planPokemonBerryEffect(profile.id, higherHp, enhanced, rng, plan) != PokemonBerryEffectResult::Ready ||
                plan.stagesRequested != 2) return 10313;
        }
        if (!std::strcmp(profile.predicate, "LOW_HP_NO_CRIT")) {
            auto critical = policy;
            critical.criticalBoostPresent = true;
            if (planPokemonBerryEffect(profile.id, actor, critical, rng, plan) != PokemonBerryEffectResult::NoEffect) return 10314;
            critical.criticalTagResolved = false;
            if (planPokemonBerryEffect(profile.id, actor, critical, rng, plan) != PokemonBerryEffectResult::UnresolvedPolicy) return 10315;
        }
        if (profile.doubledEffectCallback) {
            auto doubled = policy;
            doubled.effectMultiplier = 2;
            if (planPokemonBerryEffect(profile.id, actor, doubled, rng, plan) != PokemonBerryEffectResult::Ready) return 10316;
            if (!std::strcmp(profile.effect, "HEAL") && plan.healingRequested != 50) return 10317;
            if ((!std::strcmp(profile.effect, "STAT") || !std::strcmp(profile.effect, "RANDOM_STAT")) &&
                plan.stagesRequested != 2 * profile.amount) return 10318;
        }
        ++count;
    }
    if (count != 11 || !PokerogueContent::kBerryEffectSource.sourceHash[0] ||
        !PokerogueContent::kBerryPhaseSource.sourceHash[0]) return 10319;
    const uint16_t pouchStacks[] = {1, 2, 3};
    bool sawPreserved = false, sawConsumed = false, sawShortCircuit = false;
    for (uint32_t wave = 1; wave <= 100; ++wave) {
        PokerogueBattleRng battleRng;
        if (!battleRng.initialize(seed, 5, wave) || !battleRng.beginTurn(1)) return 10327;
        auto expected = battleRng;
        bool expectedPreserved = false;
        unsigned draws = 0;
        for (uint16_t stacks : pouchStacks) {
            if (expectedPreserved) break;
            int32_t roll = 0;
            if (!expected.randSeedInt(10, roll)) return 10328;
            expectedPreserved = roll < stacks * 3;
            ++draws;
        }
        bool preserved = !expectedPreserved;
        if (!resolvePokemonBerryPreservation(pouchStacks, 3, battleRng, preserved) ||
            preserved != expectedPreserved || !sameRng(battleRng.state(), expected.state())) return 10329;
        sawPreserved |= preserved;
        sawConsumed |= !preserved;
        sawShortCircuit |= draws < 3;
        // Exercise a single pouch too: three successive pouches have a 97.2%
        // combined success chance, so this seed sample need not contain loss.
        PokerogueBattleRng singlePouch;
        if (!singlePouch.initialize(seed, 5, wave) || !singlePouch.beginTurn(1)) return 10339;
        auto singleExpected = singlePouch;
        int32_t singleRoll = 0;
        if (!singleExpected.randSeedInt(10, singleRoll) ||
            !resolvePokemonBerryPreservation(pouchStacks, 1, singlePouch, preserved) ||
            preserved != (singleRoll < 3) || !sameRng(singlePouch.state(), singleExpected.state())) return 10340;
        sawPreserved |= preserved;
        sawConsumed |= !preserved;
        const auto before = battleRng.state();
        const uint16_t invalid[] = {1,4};
        preserved = true;
        if (resolvePokemonBerryPreservation(invalid, 2, battleRng, preserved) || !preserved ||
            !sameRng(before, battleRng.state())) return 10330;
    }
    if (!sawPreserved || !sawConsumed || !sawShortCircuit ||
        !PokerogueContent::kBerryPreserveSource.sourceHash[0]) return 10331;
    PokerogueBattleRng uninitialized;
    bool emptyPreserved = true;
    if (!resolvePokemonBerryPreservation(nullptr, 0, uninitialized, emptyPreserved) || emptyPreserved) return 10332;
    if (resolvePokemonBerryPreservation(pouchStacks, 1, uninitialized, emptyPreserved)) return 10333;
    NativeHeldModifierInstance inventory[2]{};
    if (initializeHeldBerry(0, actor.pokemonId, 2, true, inventory[0]) != HeldModifierStorageResult::Ok ||
        initializeHeldBerry(1, actor.pokemonId, 1, true, inventory[1]) != HeldModifierStorageResult::Ok) return 10321;
    size_t inventoryCount = 2;
    PokemonBerryConsumedEvent consumed{};
    if (!consumePokemonHeldBerry(inventory, 2, inventoryCount, 0, actor, true, false, nullptr, 0, true, consumed) ||
        inventoryCount != 2 || inventory[0].stackCount != 1 || !consumed.eaten || !consumed.harvestEligible) return 10322;
    if (!consumePokemonHeldBerry(inventory, 2, inventoryCount, 0, actor, true, true, nullptr, 0, true, consumed) ||
        inventoryCount != 2 || inventory[0].stackCount != 1 || !consumed.eaten || consumed.harvestEligible || consumed.consumed) return 10323;
    const auto unchanged = inventory[0];
    consumed.ownerPokemonId = 999;
    if (consumePokemonHeldBerry(inventory, 2, inventoryCount, 0, actor, false, false, nullptr, 0, true, consumed) ||
        std::memcmp(&unchanged, &inventory[0], sizeof(unchanged)) || consumed.ownerPokemonId != 999) return 10324;
    if (!consumePokemonHeldBerry(inventory, 2, inventoryCount, 0, actor, true, false, nullptr, 0, true, consumed) ||
        inventoryCount != 1 || !consumed.consumed || consumed.stacksAfter != 0) return 10325;
    uint16_t remainingType = 0;
    if (!heldBerryType(inventory[0], remainingType) || remainingType != 1) return 10326;
    // Real canonical Sitrus: no pouch consumes exactly one stack and emits
    // the deferred heal request. No phase is executed prematurely.
    if (initializeHeldBerry(0, actor.pokemonId, 2, true, inventory[0]) != HeldModifierStorageResult::Ok) return 10334;
    inventoryCount = 1;
    PokerogueBattleRng useBattle;
    if (!useBattle.initialize(seed, 5, 1) || !useBattle.beginTurn(1)) return 10335;
    PokemonHeldBerryUseEvent use{};
    const auto battleBefore = useBattle.state(), globalBefore = rng.state();
    if (preparePokemonHeldBerryUse(inventory, 2, inventoryCount, 0, actor, policy, true, false,
            nullptr, 0, nullptr, 0, true, useBattle, rng, use) != PokemonBerryEffectResult::Ready ||
        inventory[0].stackCount != 1 || use.effect.healingRequested != 25 || actor.hp != 20 ||
        !use.item.consumed || !sameRng(battleBefore, useBattle.state()) || !sameRng(globalBefore, rng.state())) return 10336;
    const auto blockedRecord = inventory[0];
    use.item.ownerPokemonId = 999;
    if (preparePokemonHeldBerryUse(inventory, 2, inventoryCount, 0, actor, policy, true, true,
            pouchStacks, 3, nullptr, 0, true, useBattle, rng, use) != PokemonBerryEffectResult::NoEffect ||
        std::memcmp(&blockedRecord, &inventory[0], sizeof(blockedRecord)) || use.item.ownerPokemonId != 999 ||
        !sameRng(battleBefore, useBattle.state()) || !sameRng(globalBefore, rng.state())) return 10337;
    const uint16_t unknownAbility = 65535;
    if (preparePokemonHeldBerryUse(inventory, 2, inventoryCount, 0, actor, policy, true, false,
            pouchStacks, 3, &unknownAbility, 1, true, useBattle, rng, use) != PokemonBerryEffectResult::UnresolvedPolicy ||
        std::memcmp(&blockedRecord, &inventory[0], sizeof(blockedRecord)) || use.item.ownerPokemonId != 999 ||
        !sameRng(battleBefore, useBattle.state()) || !sameRng(globalBefore, rng.state())) return 10338;
    PokemonBerryRecoveryPolicy recovery{};
    recovery.healing.resolved = true;
    recovery.statusReactionsResolved = true;
    PokemonBerryRecoveryEvent recovered{};
    auto healedActor = actor;
    if (applyPokemonBerryRecovery(healedActor, use.effect, recovery, recovered) != PokemonBerryEffectResult::Ready ||
        healedActor.hp != 45 || recovered.healing.healed != 25) return 10341;
    auto blockedHeal = actor;
    recovery.healing.healBlocked = true;
    if (applyPokemonBerryRecovery(blockedHeal, use.effect, recovery, recovered) != PokemonBerryEffectResult::Ready ||
        blockedHeal.hp != actor.hp || !recovered.healing.blocked) return 10342;
    recovery.healing.healBlocked = false;
    recovery.healing.healingMultiplier = 1.5;
    healedActor = actor;
    if (applyPokemonBerryRecovery(healedActor, use.effect, recovery, recovered) != PokemonBerryEffectResult::Ready ||
        healedActor.hp != 57) return 10343;
    PokemonBerryEffectPlan lum{}, leppa{};
    if (planPokemonBerryEffect(1, actor, policy, rng, lum) != PokemonBerryEffectResult::Ready ||
        planPokemonBerryEffect(10, actor, policy, rng, leppa) != PokemonBerryEffectResult::Ready) return 10344;
    auto curedActor = actor;
    curedActor.confusion.present = true;
    curedActor.confusion.turns = 2;
    if (applyPokemonBerryRecovery(curedActor, lum, recovery, recovered) != PokemonBerryEffectResult::Ready ||
        curedActor.status.present || curedActor.confusion.present || !recovered.status.lapseConfusion) return 10345;
    auto ppActor = actor;
    if (applyPokemonBerryRecovery(ppActor, leppa, recovery, recovered) != PokemonBerryEffectResult::Ready ||
        ppActor.moves[0].pp != 3 || ppActor.moves[1].pp != 10 || recovered.ppSlot != 1) return 10346;
    recovery.statusReactionsResolved = false;
    curedActor = actor;
    recovered.ppAfter = 222;
    if (applyPokemonBerryRecovery(curedActor, lum, recovery, recovered) != PokemonBerryEffectResult::UnresolvedPolicy ||
        !curedActor.status.present || recovered.ppAfter != 222) return 10347;
    if (initializeHeldBerry(10, actor.pokemonId, 2, true, inventory[0]) != HeldModifierStorageResult::Ok) return 10348;
    inventoryCount = 1;
    auto immediatePp = actor;
    if (preparePokemonHeldBerryUse(inventory, 2, inventoryCount, 0, immediatePp, policy, true, false,
            nullptr, 0, nullptr, 0, true, useBattle, rng, use) != PokemonBerryEffectResult::Ready ||
        !use.effectExecutedImmediately || immediatePp.moves[1].pp != 10 || inventory[0].stackCount != 1) return 10349;
    // A second Leppa sees restored PP immediately, and is no longer eligible.
    if (preparePokemonHeldBerryUse(inventory, 2, inventoryCount, 0, immediatePp, policy, true, false,
            nullptr, 0, nullptr, 0, true, useBattle, rng, use) != PokemonBerryEffectResult::NoEffect ||
        inventory[0].stackCount != 1 || immediatePp.moves[1].pp != 10) return 10350;
    PokemonBerryEffectPlan attackBerry{}, speedBerry{};
    if (planPokemonBerryEffect(3, actor, policy, rng, attackBerry) != PokemonBerryEffectResult::Ready ||
        planPokemonBerryEffect(7, actor, policy, rng, speedBerry) != PokemonBerryEffectResult::Ready) return 10351;
    PokemonBerryStatQueue statQueue{};
    if (!appendPokemonBerryStatRequest(statQueue, attackBerry) ||
        !appendPokemonBerryStatRequest(statQueue, speedBerry) ||
        !appendPokemonBerryStatRequest(statQueue, attackBerry) || statQueue.count != 2 ||
        statQueue.changes[0].stat != 1 || statQueue.changes[0].stages != 2 ||
        statQueue.changes[1].stat != 5 || statQueue.changes[1].stages != 1) return 10352;
    PokemonStatStageEffectPolicy statPolicy{};
    statPolicy.resolved = true;
    PokemonBerryStatPhaseEvent statEvent{};
    auto boosted = actor;
    boosted.statStages[0] = 5;
    if (applyPokemonBerryStatQueue(boosted, statQueue, statPolicy, statEvent) != PokemonStatStageEffectResult::Ok ||
        boosted.statStages[0] != 6 || boosted.statStages[4] != 1 || statEvent.count != 2 ||
        statEvent.changes[0].requestedStages != 2 || statEvent.changes[0].changes[0] != 1) return 10353;
    auto contrary = actor;
    statPolicy.stageMultiplier = -1;
    if (applyPokemonBerryStatQueue(contrary, statQueue, statPolicy, statEvent) != PokemonStatStageEffectResult::Ok ||
        contrary.statStages[0] != -2 || contrary.statStages[4] != -1) return 10354;
    auto invalidQueue = statQueue;
    invalidQueue.changes[1].stat = 8;
    boosted = actor;
    statEvent.count = 99;
    if (applyPokemonBerryStatQueue(boosted, invalidQueue, statPolicy, statEvent) != PokemonStatStageEffectResult::InvalidDefinition ||
        boosted.statStages[0] || statEvent.count != 99) return 10355;
    auto differentOwner = attackBerry;
    differentOwner.ownerPokemonId++;
    const auto unchangedQueue = statQueue;
    if (appendPokemonBerryStatRequest(statQueue, differentOwner) ||
        std::memcmp(&statQueue, &unchangedQueue, sizeof(statQueue))) return 10356;
    // Store summon critical stages alongside real actor identity, status and PP.
    FirstRunRuntime critGame(1);
    NativeRunSave critCheckpoint{};
    if (!captureActiveTestCheckpoint(critGame, critCheckpoint)) return 10357;
    const auto& critPokemon = critGame.presentation().player;
    auto critActor = critPokemon.battleState;
    critActor.berryCriticalBoostStages = 2;
    NativePokemonSave critSave{};
    if (!captureNativePokemonActorSave(critActor, critPokemon.actor, 0, critSave)) return 10358;
    char critBytes[1024]{};
    size_t critLength = 0;
    NativePokemonSave critDecoded{};
    if (encodeNativePokemonSave(critSave, critBytes, sizeof(critBytes), critLength) != NativeSaveResult::Ok ||
        decodeNativePokemonSave(critBytes, critLength, critDecoded) != NativeSaveResult::Ok ||
        critDecoded.berryCriticalBoostStages != 2) return 10359;
    PokemonBattleState critRestored{};
    PokemonActorIdentity critIdentity{};
    if (!restoreNativePokemonActorSave(critDecoded, critRestored, critIdentity) ||
        critRestored.berryCriticalBoostStages != 2) return 10360;
    critSave.berryCriticalBoostStages = 3;
    if (encodeNativePokemonSave(critSave, critBytes, sizeof(critBytes), critLength) != NativeSaveResult::InvalidRecord) return 10361;
    critSave.berryCriticalBoostStages = 0;
    if (encodeNativePokemonSave(critSave, critBytes, sizeof(critBytes), critLength) != NativeSaveResult::Ok ||
        decodeNativePokemonSave(critBytes, critLength, critDecoded) != NativeSaveResult::Ok ||
        critDecoded.berryCriticalBoostStages) return 10362;
    critCheckpoint.playerBerryCriticalBoostStages = 2;
    critCheckpoint.enemyBerryCriticalBoostStages = 1;
    if (critCheckpoint.playerPartyCount)
        critCheckpoint.playerParty[critCheckpoint.activePlayerMember].berryCriticalBoostStages = 2;
    char critRunBytes[kNativeSaveMaxBytes]{};
    size_t critRunLength = 0;
    NativeRunSave critRunDecoded{};
    if (encodeNativeRunSave(critCheckpoint, critRunBytes, sizeof(critRunBytes), critRunLength) != NativeSaveResult::Ok ||
        decodeNativeRunSave(critRunBytes, critRunLength, PokerogueContent::kContentHash, critRunDecoded) != NativeSaveResult::Ok ||
        critRunDecoded.playerBerryCriticalBoostStages != 2 || critRunDecoded.enemyBerryCriticalBoostStages != 1) return 10363;
    FirstRunRuntime critRestoredGame(2);
    if (!critRestoredGame.restoreNativeRunSave(critRunDecoded) ||
        critRestoredGame.presentation().player.battleState.berryCriticalBoostStages != 2 ||
        critRestoredGame.presentation().enemy.battleState.berryCriticalBoostStages != 1) return 10364;
    NativeRunSave recapturedCrit{};
    if (critRestoredGame.captureNativeRunSave(recapturedCrit) != NativeSaveResult::Ok ||
        recapturedCrit.playerBerryCriticalBoostStages != 2 || recapturedCrit.enemyBerryCriticalBoostStages != 1) return 10365;
    auto recalledCrit = critRestored;
    resetPokemonSummonState(recalledCrit);
    if (recalledCrit.berryCriticalBoostStages) return 10366;
    if (initializeHeldBerry(8, actor.pokemonId, 2, true, inventory[0]) != HeldModifierStorageResult::Ok) return 10380;
    inventoryCount = 1;
    auto lansatActor = actor;
    if (preparePokemonHeldBerryUse(inventory, 2, inventoryCount, 0, lansatActor, policy, true, false,
            nullptr, 0, nullptr, 0, true, useBattle, rng, use) != PokemonBerryEffectResult::Ready ||
        !use.effectExecutedImmediately || lansatActor.berryCriticalBoostStages != 2 ||
        inventory[0].stackCount != 1 || !PokerogueContent::kBerryCriticalTagSource.sourceHash[0]) return 10381;
    if (preparePokemonHeldBerryUse(inventory, 2, inventoryCount, 0, lansatActor, policy, true, false,
            nullptr, 0, nullptr, 0, true, useBattle, rng, use) != PokemonBerryEffectResult::NoEffect ||
        inventory[0].stackCount != 1) return 10382;
    resetPokemonSummonState(lansatActor);
    if (lansatActor.berryCriticalBoostStages) return 10383;
    if (!actor.hasEatenBerry) return 10390;
    auto switchedHistory = actor;
    resetPokemonSummonState(switchedHistory);
    if (!switchedHistory.hasEatenBerry) return 10391;
    critActor.berryCriticalBoostStages = 0;
    critActor.hasEatenBerry = true;
    if (!captureNativePokemonActorSave(critActor, critPokemon.actor, 0, critSave) ||
        encodeNativePokemonSave(critSave, critBytes, sizeof(critBytes), critLength) != NativeSaveResult::Ok ||
        decodeNativePokemonSave(critBytes, critLength, critDecoded) != NativeSaveResult::Ok ||
        !critDecoded.hasEatenBerry || critDecoded.berryCriticalBoostStages ||
        !restoreNativePokemonActorSave(critDecoded, critRestored, critIdentity) || !critRestored.hasEatenBerry) return 10392;
    critActor.berryCriticalBoostStages = 2;
    if (!captureNativePokemonActorSave(critActor, critPokemon.actor, 0, critSave) ||
        encodeNativePokemonSave(critSave, critBytes, sizeof(critBytes), critLength) != NativeSaveResult::Ok ||
        decodeNativePokemonSave(critBytes, critLength, critDecoded) != NativeSaveResult::Ok ||
        !critDecoded.hasEatenBerry || critDecoded.berryCriticalBoostStages != 2) return 10393;
    uint16_t consumedHistory[4]{}, turnHistory[4]{}, previousTurn[4]{};
    PokemonBerryHistoryView history{actor.pokemonId, {consumedHistory,0,4}, {turnHistory,0,4}, {previousTurn,0,4}};
    PokemonBerryConsumedEvent eaten{};
    eaten.ownerPokemonId = actor.pokemonId;
    eaten.eaten = true;
    eaten.stacksBefore = 2;
    eaten.stacksAfter = 1;
    eaten.consumed = eaten.harvestEligible = true;
    for (uint16_t type : {uint16_t(0),uint16_t(1),uint16_t(0)}) {
        eaten.berryType = type;
        if (recordPokemonBerryHistory(history, eaten) != PokemonBerryHistoryResult::Recorded) return 10401;
    }
    eaten.berryType = 10;
    eaten.consumed = eaten.harvestEligible = false;
    eaten.stacksAfter = 2;
    if (recordPokemonBerryHistory(history, eaten) != PokemonBerryHistoryResult::Recorded ||
        history.battleConsumed.count != 3 || history.turnEaten.count != 4 || consumedHistory[2] != 0 || turnHistory[3] != 10)
        return 10402;
    if (recordPokemonBerryTurnEnd(history, true, true) != PokemonBerryHistoryResult::Recorded ||
        history.lastTurnEaten.count != 4 || std::memcmp(turnHistory, previousTurn, sizeof(turnHistory))) return 10403;
    if (recordPokemonBerryHistory(history, eaten) != PokemonBerryHistoryResult::CapacityExceeded ||
        history.battleConsumed.count != 3 || history.turnEaten.count != 4) return 10404;
    resetPokemonBerryTurnHistory(history);
    if (history.turnEaten.count || history.lastTurnEaten.count != 4 || history.battleConsumed.count != 3) return 10405;
    if (recordPokemonBerryTurnEnd(history, true, true) != PokemonBerryHistoryResult::Recorded ||
        history.lastTurnEaten.count) return 10406;
    if (recordPokemonBerryHistory(history, eaten) != PokemonBerryHistoryResult::Recorded) return 10407;
    resetPokemonBerrySummonHistory(history);
    if (history.turnEaten.count || history.lastTurnEaten.count || history.battleConsumed.count != 3 || !actor.hasEatenBerry)
        return 10408;
    auto battleReset = actor;
    resetPokemonBerryArenaTransitionHistory(history, battleReset);
    if (history.battleConsumed.count || battleReset.hasEatenBerry) return 10409;
    if (initializeHeldBerry(0, actor.pokemonId, 1, true, inventory[0]) != HeldModifierStorageResult::Ok) return 10410;
    inventoryCount = 1;
    auto recordedActor = actor;
    if (usePokemonHeldBerryAndRecord(inventory, 2, inventoryCount, 0, recordedActor, history, policy, true, false,
            nullptr, 0, nullptr, 0, true, useBattle, rng, use) != PokemonBerryEffectResult::Ready ||
        inventoryCount || history.battleConsumed.count != 1 || history.turnEaten.count != 1 ||
        history.battleConsumed.values[0] != 0 || !recordedActor.hasEatenBerry) return 10411;
    if (initializeHeldBerry(0, actor.pokemonId, 1, true, inventory[0]) != HeldModifierStorageResult::Ok) return 10412;
    inventoryCount = 1;
    auto exhausted = history;
    exhausted.turnEaten.capacity = exhausted.turnEaten.count;
    const auto recordBefore = inventory[0];
    const auto rngBefore = rng.state(), battleRngBefore = useBattle.state();
    use.item.ownerPokemonId = 999;
    if (usePokemonHeldBerryAndRecord(inventory, 2, inventoryCount, 0, recordedActor, exhausted, policy, true, false,
            pouchStacks, 3, nullptr, 0, true, useBattle, rng, use) != PokemonBerryEffectResult::HistoryCapacityExceeded ||
        inventoryCount != 1 || std::memcmp(&recordBefore, &inventory[0], sizeof(recordBefore)) ||
        !sameRng(rngBefore, rng.state()) || !sameRng(battleRngBefore, useBattle.state()) ||
        use.item.ownerPokemonId != 999 || history.battleConsumed.count != 1) return 10413;
    // Simulate a legacy checkpoint whose scalar flag survives without ordered history.
    critCheckpoint.berryHistories.resolved = false;
    critCheckpoint.playerHasEatenBerry = true;
    critCheckpoint.enemyHasEatenBerry = true;
    if (critCheckpoint.playerPartyCount)
        critCheckpoint.playerParty[critCheckpoint.activePlayerMember].hasEatenBerry = true;
    if (encodeNativeRunSave(critCheckpoint, critRunBytes, sizeof(critRunBytes), critRunLength) != NativeSaveResult::Ok ||
        decodeNativeRunSave(critRunBytes, critRunLength, PokerogueContent::kContentHash, critRunDecoded) != NativeSaveResult::Ok ||
        !critRestoredGame.restoreNativeRunSave(critRunDecoded) ||
        !critRestoredGame.presentation().player.battleState.hasEatenBerry ||
        !critRestoredGame.presentation().enemy.battleState.hasEatenBerry ||
        critRestoredGame.captureNativeRunSave(recapturedCrit) != NativeSaveResult::Ok ||
        !recapturedCrit.playerHasEatenBerry || !recapturedCrit.enemyHasEatenBerry) return 10420;
    auto inconsistentHistory = critCheckpoint;
    if (inconsistentHistory.playerPartyCount) {
        inconsistentHistory.playerParty[inconsistentHistory.activePlayerMember].hasEatenBerry = false;
        if (validateNativeRunSave(inconsistentHistory, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10421;
    }
    char historyBytes[512]{};
    size_t historyLength = 0;
    if (encodeNativeBerryHistory(history, historyBytes, sizeof(historyBytes), historyLength) != NativeSaveResult::Ok) return 10440;
    uint16_t restoredConsumed[4]{}, restoredTurn[4]{}, restoredLast[4]{};
    PokemonBerryHistoryView restoredHistory{999, {restoredConsumed,0,4}, {restoredTurn,0,4}, {restoredLast,0,4}};
    if (decodeNativeBerryHistory(historyBytes, historyLength, restoredHistory) != NativeSaveResult::Ok ||
        restoredHistory.ownerPokemonId != history.ownerPokemonId || restoredHistory.battleConsumed.count != history.battleConsumed.count ||
        restoredHistory.turnEaten.count != history.turnEaten.count || restoredHistory.lastTurnEaten.count != history.lastTurnEaten.count ||
        std::memcmp(restoredConsumed, consumedHistory, history.battleConsumed.count * sizeof(uint16_t))) return 10441;
    char repeatHistory[512]{};
    size_t repeatLength = 0;
    if (encodeNativeBerryHistory(restoredHistory, repeatHistory, sizeof(repeatHistory), repeatLength) != NativeSaveResult::Ok ||
        repeatLength != historyLength || std::memcmp(repeatHistory, historyBytes, historyLength)) return 10442;
    const auto preservedOwner = restoredHistory.ownerPokemonId;
    const auto preservedCount = restoredHistory.battleConsumed.count;
    const auto preservedFirst = restoredConsumed[0];
    if (decodeNativeBerryHistory(historyBytes, historyLength - 1, restoredHistory) != NativeSaveResult::InvalidFormat ||
        restoredHistory.ownerPokemonId != preservedOwner || restoredHistory.battleConsumed.count != preservedCount ||
        restoredConsumed[0] != preservedFirst) return 10443;
    auto tooSmallHistory = restoredHistory;
    tooSmallHistory.battleConsumed.capacity = 0;
    if (decodeNativeBerryHistory(historyBytes, historyLength, tooSmallHistory) != NativeSaveResult::TooLarge ||
        tooSmallHistory.ownerPokemonId != preservedOwner || restoredConsumed[0] != preservedFirst) return 10444;
    // Actor-bound restore rejects stale/foreign history before any buffer publication.
    auto historyActor = actor;
    historyActor.pokemonId = history.ownerPokemonId;
    historyActor.hasEatenBerry = true;
    if (restoreNativePokemonBerryHistory(historyBytes, historyLength, historyActor, restoredHistory) != NativeSaveResult::Ok)
        return 10445;
    auto foreignActor = historyActor;
    foreignActor.pokemonId ^= 1;
    if (restoreNativePokemonBerryHistory(historyBytes, historyLength, foreignActor, restoredHistory) != NativeSaveResult::InvalidRecord ||
        restoredHistory.ownerPokemonId != preservedOwner || restoredConsumed[0] != preservedFirst) return 10446;
    historyActor.hasEatenBerry = false;
    if (restoreNativePokemonBerryHistory(historyBytes, historyLength, historyActor, restoredHistory) != NativeSaveResult::InvalidRecord ||
        restoredHistory.battleConsumed.count != preservedCount || restoredConsumed[0] != preservedFirst) return 10447;
    // Preserved berries can leave the flag true while all lists are empty after recall.
    PokemonBerryHistoryView emptyHistory{history.ownerPokemonId};
    char emptyBytes[128]{};
    size_t emptyLength = 0;
    historyActor.hasEatenBerry = true;
    if (encodeNativeBerryHistory(emptyHistory, emptyBytes, sizeof(emptyBytes), emptyLength) != NativeSaveResult::Ok ||
        restoreNativePokemonBerryHistory(emptyBytes, emptyLength, historyActor, emptyHistory) != NativeSaveResult::Ok)
        return 10448;
    // Decode must reject overlapping output ranges and input/output aliasing atomically.
    uint16_t separate[4]{};
    PokemonBerryHistoryView populated{history.ownerPokemonId, {consumedHistory,3,4}, {turnHistory,4,4}, {previousTurn,4,4}};
    if (encodeNativeBerryHistory(populated, repeatHistory, sizeof(repeatHistory), repeatLength) != NativeSaveResult::Ok)
        return 10449;
    PokemonBerryHistoryView aliased{history.ownerPokemonId, {separate,0,4}, {separate,0,4}, {restoredLast,0,4}};
    if (decodeNativeBerryHistory(repeatHistory, repeatLength, aliased) != NativeSaveResult::InvalidRecord ||
        separate[0] || aliased.battleConsumed.count || aliased.turnEaten.count) return 10450;
    auto inputAlias = aliased;
    inputAlias.turnEaten.values = restoredTurn;
    inputAlias.battleConsumed.values = reinterpret_cast<uint16_t*>(repeatHistory);
    char sourceBefore[512]{};
    std::memcpy(sourceBefore, repeatHistory, repeatLength);
    if (decodeNativeBerryHistory(repeatHistory, repeatLength, inputAlias) != NativeSaveResult::InvalidRecord ||
        std::memcmp(sourceBefore, repeatHistory, repeatLength)) return 10451;
    // Ordered histories cross the complete run envelope and live restore/capture.
    auto withHistories = critCheckpoint;
    withHistories.berryHistories.resolved = true;
    const uint8_t playerSlot = withHistories.playerPartyCount ? withHistories.activePlayerMember : 12;
    const uint32_t playerId = critRestoredGame.presentation().player.battleState.pokemonId;
    auto storedView = populated;
    storedView.ownerPokemonId = playerId;
    if (captureNativeBerryHistory(withHistories.berryHistories, playerSlot, storedView) != NativeSaveResult::Ok ||
        withHistories.berryHistories.recordCount != 1 || withHistories.berryHistories.valueCount != 11)
        return 10510;
    PokemonBerryHistoryView emptyEnemyHistory{critRestoredGame.presentation().enemy.battleState.pokemonId};
    if (captureNativeBerryHistory(withHistories.berryHistories, 13, emptyEnemyHistory) != NativeSaveResult::Ok) return 10522;
    auto missingHistory = withHistories;
    if (retainNativeBerryHistoryActors(missingHistory.berryHistories, 1U << playerSlot) != NativeSaveResult::Ok ||
        validateNativeRunSave(missingHistory, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10523;
    auto unknownHistory = withHistories.berryHistories;
    unknownHistory = {};
    if (captureNativeBerryHistory(unknownHistory, playerSlot, storedView) != NativeSaveResult::InvalidRecord ||
        unknownHistory.resolved || unknownHistory.recordCount) return 10524;
    if (encodeNativeRunSave(withHistories, critRunBytes, sizeof(critRunBytes), critRunLength) != NativeSaveResult::Ok ||
        decodeNativeRunSave(critRunBytes, critRunLength, PokerogueContent::kContentHash, critRunDecoded) != NativeSaveResult::Ok ||
        !critRestoredGame.restoreNativeRunSave(critRunDecoded) ||
        critRestoredGame.captureNativeRunSave(recapturedCrit) != NativeSaveResult::Ok ||
        recapturedCrit.berryHistories.recordCount != 2 || recapturedCrit.berryHistories.valueCount != 11 ||
        std::memcmp(withHistories.berryHistories.values, recapturedCrit.berryHistories.values, 11 * sizeof(uint16_t)))
        return 10511;
    PokemonBerryHistoryView pooledView{};
    if (!nativeBerryHistoryView(recapturedCrit.berryHistories, 0, pooledView) ||
        pooledView.battleConsumed.count != 3 || pooledView.turnEaten.count != 4 || pooledView.lastTurnEaten.count != 4 ||
        pooledView.ownerPokemonId != playerId) return 10512;
    auto invalidHistories = withHistories;
    invalidHistories.berryHistories.records[0].actorSlot = 15;
    if (validateNativeRunSave(invalidHistories, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10513;
    invalidHistories = withHistories;
    invalidHistories.berryHistories.records[0].ownerPokemonId ^= 1;
    if (playerSlot != 12 && validateNativeRunSave(invalidHistories, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord)
        return 10514;
    if (critRestoredGame.restoreNativeRunSave(invalidHistories) ||
        critRestoredGame.presentation().player.battleState.pokemonId != playerId) return 10515;
    invalidHistories = withHistories;
    invalidHistories.berryHistories.resolved = false;
    if (validateNativeRunSave(invalidHistories, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10516;
    auto storedPool = withHistories.berryHistories;
    auto foreignView = storedView;
    foreignView.ownerPokemonId ^= 1;
    if (captureNativeBerryHistory(storedPool, playerSlot, foreignView) != NativeSaveResult::InvalidRecord ||
        std::memcmp(&storedPool, &withHistories.berryHistories, sizeof(storedPool))) return 10517;
    uint16_t manyBerries[kNativeBerryHistoryValues]{};
    PokemonBerryHistoryView hugeView{playerId, {manyBerries,kNativeBerryHistoryValues,kNativeBerryHistoryValues}};
    if (captureNativeBerryHistory(storedPool, playerSlot, hugeView) != NativeSaveResult::Ok) return 10518;
    invalidHistories = withHistories;
    invalidHistories.berryHistories = storedPool;
    size_t tooLargeLength = 999;
    if (encodeNativeRunSave(invalidHistories, critRunBytes, sizeof(critRunBytes), tooLargeLength) != NativeSaveResult::TooLarge ||
        tooLargeLength) return 10519;
    auto enemyView = storedView;
    enemyView.ownerPokemonId = critRestoredGame.presentation().enemy.battleState.pokemonId;
    if (captureNativeBerryHistory(storedPool, 13, enemyView) != NativeSaveResult::TooLarge || storedPool.recordCount != 2)
        return 10520;
    auto recalledHistory = withHistories.berryHistories;
    if (resetNativeBerrySummonHistory(recalledHistory, playerId) != NativeSaveResult::Ok ||
        recalledHistory.valueCount != 3 || recalledHistory.records[0].counts[0] != 3 ||
        recalledHistory.records[0].counts[1] || recalledHistory.records[0].counts[2] ||
        recalledHistory.records[1].offset != 3 ||
        std::memcmp(recalledHistory.values, withHistories.berryHistories.values, 3 * sizeof(uint16_t))) return 10525;
    NativeBerryHistoryStore unknownRecall{};
    if (resetNativeBerrySummonHistory(unknownRecall, playerId) != NativeSaveResult::Ok || unknownRecall.resolved)
        return 10526;
    auto capturedHistory = withHistories.berryHistories;
    const uint32_t capturedId = capturedHistory.records[1].ownerPokemonId;
    const uint8_t captureSlot = playerSlot == 5 ? 4 : 5;
    if (transferNativeBerryHistoryToParty(capturedHistory, capturedId, captureSlot) != NativeSaveResult::Ok ||
        capturedHistory.records[1].actorSlot != captureSlot || capturedHistory.valueCount != 11 ||
        removeNativeBerryHistoryOwner(capturedHistory, capturedId) != NativeSaveResult::Ok ||
        capturedHistory.recordCount != 1 || capturedHistory.valueCount != 11) return 10527;
    auto retained = withHistories.berryHistories;
    if (retainNativeBerryHistoryActors(retained, 1U << playerSlot) != NativeSaveResult::Ok || retained.valueCount != 11 ||
        retainNativeBerryHistoryActors(retained, 0) != NativeSaveResult::Ok || retained.recordCount || retained.valueCount || !retained.resolved)
        return 10521;
    {
        // Lum resets status during BerryModifier.apply; a following Lum sees
        // the cure immediately and must neither consume nor touch history/RNG.
        auto lumActor = actor;
        lumActor.hp = 20;
        lumActor.status = {};
        lumActor.status.present = true;
        lumActor.status.effect = PokemonStatusEffect::Poison;
        lumActor.confusion = {};
        NativeHeldModifierInstance lumInventory[2]{};
        uint16_t lumType = 65535;
        for (const auto& profile : PokerogueContent::kBerryEffectProfiles)
            if (!std::strcmp(profile.effect, "CURE_STATUS")) lumType = profile.id;
        if (initializeHeldBerry(lumType, lumActor.pokemonId, 2, true, lumInventory[0]) != HeldModifierStorageResult::Ok)
            return 10701;
        size_t lumCount = 1;
        const auto beforeLum = lumInventory[0];
        PokemonHeldBerryUseEvent lumEvent{};
        lumEvent.item.ownerPokemonId = 999;
        if (preparePokemonHeldBerryUse(lumInventory, 2, lumCount, 0, lumActor, policy, true, false,
                nullptr, 0, nullptr, 0, true, useBattle, rng, lumEvent) != PokemonBerryEffectResult::UnresolvedPolicy ||
            !lumActor.status.present || std::memcmp(&beforeLum, &lumInventory[0], sizeof(beforeLum)) ||
            lumEvent.item.ownerPokemonId != 999) return 10702;
        PokemonBerryRecoveryPolicy statusPolicy{};
        statusPolicy.statusReactionsResolved = true;
        uint16_t lumConsumed[2]{}, lumTurn[2]{};
        PokemonBerryHistoryView lumHistory{lumActor.pokemonId, {lumConsumed,0,2}, {lumTurn,0,2}};
        if (usePokemonHeldBerryAndRecord(lumInventory, 2, lumCount, 0, lumActor, lumHistory, policy, true, false,
                nullptr, 0, nullptr, 0, true, useBattle, rng, lumEvent, &statusPolicy) != PokemonBerryEffectResult::Ready ||
            lumActor.status.present || !lumEvent.effectExecutedImmediately || lumInventory[0].stackCount != 1 ||
            lumHistory.battleConsumed.count != 1 || lumHistory.turnEaten.count != 1) return 10703;
        const auto lumGlobalState = rng.state(), lumBattleState = useBattle.state();
        lumEvent.item.ownerPokemonId = 999;
        if (usePokemonHeldBerryAndRecord(lumInventory, 2, lumCount, 0, lumActor, lumHistory, policy, true, false,
                nullptr, 0, nullptr, 0, true, useBattle, rng, lumEvent, &statusPolicy) != PokemonBerryEffectResult::NoEffect ||
            lumInventory[0].stackCount != 1 || lumHistory.battleConsumed.count != 1 || lumHistory.turnEaten.count != 1 ||
            lumEvent.item.ownerPokemonId != 999 || !sameRng(lumGlobalState, rng.state()) || !sameRng(lumBattleState, useBattle.state()))
            return 10704;
    }
    {
        auto queuedActor = actor;
        queuedActor.hp = 20;
        NativeHeldModifierInstance queuedInventory[2]{};
        uint16_t healingType = 65535;
        for (const auto& profile : PokerogueContent::kBerryEffectProfiles)
            if (!std::strcmp(profile.effect, "HEAL") && !std::strcmp(profile.predicate, "LOW_HP")) healingType = profile.id;
        if (initializeHeldBerry(healingType, queuedActor.pokemonId, 2, true, queuedInventory[0]) != HeldModifierStorageResult::Ok)
            return 10710;
        size_t queuedCount = 1;
        uint16_t queuedConsumed[2]{}, queuedTurn[2]{};
        PokemonBerryHistoryView queuedHistory{queuedActor.pokemonId, {queuedConsumed,0,2}, {queuedTurn,0,2}};
        PokemonBerryPhaseRequests requests{};
        requests.ownerPokemonId = queuedActor.pokemonId;
        PokemonHeldBerryUseEvent queuedEvent{};
        queuedEvent.item.ownerPokemonId = 999;
        const auto queueRecordBefore = queuedInventory[0];
        const auto queueGlobalBefore = rng.state(), queueBattleBefore = useBattle.state();
        if (usePokemonHeldBerryAndRecord(queuedInventory, 2, queuedCount, 0, queuedActor, queuedHistory,
                policy, true, false, nullptr, 0, nullptr, 0, true, useBattle, rng, queuedEvent, nullptr, &requests) !=
                PokemonBerryEffectResult::PhaseCapacityExceeded || queuedHistory.battleConsumed.count ||
            queuedHistory.turnEaten.count || queuedActor.hp != 20 || queuedEvent.item.ownerPokemonId != 999 ||
            std::memcmp(&queueRecordBefore, &queuedInventory[0], sizeof(queueRecordBefore)) ||
            !sameRng(queueGlobalBefore, rng.state()) || !sameRng(queueBattleBefore, useBattle.state())) return 10711;
        PokemonBerryEffectPlan healRequests[2]{};
        requests.recoveryPlans = healRequests;
        requests.recoveryCapacity = 2;
        if (usePokemonHeldBerryAndRecord(queuedInventory, 2, queuedCount, 0, queuedActor, queuedHistory,
                policy, true, false, nullptr, 0, nullptr, 0, true, useBattle, rng, queuedEvent, nullptr, &requests) !=
                PokemonBerryEffectResult::Ready || requests.recoveryCount != 1 || queuedActor.hp != 20 ||
            queuedInventory[0].stackCount != 1 || queuedHistory.battleConsumed.count != 1 ||
            healRequests[0].healingRequested != 25 || healRequests[0].ownerPokemonId != queuedActor.pokemonId) return 10712;
        PokemonBerryRecoveryPolicy queuedRecovery{};
        queuedRecovery.healing.resolved = true;
        PokemonBerryRecoveryEvent queuedHeal{};
        if (applyPokemonBerryRecovery(queuedActor, healRequests[0], queuedRecovery, queuedHeal) != PokemonBerryEffectResult::Ready ||
            queuedActor.hp != 45) return 10713;
    }
    {
        auto scanActor = actor;
        scanActor.hp = 20;
        scanActor.status = {};
        scanActor.status.present = true;
        scanActor.status.effect = PokemonStatusEffect::Poison;
        scanActor.confusion = {};
        uint16_t healType = 65535, lumType = 65535;
        for (const auto& profile : PokerogueContent::kBerryEffectProfiles) {
            if (!std::strcmp(profile.effect, "HEAL") && !std::strcmp(profile.predicate, "LOW_HP")) healType = profile.id;
            if (!std::strcmp(profile.effect, "CURE_STATUS")) lumType = profile.id;
        }
        NativeHeldModifierInstance scanInventory[2]{};
        if (initializeHeldBerry(healType, scanActor.pokemonId, 2, true, scanInventory[0]) != HeldModifierStorageResult::Ok ||
            initializeHeldBerry(lumType, scanActor.pokemonId, 2, true, scanInventory[1]) != HeldModifierStorageResult::Ok)
            return 10801;
        size_t scanCount = 2;
        uint16_t scanConsumed[4]{}, scanTurn[4]{};
        PokemonBerryHistoryView scanHistory{scanActor.pokemonId, {scanConsumed,0,4}, {scanTurn,0,4}};
        PokemonBerryEffectPlan scanHealPlans[4]{};
        PokemonBerryPhaseRequests scanRequests{scanActor.pokemonId, scanHealPlans, 0, 4};
        PokemonBerryRecoveryPolicy scanRecovery{};
        scanRecovery.statusReactionsResolved = true;
        scanRecovery.healing.resolved = true;
        PokemonBerryModifierScanEvent scanEvent{};
        if (scanPokemonHeldBerryModifiersInPlace(scanInventory, 2, scanCount, scanActor, scanHistory, scanRequests,
                policy, nullptr, 0, true, true, false, nullptr, 0, scanRecovery, useBattle, rng, scanEvent) !=
                PokemonBerryEffectResult::Ready || scanEvent.used != 2 || scanCount != 2 || scanActor.hp != 20 ||
            scanActor.status.present || scanRequests.recoveryCount != 1 || scanInventory[0].stackCount != 1 ||
            scanInventory[1].stackCount != 1 || scanHistory.battleConsumed.count != 2 ||
            scanConsumed[0] != healType || scanConsumed[1] != lumType) return 10802;
        PokemonBerryRecoveryEvent recovered{};
        if (applyPokemonBerryRecovery(scanActor, scanHealPlans[0], scanRecovery, recovered) != PokemonBerryEffectResult::Ready ||
            scanActor.hp != 45) return 10803;
        scanRequests.recoveryCount = 0;
        if (scanPokemonHeldBerryModifiersInPlace(scanInventory, 2, scanCount, scanActor, scanHistory, scanRequests,
                policy, nullptr, 0, true, true, false, nullptr, 0, scanRecovery, useBattle, rng, scanEvent) !=
                PokemonBerryEffectResult::Ready || scanEvent.used != 1 || scanCount != 1 || scanActor.hp != 45 ||
            scanRequests.recoveryCount != 1 || scanHistory.battleConsumed.count != 3 || scanConsumed[2] != healType)
            return 10804;
        if (applyPokemonBerryRecovery(scanActor, scanHealPlans[0], scanRecovery, recovered) != PokemonBerryEffectResult::Ready ||
            scanActor.hp != 70) return 10805;
        scanRequests.recoveryCount = 0;
        if (scanPokemonHeldBerryModifiersInPlace(scanInventory, 2, scanCount, scanActor, scanHistory, scanRequests,
                policy, nullptr, 0, true, false, false, nullptr, 0, scanRecovery, useBattle, rng, scanEvent) !=
                PokemonBerryEffectResult::NoEffect || scanEvent.used || scanCount != 1 || scanRequests.recoveryCount)
            return 10806;
        // The complete modifier scan queues Cheek Pouch once after two uses.
        uint16_t cheekId = 65535;
        for (const auto& row : PokerogueContent::kBerryAbilityProfiles)
            if (row.resolved && row.healFraction == 1.0 / 3) cheekId = row.abilityId;
        scanActor.hp = 20;
        scanActor.abilityId = cheekId;
        scanActor.status.present = true;
        scanActor.status.effect = PokemonStatusEffect::Poison;
        if (initializeHeldBerry(healType, scanActor.pokemonId, 2, true, scanInventory[0]) != HeldModifierStorageResult::Ok ||
            initializeHeldBerry(lumType, scanActor.pokemonId, 2, true, scanInventory[1]) != HeldModifierStorageResult::Ok) return 10911;
        scanCount = 2;
        scanHistory.battleConsumed.count = scanHistory.turnEaten.count = 0;
        PokemonBerryAbilityHealPlan queuedCheek[2]{};
        scanRequests.abilityHealPlans = queuedCheek;
        scanRequests.abilityHealCapacity = 2;
        if (scanPokemonHeldBerryModifiersInPlace(scanInventory, 2, scanCount, scanActor, scanHistory, scanRequests,
                policy, &cheekId, 1, true, true, false, nullptr, 0, scanRecovery, useBattle, rng, scanEvent) !=
                PokemonBerryEffectResult::Ready || scanEvent.used != 2 || scanRequests.abilityHealCount != 1 ||
            scanRequests.recoveryCount != 1 || queuedCheek[0].healingRequested != 33 || scanActor.hp != 20 ||
            scanActor.status.present) return 10912;
        if (applyPokemonBerryRecovery(scanActor, scanHealPlans[0], scanRecovery, recovered) != PokemonBerryEffectResult::Ready ||
            applyPokemonBerryAbilityHealing(scanActor, queuedCheek[0], scanRecovery.healing, recovered.healing) !=
                PokemonBerryEffectResult::Ready || scanActor.hp != 78) return 10913;
    }
    {
        uint16_t cheekPouchId = 65535, ripenId = 65535;
        for (const auto& row : PokerogueContent::kBerryAbilityProfiles) {
            if (row.resolved && row.healFraction == 1.0 / 3) cheekPouchId = row.abilityId;
            if (row.resolved && row.effectMultiplier == 2) ripenId = row.abilityId;
        }
        if (cheekPouchId == 65535 || ripenId == 65535) return 10901;
        auto cheekActor = actor;
        cheekActor.hp = 20;
        PokemonBerryPhaseRequests cheekRequests{};
        cheekRequests.ownerPokemonId = cheekActor.pokemonId;
        PokemonBerryAbilityHealPlan cheekPlans[2]{};
        const uint16_t cheekAbilities[] = {cheekPouchId, ripenId};
        if (queuePokemonBerryUseAbilityHealing(cheekActor, cheekAbilities, 2, true, true, cheekRequests) !=
                PokemonBerryEffectResult::PhaseCapacityExceeded || cheekRequests.abilityHealCount || cheekActor.hp != 20)
            return 10902;
        cheekRequests.abilityHealPlans = cheekPlans;
        cheekRequests.abilityHealCapacity = 2;
        if (queuePokemonBerryUseAbilityHealing(cheekActor, cheekAbilities, 2, true, true, cheekRequests) !=
                PokemonBerryEffectResult::Ready || cheekRequests.abilityHealCount != 1 || cheekActor.hp != 20 ||
            cheekPlans[0].healingRequested != 33 || cheekPlans[0].abilityId != cheekPouchId) return 10903;
        if (queuePokemonBerryUseAbilityHealing(cheekActor, cheekAbilities, 2, true, true, cheekRequests) !=
                PokemonBerryEffectResult::InvalidState || cheekRequests.abilityHealCount != 1) return 10904;
        PokemonHealingPolicy cheekHealing{};
        cheekHealing.resolved = true;
        cheekHealing.healingMultiplier = 1.5;
        PokemonHealingEvent cheekEvent{};
        if (applyPokemonBerryAbilityHealing(cheekActor, cheekPlans[0], cheekHealing, cheekEvent) !=
                PokemonBerryEffectResult::Ready || cheekActor.hp != 69 || cheekEvent.healed != 49) return 10905;
        cheekActor.hp = 100;
        cheekHealing.healBlocked = true;
        if (applyPokemonBerryAbilityHealing(cheekActor, cheekPlans[0], cheekHealing, cheekEvent) !=
                PokemonBerryEffectResult::Ready || !cheekEvent.blocked || cheekEvent.failedFullHp || cheekActor.hp != 100)
            return 10906;
        PokemonBerryEffectPlan fullBlockedBerry{};
        for (const auto& row : PokerogueContent::kBerryEffectProfiles)
            if (!std::strcmp(row.effect, "HEAL")) fullBlockedBerry.berryType = row.id;
        fullBlockedBerry.ownerPokemonId = cheekActor.pokemonId;
        fullBlockedBerry.healingRequested = 25;
        PokemonBerryRecoveryPolicy fullBlockedPolicy{};
        fullBlockedPolicy.healing = cheekHealing;
        PokemonBerryRecoveryEvent fullBlockedEvent{};
        if (applyPokemonBerryRecovery(cheekActor, fullBlockedBerry, fullBlockedPolicy, fullBlockedEvent) !=
                PokemonBerryEffectResult::Ready || !fullBlockedEvent.healing.blocked || fullBlockedEvent.healing.failedFullHp ||
            cheekActor.hp != 100) return 10910;
        cheekActor.hp = 0;
        if (applyPokemonBerryAbilityHealing(cheekActor, cheekPlans[0], cheekHealing, cheekEvent) !=
                PokemonBerryEffectResult::Ready || cheekActor.hp || cheekEvent.healed) return 10907;
        cheekActor.maxHp = 2;
        cheekActor.hp = 1;
        cheekRequests.abilityHealCount = 0;
        if (queuePokemonBerryUseAbilityHealing(cheekActor, &cheekPouchId, 1, true, true, cheekRequests) !=
                PokemonBerryEffectResult::Ready || cheekPlans[0].healingRequested != 1) return 10908;
        cheekRequests.abilityHealCount = 0;
        if (queuePokemonBerryUseAbilityHealing(cheekActor, &cheekPouchId, 1, false, false, cheekRequests) !=
                PokemonBerryEffectResult::NoEffect || cheekRequests.abilityHealCount) return 10909;
    }
    // The pinned Lansat/Starf threshold-holder quirk remains literal .25.
    for (const auto& profile : PokerogueContent::kBerryEffectProfiles) {
        if (std::strcmp(profile.effect, "CRIT_BOOST") && std::strcmp(profile.effect, "RANDOM_STAT")) continue;
        auto higherHp = actor;
        higherHp.hp = 40;
        auto enhanced = policy;
        enhanced.thresholdMultiplier = 2;
        PokemonBerryEffectPlan untouched{};
        const auto before = rng.state();
        if (planPokemonBerryEffect(profile.id, higherHp, enhanced, rng, untouched) != PokemonBerryEffectResult::NoEffect ||
            !sameRng(before, rng.state())) return 10320;
    }
    return 0;
}

static int checkHeldBerryStorage() {
    using namespace Pokerogue3DS;
    constexpr size_t types = sizeof(PokerogueContent::kBerryTypes) / sizeof(PokerogueContent::kBerryTypes[0]);
    NativeHeldModifierInstance records[4]{}; size_t count = 0;
    for (size_t i = 0; i < types; ++i) {
        const auto& type = PokerogueContent::kBerryTypes[i];
        NativeHeldModifierInstance held{}, decoded{};
        if (initializeHeldBerry(type.id, 123, 1, true, held) != HeldModifierStorageResult::Ok) return 10270;
        uint16_t actual = 0xffff; char bytes[1024]{}; size_t size = 0;
        if (!heldBerryType(held, actual) || actual != type.id ||
            encodeNativeHeldModifier(held, bytes, sizeof(bytes), size) != NativeSaveResult::Ok ||
            decodeNativeHeldModifier(bytes, size, decoded) != NativeSaveResult::Ok ||
            !heldBerryType(decoded, actual) || actual != type.id || decoded.ownerPokemonId != 123 ||
            decoded.stackCount != 1 || std::strcmp(decoded.rawArguments, held.rawArguments)) return 10271;
        count = 0; for (auto& row : records) row = {};
        for (uint16_t stack = 0; stack < type.maxHeldStacks; ++stack)
            if (addHeldBerry(records, 4, count, held) != HeldModifierStorageResult::Ok) return 10272;
        if (count != 1 || records[0].stackCount != type.maxHeldStacks ||
            addHeldBerry(records, 4, count, held) != HeldModifierStorageResult::CapacityExceeded ||
            records[0].stackCount != type.maxHeldStacks) return 10273;
        held.ownerPokemonId = 456;
        if (addHeldBerry(records, 4, count, held) != HeldModifierStorageResult::Ok || count != 2) return 10274;
        NativeHeldModifierInstance otherVariant{};
        if (initializeHeldBerry(PokerogueContent::kBerryTypes[(i + 1) % types].id, 123, 1, true, otherVariant) !=
                HeldModifierStorageResult::Ok || addHeldBerry(records, 4, count, otherVariant) != HeldModifierStorageResult::Ok ||
            count != 3 || records[0].stackCount != type.maxHeldStacks || records[2].stackCount != 1) return 10277;
        otherVariant.ownerPokemonId = 789;
        if (addHeldBerry(records, 3, count, otherVariant) != HeldModifierStorageResult::CapacityExceeded || count != 3) return 10278;
        held.rawArguments[6] = '2'; actual = 999;
        if (heldBerryType(held, actual) || actual != 999 ||
            addHeldBerry(records, 4, count, held) != HeldModifierStorageResult::InvalidState || count != 3) return 10275;
    }
    FirstRunRuntime game(1);
    NativeRunSave saved{}, restored{};
    if (!captureActiveTestCheckpoint(game, saved)) return 10279;
    saved.heldModifierCount = 1;
    if (initializeHeldBerry(PokerogueContent::kBerryTypes[0].id, saved.playerParty[0].pokemonId,
            1, true, saved.heldModifiers[0]) != HeldModifierStorageResult::Ok) return 10280;
    char envelope[kNativeSaveMaxBytes]{}; size_t envelopeSize = 0;
    uint16_t restoredType = 0xffff;
    if (encodeNativeRunSave(saved, envelope, sizeof(envelope), envelopeSize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(envelope, envelopeSize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        restored.heldModifierCount != 1 || !heldBerryType(restored.heldModifiers[0], restoredType) ||
        restoredType != PokerogueContent::kBerryTypes[0].id ||
        restored.heldModifiers[0].ownerPokemonId != restored.playerParty[0].pokemonId) return 10281;
    FirstRunRuntime notYetSupported(2);
    if (notYetSupported.restoreNativeRunSave(restored) || notYetSupported.run().seed != 2) return 10283;
    // Codec preserves unknown typed arguments; the current adapter rejects them.
    saved.heldModifiers[0].rawArguments[6] = '2';
    if (encodeNativeRunSave(saved, envelope, sizeof(envelope), envelopeSize) != NativeSaveResult::Ok ||
        decodeNativeRunSave(envelope, envelopeSize, PokerogueContent::kContentHash, restored) != NativeSaveResult::Ok ||
        std::strcmp(saved.heldModifiers[0].rawArguments, restored.heldModifiers[0].rawArguments) ||
        heldBerryType(restored.heldModifiers[0], restoredType)) return 10282;
    if (!*PokerogueContent::kBerryBehaviorSource.sourceHash) return 10276;
    return 0;
}

static int checkCanonicalBerryGeneration() {
    using namespace Pokerogue3DS;
    const uint16_t seed[] = {'b','e','r','r','y'};
    PokerogueRngAdapter rng; rng.sow(seed, 5);
    bool observedSpecial = false, observedFallback = false;
    constexpr size_t count = sizeof(PokerogueContent::kBerryTypes) / sizeof(PokerogueContent::kBerryTypes[0]);
    for (unsigned i = 0; i < 128; ++i) {
        auto expected = rng;
        const auto roll = expected.randSeedInt(PokerogueContent::kBerryGenerationRollRange);
        uint16_t expectedId = 0; bool special = false;
        for (const auto& row : PokerogueContent::kBerryGenerationThresholds)
            if (roll < row.upperExclusive) { expectedId = row.berryId; special = true; break; }
        if (!special) expectedId = PokerogueContent::kBerryTypes[expected.randSeedInt(
            count - PokerogueContent::kBerryGenerationExcludedCount) + PokerogueContent::kBerryGenerationOffset].id;
        observedSpecial |= special; observedFallback |= !special;
        uint16_t actual = 0xffff;
        if (!generateCanonicalBerryType(rng, actual) || actual != expectedId ||
            rng.randSeedUint32() != expected.randSeedUint32()) return 10260;
    }
    if (!observedSpecial || !observedFallback || !*PokerogueContent::kBerryEnumSource.sourceHash ||
        !*PokerogueContent::kBerryGenerationSource.sourceHash) return 10261;
    FirstRunRuntime game(1), replay(2);
    NativeRunSave won{};
    if (!captureActiveTestCheckpoint(game, won)) return 10262;
    won.stage = NativeSaveStage::BattleWon; won.enemyHp = 0;
    setSingleParticipantFixture(won, game);
    if (!game.restoreNativeRunSave(won) || !replay.restoreNativeRunSave(won) ||
        !game.advanceBattleTurn() || !replay.advanceBattleTurn() ||
        !game.advanceBattleTurn() || !replay.advanceBattleTurn()) return 10263;
    bool berryProduced = false;
    if (game.rewardChoiceCount() != replay.rewardChoiceCount()) return 10264;
    for (uint8_t slot = 0; slot < game.rewardChoiceCount(); ++slot) {
        const auto* choice = game.rewardChoice(slot);
        const auto* repeated = replay.rewardChoice(slot);
        if (!choice || !repeated || !choice->poolEntry || !repeated->poolEntry ||
            std::strcmp(choice->poolEntry->itemId, repeated->poolEntry->itemId) ||
            choice->berryType != repeated->berryType) return 10265;
        if (std::strcmp(choice->poolEntry->itemId, "BERRY")) {
            if (choice->berryType != -1) return 10266;
            continue;
        }
        berryProduced = true; bool validType = false;
        for (const auto& type : PokerogueContent::kBerryTypes) validType |= type.id == choice->berryType;
        if (!validType) return 10267;
    }
    return berryProduced ? 0 : 10268;
}

static int checkFlinchTurnLifecycle() {
    using namespace Pokerogue3DS;
    PokerogueRngAdapter rng;
    const uint16_t seed[] = {'f', 'l', 'i', 'n', 'c', 'h'};
    rng.sow(seed, 6);
    auto expected = rng; bool tag = false;
    if (!applyPokemonMoveFlinch(100, true, true, tag, rng) || !tag ||
        rng.randSeedUint32() != expected.randSeedUint32()) return 10231;
    tag = false; expected = rng;
    const bool roll = expected.randSeedInt(100) < 30;
    if (!applyPokemonMoveFlinch(30, true, true, tag, rng) || tag != roll ||
        rng.randSeedUint32() != expected.randSeedUint32()) return 10232;
    tag = false; expected = rng;
    if (applyPokemonMoveFlinch(30, false, true, tag, rng) || tag ||
        rng.randSeedUint32() != expected.randSeedUint32()) return 10233;
    tag = false; expected = rng;
    expected.randSeedInt(100); // Suppressed zero chance still rolls in AddBattlerTagAttr.apply.
    if (!applyPokemonMoveFlinch(0, true, true, tag, rng) || tag ||
        rng.randSeedUint32() != expected.randSeedUint32()) return 10244;
    tag = false; expected = rng;
    if (!applyPokemonMoveFlinch(30, true, false, tag, rng) || tag ||
        rng.randSeedUint32() != expected.randSeedUint32()) return 10245;
    if (!singleDamageFlinchMove(310) || !neutralPokemonFlinchCallbacks(65) ||
        neutralPokemonFlinchCallbacks(39) || singleDamageFlinchMove(157)) return 10234;
    const auto* innerFocus = pokemonFlinchAbilityProfile(39);
    const auto* steadfast = pokemonFlinchAbilityProfile(80);
    if (!innerFocus || !innerFocus->callbacksResolved || !innerFocus->blocksFlinch ||
        !steadfast || !steadfast->callbacksResolved || steadfast->reactionStatMask != 16 || steadfast->reactionStages != 1) return 10246;
    PokemonBattleState reactionActor{}, reactionObserver{};
    reactionActor.hp = reactionObserver.hp = 10;
    reactionActor.abilityId = 80;
    reactionObserver.abilityId = 65;
    PokemonStatStageEffectEvent reaction{};
    expected = rng;
    if (executePokemonFlinchStatReaction(reactionActor, reactionObserver, true, reaction) != PokemonStatStageEffectResult::Ok ||
        reactionActor.statStages[4] != 1 || reaction.changedStatMask != 16 ||
        rng.randSeedUint32() != expected.randSeedUint32()) return 10249;
    reactionActor.statStages[4] = 6;
    if (executePokemonFlinchStatReaction(reactionActor, reactionObserver, true, reaction) != PokemonStatStageEffectResult::Ok ||
        reactionActor.statStages[4] != 6 || reaction.changedStatMask || reaction.requestedStages != 1) return 10250;
    if (executePokemonFlinchStatReaction(reactionActor, reactionObserver, false, reaction) != PokemonStatStageEffectResult::UnresolvedPolicy ||
        reactionActor.statStages[4] != 6) return 10251;
    bool hasCopyObserver = false;
    for (const auto& profile : PokerogueContent::kAbilityStatStageProfiles) {
        if (!profile.copiesRaises) continue;
        reactionObserver.abilityId = profile.abilityId;
        hasCopyObserver = true; break;
    }
    if (!hasCopyObserver || executePokemonFlinchStatReaction(reactionActor, reactionObserver, true, reaction) !=
            PokemonStatStageEffectResult::Ok || reactionActor.statStages[4] != 6 ||
        reactionObserver.statStages[4] != 1) return 10252;
    tag = false; expected = rng;
    expected.randSeedInt(100);
    if (!applyPokemonMoveFlinch(30, innerFocus->callbacksResolved, true, tag, rng, innerFocus->blocksFlinch) || tag ||
        rng.randSeedUint32() != expected.randSeedUint32()) return 10247;
    tag = false; expected = rng;
    if (!applyPokemonMoveFlinch(100, innerFocus->callbacksResolved, true, tag, rng, innerFocus->blocksFlinch) || tag ||
        rng.randSeedUint32() != expected.randSeedUint32()) return 10248;
    FirstRunRuntime game(1);
    int16_t effectiveChance = -1;
    if (!resolvePokemonMoveEffectChance(44, 65, 19, false, effectiveChance) || effectiveChance != 0) return 10242;
    bool neutralEncounter = false;
    for (uint32_t sourceSeed = 1; sourceSeed <= 64; ++sourceSeed) {
        if (!game.restoreSetup(sourceSeed, 1)) return 10243;
        if (neutralPokemonFlinchCallbacks(game.presentation().enemy.battleState.abilityId) &&
            resolvePokemonMoveEffectChance(44, game.presentation().player.battleState.abilityId,
                game.presentation().enemy.battleState.abilityId, false, effectiveChance) && effectiveChance == 30) {
            neutralEncounter = true; break;
        }
    }
    NativeRunSave checkpoint{};
    if (!neutralEncounter || !captureActiveTestCheckpoint(game, checkpoint)) return 10235;
    const auto* move = PokerogueContent::findMoveById(44);
    checkpoint.playerMoveCount = checkpoint.playerParty[0].moveCount = 1;
    checkpoint.playerParty[0].maxPpResolved = true;
    for (uint8_t i = 0; i < 4; ++i) {
        checkpoint.playerMoveIds[i] = checkpoint.playerParty[0].moveIds[i] = i ? 0 : move->id;
        checkpoint.playerPp[i] = checkpoint.playerParty[0].pp[i] = i ? 0 : move->pp;
        checkpoint.playerParty[0].maxPp[i] = i ? 0 : move->pp;
    }
    // Keep the canonical low-level target alive so the secondary tag can apply.
    checkpoint.playerStatStages[0] = checkpoint.playerParty[0].statStages[0] = -6;
    checkpoint.playerStatStages[4] = checkpoint.playerParty[0].statStages[4] = 6;
    bool observed = false;
    for (uint32_t turn = 1; turn <= 128 && !observed; ++turn) {
        checkpoint.battleTurn = turn;
        if (!game.restoreNativeRunSave(checkpoint) || !game.battleInputSupported() || !game.advanceBattleTurn()) return 10236;
        if (game.battleFeedback() != "Pokemon flinched") continue;
        observed = true;
        if (game.presentation().player.battleState.moves[0].pp != move->pp - 1 ||
            game.presentation().player.battleState.hp != checkpoint.playerHp) return 10237;
        for (uint8_t slot = 0; slot < checkpoint.enemyMoveCount; ++slot)
            if (game.presentation().enemy.battleState.moves[slot].pp != checkpoint.enemyPp[slot]) return 10238;
        NativeRunSave after{};
        FirstRunRuntime restored(2);
        if (game.captureNativeRunSave(after) != NativeSaveResult::Ok || !restored.restoreNativeRunSave(after) ||
            !game.advanceBattleTurn() || !restored.advanceBattleTurn()) return 10239;
        const auto a = game.battleRng().state(), b = restored.battleRng().state();
        if (a.s0 != b.s0 || a.s1 != b.s1 || a.s2 != b.s2 || a.carry != b.carry ||
            game.presentation().player.battleState.hp != restored.presentation().player.battleState.hp ||
            game.presentation().enemy.battleState.hp != restored.presentation().enemy.battleState.hp) return 10240;
    }
    return observed ? 0 : 10241;
}

static int checkPersistentExperienceRewards() {
    using namespace Pokerogue3DS;
    NativePersistentModifierInstance records[kNativePersistentModifierCapacity]{};
    size_t count = 0;
    const auto& policy = PokerogueContent::kClassicFixedModifierRewardPolicy;
    ClassicFixedModifierRewards rewards{};
    if (!planClassicFixedModifierRewards(10, false, rewards) || rewards.count != 2 ||
        std::strcmp(rewards.itemIds[0], policy.experienceItemId) ||
        std::strcmp(rewards.itemIds[1], policy.experienceItemId)) return 10200;
    for (uint8_t i = 0; i < rewards.count; ++i)
        if (!addPersistentExperienceReward(records, kNativePersistentModifierCapacity, count, rewards.itemIds[i])) return 10201;
    uint32_t award = 0;
    if (count != 1 || records[0].stackCount != 2 ||
        !applyPersistentExperienceBoosters(11, records, count, award) || award != 16) return 10202;
    if (!addPersistentExperienceReward(records, kNativePersistentModifierCapacity, count, policy.superExperienceItemId) ||
        !applyPersistentExperienceBoosters(11, records, count, award) || award != 25) return 10203;
    auto limited = records[0];
    const auto* profile = expBoosterItemProfile(policy.experienceItemId);
    if (!profile || profile->boostPercent != 25 || profile->maxStacks != 99) return 10204;
    records[0].stackCount = profile->maxStacks;
    if (addPersistentExperienceReward(records, kNativePersistentModifierCapacity, count, policy.experienceItemId) ||
        records[0].stackCount != profile->maxStacks) return 10205;
    records[0] = limited;
    award = 123;
    if (applyPersistentExperienceBoosters(0xffffffffU, records, count, award) || award != 123) return 10206;
    records[0].rawArguments[0] = 'x';
    if (persistentExperienceInventorySupported(records, count)) return 10207;
    char encoded[1024]{}; size_t size = 0;
    NativePersistentModifierInstance decoded{};
    if (encodeNativePersistentModifier(records[0], encoded, sizeof(encoded), size) != NativeSaveResult::Ok ||
        decodeNativePersistentModifier(encoded, size, decoded) != NativeSaveResult::Ok ||
        decoded.stackCount != records[0].stackCount || std::strcmp(decoded.rawArguments, "x")) return 10208;
    records[0].rawArguments[0] = 0;
    if (!planClassicFixedModifierRewards(50, false, rewards) || rewards.count != 2 ||
        std::strcmp(rewards.itemIds[1], policy.goldenItemId) ||
        addPersistentExperienceReward(records, kNativePersistentModifierCapacity, count, policy.goldenItemId)) return 10209;
    return 0;
}

static int checkBiomeTransitionProgression() {
    using namespace Pokerogue3DS;
    const uint16_t rootSeed[] = {49};
    const char* destination = "unchanged";
    const auto missing = resolveClassicNextBiome("TEST_FIXTURE_INVALID_BIOME", 11, true,
        rootSeed, 1, false, nullptr, destination);
    if (missing != ClassicBiomeTransitionResult::MissingRoute || destination ||
        std::strcmp(classicBiomeTransitionResultName(missing), "Canonical biome route missing")) return 578;
    destination = "unchanged";
    const auto unsupported = resolveClassicNextBiome("town", 11, false, rootSeed, 1, false, nullptr, destination);
    if (unsupported != ClassicBiomeTransitionResult::UnsupportedMode || destination ||
        std::strcmp(classicBiomeTransitionResultName(unsupported), "Biome transition mode unsupported")) return 579;
    if (!std::strcmp(classicBiomeTransitionResultName(ClassicBiomeTransitionResult::AwaitingMapChoice),
        classicBiomeTransitionResultName(ClassicBiomeTransitionResult::InvalidMapChoice))) return 580;
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        FirstRunRuntime game(seed);
        bool eligible = true;
        for (uint16_t wave = 1; wave <= 9; ++wave) {
            NativeRunSave skipSave{};
            if (!captureActiveTestCheckpoint(game, skipSave)) { eligible = false; break; }
            skipSave.stage = NativeSaveStage::BattleWon;
            setSingleParticipantFixture(skipSave, game);
            skipSave.enemyHp = 0;
            if (skipSave.trainerPartyCount) {
                for (uint8_t i = 0; i < skipSave.trainerPartyCount; ++i)
                    skipSave.trainerParty[i].hp = 0;
            }
            if (!game.restoreNativeRunSave(skipSave) || !game.advanceBattleTurn() ||
                !game.skipVictoryReward()) { eligible = false; break; }
        }
        if (!eligible || game.run().wave != 10) continue;
        if (std::strcmp(game.run().biomeId, "town") != 0) return 80;
        NativeRunSave wave10Won{};
        const auto wave10Capture = game.captureNativeRunSave(wave10Won);
        if (wave10Capture == NativeSaveResult::UnsupportedStage) continue;
        if (wave10Capture != NativeSaveResult::Ok || !wave10Won.enemyBoss.segmentCount ||
            !wave10Won.playerPartyCount) return 10140;
        if (!wave10Won.globalRngResolved) return 10270;
        auto partialBoss = wave10Won;
        partialBoss.enemyBoss.segmentIndex = 0;
        PokerogueRngAdapter progressedGlobal;
        const uint16_t globalRoot[] = {'p', 'r', 'o', 'g', 'r', 'e', 's', 's'};
        progressedGlobal.sow(globalRoot, 8);
        for (unsigned draw = 0; draw < 23; ++draw) progressedGlobal.randSeedUint32();
        partialBoss.globalRng = progressedGlobal.state(); // Test-only checkpoint after global draws.

        FirstRunRuntime bossRestore(seed);
        NativeRunSave partialRecaptured{};
        if (!bossRestore.restoreNativeRunSave(partialBoss) ||
            bossRestore.captureNativeRunSave(partialRecaptured) != NativeSaveResult::Ok ||
            partialRecaptured.enemyBoss.segmentCount != partialBoss.enemyBoss.segmentCount ||
            partialRecaptured.enemyBoss.segmentIndex || partialRecaptured.enemyHp != partialBoss.enemyHp) return 10141;
        if (!partialRecaptured.globalRngResolved || partialRecaptured.globalRng.carry != partialBoss.globalRng.carry ||
            partialRecaptured.globalRng.s0 != partialBoss.globalRng.s0 || partialRecaptured.globalRng.s1 != partialBoss.globalRng.s1 ||
            partialRecaptured.globalRng.s2 != partialBoss.globalRng.s2) return 10271;
        auto invalidBoss = partialBoss;
        ++invalidBoss.enemyBoss.segmentCount;
        if (bossRestore.restoreNativeRunSave(invalidBoss)) return 10142;
        wave10Won.enemyBoss.segmentIndex = 0;
        wave10Won.stage = NativeSaveStage::BattleWon;
        setSingleParticipantFixture(wave10Won, game);
        wave10Won.enemyHp = 0;
        if (!game.restoreNativeRunSave(wave10Won) || !game.advanceBattleTurn()) return 81;
        if (!game.skipVictoryReward()) { std::printf("Biome transition seed=%u wave=%u biome=%s feedback=%s\n", seed, game.run().wave, game.run().biomeId, game.battleFeedback().c_str()); return 82; }
        if (game.run().wave != 11) return 83;
        if (std::strcmp(game.run().biomeId, "town") == 0) return 84;
        if (!game.presentation().biomeName || !*game.presentation().biomeName) return 85;
        NativeRunSave laterCheckpoint{};
        const auto captured = game.captureNativeRunSave(laterCheckpoint);
        // This gate covers wild singles, not every random wave-11 encounter.
        if (captured == NativeSaveResult::UnsupportedStage) continue;
        if (captured != NativeSaveResult::Ok || laterCheckpoint.wave != 11 ||
            !laterCheckpoint.playerPartyCount || std::strcmp(laterCheckpoint.biomeId, game.run().biomeId)) return 10120;
        if (laterCheckpoint.persistentModifierCount != 1 || laterCheckpoint.persistentModifiers[0].stackCount != 2 ||
            std::strcmp(persistentModifierDefinition(laterCheckpoint.persistentModifiers[0])->id,
                PokerogueContent::kClassicFixedModifierRewardPolicy.experienceItemId)) return 10210;
        char checkpointBytes[kNativeSaveMaxBytes]{}; size_t checkpointSize = 0;
        NativeRunSave decodedCheckpoint{};
        if (encodeNativeRunSave(laterCheckpoint, checkpointBytes, sizeof(checkpointBytes), checkpointSize) != NativeSaveResult::Ok ||
            decodeNativeRunSave(checkpointBytes, checkpointSize, PokerogueContent::kContentHash, decodedCheckpoint) != NativeSaveResult::Ok ||
            decodedCheckpoint.persistentModifierCount != 1 || decodedCheckpoint.persistentModifiers[0].stackCount != 2) return 10211;
        laterCheckpoint = decodedCheckpoint;
        FirstRunRuntime laterRestore(seed), laterReplay(seed);
        NativeRunSave recaptured{}, replayCaptured{};
        if (!laterRestore.restoreNativeRunSave(laterCheckpoint) || !laterReplay.restoreNativeRunSave(laterCheckpoint) ||
            laterRestore.captureNativeRunSave(recaptured) != NativeSaveResult::Ok ||
            laterReplay.captureNativeRunSave(replayCaptured) != NativeSaveResult::Ok ||
            recaptured.persistentModifierCount != 1 || recaptured.persistentModifiers[0].stackCount != 2 ||
            recaptured.wave != 11 || recaptured.encounterDex != laterCheckpoint.encounterDex ||
            std::strcmp(recaptured.biomeId, laterCheckpoint.biomeId) ||
            recaptured.playerHp != laterCheckpoint.playerHp || recaptured.enemyHp != laterCheckpoint.enemyHp ||
            recaptured.playerParty[0].pokemonId != laterCheckpoint.playerParty[0].pokemonId ||
            recaptured.playerHp != replayCaptured.playerHp || recaptured.enemyHp != replayCaptured.enemyHp) return 10121;
        const auto a = laterRestore.battleRng().state(), b = laterReplay.battleRng().state();
        if (a.carry != b.carry || a.s0 != b.s0 || a.s1 != b.s1 || a.s2 != b.s2) return 10122;
        auto legacyLater = laterCheckpoint;
        legacyLater.playerPartyCount = 0;
        legacyLater.activePlayerMember = 0xff;
        for (auto& member : legacyLater.playerParty) member = {};
        FirstRunRuntime rejectedLegacy(seed);
        if (rejectedLegacy.restoreNativeRunSave(legacyLater)) return 10123;
        return 0;
    }
    return 86;
}

static int checkDoubleBattleTargetingAndMechanics() {
    using namespace Pokerogue3DS;
    if (pokemonPendingDoubleExperienceMask(true, false, 0) != 1 ||
        pokemonPendingDoubleExperienceMask(false, true, 0) != 2 ||
        pokemonPendingDoubleExperienceMask(true, true, 1) != 2 ||
        pokemonPendingDoubleExperienceMask(true, true, 2) != 1 ||
        pokemonPendingDoubleExperienceMask(true, true, 3) != 0 ||
        pokemonPendingDoubleExperienceMask(false, false, 0) != 0) return 585;
    FirstRunRuntime friendshipSource(1);
    auto friendshipActor = friendshipSource.presentation().player.battleState;
    friendshipActor.friendship = 70;
    const auto* friendshipRoot = pokemonRootSpecies(friendshipActor.speciesDex);
    if (!friendshipRoot) return 582;
    NativeStarterCandyRecord friendshipRecord{friendshipRoot->dex, 0, 0};
    PokemonFriendshipPolicy friendshipPolicy{};
    friendshipPolicy.resolved = true;
    friendshipPolicy.candyMultiplier = PokerogueContent::kClassicCandyFriendshipMultiplier;
    const auto bothVictories = pokemonPendingDoubleExperienceMask(true, true, 0);
    if (bothVictories != 3) return 581;
    for (uint8_t defeat = 0; defeat < 2; ++defeat) {
        StarterCandyAwardEvent event{};
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord,
            PokerogueContent::kFriendshipGainFromBattle, friendshipPolicy, false, event) !=
            NativeFriendshipApplyResult::Applied) return 583;
    }
    if (friendshipActor.friendship != 76 || friendshipRecord.friendship != 18) return 584;
    for (uint32_t seed = 1; seed <= 256; ++seed) {
        FirstRunRuntime game(seed);
        if (!game.doubleBattle()) continue;
        const auto& pres = game.presentation();
        if (!pres.enemy.actorIdentityResolved || !pres.secondEnemy.actorIdentityResolved) return 100;
        if (game.selectedTarget() != 0) return 101;
        if (!game.cycleTarget(1) || game.selectedTarget() != 1) return 102;
        if (!game.cycleTarget(1) || game.selectedTarget() != 0) return 103;
        if (!game.cycleTarget(-1) || game.selectedTarget() != 1) return 104;
        if (!game.cycleTarget(-1) || game.selectedTarget() != 0) return 105;

        if (!game.doubleBattleSupported()) continue;
        const uint8_t initialHp0 = pres.enemy.battleState.hp;
        const uint8_t initialHp1 = pres.secondEnemy.battleState.hp;
        const uint8_t playerHp = pres.player.battleState.hp;
        if (!initialHp0 || !initialHp1 || !playerHp) return 106;

        const auto& scene = game.scene();
        if (!scene.nodeCount || !scene.nodes[4].text || !std::strchr(scene.nodes[4].text, '>')) return 107;

        if (!game.advanceBattleTurn()) return 108;
        return 0;
    }
    return 109;
}

static int checkWave200FinalBossAndGameClear() {
    using namespace Pokerogue3DS;
    if (classifyClassicWave(200) != ClassicWaveKind::FinalBoss) return 110;
    ClassicVictoryPlan plan{};
    if (!planClassicVictory(200, plan)) return 111;
    if (!plan.contains(ClassicVictoryStep::GameClear) || plan.nextWave != 0) return 112;

    PokerogueRngAdapter rng;
    uint16_t seed[PokerogueRngAdapter::kMaxSeedCodeUnits] = {'t', 'e', 's', 't'};
    rng.sow(seed, 4);
    if (PokerogueEncounterResolver::bossLevelForWave(200, rng) != 200) return 113;
    const auto* firstPhase = findPokemonFixedEnemyMoveset(890, 0);
    const auto* secondPhase = findPokemonFixedEnemyMoveset(890, 1);
    if (!firstPhase || !secondPhase || findPokemonFixedEnemyMoveset(1, 0)) return 335;
    const uint16_t firstExpected[] = {795, 188, 53, 322};
    const uint16_t secondExpected[] = {744, 440, 53, 105};
    for (uint8_t slot = 0; slot < 4; ++slot)
        if (firstPhase->moveIds[slot] != firstExpected[slot] ||
            secondPhase->moveIds[slot] != secondExpected[slot]) return 336;
    const auto* finalSpecies = PokerogueContent::findSpeciesByDex(890);
    PokemonBattleInit bossInput{};
    bossInput.speciesDex = 890;
    bossInput.formId = finalSpecies->firstFormId[0] ? finalSpecies->firstFormId : nullptr;
    bossInput.level = 200;
    bossInput.abilityId = finalSpecies->ability1;
    bossInput.gender = PokemonGender::Genderless;
    bossInput.nature = PokemonNature::Hardy;
    bossInput.moveCount = 4;
    for (uint8_t slot = 0; slot < 4; ++slot) bossInput.moveIds[slot] = secondPhase->moveIds[slot];
    PokemonBattleState bossActor{};
    if (initializePokemonBattleState(bossInput, bossActor) != PokemonBattleInitResult::Ok ||
        !applyPokemonFixedEnemyMovePp(*secondPhase, bossActor) ||
        bossActor.moves[3].maxPp != 1 || bossActor.moves[3].pp != 1) return 337;
    PokemonBattleState phaseOneActor{};
    for (uint8_t slot = 0; slot < 4; ++slot) bossInput.moveIds[slot] = firstPhase->moveIds[slot];
    if (initializePokemonBattleState(bossInput, phaseOneActor) != PokemonBattleInitResult::Ok) return 339;
    phaseOneActor.hp = 1;
    phaseOneActor.moves[0].pp = 0;
    phaseOneActor.statStages[0] = 2;
    PokemonBossState phaseOneBoss{};
    if (!initializeClassicPokemonBossState(890, 200, 200, true, true, phaseOneBoss)) return 340;
    const auto unreachedBoss = phaseOneBoss;
    if (preparePokemonFinalBossSecondPhase(200, phaseOneActor, phaseOneBoss) ||
        phaseOneActor.hp != 1 || phaseOneBoss.segmentIndex != unreachedBoss.segmentIndex) return 341;
    phaseOneBoss.segmentIndex = 0;
    if (preparePokemonFinalBossSecondPhase(199, phaseOneActor, phaseOneBoss) || phaseOneActor.hp != 1)
        return 342;
    if (!preparePokemonFinalBossSecondPhase(200, phaseOneActor, phaseOneBoss) ||
        phaseOneActor.hp != phaseOneActor.maxHp || std::strcmp(phaseOneActor.formId, "eternatus:eternamax") ||
        phaseOneBoss.segmentCount != 5 || phaseOneBoss.segmentIndex != 4 ||
        phaseOneBoss.classicFinalBossFirstPhase || phaseOneActor.statStages[0] != 2 ||
        phaseOneActor.moves[3].maxPp != 1 || phaseOneActor.moves[3].pp != 1) return 343;
    for (uint8_t slot = 0; slot < 4; ++slot)
        if (phaseOneActor.moves[slot].moveId != secondPhase->moveIds[slot]) return 344;
    if (preparePokemonFinalBossSecondPhase(200, phaseOneActor, phaseOneBoss) ||
        phaseOneBoss.segmentIndex != 4) return 345;
    HeldItemStackTransferEvent transfer{};
    if (calculateHeldItemStackTransfer(3, true, 2, 5, 1, transfer) !=
            HeldItemStackTransferResult::Transferred || transfer.transferred != 1 ||
        transfer.sourceRemaining != 2 || transfer.targetStack != 3 || transfer.removeSource) return 346;
    if (calculateHeldItemStackTransfer(3, true, 4, 5, 3, transfer) !=
            HeldItemStackTransferResult::Transferred || transfer.transferred != 1 ||
        transfer.sourceRemaining != 2 || transfer.targetStack != 5) return 347;
    const auto unchangedTransfer = transfer;
    if (calculateHeldItemStackTransfer(1, true, 5, 5, 1, transfer) !=
            HeldItemStackTransferResult::NoCapacity || transfer.targetStack != unchangedTransfer.targetStack ||
        transfer.transferred != unchangedTransfer.transferred) return 348;
    if (calculateHeldItemStackTransfer(1, false, 0, 1, 1, transfer) !=
            HeldItemStackTransferResult::Transferred || transfer.sourceRemaining ||
        transfer.targetStack != 1 || !transfer.removeSource) return 349;
    const auto successfulTransfer = transfer;
    if (calculateHeldItemStackTransfer(0, false, 0, 1, 1, transfer) !=
            HeldItemStackTransferResult::InvalidState || transfer.targetStack != successfulTransfer.targetStack ||
        calculateHeldItemStackTransfer(1, false, 1, 2, 1, transfer) !=
            HeldItemStackTransferResult::InvalidState) return 350;
    const uint32_t theftOpponents[] = {1001, 1002};
    const HeldItemTransferCandidate theftInventory[] = {
        {1001, 11, true}, {1002, 19, false}, {1001, 13, true}, {1002, 20, true}
    };
    auto expectedTheftRng = rng;
    auto actualTheftRng = rng;
    const auto chosenOpponent = static_cast<size_t>(expectedTheftRng.randSeedInt(2));
    const auto chosenItem = expectedTheftRng.randSeedInt(chosenOpponent ? 1 : 2);
    const size_t expectedInventoryIndex = chosenOpponent ? 20 : chosenItem ? 13 : 11;
    HeldItemTransferSelection theftSelection{};
    if (selectHeldItemTransferAttempt(theftOpponents, 2, theftInventory, 4, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::Selected ||
        !theftSelection.itemFound || theftSelection.opponentIndex != chosenOpponent ||
        theftSelection.inventoryIndex != expectedInventoryIndex) return 351;
    auto actualTheftState = actualTheftRng.state();
    auto expectedTheftState = expectedTheftRng.state();
    if (actualTheftState.carry != expectedTheftState.carry || actualTheftState.s0 != expectedTheftState.s0 ||
        actualTheftState.s1 != expectedTheftState.s1 || actualTheftState.s2 != expectedTheftState.s2) return 352;
    expectedTheftRng.randSeedInt(2);
    if (selectHeldItemTransferAttempt(theftOpponents, 2, nullptr, 0, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::NoItem || theftSelection.itemFound)
        return 353;
    actualTheftState = actualTheftRng.state();
    expectedTheftState = expectedTheftRng.state();
    if (actualTheftState.carry != expectedTheftState.carry || actualTheftState.s0 != expectedTheftState.s0 ||
        actualTheftState.s1 != expectedTheftState.s1 || actualTheftState.s2 != expectedTheftState.s2) return 354;
    const auto beforeInvalidTheft = actualTheftRng.state();
    if (selectHeldItemTransferAttempt(nullptr, 2, theftInventory, 4, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::InvalidState ||
        actualTheftRng.state().s0 != beforeInvalidTheft.s0 ||
        selectHeldItemTransferAttempt(nullptr, 0, nullptr, 0, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::NoOpponent ||
        actualTheftRng.state().s0 != beforeInvalidTheft.s0) return 355;
    expectedTheftRng.randSeedInt(2);
    if (selectHeldItemTransferAttempt(theftOpponents, 2, theftInventory, 4, 0,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::NoTransferCount ||
        theftSelection.itemFound) return 356;
    actualTheftState = actualTheftRng.state();
    expectedTheftState = expectedTheftRng.state();
    if (actualTheftState.carry != expectedTheftState.carry || actualTheftState.s0 != expectedTheftState.s0 ||
        actualTheftState.s1 != expectedTheftState.s1 || actualTheftState.s2 != expectedTheftState.s2) return 357;
    NativeHeldModifierInstance blackHoleInstance{};
    if (initializeHeldModifierInstance("MINI_BLACK_HOLE", 89001, 1, false, "[]", blackHoleInstance) !=
            HeldModifierStorageResult::Ok || !heldModifierDefinition(blackHoleInstance) ||
        std::strcmp(heldModifierDefinition(blackHoleInstance)->id, "MINI_BLACK_HOLE") ||
        blackHoleInstance.transferable || blackHoleInstance.ownerPokemonId != 89001 ||
        std::strcmp(blackHoleInstance.rawArguments, "[]")) return 358;
    NativeHeldModifierInstance heldRecords[2]{};
    size_t heldCount = 0;
    if (appendHeldModifierInstance(heldRecords, 2, heldCount, blackHoleInstance) !=
            HeldModifierStorageResult::Ok || heldCount != 1) return 359;
    auto otherHolder = blackHoleInstance;
    otherHolder.ownerPokemonId = 89002;
    otherHolder.transferable = true;
    if (appendHeldModifierInstance(heldRecords, 2, heldCount, otherHolder) != HeldModifierStorageResult::Ok ||
        appendHeldModifierInstance(heldRecords, 2, heldCount, otherHolder) !=
            HeldModifierStorageResult::CapacityExceeded || heldCount != 2) return 360;
    if (!removeHeldModifierInstance(heldRecords, 2, heldCount, 0) || heldCount != 1 ||
        heldRecords[0].ownerPokemonId != 89002 || heldRecords[1].stackCount) return 361;
    const auto heldBeforeInvalid = heldRecords[0];
    if (initializeHeldModifierInstance("MISSING_CANONICAL_ITEM", 0, 1, true, nullptr, heldRecords[0]) !=
            HeldModifierStorageResult::MissingItem || heldRecords[0].ownerPokemonId != heldBeforeInvalid.ownerPokemonId)
        return 362;
    char hugeArguments[129]{};
    for (size_t i = 0; i < 128; ++i) hugeArguments[i] = 'x';
    if (initializeHeldModifierInstance("MINI_BLACK_HOLE", 0, 1, true, hugeArguments, heldRecords[0]) !=
            HeldModifierStorageResult::CapacityExceeded || heldRecords[0].ownerPokemonId != heldBeforeInvalid.ownerPokemonId)
        return 363;
    NativeHeldModifierInstance transferRecords[3]{};
    size_t transferRecordCount = 2;
    transferRecords[0] = otherHolder;
    transferRecords[0].ownerPokemonId = 1001;
    transferRecords[0].stackCount = 2;
    transferRecords[1] = otherHolder;
    transferRecords[1].ownerPokemonId = 1002;
    HeldItemTheftPolicy theftPolicy{};
    theftPolicy.resolved = true;
    theftPolicy.matchingTargetIndex = 1;
    theftPolicy.targetMaxStack = 3;
    HeldItemInventoryTransferEvent inventoryTransfer{};
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 0, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::Transferred || transferRecordCount != 2 ||
        transferRecords[0].stackCount != 1 || transferRecords[1].stackCount != 2 ||
        inventoryTransfer.stacks.transferred != 1 || inventoryTransfer.sourcePokemonId != 1001 ||
        inventoryTransfer.targetPokemonId != 1002 || inventoryTransfer.resultingInventoryIndex != 1) return 364;
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 0, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::Transferred || transferRecordCount != 1 ||
        transferRecords[0].ownerPokemonId != 1002 || transferRecords[0].stackCount != 3 ||
        transferRecords[1].stackCount || !inventoryTransfer.stacks.removeSource) return 365;
    transferRecords[1] = otherHolder;
    transferRecords[1].ownerPokemonId = 1001;
    transferRecordCount = 2;
    theftPolicy.matchingTargetIndex = 0;
    const auto savedInventoryTransfer = inventoryTransfer;
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 1, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::NoCapacity || transferRecordCount != 2 ||
        transferRecords[1].stackCount != 1 || inventoryTransfer.sourcePokemonId != savedInventoryTransfer.sourcePokemonId)
        return 366;
    theftPolicy.resolved = false;
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 1, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::UnresolvedPolicy) return 367;
    transferRecords[1].transferable = false;
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 1, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::ProtectedItem || transferRecords[1].stackCount != 1)
        return 368;
    transferRecords[1].transferable = true;
    theftPolicy.resolved = true;
    theftPolicy.blockedByAbility = true;
    if (applySelectedHeldItemTheft(transferRecords, 3, transferRecordCount, 1, 1002, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::BlockedByAbility || transferRecordCount != 2)
        return 369;
    theftPolicy.blockedByAbility = false;
    theftPolicy.matchingTargetIndex = static_cast<size_t>(-1);
    transferRecords[1].stackCount = 2;
    if (applySelectedHeldItemTheft(transferRecords, 2, transferRecordCount, 1, 1003, theftPolicy,
            inventoryTransfer) != HeldItemInventoryTransferResult::StorageCapacity || transferRecordCount != 2 ||
        transferRecords[1].stackCount != 2 || transferRecords[1].ownerPokemonId != 1001) return 370;
    NativeHeldModifierInstance nativeTheftRecords[3]{};
    nativeTheftRecords[0] = otherHolder;
    nativeTheftRecords[0].ownerPokemonId = 1001;
    nativeTheftRecords[1] = blackHoleInstance; // The boss-held copy cannot be stolen.
    nativeTheftRecords[1].ownerPokemonId = 1001;
    nativeTheftRecords[2] = otherHolder;
    nativeTheftRecords[2].ownerPokemonId = 1002;
    actualTheftRng = rng;
    expectedTheftRng = rng;
    const auto nativeOpponent = static_cast<size_t>(expectedTheftRng.randSeedInt(2));
    expectedTheftRng.randSeedInt(1);
    if (selectNativeHeldItemTransferAttempt(theftOpponents, 2, nativeTheftRecords, 3, 3, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::Selected ||
        !theftSelection.itemFound || theftSelection.opponentIndex != nativeOpponent ||
        theftSelection.inventoryIndex != (nativeOpponent ? 2u : 0u)) return 371;
    actualTheftState = actualTheftRng.state();
    expectedTheftState = expectedTheftRng.state();
    if (actualTheftState.carry != expectedTheftState.carry || actualTheftState.s0 != expectedTheftState.s0 ||
        actualTheftState.s1 != expectedTheftState.s1 || actualTheftState.s2 != expectedTheftState.s2) return 372;
    nativeTheftRecords[0].stackCount = 0;
    const auto beforeInvalidInventorySelection = actualTheftRng.state();
    if (selectNativeHeldItemTransferAttempt(theftOpponents, 2, nativeTheftRecords, 3, 3, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::InvalidState ||
        actualTheftRng.state().s0 != beforeInvalidInventorySelection.s0 ||
        selectNativeHeldItemTransferAttempt(theftOpponents, 2, nativeTheftRecords, 2, 3, 1,
            actualTheftRng, theftSelection) != HeldItemTransferSelectionResult::InvalidState) return 373;
    auto heldForCodec = blackHoleInstance;
    std::strcpy(heldForCodec.rawArguments, "{\n\"unknown\": [1, 2]\n}");
    char heldPayload[1024]{};
    size_t heldPayloadSize = 0;
    NativeHeldModifierInstance decodedHeld{};
    if (encodeNativeHeldModifier(heldForCodec, heldPayload, sizeof(heldPayload), heldPayloadSize) !=
            NativeSaveResult::Ok || decodeNativeHeldModifier(heldPayload, heldPayloadSize, decodedHeld) !=
            NativeSaveResult::Ok || decodedHeld.ownerPokemonId != heldForCodec.ownerPokemonId ||
        decodedHeld.stackCount != heldForCodec.stackCount || decodedHeld.transferable ||
        std::strcmp(decodedHeld.rawArguments, heldForCodec.rawArguments) ||
        std::strcmp(heldModifierDefinition(decodedHeld)->id, "MINI_BLACK_HOLE")) return 379;
    char repeatedHeldPayload[1024]{};
    size_t repeatedHeldSize = 0;
    if (encodeNativeHeldModifier(decodedHeld, repeatedHeldPayload, sizeof(repeatedHeldPayload), repeatedHeldSize) !=
            NativeSaveResult::Ok || repeatedHeldSize != heldPayloadSize ||
        std::memcmp(repeatedHeldPayload, heldPayload, heldPayloadSize)) return 380;
    const auto beforeInvalidHeldDecode = decodedHeld;
    if (decodeNativeHeldModifier(heldPayload, heldPayloadSize - 1, decodedHeld) != NativeSaveResult::InvalidFormat ||
        decodedHeld.ownerPokemonId != beforeInvalidHeldDecode.ownerPokemonId) return 381;
    heldPayload[7] = 'X'; // Unknown canonical ID cannot silently resolve to another catalog row.
    if (decodeNativeHeldModifier(heldPayload, heldPayloadSize, decodedHeld) != NativeSaveResult::InvalidRecord ||
        decodedHeld.canonicalItemIndex != beforeInvalidHeldDecode.canonicalItemIndex) return 382;
    size_t tooSmallHeldSize = 99;
    if (encodeNativeHeldModifier(heldForCodec, repeatedHeldPayload, 1, tooSmallHeldSize) !=
            NativeSaveResult::TooLarge || tooSmallHeldSize) return 383;
    HeldItemTheftPolicy canonicalTheftPolicy{};
    if (resolveTurnHeldItemTransferMatchPolicy(otherHolder, nullptr, 0, 1234, false, false,
            canonicalTheftPolicy) != HeldItemMatchPolicyResult::UnresolvedAbilities || canonicalTheftPolicy.resolved)
        return 389;
    if (resolveTurnHeldItemTransferMatchPolicy(otherHolder, nullptr, 0, 1234, true, false,
            canonicalTheftPolicy) != HeldItemMatchPolicyResult::Resolved || !canonicalTheftPolicy.resolved ||
        canonicalTheftPolicy.targetMaxStack != 1 || canonicalTheftPolicy.matchingTargetIndex != static_cast<size_t>(-1))
        return 390;
    NativeHeldModifierInstance matchingBlackHole = blackHoleInstance;
    matchingBlackHole.ownerPokemonId = 1234;
    if (resolveTurnHeldItemTransferMatchPolicy(otherHolder, &matchingBlackHole, 1, 1234, true, false,
            canonicalTheftPolicy) != HeldItemMatchPolicyResult::Resolved || canonicalTheftPolicy.matchingTargetIndex != 0)
        return 391;
    auto opaqueSource = otherHolder;
    std::strcpy(opaqueSource.rawArguments, "{\"future\":true}");
    const auto policyBeforeOpaque = canonicalTheftPolicy;
    if (resolveTurnHeldItemTransferMatchPolicy(opaqueSource, nullptr, 0, 1234, true, false,
            canonicalTheftPolicy) != HeldItemMatchPolicyResult::UnresolvedArguments ||
        canonicalTheftPolicy.matchingTargetIndex != policyBeforeOpaque.matchingTargetIndex) return 392;
    if (resolveTurnHeldItemTransferMatchPolicy(otherHolder, nullptr, 0, 1234, true, true,
            canonicalTheftPolicy) != HeldItemMatchPolicyResult::Resolved || !canonicalTheftPolicy.blockedByAbility)
        return 393;
    uint16_t blockingTheftAbility = 0, pendingLostAbility = 0;
    for (const auto& profile : PokerogueContent::kHeldItemTheftAbilityProfiles) {
        if (profile.blocksTheft && !profile.conditionalCallbacks) blockingTheftAbility = profile.abilityId;
        if (profile.requiresPostLostDispatcher && !profile.blocksTheft && !profile.conditionalCallbacks)
            pendingLostAbility = profile.abilityId;
    }
    if (!blockingTheftAbility || !pendingLostAbility) return 394;
    bool canonicalBlocked = false;
    if (resolveHeldItemTheftAbilityPolicy(&blockingTheftAbility, 1, false, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::UnresolvedApplicability || canonicalBlocked) return 395;
    if (resolveHeldItemTheftAbilityPolicy(&blockingTheftAbility, 1, true, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::Resolved || !canonicalBlocked) return 396;
    if (resolveHeldItemTheftAbilityPolicy(&pendingLostAbility, 1, true, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::UnresolvedCallbacks || !canonicalBlocked) return 397;
    const uint16_t combinedTheftAbilities[] = {blockingTheftAbility, pendingLostAbility};
    if (resolveHeldItemTheftAbilityPolicy(combinedTheftAbilities, 2, true, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::Resolved || !canonicalBlocked) return 398;
    const uint16_t pressureTheftAbility = 46;
    if (resolveHeldItemTheftAbilityPolicy(&pressureTheftAbility, 1, true, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::Resolved || canonicalBlocked) return 399;
    const uint16_t unknownTheftAbility = 65535;
    if (resolveHeldItemTheftAbilityPolicy(&unknownTheftAbility, 1, true, canonicalBlocked) !=
            HeldItemTheftAbilityPolicyResult::UnknownAbility || canonicalBlocked) return 400;
    HeldItemLostTagState lostTags{};
    if (applyHeldItemLostCallbacks(&pendingLostAbility, 1, true, true, lostTags) !=
            HeldItemLostCallbackResult::NoChange || lostTags.unburden) return 401;
    if (applyHeldItemLostCallbacks(&pendingLostAbility, 1, true, false, lostTags) !=
            HeldItemLostCallbackResult::Applied || !lostTags.unburden) return 402;
    if (applyHeldItemLostCallbacks(&pendingLostAbility, 1, true, false, lostTags) !=
            HeldItemLostCallbackResult::NoChange) return 403;
    const uint16_t invalidLostSet[] = {pendingLostAbility, 65535};
    lostTags = {};
    if (applyHeldItemLostCallbacks(invalidLostSet, 2, true, false, lostTags) !=
            HeldItemLostCallbackResult::UnknownAbility || lostTags.unburden) return 404;
    NativeHeldModifierInstance callbackRecords[2]{};
    callbackRecords[0] = otherHolder;
    callbackRecords[0].ownerPokemonId = 1001;
    callbackRecords[0].stackCount = 1;
    size_t callbackCount = 1;
    PokemonBattleState callbackActor{};
    callbackActor.pokemonId = 1001;
    HeldItemTheftPolicy callbackPolicy{};
    callbackPolicy.resolved = true;
    callbackPolicy.targetMaxStack = 1;
    HeldItemInventoryTransferEvent callbackEvent{};
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, &pendingLostAbility, 1, false, true, callbackEvent) !=
            HeldItemInventoryTransferResult::UnresolvedPolicy || callbackActor.heldItemLostTags.unburden ||
        callbackRecords[0].ownerPokemonId != 1001) return 410;
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, &blockingTheftAbility, 1, true, true, callbackEvent) !=
            HeldItemInventoryTransferResult::BlockedByAbility || callbackActor.heldItemLostTags.unburden ||
        callbackRecords[0].ownerPokemonId != 1001) return 411;
    callbackRecords[1] = callbackRecords[0];
    callbackRecords[1].ownerPokemonId = 1002;
    callbackCount = 2;
    callbackPolicy.matchingTargetIndex = 1;
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, &pendingLostAbility, 1, true, true, callbackEvent) !=
            HeldItemInventoryTransferResult::NoCapacity || callbackActor.heldItemLostTags.unburden ||
        callbackCount != 2 || callbackRecords[0].ownerPokemonId != 1001) return 413;
    callbackCount = 1;
    callbackRecords[1] = {};
    callbackPolicy.matchingTargetIndex = static_cast<size_t>(-1);
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, &pendingLostAbility, 1, true, true, callbackEvent) !=
            HeldItemInventoryTransferResult::Transferred || !callbackActor.heldItemLostTags.unburden ||
        callbackRecords[0].ownerPokemonId != 1002 || callbackEvent.sourcePokemonId != 1001) return 412;
    HeldAbilityApplicabilityContext abilityContext{};
    HeldApplicableAbilitySet applicableSet{};
    if (resolveHeldApplicableAbilities(60, 84, true, abilityContext, applicableSet) !=
        HeldAbilitySetResult::UnresolvedContext) return 414;
    abilityContext.resolved = true;
    abilityContext.alive = false;
    if (resolveHeldApplicableAbilities(60, 84, true, abilityContext, applicableSet) !=
        HeldAbilitySetResult::Resolved || applicableSet.count != 2) return 415;
    abilityContext.ignoreAbilitiesFromOtherActor = true;
    if (resolveHeldApplicableAbilities(60, 84, true, abilityContext, applicableSet) !=
        HeldAbilitySetResult::Resolved || applicableSet.count != 1 || applicableSet.ids[0] != 84) return 416;
    abilityContext.suppressed = true;
    if (resolveHeldApplicableAbilities(60, 84, true, abilityContext, applicableSet) !=
        HeldAbilitySetResult::Resolved || applicableSet.count) return 417;
    abilityContext = {};
    abilityContext.resolved = true;
    if (resolveHeldApplicableAbilities(84, 84, true, abilityContext, applicableSet) !=
        HeldAbilitySetResult::Resolved || applicableSet.count != 1) return 418;
    callbackRecords[0].ownerPokemonId = callbackActor.pokemonId;
    callbackActor.heldItemLostTags = {};
    const uint16_t sameSideAbilities[] = {blockingTheftAbility, pendingLostAbility};
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, sameSideAbilities, 2, true, true, callbackEvent, false) !=
            HeldItemInventoryTransferResult::Transferred || !callbackActor.heldItemLostTags.unburden)
        return 419;
    callbackRecords[0].ownerPokemonId = callbackActor.pokemonId;
    callbackActor.heldItemLostTags = {};
    if (applyHeldItemTheftWithCallbacks(callbackRecords, 2, callbackCount, 0, callbackActor, 1002,
            callbackPolicy, &pendingLostAbility, 1, true, false, callbackEvent, false) !=
            HeldItemInventoryTransferResult::Transferred || callbackActor.heldItemLostTags.unburden)
        return 420;
    auto activationHolder = callbackActor;
    activationHolder.pokemonId = 1002;
    activationHolder.hp = 1;
    auto activationItem = otherHolder;
    activationItem.ownerPokemonId = 1002;
    activationItem.stackCount = 1;
    activationItem.rawArguments[0] = 0;
    callbackRecords[0] = activationItem;
    callbackRecords[0].ownerPokemonId = callbackActor.pokemonId;
    callbackCount = 1;
    HeldTransferOpponent activationOpponent{};
    activationOpponent.actor = &callbackActor;
    activationOpponent.applicabilityResolved = true;
    activationOpponent.abilities.ids[0] = pendingLostAbility;
    activationOpponent.abilities.count = 1;
    PokerogueRngAdapter activationRng{};
    const auto miniMatch = [](const NativeHeldModifierInstance& source,
        const NativeHeldModifierInstance* inventory, size_t inventoryCount, uint32_t target,
        HeldItemTheftPolicy& result) {
        return resolveTurnHeldItemTransferMatchPolicy(source, inventory, inventoryCount, target, true, false, result);
    };
    callbackActor.heldItemLostTags = {};
    if (activateTurnHeldItemTransfer(activationItem, activationHolder, &activationOpponent, 1,
            callbackRecords, 2, callbackCount, activationRng, callbackEvent, miniMatch) !=
            TurnHeldTransferResult::Transferred || !callbackActor.heldItemLostTags.unburden ||
        callbackRecords[0].ownerPokemonId != 1002) return 421;
    callbackRecords[0].ownerPokemonId = callbackActor.pokemonId;
    activationOpponent.abilities.ids[0] = blockingTheftAbility;
    auto expectedActivationRng = activationRng;
    expectedActivationRng.randSeedInt(1);
    expectedActivationRng.randSeedInt(1);
    callbackActor.heldItemLostTags = {};
    if (activateTurnHeldItemTransfer(activationItem, activationHolder, &activationOpponent, 1,
            callbackRecords, 2, callbackCount, activationRng, callbackEvent, miniMatch) !=
            TurnHeldTransferResult::Blocked || callbackRecords[0].ownerPokemonId != callbackActor.pokemonId ||
        callbackActor.heldItemLostTags.unburden || activationRng.state().s0 != expectedActivationRng.state().s0 ||
        activationRng.state().s1 != expectedActivationRng.state().s1 ||
        activationRng.state().s2 != expectedActivationRng.state().s2 ||
        activationRng.state().carry != expectedActivationRng.state().carry) return 423;
    activationHolder.hp = 0;
    if (activateTurnHeldItemTransfer(activationItem, activationHolder, &activationOpponent, 1,
            callbackRecords, 2, callbackCount, activationRng, callbackEvent, miniMatch) !=
            TurnHeldTransferResult::HolderFainted) return 422;
    NativeHeldModifierInstance healingItems[3]{};
    if (initializeHeldModifierInstance("LEFTOVERS", 1001, 2, true, nullptr, healingItems[0]) !=
            HeldModifierStorageResult::Ok ||
        initializeHeldModifierInstance("LEFTOVERS", 1002, 3, true, nullptr, healingItems[1]) !=
            HeldModifierStorageResult::Ok ||
        initializeHeldModifierInstance("SHELL_BELL", 1002, 1, true, nullptr, healingItems[2]) !=
            HeldModifierStorageResult::Ok) return 424;
    HeldItemTheftPolicy healingMatch{};
    if (resolveKnownHeldModifierMatchPolicy(healingItems[0], healingItems, 3, 1002, healingMatch) !=
        HeldItemMatchPolicyResult::Resolved || healingMatch.matchingTargetIndex != 1 ||
        healingMatch.targetMaxStack != 4) return 425;
    size_t healingCount = 3;
    if (applySelectedHeldItemTheft(healingItems, 3, healingCount, 0, 1002, healingMatch, callbackEvent) !=
        HeldItemInventoryTransferResult::Transferred || healingCount != 3 ||
        healingItems[0].stackCount != 1 || healingItems[2].stackCount != 4 ||
        std::strcmp(heldModifierDefinition(healingItems[1])->id, "SHELL_BELL")) return 426;
    PokemonBattleState heldHealActor{};
    heldHealActor.pokemonId = 1001;
    heldHealActor.maxHp = 31;
    heldHealActor.hp = 10;
    PokemonHealingPolicy heldHealPolicy{};
    PokemonHealingEvent heldHealEvent{};
    if (applyHeldHealingModifier(healingItems[0], heldHealActor, 0, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::UnresolvedPolicy || heldHealActor.hp != 10) return 427;
    heldHealPolicy.resolved = true;
    healingItems[0].stackCount = 4;
    if (applyHeldHealingModifier(healingItems[0], heldHealActor, 0, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealEvent.healed != 4 || heldHealActor.hp != 14) return 428;
    healingItems[1].ownerPokemonId = 1001;
    healingItems[1].stackCount = 3;
    if (applyHeldHealingModifier(healingItems[1], heldHealActor, 5, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealEvent.healed != 1 || heldHealActor.hp != 15) return 429;
    heldHealPolicy.healBlocked = true;
    if (applyHeldHealingModifier(healingItems[0], heldHealActor, 0, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || !heldHealEvent.blocked || heldHealActor.hp != 15) return 430;
    heldHealPolicy.healBlocked = false;
    heldHealActor.hp = 10;
    if (applyHeldTurnHealingPhase(healingItems, 3, heldHealActor, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealActor.hp != 14 || heldHealEvent.healed != 4) return 431;
    healingItems[2] = healingItems[0];
    healingItems[2].stackCount = 5;
    if (applyHeldTurnHealingPhase(healingItems, 3, heldHealActor, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::InvalidState || heldHealActor.hp != 14) return 432;
    heldHealActor.hp = 10;
    heldHealActor.turnDamageDealt = 19;
    healingItems[2] = {};
    if (applyHeldMoveHealingPhase(healingItems, 2, heldHealActor, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealEvent.healed != 7 || heldHealActor.hp != 17 ||
        heldHealActor.turnDamageDealt != 19) return 433;
    // Move-end healing reads accumulated turn damage, even if this move added none.
    if (applyHeldMoveHealingPhase(healingItems, 2, heldHealActor, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealEvent.healed != 7 || heldHealActor.hp != 24) return 434;
    heldHealActor.turnDamageDealt = 0;
    if (applyHeldMoveHealingPhase(healingItems, 2, heldHealActor, true, heldHealPolicy, heldHealEvent) !=
        HeldHealingResult::Resolved || heldHealEvent.healed || heldHealActor.hp != 24) return 435;
    if (!heldHealingInventorySupported(healingItems, 2)) return 436;
    healingItems[2] = healingItems[0];
    if (heldHealingInventorySupported(healingItems, 3)) return 437;
    healingItems[2] = activationItem;
    if (heldHealingInventorySupported(healingItems, 3)) return 438;
    healingItems[2] = healingItems[0];
    healingItems[2].ownerPokemonId = 2000;
    size_t retainedCount = 3;
    const uint32_t retainedPartyIds[] = {1001};
    if (!retainPartyHeldInventory(healingItems, 3, retainedCount, retainedPartyIds, 1) ||
        retainedCount != 2 || healingItems[0].ownerPokemonId != 1001 ||
        healingItems[1].ownerPokemonId != 1001 || healingItems[2].stackCount) return 442;
    NativeHeldModifierInstance rewardItems[2]{};
    size_t rewardItemCount = 0;
    auto healingReward = healingItems[0];
    healingReward.stackCount = 1;
    if (addKnownHealingHeldReward(rewardItems, 2, rewardItemCount, healingReward) !=
        HeldRewardAddResult::Added || rewardItemCount != 1) return 443;
    if (addKnownHealingHeldReward(rewardItems, 2, rewardItemCount, healingReward) !=
        HeldRewardAddResult::Merged || rewardItems[0].stackCount != 2 || rewardItemCount != 1) return 444;
    healingReward.stackCount = 3;
    if (addKnownHealingHeldReward(rewardItems, 2, rewardItemCount, healingReward) !=
        HeldRewardAddResult::FullStackNeedsReplacement || rewardItems[0].stackCount != 2) return 445;
    const auto* hyperRestore = hpRestoreItemProfile("HYPER_POTION");
    const auto* fullRestore = hpRestoreItemProfile("FULL_RESTORE");
    if (!hyperRestore || !fullRestore || hyperRestore->points != 200 || hyperRestore->percent != 50)
        return 448;
    auto potionActor = heldHealActor;
    potionActor.maxHp = 1000;
    potionActor.hp = 1;
    uint16_t potionHealed = 0;
    if (!applyPokemonHpRestoreItem(potionActor, *hyperRestore, 1.5, true, potionHealed) ||
        potionHealed != 500 || potionActor.hp != 501) return 449;
    if (applyPokemonHpRestoreItem(potionActor, *fullRestore, 1.0, false, potionHealed) ||
        potionActor.hp != 501) return 450;
    const auto* etherRestore = ppRestoreItemProfile("ETHER");
    const auto* maxElixirRestore = ppRestoreItemProfile("MAX_ELIXIR");
    if (!etherRestore || !maxElixirRestore || etherRestore->allMoves || !maxElixirRestore->allMoves)
        return 451;
    potionActor.moveCount = 2;
    potionActor.moves[0].maxPp = 20; potionActor.moves[0].pp = 5;
    potionActor.moves[1].maxPp = 10; potionActor.moves[1].pp = 0;
    if (!applyPokemonPpRestoreItem(potionActor, *etherRestore, 0) || potionActor.moves[0].pp != 15 ||
        potionActor.moves[1].pp) return 452;
    if (applyPokemonPpRestoreItem(potionActor, *etherRestore, 2) || potionActor.moves[0].pp != 15) return 453;
    if (!applyPokemonPpRestoreItem(potionActor, *maxElixirRestore, 255) || potionActor.moves[0].pp != 20 ||
        potionActor.moves[1].pp != 10) return 454;
    const auto* reviveProfile = reviveItemProfile("REVIVE");
    if (!reviveProfile || reviveProfile->percent != 50) return 456;
    potionActor.maxHp = 31; potionActor.hp = 0;
    if (applyPokemonReviveItem(potionActor, *reviveProfile, false, false, true) || potionActor.hp)
        return 457;
    if (!applyPokemonReviveItem(potionActor, *reviveProfile, true, false, true) || potionActor.hp != 15)
        return 458;
    if (applyPokemonReviveItem(potionActor, *reviveProfile, true, false, true) || potionActor.hp != 15)
        return 459;
    const auto* ashProfile = reviveItemProfile("SACRED_ASH");
    if (!ashProfile || !ashProfile->allParty) return 460;
    auto faintedReserve = potionActor;
    faintedReserve.hp = 0;
    PokemonBattleState* revivalParty[] = {&potionActor, &faintedReserve};
    if (!applyPokemonPartyReviveItem(revivalParty, 2, *ashProfile, true, false, true) ||
        potionActor.hp != 15 || faintedReserve.hp != faintedReserve.maxHp) return 461;
    faintedReserve.hp = 0;
    potionActor.hp = potionActor.maxHp + 1;
    if (applyPokemonPartyReviveItem(revivalParty, 2, *ashProfile, true, false, true) || faintedReserve.hp)
        return 462;
    const auto* ballReward = pokeballRewardProfile("POKEBALL");
    if (!ballReward || ballReward->count != 5) return 463;
    uint16_t rewardBallCounts[5] = {97, 1, 2, 3, 4};
    if (!applyPokeballReward(*ballReward, rewardBallCounts, 5) || rewardBallCounts[0] != 99 ||
        rewardBallCounts[1] != 1) return 464;
    auto invalidBallReward = *ballReward;
    invalidBallReward.ballSymbol = "UNKNOWN_FUTURE_BALL";
    if (applyPokeballReward(invalidBallReward, rewardBallCounts, 5) || rewardBallCounts[0] != 99) return 465;
    const auto preservedBoss = bossActor;
    if (applyPokemonFixedEnemyMovePp(*firstPhase, bossActor) ||
        bossActor.moves[3].maxPp != preservedBoss.moves[3].maxPp) return 338;

    // Recovery weights include reserves; current active can be completely healthy.
    PokemonBattleState rewardActors[6]{};
    const PokemonBattleState* rewardParty[6]{};
    for (uint8_t member = 0; member < 6; ++member) {
        rewardActors[member].maxHp = 200;
        rewardActors[member].hp = member ? 50 : 200;
        rewardActors[member].moveCount = 1;
        rewardActors[member].moves[0].maxPp = 20;
        rewardActors[member].moves[0].pp = member ? 2 : 20;
        rewardParty[member] = &rewardActors[member];
    }
    uint16_t partyBalls[5] = {98, 99, 0, 0, 0};
    InitialClassicRewardWeights partyWeights(rewardActors[0], false, rewardParty, 6, partyBalls, 5);
    const auto weightMatches = [&](const char* id, uint32_t expected) {
        for (const auto& entry : PokerogueContent::kModifierPoolEntries) {
            if (std::strcmp(entry.pool, "modifierPool") || std::strcmp(entry.itemId, id)) continue;
            uint32_t weight = 999;
            return partyWeights.weightFor(entry, weight) && weight == expected;
        }
        return false;
    };
    if (!weightMatches("POTION", 9) || !weightMatches("SUPER_POTION", 3) ||
        !weightMatches("HYPER_POTION", 9) || !weightMatches("MAX_POTION", 3)) return 569;
    if (!weightMatches("ETHER", 9) || !weightMatches("MAX_ETHER", 3) ||
        !weightMatches("ELIXIR", 9) || !weightMatches("MAX_ELIXIR", 3)) return 570;
    if (!weightMatches("REVIVE", 0) || !weightMatches("MAX_REVIVE", 0) ||
        !weightMatches("SACRED_ASH", 0)) return 571;
    for (uint8_t member = 1; member < 4; ++member) rewardActors[member].hp = 0;
    if (!weightMatches("REVIVE", 27) || !weightMatches("MAX_REVIVE", 9) ||
        !weightMatches("SACRED_ASH", 1) || !weightMatches("POTION", 6)) return 572;
    rewardActors[3].hp = 50;
    if (!weightMatches("SACRED_ASH", 0)) return 573;
    if (!weightMatches("POKEBALL", 6) || !weightMatches("GREAT_BALL", 0)) return 575;
    partyBalls[0] = kClassicPokeballLimit;
    partyBalls[1] = kClassicPokeballLimit - 1;
    if (!weightMatches("POKEBALL", 0) || !weightMatches("GREAT_BALL", 6)) return 576;
    partyBalls[0] = kClassicPokeballLimit + 1;
    if (weightMatches("POKEBALL", 6) || weightMatches("POKEBALL", 0)) return 577;
    rewardActors[5].hp = 201;
    if (weightMatches("POTION", 9)) return 574;
    return 0;
}

static int checkExtendedWaveAndBiomeSaveValidation() {
    using namespace Pokerogue3DS;
    NativeRunSave save{};
    save.saveVersion = kNativeSaveVersion;
    save.runtimeVersion = kNativeSaveRuntimeVersion;
    save.stage = NativeSaveStage::BattleActive;
    save.seed = 12345;
    save.starterDex = 1;
    save.playerLevel = 5;
    const auto* starter = PokerogueContent::findSpeciesByDex(1);
    if (!starter || pokemonTotalExperienceForLevel(starter->growthRate, 5, save.playerExperience) != PokemonExperienceResult::Ok) return 120;
    std::memcpy(save.contentHash, PokerogueContent::kContentHash, 64);
    std::strcpy(save.modeId, "classic");
    std::strcpy(save.biomeId, "plains");
    save.wave = 1;
    save.encounterDex = 16;
    save.playerHp = 20;
    save.enemyHp = 20;
    save.battleTurn = 1;
    save.playerMoveCount = 1;
    save.playerMoveIds[0] = 33;
    save.playerPp[0] = 35;
    save.enemyMoveCount = 1;
    save.enemyMoveIds[0] = 33;
    save.enemyPp[0] = 35;

    if (validateNativeRunSave(save, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 121;

    save.wave = 50;
    save.playerLevel = classicExperienceLevelCap(50);
    pokemonTotalExperienceForLevel(starter->growthRate, save.playerLevel, save.playerExperience);
    std::strcpy(save.biomeId, "forest");
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 122;

    save.wave = 200;
    save.playerLevel = 100;
    pokemonTotalExperienceForLevel(starter->growthRate, save.playerLevel, save.playerExperience);
    std::strcpy(save.biomeId, "end");
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 123;

    save.wave = 201;
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) == NativeSaveResult::Ok) return 124;

    save.wave = 100;
    std::strcpy(save.biomeId, "nonexistent_biome_123");
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) == NativeSaveResult::Ok) return 125;

    return 0;
}

static int checkPokeballCaptureMechanics() {
    using namespace Pokerogue3DS;
    if (getPokeballCatchMultiplier(PokeballType::Pokeball) != 1.0) return 130;
    if (getPokeballCatchMultiplier(PokeballType::GreatBall) != 1.5) return 131;
    if (getPokeballCatchMultiplier(PokeballType::UltraBall) != 2.0) return 132;
    if (getPokeballCatchMultiplier(PokeballType::RogueBall) != 3.0) return 133;
    if (getPokeballCatchMultiplier(PokeballType::MasterBall) != -1.0) return 134;

    if (speciesCatchRate(1) != 45) return 135;   // Bulbasaur
    if (speciesCatchRate(16) != 255) return 136; // Pidgey
    if (speciesCatchRate(150) != 3) return 137;  // Mewtwo

    // Direct executeCaptureAttempt blockers:
    PokemonBattleState targetState{};
    targetState.speciesDex = 16;
    targetState.hp = 20;
    targetState.maxHp = 20;

    PokerogueRngAdapter rng;
    uint16_t seed[PokerogueRngAdapter::kMaxSeedCodeUnits] = {'c', 'a', 'p', 't'};
    rng.sow(seed, 4);

    PokemonCaptureEvent outEvent{};
    // Trainer battle blocker
    if (executeCaptureAttempt(targetState, PokeballType::Pokeball, true, false, false, false, rng, outEvent) ||
        outEvent.blocker != CaptureBlocker::TrainerBattle) return 138;

    // Double battle multiple foes blocker
    if (executeCaptureAttempt(targetState, PokeballType::Pokeball, false, true, false, false, rng, outEvent) ||
        outEvent.blocker != CaptureBlocker::MultipleEnemies) return 139;

    // Target fainted blocker
    PokemonBattleState faintedState = targetState;
    faintedState.hp = 0;
    if (executeCaptureAttempt(faintedState, PokeballType::Pokeball, false, false, false, false, rng, outEvent) ||
        outEvent.blocker != CaptureBlocker::TargetFainted) return 140;

    // Wave 200 final boss blocker
    if (executeCaptureAttempt(targetState, PokeballType::Pokeball, false, false, false, true, rng, outEvent) ||
        outEvent.blocker != CaptureBlocker::FinalBossUncatchable) return 141;

    // Boss shield blocker
    if (executeCaptureAttempt(targetState, PokeballType::Pokeball, false, false, true, false, rng, outEvent) ||
        outEvent.blocker != CaptureBlocker::BossShieldActive) return 142;

    PokemonBossState shieldState{3, 2, false, false};
    uint16_t wonderGuardId = 0, ordinaryAbilityId = 0;
    for (const auto& profile : PokerogueContent::kAbilityMovegenProfiles) {
        if (std::strcmp(profile.sourceSymbol, "AbilityId.WONDER_GUARD") == 0) wonderGuardId = profile.abilityId;
        else if (!ordinaryAbilityId) ordinaryAbilityId = profile.abilityId;
    }
    bool blocked = false;
    if (!wonderGuardId || !ordinaryAbilityId ||
        !pokemonBossShieldCaptureBlocked(shieldState, ordinaryAbilityId, PokeballType::Pokeball, blocked) || !blocked) return 256;
    if (!pokemonBossShieldCaptureBlocked(shieldState, ordinaryAbilityId, PokeballType::MasterBall, blocked) || blocked) return 257;
    if (!pokemonBossShieldCaptureBlocked(shieldState, wonderGuardId, PokeballType::Pokeball, blocked) || blocked) return 258;
    shieldState.segmentIndex = 0;
    if (!pokemonBossShieldCaptureBlocked(shieldState, ordinaryAbilityId, PokeballType::Pokeball, blocked) || blocked) return 259;
    if (!executeCaptureAttempt(targetState, PokeballType::MasterBall, false, false, true, false, rng, outEvent) ||
        !outEvent.caught) return 260;

    // Even a guaranteed catch consumes upstream's critical-capture draw.
    auto expectedMasterRng = rng;
    expectedMasterRng.randSeedInt(256);
    auto actualMasterRng = rng;
    PokemonCaptureEvent masterRngEvent{};
    if (!executeCaptureAttempt(targetState, PokeballType::MasterBall, false, false, false, false,
            actualMasterRng, masterRngEvent) || !masterRngEvent.caught ||
        actualMasterRng.randSeedUint32() != expectedMasterRng.randSeedUint32()) return 603;

    // Invalid source data must not become an invented minimum catch rate.
    auto invalidCaptureTarget = targetState;
    invalidCaptureTarget.speciesDex = 65535;
    auto missingSourceRng = rng;
    auto missingSourceExpected = rng;
    PokemonCaptureEvent invalidCaptureEvent{};
    if (executeCaptureAttempt(invalidCaptureTarget, PokeballType::Pokeball, false, false, false, false,
            missingSourceRng, invalidCaptureEvent) ||
        invalidCaptureEvent.blocker != CaptureBlocker::MissingSpeciesData ||
        missingSourceRng.randSeedUint32() != missingSourceExpected.randSeedUint32()) return 604;
    invalidCaptureTarget = targetState;
    invalidCaptureTarget.hp = invalidCaptureTarget.maxHp + 1;
    if (executeCaptureAttempt(invalidCaptureTarget, PokeballType::Pokeball, false, false, false, false,
            rng, invalidCaptureEvent) || invalidCaptureEvent.blocker != CaptureBlocker::InvalidInput) return 605;

    PokemonCaptureCriticalPolicy criticalPolicy{};
    criticalPolicy.resolved = true;
    uint32_t criticalChance = 999;
    const uint32_t dexBoundaries[] = {100, 101, 200, 201, 400, 401, 600, 601, 800, 801};
    const uint32_t expectedChances[] = {0, 21, 21, 42, 42, 63, 63, 85, 85, 106};
    for (uint8_t i = 0; i < 10; ++i) {
        criticalPolicy.caughtSpeciesCount = dexBoundaries[i];
        if (!pokemonCriticalCaptureChance(255, criticalPolicy, criticalChance) ||
            criticalChance != expectedChances[i]) return 606;
    }
    criticalPolicy.catchingCharmStacks = 3;
    if (!pokemonCriticalCaptureChance(300, criticalPolicy, criticalChance) || criticalChance != 318) return 607;
    criticalPolicy.freshStartChallenge = true;
    if (!pokemonCriticalCaptureChance(255, criticalPolicy, criticalChance) || criticalChance != 0) return 608;
    criticalPolicy.freshStartChallenge = false;
    auto criticalRng = rng;
    auto expectedCriticalRng = rng;
    expectedCriticalRng.randSeedInt(256);
    expectedCriticalRng.randSeedInt(65536);
    PokemonCaptureEvent criticalEvent{};
    auto unresolvedCriticalPolicy = criticalPolicy;
    unresolvedCriticalPolicy.resolved = false;
    if (pokemonCriticalCaptureChance(255, unresolvedCriticalPolicy, criticalChance)) return 609;
    // Chance 318/256 guarantees selecting the single-check branch.
    // Choose a real high-rate species explicitly for guaranteed critical branch.
    auto highRateTarget = targetState;
    for (const auto& row : PokerogueContent::kSpeciesCatchProfiles)
        if (row.catchRate == 255) { highRateTarget.speciesDex = row.speciesDex; break; }
    highRateTarget.hp = 1;
    criticalRng = rng;
    if (!executeCaptureAttempt(highRateTarget, PokeballType::RogueBall, false, false, false, false,
            criticalRng, criticalEvent, &criticalPolicy) || !criticalEvent.isCritical ||
        criticalEvent.shakeCount != 1 || criticalRng.randSeedUint32() != expectedCriticalRng.randSeedUint32()) return 610;

    // Master ball guaranteed catch
    if (!executeCaptureAttempt(targetState, PokeballType::MasterBall, false, false, false, false, rng, outEvent) ||
        !outEvent.caught || outEvent.shakeCount != 3) return 143;

    // Runtime-level tests:
    FirstRunRuntime game(1);
    if (game.pokeballCount(PokeballType::Pokeball) != 5) return 144;
    if (game.pokeballCount(PokeballType::GreatBall) != 0) return 145;
    if (game.pokeballCount(PokeballType::MasterBall) != 0) return 146;

    // Capture blocked in trainer battle (wave 5)
    NativeRunSave trainerSave{};
    game.captureNativeRunSave(trainerSave);
    trainerSave.wave = 5;
    trainerSave.stage = NativeSaveStage::BattleActive;
    if (game.restoreNativeRunSave(trainerSave)) {
        if (game.throwPokeball(PokeballType::Pokeball)) return 147;
        if (game.pokeballCount(PokeballType::Pokeball) != 5) return 148;
    }

    // Capture blocked against Wave 200 final boss
    FirstRunRuntime bossGame(1);
    NativeRunSave bossSave{};
    bossGame.captureNativeRunSave(bossSave);
    bossSave.wave = 200;
    bossSave.playerLevel = 100;
    bossSave.stage = NativeSaveStage::BattleActive;
    std::strcpy(bossSave.biomeId, "end");
    if (bossGame.restoreNativeRunSave(bossSave)) {
        if (bossGame.throwPokeball(PokeballType::Pokeball)) return 149;
    }

    // Wild catch test with standard Poké Ball on weakened wild mon:
    FirstRunRuntime wildGame(1);
    NativeRunSave wildSave{};
    if (!captureActiveTestCheckpoint(wildGame, wildSave)) return 150;
    wildSave.wave = 1;
    wildSave.stage = NativeSaveStage::BattleActive;
    wildSave.encounterDex = wildGame.presentation().enemy.dex;
    wildSave.enemyHp = 1;
    if (!wildGame.restoreNativeRunSave(wildSave)) return 150;
    const uint8_t initialPartyCount = wildGame.playerPartyCount();
    if (initialPartyCount != 1) return 151;
    auto heldCaptureGame = wildGame;
    NativeHeldModifierInstance captureHeld{};
    const uint32_t capturePid = wildGame.presentation().enemy.battleState.pokemonId;
    if (initializeHeldModifierInstance("LEFTOVERS", capturePid, 1, true, nullptr, captureHeld) !=
            HeldModifierStorageResult::Ok || !heldCaptureGame.restoreHeldModifierInventory(&captureHeld, 1) ||
        !heldCaptureGame.throwPokeball(PokeballType::Pokeball) || heldCaptureGame.playerPartyCount() != 2 ||
        heldCaptureGame.playerPartyMember(1)->battleState.pokemonId != capturePid ||
        heldCaptureGame.heldModifierCount() != 1 || heldCaptureGame.heldModifier(0)->ownerPokemonId != capturePid)
        return 441;
    if (!wildGame.throwPokeball(PokeballType::Pokeball)) return 152;
    if (wildGame.pokeballCount(PokeballType::Pokeball) != 4) return 153;
    if (!wildGame.battleFinished() || !wildGame.playerWon()) return 154;
    if (wildGame.playerPartyCount() != 2) return 155;
    if (wildGame.playerPartyMember(1)->dex != wildSave.encounterDex) return 156;
    if (wildGame.playerPartyMember(1)->battleState.hp != 1) return 224;
    if (wildGame.presentation().enemy.battleState.hp != 0) return 225;

    return 0;
}

static int checkPlayerPartyManagementAndSwitching() {
    using namespace Pokerogue3DS;
    FirstRunRuntime game(1);
    if (game.playerPartyCount() != 1) return 160;
    if (game.activePlayerPartyIndex() != 0) return 161;
    if (game.playerPartyMember(0) == nullptr) return 162;
    if (game.playerPartyMember(1) != nullptr) return 163;
    if (game.playerPartyDefeated()) return 164;

    // Invalid switch attempts:
    if (game.switchPlayerPokemon(0)) return 165; // Already active
    if (game.switchPlayerPokemon(1)) return 166; // Index out of bounds

    // Setup wild capture to grow party to 2:
    NativeRunSave wildSave{};
    if (!captureActiveTestCheckpoint(game, wildSave)) return 167;
    wildSave.wave = 1;
    wildSave.stage = NativeSaveStage::BattleActive;
    wildSave.encounterDex = game.presentation().enemy.dex;
    wildSave.enemyHp = 1;
    if (!game.restoreNativeRunSave(wildSave)) return 167;
    PokemonFriendshipPolicy captureFriendshipPolicy{};
    captureFriendshipPolicy.resolved = true;
    captureFriendshipPolicy.candyMultiplier = PokerogueContent::kClassicCandyFriendshipMultiplier;
    if (!game.restoreStarterCandyProfile(nullptr, 0, 0, captureFriendshipPolicy)) return 589;
    FirstRunRuntime freshProfileGame(1);
    if (!freshProfileGame.initializeFreshStarterProfile(captureFriendshipPolicy) ||
        !freshProfileGame.starterProfileReady() ||
        freshProfileGame.initializeFreshStarterProfile(captureFriendshipPolicy)) return 613;
    uint32_t expectedFreshCaught = 0;
    for (const auto& species : PokerogueContent::kSpecies) {
        if (species.freshProfileStarter) {
            ++expectedFreshCaught;
            if (!freshProfileGame.hasCaughtSpecies(species.dex)) return 614;
        }
    }
    if (!expectedFreshCaught || freshProfileGame.caughtSpeciesCount() != expectedFreshCaught) return 615;
    for (size_t i = 0; i < freshProfileGame.starterProfileCount(); ++i) {
        const auto& entry = freshProfileGame.starterProfileRecords()[i];
        const auto* species = PokerogueContent::findSpeciesByDex(entry.speciesDex);
        if (!species || !species->freshProfileStarter) continue;
        PokemonNature nature = PokemonNature::Unspecified;
        if (pokemonFreshProfileNature(entry.speciesDex, nature) != PokemonFreshProfileResult::Ok ||
            entry.natureAttr != (1u << (static_cast<uint8_t>(nature) + 1)) || entry.abilityAttr != 1 ||
            entry.genderAttr != 12) return 673;
        for (uint8_t iv : entry.dexIvs) if (iv != 15) return 674;
        auto merged = entry;
        merged.dexIvs[0] = 31;
        merged.abilityAttr |= 4;
        merged.natureAttr |= 1u << 25;
        const uint32_t mergedNatures = merged.natureAttr;
        if (!seedNativeFreshStarterDexMetadata(merged) || merged.dexIvs[0] != 31 ||
            merged.abilityAttr != 5 || merged.natureAttr != mergedNatures) return 675;
    }

    if (freshProfileGame.presentationStage() != NativeSaveStage::RunSetup ||
        game.presentationStage() != NativeSaveStage::BattleActive) return 627;
    uint64_t capturedSourceForm = 0;
    if (pokemonObservedDexFormAttr(game.presentation().enemy.dex, game.presentation().enemy.actor,
            capturedSourceForm) != PokemonObservedFormResult::Ok) return 680;
    const auto friendshipBeforeCapture = game.presentation().player.battleState.friendship;
    if (!game.throwPokeball(PokeballType::Pokeball)) return 168;
    const auto* captureParticipantRoot = pokemonRootSpecies(game.presentation().player.dex);
    const NativeStarterCandyRecord* captureParticipantRecord = nullptr;
    for (size_t i = 0; i < game.starterProfileCount(); ++i)
        if (captureParticipantRoot && game.starterProfileRecords()[i].speciesDex == captureParticipantRoot->dex)
            captureParticipantRecord = &game.starterProfileRecords()[i];
    if (game.presentation().player.battleState.friendship != friendshipBeforeCapture +
            PokerogueContent::kFriendshipGainFromBattle || !captureParticipantRecord ||
        captureParticipantRecord->friendship != PokerogueContent::kFriendshipGainFromBattle *
            PokerogueContent::kClassicCandyFriendshipMultiplier || !game.hasCaughtSpecies(wildSave.encounterDex)) return 590;
    uint16_t caughtChainDex = wildSave.encounterDex;
    while (caughtChainDex) {
        const auto* caughtChainSpecies = PokerogueContent::findSpeciesByDex(caughtChainDex);
        if (!caughtChainSpecies || !game.hasCaughtSpecies(caughtChainDex)) return 611;
        if (capturedSourceForm == uint64_t(128)) {
            uint64_t allowed = 0;
            if (pokemonObtainableFormMask(caughtChainDex, allowed) != PokemonFormUnlockMaskResult::Ok) return 681;
            const NativeStarterCandyRecord* entry = nullptr;
            for (size_t i = 0; i < game.starterProfileCount(); ++i)
                if (game.starterProfileRecords()[i].speciesDex == caughtChainDex) entry = &game.starterProfileRecords()[i];
            if (!entry || ((entry->unlockedFormAttr & uint64_t(128)) != (allowed & uint64_t(128)))) return 682;
        }

        caughtChainDex = caughtChainSpecies->prevolutionDex;
    }
    const auto* caughtCandyRoot = pokemonRootSpecies(wildSave.encounterDex);
    bool foundCaptureCandy = false;
    for (size_t i = 0; i < game.starterProfileCount(); ++i)
        if (caughtCandyRoot && game.starterProfileRecords()[i].speciesDex == caughtCandyRoot->dex)
            foundCaptureCandy = game.starterProfileRecords()[i].candyCount == 1;
    if (!foundCaptureCandy) return 616;
    NativeStarterCandyRecord cappedCaptureCandy{caughtCandyRoot->dex,
        PokerogueContent::kMaxStarterCandyCount, 0, true};
    StarterCandyAwardEvent cappedCaptureEvent{};
    if (applyNativeStarterCandyAward(cappedCaptureCandy, 2, cappedCaptureEvent) != StarterCandyApplyResult::Applied ||
        cappedCaptureEvent.appliedAward || cappedCaptureEvent.requestedAward != 2 ||
        cappedCaptureCandy.candyCount != PokerogueContent::kMaxStarterCandyCount) return 617;
    uint16_t unlockableStarter = 0;
    for (const auto& species : PokerogueContent::kSpecies)
        if (species.starterEligible && !species.freshProfileStarter &&
            (!species.firstFormId || !*species.firstFormId)) { unlockableStarter = species.dex; break; }
    FirstRunRuntime unlockedStarterGame(1);
    if (!unlockableStarter || unlockedStarterGame.starterUnlocked(unlockableStarter) ||
        unlockedStarterGame.restoreSetup(1, unlockableStarter)) return 618;
    NativeStarterCandyRecord unlockedRecord{unlockableStarter, 0, 0, true};
    unlockedRecord.natureAttr = 1u << 1;
    unlockedRecord.abilityAttr = 1;
    const auto* unlockSpecies = PokerogueContent::findSpeciesByDex(unlockableStarter);
    unlockedRecord.genderAttr = unlockSpecies->malePercentTenths == 65534 ? 0 :
        unlockSpecies->malePercentTenths == 0 ? 8 : 4;
    for (uint8_t& iv : unlockedRecord.dexIvs) iv = 23;

    if (!unlockedStarterGame.restoreStarterCandyProfile(&unlockedRecord, 1, 0, captureFriendshipPolicy) ||
        !unlockedStarterGame.starterUnlocked(unlockableStarter) ||
        !unlockedStarterGame.restoreSetup(1, unlockableStarter)) return 619;
    if (unlockedStarterGame.presentation().player.actor.nature != PokemonNature::Hardy ||
        unlockedStarterGame.presentation().player.battleState.ivs[0] != 23 ||
        unlockedStarterGame.presentation().player.battleState.abilityId != unlockSpecies->ability1) return 679;
    NativeRunSave unlockedSetup{};
    unlockedStarterGame.captureNativeRunSave(unlockedSetup);
    FirstRunRuntime unlockedRestored(1);
    if (unlockedRestored.restoreNativeRunSave(unlockedSetup) ||
        !unlockedRestored.restoreNativeRunSave(unlockedSetup, &unlockedRecord, 1, &captureFriendshipPolicy) ||
        unlockedRestored.run().starterDex != unlockableStarter) return 620;
    const uint16_t duplicateStarters[] = {unlockableStarter, unlockableStarter};
    uint16_t selectionValue = 999;
    if (classicStarterSelectionValue(duplicateStarters, 2, selectionValue) !=
            PokemonStarterSelectionResult::DuplicateSpecies || selectionValue != 999 ||
        unlockedStarterGame.starterSelectionAllowed(duplicateStarters, 2)) return 621;
    uint16_t costlyStarters[6]{};
    size_t costlyCount = 0;
    uint16_t costlyTotal = 0;
    for (const auto& species : PokerogueContent::kSpecies)
        if (species.starterEligible && species.starterCost >= 3 && costlyCount < 6 &&
            costlyTotal <= kClassicStarterValueLimit) {
            costlyStarters[costlyCount++] = species.dex;
            costlyTotal += species.starterCost;
        }
    if (costlyTotal <= kClassicStarterValueLimit ||
        classicStarterSelectionValue(costlyStarters, costlyCount, selectionValue) !=
            PokemonStarterSelectionResult::OverBudget || selectionValue != 999) return 622;
    FirstRunRuntime restartCaptureGame = game;
    const auto restartProfileCount = restartCaptureGame.starterProfileCount();
    const auto restartCaughtCount = restartCaptureGame.caughtSpeciesCount();
    const uint32_t restartSeed = restartCaptureGame.run().seed;
    if (restartCaptureGame.restoreSetup(0, restartCaptureGame.run().starterDex) ||
        restartCaptureGame.run().seed != restartSeed ||
        restartCaptureGame.starterProfileCount() != restartProfileCount ||
        restartCaptureGame.caughtSpeciesCount() != restartCaughtCount) return 623;
    if (!restartCaptureGame.restoreSetup(123, restartCaptureGame.run().starterDex) ||
        restartCaptureGame.runStarted() || restartCaptureGame.run().wave != 1 ||
        restartCaptureGame.starterProfileCount() != restartProfileCount ||
        restartCaptureGame.caughtSpeciesCount() != restartCaughtCount) return 624;
    for (size_t i = 0; i < restartProfileCount; ++i) {
        const auto& before = game.starterProfileRecords()[i];
        const auto& after = restartCaptureGame.starterProfileRecords()[i];
        if (before.speciesDex != after.speciesDex || before.caught != after.caught ||
            before.candyCount != after.candyCount || before.friendship != after.friendship) return 625;
    }
    uint8_t coveredStarterCosts = 0;
    for (const auto& species : PokerogueContent::kSpecies) {
        if (!species.starterEligible || species.starterCost < 1 || species.starterCost > 3) continue;
        uint16_t reducedCost = 999;
        if (!pokemonStarterCostQuarterUnits(species.dex, 2, reducedCost)) return 628;
        const uint16_t expectedCost = species.starterCost == 1 ? 1 : species.starterCost == 2 ? 2 : 4;
        if (reducedCost != expectedCost) return 629;
        coveredStarterCosts |= static_cast<uint8_t>(1 << (species.starterCost - 1));
        reducedCost = 999;
        if (pokemonStarterCostQuarterUnits(species.dex, 3, reducedCost) || reducedCost != 999) return 630;
    }
    if (coveredStarterCosts != 7) return 631;
    NativeStarterCandyRecord reducedStarters[6]{};
    for (size_t i = 0; i < costlyCount; ++i) reducedStarters[i] = {costlyStarters[i], 0, 0, true, 2};
    for (size_t i = 0; i < costlyCount; ++i)
        for (size_t j = i + 1; j < costlyCount; ++j)
            if (reducedStarters[j].speciesDex < reducedStarters[i].speciesDex) {
                const auto swap = reducedStarters[i]; reducedStarters[i] = reducedStarters[j]; reducedStarters[j] = swap;
            }
    FirstRunRuntime reducedStarterGame(1);
    if (!reducedStarterGame.restoreStarterCandyProfile(reducedStarters, costlyCount, 0, captureFriendshipPolicy)) return 632;
    uint16_t reducedTotal = 0;
    for (size_t i = 0; i < costlyCount; ++i) {
        uint16_t cost = 0;
        if (reducedStarterGame.starterCostReduction(costlyStarters[i]) != 2 ||
            !pokemonStarterCostQuarterUnits(costlyStarters[i], 2, cost)) return 633;
        reducedTotal += cost;
    }
    if (reducedStarterGame.starterSelectionAllowed(costlyStarters, costlyCount) !=
            (reducedTotal <= kClassicStarterValueLimit * 4) ||
        reducedStarterGame.starterSelectionAllowed(duplicateStarters, 2)) return 634;

    char caughtRuntimeProfile[Pokerogue3DS::kStarterCandyProfileMaxBytes]{};
    size_t caughtRuntimeBytes = 0;
    if (encodeNativeStarterCandyProfile(game.starterProfileRecords(), game.starterProfileCount(), 1,
            PokerogueContent::kContentHash, PokerogueContent::kMaxStarterCandyCount,
            caughtRuntimeProfile, sizeof(caughtRuntimeProfile), caughtRuntimeBytes) != NativeSaveResult::Ok) return 612;
    if (game.playerPartyCount() != 2) return 169;
    const uint16_t caughtDex = game.playerPartyMember(1)->dex;
    if (caughtDex != wildSave.encounterDex) return 170;

    // Advance turn & skip reward into wave 2:
    if (!game.advanceBattleTurn()) return 171;
    if (!game.skipVictoryReward()) return 172;
    if (game.run().wave != 2) return 173;
    if (game.playerPartyCount() != 2) return 174;
    if (game.activePlayerPartyIndex() != 0) return 175;

    NativeRunSave capturedPartySave{};
    game.captureNativeRunSave(capturedPartySave);
    if (capturedPartySave.playerPartyCount != 2 || capturedPartySave.activePlayerMember != 0 ||
        validateNativeRunSave(capturedPartySave, PokerogueContent::kContentHash) != NativeSaveResult::Ok)
        return 290;
    FirstRunRuntime restoredPartyGame(7);
    if (!restoredPartyGame.restoreNativeRunSave(capturedPartySave) || restoredPartyGame.playerPartyCount() != 2 ||
        restoredPartyGame.run().wave != 2) return 291;
    for (uint8_t member = 0; member < 2; ++member) {
        const auto* before = game.playerPartyMember(member);
        const auto* after = restoredPartyGame.playerPartyMember(member);
        if (!before || !after || before->dex != after->dex || before->level != after->level ||
            before->totalExperience != after->totalExperience ||
            before->battleState.hp != after->battleState.hp ||
            before->actor.pokemonId != after->actor.pokemonId) return 292;
        for (uint8_t slot = 0; slot < before->battleState.moveCount; ++slot)
            if (before->battleState.moves[slot].moveId != after->battleState.moves[slot].moveId ||
                before->battleState.moves[slot].pp != after->battleState.moves[slot].pp) return 293;
    }
    NativeRunSave recapturedPartySave{};
    restoredPartyGame.captureNativeRunSave(recapturedPartySave);
    if (recapturedPartySave.playerPartyCount != 2 ||
        recapturedPartySave.playerParty[1].experience != capturedPartySave.playerParty[1].experience)
        return 294;
    // Full-party successful capture pauses for an explicit replacement or decline.
    NativeRunSave fullPartySave = capturedPartySave;
    fullPartySave.playerPartyCount = 6;
    for (uint8_t member = 2; member < 6; ++member) {
        fullPartySave.playerParty[member] = fullPartySave.playerParty[1];
        uint32_t id = member;
        bool collision = true;
        while (collision) {
            collision = false;
            for (uint8_t prior = 0; prior < member; ++prior)
                if (fullPartySave.playerParty[prior].pokemonId == id) { ++id; collision = true; break; }
        }
        fullPartySave.playerParty[member].pokemonId = id;
        fullPartySave.playerParty[member].ivsDerivedFromId = false; // Preserve explicit IVs for the new test identity.
    }
    fullPartySave.enemyHp = 1;
    fullPartySave.pokeballCounts[static_cast<uint8_t>(PokeballType::MasterBall)] = 1;
    FirstRunRuntime fullPartyGame(1);
    if (!fullPartyGame.restoreNativeRunSave(fullPartySave)) return 586;
    if (!fullPartyGame.throwPokeball(PokeballType::MasterBall) ||
        !fullPartyGame.capturePartyChoicePending() || fullPartyGame.playerPartyCount() != 6 ||
        fullPartyGame.pokeballCount(PokeballType::MasterBall) != 0 ||
        fullPartyGame.presentation().enemy.battleState.hp != 1) return 587;
    const auto pendingActor = fullPartyGame.pendingCapturedPokemon();
    NativeRunSave intermediate{};
    if (fullPartyGame.captureNativeRunSave(intermediate) != NativeSaveResult::UnsupportedStage) return 626;
    if (validateNativeRunSave(intermediate, PokerogueContent::kContentHash) == NativeSaveResult::Ok ||
        fullPartyGame.resolveCapturePartyChoice(6) || !fullPartyGame.capturePartyChoicePending() ||
        fullPartyGame.switchPlayerPokemon(1) || fullPartyGame.throwPokeball() ||
        fullPartyGame.battleInputSupported()) return 588;
    if (!fullPartyGame.resolveCapturePartyChoice(1) || fullPartyGame.capturePartyChoicePending() ||
        fullPartyGame.playerPartyCount() != 6 ||
        fullPartyGame.playerPartyMember(1)->battleState.pokemonId != pendingActor.battleState.pokemonId ||
        fullPartyGame.playerPartyMember(1)->dex != pendingActor.dex ||
        !fullPartyGame.playerWon() || fullPartyGame.resolveCapturePartyChoice(1)) return 591;
    // Replacement checkpoint must be restorable without a released identity
    // leaking into participation or an extra capture consuming another ball.
    NativeRunSave replacementCheckpoint{};
    fullPartyGame.captureNativeRunSave(replacementCheckpoint);
    if (validateNativeRunSave(replacementCheckpoint, PokerogueContent::kContentHash) != NativeSaveResult::Ok ||
        replacementCheckpoint.playerParty[1].pokemonId != pendingActor.battleState.pokemonId ||
        replacementCheckpoint.playerParty[1].hp != pendingActor.battleState.hp ||
        replacementCheckpoint.pokeballCounts[static_cast<uint8_t>(PokeballType::MasterBall)] != 0) return 599;
    for (uint8_t slot = 0; slot < pendingActor.battleState.moveCount; ++slot)
        if (fullPartyGame.playerPartyMember(1)->battleState.moves[slot].pp !=
                pendingActor.battleState.moves[slot].pp) return 600;
    for (uint8_t participant = 0; participant < replacementCheckpoint.participantCount; ++participant)
        if (replacementCheckpoint.participantIds[participant] == fullPartySave.playerParty[1].pokemonId)
            return 601;
    FirstRunRuntime replacementRestored(1);
    if (!replacementRestored.restoreNativeRunSave(replacementCheckpoint) ||
        replacementRestored.playerPartyCount() != 6 ||
        replacementRestored.playerPartyMember(1)->battleState.pokemonId != pendingActor.battleState.pokemonId)
        return 602;
    FirstRunRuntime declinedCapture(1);
    if (!declinedCapture.restoreNativeRunSave(fullPartySave) ||
        !declinedCapture.throwPokeball(PokeballType::MasterBall) ||
        !declinedCapture.resolveCapturePartyChoice(-1) || declinedCapture.capturePartyChoicePending() ||
        !declinedCapture.playerWon() || declinedCapture.playerPartyCount() != 6) return 592;
    for (uint8_t member = 0; member < 6; ++member)
        if (declinedCapture.playerPartyMember(member)->battleState.pokemonId !=
            fullPartySave.playerParty[member].pokemonId) return 593;
    // All six have participated; replacing the active must not require a
    // seventh history slot or grant the incoming actor pre-capture EXP.
    NativeRunSave allParticipantsCapture = fullPartySave;
    allParticipantsCapture.participantHistoryResolved = true;
    allParticipantsCapture.participantCount = 6;
    for (uint8_t member = 0; member < 6; ++member) {
        uint8_t position = member;
        const uint32_t id = allParticipantsCapture.playerParty[member].pokemonId;
        while (position && allParticipantsCapture.participantIds[position - 1] > id) {
            allParticipantsCapture.participantIds[position] = allParticipantsCapture.participantIds[position - 1];
            --position;
        }
        allParticipantsCapture.participantIds[position] = id;
    }
    FirstRunRuntime activeCapture(1);
    if (!activeCapture.restoreNativeRunSave(allParticipantsCapture) ||
        !activeCapture.throwPokeball(PokeballType::MasterBall)) return 597;
    const auto incomingCapture = activeCapture.pendingCapturedPokemon();
    if (!activeCapture.resolveCapturePartyChoice(allParticipantsCapture.activePlayerMember) ||
        activeCapture.presentation().player.battleState.pokemonId != incomingCapture.battleState.pokemonId ||
        activeCapture.presentation().player.totalExperience != incomingCapture.totalExperience ||
        !activeCapture.playerWon()) return 598;
    // Replacement removes outgoing held items and retains captured-owner items.
    FirstRunRuntime heldReplacement(1);
    if (!heldReplacement.restoreNativeRunSave(fullPartySave)) return 594;
    NativeHeldModifierInstance replacementItems[2]{};
    const auto capturedOwner = heldReplacement.presentation().enemy.battleState.pokemonId;
    if (initializeHeldModifierInstance("LEFTOVERS", fullPartySave.playerParty[1].pokemonId, 1, true,
            nullptr, replacementItems[0]) != HeldModifierStorageResult::Ok ||
        initializeHeldModifierInstance("LEFTOVERS", capturedOwner, 1, true,
            nullptr, replacementItems[1]) != HeldModifierStorageResult::Ok ||
        !heldReplacement.restoreHeldModifierInventory(replacementItems, 2) ||
        !heldReplacement.throwPokeball(PokeballType::MasterBall) ||
        !heldReplacement.resolveCapturePartyChoice(1) || heldReplacement.heldModifierCount() != 1 ||
        heldReplacement.heldModifier(0)->ownerPokemonId != capturedOwner) return 595;
    FirstRunRuntime heldDecline(1);
    if (!heldDecline.restoreNativeRunSave(fullPartySave) ||
        !heldDecline.restoreHeldModifierInventory(replacementItems, 2) ||
        !heldDecline.throwPokeball(PokeballType::MasterBall) ||
        !heldDecline.resolveCapturePartyChoice(-1) || heldDecline.heldModifierCount() != 1 ||
        heldDecline.heldModifier(0)->ownerPokemonId != fullPartySave.playerParty[1].pokemonId) return 596;
    // Real reconstructed enemy and captured party: both identities share EXP.
    NativeRunSave sharedVictory = capturedPartySave;
    sharedVictory.stage = NativeSaveStage::BattleWon;
    sharedVictory.enemyHp = 0;
    for (uint8_t member = 0; member < sharedVictory.trainerPartyCount; ++member)
        sharedVictory.trainerParty[member].hp = 0;
    sharedVictory.participantHistoryResolved = true;
    sharedVictory.participantCount = 2;
    const auto id0 = sharedVictory.playerParty[0].pokemonId;
    const auto id1 = sharedVictory.playerParty[1].pokemonId;
    sharedVictory.participantIds[0] = id0 < id1 ? id0 : id1;
    sharedVictory.participantIds[1] = id0 < id1 ? id1 : id0;
    const auto& realEnemy = restoredPartyGame.presentation().enemy;
    const auto* realEnemySpecies = PokerogueContent::findSpeciesByDex(realEnemy.dex);
    const auto* realEnemyForm = PokerogueContent::findFormById(realEnemy.formId);
    double realExp = 0.0;
    if (!realEnemySpecies || pokemonExperienceForDefeat(*realEnemySpecies, realEnemy.level, realExp,
            realEnemyForm) != PokemonExperienceResult::Ok) return 522;
    PokemonExperienceProgress expectedPartyExp[2]{};
    for (uint8_t member = 0; member < 2; ++member) {
        const auto& saved = sharedVictory.playerParty[member];
        const auto* species = PokerogueContent::findSpeciesByDex(saved.speciesDex);
        PokemonParticipantExperiencePolicy policy{};
        policy.resolved = true; policy.participantCount = 2; policy.participated = true;
        policy.eligible = saved.hp && saved.level < classicExperienceLevelCap(sharedVictory.wave);
        uint32_t award = 0;
        if (!species || pokemonParticipantExperience(realExp, sharedVictory.trainerPartyCount != 0,
                policy, award) != PokemonExperienceResult::Ok ||
            applyPokemonExperience(species->growthRate, saved.level, saved.experience, award,
                classicExperienceLevelCap(sharedVictory.wave), expectedPartyExp[member]) !=
                PokemonExperienceResult::Ok) return 523;
    }
    FirstRunRuntime sharedExpGame(1);
    if (!sharedExpGame.restoreNativeRunSave(sharedVictory) || !sharedExpGame.advanceBattleTurn() ||
        !sharedExpGame.experienceGranted()) return 524;
    for (uint8_t member = 0; member < 2; ++member) {
        const auto* actor = sharedExpGame.playerPartyMember(member);
        if (!actor || actor->level != expectedPartyExp[member].level ||
            actor->totalExperience != expectedPartyExp[member].totalExperience) return 525;
    }
    for (unsigned decision = 0; decision < 512 &&
            (sharedExpGame.moveLearningPending() || sharedExpGame.evolutionPending()); ++decision)
        if (!sharedExpGame.skipVictoryReward()) return 526;
    if (sharedExpGame.moveLearningPending() || sharedExpGame.evolutionPending()) return 527;
    NativeRunSave sharedExpCheckpoint{};
    sharedExpGame.captureNativeRunSave(sharedExpCheckpoint);
    FirstRunRuntime sharedExpReloaded(2);
    if (sharedExpCheckpoint.stage != NativeSaveStage::ExperienceGranted ||
        !sharedExpReloaded.restoreNativeRunSave(sharedExpCheckpoint)) return 528;
    for (uint8_t member = 0; member < 2; ++member)
        if (sharedExpReloaded.playerPartyMember(member)->totalExperience !=
                expectedPartyExp[member].totalExperience) return 529;
    NativeRunSave singleVictory = sharedVictory;
    singleVictory.participantCount = 1;
    singleVictory.participantIds[0] = singleVictory.playerParty[0].pokemonId;
    singleVictory.participantIds[1] = 0;
    FirstRunRuntime singleExpGame(3);
    if (!singleExpGame.restoreNativeRunSave(singleVictory) || !singleExpGame.advanceBattleTurn() ||
        singleExpGame.playerPartyMember(1)->totalExperience != singleVictory.playerParty[1].experience)
        return 530;
    auto unknownVictory = sharedVictory;
    unknownVictory.participantHistoryResolved = false;
    unknownVictory.participantCount = 0;
    for (auto& id : unknownVictory.participantIds) id = 0;
    FirstRunRuntime unknownExpGame(4);
    if (!unknownExpGame.restoreNativeRunSave(unknownVictory) || unknownExpGame.advanceBattleTurn() ||
        unknownExpGame.playerPartyMember(0)->totalExperience != unknownVictory.playerParty[0].experience ||
        unknownExpGame.playerPartyMember(1)->totalExperience != unknownVictory.playerParty[1].experience)
        return 531;
    auto friendshipVictory = sharedVictory;
    for (uint8_t member = 0; member < 2; ++member) {
        friendshipVictory.playerParty[member].friendshipResolved = true;
        friendshipVictory.playerParty[member].friendship = 70;
    }
    FirstRunRuntime friendshipVictoryGame(6);
    PokemonFriendshipPolicy classicFriendshipPolicy{};
    classicFriendshipPolicy.resolved = true;
    classicFriendshipPolicy.candyMultiplier = PokerogueContent::kClassicCandyFriendshipMultiplier;
    if (!friendshipVictoryGame.restoreNativeRunSave(friendshipVictory) ||
        !friendshipVictoryGame.restoreStarterCandyProfile(nullptr, 0, 0, classicFriendshipPolicy) ||
        !friendshipVictoryGame.advanceBattleTurn()) return 541;
    uint32_t totalCandyFriendship = 0;
    for (size_t record = 0; record < friendshipVictoryGame.starterProfileCount(); ++record)
        totalCandyFriendship += friendshipVictoryGame.starterProfileRecords()[record].friendship;
    if (friendshipVictoryGame.playerPartyMember(0)->battleState.friendship != 73 ||
        friendshipVictoryGame.playerPartyMember(1)->battleState.friendship != 73 ||
        totalCandyFriendship != 18) return 542;
    if (!friendshipVictoryGame.restoreNativeRunSave(friendshipVictory) ||
        friendshipVictoryGame.starterProfileReady() || friendshipVictoryGame.starterProfileCount() ||
        friendshipVictoryGame.playerPartyMember(0)->battleState.friendship != 70) return 544;
    static ProgressMemoryStorage progressRunDisk, progressProfileDisk;
    static char progressScratch[2 * kStarterCandyProfileMaxBytes]{};
    static NativeStarterCandyRecord progressStaging[PokerogueContent::kSpeciesCount]{};
    NativeRunSaveStore progressRuns(progressRunDisk);
    NativeStarterCandyStore progressProfiles(progressProfileDisk, progressScratch, sizeof(progressScratch));
    NativeProgressStore progressStore(progressRuns, progressProfiles);
    FirstRunRuntime durableFriendshipGame(8);
    if (!durableFriendshipGame.restoreNativeRunSave(friendshipVictory, nullptr, 0, &classicFriendshipPolicy) ||
        !durableFriendshipGame.advanceBattleTurn()) return 545;
    for (unsigned decision = 0; decision < 512 &&
            (durableFriendshipGame.moveLearningPending() || durableFriendshipGame.evolutionPending()); ++decision)
        if (!durableFriendshipGame.skipVictoryReward()) return 546;
    if (durableFriendshipGame.saveNativeProgress(progressStore) != NativeSaveResult::Ok) return 547;
    NativeRunSave durableRun{};
    FirstRunRuntime durableReloaded(9);
    if (durableReloaded.loadNativeProgress(progressRuns, progressProfiles, progressStaging,
            PokerogueContent::kSpeciesCount, classicFriendshipPolicy, &durableRun) != NativeSaveResult::Ok ||
        durableRun.starterProfileGeneration != 1 || !durableReloaded.starterProfileReady() ||
        durableReloaded.playerPartyMember(0)->battleState.friendship != 73 ||
        durableReloaded.playerPartyMember(1)->battleState.friendship != 73) return 548;
    uint32_t durableCandyGain = 0;
    for (size_t i = 0; i < durableReloaded.starterProfileCount(); ++i)
        durableCandyGain += durableReloaded.starterProfileRecords()[i].friendship;
    if (durableCandyGain != 18 || !durableReloaded.starterProfileCount()) return 549;
    ++progressStaging[0].friendship;
    if (!durableReloaded.restoreStarterCandyProfile(progressStaging, durableReloaded.starterProfileCount(),
            durableRun.starterProfileGeneration, classicFriendshipPolicy)) return 550;
    progressRunDisk.interrupt = true;
    if (durableReloaded.saveNativeProgress(progressStore) != NativeSaveResult::IoError) return 551;
    progressRunDisk.interrupt = false;
    if (durableReloaded.loadNativeProgress(progressRuns, progressProfiles, progressStaging,
            PokerogueContent::kSpeciesCount, classicFriendshipPolicy, &durableRun) != NativeSaveResult::Ok ||
        durableRun.starterProfileGeneration != 1) return 552;
    durableCandyGain = 0;
    for (size_t i = 0; i < durableReloaded.starterProfileCount(); ++i)
        durableCandyGain += durableReloaded.starterProfileRecords()[i].friendship;
    if (durableCandyGain != 18 || progressStore.exportLatest(PokerogueContent::kContentHash) !=
            NativeSaveResult::Ok) return 553;
    size_t exportedProfileCount = 0;
    uint32_t exportedProfileGeneration = 0;
    if (inspectNativeStarterCandyProfile(progressProfileDisk.exported, progressProfileDisk.exportSize,
            PokerogueContent::kContentHash, PokerogueContent::kMaxStarterCandyCount,
            exportedProfileCount, exportedProfileGeneration) != NativeSaveResult::Ok ||
        exportedProfileGeneration != 1 || progressRuns.importExport(PokerogueContent::kContentHash) !=
            NativeSaveResult::InvalidRecord) return 554;
    auto blockedFriendshipVictory = friendshipVictory;
    blockedFriendshipVictory.playerParty[1].friendship = 254;
    FirstRunRuntime blockedFriendshipGame(7);
    if (!blockedFriendshipGame.restoreNativeRunSave(blockedFriendshipVictory) ||
        !blockedFriendshipGame.restoreStarterCandyProfile(nullptr, 0, 0, classicFriendshipPolicy) ||
        blockedFriendshipGame.advanceBattleTurn() || blockedFriendshipGame.starterProfileCount() ||
        blockedFriendshipGame.playerPartyMember(0)->battleState.friendship != 70 ||
        blockedFriendshipGame.playerPartyMember(1)->battleState.friendship != 254) return 543;
    // Force a genuine reserve learnset boundary, retaining its captured identity.
    NativeRunSave learningVictory = sharedVictory;
    const auto* reserveSpecies = PokerogueContent::findSpeciesByDex(learningVictory.playerParty[1].speciesDex);
    const auto* reserveRows = reserveSpecies ? PokerogueContent::levelMovesFor(*reserveSpecies) : nullptr;
    uint16_t learningLevel = 0;
    uint16_t proposedMove = 0;
    for (uint16_t row = 0; reserveRows && row < reserveSpecies->learnsetCount; ++row) {
        const auto* move = PokerogueContent::findMoveById(reserveRows[row].moveId);
        if (reserveRows[row].level > 1 && reserveRows[row].level <= classicExperienceLevelCap(learningVictory.wave) &&
            move && !(move->upstreamFlags & PokerogueContent::MoveIsUnimplemented)) {
            learningLevel = reserveRows[row].level;
            proposedMove = move->id;
            break;
        }
    }
    if (!learningLevel) return 532;
    auto reserveLearningActor = restoredPartyGame.playerPartyMember(1)->battleState;
    if (!recalculatePokemonBattleLevel(reserveLearningActor, learningLevel - 1)) return 533;
    for (auto& move : reserveLearningActor.moves) move = {};
    reserveLearningActor.moveCount = 0;
    reserveLearningActor.pauseEvolutions = true;
    for (const auto& move : PokerogueContent::kMoves) {
        if (reserveLearningActor.moveCount == 4) break;
        if (!move.id || move.id == proposedMove || move.pp < 1 || move.pp > 255 ||
            (move.upstreamFlags & PokerogueContent::MoveIsUnimplemented)) continue;
        if (learnPokemonMoveAtSlot(reserveLearningActor, move.id, reserveLearningActor.moveCount) !=
                PokemonLearnMoveResult::Learned) return 534;
    }
    uint32_t nextReserveThreshold = 0;
    if (reserveLearningActor.moveCount != 4 || pokemonTotalExperienceForLevel(reserveSpecies->growthRate,
            learningLevel, nextReserveThreshold) != PokemonExperienceResult::Ok || !nextReserveThreshold ||
        !captureNativePokemonActorSave(reserveLearningActor, restoredPartyGame.playerPartyMember(1)->actor,
            nextReserveThreshold - 1, learningVictory.playerParty[1])) return 535;
    auto cappedActive = restoredPartyGame.presentation().player.battleState;
    const uint16_t learningCap = classicExperienceLevelCap(learningVictory.wave);
    uint32_t cappedActiveExp = 0;
    const auto* activeSpecies = PokerogueContent::findSpeciesByDex(cappedActive.speciesDex);
    if (!activeSpecies || !recalculatePokemonBattleLevel(cappedActive, learningCap) ||
        pokemonTotalExperienceForLevel(activeSpecies->growthRate, learningCap, cappedActiveExp) !=
            PokemonExperienceResult::Ok || !captureNativePokemonActorSave(cappedActive,
            restoredPartyGame.presentation().player.actor, cappedActiveExp, learningVictory.playerParty[0])) return 536;
    learningVictory.playerLevel = learningCap;
    learningVictory.playerExperience = cappedActiveExp;
    learningVictory.playerHp = cappedActive.hp;
    learningVictory.playerMoveCount = cappedActive.moveCount;
    for (uint8_t slot = 0; slot < 4; ++slot) {
        learningVictory.playerMoveIds[slot] = cappedActive.moves[slot].moveId;
        learningVictory.playerPp[slot] = cappedActive.moves[slot].pp;
    }
    FirstRunRuntime reserveLearningGame(5);
    if (!reserveLearningGame.restoreNativeRunSave(learningVictory) || !reserveLearningGame.advanceBattleTurn() ||
        !reserveLearningGame.moveLearningPending() || reserveLearningGame.progressionPartyIndex() != 1 ||
        reserveLearningGame.activePlayerPartyIndex() != 0) return 537;
    const uint16_t queuedMove = reserveLearningGame.pendingLearnMoveId();
    if (!reserveLearningGame.resolvePendingLearnMove(0) ||
        reserveLearningGame.playerPartyMember(1)->battleState.moves[0].moveId != queuedMove ||
        reserveLearningGame.presentation().player.battleState.moves[0].moveId != cappedActive.moves[0].moveId ||
        reserveLearningGame.presentation().player.totalExperience != cappedActiveExp) return 538;
    for (unsigned decision = 0; decision < 512 &&
            (reserveLearningGame.moveLearningPending() || reserveLearningGame.evolutionPending()); ++decision)
        if (!reserveLearningGame.skipVictoryReward()) return 539;
    if (reserveLearningGame.moveLearningPending() || reserveLearningGame.evolutionPending()) return 540;
    // Restore a captured actor as active without granting it the starter's EXP.
    NativeRunSave reserveActiveSave = capturedPartySave;
    reserveActiveSave.activePlayerMember = 1;
    const auto& activeReserve = reserveActiveSave.playerParty[1];
    reserveActiveSave.playerLevel = activeReserve.level;
    reserveActiveSave.playerExperience = activeReserve.experience;
    reserveActiveSave.playerHp = activeReserve.hp;
    reserveActiveSave.playerMoveCount = activeReserve.moveCount;
    for (uint8_t slot = 0; slot < 4; ++slot) {
        reserveActiveSave.playerMoveIds[slot] = activeReserve.moveIds[slot];
        reserveActiveSave.playerPp[slot] = activeReserve.pp[slot];
    }
    for (uint8_t stat = 0; stat < 7; ++stat)
        reserveActiveSave.playerStatStages[stat] = activeReserve.statStages[stat];
    if (!restoredPartyGame.restoreNativeRunSave(reserveActiveSave) ||
        restoredPartyGame.activePlayerPartyIndex() != 1 ||
        restoredPartyGame.presentation().player.dex != caughtDex ||
        restoredPartyGame.presentation().player.totalExperience != activeReserve.experience) return 295;
    auto invalidReserveSave = reserveActiveSave;
    const auto* invalidReserveSpecies = PokerogueContent::findSpeciesByDex(invalidReserveSave.playerParty[0].speciesDex);
    if (!invalidReserveSpecies || pokemonTotalExperienceForLevel(invalidReserveSpecies->growthRate,
            invalidReserveSave.playerParty[0].level + 1, invalidReserveSave.playerParty[0].experience) !=
            PokemonExperienceResult::Ok) return 296;
    if (validateNativeRunSave(invalidReserveSave, PokerogueContent::kContentHash) !=
            NativeSaveResult::InvalidRecord || restoredPartyGame.restoreNativeRunSave(invalidReserveSave) ||
        restoredPartyGame.presentation().player.dex != caughtDex) return 297;
    const auto starterExperience = game.presentation().player.totalExperience;
    const auto reserveExperience = game.playerPartyMember(1)->totalExperience;
    const auto* caughtSpecies = PokerogueContent::findSpeciesByDex(caughtDex);
    uint32_t expectedReserveExperience = 0;
    if (!caughtSpecies || pokemonTotalExperienceForLevel(caughtSpecies->growthRate,
            game.playerPartyMember(1)->level, expectedReserveExperience) != PokemonExperienceResult::Ok ||
        reserveExperience != expectedReserveExperience) return 221;

    // Capture retained 1 HP. A switch consumes a turn; the enemy can faint
    // the incoming reserve and the living starter must be sent automatically.
    auto lowHpSwitchGame = game;
    if (lowHpSwitchGame.playerPartyMember(1)->battleState.hp != 1 ||
        !lowHpSwitchGame.switchPlayerPokemon(1) || lowHpSwitchGame.activePlayerPartyIndex() != 0 ||
        lowHpSwitchGame.playerPartyMember(1)->battleState.hp || lowHpSwitchGame.playerPartyDefeated()) return 10220;
    // Test held ownership and ordinary switching with healthy canonical actors,
    // independently of the forced faint/replacement path checked above.
    NativeRunSave healthySwitchSave{};
    if (game.captureNativeRunSave(healthySwitchSave) != NativeSaveResult::Ok ||
        healthySwitchSave.playerPartyCount != 2) return 10221;
    for (uint8_t member = 0; member < 2; ++member)
        healthySwitchSave.playerParty[member].hp = game.playerPartyMember(member)->battleState.maxHp;
    healthySwitchSave.playerHp = healthySwitchSave.playerParty[healthySwitchSave.activePlayerMember].hp;
    if (!game.restoreNativeRunSave(healthySwitchSave)) return 10222;
    auto healingSwitchGame = game;
    NativeHeldModifierInstance switchHealing[2]{};
    const uint32_t outgoingPid = game.presentation().player.battleState.pokemonId;
    const uint32_t incomingPid = game.playerPartyMember(1)->battleState.pokemonId;
    if (initializeHeldModifierInstance("LEFTOVERS", outgoingPid, 1, true, nullptr, switchHealing[0]) !=
            HeldModifierStorageResult::Ok ||
        initializeHeldModifierInstance("SHELL_BELL", incomingPid, 1, true, nullptr, switchHealing[1]) !=
            HeldModifierStorageResult::Ok || !healingSwitchGame.restoreHeldModifierInventory(switchHealing, 2))
        return 439;
    if (!healingSwitchGame.switchPlayerPokemon(1) || healingSwitchGame.activePlayerPartyIndex() != 1 ||
        healingSwitchGame.heldModifierCount() != 2 ||
        healingSwitchGame.heldModifier(0)->ownerPokemonId != outgoingPid ||
        healingSwitchGame.heldModifier(1)->ownerPokemonId != incomingPid ||
        healingSwitchGame.presentation().player.battleState.turnDamageDealt) { std::printf("Switch feedback=%s finished=%u\n", healingSwitchGame.battleFeedback().c_str(), healingSwitchGame.battleFinished()); return 440; }
    const auto heldRecipientWave = healingSwitchGame.run().wave;
    if (healingSwitchGame.claimHeldRewardChoice(255) ||
        healingSwitchGame.claimHeldRewardChoice(0) || healingSwitchGame.run().wave != heldRecipientWave ||
        healingSwitchGame.heldModifierCount() != 2 ||
        healingSwitchGame.heldModifier(0)->ownerPokemonId != outgoingPid) return 446;
    if (healingSwitchGame.claimRecoveryRewardChoice(255, 0) ||
        healingSwitchGame.claimRecoveryRewardChoice(0, 255) ||
        healingSwitchGame.run().wave != heldRecipientWave || healingSwitchGame.heldModifierCount() != 2)
        return 455;
    // Switch to reserve member (index 1):
    if (!game.switchPlayerPokemon(1)) return 176;
    if (game.activePlayerPartyIndex() != 1) return 177;
    if (game.presentation().player.dex != caughtDex) return 178;
    if (game.presentation().player.totalExperience != reserveExperience) return 222;

    // Switch back to starter (index 0):
    if (!game.switchPlayerPokemon(0)) return 179;
    if (game.activePlayerPartyIndex() != 0) return 180;
    if (game.presentation().player.totalExperience != starterExperience ||
        game.playerPartyMember(1)->totalExperience != reserveExperience) return 223;

    // Canonical captured party + real Take Down: recoil knocks out the active
    // actor at the same time as the wild enemy. A living reserve must win now.
    NativeRunSave simultaneous = capturedPartySave;
    simultaneous.playerHp = simultaneous.playerParty[0].hp = simultaneous.enemyHp = 1;
    simultaneous.playerParty[0].moveCount = simultaneous.playerMoveCount = 1;
    simultaneous.playerParty[0].maxPpResolved = true;
    const auto* takeDown = PokerogueContent::findMoveById(36);
    if (!takeDown || takeDown->pp <= 0 || takeDown->pp > 255) return 559;
    for (uint8_t slot = 0; slot < 4; ++slot) {
        simultaneous.playerMoveIds[slot] = simultaneous.playerParty[0].moveIds[slot] = slot ? 0 : takeDown->id;
        simultaneous.playerPp[slot] = simultaneous.playerParty[0].pp[slot] = slot ? 0 : takeDown->pp;
        simultaneous.playerParty[0].maxPp[slot] = slot ? 0 : takeDown->pp;
    }
    simultaneous.playerStatStages[4] = simultaneous.playerParty[0].statStages[4] = 6;
    bool testedSimultaneous = false;
    FirstRunRuntime simultaneousGame(1);
    for (uint32_t turn = 1; turn <= 128 && !testedSimultaneous; ++turn) {
        simultaneous.battleTurn = turn;
        if (!simultaneousGame.restoreNativeRunSave(simultaneous)) {
            std::printf("Simultaneous checkpoint rejected: %s\n", simultaneousGame.battleFeedback().c_str()); return 560;
        }
        if (!simultaneousGame.battleInputSupported()) {
            std::printf("Simultaneous move unsupported: %s player=%u enemy=%u\n", simultaneousGame.battleFeedback().c_str(),
                simultaneousGame.presentation().player.battleState.abilityId, simultaneousGame.presentation().enemy.battleState.abilityId);
            for (const auto& slot : simultaneousGame.presentation().enemy.battleState.moves)
                std::printf("Simultaneous enemy move=%u pp=%u\n", slot.moveId, slot.pp);
            return 560;
        }
        if (!simultaneousGame.advanceBattleTurn()) return 561;
        if (simultaneousGame.playerPartyMember(0)->battleState.hp ||
            simultaneousGame.presentation().enemy.battleState.hp) continue; // Accuracy/turn-order outcomes.
        testedSimultaneous = true;
        if (!simultaneousGame.battleFinished() || !simultaneousGame.playerWon() ||
            simultaneousGame.activePlayerPartyIndex() != 1 ||
            !simultaneousGame.presentation().player.battleState.hp || simultaneousGame.experienceGranted())
            return 562;
        NativeRunSave afterSimultaneous{};
        simultaneousGame.captureNativeRunSave(afterSimultaneous);
        if (afterSimultaneous.stage != NativeSaveStage::BattleWon || afterSimultaneous.battleTurn != turn)
            return 563;
    }
    if (!testedSimultaneous) return 564; // Do not silently skip a missing real recoil scenario.
    return 0;
}

static int checkLevelUpMoveLearningAndEvolution() {
    using namespace Pokerogue3DS;

    // 1. Check evolution queries
    const auto* evo1 = checkSpeciesLevelEvolution("bulbasaur", 15, 16);
    if (!evo1 || std::strcmp(evo1->targetSpeciesId, "ivysaur") != 0) return 190;

    const auto* evoNone = checkSpeciesLevelEvolution("bulbasaur", 14, 15);
    if (evoNone != nullptr) return 191;

    const auto* evo2 = checkSpeciesLevelEvolution("ivysaur", 31, 32);
    if (!evo2 || std::strcmp(evo2->targetSpeciesId, "venusaur") != 0) return 192;

    const auto* evoBug = checkSpeciesLevelEvolution("caterpie", 6, 7);
    if (!evoBug || std::strcmp(evoBug->targetSpeciesId, "metapod") != 0) return 193;

    // 2. Check applySpeciesEvolution
    PokemonBattleInit bulbaInit{};
    bulbaInit.speciesDex = 1;
    bulbaInit.level = 16;
    bulbaInit.abilityId = PokerogueContent::findSpeciesByDex(1)->ability1;
    bulbaInit.gender = PokemonGender::Male;
    bulbaInit.nature = PokemonNature::Hardy;
    bulbaInit.moveCount = 1;
    bulbaInit.moveIds[0] = 33; // Tackle
    for (uint8_t i = 0; i < 6; ++i) bulbaInit.ivs[i] = 15;
    PokemonBattleState bulbaState{};
    if (initializePokemonBattleState(bulbaInit, bulbaState) != PokemonBattleInitResult::Ok) return 194;

    auto taggedEvolution = bulbaState;
    taggedEvolution.friendship = 173;
    taggedEvolution.heldItemLostTags.unburden = true;
    taggedEvolution.turnDamageDealt = 19;
    taggedEvolution.pauseEvolutions = true;
    taggedEvolution.moves[0].pp = 1;
    taggedEvolution.moves[0].maxPp = 42; // Tackle with one PP Up.
    EvolutionResult taggedEvolutionResult{};
    if (!applySpeciesEvolution(1, "ivysaur", taggedEvolution, taggedEvolutionResult) ||
        !taggedEvolutionResult.evolved || taggedEvolution.friendship != 173 ||
        !taggedEvolution.heldItemLostTags.unburden || taggedEvolution.turnDamageDealt != 19 ||
        taggedEvolution.pauseEvolutions || taggedEvolution.moves[0].pp != 1 ||
        taggedEvolution.moves[0].maxPp != 42) return 468;
    auto invalidEvolution = bulbaState;
    invalidEvolution.moves[0].pp = invalidEvolution.moves[0].maxPp + 1;
    EvolutionResult invalidEvolutionResult{};
    if (applySpeciesEvolution(1, "ivysaur", invalidEvolution, invalidEvolutionResult) ||
        invalidEvolution.speciesDex != 1 || invalidEvolutionResult.evolved) return 469;
    const uint16_t bulbaHp = bulbaState.hp;
    const uint16_t bulbaMaxHp = bulbaState.maxHp;
    EvolutionResult evoRes{};
    std::string evoFb;
    PokemonActorIdentity evolutionIdentity{};
    evolutionIdentity.pokemonId = bulbaState.pokemonId;
    evolutionIdentity.gender = bulbaState.gender;
    evolutionIdentity.nature = bulbaState.nature;
    evolutionIdentity.formId = bulbaState.formId;
    evolutionIdentity.initialTeraTypeResolved = true;
    for (uint8_t i = 0; i < 6; ++i) evolutionIdentity.ivs[i] = bulbaState.ivs[i];
    if (!applySpeciesEvolution(1, "ivysaur", bulbaState, evoRes, &evoFb, &evolutionIdentity)) return 195;
    if ((evolutionIdentity.formId != bulbaState.formId &&
            (!evolutionIdentity.formId || !bulbaState.formId || std::strcmp(evolutionIdentity.formId, bulbaState.formId))) ||
        evolutionIdentity.abilityIndex != 0) return 298;
    uint32_t evolvedExperience = 0;
    const auto* evolvedSpecies = PokerogueContent::findSpeciesByDex(2);
    NativePokemonSave evolvedSnapshot{};
    if (!evolvedSpecies || pokemonTotalExperienceForLevel(evolvedSpecies->growthRate, 16, evolvedExperience) !=
            PokemonExperienceResult::Ok || !captureNativePokemonActorSave(bulbaState, evolutionIdentity,
                evolvedExperience, evolvedSnapshot)) return 299;
    const auto* ppUpProfile = ppUpItemProfile("PP_UP");
    const auto* ppMaxProfile = ppUpItemProfile("PP_MAX");
    if (!ppUpProfile || !ppMaxProfile || ppUpProfile->upPoints != 1 || ppMaxProfile->upPoints != 3 ||
        !ppUpProfile->sourceHash || ppUpItemProfile("UNKNOWN_ITEM")) return 474;
    auto ppUpActor = bulbaState;
    ppUpActor.moves[0].pp = 1;
    if (!applyPokemonPpUpItem(ppUpActor, *ppUpProfile, 0) || ppUpActor.moves[0].maxPp != 42 ||
        ppUpActor.moves[0].pp != 8) return 475;
    if (!applyPokemonPpUpItem(ppUpActor, *ppMaxProfile, 0) || ppUpActor.moves[0].maxPp != 56 ||
        ppUpActor.moves[0].pp != 22) return 476;
    if (applyPokemonPpUpItem(ppUpActor, *ppUpProfile, 0) || ppUpActor.moves[0].maxPp != 56 ||
        ppUpActor.moves[0].pp != 22 || applyPokemonPpUpItem(ppUpActor, *ppUpProfile, 1)) return 477;
    ppUpActor.moves[0].maxPp = 41;
    if (applyPokemonPpUpItem(ppUpActor, *ppUpProfile, 0) || ppUpActor.moves[0].maxPp != 41)
        return 478;
    auto boostedPpActor = bulbaState;
    boostedPpActor.moves[0].maxPp = 42;
    boostedPpActor.moves[0].pp = 40;
    NativePokemonSave boostedPpSave{};
    NativePokemonSave decodedPpSave{};
    PokemonBattleState restoredPpActor{};
    PokemonActorIdentity restoredPpIdentity{};
    char boostedPpBytes[512]{};
    size_t boostedPpSize = 0;
    if (!captureNativePokemonActorSave(boostedPpActor, evolutionIdentity, evolvedExperience, boostedPpSave) ||
        !boostedPpSave.maxPpResolved || boostedPpSave.maxPp[0] != 42 ||
        encodeNativePokemonSave(boostedPpSave, boostedPpBytes, sizeof(boostedPpBytes), boostedPpSize) !=
            NativeSaveResult::Ok || decodeNativePokemonSave(boostedPpBytes, boostedPpSize, decodedPpSave) !=
            NativeSaveResult::Ok || !restoreNativePokemonActorSave(decodedPpSave, restoredPpActor, restoredPpIdentity) ||
        restoredPpActor.moves[0].maxPp != 42 || restoredPpActor.moves[0].pp != 40) return 470;
    boostedPpBytes[8] = '5';
    if (decodeNativePokemonSave(boostedPpBytes, boostedPpSize - 12, decodedPpSave) !=
            NativeSaveResult::InvalidRecord) return 471; // Legacy base PP cannot hold 40/35.
    boostedPpActor.moves[0].maxPp = 41;
    if (captureNativePokemonActorSave(boostedPpActor, evolutionIdentity, evolvedExperience, boostedPpSave))
        return 472;
    if (!evoRes.evolved || evoRes.newDex != 2 || std::strcmp(evoRes.newSpeciesId, "ivysaur") != 0) return 196;
    if (bulbaState.speciesDex != 2) return 197;
    // Ivysaur has higher base stats and max HP than Bulbasaur
    if (bulbaState.maxHp <= bulbaMaxHp || bulbaState.hp <= bulbaHp) return 198;

    auto replacementActor = bulbaState;
    replacementActor.moveCount = 4;
    replacementActor.moves[0] = {33, 1, 35};
    replacementActor.moves[1] = {45, 1, 40};
    replacementActor.moves[2] = {22, 1, 25};
    replacementActor.moves[3] = {73, 1, 10};
    const auto* newMove = PokerogueContent::findMoveById(75);
    if (!newMove || learnPokemonMoveAtSlot(replacementActor, 75, 2) !=
            PokemonLearnMoveResult::Learned || replacementActor.moveCount != 4 ||
        replacementActor.moves[2].moveId != 75 || replacementActor.moves[2].pp != newMove->pp ||
        replacementActor.moves[0].pp != 1 || replacementActor.moves[3].pp != 1) return 303;
    if (learnPokemonMoveAtSlot(replacementActor, 75, 0) != PokemonLearnMoveResult::AlreadyKnown ||
        replacementActor.moves[0].moveId != 33) return 304;
    if (learnPokemonMoveAtSlot(replacementActor, 14, 4) != PokemonLearnMoveResult::InvalidSlot ||
        replacementActor.moves[2].moveId != 75) return 305;
    const PokerogueContent::Move* unimplementedMove = nullptr;
    for (const auto& move : PokerogueContent::kMoves)
        if (move.upstreamFlags & PokerogueContent::MoveIsUnimplemented) { unimplementedMove = &move; break; }
    if (!unimplementedMove || learnPokemonMoveAtSlot(replacementActor, unimplementedMove->id, 0) != PokemonLearnMoveResult::UpstreamUnimplemented ||
        replacementActor.moves[0].moveId != 33) return 306;
    PokemonPendingLevelMoves pendingMoves{};
    uint16_t queuedIds[4]{};
    uint8_t queuedCount = replacementActor.moveCount;
    (void)learnNewLevelMoves(replacementActor.speciesDex, 1, 100, replacementActor,
        queuedIds, queuedCount, nullptr, &pendingMoves);
    if (!pendingMoves.count || pendingMoves.overflow || replacementActor.moveCount != 4) return 307;
    for (uint16_t index = 0; index < pendingMoves.count; ++index) {
        const auto* queued = PokerogueContent::findMoveById(pendingMoves.moveIds[index]);
        if (!queued || (queued->upstreamFlags & PokerogueContent::MoveIsUnimplemented)) return 308;
        for (uint16_t prior = 0; prior < index; ++prior)
            if (pendingMoves.moveIds[prior] == pendingMoves.moveIds[index]) return 309;
        for (uint8_t slot = 0; slot < replacementActor.moveCount; ++slot)
            if (replacementActor.moves[slot].moveId == pendingMoves.moveIds[index]) return 310;
    }
    // A chosen replacement survives the subsequent evolution phase.
    PokemonBattleInit sequenceInput = bulbaInit;
    sequenceInput.moveCount = 4;
    sequenceInput.moveIds[0] = 33;
    sequenceInput.moveIds[1] = 45;
    sequenceInput.moveIds[2] = 22;
    sequenceInput.moveIds[3] = 73;
    PokemonBattleState sequenceActor{};
    if (initializePokemonBattleState(sequenceInput, sequenceActor) != PokemonBattleInitResult::Ok ||
        learnPokemonMoveAtSlot(sequenceActor, 75, 2) != PokemonLearnMoveResult::Learned) return 311;
    auto sequenceIdentity = evolutionIdentity;
    sequenceIdentity.formId = sequenceActor.formId;
    EvolutionResult sequenceEvolution{};
    if (!applySpeciesEvolution(1, "ivysaur", sequenceActor, sequenceEvolution, nullptr, &sequenceIdentity) ||
        sequenceActor.speciesDex != 2 || sequenceActor.moves[2].moveId != 75 ||
        sequenceActor.moves[2].pp != newMove->pp) return 312;
    bool evolutionMoveCovered = false;
    for (const auto& species : PokerogueContent::kSpecies) {
        const auto* rows = PokerogueContent::levelMovesFor(species);
        for (uint16_t i = 0; rows && i < species.learnsetCount; ++i) {
            const auto* move = PokerogueContent::findMoveById(rows[i].moveId);
            if (rows[i].level != 0 || !move || (move->upstreamFlags & PokerogueContent::MoveIsUnimplemented)) continue;
            PokemonBattleState learner{};
            learner.speciesDex = species.dex;
            PokemonPendingLevelMoves queue{};
            if (!learnPokemonEvolutionMoves(learner, queue) || !learner.moveCount) return 313;
            bool found = false;
            for (uint8_t slot = 0; slot < learner.moveCount; ++slot) found |= learner.moves[slot].moveId == move->id;
            if (!found) return 314;
            if (!learnPokemonEvolutionMoves(learner, queue)) return 315;
            evolutionMoveCovered = true;
            break;
        }
        if (evolutionMoveCovered) break;
    }
    if (!evolutionMoveCovered) return 316;
    // A real Onix -> Steelix type change must retain its original Rock Tera type.
    const auto* onix = PokerogueContent::findSpeciesByDex(95);
    const auto* steelix = PokerogueContent::findSpeciesByDex(208);
    if (!onix || !steelix) return 330;
    PokemonBattleInit onixInput = bulbaInit;
    onixInput.speciesDex = onix->dex;
    onixInput.formId = onix->firstFormId[0] ? onix->firstFormId : nullptr;
    onixInput.abilityId = onix->ability1;
    PokemonBattleState onixState{};
    if (initializePokemonBattleState(onixInput, onixState) != PokemonBattleInitResult::Ok) return 331;
    PokemonActorIdentity onixIdentity{};
    onixIdentity.pokemonId = onixState.pokemonId;
    onixIdentity.gender = onixState.gender;
    onixIdentity.nature = onixState.nature;
    onixIdentity.formId = onixState.formId;
    onixIdentity.initialTeraTypeResolved = true;
    onixIdentity.initialTeraType = resolvePokemonTypeSymbol(onix->type1);
    for (uint8_t i = 0; i < 6; ++i) onixIdentity.ivs[i] = onixState.ivs[i];
    EvolutionResult steelixEvolution{};
    if (!applySpeciesEvolution(onix->dex, steelix->id, onixState, steelixEvolution, nullptr, &onixIdentity) ||
        !onixIdentity.initialTeraType || std::strcmp(onixIdentity.initialTeraType, resolvePokemonTypeSymbol(onix->type1)) ||
        !std::strcmp(onixIdentity.initialTeraType, resolvePokemonTypeSymbol(steelix->type1))) return 332;
    uint32_t steelixExp = 0;
    NativePokemonSave steelixSave{};
    PokemonBattleState restoredSteelix{};
    PokemonActorIdentity restoredSteelixIdentity{};
    if (pokemonTotalExperienceForLevel(steelix->growthRate, onixState.level, steelixExp) !=
            PokemonExperienceResult::Ok || !captureNativePokemonActorSave(onixState, onixIdentity,
                steelixExp, steelixSave) || !restoreNativePokemonActorSave(steelixSave, restoredSteelix,
                restoredSteelixIdentity) || std::strcmp(restoredSteelixIdentity.initialTeraType, resolvePokemonTypeSymbol(onix->type1)))
        return 333;
    std::strcpy(steelixSave.initialTeraType, "INVALID_TYPE");
    if (restoreNativePokemonActorSave(steelixSave, restoredSteelix, restoredSteelixIdentity) ||
        std::strcmp(restoredSteelixIdentity.initialTeraType, resolvePokemonTypeSymbol(onix->type1))) return 334;
    // 3. Check learnNewLevelMoves
    // Bulbasaur learns Vine Whip (id 22) or Leech Seed at early levels
    PokemonBattleState learnState = bulbaState;
    learnState.speciesDex = 1; // Bulbasaur
    learnState.moveCount = 1;
    learnState.moves[0].moveId = 33; // Tackle
    uint16_t moveIds[4] = { 33, 0, 0, 0 };
    uint8_t moveCount = 1;
    std::string learnFb;
    const uint8_t learned = learnNewLevelMoves(1, 1, 10, learnState, moveIds, moveCount, &learnFb);
    if (learned == 0 || learnState.moveCount <= 1) return 199;
    if (learnState.moves[1].pp == 0) return 200;

    return 0;
}

static int checkBossDamageAbilityCapabilities() {
    unsigned inspected = 0;
    for (const auto& profile : PokerogueContent::kAbilityMovegenProfiles) {
        if (std::strcmp(profile.sourceSymbol, "AbilityId.PRESSURE") == 0) {
            if (!profile.bossDamageCallbacksResolved) return 261;
            ++inspected;
        }
        if (std::strcmp(profile.sourceSymbol, "AbilityId.STURDY") == 0) {
            if (!profile.bossDamageCallbacksResolved) return 262; // Pinned Sturdy endurance and OHKO attributes are implemented.
            ++inspected;
        }
    }
    return inspected == 2 ? 0 : 263;
}

static int checkTrainerParentEvolutionThresholds() {
    const PokerogueContent::TrainerPartySegment* normal = nullptr;
    for (const auto& segment : PokerogueContent::kTrainerPartySegments)
        if (segment.evolutionThresholdKind && std::strcmp(segment.evolutionThresholdKind, "NORMAL") == 0)
            normal = &segment;
    if (!normal) return 252;
    unsigned compounds = 0;
    for (const auto& party : PokerogueContent::kTrainerPartyTemplates) {
        if (party.isCompound) {
            if (party.parentEvolutionThresholdKindId != normal->evolutionThresholdKindId) return 253;
            ++compounds;
        } else {
            const auto* segments = PokerogueContent::trainerPartySegmentsFor(party);
            if (!segments || party.parentEvolutionThresholdKindId != segments[0].evolutionThresholdKindId) return 254;
        }
    }
    return compounds ? 0 : 255;
}

static int checkCanonicalTrainerSpecialtyTypes() {
    unsigned inspected = 0;
    for (const auto& trainer : PokerogueContent::kTrainerTypes) {
        if (std::strcmp(trainer.key, "brock") == 0 || std::strcmp(trainer.key, "misty") == 0) {
            const char* expected = std::strcmp(trainer.key, "brock") == 0 ? "ROCK" : "WATER";
            if (!trainer.specialtyTypeResolved || !trainer.specialtyType ||
                std::strcmp(trainer.specialtyType, expected)) return 249;
            ++inspected;
        }
        if (!trainer.specialtyTypeResolved || !trainer.specialtyType || !*trainer.specialtyType) continue;
        bool canonicalType = false;
        for (const auto& species : PokerogueContent::kSpecies)
            if ((Pokerogue3DS::resolvePokemonTypeSymbol(species.type1) && std::strcmp(Pokerogue3DS::resolvePokemonTypeSymbol(species.type1), trainer.specialtyType) == 0) ||
                (Pokerogue3DS::resolvePokemonTypeSymbol(species.type2) && std::strcmp(Pokerogue3DS::resolvePokemonTypeSymbol(species.type2), trainer.specialtyType) == 0)) canonicalType = true;
        if (!canonicalType) return 250;
    }
    return inspected == 2 ? 0 : 251;
}

static int checkTrainerBalancedTypes() {
    using namespace Pokerogue3DS;
    const auto* species = PokerogueContent::findSpeciesByDex(1);
    if (!species || !species->type1) return 242;
    const auto* form = species->firstFormId[0] ? PokerogueContent::findFormById(species->firstFormId) : nullptr;
    if (species->firstFormId[0] && !form) return 243;
    TrainerPartyMemberTypes prior[] = {{form ? form->type1 : species->type1, form ? form->type2 : species->type2, true}};
    bool overlap = false;
    if (!trainerBalancedTypeOverlap(*species, prior, 1, overlap) || !overlap) return 244;
    if (!trainerBalancedTypeOverlap(*species, nullptr, 0, overlap) || overlap) return 245;
    prior[0].resolved = false;
    overlap = true;
    if (trainerBalancedTypeOverlap(*species, prior, 1, overlap) || !overlap ||
        trainerBalancedTypeOverlap(*species, nullptr, 1, overlap)) return 246;
    unsigned disjoint = 0;
    prior[0].resolved = true;
    for (const auto& other : PokerogueContent::kSpecies) {
        const bool expected = std::strcmp(other.type1, prior[0].type1) == 0 ||
            (prior[0].type2 && std::strcmp(prior[0].type2, "NONE") && std::strcmp(other.type1, prior[0].type2) == 0) ||
            (other.type2 && std::strcmp(other.type2, "NONE") &&
                (std::strcmp(other.type2, prior[0].type1) == 0 ||
                 (prior[0].type2 && std::strcmp(prior[0].type2, "NONE") && std::strcmp(other.type2, prior[0].type2) == 0)));
        if (!trainerBalancedTypeOverlap(other, prior, 1, overlap) || overlap != expected) return 247;
        if (!overlap) ++disjoint;
    }
    return disjoint ? 0 : 248;
}

static int checkReservedTrainerSpecies() {
    using namespace Pokerogue3DS;
    unsigned checked = 0;
    for (const auto& trainer : PokerogueContent::kTrainerTypes) {
        if (!trainer.signatureCount) continue;
        if (trainer.signatureOffset + trainer.signatureCount > PokerogueContent::kTrainerSignatureChoiceCount) return 237;
        for (uint8_t slot = 0; slot < trainer.signatureCount; ++slot) {
            const auto& choice = PokerogueContent::kTrainerSignatureChoices[trainer.signatureOffset + slot];
            for (uint8_t i = 0; i < choice.speciesCount; ++i) {
                const auto* species = trainerPartySpeciesById(
                    PokerogueContent::kTrainerSignatureSpecies[choice.speciesOffset + i].speciesId);
                if (!species) return 238;
                bool duplicate = false;
                if (!trainerReservedSpeciesDuplicate(trainer, trainerPartyRootDex(*species), duplicate) ||
                    !duplicate) return 239;
                ++checked;
            }
        }
        bool sentinel = true;
        if (trainerReservedSpeciesDuplicate(trainer, 65535, sentinel) || !sentinel) return 240;
    }
    return checked ? 0 : 241;
}

static int checkTrainerPoolEvolutionDraws() {
    using namespace Pokerogue3DS;
    unsigned checked = 0;
    for (const auto& trainer : PokerogueContent::kTrainerTypes) {
        if (!trainer.speciesPoolCount || trainer.signatureCount || !trainer.specialtyTypeResolved ||
                (trainer.specialtyType && *trainer.specialtyType)) continue;
        const uint16_t seed[] = {'p', 'o', 'o', 'l'};
        PokerogueRngAdapter templateRng;
        templateRng.sow(seed, 4);
        const auto choice = selectTrainerPartyTemplate(trainer, 25, templateRng);
        if (!choice.supported || !choice.value) continue;
        const auto slot = trainerPartyMemberTemplate(*choice.value, 0);
        if (!slot.supported || slot.balanced || slot.sameSpecies) continue;
        PokerogueRngAdapter expectedRng, actualRng;
        expectedRng.sow(seed, 4);
        actualRng.sow(seed, 4);
        const char* expected = nullptr;
        for (uint8_t attempt = 0; attempt <= 10; ++attempt) {
            expected = nullptr;
            const auto pool = PokerogueEncounterResolver::resolveTrainerPoolSpecies(trainer, expectedRng);
            if (!pool.valid) break;
            const auto* base = trainerPartySpeciesById(pool.speciesId);
            if (!base) break;
            expected = PokerogueEncounterResolver::resolveTrainerSpeciesForLevel(base->id,
                25, choice.value->parentEvolutionThresholdKindId, false, expectedRng);
            if (!expected) break;
            if (base->prevolutionDex && std::strcmp(expected, base->id) && attempt < 10) continue;
            break;
        }
        if (!expected) continue;
        const auto actual = resolveSimpleTrainerPoolMember(trainer, *choice.value, 0, 25,
            25, nullptr, 0, actualRng);
        if (!actual.supported || !actual.species || std::strcmp(expected, actual.species->id)) return 234;
        const auto a = actualRng.state(), e = expectedRng.state();
        if (a.carry != e.carry || a.s0 != e.s0 || a.s1 != e.s1 || a.s2 != e.s2) return 235;
        ++checked;
    }
    return checked ? 0 : 236;
}

static int checkCanonicalSameSpeciesTrainerMembers() {
    using namespace Pokerogue3DS;
    unsigned checked = 0;
    for (const auto& trainer : PokerogueContent::kTrainerTypes) {
        if (!trainer.speciesPoolCount || trainer.signatureCount) continue;
        PokerogueRngAdapter templateRng;
        const uint16_t seed[] = {'s', 'a', 'm', 'e'};
        templateRng.sow(seed, 4);
        const auto choice = selectTrainerPartyTemplate(trainer, 25, templateRng);
        if (!choice.supported || !choice.value) continue;
        const auto levels = resolveClassicTrainerPartyLevels(*choice.value, 25, false);
        if (!levels.supported) continue;
        const PokerogueContent::Species* previous[6]{};
        for (uint8_t slot = 0; slot < levels.count; ++slot) {
            PokerogueRngAdapter memberRng;
            memberRng.sow(seed, 4);
            const auto member = resolveSimpleTrainerPoolMember(trainer, *choice.value,
                slot, levels.values[slot], 25, previous, slot, memberRng);
            if (!member.supported || !member.species) break;
            const auto metadata = trainerPartyMemberTemplate(*choice.value, slot);
            if (metadata.sameSpecies && slot > metadata.segmentStart) {
                const char* expected = PokerogueEncounterResolver::resolveTrainerSpeciesForLevel(
                    previous[metadata.segmentStart]->id, levels.values[slot],
                    choice.value->parentEvolutionThresholdKindId, false, memberRng, true, false);
                if (!expected || std::strcmp(expected, member.species->id)) return 232;
                ++checked;
            }
            previous[slot] = member.species;
        }
    }
    return checked ? 0 : 233;
}

static int checkCanonicalTrainerSignatureSlots() {
    using namespace Pokerogue3DS;
    unsigned checked = 0;
    for (const auto& trainer : PokerogueContent::kTrainerTypes) {
        if (!(trainer.flags & 32U) || !trainer.signatureCount || trainer.signatureCount > 6) continue;
        const uint8_t partySize = 6;
        for (uint8_t distance = 1; distance <= trainer.signatureCount; ++distance) {
            PokerogueRngAdapter rng;
            const uint16_t seed[] = {'s', 'i', 'g'};
            rng.sow(seed, 3);
            const char* id = trainerSignatureSpeciesForMember(trainer, partySize,
                static_cast<uint8_t>(partySize - distance), rng);
            if (!id || !trainerPartySpeciesById(id)) return 230;
            ++checked;
        }
    }
    return checked ? 0 : 231; // Real catalog coverage, not a synthetic trainer.
}

static int checkDoublePoisonHealTurn() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 1024; ++seed) {
        FirstRunRuntime baseline(seed), poisoned(seed);
        if (!baseline.doubleBattle() || baseline.arenaWeather().type != PokemonEffectiveWeather::None) continue;
        auto& base = const_cast<PresentationContext&>(baseline.presentation()).secondEnemy.battleState;
        auto& target = const_cast<PresentationContext&>(poisoned.presentation()).secondEnemy.battleState;
        // Canonical Poison Heal ability in a controlled, test-only real field context.
        base.abilityId = target.abilityId = 90;
        base.hp = target.hp = base.maxHp > 1 ? base.maxHp / 2 : 1;
        target.status = {};
        target.status.present = true;
        target.status.effect = PokemonStatusEffect::Toxic;
        if (!baseline.doubleBattleSupported() || !poisoned.doubleBattleSupported()) continue;
        if (!baseline.advanceBattleTurn()) return 10470;
        if (baseline.battleFinished() || !base.hp) continue;
        auto expected = base;
        expected.status = target.status;
        expected.status.toxicTurnCount = 1;
        PokemonHealingPolicy healing{};
        healing.resolved = true;
        PokemonHealingEvent event{};
        if (applyPokemonPostTurnStatusHealing(expected, true, healing, event) != PokemonHealingResult::Ok)
            return 10471;
        if (!poisoned.advanceBattleTurn() || target.hp != expected.hp ||
            !target.status.present || target.status.effect != PokemonStatusEffect::Toxic ||
            target.status.toxicTurnCount != 1) return 10472;
        return 0;
    }
    return 10473; // Require actual end-turn residual block + healing, not just ability metadata.
}

static int checkDynamicDoubleSpeedChangeTurn() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 4096; ++seed) {
        FirstRunRuntime game(seed);
        if (!game.doubleBattle() || game.arenaWeather().type != PokemonEffectiveWeather::None) continue;
        auto& field = const_cast<PresentationContext&>(game.presentation());
        if (field.enemy.bossState.segmentCount || field.secondEnemy.bossState.segmentCount) continue;
        PokemonBattleState* actors[] = {&field.player.battleState, &field.enemy.battleState,
            &field.secondEnemy.battleState};
        bool neutral = true;
        for (auto* actor : actors) {
            const auto* profile = PokerogueContent::findAbilityStatStageProfile(actor->abilityId);
            if (profile && (profile->multiplier != 1 || profile->protectedMask || profile->reflectDrops ||
                    profile->copiesRaises)) neutral = false;
            for (const auto& row : PokerogueContent::kAbilityStatStageReactions)
                if (row.abilityId == actor->abilityId) neutral = false;
            for (auto& stage : actor->statStages) stage = 0;
            actor->moveCount = 1;
        }
        if (!neutral) continue;
        actors[0]->moves[0] = {184, 10, 10}; // Canonical Scary Face.
        actors[1]->moves[0] = actors[2]->moves[0] = {129, 20, 20}; // Canonical Swift.
        // Controlled speeds are test-only; species, forms, attack/defense remain real.
        actors[0]->stats[5] = 300;
        actors[1]->stats[5] = 200;
        actors[2]->stats[5] = 150;
        if (!game.doubleBattleSupported() || !game.battleRng().currentStream()) continue;
        auto rng = *game.battleRng().currentStream();
        PokemonBattleState expected[] = {*actors[0], *actors[1], *actors[2]};
        PokemonMoveWeatherContext weather{true};
        PokemonHitPolicy hit{};
        const PokemonWeatherAbilityComponent components[] = {
            {expected[0].abilityId, true, true}, {expected[1].abilityId, true, false}
        };
        if (!composePokemonAlwaysHitPolicy(components, 2, hit, 184, &weather)) continue;
        PokemonStatStageCommandPolicy policy{};
        policy.postChangePoliciesResolved = true;
        policy.move.hitPolicyResolved = true;
        if (!pokemonSingleOpponentPpCost(expected[1].abilityId, policy.move.ppCost)) continue;
        policy.move.accuracyMultiplier = hit.accuracyMultiplier;
        policy.move.bypassAccuracy = hit.bypassAccuracy;
        policy.move.blockedBeforeAccuracy = hit.blockedByAbility;
        policy.move.stagePolicy.resolved = true;
        policy.move.stagePolicy.chance = -1;
        PokemonStatStageCommandEvent stageEvent{};
        if (usePokemonStatStageStatusCommand(expected[0], expected[1], 0, policy, rng, stageEvent) !=
                PokemonStatStageEffectResult::Ok || !stageEvent.move.hit || expected[1].statStages[4] != -2) continue;
        uint32_t firstSpeed = 0, secondSpeed = 0;
        if (!pokemonWeatherEffectiveSpeed(expected[1], weather, firstSpeed) ||
            !pokemonWeatherEffectiveSpeed(expected[2], weather, secondSpeed) || firstSpeed >= secondSpeed) continue;
        const auto afterPlayer = rng;
        const auto simulate = [&](PokemonBattleState* snapshots, PokerogueRngAdapter& stream, bool dynamic) {
            const uint8_t order[] = {static_cast<uint8_t>(dynamic ? 2 : 1), static_cast<uint8_t>(dynamic ? 1 : 2)};
            for (uint8_t id : order) {
                PokemonHitPolicy attackHit{};
                PokemonCriticalPolicy critical{};
                const PokemonWeatherAbilityComponent hitComponents[] = {
                    {snapshots[id].abilityId, true, true}, {snapshots[0].abilityId, true, false}
                };
                const PokemonCriticalAbilityComponent critComponents[] = {
                    {snapshots[id].abilityId, true, true}, {snapshots[0].abilityId, true, false}
                };
                if (!composePokemonAlwaysHitPolicy(hitComponents, 2, attackHit, 129, &weather) ||
                    !composePokemonCriticalAbilityPolicy(critComponents, 2, false, critical)) return false;
                PokemonPpPolicy pp{true, 1};
                if (!pokemonSingleOpponentPpCost(snapshots[0].abilityId, pp.cost)) return false;
                PokemonMoveActionResult result{};
                if (useStandardPokemonMove(snapshots[id], snapshots[0], 0, false, stream, result,
                        &weather, &critical, &attackHit, &pp) != PokemonMoveActionStatus::Ok || !snapshots[0].hp)
                    return false;
            }
            return true;
        };
        PokemonBattleState oldOrder[] = {expected[0], expected[1], expected[2]};
        auto oldRng = afterPlayer;
        if (!simulate(expected, rng, true) || !simulate(oldOrder, oldRng, false) ||
            expected[0].hp == oldOrder[0].hp) continue; // Require an observable ordering difference.
        if (!game.advanceBattleTurn() || field.player.battleState.hp != expected[0].hp ||
            field.enemy.battleState.statStages[4] != -2 ||
            field.player.battleState.moves[0].pp != expected[0].moves[0].pp ||
            field.enemy.battleState.moves[0].pp != expected[1].moves[0].pp ||
            field.secondEnemy.battleState.moves[0].pp != expected[2].moves[0].pp) return 10460;
        return 0;
    }
    return 10461; // Require an actual turn whose HP distinguishes dynamic from initial order.
}

static int checkDoubleMirrorArmorSourceProtection() {
    using namespace Pokerogue3DS;
    const uint16_t sourceAbilities[] = {86, 126, 29, 240};
    const int8_t expectedDefense[] = {-4, 2, 0, -2};
    for (uint32_t seed = 1; seed <= 1024; ++seed) {
        FirstRunRuntime source(seed);
        if (!source.doubleBattle()) continue;
        auto& field = const_cast<PresentationContext&>(source.presentation());
        field.player.battleState.moveCount = 1;
        field.player.battleState.moves[0] = {103, 1, 40};
        field.player.battleState.statStages[5] = 6;
        bool durationResolved = true;
        PokemonBattleState* enemies[] = {&field.enemy.battleState, &field.secondEnemy.battleState};
        for (auto* enemy : enemies) {
            bool duration = false;
            for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles)
                if (profile.abilityId == enemy->abilityId) duration = profile.resolved;
            durationResolved &= duration;
            enemy->status = {};
            enemy->status.present = enemy->status.hasSleepTurnsRemaining = true;
            enemy->status.effect = PokemonStatusEffect::Sleep;
            enemy->status.sleepTurnsRemaining = 8;
            for (auto& stage : enemy->statStages) stage = 0;
            for (uint8_t slot = 0; slot < enemy->moveCount; ++slot) enemy->moves[slot].pp = 0;
        }
        if (!durationResolved || !source.doubleBattleSupported()) continue;
        for (uint8_t i = 0; i < 4; ++i) {
            FirstRunRuntime game = source;
            auto& actors = const_cast<PresentationContext&>(game.presentation());
            // Canonical ability contexts injected exclusively for regression.
            actors.player.battleState.abilityId = sourceAbilities[i];
            actors.enemy.battleState.abilityId = 240;
            if (!game.doubleBattleSupported() || !game.advanceBattleTurn()) return 10430;
            if (actors.enemy.battleState.statStages[1] ||
                actors.player.battleState.statStages[1] != expectedDefense[i] ||
                actors.secondEnemy.battleState.statStages[1] || actors.player.battleState.moves[0].pp)
                return 10431;
        }
        return 0;
    }
    return 10432;
}

static int checkDoubleSingleTargetDropReactions() {
    using namespace Pokerogue3DS;
    const auto* screech = PokerogueContent::findMoveById(103);
    if (!screech || !screech->target || std::strcmp(screech->target, "NEAR_OTHER")) return 10420;
    const uint16_t abilities[] = {128, 172}; // Canonical Defiant and Competitive.
    const uint8_t boostedStats[] = {0, 2}; // ATK / SPATK stage indices.
    for (uint32_t seed = 1; seed <= 1024; ++seed) {
        FirstRunRuntime source(seed);
        if (!source.doubleBattle()) continue;
        auto& field = const_cast<PresentationContext&>(source.presentation());
        auto& player = field.player.battleState;
        player.moveCount = 1;
        player.moves[0] = {screech->id, 1, static_cast<uint8_t>(screech->pp)};
        player.statStages[5] = 6; // Test-only guarantee via accuracy threshold, no bypass RNG.
        bool durationResolved = true;
        PokemonBattleState* enemies[] = {&field.enemy.battleState, &field.secondEnemy.battleState};
        for (auto* enemy : enemies) {
            bool resolved = false;
            for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles)
                if (profile.abilityId == enemy->abilityId) resolved = profile.resolved;
            durationResolved &= resolved;
            enemy->status = {};
            enemy->status.present = enemy->status.hasSleepTurnsRemaining = true;
            enemy->status.effect = PokemonStatusEffect::Sleep;
            enemy->status.sleepTurnsRemaining = 8;
            for (auto& stage : enemy->statStages) stage = 0;
            for (uint8_t slot = 0; slot < enemy->moveCount; ++slot) enemy->moves[slot].pp = 0;
        }
        if (!durationResolved || !source.doubleBattleSupported()) continue;
        for (uint8_t i = 0; i < 2; ++i) {
            FirstRunRuntime game = source;
            auto& target = const_cast<PresentationContext&>(game.presentation()).enemy.battleState;
            target.abilityId = abilities[i]; // Canonical capability injected only for this regression.
            const uint16_t hp = target.hp;
            if (!game.doubleBattleSupported() || !game.advanceBattleTurn()) return 10421;
            if (target.statStages[1] != -2 || target.statStages[boostedStats[i]] != 2 || target.hp != hp ||
                game.presentation().player.battleState.moves[0].pp ||
                game.presentation().secondEnemy.battleState.statStages[1]) return 10422;
        }
        return 0;
    }
    return 10423;
}

static int checkDoubleLocalStageAbilities() {
    using namespace Pokerogue3DS;
    const uint16_t abilityIds[] = {86, 126, 29}; // Canonical Simple, Contrary, Clear Body.
    const int8_t expectedStages[] = {-2, 1, 0};
    for (uint32_t seed = 1; seed <= 1024; ++seed) {
        FirstRunRuntime source(seed);
        if (!source.doubleBattle()) continue;
        auto& field = const_cast<PresentationContext&>(source.presentation());
        field.player.battleState.moveCount = 1;
        field.player.battleState.moves[0] = {45, 1, 40};
        bool durationResolved = true;
        PokemonBattleState* enemies[] = {&field.enemy.battleState, &field.secondEnemy.battleState};
        for (auto* enemy : enemies) {
            bool duration = false;
            for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles)
                if (profile.abilityId == enemy->abilityId) duration = profile.resolved;
            durationResolved &= duration;
            enemy->status = {};
            enemy->status.present = enemy->status.hasSleepTurnsRemaining = true;
            enemy->status.effect = PokemonStatusEffect::Sleep;
            enemy->status.sleepTurnsRemaining = 8;
            for (uint8_t slot = 0; slot < enemy->moveCount; ++slot) enemy->moves[slot].pp = 0;
            for (auto& stage : enemy->statStages) stage = 0;
        }
        const auto* firstProfile = PokerogueContent::findAbilityStatStageProfile(field.enemy.battleState.abilityId);
        if (firstProfile && (firstProfile->multiplier != 1 || firstProfile->protectedMask)) continue;
        if (!durationResolved || !source.doubleBattleSupported()) continue;
        for (uint8_t caseIndex = 0; caseIndex < 3; ++caseIndex) {
            const auto* profile = PokerogueContent::findAbilityStatStageProfile(abilityIds[caseIndex]);
            if (!profile) return 10410;
            FirstRunRuntime game = source;
            auto& target = const_cast<PresentationContext&>(game.presentation()).secondEnemy.battleState;
            // Canonical ability IDs in test-only context; no production actor identity changes.
            target.abilityId = abilityIds[caseIndex];
            const uint16_t beforeHp = target.hp;
            if (!game.doubleBattleSupported() || !game.advanceBattleTurn()) {
                std::printf("Stage ability integration failed: seed=%u ability=%u feedback=%s\n",
                    static_cast<unsigned>(seed), static_cast<unsigned>(target.abilityId), game.battleFeedback().c_str());
                return 10411;
            }
            if (target.statStages[0] != expectedStages[caseIndex] || target.hp != beforeHp ||
                game.presentation().enemy.battleState.statStages[0] != -1 ||
                game.presentation().player.battleState.moves[0].pp) return 10412;
        }
        return 0;
    }
    return 10413; // Require a real field with neutral surrounding callbacks.
}

static int checkEnemyAreaStatAction() {
    using namespace Pokerogue3DS;
    const auto* growl = PokerogueContent::findMoveById(45);
    if (!growl) return 10400;
    for (uint32_t seed = 1; seed <= 1024; ++seed) {
        FirstRunRuntime game(seed);
        if (!game.doubleBattle()) continue;
        auto& field = const_cast<PresentationContext&>(game.presentation());
        auto& enemy = field.enemy.battleState;
        enemy.moveCount = 1;
        enemy.moves[0] = {growl->id, 1, static_cast<uint8_t>(growl->pp)};
        bool resolved = true;
        PokemonBattleState* sleeping[] = {&field.player.battleState, &field.secondEnemy.battleState};
        for (auto* actor : sleeping) {
            bool duration = false;
            for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles)
                if (profile.abilityId == actor->abilityId) duration = profile.resolved;
            resolved &= duration;
            actor->status = {};
            actor->status.present = actor->status.hasSleepTurnsRemaining = true;
            actor->status.effect = PokemonStatusEffect::Sleep;
            actor->status.sleepTurnsRemaining = 8;
            for (uint8_t slot = 0; slot < actor->moveCount; ++slot) actor->moves[slot].pp = 0;
            for (auto& stage : actor->statStages) stage = 0;
        }
        if (!resolved || !game.doubleBattleSupported()) continue;
        const uint16_t hp[] = {field.player.battleState.hp, enemy.hp, field.secondEnemy.battleState.hp};
        if (!game.advanceBattleTurn()) return 10401;
        if (enemy.moves[0].moveId != growl->id || enemy.moves[0].pp ||
            field.player.battleState.statStages[0] != -1 || field.secondEnemy.battleState.statStages[0] ||
            field.player.battleState.hp != hp[0] || enemy.hp != hp[1] ||
            field.secondEnemy.battleState.hp != hp[2]) return 10402;
        return 0;
    }
    return 10403; // Require an admitted real field with the enemy area command connected.
}

static int checkEnemyAreaAllyDamage() {
    using namespace Pokerogue3DS;
    const auto* move = PokerogueContent::findMoveById(572);
    if (!move) return 10390;
    for (uint32_t seed = 1; seed <= 1024; ++seed) {
        FirstRunRuntime source(seed);
        if (!source.doubleBattle() || source.arenaWeather().type != PokemonEffectiveWeather::None) continue;
        auto& field = const_cast<PresentationContext&>(source.presentation());
        auto& enemy = field.enemy.battleState;
        enemy.moveCount = 1;
        enemy.moves[0] = {move->id, 1, static_cast<uint8_t>(move->pp)};
        PokemonBattleState* sleeping[] = {&field.player.battleState, &field.secondEnemy.battleState};
        bool supported = true;
        for (auto* actor : sleeping) {
            bool duration = false;
            for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles)
                if (profile.abilityId == actor->abilityId) duration = profile.resolved;
            supported &= duration;
            actor->status = {};
            actor->status.present = actor->status.hasSleepTurnsRemaining = true;
            actor->status.effect = PokemonStatusEffect::Sleep;
            actor->status.sleepTurnsRemaining = 8;
            for (uint8_t slot = 0; slot < actor->moveCount; ++slot) actor->moves[slot].pp = 0;
        }
        if (!supported || !source.doubleBattleSupported()) continue;
        const uint16_t playerHp = field.player.battleState.hp;
        const uint16_t allyHp = field.secondEnemy.battleState.hp;
        const uint16_t enemyHp = enemy.hp;
        FirstRunRuntime left = source, right = source;
        if (!left.advanceBattleTurn() || !right.advanceBattleTurn()) return 10391;
        const auto& a = left.presentation();
        const auto& b = right.presentation();
        if (left.battleFinished() || !a.secondEnemy.battleState.hp) continue;
        if (!a.player.battleState.hp || a.player.battleState.hp >= playerHp ||
            a.secondEnemy.battleState.hp >= allyHp || a.enemy.battleState.hp != enemyHp ||
            a.enemy.battleState.moves[0].moveId != move->id || a.enemy.battleState.moves[0].pp ||
            a.player.battleState.hp != b.player.battleState.hp ||
            a.secondEnemy.battleState.hp != b.secondEnemy.battleState.hp ||
            a.enemy.battleState.moves[0].pp != b.enemy.battleState.moves[0].pp) return 10392;
        if (a.player.battleState.status.toxicTurnCount != 1 ||
            a.secondEnemy.battleState.status.toxicTurnCount != 1) return 10393;
        return 0;
    }
    return 10394; // Require a real enemy -> opponent + ally action, not just target metadata.
}

static int checkDoubleAreaHitBatchRng() {
    using namespace Pokerogue3DS;
    const auto* move = PokerogueContent::findMoveById(572); // Real Petal Blizzard, no extra effects.
    if (!move || move->attributeCount || !move->target || std::strcmp(move->target, "ALL_NEAR_OTHERS")) return 10380;
    for (uint32_t seed = 1; seed <= 4096; ++seed) {
        FirstRunRuntime game(seed);
        if (!game.doubleBattle() || game.arenaWeather().type != PokemonEffectiveWeather::None) continue;
        auto& field = const_cast<PresentationContext&>(game.presentation());
        if (field.enemy.bossState.segmentCount || field.secondEnemy.bossState.segmentCount) continue;
        auto& player = field.player.battleState;
        player.moveCount = 1;
        player.moves[0] = {move->id, 1, static_cast<uint8_t>(move->pp)};
        for (auto& stage : player.statStages) stage = 0;
        player.statStages[5] = -6; // Test-only lowered accuracy to exercise mixed outcomes.
        PokemonBattleState* enemies[] = {&field.enemy.battleState, &field.secondEnemy.battleState};
        bool supported = true;
        for (auto* enemy : enemies) {
            bool duration = false;
            for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles)
                if (profile.abilityId == enemy->abilityId) duration = profile.resolved;
            supported &= duration;
            enemy->status = {};
            enemy->status.present = enemy->status.hasSleepTurnsRemaining = true;
            enemy->status.effect = PokemonStatusEffect::Sleep;
            enemy->status.sleepTurnsRemaining = 8;
            for (auto& stage : enemy->statStages) stage = 0;
            for (uint8_t slot = 0; slot < enemy->moveCount; ++slot) enemy->moves[slot].pp = 0;
        }
        if (!supported || !game.doubleBattleSupported() || !game.battleRng().currentStream()) continue;
        auto rng = *game.battleRng().currentStream();
        auto expectedAccuracyRng = rng;
        const auto firstRoll = expectedAccuracyRng.randSeedInt(100);
        const auto secondRoll = expectedAccuracyRng.randSeedInt(100);
        PokemonDamageMoveHitCheck checks[2]{};
        PokemonHitPolicy hits[2]{};
        PokemonCriticalPolicy critical[2]{};
        PokemonMoveWeatherContext weather{true};
        for (uint8_t i = 0; i < 2; ++i) {
            const PokemonWeatherAbilityComponent hitComponents[] = {
                {player.abilityId, true, true}, {enemies[i]->abilityId, true, false}
            };
            const PokemonCriticalAbilityComponent critComponents[] = {
                {player.abilityId, true, true}, {enemies[i]->abilityId, true, false}
            };
            if (!composePokemonAlwaysHitPolicy(hitComponents, 2, hits[i], move->id, &weather) ||
                !composePokemonCriticalAbilityPolicy(critComponents, 2, false, critical[i])) return 10381;
            for (const auto& profile : PokerogueContent::kStatusActionAbilityProfiles) {
                if (!profile.resolved) continue;
                if (profile.abilityId == player.abilityId) hits[i].ignoreDefenderEvasionStage = profile.ignoresOpponentEvasion;
                if (profile.abilityId == enemies[i]->abilityId) hits[i].ignoreAttackerAccuracyStage = profile.ignoresOpponentAccuracy;
            }
            if (resolvePokemonDamageMoveHitCheck(player, *enemies[i], move->id, rng,
                    checks[i], &weather, &hits[i]) != PokemonMoveDamageResult::Ok) return 10382;
        }
        if (!checks[0].result.accuracyWasRolled || !checks[1].result.accuracyWasRolled) continue;
        if (checks[0].result.accuracyRoll != firstRoll || checks[1].result.accuracyRoll != secondRoll ||
            rng.state().s0 != expectedAccuracyRng.state().s0 || rng.state().s1 != expectedAccuracyRng.state().s1 ||
            rng.state().s2 != expectedAccuracyRng.state().s2 || rng.state().carry != expectedAccuracyRng.state().carry)
            return 10383;
        if (checks[0].result.hit == checks[1].result.hit) continue; // Require one hit and one miss.
        auto expectedPlayer = player;
        PokemonBattleState expectedEnemies[] = {*enemies[0], *enemies[1]};
        PokemonMoveTargetPolicy targets{true, 2};
        PokemonPpPolicy pp{true, 1};
        for (uint8_t i = 0; i < 2; ++i) {
            PokemonMoveActionResult result{};
            if (useStandardPokemonMove(expectedPlayer, expectedEnemies[i], 0, false, rng, result,
                    &weather, &critical[i], &hits[i], &pp, nullptr, nullptr, nullptr, nullptr, &targets,
                    &checks[i]) != PokemonMoveActionStatus::Ok) return 10384;
            pp.cost = 0;
        }
        if (!expectedEnemies[0].hp || !expectedEnemies[1].hp) continue;
        if (!game.advanceBattleTurn() || field.enemy.battleState.hp != expectedEnemies[0].hp ||
            field.secondEnemy.battleState.hp != expectedEnemies[1].hp || player.moves[0].pp ||
            player.moves[0].moveId != move->id || player.hp != expectedPlayer.hp) return 10385;
        auto rejected = checks[0];
        rejected.targetPokemonId ^= 1;
        PokemonMoveDamageRoll unchanged{};
        unchanged.damage = 123;
        auto beforeReject = rng;
        if (resolveStandardPokemonMoveDamage(expectedPlayer, expectedEnemies[0], move->id, false, rng,
                unchanged, &weather, &critical[0], &hits[0], nullptr, &targets, &rejected) !=
                PokemonMoveDamageResult::InvalidStats || unchanged.damage != 123 ||
            rng.randSeedUint32() != beforeReject.randSeedUint32()) return 10386;
        rejected = checks[0];
        rejected.attackerPokemonId ^= 1;
        unchanged.damage = 123;
        beforeReject = rng;
        if (resolveStandardPokemonMoveDamage(expectedPlayer, expectedEnemies[0], move->id, false, rng,
                unchanged, &weather, &critical[0], &hits[0], nullptr, &targets, &rejected) !=
                PokemonMoveDamageResult::InvalidStats || unchanged.damage != 123 ||
            rng.randSeedUint32() != beforeReject.randSeedUint32()) return 10440;
        return 0;
    }
    return 10387; // Require a real mixed hit/miss area action in the native runtime.
}

static int checkDoublePlainAreaDamage() {
    using namespace Pokerogue3DS;
    const auto* swift = PokerogueContent::findMoveById(129);
    if (!swift || swift->attributeCount || swift->accuracy != -1) return 10370;
    double multiplier = 7;
    PokemonMoveTargetPolicy two{true, 2}, one{true, 1}, invalid{false, 2};
    if (!pokemonMoveTargetMultiplier(swift->id, &two, multiplier) || multiplier != 0.75 ||
        !pokemonMoveTargetMultiplier(swift->id, &one, multiplier) || multiplier != 1) return 10371;
    multiplier = 7;
    if (pokemonMoveTargetMultiplier(swift->id, &invalid, multiplier) || multiplier != 7 ||
        pokemonMoveTargetMultiplier(33, &two, multiplier)) return 10372;
    for (uint32_t seed = 1; seed <= 1024; ++seed) {
        FirstRunRuntime game(seed);
        if (!game.doubleBattle()) continue;
        auto& field = const_cast<PresentationContext&>(game.presentation());
        field.player.battleState.moveCount = 1;
        field.player.battleState.moves[0] = {swift->id, 1, static_cast<uint8_t>(swift->pp)};
        bool resolved = true;
        PokemonBattleState* opponents[] = {&field.enemy.battleState, &field.secondEnemy.battleState};
        for (auto* enemy : opponents) {
            bool duration = false;
            for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles)
                if (profile.abilityId == enemy->abilityId) duration = profile.resolved;
            resolved &= duration;
            enemy->status = {};
            enemy->status.present = enemy->status.hasSleepTurnsRemaining = true;
            enemy->status.effect = PokemonStatusEffect::Sleep;
            enemy->status.sleepTurnsRemaining = 8;
            for (uint8_t slot = 0; slot < enemy->moveCount; ++slot) enemy->moves[slot].pp = 0;
        }
        if (!resolved || !game.doubleBattleSupported()) continue;
        uint32_t full = 0, spread = 0;
        if (calculatePokemonDamageCore(field.player.battleState, field.enemy.battleState,
                swift->id, false, full, nullptr, nullptr, &one) != PokemonDamageCoreResult::Ok ||
            calculatePokemonDamageCore(field.player.battleState, field.enemy.battleState,
                swift->id, false, spread, nullptr, nullptr, &two) != PokemonDamageCoreResult::Ok ||
            spread > full || !spread) return 10373;
        const uint16_t playerHp = field.player.battleState.hp;
        const uint16_t enemyHp = field.enemy.battleState.hp, secondHp = field.secondEnemy.battleState.hp;
        if (!game.advanceBattleTurn()) return 10374;
        if (game.battleFinished() || !field.enemy.battleState.hp || !field.secondEnemy.battleState.hp) continue;
        if (field.player.battleState.hp != playerHp || field.player.battleState.moves[0].pp ||
            field.player.battleState.moves[0].moveId != swift->id ||
            field.enemy.battleState.hp >= enemyHp || field.secondEnemy.battleState.hp >= secondHp) return 10375;
        NativeRunSave saved{};
        FirstRunRuntime restored(seed);
        const auto saveResult = game.captureNativeRunSave(saved);
        const bool restoreResult = saveResult == NativeSaveResult::Ok && restored.restoreNativeRunSave(saved);
        if (saveResult != NativeSaveResult::Ok || !restoreResult)
            std::printf("Area checkpoint failed: seed=%u save=%u restore=%u\n", static_cast<unsigned>(seed),
                static_cast<unsigned>(saveResult), static_cast<unsigned>(restoreResult));
        if (saveResult != NativeSaveResult::Ok || !restoreResult ||
            restored.presentation().player.battleState.moves[0].moveId != swift->id ||
            restored.presentation().player.battleState.moves[0].pp ||
            restored.presentation().enemy.battleState.hp != field.enemy.battleState.hp ||
            restored.presentation().secondEnemy.battleState.hp != field.secondEnemy.battleState.hp) return 10376;
        return 0;
    }
    return 10377;
}

static int checkDoubleAreaActionChecksAndLastPp() {
    using namespace Pokerogue3DS;
    const auto* growl = PokerogueContent::findMoveById(45);
    if (!growl || !growl->target || std::strcmp(growl->target, "ALL_NEAR_ENEMIES") || growl->pp <= 0)
        return 10360;
    for (uint32_t seed = 1; seed <= 1024; ++seed) {
        FirstRunRuntime source(seed);
        if (!source.doubleBattle()) continue;
        auto& field = const_cast<PresentationContext&>(source.presentation());
        PokemonBattleState* actors[] = {&field.player.battleState, &field.enemy.battleState,
            &field.secondEnemy.battleState};
        bool policiesResolved = true;
        uint8_t playerSleepReduction = 0;
        for (uint8_t actorIndex = 0; actorIndex < 3; ++actorIndex) {
            auto& actor = *actors[actorIndex];
            bool durationResolved = false;
            for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles)
                if (profile.abilityId == actor.abilityId) {
                    durationResolved = profile.resolved;
                    if (!actorIndex) playerSleepReduction = profile.sleepReduction;
                }
            policiesResolved &= durationResolved;
            actor.status = {};
            actor.status.present = actor.status.hasSleepTurnsRemaining = true;
            actor.status.effect = PokemonStatusEffect::Sleep;
            actor.status.sleepTurnsRemaining = 8;
            for (auto& stage : actor.statStages) stage = 0;
            for (uint8_t slot = 0; slot < actor.moveCount; ++slot) actor.moves[slot].pp = 0;
        }
        // Canonical move in a test-only actor setup, not production starter data.
        actors[0]->moveCount = 1;
        actors[0]->moves[0].moveId = growl->id;
        actors[0]->moves[0].maxPp = static_cast<uint8_t>(growl->pp);
        actors[0]->moves[0].pp = 1;
        if (!policiesResolved || !source.doubleBattleSupported()) continue;
        const uint16_t beforeHp[] = {actors[0]->hp, actors[1]->hp, actors[2]->hp};
        FirstRunRuntime asleep = source;
        if (!asleep.advanceBattleTurn()) return 10361;
        const auto& sleeping = asleep.presentation();
        if (sleeping.player.battleState.status.toxicTurnCount != 1 ||
            sleeping.player.battleState.status.sleepTurnsRemaining != 7u - playerSleepReduction ||
            sleeping.player.battleState.moves[0].pp != 1 ||
            sleeping.player.battleState.hp != beforeHp[0] ||
            sleeping.enemy.battleState.statStages[0] || sleeping.secondEnemy.battleState.statStages[0])
            return 10362; // Cancel entire action; do not recheck/cancel separately per target.
        FirstRunRuntime awake = source;
        const_cast<PresentationContext&>(awake.presentation()).player.battleState.status = {};
        if (!awake.advanceBattleTurn()) return 10363;
        const auto& completed = awake.presentation();
        if (completed.player.battleState.moves[0].moveId != growl->id ||
            completed.player.battleState.moves[0].pp ||
            completed.enemy.battleState.statStages[0] != -1 ||
            completed.secondEnemy.battleState.statStages[0] != -1 ||
            completed.player.battleState.hp != beforeHp[0] ||
            completed.enemy.battleState.hp != beforeHp[1] ||
            completed.secondEnemy.battleState.hp != beforeHp[2]) return 10364;
        FirstRunRuntime invalidSecond = source;
        auto& invalidField = const_cast<PresentationContext&>(invalidSecond.presentation());
        invalidField.player.battleState.status = {};
        invalidField.secondEnemy.battleState.statStages[0] = 7; // Late target failure, test-only.
        if (invalidSecond.advanceBattleTurn() || invalidField.player.battleState.moves[0].pp != 1 ||
            invalidField.enemy.battleState.statStages[0] ||
            invalidField.secondEnemy.battleState.statStages[0] != 7 ||
            invalidField.player.battleState.hp != beforeHp[0] ||
            invalidField.enemy.battleState.hp != beforeHp[1]) return 10368;
        FirstRunRuntime confused = source;
        auto& confusedPlayer = const_cast<PresentationContext&>(confused.presentation()).player.battleState;
        confusedPlayer.status = {};
        confusedPlayer.confusion = {};
        confusedPlayer.confusion.present = true;
        confusedPlayer.confusion.turns = 3;
        confusedPlayer.confusion.sourceMoveId = 109;
        confusedPlayer.confusion.sourceMoveResolved = true;
        confusedPlayer.confusion.sourcePokemonResolved = true;
        confusedPlayer.confusion.sourcePokemonId = field.enemy.battleState.pokemonId;
        if (!confused.advanceBattleTurn()) return 10365;
        const auto& outcome = confused.presentation();
        const bool executed = !outcome.player.battleState.moves[0].pp;
        if (outcome.player.battleState.confusion.turns != 2 ||
            outcome.enemy.battleState.statStages[0] != (executed ? -1 : 0) ||
            outcome.secondEnemy.battleState.statStages[0] != (executed ? -1 : 0)) return 10366;
        return 0;
    }
    return 10367; // Require the full real field -> area action path, never silently skip.
}

static int checkDoubleSingleTargetSleepCheckpoint() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 1024; ++seed) {
        FirstRunRuntime game(seed);
        if (!game.doubleBattle() || !game.doubleBattleSupported()) continue;
        auto& player = const_cast<PresentationContext&>(game.presentation()).player.battleState;
        // Use the already supported virtual single-target action, retaining real
        // actor identity and original moves. Mutations are exclusively test setup.
        for (uint8_t i = 0; i < player.moveCount; ++i) player.moves[i].pp = 0;
        if (!game.doubleBattleSupported()) continue;
        bool durationResolved = false;
        uint8_t reduction = 0;
        for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles)
            if (profile.abilityId == player.abilityId) {
                durationResolved = profile.resolved;
                reduction = profile.sleepReduction;
            }
        if (!durationResolved) continue;
        player.status = {};
        player.status.present = true;
        player.status.effect = PokemonStatusEffect::Sleep;
        player.status.hasSleepTurnsRemaining = true;
        player.status.sleepTurnsRemaining = 8;
        auto& field = const_cast<PresentationContext&>(game.presentation());
        bool enemyPoliciesResolved = true;
        PokemonBattleState* sleepingEnemies[] = {&field.enemy.battleState, &field.secondEnemy.battleState};
        for (auto* enemy : sleepingEnemies) {
            bool resolved = false;
            for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles)
                if (profile.abilityId == enemy->abilityId) resolved = profile.resolved;
            enemyPoliciesResolved &= resolved;
            enemy->status = player.status;
            for (uint8_t i = 0; i < enemy->moveCount; ++i) enemy->moves[i].pp = 0;
        }
        if (!enemyPoliciesResolved || !game.doubleBattleSupported()) continue;
        const auto playerHp = player.hp;
        const auto enemyHp = game.presentation().enemy.battleState.hp;
        const auto secondHp = game.presentation().secondEnemy.battleState.hp;
        if (!game.advanceBattleTurn()) return 10350;
        if (!player.hp || game.battleFinished()) continue;
        if (player.hp != playerHp || !player.status.present || player.status.effect != PokemonStatusEffect::Sleep ||
            player.status.toxicTurnCount != 1 || player.status.sleepTurnsRemaining != 7u - reduction ||
            game.presentation().enemy.battleState.hp != enemyHp ||
            game.presentation().secondEnemy.battleState.hp != secondHp) return 10351;
        for (uint8_t i = 0; i < player.moveCount; ++i)
            if (player.moves[i].pp) return 10352;
        NativeRunSave checkpoint{};
        if (game.captureNativeRunSave(checkpoint) != NativeSaveResult::Ok) return 10353;
        FirstRunRuntime left(seed), right(seed);
        if (!left.restoreNativeRunSave(checkpoint) || !right.restoreNativeRunSave(checkpoint) ||
            !left.advanceBattleTurn() || !right.advanceBattleTurn()) return 10354;
        const auto& a = left.presentation().player.battleState;
        const auto& b = right.presentation().player.battleState;
        if (a.hp != b.hp || a.status.toxicTurnCount != b.status.toxicTurnCount ||
            a.status.sleepTurnsRemaining != b.status.sleepTurnsRemaining ||
            left.presentation().enemy.battleState.hp != right.presentation().enemy.battleState.hp ||
            left.presentation().secondEnemy.battleState.hp != right.presentation().secondEnemy.battleState.hp)
            return 10355;
        return 0;
    }
    return 10356; // Require a real double encounter with a resolved sleep policy.
}

static int checkDoubleStatusResidualCheckpoint() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 1024; ++seed) {
        FirstRunRuntime baseline(seed), affected(seed);
        if (!baseline.doubleBattle() || !baseline.doubleBattleSupported()) continue;
        const auto& original = baseline.presentation().secondEnemy;
        bool supported = false;
        for (const auto& profile : PokerogueContent::kStatusResidualAbilityProfiles)
            if (profile.abilityId == original.battleState.abilityId)
                supported = profile.resolved && !profile.healedStatusMask;
        if (!supported) continue;
        auto& target = const_cast<PresentationContext&>(affected.presentation()).secondEnemy.battleState;
        target.status = {};
        target.status.present = true;
        target.status.effect = PokemonStatusEffect::Toxic;
        target.status.toxicTurnCount = 2; // Test-only already-poisoned real generated actor.
        if (!baseline.advanceBattleTurn()) return 10340;
        if (baseline.battleFinished() || !baseline.presentation().secondEnemy.battleState.hp) continue;
        auto expected = baseline.presentation().secondEnemy.battleState;
        expected.status = target.status;
        PokemonStatusResidualPolicy policy{};
        PokemonStatusResidualEvent event{};
        if (!resolvePokemonStatusResidualPolicy(expected.abilityId, expected.status.effect, true, true, policy)) return 10341;
        const auto result = applyPokemonStatusResidual(expected, policy, event);
        if (result != PokemonStatusResidualResult::Applied && result != PokemonStatusResidualResult::Blocked) return 10341;
        if (!expected.hp) continue; // This case exercises live checkpoint persistence, not EXP/faint callbacks.
        if (!affected.advanceBattleTurn() || target.hp != expected.hp || !target.status.present ||
            target.status.effect != PokemonStatusEffect::Toxic || target.status.toxicTurnCount != 3) return 10342;
        NativeRunSave checkpoint{};
        if (affected.captureNativeRunSave(checkpoint) != NativeSaveResult::Ok ||
            checkpoint.secondEnemy.status.toxicTurnCount != 3) return 10343;
        FirstRunRuntime restored(seed);
        if (!restored.restoreNativeRunSave(checkpoint) ||
            restored.presentation().secondEnemy.battleState.hp != target.hp ||
            restored.presentation().secondEnemy.battleState.status.toxicTurnCount != 3) return 10344;
        return 0;
    }
    return 10345; // Require real double residual -> checkpoint -> restore coverage.
}

static int checkDoublePartialExperienceCheckpoint() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 1024; ++seed) {
        FirstRunRuntime game(seed);
        if (!game.doubleBattle()) continue;
        auto& field = const_cast<PresentationContext&>(game.presentation());
        if (!field.enemy.actorIdentityResolved || !field.secondEnemy.actorIdentityResolved) continue;
        field.enemy.battleState.hp = 1; // Test-only near-faint state of the real first enemy.
        for (uint8_t slot = 0; slot < field.enemy.battleState.moveCount; ++slot)
            field.enemy.battleState.moves[slot].pp = 0;
        if (!game.doubleBattleSupported()) continue;
        const auto beforeExperience = field.player.totalExperience;
        if (!game.advanceBattleTurn()) return 10300;
        if (game.battleFinished() || field.enemy.battleState.hp || !field.secondEnemy.battleState.hp) continue;
        for (unsigned decision = 0; decision < 32 && (game.moveLearningPending() || game.evolutionPending()); ++decision) {
            if (game.moveLearningPending()) { if (!game.resolvePendingLearnMove(-1)) return 10301; }
            else if (!game.skipVictoryReward()) return 10302;
        }
        NativeRunSave checkpoint{};
        if (game.captureNativeRunSave(checkpoint) != NativeSaveResult::Ok || !checkpoint.doubleBattle ||
            checkpoint.stage != NativeSaveStage::BattleActive || checkpoint.doubleExperienceGrantedMask != 1 ||
            checkpoint.playerExperience <= beforeExperience) return 10303;
        FirstRunRuntime restored(seed);
        if (!restored.restoreNativeRunSave(checkpoint)) return 10304;
        NativeRunSave repeated{};
        if (restored.captureNativeRunSave(repeated) != NativeSaveResult::Ok ||
            repeated.playerExperience != checkpoint.playerExperience || repeated.doubleExperienceGrantedMask != 1 ||
            repeated.enemyHp || repeated.secondEnemy.hp != checkpoint.secondEnemy.hp) return 10305;
        if (!restored.advanceBattleTurn()) return 10306;
        if (restored.presentation().secondEnemy.battleState.hp &&
            restored.presentation().player.totalExperience != checkpoint.playerExperience) return 10307;
        auto invalid = checkpoint;
        invalid.doubleExperienceGrantedMask = 0; // Already-defeated enemy cannot lose its award history.
        if (restored.restoreNativeRunSave(invalid)) return 10308;
        auto defeat = checkpoint;
        defeat.stage = NativeSaveStage::BattleLost;
        defeat.playerHp = defeat.playerParty[defeat.activePlayerMember].hp = 0;
        defeat.enemyHp = defeat.secondEnemy.hp = 0;
        FirstRunRuntime lost(seed);
        if (!lost.restoreNativeRunSave(defeat) || !lost.battleFinished() || lost.playerWon() ||
            lost.captureNativeRunSave(repeated) != NativeSaveResult::Ok || repeated.stage != NativeSaveStage::BattleLost ||
            repeated.enemyHp || repeated.secondEnemy.hp) return 10309;
        return 0;
    }
    return 10310; // Require a real partial defeat, never skip the scenario silently.
}

static int checkDoubleCheckpointRoundtrip() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 512; ++seed) {
        FirstRunRuntime game(seed);
        if (!game.doubleBattle() || !game.doubleBattleSupported()) continue;
        if (!game.cycleTarget(1) || !game.advanceBattleTurn()) return 10280;
        NativeRunSave snapshot{};
        const auto status = game.captureNativeRunSave(snapshot);
        if (status == NativeSaveResult::UnsupportedStage && (game.moveLearningPending() || game.evolutionPending())) continue;
        if (status != NativeSaveResult::Ok || !snapshot.doubleBattle || !snapshot.playerPartyCount ||
            !snapshot.globalRngResolved || snapshot.secondEnemy.speciesDex != game.presentation().secondEnemy.dex)
            return 10281;
        char bytes[kNativeSaveMaxBytes]{};
        size_t length = 0;
        NativeRunSave decoded{};
        if (encodeNativeRunSave(snapshot, bytes, sizeof(bytes), length) != NativeSaveResult::Ok ||
            decodeNativeRunSave(bytes, length, PokerogueContent::kContentHash, decoded) != NativeSaveResult::Ok) return 10282;
        FirstRunRuntime restored(seed), replay(seed);
        NativeRunSave recaptured{};
        if (!restored.restoreNativeRunSave(decoded) || !replay.restoreNativeRunSave(decoded) ||
            restored.captureNativeRunSave(recaptured) != NativeSaveResult::Ok ||
            !restored.doubleBattle() || restored.selectedTarget() != snapshot.selectedTarget ||
            recaptured.secondEnemy.hp != snapshot.secondEnemy.hp ||
            recaptured.secondEnemy.pokemonId != snapshot.secondEnemy.pokemonId ||
            recaptured.secondEnemyBoss.segmentCount != snapshot.secondEnemyBoss.segmentCount ||
            recaptured.secondEnemyBoss.segmentIndex != snapshot.secondEnemyBoss.segmentIndex ||
            recaptured.doubleExperienceGrantedMask != snapshot.doubleExperienceGrantedMask ||
            recaptured.globalRng.s0 != snapshot.globalRng.s0 || recaptured.globalRng.s1 != snapshot.globalRng.s1 ||
            recaptured.globalRng.s2 != snapshot.globalRng.s2 || recaptured.globalRng.carry != snapshot.globalRng.carry)
            return 10283;
        for (uint8_t slot = 0; slot < snapshot.secondEnemy.moveCount; ++slot)
            if (recaptured.secondEnemy.moveIds[slot] != snapshot.secondEnemy.moveIds[slot] ||
                recaptured.secondEnemy.pp[slot] != snapshot.secondEnemy.pp[slot]) return 10284;
        auto invalid = snapshot;
        invalid.doubleExperienceGrantedMask = 3; // Invalid if either enemy still lives.
        if ((snapshot.enemyHp || snapshot.secondEnemy.hp) &&
            validateNativeRunSave(invalid, PokerogueContent::kContentHash) != NativeSaveResult::InvalidRecord) return 10285;
        invalid = snapshot;
        ++invalid.secondEnemy.pokemonId; // Wrong seeded encounter identity; immutable data must not be accepted.
        if (restored.restoreNativeRunSave(invalid)) return 10286;
        if (!game.battleFinished()) {
            if (!restored.advanceBattleTurn() || !replay.advanceBattleTurn()) return 10287;
            if (restored.presentation().player.battleState.hp != replay.presentation().player.battleState.hp ||
                restored.presentation().enemy.battleState.hp != replay.presentation().enemy.battleState.hp ||
                restored.presentation().secondEnemy.battleState.hp != replay.presentation().secondEnemy.battleState.hp)
                return 10288;
        }
        return 0;
    }
    return 10289; // No silent skip of the real save/codec/restore pipeline.
}

static int checkDoubleExhaustedPpStruggle() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 512; ++seed) {
        FirstRunRuntime a(seed), b(seed);
        if (!a.doubleBattle()) continue;
        auto& fieldA = const_cast<PresentationContext&>(a.presentation());
        auto& fieldB = const_cast<PresentationContext&>(b.presentation());
        // Test-only PP exhaustion of real generated actors; double checkpoint codec is still pending.
        ResolvedPokemon* actorsA[] = {&fieldA.player, &fieldA.enemy, &fieldA.secondEnemy};
        ResolvedPokemon* actorsB[] = {&fieldB.player, &fieldB.enemy, &fieldB.secondEnemy};
        uint16_t originalMoves[3][4]{};
        bool resolved = true;
        for (uint8_t i = 0; i < 3; ++i) {
            resolved &= actorsA[i]->actorIdentityResolved;
            for (uint8_t slot = 0; slot < actorsA[i]->battleState.moveCount; ++slot) {
                originalMoves[i][slot] = actorsA[i]->battleState.moves[slot].moveId;
                actorsA[i]->battleState.moves[slot].pp = actorsB[i]->battleState.moves[slot].pp = 0;
            }
        }
        if (!resolved || !a.doubleBattleSupported()) continue;
        const auto enemyHp = fieldA.enemy.battleState.hp;
        const auto secondHp = fieldA.secondEnemy.battleState.hp;
        if (!b.cycleTarget(1) || !a.advanceBattleTurn() || !b.advanceBattleTurn()) return 10250;
        // Manual target cursor must not control Struggle's random target.
        for (uint8_t i = 0; i < 3; ++i) {
            if (actorsA[i]->battleState.hp != actorsB[i]->battleState.hp ||
                actorsA[i]->battleState.moveCount != actorsB[i]->battleState.moveCount) return 10251;
            for (uint8_t slot = 0; slot < actorsA[i]->battleState.moveCount; ++slot)
                if (actorsA[i]->battleState.moves[slot].pp || actorsB[i]->battleState.moves[slot].pp ||
                    actorsA[i]->battleState.moves[slot].moveId != actorsB[i]->battleState.moves[slot].moveId ||
                    actorsA[i]->battleState.moves[slot].moveId != originalMoves[i][slot]) return 10252;
        }
        if (fieldA.enemy.battleState.hp == enemyHp && fieldA.secondEnemy.battleState.hp == secondHp) return 10253;
        return 0;
    }
    return 10254; // No silent skip of a real eligible double field.
}

static int checkExhaustedPpStruggleReplay() {
    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 256; ++seed) {
        FirstRunRuntime original(seed);
        const auto& context = original.presentation();
        if (!context.enemy.actorIdentityResolved || context.secondEnemy.dex || context.trainerPartyCount) continue;
        NativeRunSave save{};
        if (original.captureNativeRunSave(save) != NativeSaveResult::Ok) return 10220;
        save.stage = NativeSaveStage::BattleActive;
        save.battleTurn = 1;
        save.encounterDex = context.enemy.dex;
        save.playerHp = context.player.battleState.hp;
        save.enemyHp = context.enemy.battleState.hp;
        save.playerMoveCount = context.player.battleState.moveCount;
        save.enemyMoveCount = context.enemy.battleState.moveCount;
        for (uint8_t slot = 0; slot < 4; ++slot) {
            save.playerMoveIds[slot] = slot < save.playerMoveCount ? context.player.battleState.moves[slot].moveId : 0;
            save.enemyMoveIds[slot] = slot < save.enemyMoveCount ? context.enemy.battleState.moves[slot].moveId : 0;
            save.playerPp[slot] = save.enemyPp[slot] = 0; // Test-only exhausted checkpoint.
        }
        auto actor = context.player.battleState;
        for (uint8_t slot = 0; slot < actor.moveCount; ++slot) actor.moves[slot].pp = 0;
        save.playerPartyCount = 1;
        save.activePlayerMember = 0;
        if (!captureNativePokemonActorSave(actor, context.player.actor, context.player.totalExperience, save.playerParty[0]))
            return 10221;
        FirstRunRuntime a(seed), b(seed);
        if (!a.restoreNativeRunSave(save) || !b.restoreNativeRunSave(save)) return 10222;
        if (!a.battleInputSupported()) continue; // Search real resolved ability context, never fabricate one.
        if (!a.selectBattleMove(1) || !a.advanceBattleTurn() || !b.advanceBattleTurn()) return 10223;
        const auto& pa = a.presentation();
        const auto& pb = b.presentation();
        if (pa.player.battleState.hp != pb.player.battleState.hp || pa.enemy.battleState.hp != pb.enemy.battleState.hp ||
            pa.player.battleState.hp >= save.playerHp || pa.enemy.battleState.hp >= save.enemyHp) return 10224;
        for (uint8_t slot = 0; slot < save.playerMoveCount; ++slot)
            if (pa.player.battleState.moves[slot].moveId != save.playerMoveIds[slot] || pa.player.battleState.moves[slot].pp)
                return 10225;
        for (uint8_t slot = 0; slot < save.enemyMoveCount; ++slot)
            if (pa.enemy.battleState.moves[slot].moveId != save.enemyMoveIds[slot] || pa.enemy.battleState.moves[slot].pp)
                return 10226;
        return 0;
    }
    return 10227; // Fail if no real eligible encounter was exercised.
}

static int checkLegacyFirstRunRestore() {

    // A high new level must not wrap to negative and hide real low-level moves.
    {
        Pokerogue3DS::PokemonBattleState learner{};
        learner.speciesDex = PokerogueContent::kSpecies[0].dex;
        uint16_t ids[4]{};
        uint8_t count = 0;
        if (!Pokerogue3DS::learnNewLevelMoves(learner.speciesDex, 1, 128, learner, ids, count) ||
            !count || count != learner.moveCount) return 214;
        bool formMoveCovered = false;
        for (const auto& form : PokerogueContent::kForms) {
            const auto* species = Pokerogue3DS::findSpeciesById(form.speciesId);
            const auto* rows = PokerogueContent::levelMovesFor(form);
            if (!species || !rows) continue;
            for (uint16_t i = 0; i < form.learnsetCount; ++i) {
                if (rows[i].level < 2) continue;
                const uint16_t level = static_cast<uint16_t>(rows[i].level);
                bool speciesHasMove = false;
                const auto* baseRows = PokerogueContent::levelMovesFor(*species);
                for (uint16_t j = 0; baseRows && j < species->learnsetCount; ++j)
                    speciesHasMove |= baseRows[j].moveId == rows[i].moveId;
                if (speciesHasMove) continue;
                Pokerogue3DS::PokemonBattleState formLearner{};
                formLearner.speciesDex = species->dex;
                formLearner.formId = form.id;
                uint16_t formIds[4]{};
                uint8_t formMoveCount = 0;
                if (!Pokerogue3DS::learnNewLevelMoves(species->dex, level - 1, level,
                        formLearner, formIds, formMoveCount)) return 300;
                bool found = false;
                for (uint8_t slot = 0; slot < formMoveCount; ++slot) found |= formIds[slot] == rows[i].moveId;
                if (!found) return 301;
                formMoveCovered = true;
                break;
            }
            if (formMoveCovered) break;
        }
        if (!formMoveCovered) return 302;
        const auto* alreadyEligible = Pokerogue3DS::checkSpeciesLevelEvolution("bulbasaur", 20, 21);
        if (!alreadyEligible || alreadyEligible->level != 16) return 215;
        // A level alone must not select an item/condition/form evolution.
        for (const auto& edge : PokerogueContent::kSpeciesEvolutions) {
            const auto* selected = Pokerogue3DS::checkSpeciesLevelEvolution(edge.sourceSpeciesId, 1, 100);
            if (!selected) continue;
            bool eligible = false;
            for (const auto& row : PokerogueContent::kSimpleLevelEvolutionProfiles)
                if (row.sourceOrder == selected->sourceOrder &&
                    Pokerogue3DS::pokemonEvolutionTextEqual(row.speciesId, selected->sourceSpeciesId)) eligible = true;
            if (!eligible) return 216;
        }
    }
    // Catch rates must be generated for the full real canonical species catalog.
    if (sizeof(PokerogueContent::kSpeciesCatchProfiles) / sizeof(PokerogueContent::kSpeciesCatchProfiles[0]) !=
            PokerogueContent::kSpeciesCount) return 210;
    for (const auto& species : PokerogueContent::kSpecies) {
        bool found = false;
        for (const auto& row : PokerogueContent::kSpeciesCatchProfiles) {
            if (row.speciesDex != species.dex) continue;
            if (!row.sourceHash || !*row.sourceHash || !row.sourcePath || !*row.sourcePath) return 211;
            found = true;
        }
        if (!found) return 212;
    }
    {
        Pokerogue3DS::FirstRunRuntime game(1);
        const auto beforeRng = game.battleRng().state();
        const auto beforeDex = game.presentation().player.dex;
        const auto beforeHp = game.presentation().player.battleState.hp;
        if (game.switchPlayerPokemon(255) || game.presentation().player.dex != beforeDex ||
            game.presentation().player.battleState.hp != beforeHp ||
            game.pokeballCount(Pokerogue3DS::PokeballType::Pokeball) != 5) return 213;
        using namespace Pokerogue3DS;
        NativeRunSave beforeTransition{}, afterTransition{};
        game.captureNativeRunSave(beforeTransition);
        if (game.skipVictoryReward() || game.claimRewardChoice()) return 226;
        game.captureNativeRunSave(afterTransition);
        char beforeBytes[kNativeSaveMaxBytes]{}, afterBytes[kNativeSaveMaxBytes]{};
        size_t beforeLength = 0, afterLength = 0;
        if (encodeNativeRunSave(beforeTransition, beforeBytes, sizeof(beforeBytes), beforeLength) != NativeSaveResult::Ok ||
            encodeNativeRunSave(afterTransition, afterBytes, sizeof(afterBytes), afterLength) != NativeSaveResult::Ok ||
            beforeLength != afterLength || std::memcmp(beforeBytes, afterBytes, beforeLength)) return 227;
        (void)beforeRng; // Invalid command must not publish a party or inventory mutation.
    }

    using namespace Pokerogue3DS;
    for (uint32_t seed = 1; seed <= 64; ++seed) {
        FirstRunRuntime game(seed);
        const auto& context = game.presentation();
        if (!context.enemy.actorIdentityResolved || context.secondEnemy.dex) continue;
        NativeRunSave setup{};
        game.captureNativeRunSave(setup);
        NativeRunSave active = setup;
        active.stage = NativeSaveStage::BattleActive;
        active.encounterDex = context.enemy.dex;
        active.playerHp = context.player.battleState.hp;
        active.enemyHp = context.enemy.battleState.hp;
        active.battleTurn = 1;
        active.playerMoveCount = context.player.battleState.moveCount;
        active.enemyMoveCount = context.enemy.battleState.moveCount;
        for (uint8_t slot = 0; slot < active.playerMoveCount; ++slot) {
            active.playerMoveIds[slot] = context.player.battleState.moves[slot].moveId;
            active.playerPp[slot] = context.player.battleState.moves[slot].pp;
        }
        for (uint8_t slot = 0; slot < active.enemyMoveCount; ++slot) {
            active.enemyMoveIds[slot] = context.enemy.battleState.moves[slot].moveId;
            active.enemyPp[slot] = context.enemy.battleState.moves[slot].pp;
        }
        if (validateNativeRunSave(active, PokerogueContent::kContentHash) != NativeSaveResult::Ok)
            return 1;
        active.playerStatStages[0] = -2;
        active.enemyStatStages[4] = 3;
        NativeRunSave lateInvalid = active;
        lateInvalid.enemyHp = 65535; // Structurally valid, fails reconstructed max HP.
        if (validateNativeRunSave(lateInvalid, PokerogueContent::kContentHash) != NativeSaveResult::Ok ||
            game.restoreNativeRunSave(lateInvalid)) return 2;
        NativeRunSave afterRejected{};
        game.captureNativeRunSave(afterRejected);
        char beforeBytes[kNativeSaveMaxBytes]{}, afterBytes[kNativeSaveMaxBytes]{};
        size_t beforeSize = 0, afterSize = 0;
        if (encodeNativeRunSave(setup, beforeBytes, sizeof(beforeBytes), beforeSize) != NativeSaveResult::Ok ||
            encodeNativeRunSave(afterRejected, afterBytes, sizeof(afterBytes), afterSize) != NativeSaveResult::Ok ||
            beforeSize != afterSize || std::memcmp(beforeBytes, afterBytes, beforeSize)) return 3;
        active.starterProfileGeneration = 17;
        active.participantHistoryResolved = true;
        active.participantCount = 1;
        active.participantIds[0] = context.player.battleState.pokemonId;
        auto foreignParticipant = active;
        foreignParticipant.participantIds[0] ^= 0xffffffffU;
        if (game.restoreNativeRunSave(foreignParticipant)) return 520;
        NativeRunSave unchangedParticipants{};
        game.captureNativeRunSave(unchangedParticipants);
        if (!unchangedParticipants.participantHistoryResolved || unchangedParticipants.participantCount)
            return 521;
        if (!game.restoreNativeRunSave(active)) return 4;
        if (!game.scene().nodes || !game.scene().nodeCount ||
            !game.scene().nodes[0].text || std::strcmp(game.scene().nodes[0].text,
                "POKEROGUE 3DS - CLASSIC 1")) return 5;
        NativeRunSave loaded{};
        game.captureNativeRunSave(loaded);
        if (loaded.stage != NativeSaveStage::BattleActive || loaded.enemyHp != active.enemyHp ||
            loaded.seed != seed || loaded.playerStatStages[0] != -2 ||
            loaded.enemyStatStages[4] != 3 || loaded.starterProfileGeneration != 17 || !loaded.participantHistoryResolved ||
            loaded.participantCount != 1 || loaded.participantIds[0] != active.participantIds[0]) return 6;
        if (std::strcmp(loaded.biomeId, game.run().biomeId)) return 10110;
        // A valid but different biome is not the checkpoint's reconstructed arena.
        auto mismatchedBiome = loaded;
        // Exercise legacy seed replay: explicit actors intentionally use the saved arena.
        mismatchedBiome.playerPartyCount = 0;
        mismatchedBiome.activePlayerMember = 0xff;
        for (auto& member : mismatchedBiome.playerParty) member = {};
        std::strcpy(mismatchedBiome.biomeId, "forest");
        FirstRunRuntime mismatchedRuntime(seed);
        if (mismatchedRuntime.restoreNativeRunSave(mismatchedBiome)) return 10111;
        // Restore binds the arena ID to canonical storage, not the input buffer.
        auto borrowedCheckpoint = loaded;
        FirstRunRuntime stableBiome(seed);
        if (!stableBiome.restoreNativeRunSave(borrowedCheckpoint)) return 10112;
        borrowedCheckpoint.biomeId[0] = 'x';
        if (std::strcmp(stableBiome.run().biomeId, loaded.biomeId)) return 10113;
        NativePokemonSave actorSnapshot{};
        const auto& currentActor = game.presentation().player;
        if (!captureNativePokemonActorSave(currentActor.battleState, currentActor.actor, currentActor.totalExperience,
                actorSnapshot)) return 269;
        PokemonBattleState restoredActor{};
        if (!restoreNativePokemonSave(actorSnapshot, restoredActor) ||
            restoredActor.pokemonId != currentActor.battleState.pokemonId ||
            restoredActor.hp != currentActor.battleState.hp ||
            restoredActor.formId == actorSnapshot.formId ||
            (actorSnapshot.formId[0] ? (!restoredActor.formId || std::strcmp(restoredActor.formId, actorSnapshot.formId))
                : restoredActor.formId != nullptr)) return 270;
        for (uint8_t stat = 0; stat < 6; ++stat)
            if (restoredActor.stats[stat] != currentActor.battleState.stats[stat] ||
                restoredActor.ivs[stat] != currentActor.battleState.ivs[stat]) return 271;
        for (uint8_t slot = 0; slot < restoredActor.moveCount; ++slot)
            if (restoredActor.moves[slot].moveId != currentActor.battleState.moves[slot].moveId ||
                restoredActor.moves[slot].pp != currentActor.battleState.moves[slot].pp) return 272;
        // Real species + pinned candy plan with an explicit uncapped-limit override.
        const auto* candySpecies = PokerogueContent::findSpeciesByDex(actorSnapshot.speciesDex);
        PokemonLevelIncrementPlan candyPlan{};
        if (!candySpecies || planPokemonLevelIncrement(candySpecies->growthRate,
                actorSnapshot.level, actorSnapshot.experience, 0, true, actorSnapshot.level, candyPlan) !=
                PokemonExperienceResult::Ok || candyPlan.progress.totalExperience != actorSnapshot.experience)
            return 501;
        auto candyActor = currentActor.battleState;
        if (!recalculatePokemonBattleLevel(candyActor, candyPlan.progress.level)) return 502;
        NativePokemonSave candySnapshot{}, decodedCandy{};
        if (!captureNativePokemonActorSave(candyActor, currentActor.actor, candyPlan.progress.totalExperience, candySnapshot)) return 503;
        char candyPayload[1024]{};
        size_t candyPayloadSize = 0;
        if (encodeNativePokemonSave(candySnapshot, candyPayload, sizeof(candyPayload), candyPayloadSize) !=
                NativeSaveResult::Ok || decodeNativePokemonSave(candyPayload, candyPayloadSize, decodedCandy) !=
                NativeSaveResult::Ok || decodedCandy.experience != actorSnapshot.experience ||
            decodedCandy.level != candyPlan.progress.level) return 504;
        PokemonBattleState candyRestored{};
        if (!restoreNativePokemonSave(decodedCandy, candyRestored) ||
            candyRestored.level != candyActor.level || candyRestored.maxHp != candyActor.maxHp ||
            candyRestored.hp != candyActor.hp || candyRestored.friendship != candyActor.friendship)
            return 505;
        auto friendshipActor = currentActor.battleState;
        const auto* friendshipRoot = pokemonRootSpecies(friendshipActor.speciesDex);
        if (!friendshipRoot) return 506;
        NativeStarterCandyRecord friendshipRecord{friendshipRoot->dex, 0, 0};
        StarterCandyAwardEvent friendshipEvent{};
        PokemonFriendshipPolicy friendshipPolicy{};
        friendshipActor.friendship = 70;
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord, 3, friendshipPolicy, false,
                friendshipEvent) != NativeFriendshipApplyResult::UnresolvedPolicy ||
            friendshipActor.friendship != 70 || friendshipRecord.friendship) return 507;
        friendshipPolicy.resolved = true;
        friendshipPolicy.candyMultiplier = PokerogueContent::kClassicCandyFriendshipMultiplier;
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord,
                PokerogueContent::kFriendshipGainFromBattle, friendshipPolicy, false, friendshipEvent) !=
                NativeFriendshipApplyResult::Applied || friendshipActor.friendship != 73 ||
            friendshipRecord.friendship != 9 || friendshipRecord.candyCount) return 508;
        friendshipActor.friendship = 254;
        const auto savedCandyFriendship = friendshipRecord.friendship;
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord, 3, friendshipPolicy, false,
                friendshipEvent) != NativeFriendshipApplyResult::UnresolvedPolicy ||
            friendshipActor.friendship != 254 || friendshipRecord.friendship != savedCandyFriendship) return 509;
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord, 3, friendshipPolicy, true,
                friendshipEvent) != NativeFriendshipApplyResult::Applied || friendshipActor.friendship != 255 ||
            friendshipRecord.friendship != savedCandyFriendship + 9) return 510;
        const auto beforeLossProgress = friendshipRecord.friendship;
        PokemonFriendshipPolicy unresolvedLossPolicy{};
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord,
                -static_cast<int32_t>(PokerogueContent::kFriendshipLossFromFaint), unresolvedLossPolicy, false,
                friendshipEvent) != NativeFriendshipApplyResult::Applied || friendshipActor.friendship != 250 ||
            friendshipRecord.friendship != beforeLossProgress || friendshipEvent.requestedAward) return 511;
        friendshipRecord.speciesDex = 0;
        if (applyNativePokemonFriendship(friendshipActor, friendshipRecord, 3, friendshipPolicy, true,
                friendshipEvent) != NativeFriendshipApplyResult::RootMismatch ||
            friendshipActor.friendship != 250) return 512;
        PokemonParticipantExperiencePolicy expPolicy{};
        uint32_t memberAward = 99;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::UnresolvedPolicy || memberAward != 99) return 513;
        expPolicy.resolved = true;
        expPolicy.eligible = true;
        expPolicy.participated = true;
        expPolicy.participantCount = 2;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward != 76) return 514;
        expPolicy.multipleParticipantBonusStacks = 1;
        expPolicy.pokerus = true;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward != 159) return 515;
        expPolicy.participated = false;
        expPolicy.pokerus = false;
        expPolicy.expShareStacks = 2;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward != 30) return 516;
        expPolicy.hasMultiplierOverride = true;
        expPolicy.multiplierOverride = 2.0;
        expPolicy.boosterMultiplier = 1.5;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward != 456) return 517;
        expPolicy.eligible = false;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward) return 518;
        expPolicy.eligible = true;
        expPolicy.participantCount = 0;
        if (pokemonParticipantExperience(101.9, true, expPolicy, memberAward) !=
                PokemonExperienceResult::Ok || memberAward) return 519;
        auto invalidActor = actorSnapshot;
        invalidActor.hp = 65535;
        const uint16_t beforeInvalidHp = restoredActor.hp;
        if (restoreNativePokemonSave(invalidActor, restoredActor) || restoredActor.hp != beforeInvalidHp)
            return 273;
        invalidActor = actorSnapshot;
        invalidActor.nature = 255;
        if (restoreNativePokemonSave(invalidActor, restoredActor)) return 274;
        invalidActor = actorSnapshot;
        invalidActor.ivs[0] = 32;
        if (restoreNativePokemonSave(invalidActor, restoredActor)) return 275;
        invalidActor = actorSnapshot;
        for (auto& ch : invalidActor.formId) ch = 'a';
        if (restoreNativePokemonSave(invalidActor, restoredActor)) return 276;
        if (!captureNativePokemonActorSave(currentActor.battleState, currentActor.actor,
                currentActor.totalExperience, actorSnapshot)) return 277;
        PokemonActorIdentity restoredIdentity{};
        if (!restoreNativePokemonActorSave(actorSnapshot, restoredActor, restoredIdentity) ||
            restoredIdentity.abilityIndex != currentActor.actor.abilityIndex ||
            restoredIdentity.initialTeraTypeIndex != currentActor.actor.initialTeraTypeIndex ||
            !restoredIdentity.initialTeraTypeResolved) return 278;
        auto invalidIdentity = actorSnapshot;
        invalidIdentity.abilityIndex = 3;
        const auto preservedIdentity = restoredIdentity;
        if (restoreNativePokemonActorSave(invalidIdentity, restoredActor, restoredIdentity) ||
            restoredIdentity.pokemonId != preservedIdentity.pokemonId) return 279;
        invalidIdentity = actorSnapshot;
        invalidIdentity.initialTeraTypeResolved = false;
        if (restoreNativePokemonActorSave(invalidIdentity, restoredActor, restoredIdentity)) return 280;
        char actorBytes[512]{};
        size_t actorSize = 0;
        if (encodeNativePokemonSave(actorSnapshot, actorBytes, sizeof(actorBytes), actorSize) !=
                NativeSaveResult::Ok) return 281;
        NativePokemonSave decodedActor{};
        if (decodeNativePokemonSave(actorBytes, actorSize, decodedActor) != NativeSaveResult::Ok ||
            decodedActor.pokemonId != actorSnapshot.pokemonId || decodedActor.experience != actorSnapshot.experience ||
            std::strcmp(decodedActor.formId, actorSnapshot.formId)) return 282;
        char actorBytesAgain[512]{};
        size_t actorSizeAgain = 0;
        if (encodeNativePokemonSave(decodedActor, actorBytesAgain, sizeof(actorBytesAgain), actorSizeAgain) !=
                NativeSaveResult::Ok || actorSizeAgain != actorSize ||
            std::memcmp(actorBytes, actorBytesAgain, actorSize)) return 283;
        const uint32_t preservedActorId = decodedActor.pokemonId;
        if (decodeNativePokemonSave(actorBytes, actorSize - 1, decodedActor) != NativeSaveResult::InvalidFormat ||
            decodedActor.pokemonId != preservedActorId) return 284;
        if (decodeNativePokemonSave(actorBytes, 513, decodedActor) != NativeSaveResult::InvalidFormat) return 285;
        size_t rejectedSize = 99;
        if (encodeNativePokemonSave(actorSnapshot, actorBytesAgain, 1, rejectedSize) != NativeSaveResult::TooLarge ||
            rejectedSize) return 286;
        // Actor v11 preserves Sturdy alongside independently optional status/confusion.
        for (const bool status : {false, true}) for (const bool confused : {false, true}) {
            auto survivalActor = actorSnapshot;
            survivalActor.sturdyTag = true;
            survivalActor.status = {};
            survivalActor.confusion = {};
            if (status) {
                survivalActor.status.present = true;
                survivalActor.status.effect = PokemonStatusEffect::Burn;
            }
            if (confused) survivalActor.confusion = {3, true, 109, true, 42, true};
            char payload[512]{}, repeatedPayload[512]{};
            size_t payloadSize = 0, repeatedSize = 0;
            NativePokemonSave decoded{};
            PokemonBattleState restored{};
            if (encodeNativePokemonSave(survivalActor, payload, sizeof(payload), payloadSize) != NativeSaveResult::Ok ||
                decodeNativePokemonSave(payload, payloadSize, decoded) != NativeSaveResult::Ok ||
                !decoded.sturdyTag || decoded.status.present != status || decoded.confusion.present != confused ||
                (confused && (decoded.confusion.turns != 3 || decoded.confusion.sourcePokemonId != 42)) ||
                !restoreNativePokemonSave(decoded, restored) || !restored.sturdy.present ||
                encodeNativePokemonSave(decoded, repeatedPayload, sizeof(repeatedPayload), repeatedSize) !=
                    NativeSaveResult::Ok || repeatedSize != payloadSize ||
                std::memcmp(payload, repeatedPayload, payloadSize)) return 9940;
            if (decodeNativePokemonSave(payload, payloadSize - 1, decoded) != NativeSaveResult::InvalidFormat ||
                !decoded.sturdyTag) return 9941;
        }
        // Actor payload v7 preserves optional zero counters and full uint32 counts.
        auto statusActor = currentActor.battleState;
        statusActor.status.present = true;
        statusActor.status.effect = PokemonStatusEffect::Sleep;
        statusActor.status.toxicTurnCount = 0xFFFFFFFFu;
        statusActor.status.hasSleepTurnsRemaining = true;
        statusActor.status.sleepTurnsRemaining = 0;
        NativePokemonSave statusSnapshot{};
        if (!captureNativePokemonActorSave(statusActor, currentActor.actor,
                currentActor.totalExperience, statusSnapshot) ||
            encodeNativePokemonSave(statusSnapshot, actorBytesAgain, sizeof(actorBytesAgain), actorSizeAgain) !=
                NativeSaveResult::Ok ||
            decodeNativePokemonSave(actorBytesAgain, actorSizeAgain, decodedActor) != NativeSaveResult::Ok ||
            !restoreNativePokemonActorSave(decodedActor, restoredActor, restoredIdentity) ||
            !restoredActor.status.present || restoredActor.status.effect != PokemonStatusEffect::Sleep ||
            restoredActor.status.toxicTurnCount != 0xFFFFFFFFu ||
            !restoredActor.status.hasSleepTurnsRemaining || restoredActor.status.sleepTurnsRemaining)
            return 9001;
        statusSnapshot.status.hasSleepTurnsRemaining = false;
        statusSnapshot.status.sleepTurnsRemaining = 2;
        if (encodeNativePokemonSave(statusSnapshot, actorBytesAgain, sizeof(actorBytesAgain), actorSizeAgain) !=
                NativeSaveResult::InvalidRecord || actorSizeAgain) return 9002;
        if (decodeNativePokemonSave(actorBytes, actorSize, decodedActor) != NativeSaveResult::Ok ||
            decodedActor.status.present) return 9003;
        auto pausedState = currentActor.battleState;
        pausedState.pauseEvolutions = true;
        NativePokemonSave pausedActor{};
        if (!captureNativePokemonActorSave(pausedState, currentActor.actor,
                currentActor.totalExperience, pausedActor) || !pausedActor.pauseEvolutions) return 320;
        char pauseBytes[512]{};
        size_t pauseSize = 0;
        if (encodeNativePokemonSave(pausedActor, pauseBytes, sizeof(pauseBytes), pauseSize) !=
                NativeSaveResult::Ok || decodeNativePokemonSave(pauseBytes, pauseSize, decodedActor) !=
                NativeSaveResult::Ok || !decodedActor.pauseEvolutions ||
            !restoreNativePokemonActorSave(decodedActor, restoredActor, restoredIdentity) ||
            !restoredActor.pauseEvolutions) return 321;
        auto tagState = pausedState;
        tagState.heldItemLostTags.unburden = true;
        NativePokemonSave tagSnapshot{};
        char tagBytes[512]{};
        size_t tagSize = 0;
        if (!captureNativePokemonActorSave(tagState, currentActor.actor, currentActor.totalExperience, tagSnapshot) ||
            !tagSnapshot.unburdenTag || encodeNativePokemonSave(tagSnapshot, tagBytes, sizeof(tagBytes), tagSize) !=
            NativeSaveResult::Ok || decodeNativePokemonSave(tagBytes, tagSize, decodedActor) != NativeSaveResult::Ok ||
            !restoreNativePokemonActorSave(decodedActor, restoredActor, restoredIdentity) ||
            !restoredActor.heldItemLostTags.unburden) return 405;
        tagState.pendingStatus = PokemonStatusEffect::Sleep;
        if (captureNativePokemonActorSave(tagState, currentActor.actor, currentActor.totalExperience, tagSnapshot))
            return 9046;
        tagState.pendingStatus = PokemonStatusEffect::None;
        tagState.confusion = {3, true, 109, true, 0xffffffffu, true};
        if (!captureNativePokemonActorSave(tagState, currentActor.actor, currentActor.totalExperience, tagSnapshot))
            return 9161;
        char confusionBytes[512]{};
        size_t confusionSize = 0;
        NativePokemonSave confusionDecoded{};
        PokemonBattleState confusionRestored{};
        PokemonActorIdentity confusionIdentity{};
        if (encodeNativePokemonSave(tagSnapshot, confusionBytes, sizeof(confusionBytes), confusionSize) !=
                NativeSaveResult::Ok || confusionBytes[8] != 'a' ||
            decodeNativePokemonSave(confusionBytes, confusionSize, confusionDecoded) != NativeSaveResult::Ok ||
            !restoreNativePokemonActorSave(confusionDecoded, confusionRestored, confusionIdentity) ||
            !confusionRestored.confusion.present || confusionRestored.confusion.turns != 3 ||
            !confusionRestored.confusion.sourceMoveResolved || confusionRestored.confusion.sourceMoveId != 109 ||
            !confusionRestored.confusion.sourcePokemonResolved ||
            confusionRestored.confusion.sourcePokemonId != 0xffffffffu) return 9170;
        // Actor v9 retains move provenance but lacks source actor identity.
        confusionBytes[8] = '9';
        if (decodeNativePokemonSave(confusionBytes, confusionSize - 11, confusionDecoded) != NativeSaveResult::Ok ||
            confusionDecoded.confusion.sourceMoveId != 109 || !confusionDecoded.confusion.sourceMoveResolved ||
            confusionDecoded.confusion.sourcePokemonResolved || confusionDecoded.confusion.sourcePokemonId) return 9520;
        // Actor v8 has duration but no source fields: preserve unknown origin.
        confusionBytes[8] = '8';
        if (decodeNativePokemonSave(confusionBytes, confusionSize - 18, confusionDecoded) != NativeSaveResult::Ok ||
            confusionDecoded.confusion.turns != 3 || confusionDecoded.confusion.sourceMoveResolved ||
            confusionDecoded.confusion.sourceMoveId) return 9510;
        confusionBytes[8] = 'a';
        if (decodeNativePokemonSave(confusionBytes, confusionSize - 1, confusionDecoded) == NativeSaveResult::Ok)
            return 9171;
        tagSnapshot.status.present = true;
        tagSnapshot.status.effect = PokemonStatusEffect::Toxic;
        tagSnapshot.status.toxicTurnCount = 17;
        if (encodeNativePokemonSave(tagSnapshot, confusionBytes, sizeof(confusionBytes), confusionSize) !=
                NativeSaveResult::Ok ||
            decodeNativePokemonSave(confusionBytes, confusionSize, confusionDecoded) != NativeSaveResult::Ok ||
            confusionDecoded.status.effect != PokemonStatusEffect::Toxic ||
            confusionDecoded.status.toxicTurnCount != 17 || confusionDecoded.confusion.turns != 3) return 9172;
        tagSnapshot.confusion.turns = 0;
        if (encodeNativePokemonSave(tagSnapshot, confusionBytes, sizeof(confusionBytes), confusionSize) !=
                NativeSaveResult::InvalidRecord) return 9173;
        tagState.confusion = {};

        tagState.friendship = 173;
        if (!captureNativePokemonActorSave(tagState, currentActor.actor, currentActor.totalExperience, tagSnapshot) ||
            encodeNativePokemonSave(tagSnapshot, tagBytes, sizeof(tagBytes), tagSize) != NativeSaveResult::Ok ||
            decodeNativePokemonSave(tagBytes, tagSize, decodedActor) != NativeSaveResult::Ok ||
            !decodedActor.friendshipResolved || decodedActor.friendship != 173 ||
            !restoreNativePokemonActorSave(decodedActor, restoredActor, restoredIdentity) ||
            restoredActor.friendship != 173) return 466;
        tagBytes[8] = '5';
        if (decodeNativePokemonSave(tagBytes, tagSize - 12, decodedActor) != NativeSaveResult::Ok ||
            decodedActor.maxPpResolved || !decodedActor.friendshipResolved || decodedActor.friendship != 173)
            return 473;
        tagBytes[8] = '4';
        if (decodeNativePokemonSave(tagBytes, tagSize - 15, decodedActor) != NativeSaveResult::Ok ||
            decodedActor.friendshipResolved || !decodedActor.unburdenTag ||
            !restoreNativePokemonActorSave(decodedActor, restoredActor, restoredIdentity) ||
            restoredActor.friendship != currentActor.battleState.friendship) return 467;
        tagBytes[8] = '3';
        if (decodeNativePokemonSave(tagBytes, tagSize - 18, decodedActor) != NativeSaveResult::Ok ||
            decodedActor.unburdenTag) return 406;
        tagBytes[8] = '6';
        tagBytes[tagSize - 17] = '2';
        if (decodeNativePokemonSave(tagBytes, tagSize, decodedActor) != NativeSaveResult::InvalidFormat ||
            decodedActor.unburdenTag) return 407;
        // Member schema 1 has no pause field: preserve its exact former layout.
        const size_t concreteTypeBytes = std::strlen(pausedActor.initialTeraType) + 1;
        if (pauseSize < concreteTypeBytes + 6) return 328;
        const size_t version2Size = pauseSize - concreteTypeBytes - 18;
        pauseBytes[8] = '2';
        if (decodeNativePokemonSave(pauseBytes, version2Size, decodedActor) != NativeSaveResult::Ok ||
            !decodedActor.pauseEvolutions) return 329;
        pauseBytes[8] = '1';
        if (decodeNativePokemonSave(pauseBytes, version2Size - 3, decodedActor) !=
                NativeSaveResult::Ok || decodedActor.pauseEvolutions) return 322;
        pauseBytes[8] = '2';
        pauseBytes[version2Size - 2] = '2';
        if (decodeNativePokemonSave(pauseBytes, version2Size, decodedActor) !=
                NativeSaveResult::InvalidFormat || decodedActor.pauseEvolutions) return 323;
        NativeRunSave explicitParty = loaded;
        explicitParty.playerPartyCount = 1;
        explicitParty.activePlayerMember = 0;
        explicitParty.playerParty[0] = actorSnapshot;
        char partyPayload[kNativeSaveMaxBytes]{};
        size_t partyPayloadSize = 0;
        NativeRunSave decodedParty{};
        if (encodeNativeRunSave(explicitParty, partyPayload, sizeof(partyPayload), partyPayloadSize) !=
                NativeSaveResult::Ok || decodeNativeRunSave(partyPayload, partyPayloadSize,
                PokerogueContent::kContentHash, decodedParty) != NativeSaveResult::Ok ||
            decodedParty.playerPartyCount != 1 || decodedParty.activePlayerMember ||
            decodedParty.playerParty[0].pokemonId != actorSnapshot.pokemonId) return 287;
        auto taggedRun = decodedParty;
        taggedRun.playerParty[0].unburdenTag = true;
        FirstRunRuntime taggedRuntime(7);
        NativeRunSave taggedRecaptured{};
        if (!taggedRuntime.restoreNativeRunSave(taggedRun)) return 408;
        taggedRuntime.captureNativeRunSave(taggedRecaptured);
        if (taggedRecaptured.playerPartyCount != 1 || !taggedRecaptured.playerParty[0].unburdenTag)
            return 409;
        FirstRunRuntime singleSnapshotRuntime(7);
        if (!singleSnapshotRuntime.restoreNativeRunSave(decodedParty)) return 317;
        NativeRunSave singleRecaptured{};
        singleSnapshotRuntime.captureNativeRunSave(singleRecaptured);
        if (singleRecaptured.playerPartyCount != 1 || singleRecaptured.activePlayerMember ||
            singleRecaptured.playerParty[0].pokemonId != actorSnapshot.pokemonId ||
            singleRecaptured.playerParty[0].experience != actorSnapshot.experience ||
            singleRecaptured.playerParty[0].speciesDex != actorSnapshot.speciesDex) return 318;
        for (uint8_t slot = 0; slot < actorSnapshot.moveCount; ++slot)
            if (singleRecaptured.playerParty[0].moveIds[slot] != actorSnapshot.moveIds[slot] ||
                singleRecaptured.playerParty[0].pp[slot] != actorSnapshot.pp[slot]) return 319;
        bool actorHasEvolutions = false;
        for (const auto& edge : PokerogueContent::kSpeciesEvolutions)
            actorHasEvolutions |= pokemonEvolutionTextEqual(edge.sourceSpeciesId,
                singleSnapshotRuntime.presentation().player.speciesId);
        if (singleSnapshotRuntime.togglePlayerEvolutionPause(6) ||
            singleSnapshotRuntime.togglePlayerEvolutionPause(0) != actorHasEvolutions) return 324;
        NativeRunSave toggledPauseSave{};
        singleSnapshotRuntime.captureNativeRunSave(toggledPauseSave);
        if (toggledPauseSave.playerPartyCount != 1 ||
            toggledPauseSave.playerParty[0].pauseEvolutions != actorHasEvolutions ||
            toggledPauseSave.battleTurn != singleRecaptured.battleTurn ||
            toggledPauseSave.wave != singleRecaptured.wave ||
            toggledPauseSave.playerHp != singleRecaptured.playerHp ||
            toggledPauseSave.playerExperience != singleRecaptured.playerExperience) return 325;
        if (!singleSnapshotRuntime.restoreNativeRunSave(toggledPauseSave) ||
            singleSnapshotRuntime.presentation().player.battleState.pauseEvolutions != actorHasEvolutions)
            return 326;
        if (actorHasEvolutions && (!singleSnapshotRuntime.togglePlayerEvolutionPause(0) ||
            singleSnapshotRuntime.presentation().player.battleState.pauseEvolutions)) return 327;
        NativeHeldModifierInstance runtimeHeld{};
        if (initializeHeldModifierInstance("MINI_BLACK_HOLE",
                singleSnapshotRuntime.presentation().player.battleState.pokemonId, 1, false, "[]", runtimeHeld) !=
                HeldModifierStorageResult::Ok || !singleSnapshotRuntime.restoreHeldModifierInventory(&runtimeHeld, 1) ||
            singleSnapshotRuntime.heldModifierCount() != 1 || !singleSnapshotRuntime.heldModifier(0) ||
            singleSnapshotRuntime.heldModifier(1)) return 374;
        auto invalidRuntimeHeld = runtimeHeld;
        invalidRuntimeHeld.canonicalItemIndex = PokerogueContent::kItemCount;
        if (singleSnapshotRuntime.restoreHeldModifierInventory(&invalidRuntimeHeld, 1) ||
            singleSnapshotRuntime.heldModifierCount() != 1 ||
            singleSnapshotRuntime.restoreHeldModifierInventory(&runtimeHeld,
                FirstRunRuntime::kHeldModifierStorageCapacity + 1)) return 375;
        const auto beforeUnsupportedHeldTurn = singleSnapshotRuntime.presentation().player.battleState.hp;
        if (singleSnapshotRuntime.advanceBattleTurn() ||
            singleSnapshotRuntime.presentation().player.battleState.hp != beforeUnsupportedHeldTurn) return 376;
        NativeRunSave unsupportedHeldSave{};
        singleSnapshotRuntime.captureNativeRunSave(unsupportedHeldSave);
        if (validateNativeRunSave(unsupportedHeldSave, PokerogueContent::kContentHash) != NativeSaveResult::Ok ||
            unsupportedHeldSave.heldModifierCount != 1 || !unsupportedHeldSave.playerPartyCount) return 377;
        char heldRunPayload[kNativeSaveMaxBytes]{};
        size_t heldRunSize = 0;
        NativeRunSave decodedHeldRun{};
        if (encodeNativeRunSave(unsupportedHeldSave, heldRunPayload, sizeof(heldRunPayload), heldRunSize) !=
                NativeSaveResult::Ok || decodeNativeRunSave(heldRunPayload, heldRunSize,
                PokerogueContent::kContentHash, decodedHeldRun) != NativeSaveResult::Ok ||
            decodedHeldRun.heldModifierCount != 1 || decodedHeldRun.heldModifiers[0].ownerPokemonId !=
                runtimeHeld.ownerPokemonId || decodedHeldRun.heldModifiers[0].transferable) return 384;
        FirstRunRuntime restoredHeldGame(7);
        if (!restoredHeldGame.restoreNativeRunSave(decodedHeldRun) || restoredHeldGame.heldModifierCount() != 1 ||
            std::strcmp(restoredHeldGame.heldModifier(0)->rawArguments, runtimeHeld.rawArguments)) return 385;
        // Historical v10 ends after explicit player members, before held inventory.
        char versionTenPayload[kNativeSaveMaxBytes]{};
        const char* heldSection = std::strstr(heldRunPayload, "heldModifierCount=");
        if (!heldSection) return 386;
        size_t versionTenSize = static_cast<size_t>(heldSection - heldRunPayload);
        std::memcpy(versionTenPayload, heldRunPayload, versionTenSize);
        char* tenSaveVersion = std::strstr(versionTenPayload, "saveVersion=");
        char* tenRuntimeVersion = std::strstr(versionTenPayload, "runtimeVersion=");
        if (!tenSaveVersion || !tenRuntimeVersion) return 387;
        std::memcpy(tenSaveVersion + std::strlen("saveVersion="), "000a", 4);
        std::memcpy(tenRuntimeVersion + std::strlen("runtimeVersion="), "000a", 4);
        char tenHash[65]{};
        IntegritySha256::hashHex(versionTenPayload, versionTenSize, tenHash);
        std::memcpy(versionTenPayload + versionTenSize, "sha256=", 7);
        std::memcpy(versionTenPayload + versionTenSize + 7, tenHash, 64);
        versionTenPayload[versionTenSize + 71] = '\n';
        versionTenSize += 72;
        NativeRunSave migratedTen{};
        if (decodeNativeRunSave(versionTenPayload, versionTenSize, PokerogueContent::kContentHash, migratedTen) !=
                NativeSaveResult::Ok || migratedTen.saveVersion != kNativeSaveVersion || migratedTen.runtimeVersion != kNativeSaveRuntimeVersion ||
            migratedTen.heldModifierCount || migratedTen.playerPartyCount != unsupportedHeldSave.playerPartyCount ||
            migratedTen.playerParty[0].pokemonId != unsupportedHeldSave.playerParty[0].pokemonId) return 388;
        if (!singleSnapshotRuntime.restoreSetup(singleRecaptured.seed, singleRecaptured.starterDex) ||
            singleSnapshotRuntime.heldModifierCount()) return 378;
        explicitParty.activePlayerMember = 1;
        if (validateNativeRunSave(explicitParty, PokerogueContent::kContentHash) !=
                NativeSaveResult::InvalidRecord) return 288;
        explicitParty.activePlayerMember = 0;
        ++explicitParty.playerParty[0].hp;
        if (validateNativeRunSave(explicitParty, PokerogueContent::kContentHash) !=
                NativeSaveResult::InvalidRecord) return 289;
        NativeRunSave inventorySnapshot = loaded;
        inventorySnapshot.pokeballCounts[0] = 0;
        inventorySnapshot.pokeballCounts[1] = 3;
        inventorySnapshot.pokeballCounts[2] = 99;
        inventorySnapshot.pokeballCounts[3] = 1;
        inventorySnapshot.pokeballCounts[4] = 2;
        if (!game.restoreNativeRunSave(inventorySnapshot)) return 264;
        NativeRunSave inventoryRoundtrip{};
        game.captureNativeRunSave(inventoryRoundtrip);
        if (validateNativeRunSave(inventoryRoundtrip, PokerogueContent::kContentHash) != NativeSaveResult::Ok)
            return 265;
        for (uint8_t ball = 0; ball < 5; ++ball)
            if (game.pokeballCount(static_cast<PokeballType>(ball)) != inventorySnapshot.pokeballCounts[ball] ||
                inventoryRoundtrip.pokeballCounts[ball] != inventorySnapshot.pokeballCounts[ball]) return 266;
        NativeRunSave invalidInventory = inventorySnapshot;
        invalidInventory.pokeballCounts[2] = 100;
        if (game.restoreNativeRunSave(invalidInventory) || game.pokeballCount(PokeballType::UltraBall) != 99)
            return 267;
        if (!game.restoreNativeRunSave(loaded)) return 268;
        if (loaded.weatherType || loaded.weatherTurnsLeft || loaded.weatherMaxDuration) return 23;
        NativeRunSave unsupportedWeather = loaded;
        unsupportedWeather.weatherType = 2;
        unsupportedWeather.weatherTurnsLeft = 3;
        unsupportedWeather.weatherMaxDuration = 5;
        if (validateNativeRunSave(unsupportedWeather, PokerogueContent::kContentHash) != NativeSaveResult::Ok) return 24;
        const bool weatherEligible = pokemonWeatherLifecycleSupported(game.presentation().player.battleState.abilityId) &&
            pokemonWeatherLifecycleSupported(game.presentation().enemy.battleState.abilityId);
        if (game.restoreNativeRunSave(unsupportedWeather) != weatherEligible) return 35;
        NativeRunSave afterWeatherRestore{};
        game.captureNativeRunSave(afterWeatherRestore);
        if (afterWeatherRestore.weatherType != (weatherEligible ? 2 : 0) ||
            afterWeatherRestore.weatherTurnsLeft != (weatherEligible ? 3 : 0) ||
            afterWeatherRestore.weatherMaxDuration != (weatherEligible ? 5 : 0) ||
            afterWeatherRestore.enemyHp != loaded.enemyHp || afterWeatherRestore.playerHp != loaded.playerHp ||
            afterWeatherRestore.battleTurn != loaded.battleTurn) return 25;
        if (!game.restoreNativeRunSave(loaded)) return 36;
        NativeRunSave roomCheckpoint = loaded;
        roomCheckpoint.trickRoomTurnsLeft = 3;
        roomCheckpoint.trickRoomMaxDuration = 5;
        roomCheckpoint.trickRoomSourceMoveId = 433;
        roomCheckpoint.trickRoomSourcePokemonId = context.player.battleState.pokemonId;
        if (!game.restoreNativeRunSave(roomCheckpoint)) return 26;
        NativeRunSave roomRestored{};
        game.captureNativeRunSave(roomRestored);
        if (roomRestored.trickRoomTurnsLeft != 3 || roomRestored.trickRoomMaxDuration != 5 ||
            roomRestored.trickRoomSourceMoveId != 433 ||
            roomRestored.trickRoomSourcePokemonId != roomCheckpoint.trickRoomSourcePokemonId) return 27;

        return 0;
    }
    return 7; // No supported canonical encounter found: do not silently skip.
}

static int checkSetupCatalogNavigation() {
    Pokerogue3DS::FirstRunRuntime game(1);
    for(const auto& species:PokerogueContent::kSpecies) {
        const bool selected=game.selectSetupStarter(species.dex);
        if(selected!=species.starterEligible) return 1;
        if(selected && game.selectedSetupStarterDex()!=species.dex) return 2;
    }
    if(game.selectSetupStarter(0) || game.selectSetupStarter(65535)) return 3;
    return 0;
}

static int checkBattleFleeMechanicsAndRestrictions() {
    using namespace Pokerogue3DS;
    FirstRunRuntime game(1);
    if (game.fleeBattle()) return 10301;
    if (!game.restoreSetup(1, 1)) return 10302;
    if (!game.startRun()) return 10303;
    if (!game.runStarted() || game.battleFinished()) return 10304;
    // AttemptRunPhase consumes one battle roll; success starts the next battle
    // through BattleEndPhase(false), with no victory, EXP or reward grant.
    auto expectedRng = *game.battleRng().currentStream();
    const unsigned roll = expectedRng.randSeedInt(100);
    const auto& field = game.presentation();
    const double ratio = double(field.player.battleState.stats[5]) / field.enemy.battleState.stats[5];
    const unsigned chance = unsigned(std::min(95.0, std::max(5.0, std::floor(22.5 * ratio + 5.0 + 0.5))));
    const auto experience = field.player.totalExperience;
    if (!game.fleeBattle()) return 10305;
    if (game.playerWon() || game.experienceGranted() || game.rewardsPending() ||
        game.presentation().player.totalExperience != experience) return 10306;
    NativeRunSave after{};
    if (game.captureNativeRunSave(after) != NativeSaveResult::Ok) return 10307;
    if (after.wave != (roll < chance ? 2 : 1)) return 10308;
    if (game.battleFeedback() != (roll < chance ? "¡Escapaste sin problemas!" : "¡No pudiste escapar!")) return 10309;
    return 0;
}

// Multiple runtimes and replay candidates must never share encounter history.
// Upstream Arena.randomSpecies has no anti-repeat rule across ordinary waves.
static int checkIndependentEncounterReplay() {
    using namespace Pokerogue3DS;
    uint16_t firstDex = 0;
    bool sawDistinctSpecies = false, sawDouble = false;
    for (uint32_t seed = 1; seed <= 32; ++seed) {
        FirstRunRuntime original(seed);
        if (!original.startRun()) return 11101;
        NativeRunSave snapshot{};
        if (original.captureNativeRunSave(snapshot) != NativeSaveResult::Ok) return 11102;
        FirstRunRuntime unrelated(seed + 100), restored(999);
        if (!unrelated.startRun()) return 11103;
        if (!restored.restoreNativeRunSave(snapshot)) return 11104;
        const auto& left = original.presentation();
        const auto& right = restored.presentation();
        if (!firstDex) firstDex = left.enemy.dex;
        sawDistinctSpecies |= left.enemy.dex != firstDex;
        if (left.enemy.battleState.abilityId != right.enemy.battleState.abilityId) return 11110;
        if (original.doubleBattle()) {
            sawDouble = true;
            if (!restored.doubleBattle() || left.secondEnemy.dex != right.secondEnemy.dex ||
                left.secondEnemy.actor.pokemonId != right.secondEnemy.actor.pokemonId ||
                left.secondEnemy.battleState.abilityId != right.secondEnemy.battleState.abilityId ||
                left.secondEnemy.moveCount != right.secondEnemy.moveCount) return 11111;
            for (uint8_t slot = 0; slot < left.secondEnemy.moveCount; ++slot)
                if (left.secondEnemy.moveIds[slot] != right.secondEnemy.moveIds[slot]) return 11112;
        }
        if (left.enemy.dex != right.enemy.dex || left.enemy.actor.pokemonId != right.enemy.actor.pokemonId ||
            left.enemy.moveCount != right.enemy.moveCount) return 11105;
        for (uint8_t slot = 0; slot < left.enemy.moveCount; ++slot)
            if (left.enemy.moveIds[slot] != right.enemy.moveIds[slot]) return 11106;
        const auto l = original.battleRng().state(), r = restored.battleRng().state();
        if (l.carry != r.carry || l.s0 != r.s0 || l.s1 != r.s1 || l.s2 != r.s2) return 11107;
    }
    return sawDistinctSpecies && sawDouble ? 0 : 11113;
}

static int checkPresentationExperienceLevelCap() {
    using namespace Pokerogue3DS;
    FirstRunRuntime runtime(1);
    if(runtime.experienceLevelCap()!=10) return 601;
    for(unsigned wave=1;wave<=200;++wave) {
        const double difficulty=std::ceil(double(wave)/10)*10;
        const double base=(1+difficulty/2+std::pow(difficulty/25,2))*1.2;
        if(classicExperienceLevelCap(wave)!=std::ceil(base/2)*2+2) return 602;
    }
    if(classicExperienceLevelCap(0) || classicExperienceLevelCap(201)) return 603;
    return 0;
}

extern "C" int runFirstRunRestoreChecks() {
    struct Check { const char* name; int (*run)(); };
    const Check checks[] = {
        {"checkIndependentEncounterReplay", checkIndependentEncounterReplay},
        {"checkSetupCatalogNavigation", checkSetupCatalogNavigation},
        {"checkBattleFleeMechanicsAndRestrictions", checkBattleFleeMechanicsAndRestrictions},
        {"checkLegacyFirstRunRestore", checkLegacyFirstRunRestore},
        {"checkDoublePoisonHealTurn", checkDoublePoisonHealTurn},
        {"checkDynamicDoubleSpeedChangeTurn", checkDynamicDoubleSpeedChangeTurn},
        {"checkDoubleMirrorArmorSourceProtection", checkDoubleMirrorArmorSourceProtection},
        {"checkDoubleSingleTargetDropReactions", checkDoubleSingleTargetDropReactions},
        {"checkDoubleLocalStageAbilities", checkDoubleLocalStageAbilities},
        {"checkEnemyAreaStatAction", checkEnemyAreaStatAction},
        {"checkEnemyAreaAllyDamage", checkEnemyAreaAllyDamage},
        {"checkDoubleAreaHitBatchRng", checkDoubleAreaHitBatchRng},
        {"checkDoublePlainAreaDamage", checkDoublePlainAreaDamage},
        {"checkDoubleAreaActionChecksAndLastPp", checkDoubleAreaActionChecksAndLastPp},
        {"checkDoubleSingleTargetSleepCheckpoint", checkDoubleSingleTargetSleepCheckpoint},
        {"checkDoubleStatusResidualCheckpoint", checkDoubleStatusResidualCheckpoint},
        {"checkDoublePartialExperienceCheckpoint", checkDoublePartialExperienceCheckpoint},
        {"checkDoubleCheckpointRoundtrip", checkDoubleCheckpointRoundtrip},
        {"checkDoubleExhaustedPpStruggle", checkDoubleExhaustedPpStruggle},
        {"checkExhaustedPpStruggleReplay", checkExhaustedPpStruggleReplay},
        {"checkStatusActionAdmission", checkStatusActionAdmission},
        {"checkBossDamageAbilityCapabilities", checkBossDamageAbilityCapabilities},
        {"checkTrainerParentEvolutionThresholds", checkTrainerParentEvolutionThresholds},
        {"checkCanonicalTrainerSpecialtyTypes", checkCanonicalTrainerSpecialtyTypes},
        {"checkTrainerBalancedTypes", checkTrainerBalancedTypes},
        {"checkReservedTrainerSpecies", checkReservedTrainerSpecies},
        {"checkTrainerPoolEvolutionDraws", checkTrainerPoolEvolutionDraws},
        {"checkCanonicalSameSpeciesTrainerMembers", checkCanonicalSameSpeciesTrainerMembers},
        {"checkCanonicalTrainerSignatureSlots", checkCanonicalTrainerSignatureSlots},
        {"checkResolvedActionFieldLifecycle", checkResolvedActionFieldLifecycle},
        {"checkTrainerExperienceReplay", checkTrainerExperienceReplay},
        {"checkTrainerInteractiveBattle", checkTrainerInteractiveBattle},
        {"checkModifierRewardGenerationAndClaim", checkModifierRewardGenerationAndClaim},
        {"checkFirstRivalEncounterTraceability", checkFirstRivalEncounterTraceability},
        {"checkHeldBerryStorage", checkHeldBerryStorage},
        {"checkCanonicalBerryEffects", checkCanonicalBerryEffects},
        {"checkCanonicalBerryGeneration", checkCanonicalBerryGeneration},
        {"checkFlinchTurnLifecycle", checkFlinchTurnLifecycle},
        {"checkPersistentExperienceRewards", checkPersistentExperienceRewards},
        {"checkBiomeTransitionProgression", checkBiomeTransitionProgression},
        {"checkDoubleBattleTargetingAndMechanics", checkDoubleBattleTargetingAndMechanics},
        {"checkWave200FinalBossAndGameClear", checkWave200FinalBossAndGameClear},
        {"checkPokeballCaptureMechanics", checkPokeballCaptureMechanics},
        {"checkPlayerPartyManagementAndSwitching", checkPlayerPartyManagementAndSwitching},
        {"checkLevelUpMoveLearningAndEvolution", checkLevelUpMoveLearningAndEvolution},
        {"checkInitialStarterTeamSetup", checkInitialStarterTeamSetup},
        {"checkInitialTeamFirstTurnRoundtrip", checkInitialTeamFirstTurnRoundtrip},
        {"checkPresentationExperienceLevelCap", checkPresentationExperienceLevelCap},
        {"checkStarterFormPreferencePersistence", checkStarterFormPreferencePersistence},
        {"checkStarterCostPurchasePersistence", checkStarterCostPurchasePersistence},
        {"checkExtendedWaveAndBiomeSaveValidation", checkExtendedWaveAndBiomeSaveValidation},
    };
    unsigned failed = 0, total = 0;
    int firstFailure = 0;
    for (const auto& check : checks) {
        std::printf("FirstRunRuntime running %s\n", check.name);
        std::fflush(stdout);
        const int result = check.run();
        ++total;
        std::printf("FirstRunRuntime case %s: %s (%d)\n", check.name, result ? "FAIL" : "PASS", result);
        std::fflush(stdout);
        if (result) { ++failed; if (!firstFailure) firstFailure = result; }
    }
    std::printf("FirstRunRuntime cases: %u passed, %u failed, %u total\n", total - failed, failed, total);
    return firstFailure;
}

int main() {
    const int result = runFirstRunRestoreChecks();
    std::printf("FirstRunRuntime checks: %d\n", result);
    return result == 0 ? 0 : 1;
}
