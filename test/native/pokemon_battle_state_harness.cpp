#include "game/PokemonBattleState.hpp"
#include "game/PokemonFreshProfile.hpp"
#include "game/PokemonLevelMovePool.hpp"
#include "game/PokemonWildMovesetGenerator.hpp"
#include "game/PokemonStarterMoveset.hpp"
#include "game/PokemonTrainerAi.hpp"
#include "game/PokemonStatStageEffect.hpp"
#include "game/PokemonHealingEffect.hpp"
#include "game/PokemonRecoilEffect.hpp"
#include "game/PokemonWeatherPhase.hpp"
#include "game/PokerogueTurnOrder.hpp"
#include "game/PokemonExperience.hpp"
#include "game/PokerogueRngAdapter.hpp"

using Pokerogue3DS::PokemonBattleInit;
using Pokerogue3DS::PokemonBattleInitResult;
using Pokerogue3DS::PokemonBattleState;
using Pokerogue3DS::PokemonActorIdentity;
using Pokerogue3DS::PokemonFormSelectionContext;
using Pokerogue3DS::PokemonNature;
using Pokerogue3DS::PokemonAbilitySelectionResult;
using Pokerogue3DS::PokemonGender;
using Pokerogue3DS::PokemonGenderSelectionResult;
using Pokerogue3DS::PokemonMoveDamageResult;
using Pokerogue3DS::PokemonMoveDamageRoll;
using Pokerogue3DS::PokemonMoveActionResult;
using Pokerogue3DS::PokemonMoveActionStatus;
using Pokerogue3DS::PokerogueRngAdapter;

