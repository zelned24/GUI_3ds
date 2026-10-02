#include "game/PokemonStarterMoveset.hpp"
#include "game/FirstRunRuntime.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cstring>
#include "storage/IntegritySha256.hpp"
#include "storage/NativeStarterCandyProfile.hpp"
#include "storage/NativeProgressStore.hpp"
#include "game/PokemonExperience.hpp"
#include "game/PokemonWeatherPhase.hpp"
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
    saveVersion[15] = 'D';
    runtimeVersion[18] = 'D';
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
                input.formId = species->firstFormId;
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
                sturdyCheckpoint.enemyMoveCount = 1;
                sturdyCheckpoint.enemyMoveIds[0] = 82;
                sturdyCheckpoint.enemyPp[0] = 10;
                for (uint8_t slot = 1; slot < 4; ++slot) {
                    sturdyCheckpoint.enemyMoveIds[slot] = 0;
                    sturdyCheckpoint.enemyPp[slot] = 0;
                }
                FirstRunRuntime sturdyRun(seed), replay(seed);
                NativeRunSave after{}, repeatedAfter{};
                if (!sturdyRun.restoreNativeRunSave(sturdyCheckpoint) ||
                    !replay.restoreNativeRunSave(sturdyCheckpoint) || !sturdyRun.battleInputSupported() ||
                    !replay.battleInputSupported() || !sturdyRun.advanceBattleTurn() || !replay.advanceBattleTurn() ||
                    sturdyRun.captureNativeRunSave(after) != NativeSaveResult::Ok ||
                    replay.captureNativeRunSave(repeatedAfter) != NativeSaveResult::Ok ||
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
                input.formId = species->firstFormId;
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
                sharpnessCheckpoint.enemyMoveCount = 1;
                sharpnessCheckpoint.enemyMoveIds[0] = 45;
                sharpnessCheckpoint.enemyPp[0] = 40;
                for (uint8_t slot = 1; slot < 4; ++slot) {
                    sharpnessCheckpoint.enemyMoveIds[slot] = 0;
                    sharpnessCheckpoint.enemyPp[slot] = 0;
                }
                FirstRunRuntime sharpnessRun(seed), replay(seed);
                NativeRunSave after{}, repeatedAfter{};
                if (!sharpnessRun.restoreNativeRunSave(sharpnessCheckpoint) ||
                    !replay.restoreNativeRunSave(sharpnessCheckpoint) || !sharpnessRun.battleInputSupported() ||
                    !replay.battleInputSupported() || !sharpnessRun.advanceBattleTurn() || !replay.advanceBattleTurn() ||
                    sharpnessRun.captureNativeRunSave(after) != NativeSaveResult::Ok ||
                    replay.captureNativeRunSave(repeatedAfter) != NativeSaveResult::Ok ||
                    after.playerPp[0] != 14 || after.battleTurn != sharpnessCheckpoint.battleTurn + 1 ||
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
                    stageAfter.playerStatStages[0] != repeatedStageAfter.playerStatStages[0]) return 9603;
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
        game.captureNativeRunSave(won);
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
        const auto* choice0 = game.rewardChoice(0);
        if (!choice0 || !choice0->poolEntry || !choice0->poolEntry->itemId) return 68;
        if (!game.claimRewardChoice()) return 69;
        if (game.run().wave != 2 || game.rewardsPending()) return 70;
        return 0;
    }
    return 71;
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
            game.captureNativeRunSave(skipSave);
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
        auto partialBoss = wave10Won;
        partialBoss.enemyBoss.segmentIndex = 0;
        FirstRunRuntime bossRestore(seed);
        NativeRunSave partialRecaptured{};
        if (!bossRestore.restoreNativeRunSave(partialBoss) ||
            bossRestore.captureNativeRunSave(partialRecaptured) != NativeSaveResult::Ok ||
            partialRecaptured.enemyBoss.segmentCount != partialBoss.enemyBoss.segmentCount ||
            partialRecaptured.enemyBoss.segmentIndex || partialRecaptured.enemyHp != partialBoss.enemyHp) return 10141;
        auto invalidBoss = partialBoss;
        ++invalidBoss.enemyBoss.segmentCount;
        if (bossRestore.restoreNativeRunSave(invalidBoss)) return 10142;
        wave10Won.enemyBoss.segmentIndex = 0;
        wave10Won.stage = NativeSaveStage::BattleWon;
        setSingleParticipantFixture(wave10Won, game);
        wave10Won.enemyHp = 0;
        if (!game.restoreNativeRunSave(wave10Won) || !game.advanceBattleTurn()) return 81;
        if (!game.skipVictoryReward()) return 82;
        if (game.run().wave != 11) return 83;
        if (std::strcmp(game.run().biomeId, "town") == 0) return 84;
        if (!game.presentation().biomeName || !*game.presentation().biomeName) return 85;
        NativeRunSave laterCheckpoint{};
        const auto captured = game.captureNativeRunSave(laterCheckpoint);
        // This gate covers wild singles, not every random wave-11 encounter.
        if (captured == NativeSaveResult::UnsupportedStage) continue;
        if (captured != NativeSaveResult::Ok || laterCheckpoint.wave != 11 ||
            !laterCheckpoint.playerPartyCount || std::strcmp(laterCheckpoint.biomeId, game.run().biomeId)) return 10120;
        FirstRunRuntime laterRestore(seed), laterReplay(seed);
        NativeRunSave recaptured{}, replayCaptured{};
        if (!laterRestore.restoreNativeRunSave(laterCheckpoint) || !laterReplay.restoreNativeRunSave(laterCheckpoint) ||
            laterRestore.captureNativeRunSave(recaptured) != NativeSaveResult::Ok ||
            laterReplay.captureNativeRunSave(replayCaptured) != NativeSaveResult::Ok ||
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
    bossInput.formId = finalSpecies->firstFormId;
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
    wildGame.captureNativeRunSave(wildSave);
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
    game.captureNativeRunSave(wildSave);
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
    const auto* reserveSpecies = PokerogueContent::findSpeciesByDex(invalidReserveSave.playerParty[0].speciesDex);
    if (!reserveSpecies || pokemonTotalExperienceForLevel(reserveSpecies->growthRate,
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
        healingSwitchGame.presentation().player.battleState.turnDamageDealt) return 440;
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
        if (!simultaneousGame.restoreNativeRunSave(simultaneous) ||
            !simultaneousGame.battleInputSupported()) return 560;
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
    if (!evolutionIdentity.formId || std::strcmp(evolutionIdentity.formId, bulbaState.formId) ||
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
    onixInput.formId = onix->firstFormId;
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
        !onixIdentity.initialTeraType || std::strcmp(onixIdentity.initialTeraType, onix->type1) ||
        !std::strcmp(onixIdentity.initialTeraType, steelix->type1)) return 332;
    uint32_t steelixExp = 0;
    NativePokemonSave steelixSave{};
    PokemonBattleState restoredSteelix{};
    PokemonActorIdentity restoredSteelixIdentity{};
    if (pokemonTotalExperienceForLevel(steelix->growthRate, onixState.level, steelixExp) !=
            PokemonExperienceResult::Ok || !captureNativePokemonActorSave(onixState, onixIdentity,
                steelixExp, steelixSave) || !restoreNativePokemonActorSave(steelixSave, restoredSteelix,
                restoredSteelixIdentity) || std::strcmp(restoredSteelixIdentity.initialTeraType, onix->type1))
        return 333;
    std::strcpy(steelixSave.initialTeraType, "INVALID_TYPE");
    if (restoreNativePokemonActorSave(steelixSave, restoredSteelix, restoredSteelixIdentity) ||
        std::strcmp(restoredSteelixIdentity.initialTeraType, onix->type1)) return 334;
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
            if (profile.bossDamageCallbacksResolved) return 262;
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
            if ((species.type1 && std::strcmp(species.type1, trainer.specialtyType) == 0) ||
                (species.type2 && std::strcmp(species.type2, trainer.specialtyType) == 0)) canonicalType = true;
        if (!canonicalType) return 250;
    }
    return inspected == 2 ? 0 : 251;
}

static int checkTrainerBalancedTypes() {
    using namespace Pokerogue3DS;
    const auto* species = PokerogueContent::findSpeciesByDex(1);
    if (!species || !species->type1) return 242;
    const auto* form = PokerogueContent::findFormById(species->firstFormId);
    if (!form) return 243;
    TrainerPartyMemberTypes prior[] = {{form->type1, form->type2, true}};
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
        const bool expected = std::strcmp(other.type1, form->type1) == 0 ||
            (form->type2 && std::strcmp(form->type2, "NONE") && std::strcmp(other.type1, form->type2) == 0) ||
            (other.type2 && std::strcmp(other.type2, "NONE") &&
                (std::strcmp(other.type2, form->type1) == 0 ||
                 (form->type2 && std::strcmp(form->type2, "NONE") && std::strcmp(other.type2, form->type2) == 0)));
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

int main() {
    const int doubleStruggle = checkDoubleExhaustedPpStruggle();
    if (doubleStruggle) return doubleStruggle;
    const int struggleReplay = checkExhaustedPpStruggleReplay();
    if (struggleReplay) return struggleReplay;
    const int statusAdmissionCheck = checkStatusActionAdmission();
    if (statusAdmissionCheck) return statusAdmissionCheck;

    const int bossCapabilitiesCheck = checkBossDamageAbilityCapabilities();
    if (bossCapabilitiesCheck) return bossCapabilitiesCheck;
    const int parentThresholdCheck = checkTrainerParentEvolutionThresholds();
    if (parentThresholdCheck) return parentThresholdCheck;
    const int specialtyCheck = checkCanonicalTrainerSpecialtyTypes();
    if (specialtyCheck) return specialtyCheck;
    const int balancedTypesCheck = checkTrainerBalancedTypes();
    if (balancedTypesCheck) return balancedTypesCheck;
    const int reservedSpeciesCheck = checkReservedTrainerSpecies();
    if (reservedSpeciesCheck) return reservedSpeciesCheck;
    const int poolDrawCheck = checkTrainerPoolEvolutionDraws();
    if (poolDrawCheck) return poolDrawCheck;
    const int sameSpeciesCheck = checkCanonicalSameSpeciesTrainerMembers();
    if (sameSpeciesCheck) return sameSpeciesCheck;
    const int signatureCheck = checkCanonicalTrainerSignatureSlots();
    if (signatureCheck) return signatureCheck;
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
        uint8_t beforeBytes[kNativeSaveMaxBytes]{}, afterBytes[kNativeSaveMaxBytes]{};
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
        if (!captureNativePokemonSave(currentActor.battleState, currentActor.totalExperience,
                actorSnapshot)) return 269;
        PokemonBattleState restoredActor{};
        if (!restoreNativePokemonSave(actorSnapshot, restoredActor) ||
            restoredActor.pokemonId != currentActor.battleState.pokemonId ||
            restoredActor.hp != currentActor.battleState.hp ||
            restoredActor.formId == actorSnapshot.formId ||
            std::strcmp(restoredActor.formId, actorSnapshot.formId)) return 270;
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
                PokemonExperienceResult::Ok || candyPlan.progress.experience != actorSnapshot.experience)
            return 501;
        auto candyActor = currentActor.battleState;
        if (!recalculatePokemonBattleLevel(candyActor, candyPlan.progress.level)) return 502;
        NativePokemonSave candySnapshot{}, decodedCandy{};
        if (!captureNativePokemonSave(candyActor, candyPlan.progress.experience, candySnapshot)) return 503;
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
        char* tenSaveVersion = std::strstr(versionTenPayload, "saveVersion=000d");
        char* tenRuntimeVersion = std::strstr(versionTenPayload, "runtimeVersion=000d");
        if (!tenSaveVersion || !tenRuntimeVersion) return 387;
        tenSaveVersion[std::strlen("saveVersion=") + 3] = 'a';
        tenRuntimeVersion[std::strlen("runtimeVersion=") + 3] = 'a';
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
        const int fieldCheck = checkResolvedActionFieldLifecycle();
        if (fieldCheck) return fieldCheck;
        const int replayCheck = checkTrainerExperienceReplay();
        if (replayCheck) return replayCheck;
        const int trainerCheck = checkTrainerInteractiveBattle();
        if (trainerCheck) return trainerCheck;
        const int rewardCheck = checkModifierRewardGenerationAndClaim();
        if (rewardCheck) return rewardCheck;
        const int biomeCheck = checkBiomeTransitionProgression();
        if (biomeCheck) return biomeCheck;
        const int doubleCheck = checkDoubleBattleTargetingAndMechanics();
        if (doubleCheck) return doubleCheck;
        const int bossCheck = checkWave200FinalBossAndGameClear();
        if (bossCheck) return bossCheck;
        const int captureCheck = checkPokeballCaptureMechanics();
        if (captureCheck) return captureCheck;
        const int partyCheck = checkPlayerPartyManagementAndSwitching();
        if (partyCheck) return partyCheck;
        const int evoCheck = checkLevelUpMoveLearningAndEvolution();
        if (evoCheck) return evoCheck;
        const int initialTeamCheck = checkInitialStarterTeamSetup();
        if (initialTeamCheck) return initialTeamCheck;
        const int firstTeamTurn = checkInitialTeamFirstTurnRoundtrip();
        if (firstTeamTurn) return firstTeamTurn;
        const int formPreferenceCheck = checkStarterFormPreferencePersistence();
        if (formPreferenceCheck) return formPreferenceCheck;
        const int purchaseCheck = checkStarterCostPurchasePersistence();
        if (purchaseCheck) return purchaseCheck;
        return checkExtendedWaveAndBiomeSaveValidation();
    }
    return 7; // No supported canonical encounter found: do not silently skip.
}