extern "C" int runPokemonBattleStateChecks() {
    // Classic late-game cap exceeds 100; the source stat formula remains unchanged.
    const auto* highLevelSpecies = PokerogueContent::findSpeciesByDex(1);
    if (!highLevelSpecies || Pokerogue3DS::classicExperienceLevelCap(200) != 200) return 460;
    PokemonBattleInit highLevelInput{};
    highLevelInput.speciesDex = highLevelSpecies->dex;
    highLevelInput.abilityId = highLevelSpecies->ability1;
    highLevelInput.gender = PokemonGender::Male;
    highLevelInput.level = 200;
    PokemonBattleState highLevelActor{};
    if (Pokerogue3DS::initializePokemonBattleState(highLevelInput, highLevelActor) != PokemonBattleInitResult::Ok ||
        highLevelActor.level != 200 || highLevelActor.maxHp != 4u * highLevelSpecies->hp + 210u)
        return 461;
    uint32_t exp199 = 0, exp200 = 0;
    Pokerogue3DS::PokemonExperienceProgress highProgress{};
    if (Pokerogue3DS::pokemonTotalExperienceForLevel(highLevelSpecies->growthRate, 199, exp199) !=
            Pokerogue3DS::PokemonExperienceResult::Ok ||
        Pokerogue3DS::pokemonTotalExperienceForLevel(highLevelSpecies->growthRate, 200, exp200) !=
            Pokerogue3DS::PokemonExperienceResult::Ok ||
        Pokerogue3DS::applyPokemonExperience(highLevelSpecies->growthRate, 199, exp199,
            exp200 - exp199, 200, highProgress) != Pokerogue3DS::PokemonExperienceResult::Ok ||
        highProgress.level != 200 || highProgress.totalExperience != exp200) return 462;
    // Rare Candy can place level above the wave cap with EXP below its threshold.
    if (Pokerogue3DS::applyPokemonExperience(highLevelSpecies->growthRate, 200, exp199,
            1, 198, highProgress) != Pokerogue3DS::PokemonExperienceResult::Ok ||
        highProgress.level != 200 || highProgress.totalExperience != exp200) return 525;
    if (Pokerogue3DS::applyPokemonExperience(highLevelSpecies->growthRate, 200, exp200 + 7,
            10, 198, highProgress) != Pokerogue3DS::PokemonExperienceResult::Ok ||
        highProgress.level != 200 || highProgress.totalExperience != exp200 + 7) return 526;
    const auto* rareCandy = Pokerogue3DS::levelIncrementItemProfile("RARE_CANDY");
    const auto* rarerCandy = Pokerogue3DS::levelIncrementItemProfile("RARER_CANDY");
    if (!rareCandy || rareCandy->allParty || !rarerCandy || !rarerCandy->allParty ||
        !rareCandy->sourceHash || Pokerogue3DS::levelIncrementItemProfile("UNKNOWN_ITEM")) return 527;
    Pokerogue3DS::PokemonLevelIncrementPlan candyPlan{};
    if (Pokerogue3DS::planPokemonLevelIncrement(highLevelSpecies->growthRate, 199, exp199,
            0, true, 10000, candyPlan) != Pokerogue3DS::PokemonExperienceResult::Ok ||
        candyPlan.progress.level != 200 || candyPlan.progress.totalExperience != exp200 ||
        candyPlan.previousLevel != 199 || !candyPlan.requiresFriendship ||
        !candyPlan.requiresLevelUpPhase) return 528;
    if (Pokerogue3DS::planPokemonLevelIncrement(highLevelSpecies->growthRate, 199, exp199,
            2, true, 200, candyPlan) != Pokerogue3DS::PokemonExperienceResult::Ok ||
        candyPlan.progress.level != 202 || candyPlan.progress.totalExperience != exp199) return 529;
    if (Pokerogue3DS::planPokemonLevelIncrement(highLevelSpecies->growthRate, 199, exp199,
            0, false, 10000, candyPlan) != Pokerogue3DS::PokemonExperienceResult::UnresolvedPolicy ||
        candyPlan.progress.level != 202) return 530;
    if (Pokerogue3DS::planPokemonLevelIncrement(highLevelSpecies->growthRate, 199, exp199,
            100, true, 10000, candyPlan) != Pokerogue3DS::PokemonExperienceResult::InvalidLevel ||
        candyPlan.progress.level != 202) return 531;
    Pokerogue3DS::PokemonFriendshipPolicy friendshipPolicy{};
    friendshipPolicy.resolved = true;
    friendshipPolicy.capped = true;
    friendshipPolicy.friendshipCap = 200;
    Pokerogue3DS::PokemonFriendshipChangePlan friendshipPlan{};
    if (Pokerogue3DS::planPokemonFriendshipChange(198, 6, friendshipPolicy, friendshipPlan) !=
            Pokerogue3DS::PokemonExperienceResult::Ok || friendshipPlan.friendship != 200 ||
        friendshipPlan.candyFriendshipGain != 6) return 532;
    friendshipPolicy.boosterStacks = 1;
    friendshipPolicy.candyMultiplier = 1.5;
    if (Pokerogue3DS::planPokemonFriendshipChange(212, 6, friendshipPolicy, friendshipPlan) !=
            Pokerogue3DS::PokemonExperienceResult::Ok || friendshipPlan.friendship != 212 ||
        friendshipPlan.candyFriendshipGain != 13) return 533;
    friendshipPolicy.resolved = false;
    if (Pokerogue3DS::planPokemonFriendshipChange(3, -5, friendshipPolicy, friendshipPlan) !=
            Pokerogue3DS::PokemonExperienceResult::Ok || friendshipPlan.friendship ||
        friendshipPlan.candyFriendshipGain) return 534;
    if (Pokerogue3DS::planPokemonFriendshipChange(198, 6, friendshipPolicy, friendshipPlan) !=
            Pokerogue3DS::PokemonExperienceResult::UnresolvedPolicy || friendshipPlan.friendship) return 535;
    friendshipPolicy.resolved = true;
    friendshipPolicy.capped = false;
    if (Pokerogue3DS::planPokemonFriendshipChange(250, 6, friendshipPolicy, friendshipPlan) !=
            Pokerogue3DS::PokemonExperienceResult::Ok || friendshipPlan.friendship != 255 ||
        !friendshipPlan.requiresMaxFriendshipCallbacks) return 536;
    Pokerogue3DS::StarterCandyProgressPlan candyProgress{};
    if (Pokerogue3DS::planStarterCandyProgress(24, 52, 25, true, true, candyProgress) !=
            Pokerogue3DS::PokemonExperienceResult::Ok || candyProgress.friendship != 1 ||
        candyProgress.candyAward != 3) return 537;
    if (Pokerogue3DS::planStarterCandyProgress(24, 52, 25, true, false, candyProgress) !=
            Pokerogue3DS::PokemonExperienceResult::Ok || candyProgress.friendship != 24 ||
        candyProgress.candyAward) return 538;
    if (Pokerogue3DS::planStarterCandyProgress(24, 52, 25, false, false, candyProgress) !=
            Pokerogue3DS::PokemonExperienceResult::UnresolvedPolicy || candyProgress.friendship != 24)
        return 539;
    if (Pokerogue3DS::planStarterCandyProgress(0xffffffffU, 1, 25, true, true, candyProgress) !=
            Pokerogue3DS::PokemonExperienceResult::Overflow || candyProgress.friendship != 24) return 540;
    auto faintFriendshipActor = highLevelActor;
    faintFriendshipActor.friendship = 50;
    if (Pokerogue3DS::applyPokemonFaintFriendship(faintFriendshipActor) ||
        faintFriendshipActor.friendship != 50) return 541;
    faintFriendshipActor.hp = 0;
    if (!Pokerogue3DS::applyPokemonFaintFriendship(faintFriendshipActor) ||
        faintFriendshipActor.friendship != 50 - PokerogueContent::kFriendshipLossFromFaint ||
        faintFriendshipActor.hp) return 542;
    faintFriendshipActor.friendship = 1;
    if (!Pokerogue3DS::applyPokemonFaintFriendship(faintFriendshipActor) ||
        faintFriendshipActor.friendship) return 543;
    const auto* bulbasaurRoot = Pokerogue3DS::pokemonFriendshipStarterSpecies(2);
    if (!bulbasaurRoot || bulbasaurRoot->dex != 1 ||
        Pokerogue3DS::pokemonFriendshipStarterSpecies(0)) return 544;
    for (const auto& species : PokerogueContent::kSpecies) {
        const auto* root = Pokerogue3DS::pokemonFriendshipStarterSpecies(species.dex);
        const auto* starterRoot = Pokerogue3DS::pokemonRootSpecies(species.dex, true);
        if (!root || root->prevolutionDex || !starterRoot ||
            (starterRoot->prevolutionDex && !starterRoot->starterEligible)) return 545;
    }
    highLevelInput.level = 65535;
    const uint16_t previousHp = highLevelActor.hp;
    if (Pokerogue3DS::initializePokemonBattleState(highLevelInput, highLevelActor) !=
            PokemonBattleInitResult::InvalidStatRange || highLevelActor.hp != previousHp) return 463;

    // Canonical neutral-weather actors: residual phase is separate from clock expiry.
    const PokerogueContent::Species* normalSpecies = nullptr;
    const PokerogueContent::Species* rockSpecies = nullptr;
    for (const auto& species : PokerogueContent::kSpecies) {
        if (!Pokerogue3DS::pokemonWeatherLifecycleSupported(species.ability1)) continue;
        if (!normalSpecies && std::strcmp(species.type1, "Normal") == 0 &&
            (!species.type2 || std::strcmp(species.type2, "NONE") == 0)) normalSpecies = &species;
        if (!rockSpecies && std::strcmp(species.type1, "Rock") == 0) rockSpecies = &species;
    }
    if (!normalSpecies || !rockSpecies) return 435;
    PokemonBattleInit weatherActorInput{};
    weatherActorInput.speciesDex = normalSpecies->dex;
    weatherActorInput.level = 50;
    weatherActorInput.abilityId = normalSpecies->ability1;
    PokemonBattleState phasePlayer{}, phaseEnemy{};
    if (Pokerogue3DS::initializePokemonBattleState(weatherActorInput, phasePlayer) != PokemonBattleInitResult::Ok)
        return 436;
    weatherActorInput.speciesDex = rockSpecies->dex;
    weatherActorInput.abilityId = rockSpecies->ability1;
    if (Pokerogue3DS::initializePokemonBattleState(weatherActorInput, phaseEnemy) != PokemonBattleInitResult::Ok)
        return 437;
    Pokerogue3DS::PokemonArenaWeatherState phaseWeather{Pokerogue3DS::PokemonEffectiveWeather::Sandstorm, 3, 5};
    Pokerogue3DS::PokemonWeatherPhaseEvent phaseEvent{};
    const uint16_t playerBefore = phasePlayer.hp, enemyBefore = phaseEnemy.hp;
    if (!Pokerogue3DS::applyPokemonSingleWeatherPhase(phasePlayer, phaseEnemy, phaseWeather, false, phaseEvent) ||
        phasePlayer.hp >= playerBefore || phaseEnemy.hp != enemyBefore || !phaseEvent.player.damageApplied ||
        phaseEvent.enemy.damageApplied || phaseWeather.turnsLeft != 3) return 438;
    const uint16_t afterResidual = phasePlayer.hp;
    if (!Pokerogue3DS::applyPokemonSingleWeatherPhase(phasePlayer, phaseEnemy, phaseWeather, true, phaseEvent) ||
        phasePlayer.hp != afterResidual || phaseEvent.player.damageApplied) return 439;
    PokemonBattleState invalidWeatherActor = phaseEnemy;
    invalidWeatherActor.abilityId = 65535;
    if (Pokerogue3DS::applyPokemonSingleWeatherPhase(phasePlayer, invalidWeatherActor, phaseWeather, false, phaseEvent) ||
        phasePlayer.hp != afterResidual) return 440;

    // Real opposing StatStageChangeAttr moves (NEAR_OTHER / ALL_NEAR_ENEMIES).
    const auto* tailWhip = PokerogueContent::findMoveById(39);
    const auto* screech = PokerogueContent::findMoveById(103);
    if (!tailWhip || !screech || std::strcmp(screech->target, "NEAR_OTHER") != 0 ||
        std::strcmp(tailWhip->target, "ALL_NEAR_ENEMIES") != 0) return 441;

    double aiScore = 0;
    PokemonBattleState aiUser{}, aiOpponent{};
    aiUser.hp = aiUser.maxHp = 100; aiOpponent.hp = aiOpponent.maxHp = 100;
    aiUser.moveCount = 2;
    aiUser.moves[0].moveId = 103; aiUser.moves[0].pp = 40; // Screech (DEF -2)
    aiUser.moves[1].moveId = 33;  aiUser.moves[1].pp = 35; // Tackle (Physical move required to value DEF drops)
    if (!Pokerogue3DS::calculateCanonicalStatStageStatusAiScore(aiUser, aiOpponent, 103, aiScore) || aiScore <= 0.0)
        return 442;

    PokerogueRngAdapter stageRng;
    const uint16_t stageSeed[] = {'s', 't', 'a', 'g', 'e'};
    stageRng.sow(stageSeed, 5);
    PokemonBattleState screechUser = aiUser, screechTarget = aiOpponent;
    Pokerogue3DS::PokemonStatStageCommandPolicy screechPolicy{};
    screechPolicy.move.hitPolicyResolved = screechPolicy.move.stagePolicy.resolved = true;
    screechPolicy.move.bypassAccuracy = true;
    screechPolicy.postChangePoliciesResolved = screechPolicy.recipientReaction.resolved =
        screechPolicy.sourceReaction.resolved = screechPolicy.reflection.resolved =
        screechPolicy.opponentCopy.resolved = true;
    Pokerogue3DS::PokemonStatStageCommandEvent screechEvent{};
    if (Pokerogue3DS::usePokemonStatStageStatusCommand(screechUser, screechTarget, 0, screechPolicy, stageRng, screechEvent) !=
        Pokerogue3DS::PokemonStatStageEffectResult::Ok || !screechEvent.move.hit ||
        screechTarget.statStages[1] != -2 || screechUser.moves[0].pp != 39) return 443;

    PokemonBattleState soundUser = aiUser, soundTarget = aiOpponent;
    soundUser.moves[0].moveId = 45; soundUser.moves[0].pp = 40; // Growl
    Pokerogue3DS::PokemonStatStageCommandPolicy soundPolicy = screechPolicy;
    soundPolicy.move.bypassAccuracy = false;
    soundPolicy.move.blockedBeforeAccuracy = true;
    Pokerogue3DS::PokemonStatStageCommandEvent soundEvent{};
    if (Pokerogue3DS::usePokemonStatStageStatusCommand(soundUser, soundTarget, 0, soundPolicy, stageRng, soundEvent) !=
        Pokerogue3DS::PokemonStatStageEffectResult::Ok || soundEvent.move.hit ||
        soundTarget.statStages[0] != 0 || soundUser.moves[0].pp != 39) return 444;

    PokemonBattleState clearUser = aiUser, clearTarget = aiOpponent;
    clearUser.moves[0].moveId = 39; clearUser.moves[0].pp = 30; // Tail Whip (DEF -1)
    Pokerogue3DS::PokemonStatStageCommandPolicy clearPolicy = screechPolicy;
    const auto* clearBodyProfile = PokerogueContent::findAbilityStatStageProfile(29); // Clear Body
    if (!clearBodyProfile || clearBodyProfile->protectedMask != 127) return 445;
    const Pokerogue3DS::ResolvedStatStageAbilityComponent clearComp[] = {{clearBodyProfile, true}};
    const PokerogueContent::MoveStatStageEffect tailWhipEffect{39, 2, -1, false};
    if (!Pokerogue3DS::composePokemonStatStageAbilityPolicy(tailWhipEffect, clearComp, 1, false, clearPolicy.move.stagePolicy, true))
        return 446;
    Pokerogue3DS::PokemonStatStageCommandEvent clearEvent{};
    if (Pokerogue3DS::usePokemonStatStageStatusCommand(clearUser, clearTarget, 0, clearPolicy, stageRng, clearEvent) !=
        Pokerogue3DS::PokemonStatStageEffectResult::Ok || !clearEvent.move.hit ||
        clearEvent.move.stages.changedStatMask != 0 || clearTarget.statStages[1] != 0 ||
        clearUser.moves[0].pp != 29) return 447;

    PokemonBattleState mirrorUser = aiUser, mirrorTarget = aiOpponent;
    mirrorUser.moves[0].moveId = 103; mirrorUser.moves[0].pp = 40; // Screech
    Pokerogue3DS::PokemonStatStageCommandPolicy mirrorPolicy = screechPolicy;
    const auto* mirrorArmorProfile = PokerogueContent::findAbilityStatStageProfile(240); // Mirror Armor
    if (!mirrorArmorProfile || !mirrorArmorProfile->reflectDrops) return 448;
    const Pokerogue3DS::ResolvedStatStageAbilityComponent mirrorComp[] = {{mirrorArmorProfile, true}};
    const PokerogueContent::MoveStatStageEffect screechEffect{103, 2, -2, false};
    if (!Pokerogue3DS::composePokemonStatStageAbilityPolicy(screechEffect, mirrorComp, 1, false, mirrorPolicy.move.stagePolicy, true))
        return 449;
    Pokerogue3DS::PokemonStatStageCommandEvent mirrorEvent{};
    if (Pokerogue3DS::usePokemonStatStageStatusCommand(mirrorUser, mirrorTarget, 0, mirrorPolicy, stageRng, mirrorEvent) !=
        Pokerogue3DS::PokemonStatStageEffectResult::Ok || !mirrorEvent.move.hit ||
        mirrorTarget.statStages[1] != 0 || mirrorUser.statStages[1] != -2 ||
        !(mirrorEvent.reflection.changedStatMask & 2)) return 450;

    // Real imported WeatherChangeAttr records; no synthetic move catalog.
    if (sizeof(PokerogueContent::kMoveWeatherChangeProfiles) == 0) return 427;
    for (const auto& profile : PokerogueContent::kMoveWeatherChangeProfiles) {
        const auto* move = PokerogueContent::findMoveById(profile.moveId);
        if (!move || !profile.sourcePath || !*profile.sourcePath ||
            !profile.sourceSymbol || !*profile.sourceSymbol ||
            !profile.sourceHash || !*profile.sourceHash) return 428;
        PokemonBattleState user{};
        user.hp = user.maxHp = 100;
        user.moveCount = 1;
        user.moves[0].moveId = move->id;
        user.moves[0].pp = user.moves[0].maxPp = move->pp;
        Pokerogue3DS::PokemonArenaWeatherState arena{};
        Pokerogue3DS::PokemonWeatherChangePolicy policy{};
        Pokerogue3DS::PokemonWeatherChangeEvent event{};
        policy.resolved = true;
        policy.duration = 5;
        policy.ppCost = 1;
        if (Pokerogue3DS::usePokemonWeatherChangeCommand(user, arena, 0, policy, event) !=
                Pokerogue3DS::PokemonWeatherChangeResult::UnresolvedPolicy ||
            user.moves[0].pp != move->pp || arena.type != Pokerogue3DS::PokemonEffectiveWeather::None) return 429;
        policy.weatherCallbacksResolved = true;
        if (Pokerogue3DS::usePokemonWeatherChangeCommand(user, arena, 0, policy, event) !=
                Pokerogue3DS::PokemonWeatherChangeResult::Ok || !event.changed || event.ppSpent != 1 ||
            static_cast<uint8_t>(arena.type) != profile.weatherType ||
            arena.turnsLeft != 5 || arena.maxDuration != 5) return 430;
        if (Pokerogue3DS::usePokemonWeatherChangeCommand(user, arena, 0, policy, event) !=
                Pokerogue3DS::PokemonWeatherChangeResult::Ok || !event.failedCondition ||
            event.changed || event.ppSpent != 1 || user.moves[0].pp != move->pp - 2 ||
            arena.turnsLeft != 5) return 431;
        arena = {Pokerogue3DS::PokemonEffectiveWeather::HeavyRain, 0, 0};
        if (Pokerogue3DS::usePokemonWeatherChangeCommand(user, arena, 0, policy, event) !=
                Pokerogue3DS::PokemonWeatherChangeResult::Ok || !event.failedCondition ||
            event.changed || arena.type != Pokerogue3DS::PokemonEffectiveWeather::HeavyRain) return 432;
        policy.blockedBeforeMove = true;
        const uint8_t ppBefore = user.moves[0].pp;
        if (Pokerogue3DS::usePokemonWeatherChangeCommand(user, arena, 0, policy, event) !=
                Pokerogue3DS::PokemonWeatherChangeResult::Ok || !event.blocked || event.ppSpent ||
            user.moves[0].pp != ppBefore) return 433;
        policy.blockedBeforeMove = false;
        policy.ppCost = 2;
        arena = {};
        if (Pokerogue3DS::usePokemonWeatherChangeCommand(user, arena, 0, policy, event) !=
                Pokerogue3DS::PokemonWeatherChangeResult::Ok || event.ppSpent != 2 ||
            user.moves[0].pp != 0 || !event.changed) return 434;
    }

    // Real canonical Recover/Soft-Boiled: no invented move identifiers.
    uint16_t recoverId = 0, softBoiledId = 0;
    for (const auto& move : PokerogueContent::kMoves) {
        if (std::strcmp(move.key, "recover") == 0) recoverId = move.id;
        if (std::strcmp(move.key, "soft-boiled") == 0 || std::strcmp(move.key, "soft_boiled") == 0)
            softBoiledId = move.id;
    }
    if (!recoverId || !softBoiledId || !Pokerogue3DS::selfHealingProfile(recoverId) ||
        !Pokerogue3DS::selfHealingProfile(softBoiledId)) return 400;
    PokemonBattleState healing{};
    healing.maxHp = 101; healing.hp = 20; healing.moveCount = 1;
    healing.moves[0].moveId = recoverId; healing.moves[0].pp = healing.moves[0].maxPp = 5;
    Pokerogue3DS::PokemonHealingPolicy healPolicy{};
    Pokerogue3DS::PokemonHealingEvent healEvent{};
    if (Pokerogue3DS::usePokemonSelfHealingCommand(healing, 0, healPolicy, healEvent) !=
        Pokerogue3DS::PokemonHealingResult::UnresolvedPolicy || healing.hp != 20 || healing.moves[0].pp != 5) return 401;
    healPolicy.resolved = true;
    if (Pokerogue3DS::usePokemonSelfHealingCommand(healing, 0, healPolicy, healEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || healing.hp != 71 || healEvent.healed != 51 ||
        healEvent.ppSpent != 1 || healing.moves[0].pp != 4) return 402;
    if (Pokerogue3DS::usePokemonSelfHealingCommand(healing, 0, healPolicy, healEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || healing.hp != 101 || healEvent.healed != 30) return 403;
    if (Pokerogue3DS::usePokemonSelfHealingCommand(healing, 0, healPolicy, healEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || !healEvent.failedFullHp || healEvent.healed ||
        healing.moves[0].pp != 2) return 404;
    healing.hp = 10; healPolicy.healBlocked = true;
    if (Pokerogue3DS::usePokemonSelfHealingCommand(healing, 0, healPolicy, healEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || !healEvent.blocked || healing.hp != 10 ||
        healing.moves[0].pp != 1) return 405;
    healPolicy.blockedBeforeMove = true;
    if (Pokerogue3DS::usePokemonSelfHealingCommand(healing, 0, healPolicy, healEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || healEvent.ppSpent || healing.moves[0].pp != 1) return 406;
    healing.hp = 0;
    if (Pokerogue3DS::usePokemonSelfHealingCommand(healing, 0, healPolicy, healEvent) !=
        Pokerogue3DS::PokemonHealingResult::InvalidState || healing.hp) return 407;
    healing.hp = 101; double healScore = 0;
    if (!Pokerogue3DS::canonicalSelfHealingAiScore(healing, recoverId, healScore) || healScore != -7) return 408;
    healing.hp = 20;
    if (!Pokerogue3DS::canonicalSelfHealingAiScore(healing, recoverId, healScore) || healScore != 15) return 409;

    healing.maxHp = 7; healing.hp = 1; healing.moves[0].pp = 1;
    healPolicy.blockedBeforeMove = healPolicy.healBlocked = false;
    healPolicy.healingMultiplier = 1.5;
    if (Pokerogue3DS::usePokemonSelfHealingCommand(healing, 0, healPolicy, healEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || healEvent.healed != 6 || healing.hp != 7) return 410;
    uint16_t absorbId = 0;
    for (const auto& move : PokerogueContent::kMoves)
        if (std::strcmp(move.key, "absorb") == 0) absorbId = move.id;
    if (!absorbId || !Pokerogue3DS::damageDrainProfile(absorbId)) return 411;
    Pokerogue3DS::PokemonDrainPolicy drainPolicy{};
    Pokerogue3DS::PokemonDrainEvent drainEvent{};
    healing.maxHp = 101; healing.hp = 20;
    if (Pokerogue3DS::applyPokemonDamageDrain(healing, absorbId, 3, drainPolicy, drainEvent) !=
        Pokerogue3DS::PokemonHealingResult::UnresolvedPolicy || healing.hp != 20) return 412;
    drainPolicy.resolved = true;
    if (Pokerogue3DS::applyPokemonDamageDrain(healing, absorbId, 3, drainPolicy, drainEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || drainEvent.healed != 1 || healing.hp != 21) return 413;
    if (Pokerogue3DS::applyPokemonDamageDrain(healing, absorbId, 1, drainPolicy, drainEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || drainEvent.healed != 1 || healing.hp != 22) return 414;
    if (Pokerogue3DS::applyPokemonDamageDrain(healing, absorbId, 0, drainPolicy, drainEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || drainEvent.healed || healing.hp != 22) return 415;
    drainPolicy.reverseDrain = true; drainPolicy.healingMultiplier = 1.5;
    if (Pokerogue3DS::applyPokemonDamageDrain(healing, absorbId, 3, drainPolicy, drainEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || drainEvent.reversedDamage != 2 || healing.hp != 20) return 416;
    drainPolicy.indirectDamageBlocked = true;
    if (Pokerogue3DS::applyPokemonDamageDrain(healing, absorbId, 3, drainPolicy, drainEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || !drainEvent.blocked || drainEvent.healed ||
        drainEvent.reversedDamage || healing.hp != 20) return 417;
    drainPolicy.reverseDrain = false; drainPolicy.healingMultiplier = 1; drainPolicy.healBlocked = true;
    if (Pokerogue3DS::applyPokemonDamageDrain(healing, absorbId, 3, drainPolicy, drainEvent) !=
        Pokerogue3DS::PokemonHealingResult::Ok || !drainEvent.blocked || healing.hp != 20) return 418;
    uint16_t takeDownId = 0, struggleId = 0;
    for (const auto& move : PokerogueContent::kMoves) {
        if (std::strcmp(move.key, "take_down") == 0) takeDownId = move.id;
        if (std::strcmp(move.key, "struggle") == 0) struggleId = move.id;
    }
    if (!takeDownId || !struggleId || !Pokerogue3DS::damageRecoilProfile(takeDownId) ||
        !Pokerogue3DS::canonicalRecoilProfile(struggleId)) return 419;
    healing.maxHp = 101; healing.hp = 80;
    Pokerogue3DS::PokemonRecoilPolicy recoilPolicy{};
    Pokerogue3DS::PokemonRecoilEvent recoilEvent{};
    if (Pokerogue3DS::applyPokemonRecoil(healing, takeDownId, 11, true, recoilPolicy, recoilEvent) !=
        Pokerogue3DS::PokemonRecoilResult::UnresolvedPolicy || healing.hp != 80) return 420;
    recoilPolicy.resolved = true;
    if (Pokerogue3DS::applyPokemonRecoil(healing, takeDownId, 11, true, recoilPolicy, recoilEvent) !=
        Pokerogue3DS::PokemonRecoilResult::Ok || recoilEvent.damage != 2 || healing.hp != 78) return 421;
    if (Pokerogue3DS::applyPokemonRecoil(healing, takeDownId, 1, true, recoilPolicy, recoilEvent) !=
        Pokerogue3DS::PokemonRecoilResult::Ok || recoilEvent.damage != 1) return 422;
    recoilPolicy.abilityBlocksRecoil = true;
    if (Pokerogue3DS::applyPokemonRecoil(healing, takeDownId, 11, true, recoilPolicy, recoilEvent) !=
        Pokerogue3DS::PokemonRecoilResult::Ok || !recoilEvent.blocked || recoilEvent.damage) return 423;
    if (Pokerogue3DS::applyPokemonRecoil(healing, struggleId, 0, true, recoilPolicy, recoilEvent) !=
        Pokerogue3DS::PokemonRecoilResult::Ok || recoilEvent.blocked || recoilEvent.damage != 25) return 424;
    if (Pokerogue3DS::applyPokemonRecoil(healing, struggleId, 0, false, recoilPolicy, recoilEvent) !=
        Pokerogue3DS::PokemonRecoilResult::Ok || recoilEvent.damage) return 425;
    recoilPolicy.abilityBlocksRecoil = false; healing.hp = 1;
    if (Pokerogue3DS::applyPokemonRecoil(healing, takeDownId, 11, true, recoilPolicy, recoilEvent) !=
        Pokerogue3DS::PokemonRecoilResult::Ok || !recoilEvent.fainted || healing.hp) return 426;
    uint32_t awardedExperience = 99;
    if (Pokerogue3DS::pokemonSingleParticipantExperience(52.2, false, awardedExperience) !=
        Pokerogue3DS::PokemonExperienceResult::Ok || awardedExperience != 52) return 198;
    if (Pokerogue3DS::pokemonSingleParticipantExperience(52.2, true, awardedExperience) !=
        Pokerogue3DS::PokemonExperienceResult::Ok || awardedExperience != 78) return 199;
    if (Pokerogue3DS::pokemonSingleParticipantExperience(-1.0, true, awardedExperience) !=
        Pokerogue3DS::PokemonExperienceResult::Overflow || awardedExperience != 0) return 200;
    if (Pokerogue3DS::pokemonSingleParticipantExperience(4294967295.0, true, awardedExperience) !=
        Pokerogue3DS::PokemonExperienceResult::Overflow) return 201;
    const uint16_t trainerAiSeed[] = {'t', 'r', 'a', 'i', 'n', 'e', 'r'};
    PokerogueRngAdapter trainerAiRng;
    trainerAiRng.sow(trainerAiSeed, 7);
    const double tiedScores[3] = {10.0, 10.0, -1.0};
    const uint8_t tiedSlots[3] = {3, 1, 2};
    PokerogueRngAdapter expectedTrainerAiRng = trainerAiRng;
    const uint8_t expectedTrainerSlot = expectedTrainerAiRng.randSeedInt(100) < 50 ? 1 : 3;
    uint8_t selectedTrainerSlot = 0;
    if (!Pokerogue3DS::selectSmartTrainerMoveSlot(tiedScores, tiedSlots, 3,
            trainerAiRng, selectedTrainerSlot) || selectedTrainerSlot != expectedTrainerSlot)
        return 176;
    const auto sameTrainerRng = [](const Pokerogue3DS::PokerogueRngState& left,
                                   const Pokerogue3DS::PokerogueRngState& right) {
        return left.carry == right.carry && left.s0 == right.s0 &&
            left.s1 == right.s1 && left.s2 == right.s2;
    };
    if (!sameTrainerRng(trainerAiRng.state(), expectedTrainerAiRng.state())) return 177;
    const double separatedScores[2] = {8.0, -2.0};
    const uint8_t separatedSlots[2] = {2, 0};
    const auto beforeNoDraw = trainerAiRng.state();
    if (!Pokerogue3DS::selectSmartTrainerMoveSlot(separatedScores, separatedSlots, 2,
            trainerAiRng, selectedTrainerSlot) || selectedTrainerSlot != 2 ||
        !sameTrainerRng(trainerAiRng.state(), beforeNoDraw)) return 178;
    Pokerogue3DS::PokemonTrainerMatchupInput matchup{};
    matchup.usableAttackCount = 1;
    matchup.attackEffectiveness[0] = 1.5;
    matchup.hpRatio = 0.1;
    matchup.active = true;
    matchup.outspeeds = true;
    double matchupScore = 0;
    if (!Pokerogue3DS::calculateTrainerMatchupScore(matchup, matchupScore) ||
        matchupScore != 2.5) return 183;
    matchup.active = false;
    if (!Pokerogue3DS::calculateTrainerMatchupScore(matchup, matchupScore) ||
        matchupScore != 0.3125) return 184;
    bool wantsSwitch = false;
    if (!Pokerogue3DS::shouldTrainerSwitch(1.0, 3.0, 0, false, wantsSwitch) ||
        !wantsSwitch) return 185;
    if (!Pokerogue3DS::shouldTrainerSwitch(1.0, 3.0, 1, false, wantsSwitch) ||
        wantsSwitch) return 186;
    const double reserveScores[3] = {2.0, 3.0, 3.0};
    const uint8_t reserveIndexes[3] = {1, 4, 2};
    PokerogueRngAdapter summonRng = trainerAiRng;
    PokerogueRngAdapter expectedSummonRng = summonRng;
    const uint8_t expectedSummon = expectedSummonRng.randSeedInt(2) ? 2 : 4;
    uint8_t nextSummon = 0;
    if (!Pokerogue3DS::selectTrainerSummonIndex(reserveScores, reserveIndexes, 3,
            summonRng, nextSummon) || nextSummon != expectedSummon ||
        !sameTrainerRng(summonRng.state(), expectedSummonRng.state())) return 187;
    double plainAttackScore = 0;
    if (!Pokerogue3DS::calculatePlainAttackAiScore(1.0, 20, 20, 40, 100,
            true, plainAttackScore) || plainAttackScore != 12.0) return 188;
    if (!Pokerogue3DS::calculatePlainAttackAiScore(1.0, 20, 20, 50, 80,
            false, plainAttackScore) || plainAttackScore != 8.0) return 189;
    if (!Pokerogue3DS::calculatePlainAttackAiScore(0.0, 20, 20, 40, 100,
            true, plainAttackScore) || plainAttackScore != -20.0) return 190;
    if (!Pokerogue3DS::calculatePlainAttackAiScore(2.0, 20, 10, 40, 100,
            false, plainAttackScore) || plainAttackScore != 24.0) return 191;
    const uint8_t koInputSlots[3] = {3, 0, 2};
    const uint32_t koDamage[3] = {9, 10, 12};
    uint8_t koSlots[4]{};
    uint8_t koCount = 0;
    if (!Pokerogue3DS::filterEnemyKoMoveSlots(koInputSlots, koDamage, 3, 10,
            koSlots, koCount) || koCount != 2 || koSlots[0] != 0 || koSlots[1] != 2)
        return 192;
    if (!Pokerogue3DS::filterEnemyKoMoveSlots(koInputSlots, koDamage, 3, 13,
            koSlots, koCount) || koCount != 3 || koSlots[0] != 3 || koSlots[2] != 2)
        return 193;
    if (Pokerogue3DS::filterEnemyKoMoveSlots(koInputSlots, koDamage, 3, 0,
            koSlots, koCount) || koCount) return 194;
    uint8_t derivedIvs[6]{};
    Pokerogue3DS::derivePokemonIvsFromId(0xFFFFFFFFu, derivedIvs);
    for (uint8_t iv : derivedIvs) if (iv != 31) return 57;
    Pokerogue3DS::derivePokemonIvsFromId(0, derivedIvs);
    for (uint8_t iv : derivedIvs) if (iv != 0) return 58;
    Pokerogue3DS::derivePokemonIvsFromId(0x12345678u, derivedIvs);
    const uint8_t expectedDerivedIvs[6] = {9, 3, 8, 21, 19, 24};
    for (uint8_t i = 0; i < 6; ++i) if (derivedIvs[i] != expectedDerivedIvs[i]) return 59;

    const int8_t expectedRaised[25] = {
        -1, 1, 1, 1, 1, 2, -1, 2, 2, 2, 5, 5, -1, 5, 5,
        3, 3, 3, -1, 3, 4, 4, 4, 4, -1,
    };
    const int8_t expectedLowered[25] = {
        -1, 2, 5, 3, 4, 1, -1, 5, 3, 4, 1, 2, -1, 3, 4,
        1, 2, 5, -1, 4, 1, 2, 5, 3, -1,
    };
    for (uint8_t i = 0; i < 25; ++i) {
        Pokerogue3DS::PokemonNatureModifiers nature{};
        if (!Pokerogue3DS::getPokemonNatureModifiers(static_cast<Pokerogue3DS::PokemonNature>(i), nature) ||
            nature.raisedStat != expectedRaised[i] || nature.loweredStat != expectedLowered[i]) return 74;
    }
    Pokerogue3DS::PokemonNatureModifiers invalidNature{};
    if (Pokerogue3DS::getPokemonNatureModifiers(Pokerogue3DS::PokemonNature::Unspecified, invalidNature)) return 75;
    const uint16_t natureSeed[] = {'n', 'a', 't', 'u', 'r', 'e'};
    PokerogueRngAdapter natureRng;
    natureRng.sow(natureSeed, sizeof(natureSeed) / sizeof(natureSeed[0]));
    PokerogueRngAdapter natureExpectedRng = natureRng;
    const auto expectedNature = static_cast<Pokerogue3DS::PokemonNature>(natureExpectedRng.randSeedInt(25));
    if (Pokerogue3DS::selectPokemonNature(natureRng) != expectedNature) return 76;
    const auto natureActualState = natureRng.state();
    const auto natureExpectedState = natureExpectedRng.state();
    if (natureActualState.carry != natureExpectedState.carry || natureActualState.s0 != natureExpectedState.s0 ||
        natureActualState.s1 != natureExpectedState.s1 || natureActualState.s2 != natureExpectedState.s2) return 77;

    PokemonBattleInit input{};
    input.speciesDex = 1;
    input.level = 5;
    input.abilityId = 65;
    input.moveCount = 2;
    input.moveIds[0] = 33;
    input.moveIds[1] = 45;
    input.gender = PokemonGender::Male;

    PokemonBattleState state{};
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::Ok) return 1;
    if (state.maxHp != 19 || state.hp != 19 || state.stats[1] != 9 || state.stats[2] != 9) return 2;
    if (state.moveCount != 2 || state.moves[0].moveId != 33 || state.moves[0].pp != 35 ||
        state.moves[1].moveId != 45 || state.moves[1].pp != 40) return 3;
    if (state.gender != PokemonGender::Male) return 28;
    if (!state.statsAreBaseFormulaOnly) return 4;
    for (uint8_t iv : state.ivs) if (iv != 0) return 60;

    PokemonBattleInit matchupOpponentInput{};
    matchupOpponentInput.speciesDex = 7;
    matchupOpponentInput.level = 5;
    matchupOpponentInput.abilityId = 67;
    matchupOpponentInput.gender = PokemonGender::Male;
    PokemonBattleState matchupOpponent{};
    if (Pokerogue3DS::initializePokemonBattleState(matchupOpponentInput, matchupOpponent) !=
        PokemonBattleInitResult::Ok) return 195;
    PokemonBattleState matchupActor = state;
    matchupActor.moves[0].moveId = 22; // Canonical Vine Whip against Squirtle.
    Pokerogue3DS::PokemonTrainerMatchupInput resolvedMatchup{};
    if (!Pokerogue3DS::buildBaselineTrainerMatchupInput(matchupActor, matchupOpponent,
            10, 9, true, resolvedMatchup) || resolvedMatchup.usableAttackCount != 1 ||
        resolvedMatchup.attackEffectiveness[0] != 3.0 ||
        resolvedMatchup.defensiveEffectiveness[0] != 0.5 || !resolvedMatchup.outspeeds)
        return 196;
    matchupActor.moves[0].pp = 0;
    if (!Pokerogue3DS::buildBaselineTrainerMatchupInput(matchupActor, matchupOpponent,
            9, 10, false, resolvedMatchup) || resolvedMatchup.usableAttackCount != 0 ||
        resolvedMatchup.outspeeds || resolvedMatchup.active) return 197;
    PokemonBattleState replacementParty[3] = {state, state, state};
    replacementParty[0].hp = 0;
    replacementParty[1].hp = 0;
    uint8_t replacementIndex = 99;
    PokerogueRngAdapter replacementRng;
    replacementRng.sow(trainerAiSeed, sizeof(trainerAiSeed) / sizeof(trainerAiSeed[0]));
    if (!Pokerogue3DS::selectBaselineTrainerReplacement(replacementParty, 3, 0,
            matchupOpponent, replacementRng, replacementIndex) || replacementIndex != 2)
        return 202;
    PokemonBattleState stagedReserveParty[3] = {state, state, state};
    stagedReserveParty[0].hp = 0;
    stagedReserveParty[1].statStages[4] = -6;
    stagedReserveParty[2].statStages[4] = 6;
    PokerogueRngAdapter stagedReserveRng, plainReserveRng;
    stagedReserveRng.sow(trainerAiSeed, sizeof(trainerAiSeed) / sizeof(trainerAiSeed[0]));
    plainReserveRng = stagedReserveRng;
    uint8_t stagedReserveIndex = 99, plainReserveIndex = 99;
    if (!Pokerogue3DS::selectBaselineTrainerReplacement(stagedReserveParty, 3, 0,
            matchupOpponent, stagedReserveRng, stagedReserveIndex)) return 393;
    stagedReserveParty[1].statStages[4] = stagedReserveParty[2].statStages[4] = 0;
    if (!Pokerogue3DS::selectBaselineTrainerReplacement(stagedReserveParty, 3, 0,
            matchupOpponent, plainReserveRng, plainReserveIndex) ||
        stagedReserveIndex != plainReserveIndex) return 394;
    replacementParty[2].hp = 0;
    if (Pokerogue3DS::selectBaselineTrainerReplacement(replacementParty, 3, 0,
            matchupOpponent, replacementRng, replacementIndex)) return 203;
    const double reserveScores[] = {6.0};
    const uint8_t reserveIndexes[] = {1};
    Pokerogue3DS::TrainerSwitchDecision switchDecision{};
    if (!Pokerogue3DS::resolveTrainerSwitchDecision(2.0, reserveScores, reserveIndexes,
            1, 0, false, false, false, replacementRng, switchDecision) ||
        !switchDecision.switchPokemon || switchDecision.partyIndex != 1 ||
        switchDecision.nextSwitchCounter != 1) return 204;
    if (!Pokerogue3DS::resolveTrainerSwitchDecision(2.0, reserveScores, reserveIndexes,
            1, 1, false, false, false, replacementRng, switchDecision) ||
        switchDecision.switchPokemon || switchDecision.nextSwitchCounter) return 205;
    if (!Pokerogue3DS::resolveTrainerSwitchDecision(2.0, reserveScores, reserveIndexes,
            1, 2, true, true, false, replacementRng, switchDecision) ||
        switchDecision.switchPokemon || switchDecision.nextSwitchCounter != 1) return 206;
    if (!Pokerogue3DS::resolveTrainerSwitchDecision(2.0, reserveScores, reserveIndexes,
            1, 2, true, false, true, replacementRng, switchDecision) ||
        switchDecision.switchPokemon || switchDecision.nextSwitchCounter != 1) return 207;
    PokemonBattleState staged = state;
    double stageMultiplier = 0;
    if (!Pokerogue3DS::setPokemonStatStage(staged, 1, -9) || staged.statStages[0] != -6 ||
        !Pokerogue3DS::pokemonStatStageMultiplier(staged, 1, false, stageMultiplier) ||
        stageMultiplier != 0.25) return 208;
    if (!Pokerogue3DS::pokemonStatStageMultiplier(staged, 1, true, stageMultiplier) ||
        stageMultiplier != 1.0) return 209;
    if (!Pokerogue3DS::setPokemonStatStage(staged, 2, 9) ||
        !Pokerogue3DS::pokemonStatStageMultiplier(staged, 2, true, stageMultiplier) ||
        stageMultiplier != 1.0 || Pokerogue3DS::setPokemonStatStage(staged, 0, 1)) return 210;
    if (!Pokerogue3DS::setPokemonStatStage(staged, 6, 6) ||
        !Pokerogue3DS::setPokemonStatStage(matchupOpponent, 7, -6) ||
        !Pokerogue3DS::pokemonAccuracyStageMultiplier(staged, matchupOpponent, stageMultiplier) ||
        stageMultiplier != 3.0) return 211;
    const uint16_t retainedHp = staged.hp;
    Pokerogue3DS::resetPokemonStatStages(staged);
    for (int8_t stage : staged.statStages) if (stage) return 212;
    if (staged.hp != retainedHp || staged.moves[0].pp != state.moves[0].pp) return 213;
    double neutralStageDamage = 0, reducedStageDamage = 0, criticalStageDamage = 0;
    PokemonBattleState stagedDefender = state;
    if (Pokerogue3DS::calculatePokemonBaseDamage(state, stagedDefender, 33,
            neutralStageDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        !Pokerogue3DS::setPokemonStatStage(staged, 1, -6) ||
        !Pokerogue3DS::setPokemonStatStage(stagedDefender, 2, 6) ||
        Pokerogue3DS::calculatePokemonBaseDamage(staged, stagedDefender, 33,
            reducedStageDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        reducedStageDamage >= neutralStageDamage ||
        Pokerogue3DS::calculatePokemonBaseDamage(staged, stagedDefender, 33,
            criticalStageDamage, true) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        criticalStageDamage != neutralStageDamage) return 214;
    PokemonBattleState fastActor = state, slowActor = state;
    fastActor.stats[5] = 20;
    slowActor.stats[5] = 10;
    if (!Pokerogue3DS::setPokemonStatStage(fastActor, 5, -6) ||
        Pokerogue3DS::resolveBaselineFirstMover(fastActor, slowActor, 33, 33,
            trainerAiSeed, sizeof(trainerAiSeed) / sizeof(trainerAiSeed[0]), 5, 1) !=
            Pokerogue3DS::BaselineFirstMover::Enemy) return 215;
    uint32_t effectiveSpeed = 0;
    fastActor.stats[5] = 65535;
    if (!Pokerogue3DS::setPokemonStatStage(fastActor, 5, 6) ||
        !Pokerogue3DS::pokemonBaselineEffectiveStat(fastActor, 5, false, effectiveSpeed) ||
        effectiveSpeed != 262140) return 216;
    bool sawGrowlEffect = false, sawSwordsDanceEffect = false;
    for (const auto& effect : PokerogueContent::kMoveStatStageEffects) {
        if (!PokerogueContent::findMoveById(effect.moveId) || !effect.statMask ||
            effect.statMask > 127 || effect.stages < -6 || effect.stages > 6) return 217;
        if (effect.moveId == 45) {
            if (effect.statMask != 1 || effect.stages != -1 || effect.selfTarget) return 218;
            sawGrowlEffect = true;
        }
        if (effect.moveId == 14) {
            if (effect.statMask != 1 || effect.stages != 2 || !effect.selfTarget) return 219;
            sawSwordsDanceEffect = true;
        }
    }
    if (!sawGrowlEffect || !sawSwordsDanceEffect) return 220;
    PokemonBattleState effectRecipient = state;
    Pokerogue3DS::PokemonStatStageEffectPolicy stagePolicy{};
    Pokerogue3DS::PokemonStatStageEffectEvent stageEvent{};
    const PokerogueContent::MoveStatStageEffect growlEffect{45, 1, -1, false};
    if (Pokerogue3DS::applyPokemonStatStageEffect(effectRecipient, growlEffect, stagePolicy,
            replacementRng, stageEvent) != Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
        effectRecipient.statStages[0]) return 221;
    stagePolicy.resolved = true;
    if (Pokerogue3DS::applyPokemonStatStageEffect(effectRecipient, growlEffect, stagePolicy,
            replacementRng, stageEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        effectRecipient.statStages[0] != -1 || stageEvent.changes[0] != -1 ||
        stageEvent.changedStatMask != 1) return 222;
    stagePolicy.stageMultiplier = -1;
    if (Pokerogue3DS::applyPokemonStatStageEffect(effectRecipient, growlEffect, stagePolicy,
            replacementRng, stageEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        effectRecipient.statStages[0] != 0 || stageEvent.changes[0] != 1) return 223;
    stagePolicy.cancelledStatMask = 1;
    if (Pokerogue3DS::applyPokemonStatStageEffect(effectRecipient, growlEffect, stagePolicy,
            replacementRng, stageEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        stageEvent.changedStatMask || effectRecipient.statStages[0]) return 224;
    const auto* simpleStageProfile = PokerogueContent::findAbilityStatStageProfile(86);
    const auto* contraryStageProfile = PokerogueContent::findAbilityStatStageProfile(126);
    const auto* clearBodyStageProfile = PokerogueContent::findAbilityStatStageProfile(29);
    const auto* metalBodyStageProfile = PokerogueContent::findAbilityStatStageProfile(230);
    if (!simpleStageProfile || simpleStageProfile->multiplier != 2 ||
        !contraryStageProfile || contraryStageProfile->multiplier != -1 ||
        !clearBodyStageProfile || clearBodyStageProfile->protectedMask != 127 ||
        !clearBodyStageProfile->ignorable || !metalBodyStageProfile ||
        metalBodyStageProfile->protectedMask != 127 || metalBodyStageProfile->ignorable)
        return 225;
    const Pokerogue3DS::ResolvedStatStageAbilityComponent clearBodyComponent[] = {
        {clearBodyStageProfile, true}
    };
    Pokerogue3DS::PokemonStatStageEffectPolicy protectedPolicy{};
    if (!Pokerogue3DS::composePokemonStatStageAbilityPolicy(growlEffect,
            clearBodyComponent, 1, false, protectedPolicy) ||
        protectedPolicy.cancelledStatMask != 1 || protectedPolicy.resolved) return 226;
    const Pokerogue3DS::ResolvedStatStageAbilityComponent contraryAndProtection[] = {
        {contraryStageProfile, true}, {clearBodyStageProfile, true}
    };
    protectedPolicy = {};
    if (!Pokerogue3DS::composePokemonStatStageAbilityPolicy(growlEffect,
            contraryAndProtection, 2, false, protectedPolicy) ||
        protectedPolicy.stageMultiplier != -1 || protectedPolicy.cancelledStatMask) return 227;
    protectedPolicy = {};
    if (!Pokerogue3DS::composePokemonStatStageAbilityPolicy(growlEffect,
            contraryAndProtection, 2, true, protectedPolicy) ||
        protectedPolicy.stageMultiplier != 1 || protectedPolicy.cancelledStatMask != 1) return 228;
    PokemonBattleState growlUser = state, growlTarget = state;
    growlUser.moves[0].moveId = 45;
    growlUser.moves[0].pp = 40;
    Pokerogue3DS::PokemonStatStageMovePolicy supportPolicy{};
    Pokerogue3DS::PokemonStatStageMoveEvent supportEvent{};
    if (Pokerogue3DS::usePokemonStatStageStatusMove(growlUser, growlTarget, 0,
            supportPolicy, replacementRng, supportEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
        growlUser.moves[0].pp != 40) return 229;
    supportPolicy.hitPolicyResolved = supportPolicy.stagePolicy.resolved = true;
    if (Pokerogue3DS::usePokemonStatStageStatusMove(growlUser, growlTarget, 0,
            supportPolicy, replacementRng, supportEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || !supportEvent.hit ||
        !supportEvent.accuracyRolled || growlUser.moves[0].pp != 39 ||
        growlTarget.statStages[0] != -1 || growlTarget.hp != state.hp) return 230;
    supportPolicy.blockedBeforeAccuracy = true;
    if (Pokerogue3DS::usePokemonStatStageStatusMove(growlUser, growlTarget, 0,
            supportPolicy, replacementRng, supportEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || supportEvent.hit ||
        supportEvent.accuracyRolled || growlUser.moves[0].pp != 38 ||
        growlTarget.statStages[0] != -1) return 231;
    double supportBenefit = 0;
    PokemonBattleState benefitTarget = state;
    if (!Pokerogue3DS::calculateStatStageTargetBenefit(state, benefitTarget,
            growlEffect, supportBenefit) || supportBenefit != -2) return 232;
    benefitTarget.statStages[0] = -6;
    if (!Pokerogue3DS::calculateStatStageTargetBenefit(state, benefitTarget,
            growlEffect, supportBenefit) || supportBenefit != 2) return 233;
    const PokerogueContent::MoveStatStageEffect swordsDanceEffect{14, 1, 2, true};
    if (!Pokerogue3DS::calculateStatStageTargetBenefit(state, state,
            swordsDanceEffect, supportBenefit) || supportBenefit != 6) return 234;
    PokemonBattleState specialOnly = state;
    specialOnly.moves[0].moveId = 55;
    const PokerogueContent::MoveStatStageEffect specialBoostEffect{417, 4, 2, true};
    if (!Pokerogue3DS::calculateStatStageTargetBenefit(specialOnly, specialOnly,
            specialBoostEffect, supportBenefit) || supportBenefit != 0) return 235;
    if (!Pokerogue3DS::calculateCanonicalStatStageStatusAiScore(state, state, 45,
            supportBenefit) || supportBenefit != 2 ||
        !Pokerogue3DS::calculateCanonicalStatStageStatusAiScore(state, state, 14,
            supportBenefit) || supportBenefit != 6) return 236;
    {
        PokemonBattleState soundUser = state, soundTarget = state;
        soundUser.moveCount = 1;
        soundUser.moves[0] = {45, 40, 40};
        Pokerogue3DS::PokemonStatusMoveHitPolicy soundHit{};
        soundHit.resolved = true;
        const Pokerogue3DS::PokemonStatusAbilityComponent soundDefenders[] = {{43, true, true}};
        if (!Pokerogue3DS::composePokemonStatusFlagAbilityHitPolicy(45, soundHit, false,
                soundDefenders, 1, soundHit) || !soundHit.blockedBeforeAccuracy) return 9550;
        Pokerogue3DS::PokemonStatStageMovePolicy soundPolicy{};
        soundPolicy.hitPolicyResolved = soundPolicy.stagePolicy.resolved = true;
        soundPolicy.blockedBeforeAccuracy = soundHit.blockedBeforeAccuracy;
        Pokerogue3DS::PokemonStatStageMoveEvent soundEvent{};
        auto soundRng = replacementRng, expectedSoundRng = soundRng;
        if (Pokerogue3DS::usePokemonStatStageStatusMove(soundUser, soundTarget, 0, soundPolicy,
                soundRng, soundEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
            soundEvent.hit || soundEvent.accuracyRolled || soundUser.moves[0].pp != 39 ||
            soundTarget.statStages[0] != state.statStages[0] ||
            soundRng.randSeedUint32() != expectedSoundRng.randSeedUint32()) return 9551;
        soundPolicy.blockedBeforeAccuracy = false;
        soundPolicy.typeImmune = true;
        soundRng = expectedSoundRng;
        if (Pokerogue3DS::usePokemonStatStageStatusMove(soundUser, soundTarget, 0, soundPolicy,
                soundRng, soundEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
            soundEvent.hit || !soundEvent.typeImmune || soundEvent.accuracyRolled ||
            soundUser.moves[0].pp != 38 || soundTarget.statStages[0] != state.statStages[0] ||
            soundRng.randSeedUint32() != expectedSoundRng.randSeedUint32()) return 9552;
    }
    {
        PokemonBattleState stageUser = state, stageTarget = state;
        stageUser.moves[0] = {94, 10, 10};
        for (auto& value : stageTarget.statStages) value = 0;
        const PokerogueContent::MoveStatStageEffect psychic{94, 16, -1, false};
        Pokerogue3DS::PokemonStatStageCommandPolicy stagePolicy{};
        stagePolicy.postChangePoliciesResolved = stagePolicy.move.stagePolicy.resolved = true;
        stagePolicy.move.stagePolicy.chance = 100;
        Pokerogue3DS::PokemonStatStageCommandEvent phaseEvent{};
        auto stageRng = replacementRng, expectedStageRng = stageRng;
        const auto userHp = stageUser.hp, targetHp = stageTarget.hp;
        if (Pokerogue3DS::executePokemonDamageStatStagePhase(stageUser, stageTarget, psychic,
                stagePolicy, stageRng, phaseEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
            stageTarget.statStages[4] != -1 || phaseEvent.move.stages.changedStatMask != 16 ||
            stageUser.moves[0].pp != 10 || stageUser.hp != userHp || stageTarget.hp != targetHp ||
            stageRng.randSeedUint32() != expectedStageRng.randSeedUint32()) return 9560;
        stageTarget.hp = 0;
        stagePolicy.move.stagePolicy.chance = 10;
        stageRng = expectedStageRng;
        if (Pokerogue3DS::executePokemonDamageStatStagePhase(stageUser, stageTarget, psychic,
                stagePolicy, stageRng, phaseEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
            phaseEvent.move.stages.triggered || stageTarget.statStages[4] != -1 || stageUser.moves[0].pp != 10 ||
            stageRng.randSeedUint32() != expectedStageRng.randSeedUint32()) return 9561;
        // Even a fainted recipient cannot legitimize a forged definition.
        auto forgedPsychic = psychic;
        forgedPsychic.statMask = 1;
        phaseEvent.move.stages.changedStatMask = 123;
        if (Pokerogue3DS::executePokemonDamageStatStagePhase(stageUser, stageTarget, forgedPsychic,
                stagePolicy, stageRng, phaseEvent) != Pokerogue3DS::PokemonStatStageEffectResult::InvalidDefinition ||
            phaseEvent.move.stages.changedStatMask != 123 || stageTarget.statStages[4] != -1) return 9590;
        auto malformedPolicy = stagePolicy;
        malformedPolicy.move.stagePolicy.chance = 101;
        if (Pokerogue3DS::executePokemonDamageStatStagePhase(stageUser, stageTarget, psychic,
                malformedPolicy, stageRng, phaseEvent) != Pokerogue3DS::PokemonStatStageEffectResult::InvalidDefinition ||
            phaseEvent.move.stages.changedStatMask != 123 || stageTarget.hp) return 9660;
        malformedPolicy = stagePolicy;
        malformedPolicy.move.stagePolicy.reflectedStatMask = 16;
        if (Pokerogue3DS::executePokemonDamageStatStagePhase(stageUser, stageTarget, psychic,
                malformedPolicy, stageRng, phaseEvent) != Pokerogue3DS::PokemonStatStageEffectResult::InvalidDefinition ||
            phaseEvent.move.stages.changedStatMask != 123 || stageTarget.hp) return 9661;
        stagePolicy.move.stagePolicy.resolved = false;
        if (Pokerogue3DS::executePokemonDamageStatStagePhase(stageUser, stageTarget, psychic,
                stagePolicy, stageRng, phaseEvent) != Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
            phaseEvent.move.stages.changedStatMask != 123) return 9591;
        stagePolicy.move.stagePolicy.resolved = true;
        const auto validMaxHp = stageTarget.maxHp;
        stageTarget.hp = 1;
        stageTarget.maxHp = 0;
        if (Pokerogue3DS::executePokemonDamageStatStagePhase(stageUser, stageTarget, psychic,
                stagePolicy, stageRng, phaseEvent) != Pokerogue3DS::PokemonStatStageEffectResult::InvalidState ||
            phaseEvent.move.stages.changedStatMask != 123) return 9592;
        stageTarget.maxHp = validMaxHp;
        stageTarget.hp = targetHp;
        // Failure in a reaction occurs AFTER chance and tentative mutation.
        // Neither that mutation nor the chance draw may escape the transaction.
        const PokerogueContent::AbilityStatStageReaction lateReaction{128, 1, 2, "AbilityId.DEFIANT"};
        stagePolicy.recipientReactions[0] = &lateReaction;
        stagePolicy.move.stagePolicy.chance = 99;
        bool checkedLateFailure = false;
        for (uint32_t seed = 1; seed <= 128; ++seed) {
            Pokerogue3DS::PokerogueRngAdapter lateRng;
            const uint16_t lateSeed[] = {static_cast<uint16_t>(seed)};
            lateRng.sow(lateSeed, 1);
            auto expectedLateRng = lateRng;
            if (expectedLateRng.randSeedInt(100) >= 99) continue;
            expectedLateRng = lateRng;
            phaseEvent.move.stages.changedStatMask = 123;
            if (Pokerogue3DS::executePokemonDamageStatStagePhase(stageUser, stageTarget, psychic,
                    stagePolicy, lateRng, phaseEvent) != Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
                stageTarget.statStages[4] != -1 || stageTarget.hp != targetHp || stageUser.hp != userHp ||
                stageUser.moves[0].pp != 10 || phaseEvent.move.stages.changedStatMask != 123 ||
                lateRng.randSeedUint32() != expectedLateRng.randSeedUint32()) return 9600;
            checkedLateFailure = true;
            break;
        }
        if (!checkedLateFailure) return 9601;
        stagePolicy.recipientReactions[0] = nullptr;
        stagePolicy.postChangePoliciesResolved = false;
        stageRng = expectedStageRng;
        if (Pokerogue3DS::executePokemonDamageStatStagePhase(stageUser, stageTarget, psychic,
                stagePolicy, stageRng, phaseEvent) != Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
            stageTarget.statStages[4] != -1 || stageUser.moves[0].pp != 10 ||
            stageRng.randSeedUint32() != expectedStageRng.randSeedUint32()) return 9562;
    }
    {
        PokemonBattleState aiUser = state, aiTarget = state;
        aiUser.moveCount = 1;
        aiUser.moves[0] = {94, 10, 10};
        for (auto& stage : aiTarget.statStages) stage = 0;
        double statBenefit = 123;
        // Pinned SPDEF getter specifically requires a PHYSICAL attack in the
        // user's moveset, even though Psychic itself is SPECIAL.
        if (!Pokerogue3DS::calculateCanonicalDamageStatStageAiBenefit(aiUser, aiTarget, 94, statBenefit) ||
            statBenefit != 0) return 9570;
        aiUser.moveCount = 2;
        aiUser.moves[1] = {33, 35, 35};
        if (!Pokerogue3DS::calculateCanonicalDamageStatStageAiBenefit(aiUser, aiTarget, 94, statBenefit) ||
            statBenefit != 2) return 9571;
        double combinedScore = 0;
        if (!Pokerogue3DS::calculatePlainAttackAiScore(2, 100, 100, 90, 100, true, combinedScore, statBenefit) ||
            combinedScore != 66) return 9572;
        aiTarget.statStages[4] = -6;
        if (!Pokerogue3DS::calculateCanonicalDamageStatStageAiBenefit(aiUser, aiTarget, 94, statBenefit) ||
            statBenefit != -2) return 9573; // Pinned levels==0 keeps the +2 term.
        statBenefit = 123;
        if (Pokerogue3DS::calculateCanonicalDamageStatStageAiBenefit(aiUser, aiTarget, 95, statBenefit) ||
            statBenefit != 123) return 9574;
        aiUser.moves[1].moveId = 65535;
        if (Pokerogue3DS::calculateCanonicalDamageStatStageAiBenefit(aiUser, aiTarget, 94, statBenefit) ||
            statBenefit != 123) return 9575;
    }
    {
        double composed = 0;
        // Ember's 10% status benefit is one, before effectiveness and STAB.
        if (!Pokerogue3DS::calculatePlainAttackAiScore(2, 100, 100, 40, 100, true, composed, 1) ||
            composed != 33) return 9580;
        if (!Pokerogue3DS::calculatePlainAttackAiScore(0, 100, 100, 40, 100, true, composed, 1) ||
            composed != -20) return 9581; // Secondary benefit cannot revive type immunity.
        if (!Pokerogue3DS::calculatePlainAttackAiScore(0.5, 100, 100, 40, 100, false, composed, 1) ||
            composed != 4.25) return 9582;
    }
    {
        if (!Pokerogue3DS::pokemonStatStageMoveBuildersResolved(97) ||
            !Pokerogue3DS::pokemonStatStageMoveBuildersResolved(45) ||
            Pokerogue3DS::pokemonStatStageMoveBuildersResolved(14) ||
            Pokerogue3DS::pokemonStatStageMoveBuildersResolved(702)) return 9610;
        auto unresolvedUser = state, unresolvedTarget = state;
        unresolvedUser.moveCount = 1;
        unresolvedUser.moves[0] = {14, 20, 20};
        Pokerogue3DS::PokemonStatStageMovePolicy unresolvedPolicy{};
        unresolvedPolicy.hitPolicyResolved = unresolvedPolicy.stagePolicy.resolved = true;
        Pokerogue3DS::PokemonStatStageMoveEvent unresolvedEvent{};
        unresolvedEvent.accuracyRoll = 123;
        auto unresolvedRng = replacementRng, expectedUnresolvedRng = unresolvedRng;
        if (Pokerogue3DS::usePokemonStatStageStatusMove(unresolvedUser, unresolvedTarget, 0,
                unresolvedPolicy, unresolvedRng, unresolvedEvent) !=
                Pokerogue3DS::PokemonStatStageEffectResult::InvalidDefinition ||
            unresolvedUser.moves[0].pp != 20 || unresolvedUser.statStages[0] != state.statStages[0] ||
            unresolvedEvent.accuracyRoll != 123 ||
            unresolvedRng.randSeedUint32() != expectedUnresolvedRng.randSeedUint32()) return 9611;
    }
    {
        auto selfUser = state, selfOpponent = state;
        selfUser.moveCount = 1;
        selfUser.moves[0] = {97, 30, 30};
        selfUser.statStages[4] = 5;
        selfOpponent.hp = 0; // USER must not need a living opponent.
        Pokerogue3DS::PokemonStatStageMovePolicy selfPolicy{};
        selfPolicy.hitPolicyResolved = selfPolicy.stagePolicy.resolved = true;
        selfPolicy.blockedBeforeAccuracy = selfPolicy.typeImmune = true;
        Pokerogue3DS::PokemonStatStageMoveEvent selfEvent{};
        auto selfRng = replacementRng, expectedSelfRng = selfRng;
        if (Pokerogue3DS::usePokemonStatStageStatusMove(selfUser, selfOpponent, 0, selfPolicy,
                selfRng, selfEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
            !selfEvent.hit || selfEvent.typeImmune || selfEvent.accuracyRolled || selfUser.statStages[4] != 6 ||
            selfEvent.stages.changes[4] != 1 || selfUser.moves[0].pp != 29 || selfOpponent.hp ||
            selfRng.randSeedUint32() != expectedSelfRng.randSeedUint32()) return 9620;
        selfUser.moves[0].pp = 31;
        selfEvent.accuracyRoll = 123;
        if (Pokerogue3DS::usePokemonStatStageStatusMove(selfUser, selfOpponent, 0, selfPolicy,
                selfRng, selfEvent) != Pokerogue3DS::PokemonStatStageEffectResult::InvalidState ||
            selfUser.moves[0].pp != 31 || selfUser.statStages[4] != 6 || selfEvent.accuracyRoll != 123)
            return 9621;
    }
    {
        for (const uint16_t ability : {uint16_t(86), uint16_t(126)}) {
            bool resolved = false;
            for (const auto& row : PokerogueContent::kStatusActionAbilityProfiles)
                if (row.abilityId == ability) resolved = row.resolved;
            if (!resolved) return 9630;
            const auto* stages = PokerogueContent::findAbilityStatStageProfile(ability);
            if (!stages || stages->multiplier != (ability == 86 ? 2 : -1)) return 9631;
            auto multiplierUser = state, multiplierTarget = state;
            multiplierUser.moveCount = 1;
            multiplierUser.moves[0] = {97, 30, 30};
            multiplierUser.statStages[4] = 0;
            Pokerogue3DS::PokemonStatStageMovePolicy multiplierPolicy{};
            multiplierPolicy.hitPolicyResolved = multiplierPolicy.stagePolicy.resolved = true;
            const Pokerogue3DS::ResolvedStatStageAbilityComponent component[] = {{stages, true}};
            const PokerogueContent::MoveStatStageEffect agility{97, 16, 2, true};
            if (!Pokerogue3DS::composePokemonStatStageAbilityPolicy(agility, component, 1, false,
                    multiplierPolicy.stagePolicy, true)) return 9632;
            Pokerogue3DS::PokemonStatStageMoveEvent multiplierEvent{};
            auto multiplierRng = replacementRng;
            if (Pokerogue3DS::usePokemonStatStageStatusMove(multiplierUser, multiplierTarget, 0,
                    multiplierPolicy, multiplierRng, multiplierEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
                multiplierUser.statStages[4] != (ability == 86 ? 4 : -2) ||
                multiplierUser.moves[0].pp != 29) return 9633;
        }
    }
    {
        for (const uint16_t ability : {uint16_t(240), uint16_t(290)}) {
            bool resolved = false;
            for (const auto& profile : PokerogueContent::kStatusActionAbilityProfiles)
                if (profile.abilityId == ability) resolved = profile.resolved;
            const auto* stages = PokerogueContent::findAbilityStatStageProfile(ability);
            if (!resolved || !stages || stages->reflectDrops != (ability == 240) ||
                stages->copiesRaises != (ability == 290)) return 9640;
        }
    }
    {
        for (const uint16_t ability : {uint16_t(128), uint16_t(172)}) {
            bool resolved = false;
            for (const auto& row : PokerogueContent::kStatusActionAbilityProfiles)
                if (row.abilityId == ability) resolved = row.resolved;
            const PokerogueContent::AbilityStatStageReaction* reaction = nullptr;
            for (const auto& row : PokerogueContent::kAbilityStatStageReactions)
                if (row.abilityId == ability) reaction = &row;
            if (!resolved || !reaction || reaction->stagesPerRequestedStat != 2 ||
                reaction->stat != (ability == 128 ? 1 : 3)) return 9650;
            auto reactionUser = state, reactionTarget = state;
            for (auto& stage : reactionTarget.statStages) stage = 0;
            reactionUser.moveCount = 1;
            reactionUser.moves[0] = {45, 40, 40};
            Pokerogue3DS::PokemonStatStageCommandPolicy reactionPolicy{};
            reactionPolicy.move.hitPolicyResolved = reactionPolicy.move.stagePolicy.resolved = true;
            reactionPolicy.postChangePoliciesResolved = reactionPolicy.recipientReaction.resolved = true;
            reactionPolicy.recipientReactions[0] = reaction;
            Pokerogue3DS::PokemonStatStageCommandEvent reactionEvent{};
            auto reactionRng = replacementRng;
            if (Pokerogue3DS::usePokemonStatStageStatusCommand(reactionUser, reactionTarget, 0,
                    reactionPolicy, reactionRng, reactionEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
                !reactionEvent.move.hit || reactionUser.moves[0].pp != 39 ||
                reactionTarget.statStages[0] != (ability == 128 ? 1 : -1) ||
                (ability == 172 && reactionTarget.statStages[2] != 2)) return 9651;
        }
    }
    {
        for (const uint16_t ability : {uint16_t(86), uint16_t(126), uint16_t(128), uint16_t(172),
                uint16_t(240), uint16_t(290)}) {
            bool residualResolved = false;
            for (const auto& row : PokerogueContent::kStatusResidualAbilityProfiles)
                if (row.abilityId == ability) {
                    residualResolved = row.resolved && !row.blockNonDirectDamage && !row.blockedStatusMask &&
                        !row.healedStatusMask && row.burnNumerator == row.burnDenominator;
                }
            if (!residualResolved) return 9670;
        }
        bool unresolvedResidual = false;
        for (const auto& row : PokerogueContent::kStatusResidualAbilityProfiles)
            if (row.abilityId == 125) unresolvedResidual = !row.resolved; // Sheer Force power callback pending.
        if (!unresolvedResidual) return 9671;
    }
    {
        auto cappedUser = state, opportunistTarget = state;
        cappedUser.moveCount = 1;
        cappedUser.moves[0] = {97, 30, 30};
        cappedUser.statStages[4] = 6;
        for (auto& stage : opportunistTarget.statStages) stage = 0;
        const auto* opportunist = PokerogueContent::findAbilityStatStageProfile(290);
        if (!opportunist || !opportunist->copiesRaises) return 9680;
        Pokerogue3DS::PokemonStatStageCommandPolicy copyCommand{};
        copyCommand.move.hitPolicyResolved = copyCommand.move.stagePolicy.resolved = true;
        copyCommand.postChangePoliciesResolved = true;
        copyCommand.opponentCopyProfile = opportunist;
        Pokerogue3DS::PokemonStatStageCommandEvent copyCommandEvent{};
        copyCommandEvent.move.accuracyRoll = 123;
        auto copyCommandRng = replacementRng, expectedCopyCommandRng = copyCommandRng;
        // Required copy callback unresolved: rollback even though USER already
        // tentatively consumed PP and emitted a clamped original phase.
        if (Pokerogue3DS::usePokemonStatStageStatusCommand(cappedUser, opportunistTarget, 0,
                copyCommand, copyCommandRng, copyCommandEvent) !=
                Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
            cappedUser.moves[0].pp != 30 || cappedUser.statStages[4] != 6 ||
            opportunistTarget.statStages[4] || copyCommandEvent.move.accuracyRoll != 123 ||
            copyCommandRng.randSeedUint32() != expectedCopyCommandRng.randSeedUint32()) return 9681;
        copyCommand.opponentCopy.resolved = true;
        copyCommandRng = expectedCopyCommandRng;
        if (Pokerogue3DS::usePokemonStatStageStatusCommand(cappedUser, opportunistTarget, 0,
                copyCommand, copyCommandRng, copyCommandEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
            cappedUser.statStages[4] != 6 || copyCommandEvent.move.stages.changedStatMask ||
            copyCommandEvent.move.stages.requestedStages != 2 || cappedUser.moves[0].pp != 29 ||
            opportunistTarget.statStages[4] != 2 || copyCommandEvent.opponentCopy.changes[4] != 2 ||
            copyCommandRng.randSeedUint32() != expectedCopyCommandRng.randSeedUint32()) return 9682;
    }
    const uint8_t mixedSlots[] = {0, 1};
    const uint32_t mixedDamage[] = {0, 10};
    uint8_t mixedFiltered[4]{}, mixedCount = 0;
    if (!Pokerogue3DS::filterEnemyKoMoveSlots(mixedSlots, mixedDamage, 2, 10,
            mixedFiltered, mixedCount) || mixedCount != 1 || mixedFiltered[0] != 1) return 237;
    if (!Pokerogue3DS::filterEnemyKoMoveSlots(mixedSlots, mixedDamage, 2, 11,
            mixedFiltered, mixedCount) || mixedCount != 2 || mixedFiltered[0] != 0) return 238;
    const PokerogueContent::AbilityStatStageProfile* mirrorArmor = nullptr;
    for (const auto& profile : PokerogueContent::kAbilityStatStageProfiles)
        if (std::strcmp(profile.sourceSymbol, "AbilityId.MIRROR_ARMOR") == 0) mirrorArmor = &profile;
    if (!mirrorArmor || !mirrorArmor->reflectDrops) return 239;
    const Pokerogue3DS::ResolvedStatStageAbilityComponent mirrorComponent[] = {{mirrorArmor, true}};
    protectedPolicy = {};
    if (!Pokerogue3DS::composePokemonStatStageAbilityPolicy(growlEffect, mirrorComponent,
            1, false, protectedPolicy, true) || protectedPolicy.reflectedStatMask != 1 ||
        protectedPolicy.cancelledStatMask != 1) return 240;
    protectedPolicy.resolved = true;
    effectRecipient = state;
    if (Pokerogue3DS::applyPokemonStatStageEffect(effectRecipient, growlEffect, protectedPolicy,
            replacementRng, stageEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        effectRecipient.statStages[0] || stageEvent.reflectedStatMask != 1 ||
        stageEvent.reflectedStages != -1) return 241;
    PokemonBattleState reflectedSource = state;
    Pokerogue3DS::PokemonStatStageEffectPolicy reflectedPolicy{};
    Pokerogue3DS::PokemonStatStageEffectEvent reflectedEvent{};
    reflectedPolicy.resolved = true;
    reflectedPolicy.stageMultiplier = 2;
    if (Pokerogue3DS::applyReflectedPokemonStatStages(reflectedSource, stageEvent,
            reflectedPolicy, reflectedEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        reflectedSource.statStages[0] != -2 || reflectedEvent.changes[0] != -2 ||
        reflectedEvent.reflectedStatMask) return 242;
    reflectedPolicy.reflectedStatMask = 1;
    if (Pokerogue3DS::applyReflectedPokemonStatStages(reflectedSource, stageEvent,
            reflectedPolicy, reflectedEvent) != Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
        reflectedSource.statStages[0] != -2) return 243;
    reflectedPolicy.reflectedStatMask = 0;
    reflectedPolicy.stageMultiplier = -1;
    if (Pokerogue3DS::applyReflectedPokemonStatStages(reflectedSource, stageEvent,
            reflectedPolicy, reflectedEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        reflectedSource.statStages[0] != -1 || reflectedEvent.changes[0] != 1) return 244;
    const PokerogueContent::AbilityStatStageReaction* defiantReaction = nullptr;
    for (const auto& reaction : PokerogueContent::kAbilityStatStageReactions)
        if (std::strcmp(reaction.sourceSymbol, "AbilityId.DEFIANT") == 0) defiantReaction = &reaction;
    if (!defiantReaction || defiantReaction->stat != 1) return 245;
    Pokerogue3DS::PokemonStatStageEffectEvent requestedDrop{};
    requestedDrop.triggered = true;
    requestedDrop.processedStatMask = 3;
    requestedDrop.requestedStages = -1;
    Pokerogue3DS::PokemonStatStageReactionRequest reactionRequest{};
    if (!Pokerogue3DS::planPokemonStatStageDropReaction(*defiantReaction, requestedDrop,
            false, reactionRequest) || reactionRequest.stat != 1 || reactionRequest.stages != 4)
        return 246; // Source uses requested changes, even if applied changes are capped.
    if (!Pokerogue3DS::planPokemonStatStageDropReaction(*defiantReaction, requestedDrop,
            true, reactionRequest) || reactionRequest.stages) return 247;
    requestedDrop.processedStatMask = 0;
    if (!Pokerogue3DS::planPokemonStatStageDropReaction(*defiantReaction, requestedDrop,
            false, reactionRequest) || reactionRequest.stages) return 248;
    PokemonBattleState reactionActor = state;
    reactionActor.statStages[0] = -6;
    requestedDrop.processedStatMask = 3;
    Pokerogue3DS::PokemonStatStageEffectPolicy boostPolicy{};
    boostPolicy.resolved = true;
    if (Pokerogue3DS::applyPokemonStatStageDropReaction(reactionActor, *defiantReaction,
            requestedDrop, false, boostPolicy, reflectedEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        reactionActor.statStages[0] != -2 || reflectedEvent.changes[0] != 4) return 249;
    boostPolicy.stageMultiplier = 2;
    if (Pokerogue3DS::applyPokemonStatStageDropReaction(reactionActor, *defiantReaction,
            requestedDrop, false, boostPolicy, reflectedEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        reactionActor.statStages[0] != 6 || reflectedEvent.changes[0] != 8) return 250;
    if (Pokerogue3DS::applyPokemonStatStageDropReaction(reactionActor, *defiantReaction,
            requestedDrop, true, boostPolicy, reflectedEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || reflectedEvent.triggered) return 251;
    growlUser = state;
    growlTarget = state;
    growlUser.moves[0].moveId = 45;
    growlUser.moves[0].pp = 40;
    Pokerogue3DS::PokemonStatStageCommandPolicy commandPolicy{};
    commandPolicy.postChangePoliciesResolved = true;
    commandPolicy.move.hitPolicyResolved = true;
    commandPolicy.move.stagePolicy.resolved = true;
    commandPolicy.recipientReactions[0] = defiantReaction;
    Pokerogue3DS::PokemonStatStageCommandEvent commandEvent{};
    const auto beforeCommandRng = replacementRng.state();
    if (Pokerogue3DS::usePokemonStatStageStatusCommand(growlUser, growlTarget, 0,
            commandPolicy, replacementRng, commandEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::UnresolvedPolicy ||
        growlUser.moves[0].pp != 40 || growlTarget.statStages[0] ||
        replacementRng.state().s0 != beforeCommandRng.s0 ||
        replacementRng.state().s1 != beforeCommandRng.s1 ||
        replacementRng.state().s2 != beforeCommandRng.s2) return 252;
    commandPolicy.recipientReaction.resolved = true;
    if (Pokerogue3DS::usePokemonStatStageStatusCommand(growlUser, growlTarget, 0,
            commandPolicy, replacementRng, commandEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        growlUser.moves[0].pp != 39 || growlTarget.statStages[0] != 1 ||
        commandEvent.move.stages.changes[0] != -1 ||
        commandEvent.recipientReactions[0].changes[0] != 2) return 253;
    auto noCostStagePolicy = commandPolicy;
    noCostStagePolicy.move.ppCost = 0;
    PokemonBattleState noCostStageUser = growlUser, noCostStageTarget = growlTarget;
    noCostStageUser.moves[0].pp = 0;
    if (Pokerogue3DS::usePokemonStatStageStatusCommand(noCostStageUser, noCostStageTarget, 0,
            noCostStagePolicy, replacementRng, commandEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || noCostStageUser.moves[0].pp) return 395;
    const PokerogueContent::AbilityStatStageProfile* opportunist = nullptr;
    for (const auto& profile : PokerogueContent::kAbilityStatStageProfiles)
        if (std::strcmp(profile.sourceSymbol, "AbilityId.OPPORTUNIST") == 0) opportunist = &profile;
    if (!opportunist || !opportunist->copiesRaises) return 254;
    PokemonBattleState copyingActor = state;
    PokemonStatStageEffectEvent requestedRaise{};
    requestedRaise.triggered = true;
    requestedRaise.processedStatMask = 1;
    requestedRaise.requestedStages = 2;
    boostPolicy.stageMultiplier = 1;
    if (Pokerogue3DS::applyPokemonCopiedStatStageRaise(copyingActor, requestedRaise,
            false, boostPolicy, reflectedEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        copyingActor.statStages[0] != 2) return 255;
    if (Pokerogue3DS::applyPokemonCopiedStatStageRaise(copyingActor, requestedRaise,
            true, boostPolicy, reflectedEvent) != Pokerogue3DS::PokemonStatStageEffectResult::Ok ||
        copyingActor.statStages[0] != 2 || reflectedEvent.triggered) return 256;
    PokemonBattleState herbHolder = state;
    herbHolder.statStages[0] = -3;
    herbHolder.statStages[1] = 2;
    uint8_t herbStack = 2;
    Pokerogue3DS::PokemonNegativeStageResetItemEvent herbEvent{};
    if (Pokerogue3DS::applyPokemonNegativeStageResetItem(herbHolder, "WHITE_HERB",
            herbHolder.pokemonId, herbStack, true, herbEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || herbEvent.consumed ||
        herbHolder.statStages[0] != -3 || herbStack != 2) return 257;
    if (Pokerogue3DS::applyPokemonNegativeStageResetItem(herbHolder, "WHITE_HERB",
            herbHolder.pokemonId, herbStack, false, herbEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || !herbEvent.consumed ||
        herbEvent.changes[0] != 3 || herbHolder.statStages[0] ||
        herbHolder.statStages[1] != 2 || herbStack != 1) return 258;
    if (Pokerogue3DS::applyPokemonNegativeStageResetItem(herbHolder, "WHITE_HERB",
            herbHolder.pokemonId, herbStack, false, herbEvent) !=
            Pokerogue3DS::PokemonStatStageEffectResult::Ok || herbEvent.consumed || herbStack != 1)
        return 259;
    PokemonBattleState lowHpAttacker = state;
    lowHpAttacker.abilityId = 65; // Imported Overgrow.
    lowHpAttacker.maxHp = 100;
    lowHpAttacker.hp = 34;
    double aboveThresholdDamage = 0, lowHpDamage = 0;
    if (Pokerogue3DS::calculatePokemonBaseDamage(lowHpAttacker, state, 22,
            aboveThresholdDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok) return 260;
    lowHpAttacker.hp = 33;
    if (Pokerogue3DS::calculatePokemonBaseDamage(lowHpAttacker, state, 22,
            lowHpDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        lowHpDamage != (aboveThresholdDamage - 2.0) * 1.5 + 2.0) return 261;
    lowHpAttacker.maxHp = 3;
    lowHpAttacker.hp = 1;
    if (Pokerogue3DS::calculatePokemonBaseDamage(lowHpAttacker, state, 22,
            lowHpDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        lowHpDamage != aboveThresholdDamage) return 262;
    const PokerogueContent::TypePowerAbility* steelworker = nullptr;
    const PokerogueContent::TypePowerAbility* sandForce = nullptr;
    for (const auto& profile : PokerogueContent::kTypePowerAbilities) {
        if (std::strcmp(profile.sourceSymbol, "AbilityId.STEELWORKER") == 0) steelworker = &profile;
        if (std::strcmp(profile.sourceSymbol, "AbilityId.SAND_FORCE") == 0 &&
            std::strcmp(profile.type, "STEEL") == 0) sandForce = &profile;
    }
    if (!steelworker || steelworker->multiplier != 1.5 || steelworker->requiresCondition ||
        !sandForce || !sandForce->requiresCondition) return 263;
    PokemonBattleState typePowerActor = state;
    typePowerActor.abilityId = 0;
    double neutralSteelDamage = 0, boostedSteelDamage = 0;
    if (Pokerogue3DS::calculatePokemonBaseDamage(typePowerActor, state, 232,
            neutralSteelDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok) return 264;
    typePowerActor.abilityId = steelworker->abilityId;
    if (Pokerogue3DS::calculatePokemonBaseDamage(typePowerActor, state, 232,
            boostedSteelDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        boostedSteelDamage != (neutralSteelDamage - 2.0) * 1.5 + 2.0) return 265;
    typePowerActor.abilityId = sandForce->abilityId;
    if (Pokerogue3DS::calculatePokemonBaseDamage(typePowerActor, state, 232,
            boostedSteelDamage) != Pokerogue3DS::PokemonBaseDamageResult::UnsupportedAbilityCondition) return 266;
    Pokerogue3DS::PokemonMoveWeatherContext sandContext{};
    sandContext.resolved = true;
    sandContext.effectiveWeather = Pokerogue3DS::PokemonEffectiveWeather::Sunny;
    sandContext.cancellationWeather = Pokerogue3DS::PokemonEffectiveWeather::Sandstorm;
    if (std::strcmp(sandForce->conditionWeatherSymbol, "SANDSTORM") != 0 ||
        Pokerogue3DS::calculatePokemonBaseDamage(typePowerActor, state, 232,
            boostedSteelDamage, false, &sandContext) != Pokerogue3DS::PokemonBaseDamageResult::Ok) return 315;
    const double expectedSandBoost = (neutralSteelDamage - 2.0) * 1.3 + 2.0;
    const double sandError = boostedSteelDamage - expectedSandBoost;
    if (sandError < -0.00000001 || sandError > 0.00000001) return 316;
    sandContext.effectiveWeather = Pokerogue3DS::PokemonEffectiveWeather::Sandstorm;
    sandContext.cancellationWeather = Pokerogue3DS::PokemonEffectiveWeather::None;
    if (Pokerogue3DS::calculatePokemonBaseDamage(typePowerActor, state, 232,
            boostedSteelDamage, false, &sandContext) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        boostedSteelDamage != neutralSteelDamage) return 317;
    PokemonBattleState rockDefender = state;
    rockDefender.speciesDex = 74; // Canonical Geodude Rock/Ground.
    rockDefender.formId = nullptr;
    rockDefender.stats[4] = 53;
    rockDefender.statStages[3] = 1;
    PokemonBattleState weatherAttack = state;
    weatherAttack.abilityId = 0;
    double ordinaryRockDamage = 0, sandRockDamage = 0;
    if (Pokerogue3DS::calculatePokemonBaseDamage(weatherAttack, rockDefender, 55,
            ordinaryRockDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok) return 318;
    sandContext.effectiveWeather = Pokerogue3DS::PokemonEffectiveWeather::Sandstorm;
    if (Pokerogue3DS::calculatePokemonBaseDamage(weatherAttack, rockDefender, 55,
            sandRockDamage, false, &sandContext) != Pokerogue3DS::PokemonBaseDamageResult::Ok) return 319;
    const double expectedRockDamage = (ordinaryRockDamage - 2.0) * 79.0 / 119.0 + 2.0;
    const double rockError = sandRockDamage - expectedRockDamage;
    if (rockError < -0.00000001 || rockError > 0.00000001) return 320;
    PokemonBattleState iceDefender = rockDefender;
    iceDefender.speciesDex = 144; // Canonical Articuno Ice/Flying.
    iceDefender.stats[2] = 53;
    iceDefender.statStages[1] = 1;
    double ordinaryIceDamage = 0, snowIceDamage = 0;
    if (Pokerogue3DS::calculatePokemonBaseDamage(weatherAttack, iceDefender, 33,
            ordinaryIceDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok) return 321;
    sandContext.effectiveWeather = Pokerogue3DS::PokemonEffectiveWeather::Snow;
    if (Pokerogue3DS::calculatePokemonBaseDamage(weatherAttack, iceDefender, 33,
            snowIceDamage, false, &sandContext) != Pokerogue3DS::PokemonBaseDamageResult::Ok) return 322;
    const double iceError = snowIceDamage - ((ordinaryIceDamage - 2.0) * 79.0 / 119.0 + 2.0);
    if (iceError < -0.00000001 || iceError > 0.00000001) return 323;
    uint32_t unresolvedDamage = 777;
    if (Pokerogue3DS::calculatePokemonDamageCore(typePowerActor, state, 232, false,
            unresolvedDamage) != Pokerogue3DS::PokemonDamageCoreResult::UnsupportedAbilityCondition ||
        unresolvedDamage != 777) return 267;
    bool plainsWeatherFound = false, forestRainFound = false, forestNoneFound = false;
    for (const auto& pool : PokerogueContent::kBiomeWeatherPools) {
        if (!pool.sourcePath || !*pool.sourcePath || !pool.sourceHash || !*pool.sourceHash) return 291;
        if (std::strcmp(pool.biomeId, "plains") == 0 && std::strcmp(pool.weatherSymbol, "NONE") == 0)
            plainsWeatherFound = pool.weight == 1;
        if (std::strcmp(pool.biomeId, "forest") == 0 && std::strcmp(pool.weatherSymbol, "RAIN") == 0)
            forestRainFound = pool.weight == 4;
        if (std::strcmp(pool.biomeId, "forest") == 0 && std::strcmp(pool.weatherSymbol, "NONE") == 0)
            forestNoneFound = pool.weight == 8;
    }
    if (!plainsWeatherFound || !forestRainFound || !forestNoneFound) return 292;
    PokerogueRngAdapter biomeWeatherRng;
    const uint16_t weatherSeed[] = {'w', 'e', 'a', 't', 'h', 'e', 'r'};
    biomeWeatherRng.sow(weatherSeed, 7);
    const auto beforePlainsWeather = biomeWeatherRng.state();
    Pokerogue3DS::PokemonEffectiveWeather selectedWeather = Pokerogue3DS::PokemonEffectiveWeather::Rain;
    if (!Pokerogue3DS::selectPokemonBiomeWeather("plains", false, biomeWeatherRng, selectedWeather) ||
        selectedWeather != Pokerogue3DS::PokemonEffectiveWeather::None) return 293;
    const auto afterPlainsWeather = biomeWeatherRng.state();
    if (beforePlainsWeather.carry != afterPlainsWeather.carry || beforePlainsWeather.s0 != afterPlainsWeather.s0 ||
        beforePlainsWeather.s1 != afterPlainsWeather.s1 || beforePlainsWeather.s2 != afterPlainsWeather.s2) return 294;
    PokerogueRngAdapter forestExpectedRng = biomeWeatherRng;
    const int32_t forestRoll = forestExpectedRng.randSeedInt(13); // NONE8, RAIN4, night FOG1.
    const auto forestExpected = forestRoll < 8 ? Pokerogue3DS::PokemonEffectiveWeather::None :
        forestRoll < 12 ? Pokerogue3DS::PokemonEffectiveWeather::Rain : Pokerogue3DS::PokemonEffectiveWeather::Fog;
    if (!Pokerogue3DS::selectPokemonBiomeWeather("forest", true, biomeWeatherRng, selectedWeather) ||
        selectedWeather != forestExpected) return 295;
    uint16_t sunOnlyEvent[10]{};
    sunOnlyEvent[1] = 5;
    if (!Pokerogue3DS::selectPokemonBiomeWeather("plains", true, biomeWeatherRng, selectedWeather, sunOnlyEvent) ||
        selectedWeather != Pokerogue3DS::PokemonEffectiveWeather::None) return 296;
    if (Pokerogue3DS::selectPokemonBiomeWeather("missing-biome", false, biomeWeatherRng, selectedWeather)) return 297;
    uint16_t cloudNineId = 0, megaSolId = 0;
    for (const auto& profile : PokerogueContent::kWeatherAbilityProfiles) {
        if (std::strcmp(profile.sourceSymbol, "AbilityId.CLOUD_NINE") == 0 &&
            profile.suppressesWeather && profile.affectsImmutable) cloudNineId = profile.abilityId;
        if (std::strcmp(profile.sourceSymbol, "AbilityId.MEGA_SOL") == 0 &&
            std::strcmp(profile.overrideWeatherSymbol, "SUNNY") == 0) megaSolId = profile.abilityId;
    }
    if (!cloudNineId || !megaSolId) return 302;
    Pokerogue3DS::PokemonWeatherAbilityComponent weatherComponents[2] = {
        {cloudNineId, true, false}, {megaSolId, true, true}
    };
    Pokerogue3DS::PokemonWeatherResolutionPolicy composedWeatherPolicy{};
    if (!Pokerogue3DS::composePokemonWeatherResolutionPolicy(weatherComponents, 2, composedWeatherPolicy) ||
        !composedWeatherPolicy.resolved || !composedWeatherPolicy.suppressesImmutableWeather ||
        composedWeatherPolicy.attackerOverride != Pokerogue3DS::PokemonEffectiveWeather::Sunny) return 303;
    weatherComponents[0].applies = false;
    weatherComponents[1].belongsToAttacker = false;
    if (!Pokerogue3DS::composePokemonWeatherResolutionPolicy(weatherComponents, 2, composedWeatherPolicy) ||
        composedWeatherPolicy.suppressesOrdinaryWeather ||
        composedWeatherPolicy.attackerOverride != Pokerogue3DS::PokemonEffectiveWeather::None) return 304;
    Pokerogue3DS::PokemonArenaWeatherState arenaWeather{};
    using Pokerogue3DS::PokemonEffectiveWeather;
    if (!Pokerogue3DS::setPokemonArenaWeather(arenaWeather, PokemonEffectiveWeather::Rain, 2) ||
        arenaWeather.turnsLeft != 2 || arenaWeather.maxDuration != 2) return 280;
    if (!Pokerogue3DS::lapsePokemonArenaWeather(arenaWeather) || arenaWeather.turnsLeft != 1 ||
        Pokerogue3DS::lapsePokemonArenaWeather(arenaWeather) || arenaWeather.turnsLeft != 0) return 281;
    if (!Pokerogue3DS::setPokemonArenaWeather(arenaWeather, PokemonEffectiveWeather::HeavyRain, 5) ||
        arenaWeather.turnsLeft != 0 || arenaWeather.maxDuration != 0 ||
        !Pokerogue3DS::lapsePokemonArenaWeather(arenaWeather)) return 282;
    if (Pokerogue3DS::setPokemonArenaWeather(arenaWeather, PokemonEffectiveWeather::Sunny, 5) ||
        arenaWeather.type != PokemonEffectiveWeather::HeavyRain) return 283;
    Pokerogue3DS::PokemonWeatherResolutionPolicy weatherPolicy{};
    weatherPolicy.resolved = true;
    weatherPolicy.suppressesOrdinaryWeather = true;
    Pokerogue3DS::PokemonMoveWeatherContext resolvedWeather{};
    if (!Pokerogue3DS::resolvePokemonMoveWeatherContext(arenaWeather, weatherPolicy, resolvedWeather) ||
        resolvedWeather.effectiveWeather != PokemonEffectiveWeather::HeavyRain) return 284;
    weatherPolicy.suppressesImmutableWeather = true;
    if (!Pokerogue3DS::resolvePokemonMoveWeatherContext(arenaWeather, weatherPolicy, resolvedWeather) ||
        resolvedWeather.effectiveWeather != PokemonEffectiveWeather::None) return 285;
    weatherPolicy.attackerOverride = PokemonEffectiveWeather::Sunny;
    if (!Pokerogue3DS::resolvePokemonMoveWeatherContext(arenaWeather, weatherPolicy, resolvedWeather) ||
        resolvedWeather.effectiveWeather != PokemonEffectiveWeather::Sunny) return 286;
    if (!Pokerogue3DS::setPokemonArenaWeather(arenaWeather, PokemonEffectiveWeather::None, 5) ||
        arenaWeather.turnsLeft != 0) return 287;
    Pokerogue3DS::PokemonWeatherTurnEndEvent weatherEnd{};
    if (!Pokerogue3DS::setPokemonArenaWeather(arenaWeather, PokemonEffectiveWeather::Rain, 1) ||
        !Pokerogue3DS::advancePokemonArenaWeatherTurnEnd(arenaWeather, weatherEnd) ||
        !weatherEnd.expired || weatherEnd.previousWeather != PokemonEffectiveWeather::Rain ||
        !weatherEnd.requestWeatherFormReversion || arenaWeather.type != PokemonEffectiveWeather::None ||
        arenaWeather.turnsLeft || arenaWeather.maxDuration) return 298;
    if (!Pokerogue3DS::advancePokemonArenaWeatherTurnEnd(arenaWeather, weatherEnd) ||
        weatherEnd.expired || weatherEnd.requestWeatherFormReversion) return 299;
    if (!Pokerogue3DS::setPokemonArenaWeather(arenaWeather, PokemonEffectiveWeather::Rain, 0) ||
        !Pokerogue3DS::advancePokemonArenaWeatherTurnEnd(arenaWeather, weatherEnd) ||
        weatherEnd.expired || arenaWeather.type != PokemonEffectiveWeather::Rain) return 300;
    arenaWeather.turnsLeft = 2; // Invalid duration must not mutate state or event.
    weatherEnd.expired = true;
    if (Pokerogue3DS::advancePokemonArenaWeatherTurnEnd(arenaWeather, weatherEnd) ||
        arenaWeather.turnsLeft != 2 || !weatherEnd.expired) return 301;
    uint16_t magicGuardId = 0, sandVeilId = 0;
    for (const auto& profile : PokerogueContent::kWeatherDamageAbilityProfiles) {
        if (std::strcmp(profile.sourceSymbol, "AbilityId.MAGIC_GUARD") == 0 &&
            profile.blocksIndirectDamage) magicGuardId = profile.abilityId;
        if (std::strcmp(profile.sourceSymbol, "AbilityId.SAND_VEIL") == 0 &&
            profile.weatherMask == (1u << 3)) sandVeilId = profile.abilityId;
    }
    if (!magicGuardId || !sandVeilId) return 311;
    bool abilityWeatherBlocked = false;
    if (!Pokerogue3DS::pokemonAbilityBlocksWeatherDamage(magicGuardId, PokemonEffectiveWeather::Hail,
            abilityWeatherBlocked) || !abilityWeatherBlocked) return 312;
    if (!Pokerogue3DS::pokemonAbilityBlocksWeatherDamage(sandVeilId, PokemonEffectiveWeather::Sandstorm,
            abilityWeatherBlocked) || !abilityWeatherBlocked) return 313;
    if (!Pokerogue3DS::pokemonAbilityBlocksWeatherDamage(sandVeilId, PokemonEffectiveWeather::Hail,
            abilityWeatherBlocked) || abilityWeatherBlocked) return 314;
    PokemonBattleState weatherVictim = state;
    weatherVictim.maxHp = weatherVictim.hp = 100;
    Pokerogue3DS::PokemonArenaWeatherState damageWeather{};
    damageWeather.type = PokemonEffectiveWeather::Sandstorm;
    Pokerogue3DS::PokemonWeatherDamagePolicy residualPolicy{};
    residualPolicy.resolved = true;
    residualPolicy.type1 = "NORMAL";
    residualPolicy.type2 = "NONE";
    Pokerogue3DS::PokemonWeatherDamageEvent residualEvent{};
    if (!Pokerogue3DS::applyPokemonWeatherResidualDamage(weatherVictim, damageWeather, residualPolicy, residualEvent) ||
        residualEvent.damageApplied != 6 || weatherVictim.hp != 94 || residualEvent.fainted) return 305;
    residualPolicy.type2 = "STEEL";
    if (!Pokerogue3DS::applyPokemonWeatherResidualDamage(weatherVictim, damageWeather, residualPolicy, residualEvent) ||
        residualEvent.damageApplied || weatherVictim.hp != 94) return 306;
    damageWeather.type = PokemonEffectiveWeather::Hail;
    residualPolicy.type2 = "ICE";
    if (!Pokerogue3DS::applyPokemonWeatherResidualDamage(weatherVictim, damageWeather, residualPolicy, residualEvent) ||
        residualEvent.damageApplied) return 307;
    residualPolicy.type2 = "NONE";
    residualPolicy.underground = true;
    if (!Pokerogue3DS::applyPokemonWeatherResidualDamage(weatherVictim, damageWeather, residualPolicy, residualEvent) ||
        residualEvent.damageApplied) return 308;
    residualPolicy.underground = false;
    weatherVictim.maxHp = weatherVictim.hp = 1;
    if (!Pokerogue3DS::applyPokemonWeatherResidualDamage(weatherVictim, damageWeather, residualPolicy, residualEvent) ||
        residualEvent.damageApplied != 1 || !residualEvent.fainted || weatherVictim.hp) return 309;
    residualPolicy.resolved = false;
    residualEvent.damageApplied = 123;
    if (Pokerogue3DS::applyPokemonWeatherResidualDamage(weatherVictim, damageWeather, residualPolicy, residualEvent) ||
        residualEvent.damageApplied != 123) return 310;
    Pokerogue3DS::PokemonMoveWeatherContext weatherContext{};
    double weatherMultiplier = 17.0;
    if (Pokerogue3DS::pokemonMoveWeatherMultiplier(52, weatherContext, weatherMultiplier) ||
        weatherMultiplier != 17.0) return 268;
    weatherContext.resolved = true;
    weatherContext.effectiveWeather = Pokerogue3DS::PokemonEffectiveWeather::Sunny;
    if (!Pokerogue3DS::pokemonMoveWeatherMultiplier(52, weatherContext, weatherMultiplier) ||
        weatherMultiplier != 1.5) return 269;
    if (!Pokerogue3DS::pokemonMoveWeatherMultiplier(55, weatherContext, weatherMultiplier) ||
        weatherMultiplier != 0.5) return 270;
    weatherContext.effectiveWeather = Pokerogue3DS::PokemonEffectiveWeather::HeavyRain;
    if (!Pokerogue3DS::pokemonMoveWeatherMultiplier(55, weatherContext, weatherMultiplier) ||
        weatherMultiplier != 1.5) return 271;
    if (!Pokerogue3DS::pokemonMoveWeatherMultiplier(52, weatherContext, weatherMultiplier) ||
        weatherMultiplier != 0.5) return 272;
    if (!Pokerogue3DS::pokemonMoveWeatherMultiplier(33, weatherContext, weatherMultiplier) ||
        weatherMultiplier != 1.0) return 273;
    if (Pokerogue3DS::pokemonMoveWeatherMultiplier(311, weatherContext, weatherMultiplier)) return 274;
    weatherContext.resolved = false;
    unresolvedDamage = 777;
    if (Pokerogue3DS::calculatePokemonDamageCore(state, state, 33, false,
            unresolvedDamage, &weatherContext) != Pokerogue3DS::PokemonDamageCoreResult::UnresolvedWeather ||
        unresolvedDamage != 777) return 275;
    int16_t weatherAccuracy = 17;
    weatherContext.resolved = true;
    weatherContext.effectiveWeather = PokemonEffectiveWeather::Sunny;
    if (!Pokerogue3DS::pokemonWeatherMoveAccuracy(87, &weatherContext, weatherAccuracy) ||
        weatherAccuracy != 50) return 324;
    weatherContext.effectiveWeather = PokemonEffectiveWeather::Rain;
    if (!Pokerogue3DS::pokemonWeatherMoveAccuracy(87, &weatherContext, weatherAccuracy) ||
        weatherAccuracy != -1) return 325;
    weatherContext.effectiveWeather = PokemonEffectiveWeather::Snow;
    if (!Pokerogue3DS::pokemonWeatherMoveAccuracy(59, &weatherContext, weatherAccuracy) ||
        weatherAccuracy != -1) return 326;
    weatherContext.effectiveWeather = PokemonEffectiveWeather::None;
    if (!Pokerogue3DS::pokemonWeatherMoveAccuracy(59, &weatherContext, weatherAccuracy) ||
        weatherAccuracy != 70) return 327;
    weatherAccuracy = 17;
    if (Pokerogue3DS::pokemonWeatherMoveAccuracy(87, nullptr, weatherAccuracy) ||
        weatherAccuracy != 17) return 328;
    weatherContext.effectiveWeather = PokemonEffectiveWeather::Sunny;
    if (!Pokerogue3DS::pokemonMoveWeatherMultiplier(876, weatherContext, weatherMultiplier) ||
        weatherMultiplier != 1.5) return 329;
    weatherContext.effectiveWeather = PokemonEffectiveWeather::HarshSun;
    if (!Pokerogue3DS::pokemonMoveWeatherMultiplier(876, weatherContext, weatherMultiplier) ||
        weatherMultiplier != 0.5) return 330;
    weatherContext.effectiveWeather = PokemonEffectiveWeather::None;
    if (!Pokerogue3DS::pokemonMoveWeatherMultiplier(876, weatherContext, weatherMultiplier) ||
        weatherMultiplier != 1.0) return 331;
    uint8_t criticalDenominator = 0;
    if (!Pokerogue3DS::pokemonMoveCriticalDenominator(33, criticalDenominator) ||
        criticalDenominator != 24) return 332;
    if (!Pokerogue3DS::pokemonMoveCriticalDenominator(2, criticalDenominator) ||
        criticalDenominator != 8) return 333;
    if (!Pokerogue3DS::pokemonMoveCriticalDenominator(480, criticalDenominator) ||
        criticalDenominator != 1) return 334;
    double highCritAiScore = 0;
    if (!Pokerogue3DS::calculatePlainAttackAiScore(1.0, 100, 100, 40, 100, false,
            highCritAiScore, 3.0) || highCritAiScore != 11.0) return 335;
    PokemonBattleInit actorIdInput = input;
    actorIdInput.pokemonId = 0x12345678u;
    actorIdInput.deriveIvsFromPokemonId = true;
    PokemonBattleState actorIdState{};
    if (Pokerogue3DS::initializePokemonBattleState(actorIdInput, actorIdState) != PokemonBattleInitResult::Ok ||
        actorIdState.pokemonId != actorIdInput.pokemonId || !actorIdState.ivsWereDerivedFromPokemonId) return 61;
    PokemonBattleInit equivalentExplicitInput = input;
    Pokerogue3DS::derivePokemonIvsFromId(actorIdInput.pokemonId, equivalentExplicitInput.ivs);
    PokemonBattleState equivalentExplicitState{};
    if (Pokerogue3DS::initializePokemonBattleState(equivalentExplicitInput, equivalentExplicitState) !=
        PokemonBattleInitResult::Ok) return 62;
    for (uint8_t i = 0; i < 6; ++i) {
        if (actorIdState.ivs[i] != equivalentExplicitState.ivs[i] ||
            actorIdState.stats[i] != equivalentExplicitState.stats[i]) return 63;
    }

    PokemonBattleState damageTarget = state;
    double baseDamage = -1.0;
    if (Pokerogue3DS::calculatePokemonBaseDamage(state, damageTarget, 33, baseDamage) !=
        Pokerogue3DS::PokemonBaseDamageResult::Ok || baseDamage < 5.199999999 || baseDamage > 5.200000001) return 32;
    baseDamage = -1.0;
    if (Pokerogue3DS::calculatePokemonBaseDamage(state, damageTarget, 45, baseDamage) !=
        Pokerogue3DS::PokemonBaseDamageResult::NonDamagingMove || baseDamage != -1.0) return 33;
    if (Pokerogue3DS::calculatePokemonBaseDamage(state, damageTarget, 65535, baseDamage) !=
        Pokerogue3DS::PokemonBaseDamageResult::MissingMove || baseDamage != -1.0) return 34;
    double typeEffectiveness = -1.0;
    if (Pokerogue3DS::calculatePokemonTypeEffectiveness(33, state, typeEffectiveness) !=
        Pokerogue3DS::PokemonTypeEffectivenessResult::Ok || typeEffectiveness != 1.0) return 35;
    damageTarget.speciesDex = 92; // Gastly: Ghost/Poison; Normal is immune.
    if (Pokerogue3DS::calculatePokemonTypeEffectiveness(33, damageTarget, typeEffectiveness) !=
        Pokerogue3DS::PokemonTypeEffectivenessResult::Ok || typeEffectiveness != 0.0) return 36;
    damageTarget.speciesDex = 304; // Aron: Steel/Rock; two 0.5 multipliers combine.
    if (Pokerogue3DS::calculatePokemonTypeEffectiveness(33, damageTarget, typeEffectiveness) !=
        Pokerogue3DS::PokemonTypeEffectivenessResult::Ok || typeEffectiveness != 0.25) return 37;
    typeEffectiveness = -1.0;
    if (Pokerogue3DS::calculatePokemonTypeEffectiveness(65535, damageTarget, typeEffectiveness) !=
        Pokerogue3DS::PokemonTypeEffectivenessResult::MissingMove || typeEffectiveness != -1.0) return 38;
    damageTarget.speciesDex = 7; // Squirtle: Vine Whip is super-effective.
    damageTarget.stats[2] = 12;
    uint32_t coreDamage = 0;
    const auto coreResult = Pokerogue3DS::calculatePokemonDamageCore(state, damageTarget, 22, false, coreDamage);
    if (coreResult != Pokerogue3DS::PokemonDamageCoreResult::Ok) return 39;
    if (coreDamage != 14) return 42;
    if (Pokerogue3DS::calculatePokemonDamageCore(state, damageTarget, 22, true, coreDamage) !=
        Pokerogue3DS::PokemonDamageCoreResult::Ok || coreDamage != 9) return 40;
    damageTarget.speciesDex = 92;
    if (Pokerogue3DS::calculatePokemonDamageCore(state, damageTarget, 33, false, coreDamage) !=
        Pokerogue3DS::PokemonDamageCoreResult::Ok || coreDamage != 0) return 41;

    const uint16_t damageSeed[] = {'m', 'o', 'v', 'e'};
    Pokerogue3DS::PokerogueRngAdapter damageRng;
    damageRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    Pokerogue3DS::PokerogueRngAdapter damageExpectedRng = damageRng;
    const uint8_t accuracyRoll = static_cast<uint8_t>(damageExpectedRng.randSeedInt(100));
    const uint8_t criticalRoll = static_cast<uint8_t>(damageExpectedRng.randSeedInt(24));
    const uint8_t randomPercent = static_cast<uint8_t>(damageExpectedRng.randSeedIntRange(85, 100));
    const double expectedRawDamage = 5.2 * (criticalRoll == 0 ? 1.5 : 1.0) *
        (static_cast<double>(randomPercent) / 100.0);
    const uint32_t expectedDamage = expectedRawDamage < 1.0 ? 1 : static_cast<uint32_t>(expectedRawDamage);
    PokemonBattleState sameSpeciesTarget = state;
    PokemonMoveDamageRoll damageRoll{};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(state, sameSpeciesTarget, 33, false,
        damageRng, damageRoll) != PokemonMoveDamageResult::Ok) return 43;
    if (!damageRoll.hit || damageRoll.accuracyRoll != accuracyRoll || !damageRoll.accuracyWasRolled ||
        damageRoll.criticalRoll != criticalRoll || damageRoll.critical != (criticalRoll == 0) ||
        damageRoll.randomDamagePercent != randomPercent || damageRoll.damage != expectedDamage) return 44;
    const auto actualDamageRngState = damageRng.state();
    const auto expectedDamageRngState = damageExpectedRng.state();
    if (actualDamageRngState.carry != expectedDamageRngState.carry || actualDamageRngState.s0 != expectedDamageRngState.s0 ||
        actualDamageRngState.s1 != expectedDamageRngState.s1 || actualDamageRngState.s2 != expectedDamageRngState.s2) return 45;

    // Same pinned Tackle damage and seeded draws; burn is applied before truncation.
    PokemonBattleState burnCommandActor = state;
    burnCommandActor.status.present = true;
    burnCommandActor.status.effect = Pokerogue3DS::PokemonStatusEffect::Burn;
    PokemonBattleState burnCommandTarget = state;
    Pokerogue3DS::PokemonBurnDamagePolicy commandBurnPolicy{};
    commandBurnPolicy.resolved = true;
    PokerogueRngAdapter burnCommandRng;
    burnCommandRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    PokemonMoveActionResult burnCommandResult{};
    const uint8_t initialBurnPp = burnCommandActor.moves[0].pp;
    const uint16_t initialBurnHp = burnCommandTarget.hp;
    const double expectedBurnRaw = expectedRawDamage * 0.5;
    const uint32_t expectedBurnDamage = expectedBurnRaw < 1.0 ? 1 : static_cast<uint32_t>(expectedBurnRaw);
    if (Pokerogue3DS::useStandardPokemonMove(burnCommandActor, burnCommandTarget, 0, false,
            burnCommandRng, burnCommandResult, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, &commandBurnPolicy) != PokemonMoveActionStatus::Ok ||
        burnCommandResult.damageRoll.damage != expectedBurnDamage ||
        burnCommandActor.moves[0].pp != initialBurnPp - 1 ||
        burnCommandTarget.hp != initialBurnHp - burnCommandResult.damageApplied) return 612;
    const auto burnCommandState = burnCommandRng.state();
    if (burnCommandState.carry != expectedDamageRngState.carry ||
        burnCommandState.s0 != expectedDamageRngState.s0 ||
        burnCommandState.s1 != expectedDamageRngState.s1 ||
        burnCommandState.s2 != expectedDamageRngState.s2) return 613;

    burnCommandActor = state;
    burnCommandActor.status.present = true;
    burnCommandActor.status.effect = Pokerogue3DS::PokemonStatusEffect::Burn;
    burnCommandTarget = state;
    burnCommandRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    commandBurnPolicy.abilityBypassesReduction = true;
    if (Pokerogue3DS::useStandardPokemonMove(burnCommandActor, burnCommandTarget, 0, false,
            burnCommandRng, burnCommandResult, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, &commandBurnPolicy) != PokemonMoveActionStatus::Ok ||
        burnCommandResult.damageRoll.damage != expectedDamage) return 614;

    burnCommandActor = state;
    burnCommandActor.status.present = true;
    burnCommandActor.status.effect = Pokerogue3DS::PokemonStatusEffect::Burn;
    burnCommandTarget = state;
    burnCommandRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    commandBurnPolicy.resolved = false;
    burnCommandResult.ppConsumed = 123; // Failure must preserve caller output too.
    if (Pokerogue3DS::useStandardPokemonMove(burnCommandActor, burnCommandTarget, 0, false,
            burnCommandRng, burnCommandResult, nullptr, nullptr, nullptr, nullptr, nullptr,
            nullptr, nullptr, &commandBurnPolicy) != PokemonMoveActionStatus::UnsupportedAbilityCondition ||
        burnCommandActor.moves[0].pp != initialBurnPp || burnCommandTarget.hp != initialBurnHp ||
        burnCommandActor.turnDamageDealt != state.turnDamageDealt ||
        burnCommandResult.ppConsumed != 123 || !burnCommandActor.status.present) return 615;
    PokerogueRngAdapter untouchedBurnRng;
    untouchedBurnRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    if (burnCommandRng.randSeedUint32() != untouchedBurnRng.randSeedUint32()) return 616;

    PokerogueRngAdapter missRng;
    PokerogueRngAdapter missExpectedRng;
    uint16_t missSeed = 0;
    bool foundMissSeed = false;
    for (uint16_t candidate = 1; candidate < 256 && !foundMissSeed; ++candidate) {
        missRng.sow(&candidate, 1);
        missExpectedRng = missRng;
        if (missExpectedRng.randSeedInt(100) >= 50) { missSeed = candidate; foundMissSeed = true; }
    }
    if (!foundMissSeed) return 46;
    missRng.sow(&missSeed, 1);
    missExpectedRng = missRng;
    const uint8_t expectedMissRoll = static_cast<uint8_t>(missExpectedRng.randSeedInt(100));
    damageTarget = state;
    damageRoll = {};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(state, damageTarget, 192, false,
        missRng, damageRoll) != PokemonMoveDamageResult::Ok || damageRoll.hit ||
        !damageRoll.accuracyWasRolled || damageRoll.accuracyRoll != expectedMissRoll || damageRoll.damage != 0) return 47;
    const auto actualMissState = missRng.state();
    const auto expectedMissState = missExpectedRng.state();
    if (actualMissState.carry != expectedMissState.carry || actualMissState.s0 != expectedMissState.s0 ||
        actualMissState.s1 != expectedMissState.s1 || actualMissState.s2 != expectedMissState.s2) return 48;

    PokerogueRngAdapter immuneRng;
    immuneRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    const auto beforeImmuneState = immuneRng.state();
    damageTarget.speciesDex = 92;
    damageRoll = {};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(state, damageTarget, 33, false,
        immuneRng, damageRoll) != PokemonMoveDamageResult::Ok || damageRoll.hit || damageRoll.damage != 0) return 49;
    const auto afterImmuneState = immuneRng.state();
    if (beforeImmuneState.carry != afterImmuneState.carry || beforeImmuneState.s0 != afterImmuneState.s0 ||
        beforeImmuneState.s1 != afterImmuneState.s1 || beforeImmuneState.s2 != afterImmuneState.s2) return 50;

    PokemonBattleState moveActor = state;
    PokemonBattleState oneHpTarget = state;
    oneHpTarget.hp = 1;
    PokerogueRngAdapter actionRng;
    actionRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    PokerogueRngAdapter actionExpectedRng = actionRng;
    PokemonMoveDamageRoll actionExpectedRoll{};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(moveActor, oneHpTarget, 33, false,
        actionExpectedRng, actionExpectedRoll) != PokemonMoveDamageResult::Ok) return 51;
    moveActor.turnDamageDealt = 7;
    PokemonMoveActionResult actionResult{};
    if (Pokerogue3DS::useStandardPokemonMove(moveActor, oneHpTarget, 0, false, actionRng,
        actionResult) != PokemonMoveActionStatus::Ok || !actionResult.damageRoll.hit ||
        actionResult.damageRoll.damage != actionExpectedRoll.damage || actionResult.damageApplied != 1 ||
        !actionResult.targetFainted || oneHpTarget.hp != 0 || moveActor.moves[0].pp != 34) return 52;
    if (moveActor.turnDamageDealt != 8) return 521;
    const auto actualActionState = actionRng.state();
    const auto expectedActionState = actionExpectedRng.state();
    if (actualActionState.carry != expectedActionState.carry || actualActionState.s0 != expectedActionState.s0 ||
        actualActionState.s1 != expectedActionState.s1 || actualActionState.s2 != expectedActionState.s2) return 53;

    PokemonBattleState weatherActor = state, weatherTarget = state;
    weatherActor.abilityId = 0;
    weatherActor.moves[0] = {55, 25, 25}; // Real Water Gun.
    PokerogueRngAdapter weatherRng = actionExpectedRng;
    PokerogueRngAdapter expectedWeatherRng = weatherRng;
    weatherContext.resolved = true;
    weatherContext.effectiveWeather = Pokerogue3DS::PokemonEffectiveWeather::Rain;
    PokemonMoveDamageRoll expectedWeatherRoll{};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(weatherActor, weatherTarget, 55, false,
            expectedWeatherRng, expectedWeatherRoll, &weatherContext) != PokemonMoveDamageResult::Ok) return 276;
    PokemonMoveActionResult weatherAction{};
    if (Pokerogue3DS::useStandardPokemonMove(weatherActor, weatherTarget, 0, false, weatherRng,
            weatherAction, &weatherContext) != PokemonMoveActionStatus::Ok ||
        weatherActor.moves[0].pp != 24 || weatherAction.damageRoll.damage != expectedWeatherRoll.damage) return 277;
    weatherContext.resolved = false;
    const auto beforeWeatherFailure = weatherRng.state();
    const uint16_t beforeWeatherHp = weatherTarget.hp;
    if (beforeWeatherHp == 0) weatherTarget.hp = 1;
    const uint16_t failureHp = weatherTarget.hp;
    weatherAction.damageApplied = 123;
    if (Pokerogue3DS::useStandardPokemonMove(weatherActor, weatherTarget, 0, false, weatherRng,
            weatherAction, &weatherContext) != PokemonMoveActionStatus::UnresolvedWeather ||
        weatherActor.moves[0].pp != 24 || weatherTarget.hp != failureHp ||
        weatherAction.damageApplied != 123) return 278;
    const auto afterWeatherFailure = weatherRng.state();
    if (beforeWeatherFailure.carry != afterWeatherFailure.carry ||
        beforeWeatherFailure.s0 != afterWeatherFailure.s0 || beforeWeatherFailure.s1 != afterWeatherFailure.s1 ||
        beforeWeatherFailure.s2 != afterWeatherFailure.s2) return 279;

    weatherContext.resolved = true;
    weatherContext.effectiveWeather = PokemonEffectiveWeather::Rain;
    weatherContext.cancellationWeather = PokemonEffectiveWeather::HarshSun;
    const auto beforeCancelledRng = weatherRng.state();
    if (Pokerogue3DS::useStandardPokemonMove(weatherActor, weatherTarget, 0, false, weatherRng,
            weatherAction, &weatherContext) != PokemonMoveActionStatus::Ok ||
        !weatherAction.weatherCancelled || weatherActor.moves[0].pp != 23 ||
        weatherAction.damageApplied != 0 || weatherTarget.hp != failureHp) return 288;
    const auto afterCancelledRng = weatherRng.state();
    if (beforeCancelledRng.carry != afterCancelledRng.carry || beforeCancelledRng.s0 != afterCancelledRng.s0 ||
        beforeCancelledRng.s1 != afterCancelledRng.s1 || beforeCancelledRng.s2 != afterCancelledRng.s2) return 289;
    if (resolvedWeather.effectiveWeather != PokemonEffectiveWeather::Sunny ||
        resolvedWeather.cancellationWeather != PokemonEffectiveWeather::None) return 290;

    PokerogueRngAdapter guaranteedCritRng = actionRng;
    PokerogueRngAdapter expectedGuaranteedCritRng = guaranteedCritRng;
    (void)expectedGuaranteedCritRng.randSeedInt(100); // Storm Throw accuracy100.
    const auto expectedGuaranteedDamageRoll = expectedGuaranteedCritRng.randSeedIntRange(85, 100);
    PokemonMoveDamageRoll guaranteedCritRoll{};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(state, state, 480, false,
            guaranteedCritRng, guaranteedCritRoll) != PokemonMoveDamageResult::Ok ||
        !guaranteedCritRoll.hit || !guaranteedCritRoll.critical || guaranteedCritRoll.criticalWasRolled ||
        guaranteedCritRoll.randomDamagePercent != expectedGuaranteedDamageRoll) return 336;
    const auto actualGuaranteedState = guaranteedCritRng.state();
    const auto expectedGuaranteedState = expectedGuaranteedCritRng.state();
    if (actualGuaranteedState.carry != expectedGuaranteedState.carry ||
        actualGuaranteedState.s0 != expectedGuaranteedState.s0 || actualGuaranteedState.s1 != expectedGuaranteedState.s1 ||
        actualGuaranteedState.s2 != expectedGuaranteedState.s2) return 337;
    uint16_t battleArmorId = 0, superLuckId = 0;
    for (const auto& profile : PokerogueContent::kCriticalAbilityProfiles) {
        if (std::strcmp(profile.sourceSymbol, "AbilityId.BATTLE_ARMOR") == 0 &&
            profile.blocksCritical && profile.ignorable) battleArmorId = profile.abilityId;
        if (std::strcmp(profile.sourceSymbol, "AbilityId.SUPER_LUCK") == 0 &&
            profile.bonusStages == 1) superLuckId = profile.abilityId;
    }
    if (!battleArmorId || !superLuckId) return 338;
    if (!Pokerogue3DS::pokemonMoveCriticalDenominator(2, criticalDenominator, 1) ||
        criticalDenominator != 2) return 339;
    PokemonBattleState armorTarget = state;
    armorTarget.abilityId = battleArmorId;
    PokerogueRngAdapter armorRng = actionRng;
    PokerogueRngAdapter expectedArmorRng = armorRng;
    (void)expectedArmorRng.randSeedInt(100);
    const auto armorDamagePercent = expectedArmorRng.randSeedIntRange(85, 100);
    PokemonMoveDamageRoll armorRoll{};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(state, armorTarget, 480, false,
            armorRng, armorRoll) != PokemonMoveDamageResult::Ok || armorRoll.critical ||
        armorRoll.criticalWasRolled || armorRoll.randomDamagePercent != armorDamagePercent) return 340;
    const auto armorActual = armorRng.state(), armorExpected = expectedArmorRng.state();
    if (armorActual.carry != armorExpected.carry || armorActual.s0 != armorExpected.s0 ||
        armorActual.s1 != armorExpected.s1 || armorActual.s2 != armorExpected.s2) return 341;
    Pokerogue3DS::PokemonCriticalAbilityComponent critComponents[2] = {
        {superLuckId, true, true}, {battleArmorId, true, false}
    };
    Pokerogue3DS::PokemonCriticalPolicy resolvedCritPolicy{};
    if (!Pokerogue3DS::composePokemonCriticalAbilityPolicy(critComponents, 2, false, resolvedCritPolicy) ||
        !resolvedCritPolicy.resolved || resolvedCritPolicy.bonusStages != 1 || !resolvedCritPolicy.blocked) return 342;
    if (!Pokerogue3DS::composePokemonCriticalAbilityPolicy(critComponents, 2, true, resolvedCritPolicy) ||
        resolvedCritPolicy.blocked || resolvedCritPolicy.bonusStages != 1) return 343;
    critComponents[0].applies = false;
    if (!Pokerogue3DS::composePokemonCriticalAbilityPolicy(critComponents, 2, true, resolvedCritPolicy) ||
        resolvedCritPolicy.bonusStages) return 344;
    resolvedCritPolicy.resolved = false;
    PokemonMoveActionResult unresolvedCritAction{};
    PokemonBattleState unresolvedCritActor = state, unresolvedCritTarget = state;
    if (Pokerogue3DS::useStandardPokemonMove(unresolvedCritActor, unresolvedCritTarget, 0, false,
            armorRng, unresolvedCritAction, nullptr, &resolvedCritPolicy) !=
            PokemonMoveActionStatus::UnsupportedAbilityCondition ||
        unresolvedCritActor.moves[0].pp != state.moves[0].pp || unresolvedCritTarget.hp != state.hp) return 345;
    resolvedCritPolicy.resolved = true;
    resolvedCritPolicy.blocked = false;
    resolvedCritPolicy.bonusStages = 1;
    PokerogueRngAdapter luckRng = actionRng, expectedLuckRng = actionRng;
    (void)expectedLuckRng.randSeedInt(100);
    const uint8_t expectedLuckCritical = static_cast<uint8_t>(expectedLuckRng.randSeedInt(8));
    const uint8_t expectedLuckDamage = static_cast<uint8_t>(expectedLuckRng.randSeedIntRange(85, 100));
    PokemonMoveDamageRoll luckRoll{};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(state, state, 33, false, luckRng,
            luckRoll, nullptr, &resolvedCritPolicy) != PokemonMoveDamageResult::Ok ||
        !luckRoll.criticalWasRolled || luckRoll.criticalRoll != expectedLuckCritical ||
        luckRoll.critical != (expectedLuckCritical == 0) ||
        luckRoll.randomDamagePercent != expectedLuckDamage) return 346;
    uint16_t sniperId = 0;
    for (const auto& profile : PokerogueContent::kCriticalAbilityProfiles)
        if (std::strcmp(profile.sourceSymbol, "AbilityId.SNIPER") == 0 && profile.criticalMultiplier == 1.5)
            sniperId = profile.abilityId;
    if (!sniperId) return 347;
    Pokerogue3DS::PokemonCriticalAbilityComponent sniperComponent{sniperId, true, true};
    Pokerogue3DS::PokemonCriticalPolicy sniperPolicy{};
    if (!Pokerogue3DS::composePokemonCriticalAbilityPolicy(&sniperComponent, 1, false, sniperPolicy) ||
        sniperPolicy.damageMultiplier != 1.5) return 348;
    PokemonBattleState sniperActor = state;
    sniperActor.abilityId = sniperId;
    PokerogueRngAdapter sniperRng = actionRng, expectedSniperRng = actionRng;
    (void)expectedSniperRng.randSeedInt(100);
    const uint8_t sniperPercent = static_cast<uint8_t>(expectedSniperRng.randSeedIntRange(85, 100));
    double sniperBase = 0, sniperType = 0;
    if (Pokerogue3DS::calculatePokemonBaseDamage(sniperActor, state, 480, sniperBase, true) !=
            Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        Pokerogue3DS::calculatePokemonTypeEffectiveness(480, state, sniperType) !=
            Pokerogue3DS::PokemonTypeEffectivenessResult::Ok) return 349;
    PokemonMoveDamageRoll sniperRoll{};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(sniperActor, state, 480, false, sniperRng,
            sniperRoll, nullptr, &sniperPolicy) != PokemonMoveDamageResult::Ok || !sniperRoll.critical) return 350;
    const uint32_t expectedSniperDamage = static_cast<uint32_t>(sniperBase * 2.25 *
        (static_cast<double>(sniperPercent) / 100.0) * sniperType);
    if (sniperRoll.damage != (expectedSniperDamage ? expectedSniperDamage : 1)) return 351;
    uint16_t noGuardId = 0;
    for (const auto& profile : PokerogueContent::kAlwaysHitAbilityProfiles)
        if (std::strcmp(profile.sourceSymbol, "AbilityId.NO_GUARD") == 0) noGuardId = profile.abilityId;
    if (!noGuardId) return 352;
    PokemonBattleState guardedTarget = state;
    guardedTarget.abilityId = noGuardId;
    PokerogueRngAdapter noGuardRng = actionRng, expectedNoGuardRng = actionRng;
    const uint8_t expectedNoGuardCritical = static_cast<uint8_t>(expectedNoGuardRng.randSeedInt(24));
    const uint8_t expectedNoGuardDamage = static_cast<uint8_t>(expectedNoGuardRng.randSeedIntRange(85, 100));
    PokemonMoveDamageRoll noGuardRoll{};
    if (Pokerogue3DS::resolveStandardPokemonMoveDamage(state, guardedTarget, 33, false, noGuardRng,
            noGuardRoll) != PokemonMoveDamageResult::Ok || !noGuardRoll.hit || noGuardRoll.accuracyWasRolled ||
        noGuardRoll.criticalRoll != expectedNoGuardCritical || noGuardRoll.randomDamagePercent != expectedNoGuardDamage) return 353;
    Pokerogue3DS::PokemonWeatherAbilityComponent hitComponent{noGuardId, true, false};
    Pokerogue3DS::PokemonHitPolicy hitPolicy{};
    if (!Pokerogue3DS::composePokemonAlwaysHitPolicy(&hitComponent, 1, hitPolicy) ||
        !hitPolicy.resolved || !hitPolicy.bypassAccuracy) return 354;
    hitComponent.applies = false;
    if (!Pokerogue3DS::composePokemonAlwaysHitPolicy(&hitComponent, 1, hitPolicy) ||
        hitPolicy.bypassAccuracy) return 355;
    hitPolicy.resolved = false;
    PokemonMoveActionResult invalidHitAction{};
    PokemonBattleState invalidHitActor = state, invalidHitTarget = state;
    if (Pokerogue3DS::useStandardPokemonMove(invalidHitActor, invalidHitTarget, 0, false,
            noGuardRng, invalidHitAction, nullptr, nullptr, &hitPolicy) !=
            PokemonMoveActionStatus::UnsupportedAbilityCondition ||
        invalidHitActor.moves[0].pp != state.moves[0].pp || invalidHitTarget.hp != state.hp) return 356;
    uint16_t compoundEyesId = 0;
    for (const auto& profile : PokerogueContent::kAccuracyAbilityProfiles)
        if (std::strcmp(profile.sourceSymbol, "AbilityId.COMPOUND_EYES") == 0 &&
            profile.accuracy && profile.multiplier == 1.3) compoundEyesId = profile.abilityId;
    if (!compoundEyesId) return 357;
    Pokerogue3DS::PokemonWeatherAbilityComponent eyesComponent{compoundEyesId, true, true};
    if (!Pokerogue3DS::composePokemonAlwaysHitPolicy(&eyesComponent, 1, hitPolicy) ||
        hitPolicy.accuracyMultiplier != 1.3 || hitPolicy.bypassAccuracy) return 358;
    eyesComponent.belongsToAttacker = false;
    if (!Pokerogue3DS::composePokemonAlwaysHitPolicy(&eyesComponent, 1, hitPolicy) ||
        hitPolicy.accuracyMultiplier != 1.0) return 359;
    uint16_t hustleId = 0;
    for (const auto& profile : PokerogueContent::kAccuracyAbilityProfiles)
        if (std::strcmp(profile.sourceSymbol, "AbilityId.HUSTLE") == 0 &&
            profile.accuracy && profile.multiplier == 0.8 && profile.requiredCategory == 0) hustleId = profile.abilityId;
    if (!hustleId) return 360;
    Pokerogue3DS::PokemonWeatherAbilityComponent hustleComponent{hustleId, true, true};
    if (!Pokerogue3DS::composePokemonAlwaysHitPolicy(&hustleComponent, 1, hitPolicy, 33) ||
        hitPolicy.accuracyMultiplier != 0.8) return 361;
    if (!Pokerogue3DS::composePokemonAlwaysHitPolicy(&hustleComponent, 1, hitPolicy, 55) ||
        hitPolicy.accuracyMultiplier != 1.0) return 362;
    if (Pokerogue3DS::composePokemonAlwaysHitPolicy(&hustleComponent, 1, hitPolicy)) return 363;
    PokemonBattleState hustleActor = state;
    hustleActor.abilityId = hustleId;
    hustleActor.stats[1] = 53;
    hustleActor.statStages[0] = 1;
    PokemonBattleState neutralHustleActor = hustleActor;
    neutralHustleActor.abilityId = 0;
    double neutralHustleDamage = 0, boostedHustleDamage = 0;
    if (Pokerogue3DS::calculatePokemonBaseDamage(neutralHustleActor, state, 33,
            neutralHustleDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        Pokerogue3DS::calculatePokemonBaseDamage(hustleActor, state, 33,
            boostedHustleDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok) return 364;
    const double hustleError = boostedHustleDamage - ((neutralHustleDamage - 2.0) * 119.0 / 79.0 + 2.0);
    if (hustleError < -0.00000001 || hustleError > 0.00000001) return 365;
    if (Pokerogue3DS::calculatePokemonBaseDamage(neutralHustleActor, state, 55,
            neutralHustleDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        Pokerogue3DS::calculatePokemonBaseDamage(hustleActor, state, 55,
            boostedHustleDamage) != Pokerogue3DS::PokemonBaseDamageResult::Ok ||
        boostedHustleDamage != neutralHustleDamage) return 366;
    Pokerogue3DS::PokemonWeatherAbilityComponent veilComponent{sandVeilId, true, false};
    Pokerogue3DS::PokemonMoveWeatherContext veilWeather{};
    veilWeather.resolved = true;
    if (!Pokerogue3DS::composePokemonAlwaysHitPolicy(&veilComponent, 1, hitPolicy, 33, &veilWeather) ||
        hitPolicy.accuracyMultiplier != 1.0) return 367;
    veilWeather.cancellationWeather = PokemonEffectiveWeather::Sandstorm;
    if (!Pokerogue3DS::composePokemonAlwaysHitPolicy(&veilComponent, 1, hitPolicy, 33, &veilWeather) ||
        hitPolicy.accuracyMultiplier != 0.8) return 368;
    if (Pokerogue3DS::composePokemonAlwaysHitPolicy(&veilComponent, 1, hitPolicy, 33)) return 369;
    uint16_t swiftSwimId = 0;
    for (const auto& profile : PokerogueContent::kSpeedAbilityProfiles)
        if (std::strcmp(profile.sourceSymbol, "AbilityId.SWIFT_SWIM") == 0 &&
            profile.multiplier == 2.0 && profile.weatherMask == ((1u << 2) | (1u << 7))) swiftSwimId = profile.abilityId;
    if (!swiftSwimId) return 370;
    PokemonBattleState swimmer = state;
    swimmer.abilityId = swiftSwimId;
    swimmer.stats[5] = 53;
    swimmer.statStages[4] = 1;
    Pokerogue3DS::PokemonMoveWeatherContext speedWeather{};
    speedWeather.resolved = true;
    uint32_t weatherSpeed = 0;
    if (!Pokerogue3DS::pokemonWeatherEffectiveSpeed(swimmer, speedWeather, weatherSpeed) ||
        weatherSpeed != 79) return 371;
    speedWeather.cancellationWeather = PokemonEffectiveWeather::Rain;
    if (!Pokerogue3DS::pokemonWeatherEffectiveSpeed(swimmer, speedWeather, weatherSpeed) ||
        weatherSpeed != 159) return 372;
    speedWeather.effectiveWeather = PokemonEffectiveWeather::Rain;
    speedWeather.cancellationWeather = PokemonEffectiveWeather::None;
    if (!Pokerogue3DS::pokemonWeatherEffectiveSpeed(swimmer, speedWeather, weatherSpeed) ||
        weatherSpeed != 79) return 373;
    Pokerogue3DS::PokemonTurnOrderFieldPolicy trickRoomPolicy{};
    trickRoomPolicy.resolved = true;
    trickRoomPolicy.speedReversed = true;
    PokemonBattleState trickFast = state, trickSlow = state;
    trickFast.stats[5] = 100;
    trickSlow.stats[5] = 50;
    trickFast.statStages[4] = trickSlow.statStages[4] = 0;
    if (Pokerogue3DS::resolveBaselineFirstMover(trickFast, trickSlow, 33, 33,
            weatherSeed, 7, 1, 1, nullptr, &trickRoomPolicy) != Pokerogue3DS::BaselineFirstMover::Enemy) return 374;
    if (Pokerogue3DS::resolveBaselineFirstMover(trickFast, trickSlow, 98, 33,
            weatherSeed, 7, 1, 1, nullptr, &trickRoomPolicy) != Pokerogue3DS::BaselineFirstMover::Player) return 375;
    trickSlow.stats[5] = 100;
    const auto ordinaryTie = Pokerogue3DS::resolveBaselineFirstMover(trickFast, trickSlow, 33, 33, weatherSeed, 7, 1, 1);
    const auto reversedTie = Pokerogue3DS::resolveBaselineFirstMover(trickFast, trickSlow, 33, 33,
        weatherSeed, 7, 1, 1, nullptr, &trickRoomPolicy);
    if (ordinaryTie == Pokerogue3DS::BaselineFirstMover::Invalid || reversedTie == ordinaryTie ||
        reversedTie == Pokerogue3DS::BaselineFirstMover::Invalid) return 376;
    trickRoomPolicy.resolved = false;
    if (Pokerogue3DS::resolveBaselineFirstMover(trickFast, trickSlow, 33, 33,
            weatherSeed, 7, 1, 1, nullptr, &trickRoomPolicy) != Pokerogue3DS::BaselineFirstMover::Invalid) return 377;
    if (!Pokerogue3DS::supportsPokemonTrickRoomMove(433) ||
        Pokerogue3DS::supportsPokemonTrickRoomMove(33)) return 388;
    uint8_t pressureCost = 0;
    for (const auto& profile : PokerogueContent::kPpAbilityProfiles)
        if (!Pokerogue3DS::pokemonSingleOpponentPpCost(profile.abilityId, pressureCost) ||
            pressureCost != 1 + profile.increase) return 389;
    // Real Pressure metadata composes once across all active area targets.
    for (const auto& profile : PokerogueContent::kPpAbilityProfiles) {
        const uint16_t targets[] = {profile.abilityId, profile.abilityId};
        if (!Pokerogue3DS::pokemonActiveTargetsPpCost(targets, 2, pressureCost) ||
            pressureCost != 1 + 2 * profile.increase) return 464;
        if (!Pokerogue3DS::pokemonActiveTargetsPpCost(targets, 1, pressureCost) ||
            pressureCost != 1 + profile.increase) return 465;
    }
    if (!Pokerogue3DS::pokemonActiveTargetsPpCost(nullptr, 0, pressureCost) || pressureCost != 1) return 466;
    pressureCost = 7;
    const uint16_t unknownTarget[] = {65535};
    if (Pokerogue3DS::pokemonActiveTargetsPpCost(nullptr, 1, pressureCost) ||
        Pokerogue3DS::pokemonActiveTargetsPpCost(unknownTarget, 1, pressureCost) || pressureCost != 7) return 467;
    Pokerogue3DS::PokemonTrickRoomState roomState{};
    Pokerogue3DS::PokemonTrickRoomEvent roomEvent{};
    if (!Pokerogue3DS::applyPokemonTrickRoomMove(roomState, 433, 123, roomEvent) ||
        !roomEvent.activated || roomState.turnsLeft != 5 || roomState.sourcePokemonId != 123) return 378;
    if (!Pokerogue3DS::pokemonTrickRoomOrderPolicy(roomState).speedReversed) return 379;
    if (!Pokerogue3DS::applyPokemonTrickRoomMove(roomState, 433, 456, roomEvent) ||
        !roomEvent.removed || roomEvent.activated || roomState.turnsLeft) return 380;
    if (!Pokerogue3DS::applyPokemonTrickRoomMove(roomState, 433, 123, roomEvent)) return 381;
    for (unsigned i = 0; i < 4; ++i)
        if (!Pokerogue3DS::advancePokemonTrickRoomTurnEnd(roomState, roomEvent) || roomEvent.expired) return 382;
    if (!Pokerogue3DS::advancePokemonTrickRoomTurnEnd(roomState, roomEvent) ||
        !roomEvent.expired || !roomEvent.removed || roomState.sourceMoveId || roomState.turnsLeft) return 383;
    PokemonBattleState roomUser = state;
    roomUser.moves[0] = {433, 5, 5};
    Pokerogue3DS::PokemonTrickRoomCommandPolicy roomCommandPolicy{};
    Pokerogue3DS::PokemonTrickRoomCommandEvent roomCommandEvent{};
    if (Pokerogue3DS::usePokemonTrickRoomCommand(roomUser, roomState, 0, roomCommandPolicy,
            roomCommandEvent) != Pokerogue3DS::PokemonTrickRoomCommandResult::UnresolvedPolicy ||
        roomUser.moves[0].pp != 5) return 384;
    roomCommandPolicy.resolved = true;
    if (Pokerogue3DS::usePokemonTrickRoomCommand(roomUser, roomState, 0, roomCommandPolicy,
            roomCommandEvent) != Pokerogue3DS::PokemonTrickRoomCommandResult::Ok ||
        !roomCommandEvent.field.activated || roomCommandEvent.ppConsumed != 1 || roomUser.moves[0].pp != 4) return 385;
    if (Pokerogue3DS::usePokemonTrickRoomCommand(roomUser, roomState, 0, roomCommandPolicy,
            roomCommandEvent) != Pokerogue3DS::PokemonTrickRoomCommandResult::Ok ||
        !roomCommandEvent.field.removed || roomState.turnsLeft || roomUser.moves[0].pp != 3) return 386;
    roomCommandPolicy.failsBeforeEffect = true;
    if (Pokerogue3DS::usePokemonTrickRoomCommand(roomUser, roomState, 0, roomCommandPolicy,
            roomCommandEvent) != Pokerogue3DS::PokemonTrickRoomCommandResult::Ok ||
        !roomCommandEvent.failed || roomState.turnsLeft || roomUser.moves[0].pp != 2) return 387;
    PokemonBattleState ppActor = state;
    PokemonBattleState ppTarget = state;
    ppActor.moves[0] = {33, 1, 35};
    ppTarget.hp = ppTarget.maxHp;
    PokerogueRngAdapter ppRng;
    ppRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    Pokerogue3DS::PokemonPpPolicy ppPolicy{};
    Pokerogue3DS::PokemonMoveActionResult ppEvent{};
    if (Pokerogue3DS::useStandardPokemonMove(ppActor, ppTarget, 0, false, ppRng, ppEvent,
            nullptr, nullptr, nullptr, &ppPolicy) != Pokerogue3DS::PokemonMoveActionStatus::UnresolvedPp ||
        ppActor.moves[0].pp != 1 || ppTarget.hp != ppTarget.maxHp) return 390;
    ppPolicy.resolved = true;
    ppPolicy.cost = 2;
    if (Pokerogue3DS::useStandardPokemonMove(ppActor, ppTarget, 0, false, ppRng, ppEvent,
            nullptr, nullptr, nullptr, &ppPolicy) != Pokerogue3DS::PokemonMoveActionStatus::Ok ||
        ppActor.moves[0].pp || ppEvent.ppConsumed != 1) return 391;
    ppPolicy.cost = 0;
    ppTarget.hp = ppTarget.maxHp;
    if (Pokerogue3DS::useStandardPokemonMove(ppActor, ppTarget, 0, false, ppRng, ppEvent,
            nullptr, nullptr, nullptr, &ppPolicy) != Pokerogue3DS::PokemonMoveActionStatus::Ok ||
        ppActor.moves[0].pp || ppEvent.ppConsumed) return 392;
    uint16_t soundproofId = 0;
    for (const auto& profile : PokerogueContent::kMoveImmunityAbilityProfiles)
        if (std::strcmp(profile.sourceSymbol, "AbilityId.SOUNDPROOF") == 0) soundproofId = profile.abilityId;
    if (!soundproofId) return 396;
    Pokerogue3DS::PokemonWeatherAbilityComponent soundComponents[] = {{soundproofId, true, false}};
    Pokerogue3DS::PokemonHitPolicy soundPolicy{};
    if (!Pokerogue3DS::composePokemonAlwaysHitPolicy(soundComponents, 1, soundPolicy, 304) ||
        !soundPolicy.blockedByAbility) return 397;
    PokemonBattleState soundActor = state, soundTarget = state;
    soundActor.moves[0] = {304, 10, 10};
    PokerogueRngAdapter soundRng;
    soundRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    const auto soundBefore = soundRng.state();
    Pokerogue3DS::PokemonMoveActionResult soundEvent{};
    if (Pokerogue3DS::useStandardPokemonMove(soundActor, soundTarget, 0, false, soundRng, soundEvent,
            nullptr, nullptr, &soundPolicy) != Pokerogue3DS::PokemonMoveActionStatus::Ok ||
        !soundEvent.damageRoll.abilityBlocked || soundEvent.damageApplied || soundActor.moves[0].pp != 9 ||
        soundRng.state().s0 != soundBefore.s0 || soundRng.state().s1 != soundBefore.s1 ||
        soundRng.state().s2 != soundBefore.s2 || soundRng.state().c != soundBefore.c) return 398;
    soundComponents[0].belongsToAttacker = true;
    if (!Pokerogue3DS::composePokemonAlwaysHitPolicy(soundComponents, 1, soundPolicy, 304) ||
        soundPolicy.blockedByAbility) return 399;
    PokemonBattleState noPpActor = state;
    noPpActor.moves[0].pp = 0;
    PokemonBattleState unchangedTarget = state;
    PokerogueRngAdapter noPpRng;
    noPpRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    const auto beforeNoPpState = noPpRng.state();
    actionResult = {};
    actionResult.damageApplied = 123;
    if (Pokerogue3DS::useStandardPokemonMove(noPpActor, unchangedTarget, 0, false, noPpRng,
        actionResult) != PokemonMoveActionStatus::NoPp || actionResult.damageApplied != 123 ||
        unchangedTarget.hp != state.hp || noPpActor.moves[0].pp != 0) return 54;
    const auto afterNoPpState = noPpRng.state();
    if (beforeNoPpState.carry != afterNoPpState.carry || beforeNoPpState.s0 != afterNoPpState.s0 ||
        beforeNoPpState.s1 != afterNoPpState.s1 || beforeNoPpState.s2 != afterNoPpState.s2) return 55;

    PokemonBattleState missActor = state;
    missActor.moves[0] = {192, 1, 1}; // Zap Cannon: deliberately seeded to miss.
    PokemonBattleState missTarget = state;
    PokerogueRngAdapter actionMissRng;
    actionMissRng.sow(&missSeed, 1);
    actionResult = {};
    if (Pokerogue3DS::useStandardPokemonMove(missActor, missTarget, 0, false, actionMissRng,
        actionResult) != PokemonMoveActionStatus::Ok || actionResult.damageRoll.hit ||
        actionResult.damageApplied != 0 || actionResult.targetFainted || missTarget.hp != state.hp ||
        missActor.moves[0].pp != 0) return 56;

    input.speciesDex = 6;
    input.formId = "charizard:mega_x";
    input.abilityId = 181; // Tough Claws, from Charizard Mega X upstream form data.
    input.moveCount = 0;
    input.ivs[0] = input.ivs[1] = input.ivs[2] = input.ivs[3] = input.ivs[4] = input.ivs[5] = 0;
    input.natureRaisedStat = input.natureLoweredStat = -1;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::Ok) return 15;
    if (state.formId == nullptr || state.formId[0] != 'c' || state.stats[1] != 18) return 16;
    input.formId = "venusaur:mega";
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidForm) return 17;
    input.formId = "charizard:mega_x";
    input.abilityId = 66;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidAbility) return 18;

    input.speciesDex = 1;
    input.formId = nullptr;
    input.abilityId = 65;
    input.moveCount = 2;
    input.moveIds[0] = 33;
    input.moveIds[1] = 45;
    input.ivs[0] = input.ivs[1] = input.ivs[2] = input.ivs[3] = input.ivs[4] = input.ivs[5] = 0;

    input.ivs[0] = input.ivs[1] = input.ivs[2] = input.ivs[3] = input.ivs[4] = input.ivs[5] = 31;
    input.natureRaisedStat = 1;
    input.natureLoweredStat = 2;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::Ok) return 5;
    if (state.stats[0] != 21 || state.stats[1] != 13 || state.stats[2] != 9) return 6;

    input.natureRaisedStat = 2;
    input.natureLoweredStat = 2;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::Ok) return 13;
    if (state.stats[1] != 11 || state.stats[2] != 11) return 14;

    const PokemonBattleState preserved = state;
    input.gender = PokemonGender::Unspecified;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidGender) return 29;
    if (state.gender != preserved.gender) return 30;
    input.gender = PokemonGender::Male;
    input.ivs[0] = 32;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidIv) return 7;
    if (state.maxHp != preserved.maxHp || state.stats[1] != preserved.stats[1]) return 8;
    input.ivs[0] = 31;

    input.abilityId = 999;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidAbility) return 9;
    input.abilityId = 65;
    input.moveIds[0] = 65535;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::MissingMove) return 10;
    input.moveIds[0] = 33;
    input.moveCount = 5;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::InvalidMoveCount) return 11;
    input.moveCount = 2;

    input.speciesDex = 0;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::MissingSpecies) return 12;

    input.speciesDex = 81;
    input.abilityId = 42;
    input.gender = PokemonGender::Genderless;
    input.moveCount = 0;
    if (Pokerogue3DS::initializePokemonBattleState(input, state) != PokemonBattleInitResult::Ok ||
        state.gender != PokemonGender::Genderless || state.speciesDex != 81) return 31;

    const uint16_t seed[] = {'a', 'b', 'i', 'l', 'i', 't', 'y'};
    Pokerogue3DS::PokerogueRngAdapter rng;
    rng.sow(seed, sizeof(seed) / sizeof(seed[0]));
    Pokerogue3DS::PokerogueRngAdapter expected = rng;
    const uint8_t expectedRegular = static_cast<uint8_t>(expected.randSeedInt(2));
    const bool expectedHidden = expected.randSeedInt(256) == 0;
    uint8_t abilityIndex = 9;
    if (Pokerogue3DS::selectPokemonAbilityIndex(16, 256, rng, abilityIndex) != PokemonAbilitySelectionResult::Ok) return 19;
    const auto actualRngState = rng.state();
    const auto expectedRngState = expected.state();
    if (abilityIndex != (expectedHidden ? 2 : expectedRegular) ||
        actualRngState.carry != expectedRngState.carry || actualRngState.s0 != expectedRngState.s0 ||
        actualRngState.s1 != expectedRngState.s1 || actualRngState.s2 != expectedRngState.s2) return 20;

    rng.sow(seed, sizeof(seed) / sizeof(seed[0]));
    expected = rng;
    const bool expectedSingleHidden = expected.randSeedInt(256) == 0;
    if (Pokerogue3DS::selectPokemonAbilityIndex(1, 256, rng, abilityIndex) != PokemonAbilitySelectionResult::Ok) return 21;
    const auto singleActualState = rng.state();
    const auto singleExpectedState = expected.state();
    if (abilityIndex != (expectedSingleHidden ? 2 : 0) || singleActualState.carry != singleExpectedState.carry ||
        singleActualState.s0 != singleExpectedState.s0 || singleActualState.s1 != singleExpectedState.s1 ||
        singleActualState.s2 != singleExpectedState.s2) return 22;
    abilityIndex = 9;
    if (Pokerogue3DS::selectPokemonAbilityIndex(1, 0, rng, abilityIndex) != PokemonAbilitySelectionResult::InvalidHiddenRate || abilityIndex != 9) return 23;

    // Numeric ratios consume exactly one upstream randSeedFloat; genderless consumes none.
    rng.sow(seed, sizeof(seed) / sizeof(seed[0]));
    expected = rng;
    const double genderRoll = expected.frac() * 100.0;
    PokemonGender gender = PokemonGender::Genderless;
    if (Pokerogue3DS::selectPokemonGender(6, rng, gender) != PokemonGenderSelectionResult::Ok) return 24;
    const auto genderActualState = rng.state();
    const auto genderExpectedState = expected.state();
    if (gender != (genderRoll <= 87.5 ? PokemonGender::Male : PokemonGender::Female) ||
        genderActualState.carry != genderExpectedState.carry || genderActualState.s0 != genderExpectedState.s0 ||
        genderActualState.s1 != genderExpectedState.s1 || genderActualState.s2 != genderExpectedState.s2) return 25;

    rng.sow(seed, sizeof(seed) / sizeof(seed[0]));
    expected = rng;
    if (Pokerogue3DS::selectPokemonGender(81, rng, gender) != PokemonGenderSelectionResult::Ok ||
        gender != PokemonGender::Genderless) return 26;
    const auto genderlessActualState = rng.state();
    const auto genderlessExpectedState = expected.state();
    if (genderlessActualState.carry != genderlessExpectedState.carry ||
        genderlessActualState.s0 != genderlessExpectedState.s0 || genderlessActualState.s1 != genderlessExpectedState.s1 ||
        genderlessActualState.s2 != genderlessExpectedState.s2) return 27;

    // Pinned Pokemon constructor draws abilityIndex before id, then derives
    // IVs from that full-width ID and draws gender.
    rng.sow(seed, sizeof(seed) / sizeof(seed[0]));
    expected = rng;
    const auto* charizard = PokerogueContent::findSpeciesByDex(6);
    const uint8_t actorRegular = charizard->ability2 == charizard->ability1
        ? 0 : static_cast<uint8_t>(expected.randSeedInt(2));
    const bool actorHidden = expected.randSeedInt(256) == 0;
    const uint32_t actorId = expected.randSeedUint32();
    uint8_t actorExpectedIvs[6]{};
    Pokerogue3DS::derivePokemonIvsFromId(actorId, actorExpectedIvs);
    const double actorGenderRoll = expected.frac() * 100.0;
    Pokerogue3DS::PokemonActorIdentity actorIdentity{};
    if (Pokerogue3DS::generatePokemonActorIdentity(6, 256, rng, actorIdentity) !=
        Pokerogue3DS::PokemonActorIdentityResult::Ok) return 64;
    if (actorIdentity.pokemonId != actorId) return 68;
    if (actorIdentity.abilityIndex != (actorHidden ? 2 : actorRegular)) return 69;
    if (actorIdentity.gender != (actorGenderRoll <= 87.5 ? PokemonGender::Male : PokemonGender::Female)) return 70;
    for (uint8_t i = 0; i < 6; ++i) if (actorIdentity.ivs[i] != actorExpectedIvs[i]) return 66;
    const auto actorActualRng = rng.state();
    const auto actorExpectedRng = expected.state();
    if (actorActualRng.carry != actorExpectedRng.carry || actorActualRng.s0 != actorExpectedRng.s0 ||
        actorActualRng.s1 != actorExpectedRng.s1 || actorActualRng.s2 != actorExpectedRng.s2) return 67;

    const auto expectedActorNature = static_cast<Pokerogue3DS::PokemonNature>(expected.randSeedInt(25));
    Pokerogue3DS::generatePokemonActorNature(actorIdentity, rng);
    if (actorIdentity.nature != expectedActorNature) return 78;
    const auto actorNatureActualRng = rng.state();
    const auto actorNatureExpectedRng = expected.state();
    if (actorNatureActualRng.carry != actorNatureExpectedRng.carry ||
        actorNatureActualRng.s0 != actorNatureExpectedRng.s0 ||
        actorNatureActualRng.s1 != actorNatureExpectedRng.s1 ||
        actorNatureActualRng.s2 != actorNatureExpectedRng.s2) return 79;

    PokemonBattleInit actorBattleInput{};
    actorBattleInput.speciesDex = 6;
    actorBattleInput.level = 12;
    actorBattleInput.moveCount = 1;
    actorBattleInput.moveIds[0] = 33;
    actorBattleInput.nature = Pokerogue3DS::PokemonNature::Hardy;
    PokemonBattleState actorBattleState{};
    if (Pokerogue3DS::initializePokemonBattleStateForActor(
            actorBattleInput, actorIdentity, actorBattleState) != PokemonBattleInitResult::Ok) return 71;
    if (actorBattleState.pokemonId != actorIdentity.pokemonId || actorBattleState.nature != actorIdentity.nature ||
        actorBattleState.abilityId != charizard->ability1 || actorBattleState.gender != actorIdentity.gender ||
        !actorBattleState.ivsWereDerivedFromPokemonId || actorBattleState.level != actorBattleInput.level ||
        actorBattleState.moves[0].moveId != 33) return 72;
    PokemonBattleInit explicitNatureInput = actorBattleInput;
    explicitNatureInput.pokemonId = actorIdentity.pokemonId;
    explicitNatureInput.deriveIvsFromPokemonId = true;
    explicitNatureInput.gender = actorIdentity.gender;
    explicitNatureInput.abilityId = charizard->ability1;
    explicitNatureInput.nature = actorIdentity.nature;
    PokemonBattleState explicitNatureState{};
    if (Pokerogue3DS::initializePokemonBattleState(explicitNatureInput, explicitNatureState) !=
        PokemonBattleInitResult::Ok) return 80;
    for (uint8_t i = 0; i < 6; ++i) {
        if (actorBattleState.ivs[i] != actorIdentity.ivs[i] ||
            actorBattleState.stats[i] != explicitNatureState.stats[i]) return 73;
    }

    const auto dexFor = [](const char* id) -> uint16_t {
        for (const auto& species : PokerogueContent::kSpecies) {
            const char* a = species.id;
            const char* b = id;
            while (*a && *b && *a == *b) { ++a; ++b; }
            if (*a == *b) return species.dex;
        }
        return 0;
    };
    const auto formAt = [](const char* speciesId, std::size_t ordinal) -> const char* {
        for (const auto& form : PokerogueContent::kForms) {
            const char* a = form.speciesId;
            const char* b = speciesId;
            while (*a && *b && *a == *b) { ++a; ++b; }
            if (*a == *b) {
                if (!ordinal) return form.id;
                --ordinal;
            }
        }
        return nullptr;
    };
    const uint16_t formSeed[] = {'f', 'o', 'r', 'm'};
    PokerogueRngAdapter formRng;
    formRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    Pokerogue3DS::PokemonActorIdentity formActor{};
    Pokerogue3DS::PokemonFormSelectionContext formContext{};
    formActor.gender = PokemonGender::Female;
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("meowstic"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("meowstic", 1)) return 81;
    formActor.gender = PokemonGender::Male;
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("meowstic"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("meowstic", 0)) return 82;

    PokerogueRngAdapter formExpectedRng;
    formExpectedRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    const auto expectedUnownIndex = formExpectedRng.randSeedInt(28);
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("unown"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok) return 83;
    if (formActor.formId != formAt("unown", static_cast<std::size_t>(expectedUnownIndex))) return 84;
    const auto actualFormRngState = formRng.state();
    const auto expectedFormRngState = formExpectedRng.state();
    if (actualFormRngState.carry != expectedFormRngState.carry || actualFormRngState.s0 != expectedFormRngState.s0 ||
        actualFormRngState.s1 != expectedFormRngState.s1 || actualFormRngState.s2 != expectedFormRngState.s2) return 85;

    formContext = {};
    formContext.biomeId = "beach";
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("wormadam"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("wormadam", 1)) return 86;
    formContext.trainerBattle = true;
    formContext.trainerSpecialtyType = "STEEL";
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("wormadam"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("wormadam", 2)) return 87;
    formContext = {};
    formContext.timeOfDay = "DUSK";
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("lycanroc"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("lycanroc", 2)) return 88;
    formContext = {};
    formContext.hasMysteryEncounters = true;
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("gimmighoul"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("gimmighoul", 1)) return 89;
    formContext = {};
    formContext.trainerBattle = true;
    formContext.trainerSpecialtyType = "WATER";
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("rotom"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("rotom", 2)) return 90;
    formContext = {};
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("toxtricity"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("toxtricity", 0)) return 91;
    formContext.nature = Pokerogue3DS::PokemonNature::Timid;
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("toxtricity"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok ||
        formActor.formId != formAt("toxtricity", 1)) return 92;
    formContext = {};
    formContext.trainerBattle = true;
    formContext.waveIndex = 29;
    if (Pokerogue3DS::selectPokemonActorForm(dexFor("pikachu"), formContext, formRng, formActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok || formActor.formId != formAt("pikachu", 0)) return 93;
    formActor.gender = PokemonGender::Female;
    formActor.formId = formAt("meowstic", 1);
    PokemonBattleInit selectedFormInput{};
    selectedFormInput.speciesDex = dexFor("meowstic");
    selectedFormInput.level = 10;
    PokemonBattleState selectedFormState{};
    if (Pokerogue3DS::initializePokemonBattleStateForActor(
            selectedFormInput, formActor, selectedFormState) != PokemonBattleInitResult::Ok ||
        selectedFormState.formId != formActor.formId) return 94;

    formContext = {};
    PokerogueRngAdapter stagedActorRng;
    stagedActorRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    Pokerogue3DS::PokemonActorIdentity stagedActor{};
    if (Pokerogue3DS::generatePokemonActorIdentity(
            dexFor("unown"), 256, stagedActorRng, stagedActor) !=
        Pokerogue3DS::PokemonActorIdentityResult::Ok ||
        Pokerogue3DS::selectPokemonActorForm(
            dexFor("unown"), formContext, stagedActorRng, stagedActor) !=
        Pokerogue3DS::PokemonFormSelectionResult::Ok) return 95;
    PokerogueRngAdapter combinedActorRng;
    combinedActorRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    Pokerogue3DS::PokemonActorIdentity combinedActor{};
    if (Pokerogue3DS::generatePokemonActorIdentityAndForm(
            dexFor("unown"), 256, formContext, combinedActorRng, combinedActor) !=
        Pokerogue3DS::PokemonActorIdentityResult::Ok ||
        combinedActor.pokemonId != stagedActor.pokemonId ||
        combinedActor.abilityIndex != stagedActor.abilityIndex ||
        combinedActor.gender != stagedActor.gender || combinedActor.formId != stagedActor.formId) return 96;
    for (uint8_t i = 0; i < 6; ++i) if (combinedActor.ivs[i] != stagedActor.ivs[i]) return 97;
    const auto combinedRngState = combinedActorRng.state();
    const auto stagedRngState = stagedActorRng.state();
    if (combinedRngState.carry != stagedRngState.carry || combinedRngState.s0 != stagedRngState.s0 ||
        combinedRngState.s1 != stagedRngState.s1 || combinedRngState.s2 != stagedRngState.s2) return 98;

    PokerogueRngAdapter wildActorRng;
    wildActorRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    PokerogueRngAdapter stagedWildRng;
    stagedWildRng.sow(formSeed, sizeof(formSeed) / sizeof(formSeed[0]));
    PokemonFormSelectionContext wildContext{};
    wildContext.biomeId = "town";
    wildContext.timeOfDay = "DAY";
    wildContext.waveIndex = 1;
    PokemonActorIdentity wildActor{};
    if (Pokerogue3DS::generatePokemonActorForWildEncounter(
            dexFor("charizard"), 256, wildContext, wildActorRng, wildActor) !=
            Pokerogue3DS::PokemonActorIdentityResult::Ok ||
        !wildActor.initialTeraTypeResolved || wildActor.nature == PokemonNature::Unspecified) return 99;
    PokemonActorIdentity stagedWildActor{};
    if (Pokerogue3DS::generatePokemonActorIdentityAndForm(
            dexFor("charizard"), 256, wildContext, stagedWildRng, stagedWildActor) !=
        Pokerogue3DS::PokemonActorIdentityResult::Ok) return 100;
    Pokerogue3DS::generatePokemonActorNature(stagedWildActor, stagedWildRng);
    const auto* selectedWildForm = PokerogueContent::findFormById(stagedWildActor.formId);
    const char* wildSecondaryType = selectedWildForm ? selectedWildForm->type2 :
        PokerogueContent::findSpeciesByDex(dexFor("charizard"))->type2;
    const uint8_t wildTypeCount = wildSecondaryType && wildSecondaryType[0] ? 2 : 1;
    const uint8_t expectedTeraIndex = static_cast<uint8_t>(stagedWildRng.randSeedInt(wildTypeCount));
    const auto wildRngState = wildActorRng.state();
    const auto stagedWildRngState = stagedWildRng.state();
    if (wildActor.pokemonId != stagedWildActor.pokemonId ||
        wildActor.formId != stagedWildActor.formId || wildActor.nature != stagedWildActor.nature ||
        wildActor.initialTeraTypeIndex != expectedTeraIndex ||
        wildRngState.carry != stagedWildRngState.carry || wildRngState.s0 != stagedWildRngState.s0 ||
        wildRngState.s1 != stagedWildRngState.s1 || wildRngState.s2 != stagedWildRngState.s2) return 101;

    // Real pinned species and form learnsets feed a bounded native candidate
    // pool; test both the level gate and form-specific imported move records.
    Pokerogue3DS::PokemonLevelMoveCandidate learnset[128]{};
    std::size_t learnsetCount = 0;
    if (Pokerogue3DS::buildPokemonLevelMovePool(
            dexFor("pikachu"), "pikachu:gigantamax", 55, learnset, 128, learnsetCount) !=
            Pokerogue3DS::PokemonLevelMovePoolResult::Ok || learnsetCount < 5) return 102;
    bool hasGigantamaxLastMove = false;
    bool hasFutureLevelMove = false;
    for (std::size_t i = 0; i < learnsetCount; ++i) {
        if (learnset[i].moveId == 528 && learnset[i].sourceLevel == 55) hasGigantamaxLastMove = true;
    }
    if (Pokerogue3DS::buildPokemonLevelMovePool(
            dexFor("pikachu"), "pikachu:gigantamax", 20, learnset, 128, learnsetCount) !=
            Pokerogue3DS::PokemonLevelMovePoolResult::Ok) return 103;
    for (std::size_t i = 0; i < learnsetCount; ++i)
        if (learnset[i].sourceLevel > 20) hasFutureLevelMove = true;
    if (hasFutureLevelMove || !hasGigantamaxLastMove) return 104;
    if (Pokerogue3DS::buildPokemonLevelMovePool(
            dexFor("pikachu"), "charizard:base_0", 55, learnset, 128, learnsetCount) !=
            Pokerogue3DS::PokemonLevelMovePoolResult::InvalidForm || learnsetCount != 0) return 105;
    if (Pokerogue3DS::buildPokemonLevelMovePool(
            dexFor("charizard"), "charizard:base_0", 1, learnset, 128, learnsetCount) !=
        Pokerogue3DS::PokemonLevelMovePoolResult::Ok) return 106;
    // Current-species level-one reminders are excluded when the upstream
    // prevolution chain is present; Charmeleon's pool precedes Charizard.
    bool dragonClawReminderWeight = false;
    for (std::size_t i = 0; i < learnsetCount; ++i)
        if (learnset[i].moveId == 337 && learnset[i].weight == 50) dragonClawReminderWeight = true;
    if (dragonClawReminderWeight) return 107;
    if (PokerogueContent::findSpeciesByDex(dexFor("charizard"))->starterEligible ||
        !PokerogueContent::findSpeciesByDex(dexFor("pikachu"))->starterEligible ||
        !PokerogueContent::findSpeciesByDex(dexFor("bulbasaur"))->freshProfileStarter ||
        !PokerogueContent::findSpeciesByDex(dexFor("charmander"))->freshProfileStarter ||
        PokerogueContent::findSpeciesByDex(dexFor("pikachu"))->freshProfileStarter ||
        PokerogueContent::findSpeciesByDex(dexFor("pikachu"))->prevolutionDex != dexFor("pichu") ||
        PokerogueContent::findSpeciesByDex(dexFor("charizard"))->prevolutionDex != dexFor("charmeleon")) return 108;
    if (Pokerogue3DS::buildPokemonLevelMovePool(dexFor("pikachu"), nullptr, 200, learnset, 128, learnsetCount) !=
        Pokerogue3DS::PokemonLevelMovePoolResult::Ok || learnsetCount == 0) return 109;

    PokemonNature freshNature = PokemonNature::Unspecified;
    if (Pokerogue3DS::pokemonFreshProfileNature(dexFor("bulbasaur"), freshNature) !=
            Pokerogue3DS::PokemonFreshProfileResult::Ok ||
        PokerogueContent::findSpeciesByDex(dexFor("bulbasaur"))->freshProfileStarterOrdinal != 0 ||
        PokerogueContent::findSpeciesByDex(dexFor("charmander"))->freshProfileStarterOrdinal != 1 ||
        (freshNature != PokemonNature::Hardy && freshNature != PokemonNature::Docile &&
         freshNature != PokemonNature::Serious && freshNature != PokemonNature::Bashful &&
         freshNature != PokemonNature::Quirky) ||
        Pokerogue3DS::pokemonFreshProfileNature(dexFor("pikachu"), freshNature) !=
            Pokerogue3DS::PokemonFreshProfileResult::NotDefaultStarter) return 121;

    uint16_t starterMoves[4]{};
    uint8_t starterMoveCount = 0;
    if (Pokerogue3DS::selectPokemonStarterMoveset(dexFor("bulbasaur"), nullptr, 0,
            nullptr, 0, starterMoves, starterMoveCount) != Pokerogue3DS::PokemonStarterMovesetResult::Ok ||
        starterMoveCount != 3 || starterMoves[0] != 33 || starterMoves[1] != 45 || starterMoves[2] != 22) return 118;
    const uint16_t preferredStarterMoves[] = {202, 33};
    if (Pokerogue3DS::selectPokemonStarterMoveset(dexFor("bulbasaur"), nullptr, 1,
            preferredStarterMoves, 2, starterMoves, starterMoveCount) != Pokerogue3DS::PokemonStarterMovesetResult::Ok ||
        starterMoveCount != 4 || starterMoves[0] != 202 || starterMoves[1] != 33 ||
        starterMoves[2] != 45 || starterMoves[3] != 22) return 119;
    if (Pokerogue3DS::selectPokemonStarterMoveset(dexFor("bulbasaur"), nullptr, 0x10,
            nullptr, 0, starterMoves, starterMoveCount) != Pokerogue3DS::PokemonStarterMovesetResult::InvalidEggMoveMask) return 120;
    PokemonBattleInit freshStarterInput{};
    freshStarterInput.speciesDex = dexFor("bulbasaur");
    freshStarterInput.level = 5;
    freshStarterInput.abilityId = PokerogueContent::findSpeciesByDex(freshStarterInput.speciesDex)->ability1;
    freshStarterInput.gender = Pokerogue3DS::PokemonGender::Male;
    freshStarterInput.nature = freshNature;
    freshStarterInput.moveCount = starterMoveCount;
    for (uint8_t i = 0; i < 6; ++i) freshStarterInput.ivs[i] = 15;
    for (uint8_t i = 0; i < starterMoveCount; ++i) freshStarterInput.moveIds[i] = starterMoves[i];
    PokemonBattleState freshStarterState{};
    if (Pokerogue3DS::initializePokemonBattleState(freshStarterInput, freshStarterState) !=
            PokemonBattleInitResult::Ok || freshStarterState.hp == 0 ||
        freshStarterState.ivs[0] != 15 || freshStarterState.nature != freshNature ||
        freshStarterState.moveCount != 3 || freshStarterState.moves[0].moveId != 33 ||
        freshStarterState.moves[0].pp != PokerogueContent::findMoveById(33)->pp) return 122;

    // Real ability callbacks feed the move-generation adapter. Supported
    // declarative effects remain typed; unported callbacks fail closed.
    const auto* compoundEyes = PokerogueContent::findAbilityMovegenProfile(14);
    const auto* skillLink = PokerogueContent::findAbilityMovegenProfile(92);
    const auto* drizzle = PokerogueContent::findAbilityMovegenProfile(2);
    const auto* hustle = PokerogueContent::findAbilityMovegenProfile(55);
    const auto* analytic = PokerogueContent::findAbilityMovegenProfile(148);
    if (!compoundEyes || compoundEyes->flags != 0 || compoundEyes->accuracyMultiplier != 1.3 ||
        !compoundEyes->sourcePath || !compoundEyes->sourceSymbol || !compoundEyes->sourceHash) return 110;
    if (!skillLink || skillLink->flags != PokerogueContent::AbilityMovegenMaxMultiHit ||
        !skillLink->sourcePath || !skillLink->sourceSymbol || !skillLink->sourceHash) return 111;
    if (!drizzle || !(drizzle->flags & PokerogueContent::AbilityMovegenUnsupported)) return 112;
    if (!hustle || hustle->flags != PokerogueContent::AbilityMovegenHustle ||
        !analytic || analytic->flags != PokerogueContent::AbilityMovegenAnalytic) return 116;
    Pokerogue3DS::PokemonWildMoveRuntimeMetadata moveMetadata{};
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(33, 14, moveMetadata) ||
        moveMetadata.type == nullptr || moveMetadata.power.power != 40 ||
        moveMetadata.ability.accuracyMultiplier != 1.3) return 113;
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(292, 92, moveMetadata) ||
        !moveMetadata.ability.maxMultiHitHolderPresent || !moveMetadata.ability.maxMultiHitValue) return 114;
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(33, 55, moveMetadata) ||
        moveMetadata.ability.powerMultiplier != 1.5 ||
        moveMetadata.ability.accuracyMultiplier != 0.8) return 179;
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(55, 55, moveMetadata) ||
        moveMetadata.ability.powerMultiplier != 1.0 ||
        moveMetadata.ability.accuracyMultiplier != 1.0) return 180;
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(233, 148, moveMetadata) ||
        moveMetadata.ability.powerMultiplier != 1.3) return 181;
    if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(33, 148, moveMetadata) ||
        moveMetadata.ability.powerMultiplier != 1.0) return 182;
    if (Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(33, 2, moveMetadata) ||
        Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(33, 65535, moveMetadata)) return 115;
    const auto checkRealMoveset = [](uint16_t dex, uint16_t abilityId) {
        const auto* species = PokerogueContent::findSpeciesByDex(dex);
        if (!species || (abilityId != species->ability1 && abilityId != species->ability2 &&
                         abilityId != species->abilityHidden)) return false;
        PokemonBattleInit actorInput{};
        actorInput.speciesDex = dex;
        actorInput.level = 5;
        actorInput.abilityId = abilityId;
        actorInput.gender = PokemonGender::Male;
        PokemonBattleState actorState{};
        if (Pokerogue3DS::initializePokemonBattleState(actorInput, actorState) != PokemonBattleInitResult::Ok)
            return false;
        Pokerogue3DS::PokemonLevelMoveCandidate pool[128]{};
        std::size_t poolCount = 0;
        if (Pokerogue3DS::buildPokemonLevelMovePool(dex, nullptr, 5, pool, 128, poolCount) !=
                Pokerogue3DS::PokemonLevelMovePoolResult::Ok || poolCount == 0 || poolCount > 128) return false;
        Pokerogue3DS::PokemonWildMoveRuntimeMetadata candidates[128]{};
        for (std::size_t i = 0; i < poolCount; ++i)
            if (!Pokerogue3DS::buildPokemonWildMoveRuntimeMetadata(pool[i].moveId, abilityId, candidates[i])) return false;
        const uint16_t seed[] = {'m', 'o', 'v', static_cast<uint16_t>(dex)};
        PokerogueRngAdapter firstRng;
        firstRng.sow(seed, sizeof(seed) / sizeof(seed[0]));
        uint16_t firstMoves[4]{};
        uint8_t firstCount = 0;
        if (Pokerogue3DS::generateWildMovesetFromLearnset(dex, nullptr, 5,
                actorState.stats[1], actorState.stats[3], actorState.stats[2],
                species->type1, species->type2, candidates, poolCount,
                firstRng, firstMoves, firstCount) != Pokerogue3DS::PokemonWildMovesetResult::Ok ||
            firstCount == 0 || firstCount > 4) return false;
        PokerogueRngAdapter replayRng;
        replayRng.sow(seed, sizeof(seed) / sizeof(seed[0]));
        uint16_t replayMoves[4]{};
        uint8_t replayCount = 0;
        if (Pokerogue3DS::generateWildMovesetFromLearnset(dex, nullptr, 5,
                actorState.stats[1], actorState.stats[3], actorState.stats[2],
                species->type1, species->type2, candidates, poolCount,
                replayRng, replayMoves, replayCount) != Pokerogue3DS::PokemonWildMovesetResult::Ok ||
            replayCount != firstCount) return false;
        const auto firstState = firstRng.state();
        const auto replayState = replayRng.state();
        if (firstState.carry != replayState.carry || firstState.s0 != replayState.s0 ||
            firstState.s1 != replayState.s1 || firstState.s2 != replayState.s2) return false;
        for (uint8_t i = 0; i < firstCount; ++i) {
            if (firstMoves[i] != replayMoves[i] || !PokerogueContent::findMoveById(firstMoves[i])) return false;
            for (uint8_t j = 0; j < i; ++j) if (firstMoves[i] == firstMoves[j]) return false;
        }
        actorInput.moveCount = firstCount;
        for (uint8_t i = 0; i < firstCount; ++i) actorInput.moveIds[i] = firstMoves[i];
        PokemonBattleState battleReady{};
        if (Pokerogue3DS::initializePokemonBattleState(actorInput, battleReady) != PokemonBattleInitResult::Ok ||
            battleReady.moveCount != firstCount) return false;
        for (uint8_t i = 0; i < firstCount; ++i) {
            const auto* canonicalMove = PokerogueContent::findMoveById(firstMoves[i]);
            if (!canonicalMove || battleReady.moves[i].moveId != firstMoves[i] ||
                battleReady.moves[i].maxPp != canonicalMove->pp ||
                battleReady.moves[i].pp != battleReady.moves[i].maxPp) return false;
        }
        return true;
    };
    if (!checkRealMoveset(dexFor("bulbasaur"), 65) || !checkRealMoveset(dexFor("pikachu"), 9)) return 117;
    Pokerogue3DS::PokemonBattleState bossBoostState{};
    for (unsigned i = 0; i < 5; ++i) { bossBoostState.stats[i + 1] = 100; bossBoostState.statStages[i] = 6; }
    Pokerogue3DS::PokerogueRngAdapter bossBoostRng;
    const uint16_t bossSeed[] = {'b', 'o', 's', 's'};
    bossBoostRng.sow(bossSeed, 4);
    const auto beforeBoostRng = bossBoostRng.state();
    Pokerogue3DS::PokemonBossSegmentClearEvent clearEvent{};
    if (!Pokerogue3DS::planPokemonBossSegmentCleared(bossBoostState, 3, 2, 0, false, bossBoostRng, clearEvent) ||
        clearEvent.nextSegmentIndex != 0) return 475;
    const auto afterBoostRng = bossBoostRng.state();
    if (beforeBoostRng.carry != afterBoostRng.carry || beforeBoostRng.s0 != afterBoostRng.s0 ||
        beforeBoostRng.s1 != afterBoostRng.s1 || beforeBoostRng.s2 != afterBoostRng.s2) return 476;
    for (auto stages : clearEvent.statStages) if (stages) return 477;
    bossBoostState.statStages[0] = 0;
    if (!Pokerogue3DS::planPokemonBossSegmentCleared(bossBoostState, 3, 1, 1, false, bossBoostRng, clearEvent) ||
        clearEvent.nextSegmentIndex || clearEvent.statStages[0] != 2) return 478;
    if (!Pokerogue3DS::planPokemonBossSegmentCleared(bossBoostState, 5, 2, 0, true, bossBoostRng, clearEvent) ||
        clearEvent.nextSegmentIndex) return 479;
    for (auto stages : clearEvent.statStages) if (stages) return 480;
    const auto beforeInvalidBoost = bossBoostRng.state();
    if (Pokerogue3DS::planPokemonBossSegmentCleared(bossBoostState, 3, 3, 0, false, bossBoostRng, clearEvent)) return 481;
    const auto afterInvalidBoost = bossBoostRng.state();
    if (beforeInvalidBoost.s0 != afterInvalidBoost.s0 || beforeInvalidBoost.s1 != afterInvalidBoost.s1 ||
        beforeInvalidBoost.s2 != afterInvalidBoost.s2 || beforeInvalidBoost.carry != afterInvalidBoost.carry) return 482;
    bossBoostState.maxHp = bossBoostState.hp = 300;
    for (unsigned i = 0; i < 5; ++i) bossBoostState.statStages[i] = 6;
    Pokerogue3DS::PokemonBossState bossPhase{3, 2, true, false};
    Pokerogue3DS::PokemonBossDamagePolicy bossDamagePolicy{true, true, false};
    Pokerogue3DS::PokemonBossDamageEvent bossHit{};
    if (!Pokerogue3DS::applyPokemonBossDamage(bossBoostState, bossPhase, 500, bossDamagePolicy,
            bossBoostRng, bossHit) || bossBoostState.hp != 100 || bossPhase.segmentIndex != 0 ||
        bossHit.damageApplied != 200) return 483;
    if (!Pokerogue3DS::applyPokemonBossDamage(bossBoostState, bossPhase, 500, bossDamagePolicy,
            bossBoostRng, bossHit) || bossBoostState.hp != 1 || !bossHit.preventedFinalBossKo) return 484;
    bossPhase.classicFinalBossFirstPhase = false;
    if (!Pokerogue3DS::applyPokemonBossDamage(bossBoostState, bossPhase, 500, bossDamagePolicy,
            bossBoostRng, bossHit) || bossBoostState.hp || bossHit.damageApplied != 1) return 485;
    bossBoostState.hp = 300;
    bossPhase = {3, 2, false, true};
    bossDamagePolicy.ignoreSegments = true;
    if (!Pokerogue3DS::applyPokemonBossDamage(bossBoostState, bossPhase, 250, bossDamagePolicy,
            bossBoostRng, bossHit) || bossBoostState.hp != 50 || bossPhase.segmentIndex != 0) return 486;
    const auto beforeBossPhaseRng = bossBoostRng.state();
    bossDamagePolicy.damageCallbacksResolved = false;
    if (Pokerogue3DS::applyPokemonBossDamage(bossBoostState, bossPhase, 500, bossDamagePolicy,
            bossBoostRng, bossHit) || bossBoostState.hp != 50) return 487;
    const auto afterBossPhaseRng = bossBoostRng.state();
    if (beforeBossPhaseRng.s0 != afterBossPhaseRng.s0 || beforeBossPhaseRng.s1 != afterBossPhaseRng.s1 ||
        beforeBossPhaseRng.s2 != afterBossPhaseRng.s2 || beforeBossPhaseRng.carry != afterBossPhaseRng.carry) return 488;
    PokemonBattleInit shieldCommandInput{};
    shieldCommandInput.speciesDex = 1;
    shieldCommandInput.level = 50;
    shieldCommandInput.abilityId = 65;
    shieldCommandInput.gender = PokemonGender::Male;
    shieldCommandInput.moveCount = 1;
    shieldCommandInput.moveIds[0] = 33;
    PokemonBattleState shieldCommandActor{};
    if (Pokerogue3DS::initializePokemonBattleState(shieldCommandInput, shieldCommandActor) != PokemonBattleInitResult::Ok)
        return 509;
    auto shieldMoveUser = shieldCommandActor;
    auto shieldMoveTarget = shieldCommandActor;
    shieldMoveTarget.hp = 1;
    Pokerogue3DS::PokemonBossState moveBossState{3, 0, true, false};
    Pokerogue3DS::PokemonBossDamagePolicy moveBossPolicy{true, true, false};
    Pokerogue3DS::PokemonMoveActionResult shieldMoveEvent{};
    const auto initialShieldPp = shieldMoveUser.moves[0].pp;
    if (Pokerogue3DS::useStandardPokemonMove(shieldMoveUser, shieldMoveTarget, 0, false,
            actionRng, shieldMoveEvent, nullptr, nullptr, nullptr, nullptr, &moveBossState, &moveBossPolicy, &bossBoostRng) !=
            PokemonMoveActionStatus::Ok || shieldMoveTarget.hp != 1 || shieldMoveEvent.targetFainted ||
        shieldMoveUser.moves[0].pp != initialShieldPp - 1) return 489;
    moveBossPolicy.damageCallbacksResolved = false;
    const auto beforeRejectedShieldPp = shieldMoveUser.moves[0].pp;
    const auto beforeRejectedShieldRng = actionRng.state();
    if (Pokerogue3DS::useStandardPokemonMove(shieldMoveUser, shieldMoveTarget, 0, false,
            actionRng, shieldMoveEvent, nullptr, nullptr, nullptr, nullptr, &moveBossState, &moveBossPolicy, &bossBoostRng) !=
            PokemonMoveActionStatus::UnresolvedBoss || shieldMoveTarget.hp != 1 ||
        shieldMoveUser.moves[0].pp != beforeRejectedShieldPp) return 490;
    const auto afterRejectedShieldRng = actionRng.state();
    if (beforeRejectedShieldRng.carry != afterRejectedShieldRng.carry ||
        beforeRejectedShieldRng.s0 != afterRejectedShieldRng.s0 || beforeRejectedShieldRng.s1 != afterRejectedShieldRng.s1 ||
        beforeRejectedShieldRng.s2 != afterRejectedShieldRng.s2) return 491;
    // Damage and hit draws must be identical with and without shield boosts.
    auto streamUser = shieldCommandActor;
    auto streamTarget = shieldCommandActor;
    streamUser.moves[0].pp = streamUser.moves[0].maxPp;
    streamTarget.hp = static_cast<uint16_t>((static_cast<uint32_t>(streamTarget.maxHp) * 2 + 1) / 3 + 1);
    for (auto& stage : streamTarget.statStages) stage = 0;
    auto ordinaryUser = streamUser;
    auto ordinaryTarget = streamTarget;
    auto ordinaryRng = actionRng;
    auto shieldBattleRng = actionRng;
    auto shieldGlobalRng = bossBoostRng;
    const auto globalBefore = shieldGlobalRng.state();
    Pokerogue3DS::PokemonHitPolicy shieldHitPolicy{};
    shieldHitPolicy.resolved = shieldHitPolicy.bypassAccuracy = true;
    moveBossState = {3, 2, false, false};
    moveBossPolicy = {true, true, false};
    Pokerogue3DS::PokemonMoveActionResult ordinaryEvent{};
    if (Pokerogue3DS::useStandardPokemonMove(ordinaryUser, ordinaryTarget, 0, false,
            ordinaryRng, ordinaryEvent, nullptr, nullptr, &shieldHitPolicy) != PokemonMoveActionStatus::Ok ||
        Pokerogue3DS::useStandardPokemonMove(streamUser, streamTarget, 0, false,
            shieldBattleRng, shieldMoveEvent, nullptr, nullptr, &shieldHitPolicy, nullptr,
            &moveBossState, &moveBossPolicy, &shieldGlobalRng) != PokemonMoveActionStatus::Ok ||
        moveBossState.segmentIndex >= 2) return 501;
    const auto ordinaryAfter = ordinaryRng.state();
    const auto battleAfter = shieldBattleRng.state();
    const auto globalAfter = shieldGlobalRng.state();
    if (ordinaryAfter.s0 != battleAfter.s0 || ordinaryAfter.s1 != battleAfter.s1 ||
        ordinaryAfter.s2 != battleAfter.s2 || ordinaryAfter.carry != battleAfter.carry ||
        (globalBefore.s0 == globalAfter.s0 && globalBefore.s1 == globalAfter.s1 &&
         globalBefore.s2 == globalAfter.s2 && globalBefore.carry == globalAfter.carry)) return 502;
    const auto ppBeforeMissingGlobal = streamUser.moves[0].pp;
    if (Pokerogue3DS::useStandardPokemonMove(streamUser, streamTarget, 0, false,
            shieldBattleRng, shieldMoveEvent, nullptr, nullptr, &shieldHitPolicy, nullptr,
            &moveBossState, &moveBossPolicy) != PokemonMoveActionStatus::UnresolvedBoss ||
        streamUser.moves[0].pp != ppBeforeMissingGlobal) return 503;
    if (Pokerogue3DS::useStandardPokemonMove(streamUser, streamTarget, 0, false,
            shieldBattleRng, shieldMoveEvent, nullptr, nullptr, &shieldHitPolicy, nullptr,
            &moveBossState, &moveBossPolicy, &shieldBattleRng) != PokemonMoveActionStatus::UnresolvedBoss ||
        streamUser.moves[0].pp != ppBeforeMissingGlobal) return 504;
    Pokerogue3DS::PokemonBossState initializedBoss{};
    if (!Pokerogue3DS::initializeClassicPokemonBossState(1, 5, 1, false, false, initializedBoss) ||
        initializedBoss.segmentCount) return 492;
    if (!Pokerogue3DS::initializeClassicPokemonBossState(1, 99, 10, false, false, initializedBoss) ||
        initializedBoss.segmentCount != 2 || initializedBoss.segmentIndex != 1) return 493;
    if (!Pokerogue3DS::initializeClassicPokemonBossState(1, 100, 10, false, false, initializedBoss) ||
        initializedBoss.segmentCount != 3) return 494;
    const PokerogueContent::Species* eternatus = nullptr;
    for (const auto& species : PokerogueContent::kSpecies)
        if (std::strcmp(species.id, "eternatus") == 0) eternatus = &species;
    if (!eternatus || !Pokerogue3DS::initializeClassicPokemonBossState(eternatus->dex, 200, 200,
            true, true, initializedBoss) || initializedBoss.segmentCount != 4 ||
        !initializedBoss.classicFinalBossFirstPhase) return 495;
    if (Pokerogue3DS::initializeClassicPokemonBossState(65535, 5, 1, false, false, initializedBoss) ||
        initializedBoss.segmentCount != 4) return 496;
    PokemonBattleInit weatherBossInput{};
    weatherBossInput.speciesDex = eternatus->dex;
    weatherBossInput.level = 200;
    weatherBossInput.abilityId = eternatus->ability1;
    PokemonBattleState weatherBossActor{};
    if (Pokerogue3DS::initializePokemonBattleState(weatherBossInput, weatherBossActor) != PokemonBattleInitResult::Ok)
        return 505;
    weatherBossActor.hp = static_cast<uint16_t>((static_cast<uint32_t>(weatherBossActor.maxHp) * 4 + 4) / 5 + 1);
    Pokerogue3DS::PokemonBossState weatherBossState{5, 4, false, false};
    auto weatherPlayer = phasePlayer;
    auto residualRng = bossBoostRng;
    const auto residualBefore = residualRng.state();
    Pokerogue3DS::PokemonWeatherPhaseEvent bossWeatherEvent{};
    if (!Pokerogue3DS::applyPokemonMultiWeatherPhase(weatherPlayer, weatherBossActor, nullptr,
            phaseWeather, false, bossWeatherEvent, &weatherBossState, nullptr, &residualRng) ||
        weatherBossState.segmentIndex != 3 || !bossWeatherEvent.enemy.damageApplied) return 506;
    const auto residualAfter = residualRng.state();
    if (residualBefore.s0 == residualAfter.s0 && residualBefore.s1 == residualAfter.s1 &&
        residualBefore.s2 == residualAfter.s2 && residualBefore.carry == residualAfter.carry) return 507;
    const uint16_t weatherBossHp = weatherBossActor.hp;
    const uint16_t weatherPlayerHp = weatherPlayer.hp;
    const uint16_t weatherBossIndex = weatherBossState.segmentIndex;
    if (Pokerogue3DS::applyPokemonMultiWeatherPhase(weatherPlayer, weatherBossActor, nullptr,
            phaseWeather, false, bossWeatherEvent, &weatherBossState) ||
        weatherBossActor.hp != weatherBossHp || weatherPlayer.hp != weatherPlayerHp ||
        weatherBossState.segmentIndex != weatherBossIndex) return 508;
    Pokerogue3DS::PokemonBossState firstDoubleBoss{3, 2, false, false};
    Pokerogue3DS::PokemonBossState secondDoubleBoss{4, 3, false, false};
    if (!Pokerogue3DS::distributePokemonDoubleBossSegments(firstDoubleBoss, 300, secondDoubleBoss, 700) ||
        firstDoubleBoss.segmentCount != 1 || firstDoubleBoss.segmentIndex != 0 ||
        secondDoubleBoss.segmentCount != 3 || secondDoubleBoss.segmentIndex != 2) return 497;
    firstDoubleBoss = {3, 2, false, false};
    secondDoubleBoss = {3, 2, false, false};
    if (!Pokerogue3DS::distributePokemonDoubleBossSegments(firstDoubleBoss, 500, secondDoubleBoss, 500) ||
        firstDoubleBoss.segmentCount != 2 || secondDoubleBoss.segmentCount != 2) return 498;
    if (Pokerogue3DS::distributePokemonDoubleBossSegments(firstDoubleBoss, 0, secondDoubleBoss, 500) ||
        firstDoubleBoss.segmentCount != 2 || secondDoubleBoss.segmentCount != 2) return 499;
    secondDoubleBoss = {};
    if (!Pokerogue3DS::distributePokemonDoubleBossSegments(firstDoubleBoss, 500, secondDoubleBoss, 500) ||
        firstDoubleBoss.segmentCount != 2 || secondDoubleBoss.segmentCount) return 500;
    Pokerogue3DS::PokemonBossSegmentDamage bossDamage{};
    if (!Pokerogue3DS::calculatePokemonBossSegmentDamage(99, 300, 300, 3, 2, 0, bossDamage) ||
        bossDamage.adjustedDamage != 99 || bossDamage.clearedSegmentIndex != 3) return 468;
    if (!Pokerogue3DS::calculatePokemonBossSegmentDamage(100, 300, 300, 3, 2, 0, bossDamage) ||
        bossDamage.adjustedDamage != 100 || bossDamage.clearedSegmentIndex != 2) return 469;
    if (!Pokerogue3DS::calculatePokemonBossSegmentDamage(150, 300, 300, 3, 2, 0, bossDamage) ||
        bossDamage.adjustedDamage != 100 || bossDamage.clearedSegmentIndex != 2) return 470;
    if (!Pokerogue3DS::calculatePokemonBossSegmentDamage(500, 300, 300, 3, 2, 0, bossDamage) ||
        bossDamage.adjustedDamage != 300 || bossDamage.clearedSegmentIndex != 0) return 471;
    if (!Pokerogue3DS::calculatePokemonBossSegmentDamage(500, 300, 300, 3, 2, 1, bossDamage) ||
        bossDamage.adjustedDamage != 200 || bossDamage.clearedSegmentIndex != 1) return 472;
    if (!Pokerogue3DS::calculatePokemonBossSegmentDamage(35, 101, 101, 3, 2, 0, bossDamage) ||
        bossDamage.adjustedDamage != 33 || bossDamage.clearedSegmentIndex != 2) return 473;
    const auto beforeBossDamage = bossDamage;
    if (Pokerogue3DS::calculatePokemonBossSegmentDamage(1, 1, 0, 3, 2, 0, bossDamage) ||
        Pokerogue3DS::calculatePokemonBossSegmentDamage(1, 1, 300, 3, 1, 2, bossDamage) ||
        bossDamage.adjustedDamage != beforeBossDamage.adjustedDamage ||
        bossDamage.clearedSegmentIndex != beforeBossDamage.clearedSegmentIndex) return 474;
    const auto* eternatus = PokerogueContent::findSpeciesByDex(890);
    const auto* eternamax = PokerogueContent::findFormById("eternatus:eternamax");
    if (!eternatus || !eternamax) return 510;
    Pokerogue3DS::PokemonBattleInit finalInput{};
    finalInput.speciesDex = eternatus->dex;
    finalInput.formId = eternatus->firstFormId;
    finalInput.level = 200;
    finalInput.pokemonId = 0x12345678u;
    finalInput.deriveIvsFromPokemonId = true;
    finalInput.abilityId = eternatus->ability1;
    finalInput.gender = Pokerogue3DS::PokemonGender::Genderless;
    finalInput.nature = Pokerogue3DS::PokemonNature::Hardy;
    finalInput.moveCount = 1;
    finalInput.moveIds[0] = 33;
    Pokerogue3DS::PokemonBattleState finalActor{};
    if (Pokerogue3DS::initializePokemonBattleState(finalInput, finalActor) !=
            Pokerogue3DS::PokemonBattleInitResult::Ok) return 511;
    finalActor.hp = 1;
    finalActor.moves[0].pp = 0;
    finalActor.statStages[0] = 2;
    finalActor.pauseEvolutions = true;
    const auto beforeFormChange = finalActor;
    if (!Pokerogue3DS::changePokemonBattleForm(finalActor, eternamax->id, eternamax->ability1, true) ||
        finalActor.maxHp <= beforeFormChange.maxHp || finalActor.hp != finalActor.maxHp ||
        finalActor.moves[0].pp != finalActor.moves[0].maxPp ||
        finalActor.pokemonId != beforeFormChange.pokemonId || !finalActor.ivsWereDerivedFromPokemonId ||
        finalActor.statStages[0] != 2 || !finalActor.pauseEvolutions) return 512;
    for (uint8_t iv = 0; iv < 6; ++iv)
        if (finalActor.ivs[iv] != beforeFormChange.ivs[iv]) return 513;
    const auto preservedFinalActor = finalActor;
    if (Pokerogue3DS::changePokemonBattleForm(finalActor, PokerogueContent::findSpeciesByDex(1)->firstFormId, eternamax->ability1, true) ||
        finalActor.hp != preservedFinalActor.hp || finalActor.formId != preservedFinalActor.formId) return 514;
    auto invalidFormActor = finalActor;
    invalidFormActor.statStages[0] = 7;
    if (Pokerogue3DS::changePokemonBattleForm(invalidFormActor, eternatus->firstFormId, eternatus->ability1) ||
        invalidFormActor.statStages[0] != 7 || invalidFormActor.formId != finalActor.formId) return 516;
    invalidFormActor = finalActor;
    ++invalidFormActor.moves[0].maxPp;
    if (Pokerogue3DS::changePokemonBattleForm(invalidFormActor, eternatus->firstFormId, eternatus->ability1, true) ||
        invalidFormActor.moves[0].maxPp != finalActor.moves[0].maxPp + 1 ||
        invalidFormActor.formId != finalActor.formId) return 517;
    finalActor.hp = 0;
    finalActor.moves[0].pp = 1;
    auto statusFormActor = finalActor;
    statusFormActor.status.present = true;
    statusFormActor.status.effect = Pokerogue3DS::PokemonStatusEffect::Burn;
    statusFormActor.pendingStatus = Pokerogue3DS::PokemonStatusEffect::Poison;
    statusFormActor.confusion = {3, true};
    if (!Pokerogue3DS::changePokemonBattleForm(statusFormActor, eternatus->firstFormId, eternatus->ability1) ||
        !statusFormActor.status.present || statusFormActor.status.effect != Pokerogue3DS::PokemonStatusEffect::Burn ||
        statusFormActor.pendingStatus != Pokerogue3DS::PokemonStatusEffect::Poison ||
        !statusFormActor.confusion.present || statusFormActor.confusion.turns != 3) return 9162;
    if (!Pokerogue3DS::changePokemonBattleForm(finalActor, eternatus->firstFormId, eternatus->ability1) ||
        finalActor.hp || finalActor.moves[0].pp != 1) return 515;
    auto unburdenActor = finalActor;
    unburdenActor.abilityId = 84;
    Pokerogue3DS::PokemonMoveWeatherContext unburdenWeather{};
    unburdenWeather.resolved = true;
    uint32_t beforeUnburden = 0, afterUnburden = 0;
    if (!Pokerogue3DS::pokemonWeatherEffectiveSpeed(unburdenActor, unburdenWeather, beforeUnburden)) return 518;
    unburdenActor.heldItemLostTags.unburden = true;
    if (!Pokerogue3DS::pokemonWeatherEffectiveSpeed(unburdenActor, unburdenWeather, afterUnburden) ||
        afterUnburden != beforeUnburden * 2) return 519;
    unburdenActor.abilityId = 46;
    if (!Pokerogue3DS::pokemonWeatherEffectiveSpeed(unburdenActor, unburdenWeather, afterUnburden) ||
        afterUnburden != beforeUnburden) return 520;
    auto levelActor = unburdenActor;
    levelActor.hp = levelActor.maxHp - 1;
    levelActor.friendship = 173;
    levelActor.turnDamageDealt = 19;
    levelActor.pauseEvolutions = true;
    levelActor.statStages[0] = 2;
    const auto beforeLevel = levelActor;
    if (!Pokerogue3DS::recalculatePokemonBattleLevel(levelActor, levelActor.level + 1) ||
        levelActor.hp != levelActor.maxHp - 1 ||
        levelActor.heldItemLostTags.unburden != beforeLevel.heldItemLostTags.unburden ||
        levelActor.friendship != 173 ||
        levelActor.turnDamageDealt != 19 || !levelActor.pauseEvolutions ||
        levelActor.statStages[0] != 2 || levelActor.pokemonId != beforeLevel.pokemonId ||
        levelActor.moves[0].pp != beforeLevel.moves[0].pp ||
        levelActor.moves[0].maxPp != beforeLevel.moves[0].maxPp) return 522;
    levelActor.hp = 0;
    if (!Pokerogue3DS::recalculatePokemonBattleLevel(levelActor, levelActor.level + 1) ||
        levelActor.hp) return 523;
    const auto validLevel = levelActor.level;
    if (Pokerogue3DS::recalculatePokemonBattleLevel(levelActor, 0) ||
        levelActor.level != validLevel || levelActor.turnDamageDealt != 19) return 524;
    using Conclusion = Pokerogue3DS::PokemonBattleConclusion;
    // Includes simultaneous faint with/without a living reserve. Faint itself
    // does not make a reserve a participant or invent an EXP award.
    if (Pokerogue3DS::pokemonBattleConclusion(false, false) != Conclusion::Continue) return 555;
    if (Pokerogue3DS::pokemonBattleConclusion(false, true) != Conclusion::PlayerVictory) return 556;
    if (Pokerogue3DS::pokemonBattleConclusion(true, false) != Conclusion::PlayerDefeat) return 557;
    if (Pokerogue3DS::pokemonBattleConclusion(true, true) != Conclusion::PlayerDefeat) return 558;
    auto recalledActor = beforeLevel;
    const auto persistentActor = recalledActor;
    recalledActor.heldItemLostTags.unburden = true;
    recalledActor.turnDamageDealt = 19;
    recalledActor.confusion = {3, true};
    for (auto& stage : recalledActor.statStages) stage = 3;
    Pokerogue3DS::resetPokemonSummonState(recalledActor);
    for (auto stage : recalledActor.statStages) if (stage) return 565;
    if (recalledActor.confusion.present || recalledActor.confusion.turns) return 9160;
    if (recalledActor.heldItemLostTags.unburden || recalledActor.turnDamageDealt ||
        recalledActor.hp != persistentActor.hp || recalledActor.maxHp != persistentActor.maxHp ||
        recalledActor.friendship != persistentActor.friendship ||
        recalledActor.pauseEvolutions != persistentActor.pauseEvolutions ||
        recalledActor.pokemonId != persistentActor.pokemonId || recalledActor.level != persistentActor.level ||
        recalledActor.formId != persistentActor.formId) return 566;
    for (uint8_t slot = 0; slot < recalledActor.moveCount; ++slot)
        if (recalledActor.moves[slot].moveId != persistentActor.moves[slot].moveId ||
            recalledActor.moves[slot].pp != persistentActor.moves[slot].pp ||
            recalledActor.moves[slot].maxPp != persistentActor.moves[slot].maxPp) return 567;
    Pokerogue3DS::resetPokemonSummonState(recalledActor);
    if (recalledActor.hp != persistentActor.hp || recalledActor.turnDamageDealt) return 568;
    Pokerogue3DS::PokemonStatusState status{};
    if (Pokerogue3DS::incrementPokemonStatusTurn(status) != Pokerogue3DS::PokemonStatusTickResult::NoStatus) return 569;
    status.present = true;
    status.effect = Pokerogue3DS::PokemonStatusEffect::Sleep;
    status.hasSleepTurnsRemaining = true;
    status.sleepTurnsRemaining = 2;
    if (Pokerogue3DS::incrementPokemonStatusTurn(status) != Pokerogue3DS::PokemonStatusTickResult::Ok ||
        status.toxicTurnCount != 1 || status.sleepTurnsRemaining != 1 ||
        Pokerogue3DS::pokemonStatusCatchRateMultiplier(status) != 2.5 || Pokerogue3DS::pokemonStatusIsPostTurn(status)) return 570;
    if (Pokerogue3DS::incrementPokemonStatusTurn(status) != Pokerogue3DS::PokemonStatusTickResult::Ok ||
        status.sleepTurnsRemaining || Pokerogue3DS::incrementPokemonStatusTurn(status) != Pokerogue3DS::PokemonStatusTickResult::Ok ||
        status.sleepTurnsRemaining || status.toxicTurnCount != 3) return 571;
    status.toxicTurnCount = UINT32_MAX;
    if (Pokerogue3DS::incrementPokemonStatusTurn(status) != Pokerogue3DS::PokemonStatusTickResult::CounterOverflow ||
        status.toxicTurnCount != UINT32_MAX) return 572;
    status = {};
    status.present = true;
    status.effect = Pokerogue3DS::PokemonStatusEffect::Toxic;
    if (!Pokerogue3DS::pokemonStatusIsPostTurn(status) || Pokerogue3DS::pokemonStatusCatchRateMultiplier(status) != 1.5) return 573;
    status.sleepTurnsRemaining = 1;
    if (Pokerogue3DS::incrementPokemonStatusTurn(status) != Pokerogue3DS::PokemonStatusTickResult::InvalidStatus ||
        status.toxicTurnCount || status.sleepTurnsRemaining != 1) return 574;
    PokemonBattleState residualActor{};
    residualActor.maxHp = residualActor.hp = 101;
    residualActor.status.present = true;
    residualActor.status.effect = Pokerogue3DS::PokemonStatusEffect::Toxic;
    Pokerogue3DS::PokemonStatusResidualPolicy residualPolicy{};
    Pokerogue3DS::PokemonStatusResidualEvent residualEvent{};
    if (Pokerogue3DS::applyPokemonStatusResidual(residualActor, residualPolicy, residualEvent) !=
            Pokerogue3DS::PokemonStatusResidualResult::UnsupportedPolicy || residualActor.status.toxicTurnCount) return 575;
    residualPolicy.resolved = true;
    if (Pokerogue3DS::applyPokemonStatusResidual(residualActor, residualPolicy, residualEvent) !=
            Pokerogue3DS::PokemonStatusResidualResult::Applied || residualEvent.requestedDamage != 6 ||
        residualActor.hp != 95 || residualActor.status.toxicTurnCount != 1) return 576;
    residualPolicy.blockStatusDamage = true;
    if (Pokerogue3DS::applyPokemonStatusResidual(residualActor, residualPolicy, residualEvent) !=
            Pokerogue3DS::PokemonStatusResidualResult::Blocked || residualActor.hp != 95 ||
        residualActor.status.toxicTurnCount != 2 || !residualEvent.blocked) return 577;
    residualPolicy.blockStatusDamage = false;
    residualActor.status.effect = Pokerogue3DS::PokemonStatusEffect::Burn;
    residualPolicy.burnMultiplierDenominator = 2;
    if (Pokerogue3DS::applyPokemonStatusResidual(residualActor, residualPolicy, residualEvent) !=
            Pokerogue3DS::PokemonStatusResidualResult::Applied || residualEvent.requestedDamage != 3) return 578;
    residualActor.maxHp = 1;
    residualActor.hp = 1;
    if (Pokerogue3DS::applyPokemonStatusResidual(residualActor, residualPolicy, residualEvent) !=
            Pokerogue3DS::PokemonStatusResidualResult::Applied || residualEvent.appliedDamage != 1 ||
        !residualEvent.fainted || residualActor.hp) return 579;
    Pokerogue3DS::PokemonStatusApplicationPolicy statusPolicy{};
    Pokerogue3DS::PokemonStatusState absentStatus{};
    using Effect = Pokerogue3DS::PokemonStatusEffect;
    using Eligibility = Pokerogue3DS::PokemonStatusEligibility;
    if (Pokerogue3DS::canPokemonSetStatus(absentStatus, Effect::Burn, statusPolicy) != Eligibility::UnsupportedPolicy) return 580;
    statusPolicy.resolved = true;
    statusPolicy.poisonType = statusPolicy.steelType = true;
    statusPolicy.hasSource = statusPolicy.sourceIgnoresPoisonImmunity = true;
    if (Pokerogue3DS::canPokemonSetStatus(absentStatus, Effect::Toxic, statusPolicy) != Eligibility::SteelType) return 581;
    statusPolicy.sourceIgnoresSteelImmunity = true;
    if (Pokerogue3DS::canPokemonSetStatus(absentStatus, Effect::Toxic, statusPolicy) != Eligibility::Allowed) return 582;
    statusPolicy.hasSource = false;
    if (Pokerogue3DS::canPokemonSetStatus(absentStatus, Effect::Toxic, statusPolicy) != Eligibility::PoisonType) return 583;
    statusPolicy = {};
    statusPolicy.resolved = statusPolicy.grounded = statusPolicy.mistyTerrain = true;
    if (Pokerogue3DS::canPokemonSetStatus(absentStatus, Effect::Burn, statusPolicy) != Eligibility::MistyTerrain) return 584;
    statusPolicy.ignoreField = true;
    statusPolicy.electricTerrain = true;
    if (Pokerogue3DS::canPokemonSetStatus(absentStatus, Effect::Sleep, statusPolicy) != Eligibility::ElectricTerrain) return 585;
    statusPolicy = {};
    statusPolicy.resolved = statusPolicy.sunnyOrHarshSun = true;
    if (Pokerogue3DS::canPokemonSetStatus(absentStatus, Effect::Freeze, statusPolicy) != Eligibility::SunnyWeather) return 586;
    statusPolicy.ignoreField = true;
    if (Pokerogue3DS::canPokemonSetStatus(absentStatus, Effect::Freeze, statusPolicy) != Eligibility::Allowed) return 587;
    absentStatus.present = true;
    absentStatus.effect = Effect::Poison;
    statusPolicy.overrideStatus = true;
    statusPolicy.pendingStatus = true;
    if (Pokerogue3DS::canPokemonSetStatus(absentStatus, Effect::Poison, statusPolicy) != Eligibility::ExistingStatus ||
        Pokerogue3DS::canPokemonSetStatus(absentStatus, Effect::Sleep, statusPolicy) != Eligibility::Allowed) return 588;
    PokemonBattleState obtainActor{};
    obtainActor.hp = obtainActor.maxHp = 20;
    Pokerogue3DS::PokerogueRngAdapter obtainRng, expectedStatusRng;
    const uint32_t expectedSleep = expectedStatusRng.randSeedInt(3) == 0 ? 2 : 3;
    statusPolicy = {};
    statusPolicy.resolved = true;
    if (Pokerogue3DS::obtainPokemonStatus(obtainActor, Effect::Sleep, statusPolicy, false, obtainRng) !=
            Pokerogue3DS::PokemonStatusObtainResult::UnsupportedReactions || obtainActor.status.present) return 589;
    if (Pokerogue3DS::obtainPokemonStatus(obtainActor, Effect::Sleep, statusPolicy, true, obtainRng) !=
            Pokerogue3DS::PokemonStatusObtainResult::Applied || obtainActor.status.sleepTurnsRemaining != expectedSleep ||
        obtainRng.randSeedUint32() != expectedStatusRng.randSeedUint32()) return 590;
    obtainActor.status = {};
    if (Pokerogue3DS::obtainPokemonStatus(obtainActor, Effect::Freeze, statusPolicy, true, obtainRng) !=
            Pokerogue3DS::PokemonStatusObtainResult::Applied || obtainActor.status.freezeTurnsRemaining != 3 ||
        obtainActor.status.sleepTurnsRemaining || !obtainActor.status.hasSleepTurnsRemaining) return 591;
    Pokerogue3DS::PokemonStatusMoveCheckPolicy moveStatusPolicy{};
    Pokerogue3DS::PokemonStatusMoveCheckEvent moveStatusEvent{};
    Pokerogue3DS::PokemonStatusState checkStatus{};
    checkStatus.present = true;
    checkStatus.effect = Effect::Sleep;
    checkStatus.hasSleepTurnsRemaining = true;
    checkStatus.sleepTurnsRemaining = 2;
    Pokerogue3DS::PokerogueRngAdapter checkRng, unchangedCheckRng;
    if (Pokerogue3DS::checkPokemonStatusBeforeMove(checkStatus, moveStatusPolicy, checkRng, moveStatusEvent) !=
            Pokerogue3DS::PokemonStatusMoveCheckResult::UnsupportedPolicy || checkStatus.sleepTurnsRemaining != 2) return 592;
    moveStatusPolicy.resolved = true;
    if (Pokerogue3DS::checkPokemonStatusBeforeMove(checkStatus, moveStatusPolicy, checkRng, moveStatusEvent) !=
            Pokerogue3DS::PokemonStatusMoveCheckResult::Ok || !moveStatusEvent.cancelled || moveStatusEvent.cured ||
        checkStatus.sleepTurnsRemaining != 1 || checkStatus.toxicTurnCount != 1) return 593;
    if (Pokerogue3DS::checkPokemonStatusBeforeMove(checkStatus, moveStatusPolicy, checkRng, moveStatusEvent) !=
            Pokerogue3DS::PokemonStatusMoveCheckResult::Ok || moveStatusEvent.cancelled || !moveStatusEvent.cured ||
        checkStatus.present || checkRng.randSeedUint32() != unchangedCheckRng.randSeedUint32()) return 594;
    checkStatus = {};
    checkStatus.present = true;
    checkStatus.effect = Effect::Freeze;
    checkStatus.hasFreezeTurnsRemaining = true;
    checkStatus.freezeTurnsRemaining = 1;
    Pokerogue3DS::PokerogueRngAdapter freezeCheckRng, expectedFreezeCheckRng;
    expectedFreezeCheckRng.randSeedInt(4); // Draw still occurs on guaranteed expiry.
    if (Pokerogue3DS::checkPokemonStatusBeforeMove(checkStatus, moveStatusPolicy, freezeCheckRng, moveStatusEvent) !=
            Pokerogue3DS::PokemonStatusMoveCheckResult::Ok || !moveStatusEvent.cured || checkStatus.present ||
        freezeCheckRng.randSeedUint32() != expectedFreezeCheckRng.randSeedUint32()) return 595;
    checkStatus = {};
    checkStatus.present = true;
    checkStatus.effect = Effect::Paralysis;
    Pokerogue3DS::PokerogueRngAdapter paraCheckRng, expectedParaCheckRng;
    const bool expectedCancelled = expectedParaCheckRng.randSeedInt(8) == 0;
    if (Pokerogue3DS::checkPokemonStatusBeforeMove(checkStatus, moveStatusPolicy, paraCheckRng, moveStatusEvent) !=
            Pokerogue3DS::PokemonStatusMoveCheckResult::Ok || moveStatusEvent.cancelled != expectedCancelled ||
        checkStatus.toxicTurnCount || !checkStatus.present ||
        paraCheckRng.randSeedUint32() != expectedParaCheckRng.randSeedUint32()) return 596;
    checkStatus.effect = Effect::Sleep;
    checkStatus.hasSleepTurnsRemaining = true;
    checkStatus.sleepTurnsRemaining = 3;
    moveStatusPolicy.bypassSleep = true;
    if (Pokerogue3DS::checkPokemonStatusBeforeMove(checkStatus, moveStatusPolicy, paraCheckRng, moveStatusEvent) !=
            Pokerogue3DS::PokemonStatusMoveCheckResult::Ok || moveStatusEvent.cancelled ||
        checkStatus.sleepTurnsRemaining != 2 || !checkStatus.present) return 597;
    moveStatusPolicy.indirectSleepWake = true;
    if (Pokerogue3DS::checkPokemonStatusBeforeMove(checkStatus, moveStatusPolicy, paraCheckRng, moveStatusEvent) !=
            Pokerogue3DS::PokemonStatusMoveCheckResult::Ok || !moveStatusEvent.cured || checkStatus.present) return 598;
    moveStatusPolicy = {};
    moveStatusPolicy.resolved = true;
    moveStatusPolicy.deferredFreezeThawMove = true;
    checkStatus = {};
    checkStatus.present = true;
    checkStatus.effect = Effect::Freeze;
    checkStatus.hasFreezeTurnsRemaining = true;
    checkStatus.freezeTurnsRemaining = 3;
    auto expectedThawRng = paraCheckRng;
    if (Pokerogue3DS::checkPokemonStatusBeforeMove(checkStatus, moveStatusPolicy, paraCheckRng, moveStatusEvent) !=
            Pokerogue3DS::PokemonStatusMoveCheckResult::Ok || moveStatusEvent.cancelled || moveStatusEvent.cured ||
        !moveStatusEvent.thawAfterFailureChecks || !checkStatus.present || checkStatus.freezeTurnsRemaining != 3 ||
        checkStatus.toxicTurnCount || paraCheckRng.randSeedUint32() != expectedThawRng.randSeedUint32()) return 9006;
    moveStatusPolicy.indirectFreezeWake = true;
    expectedThawRng = paraCheckRng;
    if (Pokerogue3DS::checkPokemonStatusBeforeMove(checkStatus, moveStatusPolicy, paraCheckRng, moveStatusEvent) !=
            Pokerogue3DS::PokemonStatusMoveCheckResult::Ok || !moveStatusEvent.cured || checkStatus.present ||
        moveStatusEvent.thawAfterFailureChecks ||
        paraCheckRng.randSeedUint32() != expectedThawRng.randSeedUint32()) return 9007;
    moveStatusPolicy.indirectFreezeWake = moveStatusPolicy.deferredFreezeThawMove = false;
    moveStatusPolicy.freezeCureAfterIncrement = true;
    checkStatus.present = true;
    checkStatus.effect = Effect::Freeze;
    checkStatus.hasFreezeTurnsRemaining = true;
    checkStatus.freezeTurnsRemaining = 3;
    expectedThawRng = paraCheckRng;
    if (Pokerogue3DS::checkPokemonStatusBeforeMove(checkStatus, moveStatusPolicy, paraCheckRng, moveStatusEvent) !=
            Pokerogue3DS::PokemonStatusMoveCheckResult::Ok || !moveStatusEvent.cured || checkStatus.present ||
        paraCheckRng.randSeedUint32() != expectedThawRng.randSeedUint32()) return 9008;
    bool foundSleepReduction = false;
    for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles) {
        if (!profile.resolved || !profile.sleepReduction) continue;
        foundSleepReduction = true;
        moveStatusPolicy = {};
        moveStatusPolicy.resolved = true;
        moveStatusPolicy.sleepDurationReduction = profile.sleepReduction;
        checkStatus = {};
        checkStatus.present = true;
        checkStatus.effect = Effect::Sleep;
        checkStatus.hasSleepTurnsRemaining = true;
        checkStatus.sleepTurnsRemaining = 2;
        if (Pokerogue3DS::checkPokemonStatusBeforeMove(checkStatus, moveStatusPolicy, paraCheckRng, moveStatusEvent) !=
                Pokerogue3DS::PokemonStatusMoveCheckResult::Ok || !moveStatusEvent.cured ||
            moveStatusEvent.cancelled || checkStatus.present) return 9009;
    }
    if (!foundSleepReduction) return 9010;
    PokemonBattleState paralyzedSpeed{};
    paralyzedSpeed.stats[5] = 101;
    paralyzedSpeed.status.present = true;
    paralyzedSpeed.status.effect = Effect::Paralysis;
    uint32_t statusSpeed = 0;
    if (!Pokerogue3DS::pokemonBaselineEffectiveStat(paralyzedSpeed, 5, false, statusSpeed) || statusSpeed != 50) return 599;
    paralyzedSpeed.statStages[4] = 1;
    if (!Pokerogue3DS::pokemonBaselineEffectiveStat(paralyzedSpeed, 5, false, statusSpeed) || statusSpeed != 75) return 600;
    paralyzedSpeed.stats[5] = 1;
    paralyzedSpeed.statStages[4] = 0;
    if (!Pokerogue3DS::pokemonBaselineEffectiveStat(paralyzedSpeed, 5, false, statusSpeed) || statusSpeed != 1) return 601;
    Pokerogue3DS::PokemonStatusCureEvent cureEvent{};
    checkStatus = {};
    checkStatus.present = true;
    checkStatus.effect = Effect::Sleep;
    if (Pokerogue3DS::curePokemonStatusState(checkStatus, true, true, true, true, true, false, cureEvent) !=
            Pokerogue3DS::PokemonStatusCureResult::UnsupportedReactions || !checkStatus.present) return 602;
    if (Pokerogue3DS::curePokemonStatusState(checkStatus, true, true, true, true, true, true, cureEvent) !=
            Pokerogue3DS::PokemonStatusCureResult::Cleared || checkStatus.present || !cureEvent.lapseNightmare ||
        !cureEvent.lapseConfusion || !cureEvent.reloadAssets || cureEvent.animationFrameRate != 10) return 603;
    checkStatus.present = true;
    checkStatus.effect = Effect::Faint;
    if (Pokerogue3DS::curePokemonStatusState(checkStatus, false, true, true, false, true, true, cureEvent) !=
            Pokerogue3DS::PokemonStatusCureResult::NoEffect || !checkStatus.present) return 604;
    PokemonBattleState burnedActor{};
    burnedActor.status.present = true;
    burnedActor.status.effect = Effect::Burn;
    Pokerogue3DS::PokemonBurnDamagePolicy burnPolicy{};
    double burnMultiplier = 123;
    if (Pokerogue3DS::pokemonBurnDamageMultiplier(burnedActor, 33, burnPolicy, burnMultiplier) ||
        burnMultiplier != 123) return 605;
    burnPolicy.resolved = true;
    if (!Pokerogue3DS::pokemonBurnDamageMultiplier(burnedActor, 33, burnPolicy, burnMultiplier) ||
        burnMultiplier != 0.5) return 606;
    burnPolicy.abilityBypassesReduction = true;
    if (!Pokerogue3DS::pokemonBurnDamageMultiplier(burnedActor, 33, burnPolicy, burnMultiplier) ||
        burnMultiplier != 1) return 607;
    burnPolicy.ignoreSourceAbility = true;
    if (!Pokerogue3DS::pokemonBurnDamageMultiplier(burnedActor, 33, burnPolicy, burnMultiplier) ||
        burnMultiplier != 0.5) return 608;
    for (uint16_t ability : PokerogueContent::kBurnReductionBypassAbilities) {
        Pokerogue3DS::PokemonBurnDamagePolicy resolvedBurn{};
        if (!Pokerogue3DS::resolvePokemonBurnDamagePolicy(ability, true, true, false, resolvedBurn) ||
            !resolvedBurn.abilityBypassesReduction) return 609;
        if (!Pokerogue3DS::resolvePokemonBurnDamagePolicy(ability, true, false, false, resolvedBurn) ||
            resolvedBurn.abilityBypassesReduction) return 610;
        if (Pokerogue3DS::resolvePokemonBurnDamagePolicy(ability, false, true, false, resolvedBurn)) return 611;
    }
    // The runtime residual gate uses this complete absence-of-callback proof.
    bool foundResidualCapability = false;
    for (const auto& profile : PokerogueContent::kAbilityMovegenProfiles) {
        if (!profile.bossDamageCallbacksResolved) continue;
        foundResidualCapability = true;
        PokemonBattleState residualActor{};
        residualActor.abilityId = profile.abilityId;
        residualActor.hp = residualActor.maxHp = 160;
        residualActor.status.present = true;
        residualActor.status.effect = Effect::Toxic;
        Pokerogue3DS::PokemonStatusResidualPolicy residualPolicy{};
        residualPolicy.resolved = profile.bossDamageCallbacksResolved;
        Pokerogue3DS::PokemonStatusResidualEvent residualEvent{};
        if (Pokerogue3DS::applyPokemonStatusResidual(residualActor, residualPolicy, residualEvent) !=
                Pokerogue3DS::PokemonStatusResidualResult::Applied || residualActor.hp != 150 ||
            residualActor.status.toxicTurnCount != 1) return 9011;
        residualPolicy.bossDamageNeedsDispatcher = true;
        if (Pokerogue3DS::applyPokemonStatusResidual(residualActor, residualPolicy, residualEvent) !=
                Pokerogue3DS::PokemonStatusResidualResult::UnsupportedPolicy || residualActor.hp != 150 ||
            residualActor.status.toxicTurnCount != 1) return 9012;
    }
    if (!foundResidualCapability) return 9013;
    bool foundIndirectBlock = false, foundBurnReduction = false, foundStatusBlock = false;
    for (const auto& profile : PokerogueContent::kStatusResidualAbilityProfiles) {
        Pokerogue3DS::PokemonStatusResidualPolicy policy{};
        if (profile.blockedStatusMask) foundStatusBlock = true;
        if (!profile.resolved) {
            if (Pokerogue3DS::resolvePokemonStatusResidualPolicy(profile.abilityId, Effect::Burn, true, true, policy))
                return 9014;
            continue;
        }
        if (!Pokerogue3DS::resolvePokemonStatusResidualPolicy(profile.abilityId, Effect::Burn, true, true, policy))
            return 9015;
        PokemonBattleState actor{};
        actor.hp = actor.maxHp = 160;
        actor.status.present = true;
        actor.status.effect = Effect::Burn;
        Pokerogue3DS::PokemonStatusResidualEvent event{};
        if (profile.blockNonDirectDamage) {
            foundIndirectBlock = true;
            if (Pokerogue3DS::applyPokemonStatusResidual(actor, policy, event) !=
                    Pokerogue3DS::PokemonStatusResidualResult::Blocked || actor.hp != 160 ||
                actor.status.toxicTurnCount != 1) return 9016;
        } else if (profile.burnDenominator == 2) {
            foundBurnReduction = true;
            if (Pokerogue3DS::applyPokemonStatusResidual(actor, policy, event) !=
                    Pokerogue3DS::PokemonStatusResidualResult::Applied || actor.hp != 155) return 9017;
        }
        if (!Pokerogue3DS::resolvePokemonStatusResidualPolicy(profile.abilityId, Effect::Burn, false, true, policy) ||
            policy.blockNonDirectDamage || policy.blockStatusDamage || policy.burnMultiplierDenominator != 1)
            return 9018;
    }
    if (!foundIndirectBlock || !foundBurnReduction || !foundStatusBlock) return 9019;
    bool foundStatusHealing = false;
    for (const auto& profile : PokerogueContent::kStatusResidualAbilityProfiles) {
        if (!profile.resolved || !profile.healedStatusMask) continue;
        foundStatusHealing = true;
        PokemonBattleState actor{};
        actor.abilityId = profile.abilityId;
        actor.maxHp = 160;
        actor.hp = 100;
        actor.status.present = true;
        actor.status.effect = Effect::Toxic;
        Pokerogue3DS::PokemonStatusResidualPolicy residualPolicy{};
        if (!Pokerogue3DS::resolvePokemonStatusResidualPolicy(actor.abilityId, Effect::Toxic, true, true,
                residualPolicy)) return 9020;
        Pokerogue3DS::PokemonStatusResidualEvent residualEvent{};
        if (Pokerogue3DS::applyPokemonStatusResidual(actor, residualPolicy, residualEvent) !=
                Pokerogue3DS::PokemonStatusResidualResult::Blocked || actor.hp != 100 ||
            actor.status.toxicTurnCount != 1) return 9021;
        Pokerogue3DS::PokemonHealingPolicy healing{};
        healing.resolved = true;
        Pokerogue3DS::PokemonHealingEvent event{};
        if (Pokerogue3DS::applyPokemonPostTurnStatusHealing(actor, true, healing, event) !=
                Pokerogue3DS::PokemonHealingResult::Ok || actor.hp != 120 || event.healed != 20 ||
            !actor.status.present || actor.status.toxicTurnCount != 1) return 9022;
        healing.healBlocked = true;
        if (Pokerogue3DS::applyPokemonPostTurnStatusHealing(actor, true, healing, event) !=
                Pokerogue3DS::PokemonHealingResult::Ok || !event.blocked || actor.hp != 120) return 9023;
        healing.healBlocked = false;
        actor.hp = 159;
        if (Pokerogue3DS::applyPokemonPostTurnStatusHealing(actor, true, healing, event) !=
                Pokerogue3DS::PokemonHealingResult::Ok || actor.hp != 160 || event.healed != 1) return 9024;
        actor.hp = 100;
        actor.status.effect = Effect::Burn;
        if (Pokerogue3DS::applyPokemonPostTurnStatusHealing(actor, true, healing, event) !=
                Pokerogue3DS::PokemonHealingResult::Ok || actor.hp != 100 || event.healed) return 9025;
        healing.resolved = false;
        if (Pokerogue3DS::applyPokemonPostTurnStatusHealing(actor, true, healing, event) !=
                Pokerogue3DS::PokemonHealingResult::UnresolvedPolicy || actor.hp != 100) return 9026;
    }
    if (!foundStatusHealing) return 9027;
    PokemonBattleState statusRecipient{};
    statusRecipient.hp = statusRecipient.maxHp = 100;
    Pokerogue3DS::PokemonStatusApplicationPolicy applicationPolicy{};
    applicationPolicy.resolved = true;
    applicationPolicy.hasSource = true;
    Pokerogue3DS::PokemonMoveStatusApplicationEvent applicationEvent{};
    PokerogueRngAdapter statusApplicationRng;
    statusApplicationRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    auto expectedApplicationRng = statusApplicationRng;
    if (Pokerogue3DS::resolvePokemonMoveStatusApplication(statusRecipient, 86, 100, true,
            applicationPolicy, statusApplicationRng, applicationEvent) !=
                Pokerogue3DS::PokemonMoveStatusApplicationResult::Requested ||
        applicationEvent.effect != Effect::Paralysis || applicationEvent.quiet ||
        applicationEvent.chanceRolled || !applicationEvent.requestObtainStatusPhase ||
        statusRecipient.status.present ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9030;
    applicationPolicy.electricType = true;
    expectedApplicationRng = statusApplicationRng;
    const uint8_t expectedStatusChanceRoll = static_cast<uint8_t>(expectedApplicationRng.randSeedInt(100));
    if (Pokerogue3DS::resolvePokemonMoveStatusApplication(statusRecipient, 86, 101, true,
            applicationPolicy, statusApplicationRng, applicationEvent) !=
                Pokerogue3DS::PokemonMoveStatusApplicationResult::Ineligible ||
        !applicationEvent.chanceRolled || applicationEvent.chanceRoll != expectedStatusChanceRoll ||
        applicationEvent.eligibility != Pokerogue3DS::PokemonStatusEligibility::ElectricType ||
        applicationEvent.requestObtainStatusPhase ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9031;
    applicationPolicy.electricType = false;
    if (Pokerogue3DS::resolvePokemonMoveStatusApplication(statusRecipient, 86, 0, true,
            applicationPolicy, statusApplicationRng, applicationEvent) !=
                Pokerogue3DS::PokemonMoveStatusApplicationResult::ChanceFailed ||
        applicationEvent.requestObtainStatusPhase) return 9032;
    expectedApplicationRng = statusApplicationRng;
    applicationEvent.chanceRoll = 123;
    if (Pokerogue3DS::resolvePokemonMoveStatusApplication(statusRecipient, 86, 100, false,
            applicationPolicy, statusApplicationRng, applicationEvent) !=
                Pokerogue3DS::PokemonMoveStatusApplicationResult::UnresolvedPolicy ||
        applicationEvent.chanceRoll != 123 ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9033;
    PokemonBattleState statusMoveUser{};
    statusMoveUser.hp = statusMoveUser.maxHp = 100;
    statusMoveUser.moveCount = 1;
    statusMoveUser.moves[0] = {86, 20, 20};
    Pokerogue3DS::PokemonStatusEffectMovePolicy statusCommandPolicy{};
    statusCommandPolicy.hit.resolved = true;
    statusCommandPolicy.hit.bypassAccuracy = true;
    statusCommandPolicy.application.resolved = true;
    statusCommandPolicy.application.hasSource = true;
    statusCommandPolicy.chanceCallbacksResolved = true;
    Pokerogue3DS::PokemonStatusEffectMoveEvent statusCommandEvent{};
    expectedApplicationRng = statusApplicationRng;
    if (!Pokerogue3DS::usePokemonStatusEffectMove(statusMoveUser, statusRecipient, 0,
            statusCommandPolicy, statusApplicationRng, statusCommandEvent) ||
        statusMoveUser.moves[0].pp != 19 || !statusCommandEvent.application.requestObtainStatusPhase ||
        statusRecipient.status.present || !statusCommandEvent.hit.hit ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9034;
    statusCommandPolicy.hit.bypassAccuracy = false;
    statusCommandPolicy.hit.accuracyMultiplier = 0;
    expectedApplicationRng = statusApplicationRng;
    const auto expectedStatusAccuracy = expectedApplicationRng.randSeedInt(100);
    if (!Pokerogue3DS::usePokemonStatusEffectMove(statusMoveUser, statusRecipient, 0,
            statusCommandPolicy, statusApplicationRng, statusCommandEvent) ||
        statusMoveUser.moves[0].pp != 18 || statusCommandEvent.hit.hit ||
        !statusCommandEvent.hit.accuracyRolled || statusCommandEvent.hit.accuracyRoll != expectedStatusAccuracy ||
        statusCommandEvent.application.requestObtainStatusPhase ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9035;
    statusCommandPolicy.application.resolved = false;
    expectedApplicationRng = statusApplicationRng;
    statusCommandEvent.ppConsumed = 123;
    if (Pokerogue3DS::usePokemonStatusEffectMove(statusMoveUser, statusRecipient, 0,
            statusCommandPolicy, statusApplicationRng, statusCommandEvent) ||
        statusMoveUser.moves[0].pp != 18 || statusCommandEvent.ppConsumed != 123 ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9036;
    PokemonBattleState queuedRecipient{};
    queuedRecipient.pokemonId = 1234;
    queuedRecipient.hp = queuedRecipient.maxHp = 100;
    Pokerogue3DS::PokemonQueuedStatusRequest queuedStatus{};
    queuedStatus.recipientPokemonId = 1234;
    queuedStatus.effect = Effect::Sleep;
    applicationPolicy = {};
    applicationPolicy.resolved = true;
    if (Pokerogue3DS::enqueuePokemonStatusRequest(queuedRecipient, queuedStatus, applicationPolicy) !=
            Pokerogue3DS::PokemonStatusEligibility::Allowed || queuedRecipient.status.present ||
        queuedRecipient.pendingStatus != Effect::Sleep) return 9044;
    if (Pokerogue3DS::enqueuePokemonStatusRequest(queuedRecipient, queuedStatus, applicationPolicy) !=
            Pokerogue3DS::PokemonStatusEligibility::PendingStatus) return 9045;
    // Accepted queue phases do not repeat eligibility if status changed meanwhile.
    queuedRecipient.status.present = true;
    queuedRecipient.status.effect = Effect::Burn;
    auto expectedQueuedRng = statusApplicationRng;
    const uint32_t sleepDraw = expectedQueuedRng.randSeedInt(3);
    if (Pokerogue3DS::applyPokemonQueuedStatus(queuedRecipient, queuedStatus, true, statusApplicationRng) !=
            Pokerogue3DS::PokemonStatusObtainResult::Applied || queuedRecipient.pendingStatus != Effect::None ||
        queuedRecipient.status.effect != Effect::Sleep ||
        queuedRecipient.status.sleepTurnsRemaining != (sleepDraw == 0 ? 2u : 3u) ||
        statusApplicationRng.randSeedUint32() != expectedQueuedRng.randSeedUint32()) return 9040;
    queuedStatus.recipientPokemonId = 1235;
    expectedQueuedRng = statusApplicationRng;
    if (Pokerogue3DS::applyPokemonQueuedStatus(queuedRecipient, queuedStatus, true, statusApplicationRng) !=
            Pokerogue3DS::PokemonStatusObtainResult::Ineligible || queuedRecipient.status.effect != Effect::Sleep ||
        statusApplicationRng.randSeedUint32() != expectedQueuedRng.randSeedUint32()) return 9041;
    queuedStatus.recipientPokemonId = 1234;
    queuedStatus.effect = Effect::None;
    expectedQueuedRng = statusApplicationRng;
    if (Pokerogue3DS::applyPokemonQueuedStatus(queuedRecipient, queuedStatus, true, statusApplicationRng) !=
            Pokerogue3DS::PokemonStatusObtainResult::Ineligible || queuedRecipient.status.effect != Effect::Sleep ||
        statusApplicationRng.randSeedUint32() != expectedQueuedRng.randSeedUint32()) return 9042;
    queuedStatus.effect = Effect::Freeze;
    queuedRecipient.pendingStatus = Effect::Freeze;
    if (Pokerogue3DS::applyPokemonQueuedStatus(queuedRecipient, queuedStatus, false, statusApplicationRng) !=
            Pokerogue3DS::PokemonStatusObtainResult::UnsupportedReactions || queuedRecipient.status.effect != Effect::Sleep)
        return 9043;
    PokemonBattleState commandStatusTarget{};
    commandStatusTarget.hp = commandStatusTarget.maxHp = 100;
    commandStatusTarget.pokemonId = 42;
    statusMoveUser.pokemonId = 41;
    statusMoveUser.moves[0] = {79, 15, 15}; // Real Sleep Powder.
    Pokerogue3DS::PokemonStatusEffectCommandPolicy fullStatusPolicy{};
    fullStatusPolicy.move.hit.resolved = true;
    fullStatusPolicy.move.hit.bypassAccuracy = true;
    fullStatusPolicy.move.application.resolved = true;
    fullStatusPolicy.move.application.hasSource = true;
    fullStatusPolicy.move.chanceCallbacksResolved = true;
    fullStatusPolicy.reactionsResolved = true;
    expectedApplicationRng = statusApplicationRng;
    const uint32_t expectedSleepDraw = expectedApplicationRng.randSeedInt(3);
    if (!Pokerogue3DS::executePokemonStatusEffectCommand(statusMoveUser, commandStatusTarget, 0,
            fullStatusPolicy, statusApplicationRng, statusApplicationRng, statusCommandEvent) ||
        statusMoveUser.moves[0].pp != 14 || commandStatusTarget.status.effect != Effect::Sleep ||
        commandStatusTarget.status.sleepTurnsRemaining != (expectedSleepDraw == 0 ? 2u : 3u) ||
        commandStatusTarget.pendingStatus != Effect::None ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9050;
    commandStatusTarget.status = {};
    fullStatusPolicy.reactionsResolved = false;
    expectedApplicationRng = statusApplicationRng;
    if (Pokerogue3DS::executePokemonStatusEffectCommand(statusMoveUser, commandStatusTarget, 0,
            fullStatusPolicy, statusApplicationRng, statusApplicationRng, statusCommandEvent) ||
        statusMoveUser.moves[0].pp != 14 || commandStatusTarget.status.present ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9051;
    fullStatusPolicy.reactionsResolved = true;
    PokerogueRngAdapter recipientStatusRng;
    recipientStatusRng.sow(damageSeed, sizeof(damageSeed) / sizeof(damageSeed[0]));
    auto expectedRecipientStatusRng = recipientStatusRng;
    expectedApplicationRng = statusApplicationRng;
    const uint32_t separateSleepDraw = expectedRecipientStatusRng.randSeedInt(3);
    if (!Pokerogue3DS::executePokemonStatusEffectCommand(statusMoveUser, commandStatusTarget, 0,
            fullStatusPolicy, statusApplicationRng, recipientStatusRng, statusCommandEvent) ||
        statusMoveUser.moves[0].pp != 13 ||
        commandStatusTarget.status.sleepTurnsRemaining != (separateSleepDraw == 0 ? 2u : 3u) ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32() ||
        recipientStatusRng.randSeedUint32() != expectedRecipientStatusRng.randSeedUint32()) return 9052;
    bool immune = false;
    if (Pokerogue3DS::resolvePokemonStatusAbilityImmunity(17, Effect::Poison, true, false, true, immune) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !immune) return 9060; // Immunity.
    if (Pokerogue3DS::resolvePokemonStatusAbilityImmunity(17, Effect::Toxic, true, false, true, immune) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !immune) return 9061;
    if (Pokerogue3DS::resolvePokemonStatusAbilityImmunity(17, Effect::Sleep, true, false, true, immune) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || immune) return 9062;
    if (Pokerogue3DS::resolvePokemonStatusAbilityImmunity(17, Effect::Poison, false, false, true, immune) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || immune) return 9063;
    bool foundAllImmunity = false, foundAllyImmunity = false, foundConditionalImmunity = false;
    for (const auto& profile : PokerogueContent::kStatusImmunityAbilityProfiles) {
        if (profile.selfResolved && profile.selfMask == 127) {
            foundAllImmunity = true;
            if (Pokerogue3DS::resolvePokemonStatusAbilityImmunity(profile.abilityId, Effect::Sleep, true, false, true, immune) !=
                    Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !immune) return 9064;
            if (Pokerogue3DS::resolvePokemonStatusAbilityImmunity(profile.abilityId, Effect::Faint, true, false, true, immune) !=
                    Pokerogue3DS::PokemonStatusImmunityResult::Resolved || immune) return 9065;
        }
        if (profile.allyResolved && (profile.allyMask & (1u << static_cast<uint8_t>(Effect::Sleep)))) {
            foundAllyImmunity = true;
            if (Pokerogue3DS::resolvePokemonStatusAbilityImmunity(profile.abilityId, Effect::Sleep, true, true, true, immune) !=
                    Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !immune) return 9066;
        }
        if (!profile.selfResolved || !profile.allyResolved) {
            foundConditionalImmunity = true;
            immune = true;
            if (Pokerogue3DS::resolvePokemonStatusAbilityImmunity(profile.abilityId, Effect::Sleep, true,
                    profile.selfResolved, true, immune) != Pokerogue3DS::PokemonStatusImmunityResult::UnsupportedCondition ||
                !immune) return 9067;
        }
    }
    if (!foundAllImmunity || !foundAllyImmunity || !foundConditionalImmunity) return 9068;
    immune = true;
    if (Pokerogue3DS::resolvePokemonStatusAbilityImmunity(65535, Effect::Sleep, true, false, true, immune) !=
            Pokerogue3DS::PokemonStatusImmunityResult::UnknownAbility || !immune) return 9069;
    bool bypassPoison = false, bypassSteel = false;
    if (Pokerogue3DS::resolvePokemonStatusTypeImmunityBypass(212, Effect::Poison, "POISON", true, true, bypassPoison) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !bypassPoison ||
        Pokerogue3DS::resolvePokemonStatusTypeImmunityBypass(212, Effect::Poison, "STEEL", true, true, bypassSteel) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !bypassSteel) return 9070;
    applicationPolicy = {};
    applicationPolicy.resolved = true;
    applicationPolicy.hasSource = true;
    applicationPolicy.poisonType = applicationPolicy.steelType = true;
    applicationPolicy.sourceIgnoresPoisonImmunity = bypassPoison;
    applicationPolicy.sourceIgnoresSteelImmunity = bypassSteel;
    if (Pokerogue3DS::canPokemonSetStatus({}, Effect::Poison, applicationPolicy) !=
            Pokerogue3DS::PokemonStatusEligibility::Allowed) return 9071;
    if (Pokerogue3DS::resolvePokemonStatusTypeImmunityBypass(212, Effect::Toxic, "STEEL", false, true, bypassSteel) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || bypassSteel) return 9072;
    if (Pokerogue3DS::resolvePokemonStatusTypeImmunityBypass(212, Effect::Burn, "STEEL", true, true, bypassSteel) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || bypassSteel) return 9073;
    bypassSteel = true;
    if (Pokerogue3DS::resolvePokemonStatusTypeImmunityBypass(212, Effect::Poison, "UNKNOWN", true, true, bypassSteel) !=
            Pokerogue3DS::PokemonStatusImmunityResult::InvalidType || !bypassSteel) return 9074;
    if (Pokerogue3DS::resolvePokemonStatusTypeImmunityBypass(65535, Effect::Poison, "STEEL", true, true, bypassSteel) !=
            Pokerogue3DS::PokemonStatusImmunityResult::UnknownAbility || !bypassSteel) return 9075;
    applicationPolicy.hasSource = false;
    if (Pokerogue3DS::canPokemonSetStatus({}, Effect::Poison, applicationPolicy) !=
            Pokerogue3DS::PokemonStatusEligibility::PoisonType) return 9076;
    Pokerogue3DS::PokemonStatusApplicationPolicy environment{};
    environment.resolved = true;
    environment.hasSource = true;
    environment.poisonType = environment.steelType = true;
    const Pokerogue3DS::PokemonStatusAbilityComponent corrosionSource[] = {{212, true, true}};
    Pokerogue3DS::PokemonStatusAbilityComponent ownImmunity[] = {{17, true, true}};
    Pokerogue3DS::PokemonStatusApplicationPolicy composedPolicy{};
    if (Pokerogue3DS::composePokemonStatusApplicationPolicy(Effect::Poison, environment,
            ownImmunity, 1, nullptr, 0, corrosionSource, 1, composedPolicy) !=
                Pokerogue3DS::PokemonStatusImmunityResult::Resolved ||
        !composedPolicy.sourceIgnoresPoisonImmunity || !composedPolicy.sourceIgnoresSteelImmunity ||
        !composedPolicy.selfAbilityBlocks ||
        Pokerogue3DS::canPokemonSetStatus({}, Effect::Poison, composedPolicy) !=
                Pokerogue3DS::PokemonStatusEligibility::SelfAbility) return 9080;
    ownImmunity[0] = {0, true, true};
    if (Pokerogue3DS::composePokemonStatusApplicationPolicy(Effect::Poison, environment,
            ownImmunity, 1, nullptr, 0, corrosionSource, 1, composedPolicy) !=
                Pokerogue3DS::PokemonStatusImmunityResult::Resolved || composedPolicy.selfAbilityBlocks ||
        Pokerogue3DS::canPokemonSetStatus({}, Effect::Poison, composedPolicy) !=
                Pokerogue3DS::PokemonStatusEligibility::Allowed) return 9081;
    environment.hasSource = false;
    if (Pokerogue3DS::composePokemonStatusApplicationPolicy(Effect::Poison, environment,
            ownImmunity, 1, nullptr, 0, nullptr, 0, composedPolicy) !=
                Pokerogue3DS::PokemonStatusImmunityResult::Resolved || composedPolicy.sourceIgnoresPoisonImmunity ||
        Pokerogue3DS::canPokemonSetStatus({}, Effect::Poison, composedPolicy) !=
                Pokerogue3DS::PokemonStatusEligibility::PoisonType) return 9082;
    ownImmunity[0].callbacksResolved = false;
    composedPolicy.selfAbilityBlocks = true;
    if (Pokerogue3DS::composePokemonStatusApplicationPolicy(Effect::Poison, environment,
            ownImmunity, 1, nullptr, 0, nullptr, 0, composedPolicy) !=
                Pokerogue3DS::PokemonStatusImmunityResult::UnsupportedCondition || !composedPolicy.selfAbilityBlocks)
        return 9083;
    ownImmunity[0].callbacksResolved = true;
    for (const auto& profile : PokerogueContent::kStatusImmunityAbilityProfiles) {
        if (!profile.allyResolved || !(profile.allyMask & 16)) continue;
        const Pokerogue3DS::PokemonStatusAbilityComponent ally[] = {{profile.abilityId, true, true}};
        if (Pokerogue3DS::composePokemonStatusApplicationPolicy(Effect::Sleep, environment,
                ownImmunity, 1, ally, 1, nullptr, 0, composedPolicy) !=
                    Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !composedPolicy.allyAbilityBlocks ||
            Pokerogue3DS::canPokemonSetStatus({}, Effect::Sleep, composedPolicy) !=
                    Pokerogue3DS::PokemonStatusEligibility::AllyAbility) return 9084;
    }
    Pokerogue3DS::PokemonStatusFieldContext statusField{};
    statusField.resolved = true;
    const char* statusLiveTypes[] = {"STELLAR"};
    const char* statusOriginalTypes[] = {"POISON", "ELECTRIC"};
    statusField.effectiveTypes = statusLiveTypes;
    statusField.effectiveTypeCount = 1;
    statusField.originalIfStellarTypes = statusOriginalTypes;
    statusField.originalIfStellarTypeCount = 2;
    PokemonBattleState environmentActor{};
    environmentActor.hp = environmentActor.maxHp = 100;
    if (!Pokerogue3DS::resolvePokemonStatusApplicationEnvironment(environmentActor, nullptr, statusField, environment) ||
        !environment.poisonType || environment.electricType || environment.hasSource || environment.sourceIsTarget ||
        Pokerogue3DS::canPokemonSetStatus({}, Effect::Poison, environment) !=
            Pokerogue3DS::PokemonStatusEligibility::PoisonType ||
        Pokerogue3DS::canPokemonSetStatus({}, Effect::Paralysis, environment) !=
            Pokerogue3DS::PokemonStatusEligibility::Allowed) return 9090;
    statusField.grounded = statusField.electricTerrain = true;
    statusField.ignoreField = true;
    if (!Pokerogue3DS::resolvePokemonStatusApplicationEnvironment(environmentActor, &environmentActor, statusField, environment) ||
        !environment.hasSource || !environment.sourceIsTarget ||
        Pokerogue3DS::canPokemonSetStatus({}, Effect::Sleep, environment) !=
            Pokerogue3DS::PokemonStatusEligibility::ElectricTerrain) return 9091;
    environmentActor.pendingStatus = Effect::Sleep;
    if (!Pokerogue3DS::resolvePokemonStatusApplicationEnvironment(environmentActor, nullptr, statusField, environment) ||
        !environment.pendingStatus || Pokerogue3DS::canPokemonSetStatus({}, Effect::Burn, environment) !=
            Pokerogue3DS::PokemonStatusEligibility::PendingStatus) return 9092;
    const char* invalidLiveTypes[] = {"NOT_A_TYPE"};
    statusField.effectiveTypes = invalidLiveTypes;
    environment.fireType = true;
    if (Pokerogue3DS::resolvePokemonStatusApplicationEnvironment(environmentActor, nullptr, statusField, environment) ||
        !environment.fireType) return 9093;
    statusField.effectiveTypes = statusLiveTypes;
    statusField.resolved = false;
    if (Pokerogue3DS::resolvePokemonStatusApplicationEnvironment(environmentActor, nullptr, statusField, environment) ||
        !environment.fireType) return 9094;
    Pokerogue3DS::PokemonQueuedStatusRequest appliedStatus{};
    appliedStatus.recipientPokemonId = 42;
    appliedStatus.sourcePokemonId = 41;
    appliedStatus.hasSource = true;
    appliedStatus.effect = Effect::Burn;
    Pokerogue3DS::PokemonSynchronizeReactionEvent syncEvent{};
    if (Pokerogue3DS::resolvePokemonSynchronizeReaction(28, true, true, appliedStatus, syncEvent) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !syncEvent.abilityActivates ||
        !syncEvent.requestStatus || syncEvent.request.recipientPokemonId != 41 ||
        syncEvent.request.sourcePokemonId != 42 || syncEvent.request.effect != Effect::Burn) return 9100;
    PokemonBattleState reflectedRecipient{};
    reflectedRecipient.pokemonId = 41;
    reflectedRecipient.hp = reflectedRecipient.maxHp = 100;
    applicationPolicy = {};
    applicationPolicy.resolved = true;
    applicationPolicy.hasSource = true;
    applicationPolicy.fireType = true;
    if (Pokerogue3DS::enqueuePokemonStatusRequest(reflectedRecipient, syncEvent.request, applicationPolicy) !=
            Pokerogue3DS::PokemonStatusEligibility::FireType ||
        reflectedRecipient.pendingStatus != Effect::None || !syncEvent.abilityActivates) return 9101;
    appliedStatus.effect = Effect::Sleep;
    if (Pokerogue3DS::resolvePokemonSynchronizeReaction(28, true, true, appliedStatus, syncEvent) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || syncEvent.abilityActivates || syncEvent.requestStatus)
        return 9102;
    appliedStatus.effect = Effect::Toxic;
    appliedStatus.hasSource = false;
    appliedStatus.sourcePokemonId = 0;
    if (Pokerogue3DS::resolvePokemonSynchronizeReaction(28, true, true, appliedStatus, syncEvent) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || syncEvent.requestStatus) return 9103;
    appliedStatus.hasSource = true;
    if (Pokerogue3DS::resolvePokemonSynchronizeReaction(28, false, true, appliedStatus, syncEvent) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || syncEvent.requestStatus) return 9104;
    syncEvent.abilityActivates = true;
    if (Pokerogue3DS::resolvePokemonSynchronizeReaction(28, true, false, appliedStatus, syncEvent) !=
            Pokerogue3DS::PokemonStatusImmunityResult::UnsupportedCondition || !syncEvent.abilityActivates) return 9105;
    PokemonBattleState synchronizeActor{}, reflectedSource{};
    synchronizeActor.abilityId = 28;
    synchronizeActor.pokemonId = 81;
    reflectedSource.pokemonId = 82;
    reflectedSource.hp = reflectedSource.maxHp = 100;
    Pokerogue3DS::PokemonQueuedStatusRequest synchronizedRequest{};
    synchronizedRequest.recipientPokemonId = 81;
    synchronizedRequest.sourcePokemonId = 82;
    synchronizedRequest.hasSource = true;
    synchronizedRequest.effect = Effect::Burn;
    applicationPolicy = {};
    applicationPolicy.resolved = applicationPolicy.hasSource = true;
    Pokerogue3DS::PokemonSynchronizeCommandEvent synchronizedEvent{};
    if (!Pokerogue3DS::executePokemonSynchronizeReaction(synchronizeActor, reflectedSource, synchronizedRequest,
            true, true, applicationPolicy, true, statusApplicationRng, synchronizedEvent) ||
        !synchronizedEvent.statusApplied || !reflectedSource.status.present ||
        reflectedSource.status.effect != Effect::Burn || reflectedSource.pendingStatus != Effect::None) return 9210;
    reflectedSource.status = {};
    applicationPolicy.fireType = true;
    expectedApplicationRng = statusApplicationRng;
    if (!Pokerogue3DS::executePokemonSynchronizeReaction(synchronizeActor, reflectedSource, synchronizedRequest,
            true, true, applicationPolicy, true, statusApplicationRng, synchronizedEvent) ||
        !synchronizedEvent.reaction.abilityActivates || synchronizedEvent.statusApplied ||
        synchronizedEvent.eligibility != Pokerogue3DS::PokemonStatusEligibility::FireType ||
        reflectedSource.status.present ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9211;
    applicationPolicy.fireType = false;
    synchronizedEvent.statusApplied = true;
    expectedApplicationRng = statusApplicationRng;
    if (Pokerogue3DS::executePokemonSynchronizeReaction(synchronizeActor, reflectedSource, synchronizedRequest,
            true, true, applicationPolicy, false, statusApplicationRng, synchronizedEvent) ||
        reflectedSource.status.present || reflectedSource.pendingStatus != Effect::None ||
        !synchronizedEvent.statusApplied ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9212;
    reflectedSource.hp = 0;
    expectedApplicationRng = statusApplicationRng;
    if (!Pokerogue3DS::executePokemonSynchronizeReaction(synchronizeActor, reflectedSource, synchronizedRequest,
            true, true, applicationPolicy, true, statusApplicationRng, synchronizedEvent) ||
        !synchronizedEvent.reaction.abilityActivates || synchronizedEvent.statusApplied ||
        synchronizedEvent.eligibility != Pokerogue3DS::PokemonStatusEligibility::Fainted ||
        reflectedSource.status.present || reflectedSource.pendingStatus != Effect::None ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9220;
    reflectedSource.hp = 100;
    applicationPolicy.resolved = false;
    expectedApplicationRng = statusApplicationRng;
    if (!Pokerogue3DS::executePokemonSynchronizeReaction(synchronizeActor, reflectedSource, synchronizedRequest,
            true, true, applicationPolicy, false, statusApplicationRng, synchronizedEvent, true) ||
        !synchronizedEvent.reaction.abilityActivates || synchronizedEvent.statusApplied ||
        reflectedSource.status.present || reflectedSource.pendingStatus != Effect::None ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9221;
    PokemonBattleState reactionRecipient{}, reactionSource{};
    reactionRecipient.abilityId = 28;
    reactionRecipient.pokemonId = 101;
    reactionRecipient.hp = reactionRecipient.maxHp = 100;
    reactionRecipient.status.present = true;
    reactionRecipient.status.effect = Effect::Poison;
    reactionSource.abilityId = 310;
    reactionSource.pokemonId = 102;
    reactionSource.hp = reactionSource.maxHp = 100;
    synchronizedRequest.recipientPokemonId = 101;
    synchronizedRequest.sourcePokemonId = 102;
    synchronizedRequest.effect = Effect::Poison;
    Pokerogue3DS::PokemonPostSetStatusPolicy reactionsPolicy{};
    reactionsPolicy.formsResolved = reactionsPolicy.recipientAbilityActive =
        reactionsPolicy.recipientCallbacksResolved = reactionsPolicy.sourceAbilityActive = true;
    reactionsPolicy.reflectedApplication.resolved = reactionsPolicy.reflectedApplication.hasSource = true;
    reactionsPolicy.reflectedReactionsResolved = true;
    reactionsPolicy.confusionProbe.resolved = reactionsPolicy.confusionApplication.resolved = true;
    Pokerogue3DS::PokemonPostSetStatusEvent reactionsEvent{};
    expectedApplicationRng = statusApplicationRng;
    const auto expectedReactionDuration = expectedApplicationRng.randSeedIntRange(2, 5);
    if (!Pokerogue3DS::executePokemonPostSetStatusReactions(reactionRecipient, reactionSource,
            synchronizedRequest, reactionsPolicy, statusApplicationRng, statusApplicationRng, reactionsEvent) ||
        reactionSource.status.effect != Effect::Poison || !reactionSource.status.present ||
        reactionSource.pendingStatus != Effect::None || !reactionRecipient.confusion.present ||
        reactionRecipient.confusion.turns != expectedReactionDuration ||
        !reactionRecipient.confusion.sourceMoveResolved || reactionRecipient.confusion.sourceMoveId ||
        !reactionRecipient.confusion.sourcePokemonResolved ||
        reactionRecipient.confusion.sourcePokemonId != reactionRecipient.pokemonId ||
        !reactionsEvent.synchronize.statusApplied || !reactionsEvent.confusion.tagAttempted ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9230;
    reactionSource.status = {};
    reactionRecipient.confusion = {};
    reactionsPolicy.confusionProbe.resolved = false;
    expectedApplicationRng = statusApplicationRng;
    if (Pokerogue3DS::executePokemonPostSetStatusReactions(reactionRecipient, reactionSource,
            synchronizedRequest, reactionsPolicy, statusApplicationRng, statusApplicationRng, reactionsEvent) ||
        reactionSource.status.present || reactionSource.pendingStatus != Effect::None ||
        reactionRecipient.confusion.present ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9231;
    // Poison Powder -> ObtainStatus -> Synchronize -> Poison Puppeteer.
    PokemonBattleState completeStatusUser = reactionSource, completeStatusTarget = reactionRecipient;
    completeStatusUser.status = {};
    completeStatusUser.moveCount = 1;
    completeStatusUser.moves[0].moveId = 77;
    completeStatusUser.moves[0].pp = completeStatusUser.moves[0].maxPp = 35;
    completeStatusTarget.status = {};
    completeStatusTarget.confusion = {};
    Pokerogue3DS::PokemonStatusEffectCommandPolicy completeStatusPolicy{};
    completeStatusPolicy.reactionsResolved = true;
    completeStatusPolicy.move.hit.resolved = completeStatusPolicy.move.hit.bypassAccuracy = true;
    completeStatusPolicy.move.application.resolved = completeStatusPolicy.move.application.hasSource = true;
    completeStatusPolicy.move.chanceCallbacksResolved = true;
    reactionsPolicy.confusionProbe.resolved = true;
    Pokerogue3DS::PokemonStatusActionEvent completeStatusEvent{};
    expectedApplicationRng = statusApplicationRng;
    const auto completeTagDuration = expectedApplicationRng.randSeedIntRange(2, 5);
    if (!Pokerogue3DS::executePokemonStatusAction(completeStatusUser, completeStatusTarget, 0,
            completeStatusPolicy, reactionsPolicy, statusApplicationRng, statusApplicationRng, completeStatusEvent) ||
        completeStatusUser.moves[0].pp != 34 || completeStatusUser.status.effect != Effect::Poison ||
        !completeStatusUser.status.present || completeStatusTarget.status.effect != Effect::Poison ||
        !completeStatusTarget.status.present || completeStatusTarget.confusion.turns != completeTagDuration ||
        !completeStatusEvent.reactionsExecuted ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9240;
    auto secondaryUser = completeStatusUser;
    auto secondaryTarget = completeStatusTarget;
    secondaryUser.status = secondaryTarget.status = {};
    secondaryTarget.confusion = {};
    auto secondaryReactions = reactionsPolicy;
    secondaryReactions.formsResolved = true;
    auto secondaryRng = statusApplicationRng;
    auto expectedSecondaryRng = secondaryRng;
    const auto ppBeforeSecondary = secondaryUser.moves[0].pp;
    Pokerogue3DS::PokemonMoveStatusPhaseEvent secondaryEvent{};
    if (!Pokerogue3DS::executePokemonMoveStatusPhase(secondaryUser, secondaryTarget, 52, 100,
            completeStatusPolicy.move.application, secondaryReactions, secondaryRng, secondaryEvent) ||
        !secondaryEvent.applied || !secondaryEvent.application.quiet ||
        secondaryTarget.status.effect != Effect::Burn || secondaryUser.status.effect != Effect::Burn ||
        secondaryUser.moves[0].pp != ppBeforeSecondary ||
        secondaryRng.randSeedUint32() != expectedSecondaryRng.randSeedUint32()) return 9420;
    secondaryUser.status = secondaryTarget.status = {};
    secondaryTarget.hp = 0;
    expectedSecondaryRng = secondaryRng;
    (void)expectedSecondaryRng.randSeedInt(100);
    if (!Pokerogue3DS::executePokemonMoveStatusPhase(secondaryUser, secondaryTarget, 52, 25,
            completeStatusPolicy.move.application, secondaryReactions, secondaryRng, secondaryEvent) ||
        secondaryEvent.applied || secondaryTarget.status.present ||
        secondaryRng.randSeedUint32() != expectedSecondaryRng.randSeedUint32()) return 9421;
    secondaryTarget.hp = secondaryTarget.maxHp;
    secondaryReactions.formsResolved = false;
    expectedSecondaryRng = secondaryRng;
    if (Pokerogue3DS::executePokemonMoveStatusPhase(secondaryUser, secondaryTarget, 52, 100,
            completeStatusPolicy.move.application, secondaryReactions, secondaryRng, secondaryEvent) ||
        secondaryTarget.status.present || secondaryUser.status.present ||
        secondaryRng.randSeedUint32() != expectedSecondaryRng.randSeedUint32()) return 9422;
    // currentBattle owns both actors' duration draws; no actor-local stream.
    auto sleepActionUser = completeStatusUser;
    auto sleepActionTarget = completeStatusTarget;
    sleepActionUser.status = {};
    sleepActionTarget.status = {};
    sleepActionTarget.confusion = {};
    sleepActionUser.moves[0].moveId = 95;
    sleepActionUser.moves[0].pp = sleepActionUser.moves[0].maxPp = 20;
    auto sleepActionPolicy = completeStatusPolicy;
    sleepActionPolicy.move.effectiveChance = -1;
    auto sleepReactionPolicy = reactionsPolicy;
    sleepReactionPolicy.formsResolved = true;
    auto sharedSleepRng = statusApplicationRng;
    auto expectedSharedSleepRng = sharedSleepRng;
    const auto expectedSleepTurns = expectedSharedSleepRng.randSeedInt(3) == 0 ? 2 : 3;
    Pokerogue3DS::PokemonStatusActionEvent sleepActionEvent{};
    if (!Pokerogue3DS::executePokemonStatusAction(sleepActionUser, sleepActionTarget, 0,
            sleepActionPolicy, sleepReactionPolicy, sharedSleepRng, sharedSleepRng, sleepActionEvent) ||
        sleepActionUser.moves[0].pp != 19 || sleepActionUser.status.present ||
        sleepActionTarget.status.effect != Effect::Sleep ||
        sleepActionTarget.status.sleepTurnsRemaining != expectedSleepTurns ||
        sharedSleepRng.randSeedUint32() != expectedSharedSleepRng.randSeedUint32()) return 9370;
    completeStatusUser.status = {};
    completeStatusTarget.status = {};
    completeStatusTarget.confusion = {};
    reactionsPolicy.formsResolved = false;
    expectedApplicationRng = statusApplicationRng;
    if (Pokerogue3DS::executePokemonStatusAction(completeStatusUser, completeStatusTarget, 0,
            completeStatusPolicy, reactionsPolicy, statusApplicationRng, statusApplicationRng, completeStatusEvent) ||
        completeStatusUser.moves[0].pp != 34 || completeStatusUser.status.present ||
        completeStatusTarget.status.present || completeStatusTarget.pendingStatus != Effect::None ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9241;
    reactionRecipient.status.effect = Effect::Faint;
    synchronizedRequest.effect = Effect::Faint;
    reactionsPolicy = {};
    expectedApplicationRng = statusApplicationRng;
    if (!Pokerogue3DS::executePokemonPostSetStatusReactions(reactionRecipient, reactionSource,
            synchronizedRequest, reactionsPolicy, statusApplicationRng, statusApplicationRng, reactionsEvent) ||
        reactionsEvent.synchronize.reaction.abilityActivates || reactionsEvent.confusion.tagAttempted ||
        reactionSource.status.present || reactionRecipient.confusion.present ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9250;
    if (!Pokerogue3DS::pokemonStatusFormCallbacksAbsent(1) ||
        !Pokerogue3DS::pokemonStatusFormCallbacksAbsent(492) ||
        Pokerogue3DS::pokemonStatusFormCallbacksAbsent(65535)) return 9290;
    policyRecipient.speciesDex = 1;
    policyRecipient.abilityId = 28;
    policySource.speciesDex = 492;
    policySource.abilityId = 310;
    recipientPolicies.status.resolved = recipientPolicies.confusion.resolved = true;
    Pokerogue3DS::PokemonPostSetStatusPolicy derivedPostStatus{};
    if (!Pokerogue3DS::resolvePokemonPostSetStatusPolicy(policyRecipient, policySource, Effect::Poison,
            recipientPolicies, recipientPolicies, true, true, false, derivedPostStatus) ||
        !derivedPostStatus.formsResolved || !derivedPostStatus.recipientCallbacksResolved ||
        !derivedPostStatus.recipientAbilityActive || !derivedPostStatus.sourceAbilityActive ||
        derivedPostStatus.reflectedReactionsResolved) return 9300;
    policyRecipient.speciesDex = 65535;
    derivedPostStatus.formsResolved = false;
    if (Pokerogue3DS::resolvePokemonPostSetStatusPolicy(policyRecipient, policySource, Effect::Poison,
            recipientPolicies, recipientPolicies, true, true, false, derivedPostStatus) ||
        derivedPostStatus.formsResolved) return 9301;
    bool hypnosisFlags = false, powderFlags = false, singFlags = false;
    for (const auto& profile : PokerogueContent::kStatusMoveFlagProfiles) {
        if (profile.moveId == 95) hypnosisFlags = profile.resolved && profile.reflectable && !profile.powder && !profile.sound;
        if (profile.moveId == 77) powderFlags = profile.resolved && profile.reflectable && profile.powder && !profile.sound;
        if (profile.moveId == 47) singFlags = profile.resolved && profile.reflectable && profile.sound && !profile.powder;
    }
    if (!hypnosisFlags || !powderFlags || !singFlags) return 9310;
    const char* powderTargetTypes[] = {"GRASS", "DARK"};
    Pokerogue3DS::PokemonStatusMoveTypeImmunityPolicy typeImmunityPolicy{};
    typeImmunityPolicy.resolved = true;
    typeImmunityPolicy.originalIfStellarTypes = powderTargetTypes;
    typeImmunityPolicy.typeCount = 2;
    bool typeImmune = false;
    if (!Pokerogue3DS::resolvePokemonStatusMoveTypeImmunity(77, typeImmunityPolicy, typeImmune) ||
        !typeImmune) return 9320;
    if (!Pokerogue3DS::resolvePokemonStatusMoveTypeImmunity(95, typeImmunityPolicy, typeImmune) ||
        typeImmune) return 9321;
    typeImmunityPolicy.userHasPrankster = typeImmunityPolicy.opponents = true;
    if (!Pokerogue3DS::resolvePokemonStatusMoveTypeImmunity(95, typeImmunityPolicy, typeImmune) ||
        !typeImmune) return 9322;
    typeImmunityPolicy.opponents = false;
    if (!Pokerogue3DS::resolvePokemonStatusMoveTypeImmunity(95, typeImmunityPolicy, typeImmune) ||
        typeImmune) return 9323;
    typeImmunityPolicy.resolved = false;
    typeImmune = true;
    if (Pokerogue3DS::resolvePokemonStatusMoveTypeImmunity(95, typeImmunityPolicy, typeImmune) ||
        !typeImmune) return 9324;
    typeImmunityPolicy.resolved = true;
    Pokerogue3DS::PokemonStatusMoveHitPolicy baseTypeHit{}, composedTypeHit{};
    baseTypeHit.resolved = true;
    if (!Pokerogue3DS::composePokemonStatusMoveTypeHitPolicy(77, baseTypeHit, typeImmunityPolicy,
            composedTypeHit) || !composedTypeHit.typeImmune) return 9330;
    PokemonBattleState powderUser = completeStatusUser, grassTarget = completeStatusTarget;
    powderUser.status = {};
    grassTarget.status = {};
    powderUser.moves[0].pp = 35;
    completeStatusPolicy.move.hit = composedTypeHit;
    Pokerogue3DS::PokemonStatusEffectMoveEvent powderHitEvent{};
    expectedApplicationRng = statusApplicationRng;
    if (!Pokerogue3DS::usePokemonStatusEffectMove(powderUser, grassTarget, 0, completeStatusPolicy.move,
            statusApplicationRng, powderHitEvent) || powderUser.moves[0].pp != 34 ||
        powderHitEvent.hit.hit || !powderHitEvent.hit.typeImmune || powderHitEvent.hit.accuracyRolled ||
        powderHitEvent.application.requestObtainStatusPhase ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9331;
    bool soundImmunityFound = false, powderImmunityFound = false;
    for (const auto& profile : PokerogueContent::kStatusFlagImmunityProfiles) {
        if (!profile.resolved || (!profile.sound && !profile.powder)) continue;
        bool blocked = false;
        const uint16_t immunityMove = profile.sound ? 47 : 77;
        if (!Pokerogue3DS::resolvePokemonStatusFlagAbilityImmunity(profile.abilityId, immunityMove,
                true, true, false, blocked) || !blocked) return 9340;
        if (!Pokerogue3DS::resolvePokemonStatusFlagAbilityImmunity(profile.abilityId, immunityMove,
                true, true, true, blocked) || blocked) return 9341;
        if (!Pokerogue3DS::resolvePokemonStatusFlagAbilityImmunity(profile.abilityId, immunityMove,
                false, true, false, blocked) || blocked) return 9342;
        soundImmunityFound |= profile.sound;
        powderImmunityFound |= profile.powder;
    }
    if (!soundImmunityFound || !powderImmunityFound) return 9343;
    for (const auto& profile : PokerogueContent::kStatusFlagImmunityProfiles) {
        if (!profile.resolved || !profile.powder) continue;
        const Pokerogue3DS::PokemonStatusAbilityComponent powderDefender[] = {{profile.abilityId, true, true}};
        Pokerogue3DS::PokemonStatusMoveHitPolicy immuneHit{};
        if (!Pokerogue3DS::composePokemonStatusFlagAbilityHitPolicy(77, baseTypeHit, false,
                powderDefender, 1, immuneHit) || !immuneHit.blockedBeforeAccuracy) return 9350;
        powderUser.moves[0].pp = 35;
        completeStatusPolicy.move.hit = immuneHit;
        expectedApplicationRng = statusApplicationRng;
        if (!Pokerogue3DS::usePokemonStatusEffectMove(powderUser, grassTarget, 0, completeStatusPolicy.move,
                statusApplicationRng, powderHitEvent) || powderUser.moves[0].pp != 34 ||
            powderHitEvent.hit.hit || powderHitEvent.hit.accuracyRolled || powderHitEvent.hit.typeImmune ||
            powderHitEvent.application.requestObtainStatusPhase ||
            statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9351;
        const Pokerogue3DS::PokemonStatusAbilityComponent pendingDefender[] = {{profile.abilityId, true, false}};
        immuneHit.blockedBeforeAccuracy = false;
        if (Pokerogue3DS::composePokemonStatusFlagAbilityHitPolicy(77, baseTypeHit, false,
                pendingDefender, 1, immuneHit) || immuneHit.blockedBeforeAccuracy) return 9352;
    }

    PokemonBattleState accuracyUser{}, accuracyTarget{};
    accuracyUser.statStages[5] = 6;
    accuracyTarget.statStages[6] = -6;
    Pokerogue3DS::PokemonStatusMoveHitPolicy statusAccuracyBase{}, statusAccuracyComposed{};
    statusAccuracyBase.resolved = true;
    statusAccuracyBase.accuracyMultiplier = 0.5;
    if (!Pokerogue3DS::composePokemonStatusAccuracyStagePolicy(accuracyUser, accuracyTarget,
            statusAccuracyBase, statusAccuracyComposed) || statusAccuracyComposed.accuracyMultiplier != 1.5) return 9390;
    accuracyUser.statStages[5] = -6;
    accuracyTarget.statStages[6] = 6;
    if (!Pokerogue3DS::composePokemonStatusAccuracyStagePolicy(accuracyUser, accuracyTarget,
            statusAccuracyBase, statusAccuracyComposed) || statusAccuracyComposed.accuracyMultiplier != 1.0 / 6.0) return 9391;
    statusAccuracyComposed.accuracyMultiplier = 42;
    accuracyUser.statStages[5] = 7;
    if (Pokerogue3DS::composePokemonStatusAccuracyStagePolicy(accuracyUser, accuracyTarget,
            statusAccuracyBase, statusAccuracyComposed) || statusAccuracyComposed.accuracyMultiplier != 42) return 9392;

    accuracyUser.statStages[5] = 0;
    accuracyTarget.statStages[6] = 6;
    if (!Pokerogue3DS::composePokemonStatusAccuracyStagePolicy(accuracyUser, accuracyTarget,
            statusAccuracyBase, statusAccuracyComposed, false, true) ||
        statusAccuracyComposed.accuracyMultiplier != 0.5) return 9410;
    accuracyTarget.statStages[6] = -6;
    if (!Pokerogue3DS::composePokemonStatusAccuracyStagePolicy(accuracyUser, accuracyTarget,
            statusAccuracyBase, statusAccuracyComposed, false, true) ||
        statusAccuracyComposed.accuracyMultiplier != 0.5) return 9411;
    accuracyUser.statStages[5] = -6;
    accuracyTarget.statStages[6] = 0;
    if (!Pokerogue3DS::composePokemonStatusAccuracyStagePolicy(accuracyUser, accuracyTarget,
            statusAccuracyBase, statusAccuracyComposed, true, false) ||
        statusAccuracyComposed.accuracyMultiplier != 0.5) return 9412;
    int16_t resolvedEffectChance = -99;
    if (!Pokerogue3DS::resolvePokemonMoveEffectChance(52, 32, 0, false, resolvedEffectChance) ||
        resolvedEffectChance != 20) return 9440;
    if (!Pokerogue3DS::resolvePokemonMoveEffectChance(52, 32, 19, false, resolvedEffectChance) ||
        resolvedEffectChance != 0) return 9441;
    if (!Pokerogue3DS::resolvePokemonMoveEffectChance(95, 32, 19, false, resolvedEffectChance) ||
        resolvedEffectChance != -1) return 9442;
    if (!Pokerogue3DS::resolvePokemonMoveEffectChance(52, 32, 19, true, resolvedEffectChance) ||
        resolvedEffectChance != 20) return 9443;
    for (const auto moveId : PokerogueContent::kMoveEffectChanceExceptions) {
        const auto* exceptionMove = PokerogueContent::findMoveById(moveId);
        if (!exceptionMove || !Pokerogue3DS::resolvePokemonMoveEffectChance(moveId, 32, 0, false,
                resolvedEffectChance) || resolvedEffectChance != exceptionMove->upstreamChance) return 9444;
    }
    resolvedEffectChance = 123;
    if (Pokerogue3DS::resolvePokemonMoveEffectChance(52, 125, 0, false, resolvedEffectChance) ||
        resolvedEffectChance != 123) return 9445;
    auto shieldedUser = completeStatusUser, shieldedTarget = completeStatusTarget;
    shieldedUser.status = shieldedTarget.status = {};
    shieldedUser.abilityId = 32;
    shieldedTarget.abilityId = 19;
    auto shieldedRng = statusApplicationRng, expectedShieldedRng = shieldedRng;
    auto shieldedReactions = reactionsPolicy;
    shieldedReactions.formsResolved = true;
    Pokerogue3DS::PokemonMoveStatusPhaseEvent shieldedEvent{};
    if (!Pokerogue3DS::resolvePokemonMoveEffectChance(52, 32, 19, false, resolvedEffectChance)) return 9446;
    (void)expectedShieldedRng.randSeedInt(100); // Zero chance still consumes one draw upstream.
    if (!Pokerogue3DS::executePokemonMoveStatusPhase(shieldedUser, shieldedTarget, 52,
            resolvedEffectChance, completeStatusPolicy.move.application, shieldedReactions,
            shieldedRng, shieldedEvent) || shieldedEvent.applied || shieldedTarget.status.present ||
        !shieldedEvent.application.chanceRolled ||
        shieldedRng.randSeedUint32() != expectedShieldedRng.randSeedUint32()) return 9447;

    auto summonHealingActor = completeStatusTarget;
    summonHealingActor.abilityId = 17; // Immunity canonical ID.
    summonHealingActor.status = {};
    summonHealingActor.status.present = true;
    summonHealingActor.status.effect = Effect::Toxic;
    summonHealingActor.status.turnCountPresent = true;
    summonHealingActor.status.turnCount = 3;
    summonHealingActor.confusion = {2, true};
    Pokerogue3DS::PokemonPostSummonStatusHealingEvent summonHealingEvent{};
    if (!Pokerogue3DS::applyPokemonPostSummonStatusHealing(summonHealingActor, true, true, summonHealingEvent) ||
        !summonHealingEvent.abilityActivates || !summonHealingEvent.healed ||
        summonHealingEvent.previous != Effect::Toxic || summonHealingActor.status.present ||
        !summonHealingActor.confusion.present || summonHealingActor.confusion.turns != 2) return 9450;
    summonHealingActor.status.present = true;
    summonHealingActor.status.effect = Effect::Sleep;
    if (!Pokerogue3DS::applyPokemonPostSummonStatusHealing(summonHealingActor, true, true, summonHealingEvent) ||
        summonHealingEvent.healed || summonHealingActor.status.effect != Effect::Sleep) return 9451;
    summonHealingActor.abilityId = 15; // Insomnia.
    if (!Pokerogue3DS::applyPokemonPostSummonStatusHealing(summonHealingActor, false, false, summonHealingEvent) ||
        summonHealingEvent.healed || !summonHealingActor.status.present) return 9452;
    summonHealingEvent.previous = Effect::Burn;
    if (Pokerogue3DS::applyPokemonPostSummonStatusHealing(summonHealingActor, true, false, summonHealingEvent) ||
        !summonHealingActor.status.present || summonHealingEvent.previous != Effect::Burn) return 9453;
    if (!Pokerogue3DS::applyPokemonPostSummonStatusHealing(summonHealingActor, true, true, summonHealingEvent) ||
        !summonHealingEvent.healed || summonHealingActor.status.present) return 9454;
    summonHealingActor.pendingStatus = Effect::Poison;
    if (Pokerogue3DS::applyPokemonPostSummonStatusHealing(summonHealingActor, true, true, summonHealingEvent) ||
        summonHealingActor.pendingStatus != Effect::Poison) return 9455;
    summonHealingActor.pendingStatus = Effect::None;
    bool canonicalImmunity = false;
    if (Pokerogue3DS::resolvePokemonStatusAbilityImmunity(17, Effect::Poison, true, false, true,
            canonicalImmunity) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !canonicalImmunity) return 9456;
    if (Pokerogue3DS::resolvePokemonStatusAbilityImmunity(15, Effect::Sleep, true, false, true,
            canonicalImmunity) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !canonicalImmunity) return 9457;

    bool ownTempoActionFound = false, puppeteerActionFound = false;
    for (const auto& profile : PokerogueContent::kStatusActionAbilityProfiles) {
        if (profile.abilityId == 20) ownTempoActionFound = profile.resolved;
        if (profile.abilityId == 310) puppeteerActionFound = profile.resolved;
    }
    if (!ownTempoActionFound || !puppeteerActionFound) return 9460;
    Pokerogue3DS::PokemonConfusionTagPolicy ownTempoEnvironment{}, ownTempoPolicy{};
    ownTempoEnvironment.resolved = true;
    const Pokerogue3DS::PokemonStatusAbilityComponent ownTempoComponent[] = {{20, true, true}};
    if (Pokerogue3DS::composePokemonConfusionTagPolicy(ownTempoEnvironment, ownTempoComponent, 1,
            nullptr, 0, ownTempoPolicy) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved ||
        !ownTempoPolicy.ownAbilityBlocks) return 9461;
    auto ownTempoActor = completeStatusTarget;
    ownTempoActor.abilityId = 20;
    ownTempoActor.confusion = {3, true};
    Pokerogue3DS::PokemonConfusionRemovalEvent ownTempoRemoval{};
    if (Pokerogue3DS::applyPokemonPostSummonConfusionRemoval(ownTempoActor, true, true, ownTempoRemoval) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !ownTempoRemoval.abilityActivates ||
        !ownTempoRemoval.removed || ownTempoActor.confusion.present || ownTempoActor.confusion.turns) return 9462;

    auto confuseMoveTarget = completeStatusTarget;
    confuseMoveTarget.status = {};
    confuseMoveTarget.confusion = {};
    Pokerogue3DS::PokemonConfusionTagPolicy confuseMovePolicy{};
    confuseMovePolicy.resolved = true;
    Pokerogue3DS::PokemonMoveConfusionEvent confuseMoveEvent{};
    auto confuseMoveRng = statusApplicationRng, expectedConfuseMoveRng = confuseMoveRng;
    const auto confuseMoveDuration = expectedConfuseMoveRng.randSeedIntRange(2, 5);
    if (!Pokerogue3DS::applyPokemonMoveConfusion(confuseMoveTarget, 93, 100, false,
            confuseMovePolicy, confuseMoveRng, confuseMoveEvent, 0, true) || !confuseMoveEvent.tagAttempted ||
        confuseMoveEvent.tagResult != Pokerogue3DS::PokemonConfusionTagResult::Added ||
        confuseMoveTarget.confusion.turns != confuseMoveDuration ||
        !confuseMoveTarget.confusion.sourceMoveResolved || confuseMoveTarget.confusion.sourceMoveId != 93 ||
        !confuseMoveTarget.confusion.sourcePokemonResolved || confuseMoveTarget.confusion.sourcePokemonId ||
        confuseMoveRng.randSeedUint32() != expectedConfuseMoveRng.randSeedUint32()) return 9470;
    expectedConfuseMoveRng = confuseMoveRng;
    (void)expectedConfuseMoveRng.randSeedIntRange(2, 5);
    if (!Pokerogue3DS::applyPokemonMoveConfusion(confuseMoveTarget, 60, 100, false,
            confuseMovePolicy, confuseMoveRng, confuseMoveEvent, 0xffffffffu, true) ||
        confuseMoveEvent.tagResult != Pokerogue3DS::PokemonConfusionTagResult::Overlap ||
        confuseMoveTarget.confusion.turns != confuseMoveDuration ||
        !confuseMoveTarget.confusion.sourceMoveResolved || confuseMoveTarget.confusion.sourceMoveId != 93 ||
        !confuseMoveTarget.confusion.sourcePokemonResolved || confuseMoveTarget.confusion.sourcePokemonId ||
        confuseMoveRng.randSeedUint32() != expectedConfuseMoveRng.randSeedUint32()) return 9471;
    // Expiry and explicit removal clear source metadata as well as duration.
    auto sourcedTag = confuseMoveTarget.confusion;
    sourcedTag.turns = 1;
    Pokerogue3DS::PokemonConfusionMovePolicy expiryPolicy{};
    expiryPolicy.resolved = true;
    expiryPolicy.effectiveAttack = expiryPolicy.effectiveDefense = 1;
    Pokerogue3DS::PokemonConfusionMoveEvent expiryEvent{};
    auto expiryActor = confuseMoveTarget;
    auto expiryRng = confuseMoveRng, expectedExpiryRng = expiryRng;
    if (!Pokerogue3DS::checkPokemonConfusionBeforeMove(expiryActor, sourcedTag,
            expiryPolicy, expiryRng, expiryEvent) || !expiryEvent.removed || sourcedTag.present ||
        sourcedTag.sourceMoveResolved || sourcedTag.sourceMoveId ||
        sourcedTag.sourcePokemonResolved || sourcedTag.sourcePokemonId ||
        expiryRng.randSeedUint32() != expectedExpiryRng.randSeedUint32()) return 9516;
    sourcedTag = confuseMoveTarget.confusion;
    if (!Pokerogue3DS::removePokemonConfusionTag(sourcedTag) || sourcedTag.present ||
        sourcedTag.sourceMoveResolved || sourcedTag.sourceMoveId ||
        sourcedTag.sourcePokemonResolved || sourcedTag.sourcePokemonId) return 9517;
    sourcedTag.sourceMoveId = 93;
    if (Pokerogue3DS::validPokemonConfusionTag(sourcedTag)) return 9518;
    confuseMoveTarget.confusion = {};
    confuseMovePolicy.ownAbilityBlocks = true;
    expectedConfuseMoveRng = confuseMoveRng;
    (void)expectedConfuseMoveRng.randSeedIntRange(2, 5);
    if (!Pokerogue3DS::applyPokemonMoveConfusion(confuseMoveTarget, 93, 100, false,
            confuseMovePolicy, confuseMoveRng, confuseMoveEvent) || confuseMoveTarget.confusion.present ||
        confuseMoveEvent.tagResult != Pokerogue3DS::PokemonConfusionTagResult::OwnAbility ||
        confuseMoveRng.randSeedUint32() != expectedConfuseMoveRng.randSeedUint32()) return 9472;
    expectedConfuseMoveRng = confuseMoveRng;
    if (!Pokerogue3DS::applyPokemonMoveConfusion(confuseMoveTarget, 93, 10, true,
            confuseMovePolicy, confuseMoveRng, confuseMoveEvent) || !confuseMoveEvent.safeguardBlocked ||
        confuseMoveEvent.chanceRolled || confuseMoveEvent.tagAttempted ||
        confuseMoveRng.randSeedUint32() != expectedConfuseMoveRng.randSeedUint32()) return 9473;
    confuseMoveTarget.hp = 0;
    expectedConfuseMoveRng = confuseMoveRng;
    if (!Pokerogue3DS::applyPokemonMoveConfusion(confuseMoveTarget, 93, 10, false,
            confuseMovePolicy, confuseMoveRng, confuseMoveEvent) || !confuseMoveEvent.targetFainted ||
        confuseMoveEvent.chanceRolled ||
        confuseMoveRng.randSeedUint32() != expectedConfuseMoveRng.randSeedUint32()) return 9474;
    confuseMoveTarget.hp = confuseMoveTarget.maxHp;
    confuseMovePolicy.ownAbilityBlocks = false;
    expectedConfuseMoveRng = confuseMoveRng;
    (void)expectedConfuseMoveRng.randSeedInt(100);
    if (!Pokerogue3DS::applyPokemonMoveConfusion(confuseMoveTarget, 93, 0, false,
            confuseMovePolicy, confuseMoveRng, confuseMoveEvent) || !confuseMoveEvent.chanceRolled ||
        confuseMoveEvent.tagAttempted || confuseMoveTarget.confusion.present ||
        confuseMoveRng.randSeedUint32() != expectedConfuseMoveRng.randSeedUint32()) return 9475;
    double confuseAiBenefit = 123;
    if (!Pokerogue3DS::calculatePokemonConfusionMoveAiBenefit(93, 10, confuseAiBenefit) ||
        confuseAiBenefit != -1 || !Pokerogue3DS::calculatePokemonConfusionMoveAiBenefit(60, -1, confuseAiBenefit) ||
        confuseAiBenefit != -5) return 9476;

    bool foundStarterStatusCapability = false, foundPendingStatusCapability = false;
    for (const auto& profile : PokerogueContent::kStatusActionAbilityProfiles) {
        // Canonical IDs: Overgrow only changes power. Keen Eye now carries
        // its explicit evasion bypass; Unaware damage-stage bypass is pending.
        if (profile.abilityId == 65) {
            if (!profile.resolved) return 9400;
            foundStarterStatusCapability = true;
        }
        if (profile.abilityId == 51 && (!profile.resolved || !profile.ignoresOpponentEvasion ||
                profile.ignoresOpponentAccuracy)) return 9401;
        if (profile.abilityId == 109) {
            if (profile.resolved) return 9401;
            foundPendingStatusCapability = true;
        }
    }
    if (!foundStarterStatusCapability || !foundPendingStatusCapability) return 9402;

    PokemonBattleState scoredStatusActor{};
    scoredStatusActor.hp = scoredStatusActor.maxHp = 100;
    Pokerogue3DS::PokemonStatusApplicationPolicy scoredStatusPolicy{};
    scoredStatusPolicy.resolved = scoredStatusPolicy.grounded = scoredStatusPolicy.mistyTerrain = true;
    double statusAiBenefit = 123;
    if (!Pokerogue3DS::calculatePokemonStatusEffectAiBenefit(scoredStatusActor, 95, -1, true,
            scoredStatusPolicy, statusAiBenefit) || statusAiBenefit != -10) return 9360;
    if (!Pokerogue3DS::calculatePokemonStatusEffectAiBenefit(scoredStatusActor, 95, 25, true,
            scoredStatusPolicy, statusAiBenefit) || statusAiBenefit != -3) return 9361;
    scoredStatusActor.status.present = true;
    scoredStatusActor.status.effect = Effect::Burn;
    if (!Pokerogue3DS::calculatePokemonStatusEffectAiBenefit(scoredStatusActor, 95, 100, true,
            scoredStatusPolicy, statusAiBenefit) || statusAiBenefit != 0) return 9362;
    statusAiBenefit = 123;
    if (Pokerogue3DS::calculatePokemonStatusEffectAiBenefit(scoredStatusActor, 95, 100, false,
            scoredStatusPolicy, statusAiBenefit) || statusAiBenefit != 123) return 9363;
    bool foundStatusConfusion = false;
    for (const auto& profile : PokerogueContent::kStatusConfusionAbilityProfiles) {
        if (!profile.resolved || !(profile.statusMask & 2)) continue;
        foundStatusConfusion = true;
        PokemonBattleState confusionSource{}, confusionRecipient{};
        confusionSource.abilityId = profile.abilityId;
        confusionSource.pokemonId = 41;
        confusionRecipient.pokemonId = 42;
        confusionRecipient.hp = confusionRecipient.maxHp = 100;
        Pokerogue3DS::PokemonStatusConfusionReactionPolicy confusionPolicy{};
        confusionPolicy.resolved = confusionPolicy.abilityActive = confusionPolicy.targetCanAddConfusion = true;
        Pokerogue3DS::PokemonStatusConfusionReactionEvent confusionEvent{};
        expectedApplicationRng = statusApplicationRng;
        const uint32_t expectedConfusionTurns = expectedApplicationRng.randSeedIntRange(2, 5);
        if (Pokerogue3DS::resolvePokemonStatusConfusionReaction(confusionSource, confusionRecipient, Effect::Poison,
                confusionPolicy, statusApplicationRng, confusionEvent) !=
                    Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !confusionEvent.requestConfusionTag ||
            confusionEvent.turns != expectedConfusionTurns || confusionEvent.targetPokemonId != 42 ||
            confusionEvent.sourcePokemonId != 42 ||
            statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9110;
        confusionPolicy.targetCanAddConfusion = false;
        expectedApplicationRng = statusApplicationRng;
        if (Pokerogue3DS::resolvePokemonStatusConfusionReaction(confusionSource, confusionRecipient, Effect::Poison,
                confusionPolicy, statusApplicationRng, confusionEvent) !=
                    Pokerogue3DS::PokemonStatusImmunityResult::Resolved || confusionEvent.requestConfusionTag ||
            statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9111;
        confusionPolicy.targetCanAddConfusion = true;
        confusionPolicy.simulated = true;
        expectedApplicationRng = statusApplicationRng;
        if (Pokerogue3DS::resolvePokemonStatusConfusionReaction(confusionSource, confusionRecipient, Effect::Poison,
                confusionPolicy, statusApplicationRng, confusionEvent) !=
                    Pokerogue3DS::PokemonStatusImmunityResult::Resolved || confusionEvent.requestConfusionTag ||
            statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9112;
        confusionPolicy.resolved = false;
        confusionEvent.turns = 123;
        if (Pokerogue3DS::resolvePokemonStatusConfusionReaction(confusionSource, confusionRecipient, Effect::Poison,
                confusionPolicy, statusApplicationRng, confusionEvent) !=
                    Pokerogue3DS::PokemonStatusImmunityResult::UnsupportedCondition || confusionEvent.turns != 123)
            return 9113;
    }
    if (!foundStatusConfusion) return 9114;
    PokemonBattleState puppeteerSource{}, puppeteerTarget{};
    puppeteerSource.abilityId = 310;
    puppeteerTarget.pokemonId = 72;
    puppeteerTarget.hp = puppeteerTarget.maxHp = 100;
    Pokerogue3DS::PokemonConfusionTagState puppeteerTag{};
    Pokerogue3DS::PokemonConfusionTagPolicy probeConfusion{}, applyConfusion{};
    probeConfusion.resolved = applyConfusion.resolved = true;
    applyConfusion.grounded = applyConfusion.mistyTerrain = true;
    Pokerogue3DS::PokemonStatusConfusionCommandEvent puppeteerEvent{};
    expectedApplicationRng = statusApplicationRng;
    const auto mistyDuration = expectedApplicationRng.randSeedIntRange(2, 5);
    if (Pokerogue3DS::executePokemonStatusConfusionReaction(puppeteerSource, puppeteerTarget,
            Effect::Poison, true, false, probeConfusion, applyConfusion, puppeteerTag,
            statusApplicationRng, puppeteerEvent) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved ||
        !puppeteerEvent.tagAttempted || puppeteerTag.present || puppeteerEvent.reaction.turns != mistyDuration ||
        puppeteerEvent.tagResult != Pokerogue3DS::PokemonConfusionTagResult::MistyTerrain ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9150;
    applyConfusion.grounded = false;
    if (Pokerogue3DS::executePokemonStatusConfusionReaction(puppeteerSource, puppeteerTarget,
            Effect::Toxic, true, false, probeConfusion, applyConfusion, puppeteerTag,
            statusApplicationRng, puppeteerEvent) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved ||
        !puppeteerTag.present || puppeteerTag.turns != puppeteerEvent.reaction.turns ||
        puppeteerEvent.tagResult != Pokerogue3DS::PokemonConfusionTagResult::Added) return 9151;
    expectedApplicationRng = statusApplicationRng;
    if (Pokerogue3DS::executePokemonStatusConfusionReaction(puppeteerSource, puppeteerTarget,
            Effect::Toxic, true, false, probeConfusion, applyConfusion, puppeteerTag,
            statusApplicationRng, puppeteerEvent) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved ||
        puppeteerEvent.tagAttempted ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9152;
    puppeteerTag = {};
    applyConfusion.resolved = false;
    puppeteerEvent.tagAttempted = false;
    expectedApplicationRng = statusApplicationRng;
    if (Pokerogue3DS::executePokemonStatusConfusionReaction(puppeteerSource, puppeteerTarget,
            Effect::Toxic, true, false, probeConfusion, applyConfusion, puppeteerTag,
            statusApplicationRng, puppeteerEvent) != Pokerogue3DS::PokemonStatusImmunityResult::UnsupportedCondition ||
        puppeteerTag.present || puppeteerEvent.tagAttempted ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9153;
    PokemonBattleState ownTempoActor{};
    ownTempoActor.abilityId = 20;
    ownTempoActor.confusion = {3, true};
    Pokerogue3DS::PokemonConfusionRemovalEvent removalEvent{};
    if (Pokerogue3DS::applyPokemonPostSummonConfusionRemoval(ownTempoActor, false, true, removalEvent) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || removalEvent.removed ||
        ownTempoActor.confusion.turns != 3) return 9200;
    if (Pokerogue3DS::applyPokemonPostSummonConfusionRemoval(ownTempoActor, true, false, removalEvent) !=
            Pokerogue3DS::PokemonStatusImmunityResult::UnsupportedCondition || ownTempoActor.confusion.turns != 3)
        return 9201;
    if (Pokerogue3DS::applyPokemonPostSummonConfusionRemoval(ownTempoActor, true, true, removalEvent) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !removalEvent.abilityActivates ||
        !removalEvent.removed || ownTempoActor.confusion.present || ownTempoActor.confusion.turns) return 9202;
    if (Pokerogue3DS::applyPokemonPostSummonConfusionRemoval(ownTempoActor, true, true, removalEvent) !=
            Pokerogue3DS::PokemonStatusImmunityResult::Resolved || removalEvent.abilityActivates) return 9203;
    probeConfusion.resolved = applyConfusion.resolved = false;
    expectedApplicationRng = statusApplicationRng;
    if (Pokerogue3DS::executePokemonStatusConfusionReaction(puppeteerSource, puppeteerTarget,
            Effect::Burn, true, false, probeConfusion, applyConfusion, puppeteerTag,
            statusApplicationRng, puppeteerEvent) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved ||
        puppeteerEvent.tagAttempted ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9260;
    if (Pokerogue3DS::executePokemonStatusConfusionReaction(puppeteerSource, puppeteerTarget,
            Effect::Poison, false, false, probeConfusion, applyConfusion, puppeteerTag,
            statusApplicationRng, puppeteerEvent) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved ||
        puppeteerEvent.tagAttempted) return 9261;
    puppeteerTarget.hp = 0;
    if (Pokerogue3DS::executePokemonStatusConfusionReaction(puppeteerSource, puppeteerTarget,
            Effect::Poison, true, false, probeConfusion, applyConfusion, puppeteerTag,
            statusApplicationRng, puppeteerEvent) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved ||
        puppeteerEvent.tagAttempted) return 9262;
    puppeteerTarget.hp = 100;
    if (Pokerogue3DS::executePokemonStatusConfusionReaction(puppeteerSource, puppeteerTarget,
            Effect::Poison, true, true, probeConfusion, applyConfusion, puppeteerTag,
            statusApplicationRng, puppeteerEvent) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved ||
        puppeteerEvent.tagAttempted) return 9263;
    Pokerogue3DS::PokemonConfusionTagPolicy confusionEnvironment{}, composedConfusion{};
    confusionEnvironment.resolved = confusionEnvironment.grounded = confusionEnvironment.mistyTerrain = true;
    const Pokerogue3DS::PokemonStatusAbilityComponent tempoComponents[] = {{20, true, true}};
    if (Pokerogue3DS::composePokemonConfusionTagPolicy(confusionEnvironment, tempoComponents, 1, nullptr, 0,
            composedConfusion) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved ||
        !composedConfusion.ownAbilityBlocks || composedConfusion.allyAbilityBlocks ||
        !composedConfusion.grounded || !composedConfusion.mistyTerrain) return 9270;
    const Pokerogue3DS::PokemonStatusAbilityComponent unknownTempo[] = {{20, true, false}};
    composedConfusion.ownAbilityBlocks = false;
    if (Pokerogue3DS::composePokemonConfusionTagPolicy(confusionEnvironment, unknownTempo, 1, nullptr, 0,
            composedConfusion) != Pokerogue3DS::PokemonStatusImmunityResult::UnsupportedCondition ||
        composedConfusion.ownAbilityBlocks) return 9271;
    const Pokerogue3DS::PokemonStatusAbilityComponent inactiveTempo[] = {{20, false, true}};
    if (Pokerogue3DS::composePokemonConfusionTagPolicy(confusionEnvironment, inactiveTempo, 1, tempoComponents, 1,
            composedConfusion) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved ||
        composedConfusion.ownAbilityBlocks || composedConfusion.allyAbilityBlocks) return 9272;
    const char* recipientTypes[] = {"NORMAL"};
    Pokerogue3DS::PokemonStatusFieldContext sharedField{};
    sharedField.resolved = sharedField.grounded = sharedField.mistyTerrain = sharedField.ignoreField = true;
    sharedField.effectiveTypes = sharedField.originalIfStellarTypes = recipientTypes;
    sharedField.effectiveTypeCount = sharedField.originalIfStellarTypeCount = 1;
    Pokerogue3DS::PokemonStatusRecipientPolicies recipientPolicies{};
    PokemonBattleState policyRecipient{}, policySource{};
    policyRecipient.hp = policyRecipient.maxHp = 100;
    const Pokerogue3DS::PokemonStatusAbilityComponent neutralComponents[] = {{0, true, true}};
    if (!Pokerogue3DS::resolvePokemonStatusRecipientPolicies(policyRecipient, &policySource, Effect::Burn,
            sharedField, neutralComponents, 1, nullptr, 0, neutralComponents, 1, recipientPolicies) ||
        !recipientPolicies.status.ignoreField || !recipientPolicies.confusion.mistyTerrain ||
        !recipientPolicies.confusion.grounded || recipientPolicies.confusion.ownAbilityBlocks) return 9280;
    Pokerogue3DS::PokemonConfusionTagState policyTag{};
    if (Pokerogue3DS::canPokemonSetStatus(policyRecipient.status, Effect::Burn, recipientPolicies.status) !=
            Pokerogue3DS::PokemonStatusEligibility::Allowed ||
        Pokerogue3DS::addPokemonConfusionTag(policyTag, 3, recipientPolicies.confusion) !=
            Pokerogue3DS::PokemonConfusionTagResult::MistyTerrain) return 9281;
    recipientPolicies.confusion.ownAbilityBlocks = true;
    if (Pokerogue3DS::resolvePokemonStatusRecipientPolicies(policyRecipient, &policySource, Effect::Burn,
            sharedField, unknownTempo, 1, nullptr, 0, neutralComponents, 1, recipientPolicies) ||
        !recipientPolicies.confusion.ownAbilityBlocks) return 9282;
    bool foundConfusionImmunity = false;
    for (const auto& profile : PokerogueContent::kConfusionImmunityAbilityProfiles) {
        if (!profile.selfResolved || !profile.selfBlocks) continue;
        foundConfusionImmunity = true;
        bool blocksConfusion = false;
        if (Pokerogue3DS::resolvePokemonConfusionAbilityImmunity(profile.abilityId, true, false, true,
                blocksConfusion) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved || !blocksConfusion)
            return 9140;
        if (Pokerogue3DS::resolvePokemonConfusionAbilityImmunity(profile.abilityId, false, false, true,
                blocksConfusion) != Pokerogue3DS::PokemonStatusImmunityResult::Resolved || blocksConfusion)
            return 9141;
        blocksConfusion = true;
        if (Pokerogue3DS::resolvePokemonConfusionAbilityImmunity(profile.abilityId, true, false, false,
                blocksConfusion) != Pokerogue3DS::PokemonStatusImmunityResult::UnsupportedCondition ||
            !blocksConfusion) return 9142;
    }
    if (!foundConfusionImmunity) return 9143;
    Pokerogue3DS::PokemonConfusionTagState addedConfusion{};
    Pokerogue3DS::PokemonConfusionTagPolicy tagPolicy{};
    tagPolicy.resolved = tagPolicy.grounded = tagPolicy.mistyTerrain = true;
    bool canAddConfusion = false;
    using TagResult = Pokerogue3DS::PokemonConfusionTagResult;
    if (!Pokerogue3DS::canPokemonAddConfusionTag(addedConfusion, tagPolicy, canAddConfusion) ||
        !canAddConfusion || Pokerogue3DS::addPokemonConfusionTag(addedConfusion, 3, tagPolicy) !=
            TagResult::MistyTerrain || addedConfusion.present) return 9130;
    tagPolicy.grounded = false;
    if (Pokerogue3DS::addPokemonConfusionTag(addedConfusion, 3, tagPolicy) != TagResult::Added ||
        addedConfusion.turns != 3 || !addedConfusion.present) return 9131;
    tagPolicy.resolved = false;
    if (Pokerogue3DS::addPokemonConfusionTag(addedConfusion, 5, tagPolicy) != TagResult::Overlap ||
        addedConfusion.turns != 3 ||
        !Pokerogue3DS::canPokemonAddConfusionTag(addedConfusion, tagPolicy, canAddConfusion) ||
        canAddConfusion) return 9132;
    if (!Pokerogue3DS::removePokemonConfusionTag(addedConfusion) || addedConfusion.present ||
        addedConfusion.turns) return 9133;
    if (Pokerogue3DS::addPokemonConfusionTag(addedConfusion, 3, tagPolicy) != TagResult::Unsupported ||
        addedConfusion.present) return 9134;
    tagPolicy.resolved = tagPolicy.ownAbilityBlocks = true;
    if (Pokerogue3DS::addPokemonConfusionTag(addedConfusion, 3, tagPolicy) != TagResult::OwnAbility)
        return 9135;
    tagPolicy.ownAbilityBlocks = false;
    tagPolicy.allyAbilityBlocks = true;
    if (Pokerogue3DS::addPokemonConfusionTag(addedConfusion, 3, tagPolicy) != TagResult::AllyAbility)
        return 9136;
    // Expiration removes the tag before RNG; unsupported callbacks are atomic.
    PokemonBattleState confusedActor{};
    confusedActor.hp = confusedActor.maxHp = 100;
    confusedActor.level = 50;
    Pokerogue3DS::PokemonConfusionTagState confusedTag{1, true};
    Pokerogue3DS::PokemonConfusionMovePolicy confusedPolicy{true, 100, 100};
    Pokerogue3DS::PokemonConfusionMoveEvent confusedEvent{};
    expectedApplicationRng = statusApplicationRng;
    if (!Pokerogue3DS::checkPokemonConfusionBeforeMove(confusedActor, confusedTag, confusedPolicy,
            statusApplicationRng, confusedEvent) || !confusedEvent.removed || confusedTag.present ||
        confusedEvent.activationRolled || confusedActor.hp != 100 ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9120;
    confusedTag = {3, true};
    expectedApplicationRng = statusApplicationRng;
    const bool expectedSelfHit = expectedApplicationRng.randSeedInt(3) == 0;
    uint32_t expectedSelfDamage = 0;
    if (expectedSelfHit) {
        // At level 50 with equal stats: floor(19.6 * seeded percentage / 100).
        expectedSelfDamage = static_cast<uint32_t>(19.6 *
            expectedApplicationRng.randSeedIntRange(85, 100) / 100.0);
    }
    if (!Pokerogue3DS::checkPokemonConfusionBeforeMove(confusedActor, confusedTag, confusedPolicy,
            statusApplicationRng, confusedEvent) || confusedTag.turns != 2 ||
        confusedEvent.hurtItself != expectedSelfHit || confusedEvent.moveCancelled != expectedSelfHit ||
        confusedEvent.requestedDamage != expectedSelfDamage || confusedActor.hp != 100 - expectedSelfDamage ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9121;
    confusedPolicy.resolved = false;
    confusedEvent.requestedDamage = 123;
    expectedApplicationRng = statusApplicationRng;
    const auto previousConfusedHp = confusedActor.hp;
    if (Pokerogue3DS::checkPokemonConfusionBeforeMove(confusedActor, confusedTag, confusedPolicy,
            statusApplicationRng, confusedEvent) || confusedTag.turns != 2 ||
        confusedActor.hp != previousConfusedHp || confusedEvent.requestedDamage != 123 ||
        statusApplicationRng.randSeedUint32() != expectedApplicationRng.randSeedUint32()) return 9122;

    return 0;
}
