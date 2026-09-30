#include "game/FirstRunRuntime.hpp"
#include "game/PokerogueEncounterResolver.hpp"
#include "game/PokerogueClassicWaveSchedule.hpp"
#include "game/PokerogueTurnOrder.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include "game/PokemonLevelMovePool.hpp"
#include "game/PokemonTrainerMoveFilter.hpp"
#include "game/PokemonTrainerMovesetGenerator.hpp"
#include "game/PokemonTrainerAi.hpp"
#include "game/PokemonStatStageEffect.hpp"
#include "game/PokemonHealingEffect.hpp"
#include "game/PokemonRecoilEffect.hpp"
#include "game/PokemonWeatherPhase.hpp"
#include "game/PokemonFreshProfile.hpp"
#include "game/PokemonExperience.hpp"
#include "game/PokemonStarterMoveset.hpp"
#include "game/PokemonWildMovesetGenerator.hpp"
#include "game/PokerogueTrainerPartyLevels.hpp"
#include "game/PokerogueBiomeTransition.hpp"
#include "game/PokerogueModifierReward.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace Pokerogue3DS {
namespace {
Citro2D::SceneNodeData textNode(const char* id, Citro2D::ScreenTarget screen,
                                float x, float y, int16_t z) {
    Citro2D::SceneNodeData node{};
    node.id = id;
    node.type = Citro2D::NodeType::Text;
    node.screen = screen;
    node.parentIndex = -1;
    node.x = x;
    node.y = y;
    node.width = screen == Citro2D::ScreenTarget::Top ? 400.0f : 320.0f;
    node.height = 24.0f;
    node.scaleX = 1.0f;
    node.scaleY = 1.0f;
    node.opacity = 1.0f;
    node.visible = true;
    node.zIndex = z;
    node.tintColor = 0xFFFFFFFF;
    node.textData = {nullptr, nullptr, 16, 0, 0xFFFFFFFF, 18};
    return node;
}

const PokerogueContent::Entity* findBiomeById(const char* id) {
    if (!id) return nullptr;
    for (const auto& biome : PokerogueContent::kBiomes) {
        if (std::strcmp(biome.id, id) == 0) return &biome;
    }
    return nullptr;
}

const PokerogueContent::Entity* findItemById(const char* id) {
    if (!id) return nullptr;
    for (const auto& item : PokerogueContent::kItems) {
        if (std::strcmp(item.id, id) == 0) return &item;
    }
    return nullptr;
}
}

FirstRunRuntime::FirstRunRuntime(uint32_t seed) {
    m_run = {seed ? seed : 1u, 1, "classic", PokerogueContent::kStartingBiomeId, 0, 0};
    char seedText[11];
    std::snprintf(seedText, sizeof(seedText), "%u", static_cast<unsigned>(m_run.seed));
    while (seedText[m_seedLength] && m_seedLength < m_seedCodeUnits.size()) {
        m_seedCodeUnits[m_seedLength] = static_cast<uint8_t>(seedText[m_seedLength]);
        ++m_seedLength;
    }
    for (std::size_t i = 0; i < PokerogueContent::kSpeciesCount; ++i) {
        if (PokerogueContent::kSpecies[i].freshProfileStarter) {
            m_starterIndex = i;
            break;
        }
    }
    resolve();
    buildScene();
}

bool FirstRunRuntime::cycleStarter(int direction) {
    if (!direction || m_runStarted || !PokerogueContent::kSpeciesCount) return false;
    std::size_t index = m_starterIndex;
    for (std::size_t scanned = 0; scanned < PokerogueContent::kSpeciesCount; ++scanned) {
        index = direction > 0
            ? (index + 1) % PokerogueContent::kSpeciesCount
            : (index + PokerogueContent::kSpeciesCount - 1) % PokerogueContent::kSpeciesCount;
        if (PokerogueContent::kSpecies[index].freshProfileStarter) {
            m_starterIndex = index;
            resolve();
            buildScene();
            return true;
        }
    }
    return false;
}

bool FirstRunRuntime::restoreSetup(uint32_t seed, uint16_t starterDex) {
    if (!seed) return false;
    std::size_t index = 0;
    for (; index < PokerogueContent::kSpeciesCount; ++index) {
        const auto& species = PokerogueContent::kSpecies[index];
        if (species.dex == starterDex && species.freshProfileStarter) break;
    }
    if (index == PokerogueContent::kSpeciesCount) return false;
    m_run.seed = seed;
    m_arenaWeather = {};
    m_trickRoom = {};
    m_runStarted = false;
    m_run.wave = 1;
    m_context.player.totalExperience = 0;
    m_pokeballs = {5, 0, 0, 0, 0, 0};
    m_seedLength = 0;
    m_seedCodeUnits.fill(0);
    char seedText[11];
    std::snprintf(seedText, sizeof(seedText), "%u", static_cast<unsigned>(seed));
    while (m_seedLength < m_seedCodeUnits.size() && seedText[m_seedLength]) {
        m_seedCodeUnits[m_seedLength] = static_cast<uint8_t>(seedText[m_seedLength]);
        ++m_seedLength;
    }
    m_starterIndex = index;
    resolve();
    buildScene();
    return true;
}

void FirstRunRuntime::captureNativeRunSave(NativeRunSave& output) const {
    NativeRunSave value{};
    // v8 cannot serialize doubles, captured party, inventory or later trainer history.
    // Never report a setup checkpoint as a successful save of an active double battle.
    const std::array<uint16_t, 6> initialBalls{5, 0, 0, 0, 0, 0};
    if (m_context.enemy.bossState.segmentCount || m_doubleBattle || m_context.playerPartyCount > 1 || m_pokeballs != initialBalls ||
        m_run.wave > 9 || (m_trainerBattle && m_run.wave != 5)) {
        output = {};
        return;
    }
    if (m_runStarted && !m_checkpointAvailable) {
        output = {};
        return;
    }
    if (makeNativeRunSetupSave(m_run.seed, m_run.starterDex, value) != NativeSaveResult::Ok) {
        output = {};
        return;
    }
    value.wave = m_run.wave;
    value.playerLevel = m_context.player.level;
    value.playerExperience = m_context.player.totalExperience;
    if (m_runStarted && m_encounterResolved && !m_doubleBattle) {
        value.stage = m_battleFinished
            ? (m_playerWon ? (m_experienceGranted ? NativeSaveStage::ExperienceGranted
                                                : NativeSaveStage::BattleWon)
                           : NativeSaveStage::BattleLost)
            : NativeSaveStage::BattleActive;
        value.encounterDex = m_context.enemy.dex;
        value.playerHp = m_context.player.battleState.hp;
        for (uint8_t stat = 0; stat < 7; ++stat) {
            value.playerStatStages[stat] = m_context.player.battleState.statStages[stat];
            value.enemyStatStages[stat] = m_context.enemy.battleState.statStages[stat];
        }
        value.enemyHp = m_context.enemy.battleState.hp;
        value.battleTurn = m_turn;
        value.weatherType = static_cast<uint8_t>(m_arenaWeather.type);
        value.weatherTurnsLeft = m_arenaWeather.turnsLeft;
        value.weatherMaxDuration = m_arenaWeather.maxDuration;
        value.trickRoomTurnsLeft = m_trickRoom.turnsLeft;
        value.trickRoomMaxDuration = m_trickRoom.maxDuration;
        value.trickRoomSourceMoveId = m_trickRoom.sourceMoveId;
        value.trickRoomSourcePokemonId = m_trickRoom.sourcePokemonId;
        value.playerMoveCount = m_context.player.battleState.moveCount;
        value.enemyMoveCount = m_context.enemy.battleState.moveCount;
        for (uint8_t i = 0; i < value.playerMoveCount && i < 4; ++i) {
            value.playerMoveIds[i] = m_context.player.battleState.moves[i].moveId;
            value.playerPp[i] = m_context.player.battleState.moves[i].pp;
        }
        for (uint8_t i = 0; i < value.enemyMoveCount && i < 4; ++i) {
            value.enemyMoveIds[i] = m_context.enemy.battleState.moves[i].moveId;
            value.enemyPp[i] = m_context.enemy.battleState.moves[i].pp;
        }
        if (m_trainerBattle) {
            if (!m_context.trainerPartyBattleStatesResolved ||
                m_context.activeTrainerPartyIndex >= m_context.trainerPartyCount) {
                output = {}; return;
            }
            value.enemySwitchCounter = m_enemySwitchCounter;
            value.trainerTypeId = m_context.trainerTypeId;
            value.trainerPartyCount = m_context.trainerPartyCount;
            value.activeTrainerMember = m_context.activeTrainerPartyIndex;
            for (uint8_t member = 0; member < value.trainerPartyCount; ++member) {
                const auto& actor = member == value.activeTrainerMember
                    ? m_context.enemy : m_context.trainerParty[member];
                auto& saved = value.trainerParty[member];
                for (uint8_t stat = 0; stat < 7; ++stat) saved.statStages[stat] = actor.battleState.statStages[stat];
                saved.speciesDex = actor.dex;
                saved.hp = actor.battleState.hp;
                saved.moveCount = actor.battleState.moveCount;
                for (uint8_t slot = 0; slot < saved.moveCount && slot < 4; ++slot) {
                    saved.moveIds[slot] = actor.battleState.moves[slot].moveId;
                    saved.pp[slot] = actor.battleState.moves[slot].pp;
                }
            }
        }
    }
    output = value;
}

bool FirstRunRuntime::restoreNativeRunSave(const NativeRunSave& save) {
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) != NativeSaveResult::Ok)
        return false;
    FirstRunRuntime candidate(save.seed);
    if (!candidate.restoreNativeRunSaveInPlace(save)) return false;
    *this = candidate;
    // Scene nodes and text pointers belong to their runtime instance. Rebuild
    // after committing so none point at the temporary candidate's storage.
    buildScene();
    return true;
}

bool FirstRunRuntime::restoreNativeRunSaveInPlace(const NativeRunSave& save) {
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) != NativeSaveResult::Ok ||
        save.stage < NativeSaveStage::RunSetup ||
        save.stage > NativeSaveStage::ExperienceGranted) return false;
    // Wave five is currently the only complete deterministic trainer party.
    if (save.trainerPartyCount && save.wave != 5) return false;
    if (!restoreSetup(save.seed, save.starterDex)) return false;
    // A skipped reward adds no modifier or party member. Replay each earlier
    // supported wild/fixed trainer victory to reconstruct level/EXP;
    // saved HP and PP are overlaid only after the target encounter is rebuilt.
    for (uint16_t wave = 1; wave < save.wave; ++wave) {
        if (!m_encounterResolved) return false;
        if (m_trainerBattle) {
            if (wave != 5 || !m_context.trainerPartyBattleStatesResolved ||
                !m_context.trainerPartyCount) return false;
            for (uint8_t member = 0; member < m_context.trainerPartyCount; ++member) {
                m_context.enemy = m_context.trainerParty[member];
                m_experienceGranted = false;
                if (!grantVictoryExperience()) return false;
            }
        } else if (!grantVictoryExperience()) return false;
        m_run.wave = static_cast<uint16_t>(wave + 1);
        resolve(true);
    }
    if (save.wave != m_run.wave) return false;
    if (m_trainerBattle != (save.trainerPartyCount != 0)) return false;
    if (save.trainerPartyCount) {
        if (!m_context.trainerPartyBattleStatesResolved ||
            save.trainerTypeId != m_context.trainerTypeId ||
            save.trainerPartyCount != m_context.trainerPartyCount) return false;
        for (uint8_t member = 0; member < save.trainerPartyCount; ++member) {
            const auto& saved = save.trainerParty[member];
            const auto& actor = m_context.trainerParty[member];
            if (saved.speciesDex != actor.dex || saved.hp > actor.battleState.maxHp ||
                saved.moveCount != actor.battleState.moveCount) return false;
            for (uint8_t slot = 0; slot < saved.moveCount; ++slot) {
                const auto& move = actor.battleState.moves[slot];
                if (saved.moveIds[slot] != move.moveId || saved.pp[slot] > move.maxPp)
                    return false;
            }
        }
    }
    if (save.trainerPartyCount) {
        // The supported fixed party has no revives, reward modifiers or player
        // switches. Each fainted reserve has already awarded EXP exactly once.
        // Recompute from canonical definitions rather than trusting saved EXP.
        const ResolvedPokemon initialEnemy = m_context.enemy;
        for (uint8_t member = 0; member < save.trainerPartyCount; ++member) {
            if (save.trainerParty[member].hp ||
                (save.stage == NativeSaveStage::BattleWon &&
                 member == save.activeTrainerMember)) continue;
            m_context.enemy = m_context.trainerParty[member];
            m_experienceGranted = false;
            if (!grantVictoryExperience()) return false;
        }
        m_context.enemy = initialEnemy;
        m_experienceGranted = false;
    }
    const auto& reconstructedEnemy = save.trainerPartyCount
        ? m_context.trainerParty[save.activeTrainerMember] : m_context.enemy;

    if (!save.trainerPartyCount && save.stage == NativeSaveStage::ExperienceGranted &&
        !grantVictoryExperience()) return false;
    if (save.playerLevel != m_context.player.level ||
        save.playerExperience != m_context.player.totalExperience) return false;
    if (save.stage == NativeSaveStage::RunSetup) return save.wave == 1;
    if (!m_encounterResolved || m_doubleBattle || save.encounterDex != reconstructedEnemy.dex ||
        !save.battleTurn || !save.playerMoveCount || save.playerMoveCount > 4 ||
        !save.enemyMoveCount || save.enemyMoveCount > 4 ||
        save.playerMoveCount != m_context.player.battleState.moveCount ||
        save.enemyMoveCount != reconstructedEnemy.battleState.moveCount ||
        save.playerHp > m_context.player.battleState.maxHp ||
        save.enemyHp > reconstructedEnemy.battleState.maxHp ||
        (save.stage == NativeSaveStage::BattleActive && (!save.playerHp || !save.enemyHp)) ||
        ((save.stage == NativeSaveStage::BattleWon ||
          save.stage == NativeSaveStage::ExperienceGranted) && (save.enemyHp || !save.playerHp)) ||
        (save.stage == NativeSaveStage::BattleLost && (save.playerHp || !save.enemyHp))) return false;
    for (uint8_t i = 0; i < save.playerMoveCount; ++i) {
        const auto& move = m_context.player.battleState.moves[i];
        if (save.playerMoveIds[i] != move.moveId || save.playerPp[i] > move.maxPp) return false;
    }
    for (uint8_t i = 0; i < save.enemyMoveCount; ++i) {
        const auto& move = reconstructedEnemy.battleState.moves[i];
        if (save.enemyMoveIds[i] != move.moveId || save.enemyPp[i] > move.maxPp) return false;
    }
    // Run commands are accepted only between turns. The pinned battle stream
    // is re-seeded from battleSeed + turn index, so no mid-turn Alea state is
    // needed in a portable checkpoint.
    if (!m_battleRng.beginTurn(save.battleTurn)) return false;
    if (save.trainerPartyCount) {
        m_context.enemy = reconstructedEnemy;
        m_run.encounterDex = m_context.enemy.dex;
    }
    m_context.player.battleState.hp = save.playerHp;
    for (uint8_t stat = 0; stat < 7; ++stat) {
        m_context.player.battleState.statStages[stat] = save.playerStatStages[stat];
        m_context.enemy.battleState.statStages[stat] = save.enemyStatStages[stat];
    }
    m_context.enemy.battleState.hp = save.enemyHp;
    for (uint8_t i = 0; i < save.playerMoveCount; ++i)
        m_context.player.battleState.moves[i].pp = save.playerPp[i];
    for (uint8_t i = 0; i < save.enemyMoveCount; ++i)
        m_context.enemy.battleState.moves[i].pp = save.enemyPp[i];
    if (save.trainerPartyCount) {
        for (uint8_t member = 0; member < save.trainerPartyCount; ++member) {
            auto& actor = m_context.trainerParty[member];
            const auto& saved = save.trainerParty[member];
            actor.battleState.hp = saved.hp;
            for (uint8_t stat = 0; stat < 7; ++stat) actor.battleState.statStages[stat] = saved.statStages[stat];
            for (uint8_t slot = 0; slot < saved.moveCount; ++slot)
                actor.battleState.moves[slot].pp = saved.pp[slot];
        }
        m_context.activeTrainerPartyIndex = save.activeTrainerMember;
        m_context.trainerParty[save.activeTrainerMember] = m_context.enemy;
    }
    m_trickRoom = {save.trickRoomTurnsLeft, save.trickRoomMaxDuration,
        save.trickRoomSourceMoveId, save.trickRoomSourcePokemonId};
    m_arenaWeather = {static_cast<PokemonEffectiveWeather>(save.weatherType),
        save.weatherTurnsLeft, save.weatherMaxDuration};
    if (m_arenaWeather.type != PokemonEffectiveWeather::None && !weatherBattleSupported()) return false;
    m_turn = save.battleTurn;
    m_enemySwitchCounter = save.enemySwitchCounter;
    if (save.trainerPartyCount) refreshTrainerBaselineMatchups();
    m_runStarted = true;
    m_checkpointAvailable = true;
    m_battleFinished = save.stage == NativeSaveStage::BattleWon ||
        save.stage == NativeSaveStage::ExperienceGranted || save.stage == NativeSaveStage::BattleLost;
    m_playerWon = save.stage == NativeSaveStage::BattleWon ||
        save.stage == NativeSaveStage::ExperienceGranted;
    m_experienceGranted = save.stage == NativeSaveStage::ExperienceGranted;
    m_victoryPlan = {};
    if (m_playerWon && enemyPartyDefeated() && !planClassicVictory(m_run.wave, m_victoryPlan)) return false;
    m_battleFeedback = m_battleFinished ? (m_playerWon ? "Restored: wild battle won" : "Restored: Pokemon fainted")
                                       : (m_trainerBattle ? "Trainer party restored; battle pending" : "Battle progress restored");
    buildScene();
    return true;
}

void FirstRunRuntime::setStorageFeedback(const char* message) {
    m_text[7] = message ? message : "";
    m_nodes[7].text = m_text[7].c_str();
}

const char* FirstRunRuntime::starterName() const {
    return m_context.player.localizedName;
}

const char* FirstRunRuntime::locale(const char* canonicalId, const char* fallback) {
    const std::string key = std::string("en:") + canonicalId;
    for (std::size_t i = 0; i < PokerogueContent::kLocaleCount; ++i) {
        if (key == PokerogueContent::kLocales[i].id) return PokerogueContent::kLocales[i].name;
    }
    return fallback;
}

namespace {
bool supportsPokemonStatStageMove(uint16_t moveId) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || !move->target ||
        move->attributeCount != 1 ||
        !PokerogueContent::moveHasAttribute(*move, "StatStageChangeAttr")) return false;
    const bool self = std::strcmp(move->target, "USER") == 0;
    if (!self && std::strcmp(move->target, "NEAR_OTHER") != 0 &&
        std::strcmp(move->target, "NEAR_ENEMY") != 0 &&
        std::strcmp(move->target, "ALL_NEAR_ENEMIES") != 0) return false;
    unsigned count = 0;
    for (const auto& effect : PokerogueContent::kMoveStatStageEffects)
        if (effect.moveId == moveId && effect.selfTarget == self) ++count;
    return count == 1;
}
bool supportsBaselineBattleMove(uint16_t moveId) {
    if (pokemonWeatherChangeProfile(moveId) || supportsPokemonTrickRoomMove(moveId) || supportsPokemonStatStageMove(moveId) || selfHealingProfile(moveId) || damageDrainProfile(moveId) || damageRecoilProfile(moveId)) return true;
    const auto* move = PokerogueContent::findMoveById(moveId);
    // This first resolver only executes plain, single-target damaging moves.
    // Only plain damage or a single migrated weather/critical attribute is
    // eligible; other declared attributes/flags still need their own port.
    return move && move->category != PokerogueContent::MoveStatus && move->power > 0 &&
        move->type && move->target && std::strcmp(move->target, "NEAR_OTHER") == 0 &&
        move->upstreamFlags == 0 &&
        (move->attributeCount == 0 || (move->attributeCount == 1 &&
            (PokerogueContent::moveHasAttribute(*move, "OverrideWeatherMultiplierAttr") ||
             PokerogueContent::moveHasAttribute(*move, "HighCritAttr") ||
             PokerogueContent::moveHasAttribute(*move, "CritOnlyAttr"))));
}

double baselineEnemyMoveScore(const PokemonBattleState& user,
                              const PokemonBattleState& target,
                              const PokerogueContent::Move& move) {
    if (move.category == PokerogueContent::MoveStatus) {
        if (pokemonWeatherChangeProfile(move.id)) return 0.0; // Inherited MoveEffectAttr benefit.
        if (supportsPokemonTrickRoomMove(move.id)) return 0.0; // Inherited MoveAttr benefits.
        double score = 0;
        if (canonicalSelfHealingAiScore(user, move.id, score)) return score;
        return calculateCanonicalStatStageStatusAiScore(user, target, move.id, score)
            ? score : -20.0;
    }
    double effectiveness = 1.0;
    if (calculatePokemonTypeEffectiveness(move.id, target, effectiveness) !=
        PokemonTypeEffectivenessResult::Ok) return -20.0;
    const bool physical = move.category == PokerogueContent::MovePhysical;
    const uint16_t selectedStat = user.stats[physical ? 1 : 3];
    const uint16_t otherStat = user.stats[physical ? 3 : 1];
    if (!selectedStat) return -20.0;
    const auto* species = PokerogueContent::findSpeciesByDex(user.speciesDex);
    const auto* form = user.formId ? PokerogueContent::findFormById(user.formId) : nullptr;
    const char* type1 = form ? form->type1 : (species ? species->type1 : nullptr);
    const char* type2 = form ? form->type2 : (species ? species->type2 : nullptr);
    const bool stab = sameTrainerMoveType(type1, move.type) ||
        (type2 && !sameTrainerMoveType(type2, "NONE") && sameTrainerMoveType(type2, move.type));
    double score = -20.0;
    const double critBenefit = PokerogueContent::moveHasAttribute(move, "HighCritAttr") ? 3.0 :
        PokerogueContent::moveHasAttribute(move, "CritOnlyAttr") ? 5.0 : 0.0;
    if (!calculatePlainAttackAiScore(effectiveness, selectedStat, otherStat,
            move.power, move.accuracy, stab, score, critBenefit)) return -20.0;
    return score + canonicalDamageDrainAiBenefit(user, move) + canonicalRecoilAiBenefit(move);
}

}

bool FirstRunRuntime::resolveActiveMoveWeather(const PokemonBattleState& user,
    const PokemonBattleState& opponent, PokemonMoveWeatherContext& output) const {
    PokemonWeatherAbilityComponent components[3] = {
        {user.abilityId, user.hp != 0, true},
        {opponent.abilityId, opponent.hp != 0, false},
        {0, false, false}
    };
    uint8_t count = 2;
    if (m_doubleBattle) {
        const PokemonBattleState* field[] = {&m_context.player.battleState,
            &m_context.enemy.battleState, &m_context.secondEnemy.battleState};
        for (const auto* actor : field) {
            if (actor->pokemonId == user.pokemonId || actor->pokemonId == opponent.pokemonId) continue;
            components[count++] = {actor->abilityId, actor->hp != 0, false};
            break;
        }
    }
    PokemonWeatherResolutionPolicy policy{};
    return composePokemonWeatherResolutionPolicy(components, count, policy) &&
        resolvePokemonMoveWeatherContext(m_arenaWeather, policy, output);
}

bool FirstRunRuntime::resolveActiveMoveWeather(bool enemyAttacks,
    PokemonMoveWeatherContext& output) const {
    if (!m_context.player.actorIdentityResolved ||
        !m_context.enemy.actorIdentityResolved) return false;
    return resolveActiveMoveWeather(
        enemyAttacks ? m_context.enemy.battleState : m_context.player.battleState,
        enemyAttacks ? m_context.player.battleState : m_context.enemy.battleState,
        output);
}

bool FirstRunRuntime::resolveActiveMoveCritical(const PokemonBattleState& user,
    const PokemonBattleState& opponent, PokemonCriticalPolicy& output) const {
    const PokemonCriticalAbilityComponent components[2] = {
        {user.abilityId, true, true},
        {opponent.abilityId, true, false}
    };
    return composePokemonCriticalAbilityPolicy(components, 2, false, output);
}

bool FirstRunRuntime::resolveActiveMoveCritical(bool enemyAttacks,
    PokemonCriticalPolicy& output) const {
    if (!m_context.player.actorIdentityResolved ||
        !m_context.enemy.actorIdentityResolved) return false;
    return resolveActiveMoveCritical(
        enemyAttacks ? m_context.enemy.battleState : m_context.player.battleState,
        enemyAttacks ? m_context.player.battleState : m_context.enemy.battleState,
        output);
}

bool FirstRunRuntime::weatherBattleSupported() const {
    if (static_cast<uint8_t>(m_arenaWeather.type) > static_cast<uint8_t>(PokemonEffectiveWeather::Snow))
        return false;
    if (!m_context.player.actorIdentityResolved || !m_context.enemy.actorIdentityResolved)
        return false;
    if (!pokemonWeatherLifecycleSupported(m_context.player.battleState.abilityId) ||
        !pokemonWeatherLifecycleSupported(m_context.enemy.battleState.abilityId))
        return false;
    if (m_doubleBattle) {
        if (!m_secondEncounterResolved || !m_context.secondEnemy.actorIdentityResolved ||
            !pokemonWeatherLifecycleSupported(m_context.secondEnemy.battleState.abilityId))
            return false;
    }
    return true;
}

bool FirstRunRuntime::trainerBattleSupported() const {
    if (!m_trainerBattle || !m_context.trainerPartyBattleStatesResolved ||
        m_context.trainerPartyCount == 0 || m_context.trainerPartyCount > 6 ||
        m_context.activeTrainerPartyIndex >= m_context.trainerPartyCount) {
        return false;
    }
    const auto weatherMoveAllowed = [&](uint16_t id) {
        return !pokemonWeatherChangeProfile(id) || weatherBattleSupported();
    };
    for (uint8_t i = 0; i < m_context.trainerPartyCount; ++i) {
        const auto& member = m_context.trainerParty[i];
        if (!member.actorIdentityResolved || !member.battleState.moveCount || member.battleState.moveCount > 4) {
            return false;
        }
        if (!pokemonWeatherLifecycleSupported(member.battleState.abilityId)) {
            return false;
        }
    }
    const auto& activeEnemy = m_context.enemy.battleState;
    uint8_t usableMoves = 0;
    for (uint8_t i = 0; i < activeEnemy.moveCount; ++i) {
        const auto& move = activeEnemy.moves[i];
        if (!move.pp) continue;
        if (!supportsBaselineBattleMove(move.moveId) || !weatherMoveAllowed(move.moveId) ||
            (damageDrainProfile(move.moveId) && hasCanonicalReverseDrain(m_context.player.battleState.abilityId))) {
            return false;
        }
        ++usableMoves;
    }
    return usableMoves > 0;
}

bool FirstRunRuntime::doubleBattleSupported() const {
    if (!m_doubleBattle || !m_encounterResolved || !m_secondEncounterResolved ||
        !m_context.player.actorIdentityResolved ||
        !m_context.enemy.actorIdentityResolved ||
        !m_context.secondEnemy.actorIdentityResolved ||
        m_battleFinished) return false;
    if (!m_context.enemy.battleState.moveCount || m_context.enemy.battleState.moveCount > 4 ||
        !m_context.secondEnemy.battleState.moveCount || m_context.secondEnemy.battleState.moveCount > 4) return false;
    if (!pokemonWeatherLifecycleSupported(m_context.player.battleState.abilityId) ||
        !pokemonWeatherLifecycleSupported(m_context.enemy.battleState.abilityId) ||
        !pokemonWeatherLifecycleSupported(m_context.secondEnemy.battleState.abilityId)) return false;
    if (m_arenaWeather.type != PokemonEffectiveWeather::None && !weatherBattleSupported()) return false;

    const auto weatherMoveAllowed = [&](uint16_t id) {
        return !pokemonWeatherChangeProfile(id) || weatherBattleSupported();
    };

    uint8_t enemy0Usable = 0;
    if (m_context.enemy.battleState.hp > 0) {
        for (uint8_t i = 0; i < m_context.enemy.battleState.moveCount; ++i) {
            const auto& move = m_context.enemy.battleState.moves[i];
            if (!move.pp) continue;
            if (!supportsBaselineBattleMove(move.moveId) || !weatherMoveAllowed(move.moveId) ||
                (damageDrainProfile(move.moveId) && hasCanonicalReverseDrain(m_context.player.battleState.abilityId))) return false;
            ++enemy0Usable;
        }
    } else {
        enemy0Usable = 1;
    }

    uint8_t enemy1Usable = 0;
    if (m_context.secondEnemy.battleState.hp > 0) {
        for (uint8_t i = 0; i < m_context.secondEnemy.battleState.moveCount; ++i) {
            const auto& move = m_context.secondEnemy.battleState.moves[i];
            if (!move.pp) continue;
            if (!supportsBaselineBattleMove(move.moveId) || !weatherMoveAllowed(move.moveId) ||
                (damageDrainProfile(move.moveId) && hasCanonicalReverseDrain(m_context.player.battleState.abilityId))) return false;
            ++enemy1Usable;
        }
    } else {
        enemy1Usable = 1;
    }

    if (!enemy0Usable || !enemy1Usable) return false;

    if (m_selectedBattleMove >= m_context.player.battleState.moveCount) return false;
    const auto& playerMove = m_context.player.battleState.moves[m_selectedBattleMove];
    if (!playerMove.pp || !supportsBaselineBattleMove(playerMove.moveId) || !weatherMoveAllowed(playerMove.moveId)) return false;
    if (damageDrainProfile(playerMove.moveId) &&
        (hasCanonicalReverseDrain(m_context.enemy.battleState.abilityId) ||
         hasCanonicalReverseDrain(m_context.secondEnemy.battleState.abilityId))) return false;

    return true;
}

bool FirstRunRuntime::battleInputSupported() const {
    if (m_doubleBattle) return doubleBattleSupported();
    if (!m_encounterResolved || !m_context.player.actorIdentityResolved ||
        !m_context.enemy.actorIdentityResolved || m_battleFinished ||
        !m_context.enemy.battleState.moveCount || m_context.enemy.battleState.moveCount > 4) return false;
    if (m_trainerBattle && !trainerBattleSupported()) return false;
    if (m_arenaWeather.type != PokemonEffectiveWeather::None && !weatherBattleSupported()) return false;
    const auto weatherMoveAllowed = [&](uint16_t id) {
        return !pokemonWeatherChangeProfile(id) || weatherBattleSupported();
    };
    uint8_t enemyUsable = 0;
    for (uint8_t i = 0; i < m_context.enemy.battleState.moveCount; ++i) {
        const auto& move = m_context.enemy.battleState.moves[i];
        if (!move.pp) continue;
        if (!supportsBaselineBattleMove(move.moveId) || !weatherMoveAllowed(move.moveId) ||
            (damageDrainProfile(move.moveId) && hasCanonicalReverseDrain(m_context.player.battleState.abilityId))) return false;
        ++enemyUsable;
    }
    return enemyUsable && m_selectedBattleMove < m_context.player.battleState.moveCount &&
        m_context.player.battleState.moves[m_selectedBattleMove].pp &&
        supportsBaselineBattleMove(m_context.player.battleState.moves[m_selectedBattleMove].moveId) &&
        weatherMoveAllowed(m_context.player.battleState.moves[m_selectedBattleMove].moveId) &&
        !(damageDrainProfile(m_context.player.battleState.moves[m_selectedBattleMove].moveId) &&
          hasCanonicalReverseDrain(m_context.enemy.battleState.abilityId));
}

bool FirstRunRuntime::generateVictoryRewards() {
    if (!m_battleFinished || !m_playerWon || !m_experienceGranted || !enemyPartyDefeated())
        return false;
    m_rewardChoices = {};
    m_rewardChoiceCount = 0;
    m_selectedRewardChoice = 0;
    m_rewardsPending = false;

    PokerogueRngAdapter rewardRng;
    uint16_t waveSeed[PokerogueRngAdapter::kMaxSeedCodeUnits]{};
    if (!PokerogueRngAdapter::shiftCharCodes(m_seedCodeUnits.data(), m_seedLength,
            m_run.wave, waveSeed, PokerogueRngAdapter::kMaxSeedCodeUnits)) return false;
    PokerogueSeedOffsetScope scope(rewardRng, waveSeed, m_seedLength, 0x1000u + (static_cast<uint32_t>(m_run.wave) << 4));
    if (!scope.valid()) return false;

    InitialClassicRewardWeights weights(m_context.player.battleState, true);
    for (uint8_t slot = 0; slot < 3; ++slot) {
        ModifierRewardRoll roll{};
        if (rollPlayerModifierReward(rewardRng, 0, weights, roll) == ModifierRewardRollResult::Ok &&
            roll.poolEntry) {
            m_rewardChoices[m_rewardChoiceCount++] = roll;
        }
    }
    if (m_rewardChoiceCount > 0) {
        m_rewardsPending = true;
        return true;
    }
    return false;
}

bool FirstRunRuntime::selectRewardChoice(int direction) {
    if (!m_rewardsPending || m_rewardChoiceCount == 0) return false;
    if (direction > 0) {
        m_selectedRewardChoice = static_cast<uint8_t>((m_selectedRewardChoice + 1) % m_rewardChoiceCount);
    } else if (direction < 0) {
        m_selectedRewardChoice = static_cast<uint8_t>((m_selectedRewardChoice + m_rewardChoiceCount - 1) % m_rewardChoiceCount);
    }
    buildScene();
    return true;
}

bool FirstRunRuntime::claimRewardChoice() {
    FirstRunRuntime candidate = *this;
    if (!candidate.claimRewardChoiceInPlace()) {
        m_battleFeedback = candidate.m_battleFeedback;
        buildScene();
        return false;
    }
    *this = candidate;
    buildScene();
    return true;
}

bool FirstRunRuntime::claimRewardChoiceInPlace() {
    if (!m_rewardsPending || m_selectedRewardChoice >= m_rewardChoiceCount) return false;
    const auto& choice = m_rewardChoices[m_selectedRewardChoice];
    if (choice.poolEntry && choice.poolEntry->itemId) {
        const char* itemId = choice.poolEntry->itemId;
        auto& playerState = m_context.player.battleState;
        if (std::strcmp(itemId, "POTION") == 0) {
            playerState.hp = std::min<uint16_t>(playerState.maxHp, playerState.hp + 20);
        } else if (std::strcmp(itemId, "SUPER_POTION") == 0) {
            playerState.hp = std::min<uint16_t>(playerState.maxHp, playerState.hp + 50);
        } else if (std::strcmp(itemId, "HYPER_POTION") == 0) {
            playerState.hp = std::min<uint16_t>(playerState.maxHp, playerState.hp + 200);
        } else if (std::strcmp(itemId, "MAX_POTION") == 0 || std::strcmp(itemId, "FULL_RESTORE") == 0) {
            playerState.hp = playerState.maxHp;
        } else if (std::strcmp(itemId, "ETHER") == 0) {
            if (m_selectedBattleMove < playerState.moveCount) {
                auto& move = playerState.moves[m_selectedBattleMove];
                move.pp = std::min<uint8_t>(move.maxPp, move.pp + 10);
            }
        } else if (std::strcmp(itemId, "MAX_ETHER") == 0) {
            if (m_selectedBattleMove < playerState.moveCount) {
                auto& move = playerState.moves[m_selectedBattleMove];
                move.pp = move.maxPp;
            }
        } else if (std::strcmp(itemId, "ELIXIR") == 0) {
            for (uint8_t i = 0; i < playerState.moveCount; ++i) {
                playerState.moves[i].pp = std::min<uint8_t>(playerState.moves[i].maxPp, playerState.moves[i].pp + 10);
            }
        } else if (std::strcmp(itemId, "MAX_ELIXIR") == 0) {
            for (uint8_t i = 0; i < playerState.moveCount; ++i) {
                playerState.moves[i].pp = playerState.moves[i].maxPp;
            }
        } else if (std::strcmp(itemId, "POKEBALL") == 0) {
            m_pokeballs[0] = std::min<uint16_t>(99, m_pokeballs[0] + 5);
        } else if (std::strcmp(itemId, "GREAT_BALL") == 0) {
            m_pokeballs[1] = std::min<uint16_t>(99, m_pokeballs[1] + 5);
        } else if (std::strcmp(itemId, "ULTRA_BALL") == 0) {
            m_pokeballs[2] = std::min<uint16_t>(99, m_pokeballs[2] + 5);
        } else if (std::strcmp(itemId, "ROGUE_BALL") == 0) {
            m_pokeballs[3] = std::min<uint16_t>(99, m_pokeballs[3] + 5);
        } else if (std::strcmp(itemId, "MASTER_BALL") == 0) {
            m_pokeballs[4] = std::min<uint16_t>(99, m_pokeballs[4] + 1);
        } else if (std::strcmp(itemId, "REVIVE") == 0) {
            for (uint8_t i = 0; i < m_context.playerPartyCount; ++i) {
                if (m_context.playerParty[i].battleState.hp == 0) {
                    m_context.playerParty[i].battleState.hp = std::max<uint16_t>(1, m_context.playerParty[i].battleState.maxHp / 2);
                    if (i == m_context.activePlayerPartyIndex) {
                        playerState.hp = m_context.playerParty[i].battleState.hp;
                    }
                    break;
                }
            }
        } else if (std::strcmp(itemId, "MAX_REVIVE") == 0) {
            for (uint8_t i = 0; i < m_context.playerPartyCount; ++i) {
                if (m_context.playerParty[i].battleState.hp == 0) {
                    m_context.playerParty[i].battleState.hp = m_context.playerParty[i].battleState.maxHp;
                    if (i == m_context.activePlayerPartyIndex) {
                        playerState.hp = m_context.playerParty[i].battleState.hp;
                    }
                    break;
                }
            }
        } else if (std::strcmp(itemId, "SACRED_ASH") == 0) {
            for (uint8_t i = 0; i < m_context.playerPartyCount; ++i) {
                if (m_context.playerParty[i].battleState.hp == 0) {
                    m_context.playerParty[i].battleState.hp = m_context.playerParty[i].battleState.maxHp;
                }
            }
            playerState.hp = m_context.playerParty[m_context.activePlayerPartyIndex].battleState.hp;
        } else if (std::strcmp(itemId, "SITRUS_BERRY") == 0) {
            playerState.hp = std::min<uint16_t>(playerState.maxHp, playerState.hp + std::max<uint16_t>(1, playerState.maxHp / 4));
        } else if (std::strcmp(itemId, "ORAN_BERRY") == 0) {
            playerState.hp = std::min<uint16_t>(playerState.maxHp, playerState.hp + 10);
        } else if (std::strcmp(itemId, "LEPPA_BERRY") == 0) {
            if (m_selectedBattleMove < playerState.moveCount) {
                auto& move = playerState.moves[m_selectedBattleMove];
                move.pp = std::min<uint8_t>(move.maxPp, move.pp + 10);
            }
        }
    }
    m_rewardsPending = false;
    m_run.wave = m_victoryPlan.nextWave;
    resolve(true);
    if (!m_encounterResolved) m_checkpointAvailable = false;
    m_battleFeedback = "Reward claimed - next Classic wave";
    buildScene();
    return m_encounterResolved;
}

bool FirstRunRuntime::selectBattleMove(int direction) {
    if (m_rewardsPending) {
        return selectRewardChoice(direction);
    }
    if (!direction || !m_context.player.battleState.moveCount) return false;
    const uint8_t count = m_context.player.battleState.moveCount;
    for (uint8_t step = 0; step < count; ++step) {
        m_selectedBattleMove = direction > 0
            ? static_cast<uint8_t>((m_selectedBattleMove + 1) % count)
            : static_cast<uint8_t>((m_selectedBattleMove + count - 1) % count);
        const auto& move = m_context.player.battleState.moves[m_selectedBattleMove];
        if (move.pp && supportsBaselineBattleMove(move.moveId)) {
            m_battleFeedback.clear();
            buildScene();
            return true;
        }
    }
    m_battleFeedback = "Selected moves need unsupported effects";
    buildScene();
    return false;
}

bool FirstRunRuntime::cycleTarget(int direction) {
    if (!m_doubleBattle || m_battleFinished || !m_encounterResolved || !m_secondEncounterResolved) return false;
    if (m_context.enemy.battleState.hp == 0 && m_context.secondEnemy.battleState.hp == 0) return false;
    if (m_context.enemy.battleState.hp == 0) {
        m_selectedTarget = 1;
        buildScene();
        return true;
    }
    if (m_context.secondEnemy.battleState.hp == 0) {
        m_selectedTarget = 0;
        buildScene();
        return true;
    }
    m_selectedTarget = (m_selectedTarget == 0) ? 1 : 0;
    buildScene();
    return true;
}

bool FirstRunRuntime::grantVictoryExperience() {
    if (m_experienceGranted || !m_context.player.actorIdentityResolved ||
        !m_context.enemy.actorIdentityResolved) return false;
    if (m_doubleBattle && !m_secondEncounterResolved) return false;
    const auto* starter = PokerogueContent::findSpeciesByDex(m_context.player.dex);
    const auto* defeated = PokerogueContent::findSpeciesByDex(m_context.enemy.dex);
    const auto* defeatedForm = m_context.enemy.formId
        ? PokerogueContent::findFormById(m_context.enemy.formId) : nullptr;
    if (!starter || !defeated || (m_context.enemy.formId && !defeatedForm)) return false;
    double rawExperience = 0.0;
    if (pokemonExperienceForDefeat(*defeated, m_context.enemy.level, rawExperience,
                                  defeatedForm) != PokemonExperienceResult::Ok ||
        rawExperience < 0.0 || rawExperience > 4294967295.0) return false;
    uint32_t awardedExperience = 0;
    if (pokemonSingleParticipantExperience(rawExperience, m_trainerBattle,
            awardedExperience) != PokemonExperienceResult::Ok) return false;

    if (m_doubleBattle) {
        const auto* defeated2 = PokerogueContent::findSpeciesByDex(m_context.secondEnemy.dex);
        const auto* defeatedForm2 = m_context.secondEnemy.formId
            ? PokerogueContent::findFormById(m_context.secondEnemy.formId) : nullptr;
        if (!defeated2 || (m_context.secondEnemy.formId && !defeatedForm2)) return false;
        double rawExperience2 = 0.0;
        uint32_t award2 = 0;
        if (pokemonExperienceForDefeat(*defeated2, m_context.secondEnemy.level, rawExperience2,
                defeatedForm2) != PokemonExperienceResult::Ok ||
            rawExperience2 < 0.0 || rawExperience2 > 4294967295.0 ||
            pokemonSingleParticipantExperience(rawExperience2, false, award2) != PokemonExperienceResult::Ok ||
            award2 > 0xffffffffU - awardedExperience) return false;
        awardedExperience += award2;
    }

    PokemonExperienceProgress progress{};
    if (applyPokemonExperience(starter->growthRate, m_context.player.level,
                               m_context.player.totalExperience, awardedExperience,
                               classicExperienceLevelCap(m_run.wave), progress)
        != PokemonExperienceResult::Ok) return false;

    const uint16_t oldLevel = m_context.player.level;
    PokemonBattleState next = m_context.player.battleState;
    if (progress.level != next.level) {
        PokemonBattleInit input{};
        input.speciesDex = next.speciesDex;
        input.formId = next.formId;
        input.level = progress.level;
        input.pokemonId = next.pokemonId;
        input.nature = next.nature;
        input.gender = next.gender;
        input.abilityId = next.abilityId;
        input.moveCount = next.moveCount;
        for (uint8_t i = 0; i < 6; ++i) input.ivs[i] = next.ivs[i];
        for (uint8_t i = 0; i < next.moveCount; ++i) input.moveIds[i] = next.moves[i].moveId;
        PokemonBattleState leveled{};
        if (initializePokemonBattleState(input, leveled) != PokemonBattleInitResult::Ok) return false;
        // Pinned Pokemon.calculateStats heals the max-HP increase for a living
        // Pokemon, while retaining current PP across a level change.
        if (next.hp && leveled.maxHp > next.maxHp)
            leveled.hp = static_cast<uint16_t>(next.hp + leveled.maxHp - next.maxHp);
        else if (next.hp > leveled.maxHp) leveled.hp = leveled.maxHp;
        else leveled.hp = next.hp;
        for (uint8_t i = 0; i < next.moveCount; ++i) leveled.moves[i].pp = next.moves[i].pp;
        for (uint8_t stat = 0; stat < 7; ++stat) leveled.statStages[stat] = next.statStages[stat];
        next = leveled;

        // Learn newly available level moves if there is space in the moveset (< 4)
        std::string moveFeedback;
        learnNewLevelMoves(next.speciesDex, oldLevel, progress.level, next,
                           m_context.player.moveIds, m_context.player.moveCount, &moveFeedback);

        // Check for level-based evolution
        const auto* evo = checkSpeciesLevelEvolution(m_context.player.speciesId, oldLevel, progress.level);
        if (evo) {
            EvolutionResult evoResult{};
            std::string evoFeedback;
            if (applySpeciesEvolution(m_context.player.dex, evo->targetSpeciesId, next, evoResult, &evoFeedback)) {
                m_context.player.dex = evoResult.newDex;
                m_context.player.speciesId = evoResult.newSpeciesId;
                m_context.player.localizedName = evoResult.newName;
                const auto* evolvedSpecies = PokerogueContent::findSpeciesByDex(evoResult.newDex);
                m_context.player.formId = next.formId;
                m_context.player.actor.formId = next.formId;
                m_context.player.assetSourcePath = evolvedSpecies ? evolvedSpecies->assetSourcePath : nullptr;

                m_battleFeedback = evoFeedback;
            }
        }
    }
    m_context.player.battleState = next;
    m_context.player.level = progress.level;
    m_context.player.totalExperience = progress.totalExperience;
    m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;
    m_experienceGranted = true;
    return true;
}

bool FirstRunRuntime::selectEnemyMoveSlot(const PokemonBattleState& enemyState,
    const PokemonBattleState& playerState, PokerogueRngAdapter& rng, uint8_t& enemyMoveSlot) {
    // Pinned EnemyPokemon.SMART_RANDOM: score each usable move in moveset order,
    // then advance through the descending pool while randBattleSeedInt(8) >= 5.
    PokemonMoveWeatherContext simulatedWeather{};
    if (!resolveActiveMoveWeather(enemyState, playerState, simulatedWeather)) return false;
    uint8_t usableSlots[4]{};
    uint32_t projectedDamage[4]{};
    uint8_t usableCount = 0;
    for (uint8_t slot = 0; slot < enemyState.moveCount; ++slot) {
        if (!enemyState.moves[slot].pp) continue;
        uint32_t damage = 0;
        const auto* candidateMove = PokerogueContent::findMoveById(enemyState.moves[slot].moveId);
        if (!candidateMove) return false;
        // Status moves cannot KO and remain eligible only when no attack can KO.
        if (candidateMove->category != PokerogueContent::MoveStatus &&
            calculatePokemonDamageCore(enemyState, playerState,
                enemyState.moves[slot].moveId, false, damage, &simulatedWeather) != PokemonDamageCoreResult::Ok) {
            m_battleFeedback = "Enemy simulated damage unsupported";
            buildScene();
            return false;
        }
        usableSlots[usableCount] = slot;
        projectedDamage[usableCount++] = damage;
    }
    uint8_t filteredSlots[4]{};
    uint8_t filteredCount = 0;
    if (!filterEnemyKoMoveSlots(usableSlots, projectedDamage, usableCount,
            playerState.hp, filteredSlots, filteredCount)) {
        m_battleFeedback = "Enemy KO move pool unavailable";
        buildScene();
        return false;
    }
    uint8_t candidates[4]{};
    double scores[4]{};
    uint8_t candidateCount = 0;
    for (uint8_t entry = 0; entry < filteredCount; ++entry) {
        const uint8_t i = filteredSlots[entry];
        const auto* move = PokerogueContent::findMoveById(enemyState.moves[i].moveId);
        if (!move) {
            m_battleFeedback = "Canonical enemy move reference invalid";
            buildScene();
            return false;
        }
        // getNextTargets() resolves one candidate target. In this bounded
        // one-opponent case its adjusted weight is one, so randSeedInt(1)
        // short-circuits in the pinned source without consuming the stream.
        (void)rng.randSeedInt(1);
        candidates[candidateCount] = i;
        scores[candidateCount] = baselineEnemyMoveScore(enemyState, playerState, *move);
        ++candidateCount;
    }
    for (uint8_t i = 1; i < candidateCount; ++i) {
        const uint8_t candidate = candidates[i];
        const double score = scores[i];
        uint8_t position = i;
        while (position && score > scores[position - 1]) {
            candidates[position] = candidates[position - 1];
            scores[position] = scores[position - 1];
            --position;
        }
        candidates[position] = candidate;
        scores[position] = score;
    }
    enemyMoveSlot = 0;
    if (m_trainerBattle) {
        // EnemyPokemon constructor selects SMART whenever hasTrainer() is true.
        if (!selectSmartTrainerMoveSlot(scores, candidates, candidateCount, rng,
                enemyMoveSlot)) {
            m_battleFeedback = "Trainer SMART move selection failed";
            buildScene();
            return false;
        }
    } else {
        uint8_t chosenIndex = 0;
        while (chosenIndex + 1 < candidateCount && rng.randSeedInt(8) >= 5) ++chosenIndex;
        enemyMoveSlot = candidates[chosenIndex];
    }
    return true;
}

bool FirstRunRuntime::executeEnemyResponse(uint8_t userIndex, PokerogueRngAdapter& rng) {
    if (userIndex != 1 && userIndex != 2) return false;
    const auto& enemy = userIndex == 1 ? m_context.enemy : m_context.secondEnemy;
    if (!enemy.battleState.hp || !m_context.player.battleState.hp) return true;
    uint8_t slot = 0;
    return selectEnemyMoveSlot(enemy.battleState, m_context.player.battleState, rng, slot) &&
        executeActiveBattleMove(userIndex, 0, slot, rng);
}

bool FirstRunRuntime::advanceBattleTurn() {
    // Host processes commands before rendering; a failed phase cannot publish half a turn.
    FirstRunRuntime candidate = *this;
    if (!candidate.advanceBattleTurnInPlace()) {
        m_battleFeedback = candidate.m_battleFeedback;
        buildScene();
        return false;
    }
    *this = candidate;
    buildScene();
    return true;
}

bool FirstRunRuntime::advanceBattleTurnInPlace() {
    if (m_rewardsPending) {
        return claimRewardChoice();
    }
    if (m_battleFinished && m_playerWon && (!m_experienceGranted || (m_trainerBattle && !enemyPartyDefeated()))) {
        if (!m_experienceGranted && !grantVictoryExperience()) {
            m_battleFeedback = "Victory experience could not be resolved";
            buildScene();
            return false;
        }
        if (m_trainerBattle && !advanceTrainerAfterDefeat()) {
            m_battleFeedback = "Trainer replacement could not be resolved";
            buildScene();
            return false;
        }
        m_battleFeedback = m_battleFinished ? "Experience granted - rewards pending"
            : "Trainer sent the next Pokemon";
        buildScene();
        return true;
    }
    if (m_battleFinished && m_playerWon && m_experienceGranted && enemyPartyDefeated()) {
        if (m_victoryPlan.contains(ClassicVictoryStep::GameClear)) {
            m_battleFeedback = "Game Clear! Classic run completed";
            buildScene();
            return true;
        }
        if (!m_rewardsPending && generateVictoryRewards()) {
            m_battleFeedback = "Select reward: UP/DOWN choose, A claim, B skip";
            buildScene();
            return true;
        }
        m_battleFeedback = "Item reward selection is not ported yet";
        buildScene();
        return false;
    }
    if (!battleInputSupported()) {
        m_battleFeedback = (m_trainerBattle && !trainerBattleSupported()) ? "Trainer AI and switching unsupported" :
            m_doubleBattle ? "Double battle turn order unsupported" :
            "Battle move metadata unsupported; no action taken";
        buildScene();
        return false;
    }
    auto* rng = m_battleRng.currentStream();
    if (!rng) {
        m_battleFeedback = "Battle RNG is not initialized";
        buildScene();
        return false;
    }
    const auto& playerState = m_context.player.battleState;
    const auto& enemyState = m_context.enemy.battleState;
    const auto* selected = PokerogueContent::findMoveById(
        playerState.moves[m_selectedBattleMove].moveId);
    if (!selected) {
        m_battleFeedback = "Canonical move reference invalid";
        buildScene();
        return false;
    }

    if (m_trainerBattle) {
        // Restricted to the complete fixed party. The current plain
        // battle gate represents no queue/trap/hazard state; broader effects
        // must resolve those inputs before trainer eligibility is enabled.
        if (!m_context.trainerPartyCount || m_context.trainerPartyCount > 6 ||
            !m_context.trainerPartyBattleStatesResolved) return false;
        refreshTrainerBaselineMatchups();
        if (!m_context.trainerPartyBaselineMatchupResolved) return false;
        double reserveScores[6]{};
        uint8_t reserveIndexes[6]{};
        uint8_t reserveCount = 0;
        const auto* opponentSpecies = PokerogueContent::findSpeciesByDex(playerState.speciesDex);
        if (!opponentSpecies || opponentSpecies->legendary < 0) return false;
        for (uint8_t member = 0; member < m_context.trainerPartyCount; ++member) {
            if (member == m_context.activeTrainerPartyIndex ||
                !m_context.trainerParty[member].battleState.hp) continue;
            reserveIndexes[reserveCount] = member;
            reserveScores[reserveCount++] = m_context.trainerPartyBaselineMatchupScores[member]
                / (opponentSpecies->legendary ? 2.0 : 1.0);
        }
        if (reserveCount > 0) {
            uint16_t waveSeed[PokerogueRngAdapter::kMaxSeedCodeUnits]{};
            if (!PokerogueRngAdapter::shiftCharCodes(m_seedCodeUnits.data(), m_seedLength,
                    m_run.wave, waveSeed, PokerogueRngAdapter::kMaxSeedCodeUnits)) return false;
            PokerogueRngAdapter switchRng;
            PokerogueSeedOffsetScope scope(switchRng, waveSeed, m_seedLength, m_turn << 2);
            TrainerSwitchDecision decision{};
            if (!scope.valid() || !resolveTrainerSwitchDecision(
                    m_context.trainerPartyBaselineMatchupScores[m_context.activeTrainerPartyIndex],
                    reserveScores, reserveIndexes, reserveCount, m_enemySwitchCounter,
                    false, false, false, switchRng, decision)) return false;
            if (decision.switchPokemon) {
                // Switch commands precede FIGHT. The player attacks the incoming
                // actor; the trainer consumes its command by switching, not attacking.
                const ResolvedPokemon outgoing = m_context.enemy;
                ResolvedPokemon incoming = m_context.trainerParty[decision.partyIndex];
                resetPokemonStatStages(incoming.battleState);
                PokerogueRngAdapter actionRng = *rng;
                // Resolve abilities against the incoming actor. A rejected command
                // leaves player HP/PP, field state and RNG untouched.
                m_context.enemy = incoming;
                if (!executeActiveBattleMove(false, m_selectedBattleMove, actionRng)) {
                    m_context.enemy = outgoing;
                    return false;
                }
                m_context.trainerParty[m_context.activeTrainerPartyIndex] = outgoing;
                m_context.activeTrainerPartyIndex = decision.partyIndex;
                m_run.encounterDex = incoming.dex;
                *rng = actionRng;
                m_enemySwitchCounter = decision.nextSwitchCounter;
                m_runStarted = true;
                m_checkpointAvailable = false;
                m_battleFeedback = std::string("Trainer switched; ") + m_battleFeedback;
                return finishBattleTurn();
            }
            m_enemySwitchCounter = decision.nextSwitchCounter;
        }
    }

    if (m_doubleBattle) {
        uint8_t enemy0MoveSlot = 0;
        if (m_context.enemy.battleState.hp > 0) {
            PokemonMoveWeatherContext simWeather{};
            if (!resolveActiveMoveWeather(m_context.enemy.battleState, playerState, simWeather)) return false;
            uint8_t usableSlots[4]{};
            uint32_t projDamage[4]{};
            uint8_t usableCount = 0;
            for (uint8_t slot = 0; slot < m_context.enemy.battleState.moveCount; ++slot) {
                if (!m_context.enemy.battleState.moves[slot].pp) continue;
                uint32_t damage = 0;
                const auto* cm = PokerogueContent::findMoveById(m_context.enemy.battleState.moves[slot].moveId);
                if (!cm) return false;
                if (cm->category != PokerogueContent::MoveStatus &&
                    calculatePokemonDamageCore(m_context.enemy.battleState, playerState,
                        m_context.enemy.battleState.moves[slot].moveId, false, damage, &simWeather) != PokemonDamageCoreResult::Ok) {
                    m_battleFeedback = "Enemy simulated damage unsupported";
                    buildScene();
                    return false;
                }
                usableSlots[usableCount] = slot;
                projDamage[usableCount++] = damage;
            }
            uint8_t filteredSlots[4]{};
            uint8_t filteredCount = 0;
            if (!filterEnemyKoMoveSlots(usableSlots, projDamage, usableCount,
                    playerState.hp, filteredSlots, filteredCount)) return false;
            uint8_t candidates[4]{};
            double scores[4]{};
            uint8_t candidateCount = 0;
            for (uint8_t entry = 0; entry < filteredCount; ++entry) {
                const uint8_t i = filteredSlots[entry];
                const auto* move = PokerogueContent::findMoveById(m_context.enemy.battleState.moves[i].moveId);
                if (!move) return false;
                (void)rng->randSeedInt(1);
                candidates[candidateCount] = i;
                scores[candidateCount] = baselineEnemyMoveScore(m_context.enemy.battleState, playerState, *move);
                ++candidateCount;
            }
            for (uint8_t i = 1; i < candidateCount; ++i) {
                const uint8_t candidate = candidates[i];
                const double score = scores[i];
                uint8_t position = i;
                while (position && score > scores[position - 1]) {
                    candidates[position] = candidates[position - 1];
                    scores[position] = scores[position - 1];
                    --position;
                }
                candidates[position] = candidate;
                scores[position] = score;
            }
            uint8_t chosenIndex = 0;
            while (chosenIndex + 1 < candidateCount && rng->randSeedInt(8) >= 5) ++chosenIndex;
            enemy0MoveSlot = candidates[chosenIndex];
        }

        uint8_t enemy1MoveSlot = 0;
        if (m_context.secondEnemy.battleState.hp > 0) {
            PokemonMoveWeatherContext simWeather{};
            if (!resolveActiveMoveWeather(m_context.secondEnemy.battleState, playerState, simWeather)) return false;
            uint8_t usableSlots[4]{};
            uint32_t projDamage[4]{};
            uint8_t usableCount = 0;
            for (uint8_t slot = 0; slot < m_context.secondEnemy.battleState.moveCount; ++slot) {
                if (!m_context.secondEnemy.battleState.moves[slot].pp) continue;
                uint32_t damage = 0;
                const auto* cm = PokerogueContent::findMoveById(m_context.secondEnemy.battleState.moves[slot].moveId);
                if (!cm) return false;
                if (cm->category != PokerogueContent::MoveStatus &&
                    calculatePokemonDamageCore(m_context.secondEnemy.battleState, playerState,
                        m_context.secondEnemy.battleState.moves[slot].moveId, false, damage, &simWeather) != PokemonDamageCoreResult::Ok) {
                    m_battleFeedback = "Enemy simulated damage unsupported";
                    buildScene();
                    return false;
                }
                usableSlots[usableCount] = slot;
                projDamage[usableCount++] = damage;
            }
            uint8_t filteredSlots[4]{};
            uint8_t filteredCount = 0;
            if (!filterEnemyKoMoveSlots(usableSlots, projDamage, usableCount,
                    playerState.hp, filteredSlots, filteredCount)) return false;
            uint8_t candidates[4]{};
            double scores[4]{};
            uint8_t candidateCount = 0;
            for (uint8_t entry = 0; entry < filteredCount; ++entry) {
                const uint8_t i = filteredSlots[entry];
                const auto* move = PokerogueContent::findMoveById(m_context.secondEnemy.battleState.moves[i].moveId);
                if (!move) return false;
                (void)rng->randSeedInt(1);
                candidates[candidateCount] = i;
                scores[candidateCount] = baselineEnemyMoveScore(m_context.secondEnemy.battleState, playerState, *move);
                ++candidateCount;
            }
            for (uint8_t i = 1; i < candidateCount; ++i) {
                const uint8_t candidate = candidates[i];
                const double score = scores[i];
                uint8_t position = i;
                while (position && score > scores[position - 1]) {
                    candidates[position] = candidates[position - 1];
                    scores[position] = scores[position - 1];
                    --position;
                }
                candidates[position] = candidate;
                scores[position] = score;
            }
            uint8_t chosenIndex = 0;
            while (chosenIndex + 1 < candidateCount && rng->randSeedInt(8) >= 5) ++chosenIndex;
            enemy1MoveSlot = candidates[chosenIndex];
        }

        uint8_t activeBattlers[3];
        uint8_t activeCount = 0;
        if (m_context.player.battleState.hp > 0) activeBattlers[activeCount++] = 0;
        if (m_context.enemy.battleState.hp > 0) activeBattlers[activeCount++] = 1;
        if (m_context.secondEnemy.battleState.hp > 0) activeBattlers[activeCount++] = 2;

        uint32_t speeds[3]{};
        int32_t priorities[3]{};
        const PokemonBattleState* states[3] = {
            &m_context.player.battleState,
            &m_context.enemy.battleState,
            &m_context.secondEnemy.battleState
        };
        uint16_t moveIds[3] = {
            playerState.moves[m_selectedBattleMove].moveId,
            m_context.enemy.battleState.moves[enemy0MoveSlot].moveId,
            m_context.secondEnemy.battleState.moves[enemy1MoveSlot].moveId
        };
        for (uint8_t i = 0; i < 3; ++i) {
            if (!states[i]->hp) continue;
            PokemonMoveWeatherContext speedWeather{};
            const auto& speedOpponent = i == 0 ? m_context.enemy.battleState : playerState;
            if (!resolveActiveMoveWeather(*states[i], speedOpponent, speedWeather) ||
                !pokemonWeatherEffectiveSpeed(*states[i], speedWeather, speeds[i])) return false;
            const auto* m = PokerogueContent::findMoveById(moveIds[i]);
            if (!m) return false;
            priorities[i] = m->priority;
        }

        uint16_t waveSeed[PokerogueRngAdapter::kMaxSeedCodeUnits]{};
        if (!PokerogueRngAdapter::shiftCharCodes(m_seedCodeUnits.data(), m_seedLength,
                m_run.wave, waveSeed, PokerogueRngAdapter::kMaxSeedCodeUnits)) return false;
        PokerogueRngAdapter tieRng;
        if (m_turn > (0xffffffffU - activeCount) / 1000U) return false;
        PokerogueSeedOffsetScope tieScope(tieRng, waveSeed, m_seedLength, m_turn * 1000U + activeCount);
        if (!tieScope.valid()) return false;
        if (activeCount > 1) {
            for (int i = static_cast<int>(activeCount) - 1; i > 0; --i) {
                const int j = tieRng.integerInRange(0, i);
                std::swap(activeBattlers[i], activeBattlers[j]);
            }
        }

        const bool reverseSpeed = m_trickRoom.turnsLeft != 0;
        std::stable_sort(activeBattlers, activeBattlers + activeCount, [&](uint8_t a, uint8_t b) {
            return reverseSpeed ? speeds[a] < speeds[b] : speeds[a] > speeds[b];
        });
        std::stable_sort(activeBattlers, activeBattlers + activeCount, [&](uint8_t a, uint8_t b) {
            return priorities[a] > priorities[b];
        });

        m_runStarted = true;
        m_checkpointAvailable = false;

        for (uint8_t i = 0; i < activeCount; ++i) {
            const uint8_t battler = activeBattlers[i];
            if (battler == 0) {
                if (!m_context.player.battleState.hp) continue;
                const auto* pMove = PokerogueContent::findMoveById(playerState.moves[m_selectedBattleMove].moveId);
                const bool isSpread = pMove && pMove->target &&
                    (std::strcmp(pMove->target, "ALL_NEAR_ENEMIES") == 0 ||
                     std::strcmp(pMove->target, "ALL_ENEMIES") == 0 ||
                     std::strcmp(pMove->target, "ALL_OTHERS") == 0);
                if (isSpread) {
                    // Only the migrated stat-stage family currently admits area targets.
                    // Damage-area multipliers and other effects require their own resolver.
                    if (!supportsPokemonStatStageMove(pMove->id)) return false;
                    uint16_t targetAbilities[2]{};
                    uint8_t targetCount = 0;
                    if (m_context.enemy.battleState.hp) targetAbilities[targetCount++] = m_context.enemy.battleState.abilityId;
                    if (m_context.secondEnemy.battleState.hp) targetAbilities[targetCount++] = m_context.secondEnemy.battleState.abilityId;
                    PokemonPpPolicy areaPp{};
                    areaPp.resolved = true;
                    if (!pokemonActiveTargetsPpCost(targetAbilities, targetCount, areaPp.cost)) return false;
                    if (m_context.enemy.battleState.hp > 0) {
                        if (!executeActiveBattleMove(0, 1, m_selectedBattleMove, *rng, &areaPp)) { m_battleFeedback = "Double battle action failed"; return false; }
                        areaPp.cost = 0; // Subsequent target executes in resolved ignore-PP mode.
                    }
                    if (m_context.secondEnemy.battleState.hp > 0) {
                        if (!executeActiveBattleMove(0, 2, m_selectedBattleMove, *rng, &areaPp)) { m_battleFeedback = "Double battle action failed"; return false; }
                    }
                } else {
                    uint8_t target = m_selectedTarget == 0 ? 1 : 2;
                    if (target == 1 && m_context.enemy.battleState.hp == 0) target = 2;
                    else if (target == 2 && m_context.secondEnemy.battleState.hp == 0) target = 1;
                    if ((target == 1 && m_context.enemy.battleState.hp > 0) ||
                        (target == 2 && m_context.secondEnemy.battleState.hp > 0)) {
                        if (!executeActiveBattleMove(0, target, m_selectedBattleMove, *rng)) { m_battleFeedback = "Double battle action failed"; return false; }
                    }
                }
            } else if (battler == 1) {
                if (!m_context.enemy.battleState.hp || !m_context.player.battleState.hp) continue;
                if (!executeActiveBattleMove(1, 0, enemy0MoveSlot, *rng)) { m_battleFeedback = "Double battle action failed"; return false; }
            } else if (battler == 2) {
                if (!m_context.secondEnemy.battleState.hp || !m_context.player.battleState.hp) continue;
                if (!executeActiveBattleMove(2, 0, enemy1MoveSlot, *rng)) { m_battleFeedback = "Double battle action failed"; return false; }
            }
        }

        return finishBattleTurn();
    }

    uint8_t enemyMoveSlot = 0;
    if (!selectEnemyMoveSlot(enemyState, playerState, *rng, enemyMoveSlot)) return false;
    const auto* enemyMove = PokerogueContent::findMoveById(enemyState.moves[enemyMoveSlot].moveId);
    if (!enemyMove) {
        m_battleFeedback = "Canonical enemy move reference invalid";
        buildScene();
        return false;
    }

    PokemonMoveWeatherContext turnWeather{};
    if (!resolveActiveMoveWeather(false, turnWeather)) return false;
    const auto turnField = pokemonTrickRoomOrderPolicy(m_trickRoom);
    const auto firstMover = resolveBaselineFirstMover(playerState, enemyState,
        selected->id, enemyMove->id, m_seedCodeUnits.data(), m_seedLength,
        m_run.wave, m_turn, &turnWeather, &turnField);
    if (firstMover == BaselineFirstMover::Invalid) {
        m_battleFeedback = "Turn order inputs unsupported";
        buildScene();
        return false;
    }
    m_runStarted = true;
    m_checkpointAvailable = false;

    const auto act = [&](bool enemyActs) -> bool {
        return executeActiveBattleMove(enemyActs,
            enemyActs ? enemyMoveSlot : m_selectedBattleMove, *rng);
    };
    const bool enemyFirst = firstMover == BaselineFirstMover::Enemy;
    if (!act(enemyFirst)) {
        m_battleFeedback = enemyFirst ? "Enemy move resolution failed" : "Player move resolution failed";
        buildScene();
        return false;
    }
    if (m_context.player.battleState.hp && m_context.enemy.battleState.hp && !act(!enemyFirst)) {
        m_battleFeedback = enemyFirst ? "Player move resolution failed" : "Enemy move resolution failed";
        buildScene();
        return false;
    }

    return finishBattleTurn();
}

bool FirstRunRuntime::executeActiveBattleMove(uint8_t userIndex, uint8_t targetIndex, uint8_t moveSlot,
    PokerogueRngAdapter& rng, const PokemonPpPolicy* ppOverride) {
    if (userIndex > 2 || targetIndex > 2) return false;
    PokemonBattleState* actors[3] = {
        &m_context.player.battleState,
        &m_context.enemy.battleState,
        &m_context.secondEnemy.battleState
    };
    auto& user = *actors[userIndex];
    auto& opponent = *actors[targetIndex];
    if (!user.hp || !opponent.hp) return true;
    if (moveSlot >= user.moveCount || moveSlot >= 4) return false;
    const auto* move = PokerogueContent::findMoveById(user.moves[moveSlot].moveId);
    if (!move || !supportsBaselineBattleMove(move->id)) return false;
    if (selfHealingProfile(move->id)) {
        PokemonHealingPolicy policy{};
        policy.resolved = true;
        PokemonHealingEvent event{};
        if (usePokemonSelfHealingCommand(user, moveSlot, policy, event) != PokemonHealingResult::Ok)
            return false;
        m_battleFeedback = event.healed ? "HP restored" : "HP unchanged";
        return true;
    }
    PokemonPpPolicy pp{};
    pp.resolved = true;
    if (ppOverride) {
        if (!ppOverride->resolved) return false;
        pp = *ppOverride;
    } else if (!pokemonSingleOpponentPpCost(opponent.abilityId, pp.cost)) return false;
    if (pokemonWeatherChangeProfile(move->id)) {
        PokemonWeatherChangePolicy policy{};
        policy.resolved = policy.weatherCallbacksResolved = weatherBattleSupported();
        policy.duration = 5;
        policy.ppCost = pp.cost;
        PokemonWeatherChangeEvent event{};
        if (usePokemonWeatherChangeCommand(user, m_arenaWeather, moveSlot, policy, event) !=
                PokemonWeatherChangeResult::Ok) return false;
        m_battleFeedback = event.changed ? "Weather changed" : "Weather move failed";
        return true;
    }
    if (supportsPokemonStatStageMove(move->id)) {
        const bool self = std::strcmp(move->target, "USER") == 0;
        PokemonStatStageCommandPolicy policy{};
        policy.move.hitPolicyResolved = true;
        policy.move.ppCost = self ? 1 : pp.cost;
        policy.move.stagePolicy.resolved = true;
        policy.postChangePoliciesResolved = true;
        if (!self) {
            PokemonMoveWeatherContext weatherContext{};
            PokemonHitPolicy hit{};
            const PokemonWeatherAbilityComponent activeAbilities[2] = {
                {user.abilityId, true, true},
                {opponent.abilityId, true, false}
            };
            if (!resolveActiveMoveWeather(user, opponent, weatherContext) ||
                !composePokemonAlwaysHitPolicy(activeAbilities, 2, hit, move->id, &weatherContext)) return false;
            policy.move.blockedBeforeAccuracy = hit.blockedByAbility;
            policy.move.bypassAccuracy = hit.bypassAccuracy || move->accuracy < 0;
            policy.move.accuracyMultiplier = hit.accuracyMultiplier;
        }

        const auto* userProfile = PokerogueContent::findAbilityStatStageProfile(user.abilityId);
        const auto* opponentProfile = PokerogueContent::findAbilityStatStageProfile(opponent.abilityId);
        const auto* recipientProfile = self ? userProfile : opponentProfile;
        const auto* observerProfile = self ? opponentProfile : userProfile;
        const auto& recipient = self ? user : opponent;
        const auto& source = user;

        const ResolvedStatStageAbilityComponent recipientComp[] = {{recipientProfile, recipientProfile != nullptr}};
        const ResolvedStatStageAbilityComponent sourceComp[] = {{userProfile, userProfile != nullptr}};
        const ResolvedStatStageAbilityComponent observerComp[] = {{observerProfile, observerProfile != nullptr}};

        const PokerogueContent::MoveStatStageEffect* effect = nullptr;
        for (const auto& row : PokerogueContent::kMoveStatStageEffects)
            if (row.moveId == move->id && row.selfTarget == self) effect = &row;
        if (!effect || !composePokemonStatStageAbilityPolicy(*effect, recipientComp, 1, false,
                policy.move.stagePolicy, true)) return false;

        const PokerogueContent::MoveStatStageEffect reaction{move->id, 127, 1, true};
        policy.recipientReaction.resolved = policy.sourceReaction.resolved =
            policy.reflection.resolved = policy.opponentCopy.resolved = true;
        if (!composePokemonStatStageAbilityPolicy(reaction, recipientComp, 1, false, policy.recipientReaction) ||
            !composePokemonStatStageAbilityPolicy(reaction, sourceComp, 1, false, policy.reflection) ||
            !composePokemonStatStageAbilityPolicy(reaction, sourceComp, 1, false, policy.sourceReaction) ||
            !composePokemonStatStageAbilityPolicy(reaction, observerComp, 1, false, policy.opponentCopy)) return false;
        policy.opponentCopyProfile = observerProfile;

        uint8_t recipientCount = 0;
        for (const auto& row : PokerogueContent::kAbilityStatStageReactions)
            if (row.abilityId == recipient.abilityId && recipientCount < 2)
                policy.recipientReactions[recipientCount++] = &row;

        uint8_t sourceCount = 0;
        for (const auto& row : PokerogueContent::kAbilityStatStageReactions)
            if (row.abilityId == source.abilityId && sourceCount < 2)
                policy.sourceReactions[sourceCount++] = &row;

        PokemonStatStageCommandEvent event{};
        if (usePokemonStatStageStatusCommand(user, opponent, moveSlot, policy, rng, event) !=
                PokemonStatStageEffectResult::Ok) return false;
        if (!event.move.hit) {
            m_battleFeedback = policy.move.blockedBeforeAccuracy ? "Move blocked by ability" : "Move missed";
        } else {
            m_battleFeedback = (event.move.stages.changedStatMask || event.reflection.changedStatMask)
                ? "Stats changed" : "Stats unchanged";
        }
        return true;
    }
    if (supportsPokemonTrickRoomMove(move->id)) {
        PokemonTrickRoomCommandPolicy policy{};
        policy.resolved = true;
        policy.ppCost = pp.cost;
        PokemonTrickRoomCommandEvent event{};
        if (usePokemonTrickRoomCommand(user, m_trickRoom, moveSlot, policy, event) !=
                PokemonTrickRoomCommandResult::Ok) return false;
        m_battleFeedback = event.field.activated ? "Trick Room activated" : "Trick Room removed";
        return true;
    }
    PokemonMoveWeatherContext weather{};
    PokemonHitPolicy hit{};
    PokemonCriticalPolicy critical{};
    const PokemonWeatherAbilityComponent activeAbilities[2] = {
        {user.abilityId, true, true},
        {opponent.abilityId, true, false}
    };
    if (!resolveActiveMoveWeather(user, opponent, weather) ||
        !composePokemonAlwaysHitPolicy(activeAbilities, 2, hit, move->id, &weather) ||
        !resolveActiveMoveCritical(user, opponent, critical)) return false;
    ResolvedPokemon* resolvedActors[] = {&m_context.player, &m_context.enemy, &m_context.secondEnemy};
    auto* targetBossState = &resolvedActors[targetIndex]->bossState;
    auto nextBossState = *targetBossState;
    const bool targetIsBoss = nextBossState.segmentCount != 0;
    PokemonBossDamagePolicy bossPolicy{};
    const auto* bossAbility = PokerogueContent::findAbilityMovegenProfile(opponent.abilityId);
    bossPolicy.resolved = bossPolicy.damageCallbacksResolved =
        bossAbility && bossAbility->bossDamageCallbacksResolved;
    if (targetIsBoss && !bossPolicy.resolved) {
        m_battleFeedback = "Boss damage callbacks require dispatcher";
        return false;
    }
    if (damageRecoilProfile(move->id)) {
        auto nextUser = user;
        auto nextOpponent = opponent;
        auto nextRng = rng;
        PokemonMoveActionResult attack{};
        if (useStandardPokemonMove(nextUser, nextOpponent, moveSlot, false, nextRng,
            attack, &weather, &critical, &hit, &pp, targetIsBoss ? &nextBossState : nullptr,
            targetIsBoss ? &bossPolicy : nullptr) != PokemonMoveActionStatus::Ok) return false;
        const auto policy = canonicalFreshActorRecoilPolicy(user.abilityId);
        PokemonRecoilEvent recoil{};
        if (applyPokemonRecoil(nextUser, move->id, attack.damageApplied,
            attack.damageRoll.hit && !attack.weatherCancelled, policy, recoil) != PokemonRecoilResult::Ok)
            return false;
        user = nextUser;
        opponent = nextOpponent;
        rng = nextRng;
        if (targetIsBoss) *targetBossState = nextBossState;
        m_battleFeedback = recoil.damage ? "Attack caused recoil" :
            attack.weatherCancelled ? "Move blocked by weather" :
            attack.damageRoll.hit ? "Attack hit" : "Attack missed";
        return true;
    }
    if (damageDrainProfile(move->id)) {
        if (hasCanonicalReverseDrain(opponent.abilityId)) return false;
        auto nextUser = user;
        auto nextOpponent = opponent;
        auto nextRng = rng;
        PokemonMoveActionResult attack{};
        if (useStandardPokemonMove(nextUser, nextOpponent, moveSlot, false, nextRng,
            attack, &weather, &critical, &hit, &pp, targetIsBoss ? &nextBossState : nullptr,
            targetIsBoss ? &bossPolicy : nullptr) != PokemonMoveActionStatus::Ok) return false;
        PokemonDrainPolicy policy{};
        policy.resolved = true;
        PokemonDrainEvent event{};
        if (attack.damageRoll.hit && !attack.weatherCancelled && attack.damageApplied &&
            applyPokemonDamageDrain(nextUser, move->id, attack.damageApplied, policy, event) !=
                PokemonHealingResult::Ok) return false;
        user = nextUser;
        opponent = nextOpponent;
        rng = nextRng;
        if (targetIsBoss) *targetBossState = nextBossState;
        m_battleFeedback = event.healed ? "Attack drained HP" :
            attack.weatherCancelled ? "Move blocked by weather" :
            attack.damageRoll.hit ? "Attack hit" : "Attack missed";
        return true;
    }
    PokemonMoveActionResult result{};
    if (useStandardPokemonMove(user, opponent, moveSlot, false, rng, result,
            &weather, &critical, &hit, &pp, targetIsBoss ? &nextBossState : nullptr,
            targetIsBoss ? &bossPolicy : nullptr) != PokemonMoveActionStatus::Ok) return false;
    if (targetIsBoss) *targetBossState = nextBossState;
    const bool enemyActs = userIndex != 0;
    m_battleFeedback = result.weatherCancelled
        ? (enemyActs ? "Enemy move blocked by weather" : "Your move blocked by weather")
        : result.damageRoll.hit ? (enemyActs ? "Enemy move hit" : "Your move hit")
                               : (enemyActs ? "Enemy move missed" : "Your move missed");
    return true;
}

bool FirstRunRuntime::executeActiveBattleMove(bool enemyActs, uint8_t moveSlot,
    PokerogueRngAdapter& rng) {
    return executeActiveBattleMove(enemyActs ? 1 : 0, enemyActs ? 0 : 1, moveSlot, rng);
}

bool FirstRunRuntime::finishBattleTurn() {
    // TurnEndPhase lapses arena tags except during a biome interlude.
    // Current checkpoint progression ends before the first X0 transition.
    // Resolve both field clocks before committing either. TurnEndPhase lapses
    // weather even during an interlude; arena tags have a separate interlude gate.
    // Actors requiring unported weather callbacks remain gated before a command.
    auto nextPlayer = m_context.player.battleState;
    auto nextEnemy = m_context.enemy.battleState;
    auto nextSecondEnemy = m_context.secondEnemy.battleState;
    PokemonWeatherPhaseEvent residual{};
    const bool allEnemiesDown = m_doubleBattle
        ? (!nextEnemy.hp && !nextSecondEnemy.hp)
        : (!nextEnemy.hp);
    const bool upcomingInterlude = m_run.wave < PokerogueContent::kClassicFinalWave &&
        m_run.wave % 10 == 0 && nextPlayer.hp &&
        allEnemiesDown && enemyPartyDefeated();
    if (!applyPokemonMultiWeatherPhase(nextPlayer, nextEnemy, m_doubleBattle ? &nextSecondEnemy : nullptr,
            m_arenaWeather, upcomingInterlude, residual)) {
        m_battleFeedback = "Weather effects require ability dispatcher";
        buildScene();
        return false;
    }
    auto nextRoom = m_trickRoom;
    auto nextWeather = m_arenaWeather;
    PokemonTrickRoomEvent roomEvent{};
    PokemonWeatherTurnEndEvent weatherEvent{};
    if (!advancePokemonTrickRoomTurnEnd(nextRoom, roomEvent)) {
        m_battleFeedback = "Invalid Trick Room field state";
        buildScene();
        return false;
    }
    if (!advancePokemonArenaWeatherTurnEnd(nextWeather, weatherEvent)) {
        m_battleFeedback = "Invalid arena weather field state";
        buildScene();
        return false;
    }
    // Form reversion must execute before a changed weather state can be committed.
    if (weatherEvent.requestWeatherFormReversion && !weatherBattleSupported()) {
        m_battleFeedback = "Weather expiry requires form reversion resolver";
        buildScene();
        return false;
    }
    m_context.player.battleState = nextPlayer;
    m_context.enemy.battleState = nextEnemy;
    if (m_doubleBattle) m_context.secondEnemy.battleState = nextSecondEnemy;
    m_trickRoom = nextRoom;
    m_arenaWeather = nextWeather;
    m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;
    const bool playerDown = !m_context.player.battleState.hp;
    const bool enemiesDownNow = m_doubleBattle
        ? (!m_context.enemy.battleState.hp && !m_context.secondEnemy.battleState.hp)
        : (!m_context.enemy.battleState.hp);
    if (playerDown && !playerPartyDefeated()) {
        advancePlayerAfterDefeat();
        ++m_turn;
        if (!m_battleRng.beginTurn(m_turn)) {
            m_battleFeedback = "Next battle RNG turn could not initialize";
            buildScene();
            return false;
        }
        buildScene();
        return true;
    }
    if (playerDown || enemiesDownNow) {
        m_battleFinished = true;
        m_playerWon = enemiesDownNow && !playerDown;
        m_victoryPlan = {};
        if (m_playerWon && enemyPartyDefeated() && !planClassicVictory(m_run.wave, m_victoryPlan)) {
            m_battleFeedback = "Classic victory plan unavailable";
            buildScene();
            return false;
        }
        if (m_playerWon) {
            if (m_victoryPlan.contains(ClassicVictoryStep::GameClear)) {
                m_battleFeedback = "Game Clear! Classic completed";
            } else {
                m_battleFeedback = "Enemy defeated - experience pending";
            }
        } else {
            m_battleFeedback = "Pokemon fainted - run end pending";
        }
        m_checkpointAvailable = true;
    } else {
        ++m_turn;
        if (!m_battleRng.beginTurn(m_turn)) {
            m_battleFeedback = "Next battle RNG turn could not initialize";
            buildScene();
            return false;
        }
        m_checkpointAvailable = true;
    }
    buildScene();
    return true;
}

// VictoryPhase queues BattleEnd/rewards only when no enemy party member remains.
// Source: pinned src/phases/victory-phase.ts, VictoryPhase.start/getEnemyParty.
bool FirstRunRuntime::enemyPartyDefeated() const {
    if (!m_encounterResolved) return false;
    if (m_doubleBattle) {
        return m_context.enemy.battleState.hp == 0 && m_context.secondEnemy.battleState.hp == 0;
    }
    if (m_context.enemy.battleState.hp) return false;
    if (!m_trainerBattle) return true;
    if (!m_context.trainerPartyBattleStatesResolved || !m_context.trainerPartyCount ||
        m_context.trainerPartyCount > 6 ||
        m_context.activeTrainerPartyIndex >= m_context.trainerPartyCount) return false;
    for (uint8_t member = 0; member < m_context.trainerPartyCount; ++member) {
        // The active actor is authoritative; its stored party copy may precede the KO.
        if (member != m_context.activeTrainerPartyIndex &&
            m_context.trainerParty[member].battleState.hp) return false;
    }
    return true;
}

bool FirstRunRuntime::skipVictoryReward() {
    FirstRunRuntime candidate = *this;
    if (!candidate.skipVictoryRewardInPlace()) {
        m_battleFeedback = candidate.m_battleFeedback;
        buildScene();
        return false;
    }
    *this = candidate;
    buildScene();
    return true;
}

bool FirstRunRuntime::skipVictoryRewardInPlace() {
    if (!m_battleFinished || !m_playerWon || !m_experienceGranted ||
        !enemyPartyDefeated() || !m_victoryPlan.contains(ClassicVictoryStep::SelectModifier) ||
        !m_victoryPlan.nextWave || m_victoryPlan.nextWave > PokerogueContent::kClassicFinalWave)
        return false;
    // SelectModifierPhase permits skipping its choice. The one-starter run
    // has no reward modifier to carry into the next wave in this branch.
    m_rewardsPending = false;
    m_run.wave = m_victoryPlan.nextWave;
    resolve(true);
    if (!m_encounterResolved) m_checkpointAvailable = false;
    if (m_encounterResolved)
        m_battleFeedback = m_trainerBattle ? "Trainer battle ahead" : "Reward skipped - next Classic wave";
    buildScene();
    return m_encounterResolved;
}

bool FirstRunRuntime::advanceTrainerAfterDefeat() {
    if (!m_trainerBattle || !m_battleFinished || !m_playerWon ||
        !m_experienceGranted || m_context.enemy.battleState.hp ||
        !m_context.player.battleState.hp || !m_context.trainerPartyBattleStatesResolved ||
        m_context.activeTrainerPartyIndex >= m_context.trainerPartyCount) return false;
    bool hasReserve = false;
    for (uint8_t member = 0; member < m_context.trainerPartyCount; ++member)
        if (member != m_context.activeTrainerPartyIndex &&
            m_context.trainerParty[member].battleState.hp) hasReserve = true;
    if (!hasReserve) {
        m_context.trainerParty[m_context.activeTrainerPartyIndex] = m_context.enemy;
        return true;
    }
    refreshTrainerBaselineMatchups();
    const uint8_t next = m_context.nextTrainerPartyIndex;
    if (next >= m_context.trainerPartyCount ||
        !m_context.trainerParty[next].battleState.hp || m_turn == 0xFFFFFFFFu) return false;
    // Prepare the next turn stream before committing the actor replacement.
    PokerogueBattleRng nextTurn = m_battleRng;
    if (!nextTurn.beginTurn(m_turn + 1)) return false;
    m_context.trainerParty[m_context.activeTrainerPartyIndex] = m_context.enemy;
    m_context.activeTrainerPartyIndex = next;
    m_context.enemy = m_context.trainerParty[next];
    resetPokemonStatStages(m_context.enemy.battleState);
    m_run.encounterDex = m_context.enemy.dex;
    m_battleRng = nextTurn;
    ++m_turn;
    m_battleFinished = false;
    m_playerWon = false;
    m_experienceGranted = false;
    m_victoryPlan = {};
    m_checkpointAvailable = true;
    refreshTrainerBaselineMatchups();
    return true;
}

void FirstRunRuntime::refreshTrainerBaselineMatchups() {
    m_context.nextTrainerPartyIndex = 0xFF;
    m_context.trainerPartyBaselineMatchupResolved = false;
    for (auto& score : m_context.trainerPartyBaselineMatchupScores) score = 0;
    if (!m_trainerBattle || !m_context.trainerPartyBattleStatesResolved ||
        !m_context.trainerPartyCount || m_context.trainerPartyCount > 6 ||
        m_context.activeTrainerPartyIndex >= m_context.trainerPartyCount) return;
    PokemonMoveWeatherContext speedWeather{};
    uint32_t opponentSpeed = 0;
    if (!resolveActiveMoveWeather(false, speedWeather) ||
        !pokemonWeatherEffectiveSpeed(m_context.player.battleState, speedWeather, opponentSpeed)) return;
    double scores[6]{};
    for (uint8_t member = 0; member < m_context.trainerPartyCount; ++member) {
        const bool active = member == m_context.activeTrainerPartyIndex;
        const auto& state = active ? m_context.enemy.battleState
            : m_context.trainerParty[member].battleState;
        PokemonTrainerMatchupInput matchup{};
        uint32_t actorSpeed = state.stats[5]; // Reserve getStat(SPD, false): no stages/abilities.
        if ((active && !pokemonWeatherEffectiveSpeed(state, speedWeather, actorSpeed)) ||
            !actorSpeed ||
            !buildBaselineTrainerMatchupInput(state, m_context.player.battleState,
                actorSpeed, opponentSpeed, active, matchup) ||
            !calculateTrainerMatchupScore(matchup, scores[member])) return;
    }
    for (uint8_t member = 0; member < m_context.trainerPartyCount; ++member)
        m_context.trainerPartyBaselineMatchupScores[member] = scores[member];
    m_context.trainerPartyBaselineMatchupResolved = true;
    // Preview the legal replacement with the source wave-scoped stream.
    // Never consume the battle-turn RNG while inspecting the trainer party.
    const auto* opponentSpecies = PokerogueContent::findSpeciesByDex(m_context.player.battleState.speciesDex);
    if (!opponentSpecies || opponentSpecies->legendary < 0) return;
    double reserveScores[6]{};
    uint8_t reserveIndexes[6]{};
    uint8_t reserveCount = 0;
    for (uint8_t member = 0; member < m_context.trainerPartyCount; ++member) {
        if (member == m_context.activeTrainerPartyIndex || !m_context.trainerParty[member].battleState.hp) continue;
        reserveScores[reserveCount] = opponentSpecies->legendary ? scores[member] / 2.0 : scores[member];
        reserveIndexes[reserveCount++] = member;
    }
    uint16_t waveSeed[PokerogueRngAdapter::kMaxSeedCodeUnits]{};
    if (!PokerogueRngAdapter::shiftCharCodes(m_seedCodeUnits.data(), m_seedLength,
            m_run.wave, waveSeed, PokerogueRngAdapter::kMaxSeedCodeUnits)) return;
    PokerogueRngAdapter replacementRng;
    PokerogueSeedOffsetScope scope(replacementRng, waveSeed, m_seedLength, m_turn << 2);
    if (!scope.valid()) return;
    uint8_t replacement = 0xFF;
    if (reserveCount && selectTrainerSummonIndex(reserveScores, reserveIndexes, reserveCount,
            replacementRng, replacement)) m_context.nextTrainerPartyIndex = replacement;
}

bool FirstRunRuntime::throwPokeball(PokeballType ball) {
    // Commands run before render. Commit the complete action only on success.
    FirstRunRuntime candidate = *this;
    if (!candidate.throwPokeballInPlace(ball)) {
        m_battleFeedback = candidate.m_battleFeedback;
        buildScene();
        return false;
    }
    *this = candidate;
    buildScene(); // Rebind scene/text pointers after copying the candidate.
    return true;
}

bool FirstRunRuntime::throwPokeballInPlace(PokeballType ball) {
    if (m_battleFinished) {
        m_battleFeedback = "Battle is already finished";
        buildScene();
        return false;
    }
    if (m_trainerBattle) {
        m_battleFeedback = "Cannot catch a trainer's Pokémon!";
        buildScene();
        return false;
    }
    const auto ballIdx = static_cast<uint8_t>(ball);
    if (ballIdx >= 6 || m_pokeballs[ballIdx] == 0) {
        m_battleFeedback = "No Poké Balls of that type remaining!";
        buildScene();
        return false;
    }
    if (m_doubleBattle && m_context.enemy.battleState.hp > 0 && m_context.secondEnemy.battleState.hp > 0) {
        m_battleFeedback = "Cannot catch while two enemies are present!";
        buildScene();
        return false;
    }
    if (m_run.wave == PokerogueContent::kClassicFinalWave) {
        m_battleFeedback = "The final boss cannot be caught!";
        buildScene();
        return false;
    }

    auto* rng = m_battleRng.currentStream();
    if (!rng) {
        m_battleFeedback = "Battle RNG is not initialized";
        buildScene();
        return false;
    }

    ResolvedPokemon* target = &m_context.enemy;
    if (m_doubleBattle) {
        if (m_context.enemy.battleState.hp == 0 && m_context.secondEnemy.battleState.hp > 0) {
            target = &m_context.secondEnemy;
        } else if (m_selectedTarget == 1 && m_context.secondEnemy.battleState.hp > 0) {
            target = &m_context.secondEnemy;
        }
    }

    if (target->battleState.hp == 0) {
        m_battleFeedback = "Target is already fainted!";
        buildScene();
        return false;
    }

    bool bossShieldBlocked = false;
    if (!pokemonBossShieldCaptureBlocked(target->bossState, target->battleState.abilityId, ball, bossShieldBlocked)) {
        m_battleFeedback = "Boss capture policy could not resolve";
        return false;
    }
    if (bossShieldBlocked) {
        m_battleFeedback = "Weaken the boss before throwing a ball";
        return false;
    }
    --m_pokeballs[ballIdx];

    PokemonCaptureEvent captureEvent{};
    const bool isFinalBoss = (m_run.wave == PokerogueContent::kClassicFinalWave);
    if (!executeCaptureAttempt(target->battleState, ball, false, false, bossShieldBlocked, isFinalBoss, *rng, captureEvent)) {
        m_battleFeedback = "Capture inputs could not resolve";
        buildScene();
        return false;
    }

    if (captureEvent.caught) {
        if (m_context.playerPartyCount < 6) {
            ResolvedPokemon caughtMon = *target;
            caughtMon.bossState = {}; // PlayerPokemon is not an EnemyPokemon boss.
            const auto* caughtSpecies = PokerogueContent::findSpeciesByDex(caughtMon.dex);
            if (!caughtSpecies || pokemonTotalExperienceForLevel(caughtSpecies->growthRate,
                    caughtMon.level, caughtMon.totalExperience) != PokemonExperienceResult::Ok) {
                m_battleFeedback = "Captured Pokemon experience could not resolve";
                return false;
            }
            // EnemyPokemon.addToParty passes the source into PlayerPokemon:
            // preserve capture HP/PP; remove the enemy only after copying it.
            resetPokemonStatStages(caughtMon.battleState);
            m_context.playerParty[m_context.playerPartyCount++] = caughtMon;
        }
        target->battleState.hp = 0;

        m_checkpointAvailable = false;
        m_runStarted = true;

        if (enemyPartyDefeated()) {
            if (!grantVictoryExperience() || !planClassicVictory(m_run.wave, m_victoryPlan)) {
                m_battleFeedback = "Capture victory could not resolve";
                return false;
            }
            m_playerWon = true;
            m_battleFinished = true;
            m_battleFeedback = std::string("Gotcha! ") + (target->localizedName ? target->localizedName : "Pokémon") + " was caught!";
        } else {
            m_battleFeedback = std::string("Caught ") + (target->localizedName ? target->localizedName : "Pokémon") + "! Defeat remaining foe.";
        }
        return finishBattleTurn();
    } else {
        m_checkpointAvailable = false;
        m_runStarted = true;
        std::string fb = "Broke free after " + std::to_string(captureEvent.shakeCount) + " shake(s)!";

        if (m_context.enemy.battleState.hp > 0) {
            PokerogueRngAdapter enemyActionRng = *rng;
            if (!executeEnemyResponse(1, enemyActionRng)) {
                m_battleFeedback = "Enemy response could not resolve";
                buildScene();
                return false;
            }
            *rng = enemyActionRng;
        }
        if (m_doubleBattle && m_context.secondEnemy.battleState.hp > 0) {
            PokerogueRngAdapter secondEnemyRng = *rng;
            if (!executeEnemyResponse(2, secondEnemyRng)) {
                m_battleFeedback = "Second enemy response could not resolve";
                buildScene();
                return false;
            }
            *rng = secondEnemyRng;
        }
        m_battleFeedback = fb + " " + m_battleFeedback;
        return finishBattleTurn();
    }
}

bool FirstRunRuntime::switchPlayerPokemon(uint8_t targetIndex) {
    // Commands run before render. Commit the complete action only on success.
    FirstRunRuntime candidate = *this;
    if (!candidate.switchPlayerPokemonInPlace(targetIndex)) {
        m_battleFeedback = candidate.m_battleFeedback;
        buildScene();
        return false;
    }
    *this = candidate;
    buildScene(); // Rebind scene/text pointers after copying the candidate.
    return true;
}

bool FirstRunRuntime::switchPlayerPokemonInPlace(uint8_t targetIndex) {
    if (m_battleFinished) return false;
    if (targetIndex >= m_context.playerPartyCount || targetIndex == m_context.activePlayerPartyIndex) {
        m_battleFeedback = "Invalid party member selected";
        buildScene();
        return false;
    }
    if (m_context.playerParty[targetIndex].battleState.hp == 0) {
        m_battleFeedback = "Cannot switch to a fainted Pokémon!";
        buildScene();
        return false;
    }

    auto* rng = m_battleRng.currentStream();
    if (!rng) return false;

    m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;
    m_context.activePlayerPartyIndex = targetIndex;
    m_context.player = m_context.playerParty[targetIndex];
    resetPokemonStatStages(m_context.player.battleState);

    m_checkpointAvailable = false;
    m_runStarted = true;
    m_battleFeedback = std::string("Go, ") + (m_context.player.localizedName ? m_context.player.localizedName : "Pokémon") + "!";

    if (m_trainerBattle) {
        PokerogueRngAdapter enemyActionRng = *rng;
        if (!executeEnemyResponse(1, enemyActionRng)) {
                m_battleFeedback = "Enemy response could not resolve";
                buildScene();
                return false;
            }
        *rng = enemyActionRng;
    } else {
        if (m_context.enemy.battleState.hp > 0) {
            PokerogueRngAdapter enemyActionRng = *rng;
            if (!executeEnemyResponse(1, enemyActionRng)) {
                m_battleFeedback = "Enemy response could not resolve";
                buildScene();
                return false;
            }
            *rng = enemyActionRng;
        }
        if (m_doubleBattle && m_context.secondEnemy.battleState.hp > 0) {
            PokerogueRngAdapter secondEnemyRng = *rng;
            if (!executeEnemyResponse(2, secondEnemyRng)) {
                m_battleFeedback = "Second enemy response could not resolve";
                buildScene();
                return false;
            }
            *rng = secondEnemyRng;
        }
    }

    return finishBattleTurn();
}

bool FirstRunRuntime::playerPartyDefeated() const {
    if (m_context.player.battleState.hp > 0) return false;
    for (uint8_t i = 0; i < m_context.playerPartyCount; ++i) {
        if (m_context.playerParty[i].battleState.hp > 0) return false;
    }
    return true;
}

bool FirstRunRuntime::advancePlayerAfterDefeat() {
    if (m_context.player.battleState.hp > 0) return true;
    m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;

    for (uint8_t i = 0; i < m_context.playerPartyCount; ++i) {
        if (m_context.playerParty[i].battleState.hp > 0) {
            m_context.activePlayerPartyIndex = i;
            m_context.player = m_context.playerParty[i];
            resetPokemonStatStages(m_context.player.battleState);
            m_battleFeedback = std::string("Fainted! Sent out ") +
                (m_context.player.localizedName ? m_context.player.localizedName : "next Pokémon");
            buildScene();
            return true;
        }
    }
    return false;
}

void FirstRunRuntime::resolve(bool carryPlayer) {
    m_selectedBattleMove = 0;
    m_turn = 1;
    m_enemySwitchCounter = 0;
    m_battleFinished = false;
    m_playerWon = false;
    m_experienceGranted = false;
    m_victoryPlan = {};
    m_rewardChoices = {};
    m_rewardChoiceCount = 0;
    m_selectedRewardChoice = 0;
    m_rewardsPending = false;
    m_runStarted = carryPlayer;
    m_checkpointAvailable = !carryPlayer;
    m_battleFeedback.clear();
    m_encounterResolved = false;
    m_doubleBattle = false;
    m_trainerBattle = false;
    m_secondEncounterResolved = false;
    m_context.enemy = {};
    m_context.secondEnemy = {};
    m_run.encounterDex = 0;
    m_context.trainerTypeId = 0;
    m_context.trainerName = nullptr;
    m_context.trainerPartyTemplateKey = nullptr;
    m_context.trainerPartyCount = 0;
    m_context.activeTrainerPartyIndex = 0xFF;
    m_context.trainerFemaleVariant = false;
    m_context.trainerPartySpeciesResolved = false;
    m_context.trainerPartyConstructorResolved = false;
    m_context.trainerPartyLevelMovesResolved = false;
    m_context.trainerPartySupercedenceResolved = false;
    m_context.trainerPartyHardMoveFilterResolved = false;
    m_context.trainerPartySingleMoveFilterResolved = false;
    m_context.trainerPartyBaseWeightsResolved = false;
    m_context.trainerPartyDamageWeightsResolved = false;
    m_context.trainerPartyMovesetsResolved = false;
    m_context.trainerPartyIvsResolved = false;
    m_context.trainerPartyBattleStatesResolved = false;
    m_context.nextTrainerPartyIndex = 0xFF;
    m_context.trainerPartyBaselineMatchupResolved = false;
    for (auto& score : m_context.trainerPartyBaselineMatchupScores) score = 0;
    for (auto& level : m_context.trainerPartyLevels) level = 0;
    for (auto& member : m_context.trainerParty) member = {};
    for (auto& state : m_trainerConstructorRngStates) state = {};
    for (auto& state : m_trainerPostMovesetRngStates) state = {};
    for (auto& state : m_trainerPostIvRngStates) state = {};
    for (auto& count : m_context.trainerPartyLevelMoveCounts) count = 0;
    for (auto& count : m_context.trainerPartySupersededMoveCounts) count = 0;
    for (auto& count : m_context.trainerPartyHardEligibleMoveCounts) count = 0;
    for (auto& count : m_context.trainerPartySingleEligibleMoveCounts) count = 0;
    const auto& starter = PokerogueContent::kSpecies[m_starterIndex];
    m_run.starterDex = starter.dex;
    m_context.modeName = locale("gameMode:classic", "Classic");
    // Segment transitions: every 10 waves in Classic, transition to next biome.
    if (m_run.wave > 1 && (m_run.wave - 1) % 10 == 0) {
        const char* nextBiomeId = nullptr;
        if (resolveClassicNextBiome(m_run.biomeId, m_run.wave, true,
                m_seedCodeUnits.data(), m_seedLength, false, nullptr,
                nextBiomeId) == ClassicBiomeTransitionResult::Ok && nextBiomeId) {
            m_run.biomeId = nextBiomeId;
        }
    }
    const auto* biomeEntry = findBiomeById(m_run.biomeId);
    const std::string biomeLocaleId = std::string("biomes:") + m_run.biomeId;
    m_context.biomeName = locale(biomeLocaleId.c_str(), biomeEntry ? biomeEntry->name : m_run.biomeId);
    const std::string starterLocaleId = std::string("pokemon:") + starter.id;
    if (!carryPlayer) {
        m_context.player = {starter.dex, 5, starter.id, locale(starterLocaleId.c_str(), starter.name), starter.firstFormId, starter.assetSourcePath};
        if (pokemonTotalExperienceForLevel(starter.growthRate, 5, m_context.player.totalExperience)
            != PokemonExperienceResult::Ok) return;
    }
    // Fresh-profile save data starts with no unlocked egg moves and no saved
    // move preferences. Resolve its actual level-1-to-5 learnset through the
    // same canonical catalog as the wild actor; do not invent a starter list.
    if (!carryPlayer) {
      m_context.player.movesetResolved = selectPokemonStarterMoveset(
          starter.dex, starter.firstFormId, 0, nullptr, 0,
          m_context.player.moveIds, m_context.player.moveCount) ==
          PokemonStarterMovesetResult::Ok;
      if (m_context.player.movesetResolved) {
        PokemonActorIdentity starterActor{};
        PokemonNature starterNature = PokemonNature::Unspecified;
        if (pokemonFreshProfileNature(starter.dex, starterNature) != PokemonFreshProfileResult::Ok)
            return;
        const char* starterFormId = starter.firstFormId && *starter.firstFormId
            ? starter.firstFormId : nullptr;

        // The new profile's DexData supplies 15 IVs, the first unlocked
        // ability, default male gender (or genderless), base form, and its
        // source-derived default nature. Pokemon's constructor still consumes
        // its PID and initial Tera pick from the run's root RNG stream.
        PokerogueRngAdapter starterRng;
        starterRng.sow(m_seedCodeUnits.data(), m_seedLength);
        starterActor.pokemonId = starterRng.randSeedUint32();
        starterActor.abilityIndex = 0;
        starterActor.gender = starter.malePercentTenths == 65534
            ? PokemonGender::Genderless : PokemonGender::Male;
        starterActor.nature = starterNature;
        starterActor.formId = starterFormId;
        for (uint8_t& iv : starterActor.ivs) iv = 15;

        const auto* starterForm = PokerogueContent::findFormById(starterFormId);
        const char* starterType1 = starterForm ? starterForm->type1 : starter.type1;
        const char* starterType2 = starterForm ? starterForm->type2 : starter.type2;
        if (!starterType1 || !*starterType1) return;
        const bool hasSecondaryType = starterType2 && *starterType2 &&
            std::strcmp(starterType2, "NONE") != 0;
        starterActor.initialTeraTypeIndex = static_cast<uint8_t>(
            starterRng.randSeedInt(hasSecondaryType ? 2 : 1));
        starterActor.initialTeraTypeResolved = true;

        PokemonBattleInit starterInput{};
        starterInput.speciesDex = starter.dex;
        starterInput.formId = starterActor.formId;
        starterInput.level = m_context.player.level;
        starterInput.pokemonId = starterActor.pokemonId;
        starterInput.nature = starterActor.nature;
        starterInput.gender = starterActor.gender;
        starterInput.abilityId = starterForm && starterForm->ability1
            ? starterForm->ability1 : starter.ability1;
        for (uint8_t i = 0; i < 6; ++i) starterInput.ivs[i] = starterActor.ivs[i];
        starterInput.moveCount = m_context.player.moveCount;
        for (uint8_t i = 0; i < starterInput.moveCount; ++i)
            starterInput.moveIds[i] = m_context.player.moveIds[i];
        if (initializePokemonBattleState(starterInput, m_context.player.battleState) !=
            PokemonBattleInitResult::Ok) return;
        m_context.player.actor = starterActor;
        m_context.player.actorIdentityResolved = true;
        m_context.player.formId = starterActor.formId;
        if (!carryPlayer) {
            m_context.playerParty[0] = m_context.player;
            m_context.playerPartyCount = 1;
            m_context.activePlayerPartyIndex = 0;
        } else {
            m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;
        }
      }
    }

    uint8_t cycleOffset = 0;
    if (!PokerogueWaveClock::deriveCycleOffset(m_seedCodeUnits.data(), m_seedLength, cycleOffset)) return;
    const auto time = PokerogueWaveClock::timeOfDay(m_run.wave, cycleOffset);

    // BattleScene.resetSeed(waveIndex) establishes a fresh wave stream.
    // checkIsDouble consumes its draw before Arena.randomSpecies, after any
    // ordinary trainer-chance decision for that wave.
    PokerogueRngAdapter waveRng;
    PokerogueSeedOffsetScope waveScope(waveRng, m_seedCodeUnits.data(), m_seedLength, m_run.wave);
    if (!waveScope.valid()) return;
    const ClassicWaveKind waveKind = classifyClassicWave(m_run.wave);
    // BattleScene.doPostBattleCleanup recalls the player field at these boundaries.
    // ReturnPhase.resetSummonData removes stat stages while HP/PP/EXP persist.
    const auto resetPlayerArenaState = [&]() {
        m_trickRoom = {};
        resetPokemonStatStages(m_context.player.battleState);
        for (uint8_t i = 0; i < m_context.playerPartyCount; ++i)
            resetPokemonStatStages(m_context.playerParty[i].battleState);
        if (m_context.activePlayerPartyIndex < m_context.playerPartyCount)
            m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;
    };
    if (carryPlayer && ((m_run.wave > 1 && (m_run.wave - 1) % 10 == 0) ||
            m_run.wave == PokerogueContent::kClassicFinalWave)) resetPlayerArenaState();
    const PokerogueContent::TrainerType* randomTrainer = nullptr;
    if (waveKind == ClassicWaveKind::TrainerChanceRequired) {
        bool offsetGym = false;
        bool isTrainer = false;
        if (!PokerogueWaveClock::deriveOffsetGym(m_seedCodeUnits.data(), m_seedLength, offsetGym) ||
            resolveClassicTrainerChance(m_run.wave, m_run.biomeId, offsetGym,
                m_seedCodeUnits.data(), m_seedLength, waveRng, isTrainer) ==
                ClassicTrainerDecision::Invalid) return;
        if (isTrainer) {
            m_trainerBattle = true;
            resetPlayerArenaState(); // New trainer battle recalls the player field.
            const auto selectedTrainer = PokerogueEncounterResolver::resolveTrainerType(
                m_run.biomeId, false, false, waveRng);
            if (!selectedTrainer.valid || !selectedTrainer.trainerType) {
                m_battleFeedback = "Canonical trainer pool could not resolve";
                buildScene();
                return;
            }
            randomTrainer = selectedTrainer.trainerType;
            // generateNewBattleTrainer draws double variant before gender/template.
            // Double-capable configs require their complete chance/variant resolver.
            if (randomTrainer->flags & (2U | 4U)) {
                m_battleFeedback = "Random trainer double variant requires resolver";
                buildScene();
                return;
            }
        }
    }
    if (randomTrainer || waveKind == ClassicWaveKind::FixedTrainerBattle) {
        m_trainerBattle = true;
        resetPlayerArenaState(); // New trainer battle recalls the player field.
        const auto* fixedBattle = PokerogueContent::findClassicFixedBattleWave(m_run.wave);
        const auto* trainer = randomTrainer ? randomTrainer :
            fixedBattle && fixedBattle->hasStaticTrainerType
            ? PokerogueContent::findTrainerType(fixedBattle->trainerTypeId) : nullptr;
        if (trainer) {
            m_context.trainerTypeId = trainer->id;
            m_context.trainerName = trainer->name;
            PokerogueRngAdapter trainerRng;
            PokerogueSeedOffsetScope trainerScope(trainerRng,
                m_seedCodeUnits.data(), m_seedLength,
                static_cast<uint32_t>(m_run.wave) << 8);
            if (!trainerScope.valid()) return;
            if (randomTrainer) {
                // Non-fixed generation continues the wave RNG after trainer pool selection.
                trainerRng = waveRng;
                m_context.trainerFemaleVariant = trainerRng.randSeedInt(2) != 0;
            } else if (fixedBattle->seededBinaryGenderVariant) {
                // handleFixedBattle invokes getTrainer under rootSeed + (wave << 8).
                // TOWN_YOUNGSTER draws its gender before Trainer.constructor
                // chooses a party template. The callback is source-normalized.
                m_context.trainerFemaleVariant = trainerRng.randSeedInt(2) != 0;
            } else {
                m_context.trainerFemaleVariant = false;
            }
            const auto chosen = selectTrainerPartyTemplate(*trainer, m_run.wave, trainerRng);
                if (chosen.supported && chosen.value) {
                    const auto levels = resolveClassicTrainerPartyLevels(*chosen.value, m_run.wave, false);
                    if (levels.supported) {
                        m_context.trainerPartyTemplateKey = chosen.value->key;
                        m_context.trainerPartyCount = levels.count;
                        for (uint8_t i = 0; i < levels.count; ++i)
                            m_context.trainerPartyLevels[i] = levels.values[i];
                        const PokerogueContent::Species* selectedSpecies[6]{};
                        TrainerPartyMemberTypes selectedTypes[6]{};
                        bool allSpeciesResolved = true;
                        bool allConstructorsResolved = true;
                        bool allLevelMovesResolved = true;
                        bool allSupercedenceResolved = true;
                        bool allHardMoveFiltersResolved = true;
                        bool allSingleMoveFiltersResolved = true;
                        bool allBaseWeightsResolved = true;
                        bool allDamageWeightsResolved = true;
                        bool allMovesetsResolved = true;
                        bool allIvsResolved = true;
                        bool allBattleStatesResolved = true;
                        for (uint8_t i = 0; i < levels.count; ++i) {
                            uint32_t memberOffset = 0;
                            if (!trainerPartyMemberSeedOffset(*trainer, m_run.wave, i, memberOffset)) {
                                allSpeciesResolved = false;
                                break;
                            }
                            PokerogueRngAdapter memberRng;
                            PokerogueSeedOffsetScope memberScope(memberRng,
                                m_seedCodeUnits.data(), m_seedLength, memberOffset);
                            if (!memberScope.valid()) {
                                allSpeciesResolved = false;
                                break;
                            }
                            TrainerPartySpeciesChoice member{};
                            const bool signatureSlot = trainer->signatureCount &&
                                i + trainer->signatureCount >= levels.count;
                            if (signatureSlot) {
                                const auto slotTemplate = trainerPartyMemberTemplate(*chosen.value, i);
                                if (!slotTemplate.supported) { allSpeciesResolved = false; break; }
                                const char* speciesId = resolveTrainerSignatureMemberSpecies(*trainer,
                                    levels.count, i, levels.values[i], slotTemplate.evolutionThresholdKindId,
                                    m_run.wave, memberRng);
                                member.species = trainerPartySpeciesById(speciesId);
                                member.baseSpeciesId = speciesId;
                                member.supported = member.species != nullptr;
                            } else {
                                member = resolveSimpleTrainerPoolMember(*trainer,
                                    *chosen.value, i, levels.values[i], m_run.wave,
                                    selectedSpecies, i, memberRng, selectedTypes);
                            }
                            if (!member.supported || !member.species) {
                                allSpeciesResolved = false;
                                break;
                            }
                            selectedSpecies[i] = member.species;
                            const std::string localeId = std::string("pokemon:") + member.species->id;
                            m_context.trainerParty[i] = {member.species->dex, levels.values[i],
                                member.species->id, locale(localeId.c_str(), member.species->name),
                                member.species->firstFormId, member.species->assetSourcePath};
                            PokemonFormSelectionContext formContext{};
                            formContext.biomeId = m_run.biomeId;
                            formContext.timeOfDay = time == PokerogueTimeOfDay::Day ? "DAY"
                                : time == PokerogueTimeOfDay::Dusk ? "DUSK"
                                : time == PokerogueTimeOfDay::Night ? "NIGHT" : "DAWN";
                            formContext.waveIndex = m_run.wave;
                            formContext.trainerBattle = true;
                            PokemonActorIdentity actor{};
                            if (generatePokemonActorForWildEncounter(member.species->dex, 256,
                                    formContext, memberRng, actor) != PokemonActorIdentityResult::Ok) {
                                allConstructorsResolved = false;
                                continue;
                            }
                            // This is only the shared Pokemon/EnemyPokemon constructor
                            // prefix. Trainer moveset generation and the later six IV
                            // draws must run before publishing a battle-ready actor.
                            m_context.trainerParty[i].actor = actor;
                            m_context.trainerParty[i].formId = actor.formId;
                            const auto* memberForm = actor.formId ? PokerogueContent::findFormById(actor.formId) : nullptr;
                            if (actor.formId && !memberForm) { allConstructorsResolved = false; break; }
                            selectedTypes[i] = {memberForm ? memberForm->type1 : member.species->type1,
                                memberForm ? memberForm->type2 : member.species->type2, true};
                            m_trainerConstructorRngStates[i] = memberRng.state();
                            PokemonLevelMoveCandidate levelMoves[128]{};
                            std::size_t levelMoveCount = 0;
                            if (buildPokemonLevelMovePool(member.species->dex,
                                    actor.formId, levels.values[i], levelMoves, 128,
                                    levelMoveCount, true) != PokemonLevelMovePoolResult::Ok) {
                                allLevelMovesResolved = false;
                                continue;
                            }
                            m_context.trainerPartyLevelMoveCounts[i] =
                                static_cast<uint16_t>(levelMoveCount);
                            PokemonLevelMoveCandidate nonSupercededMoves[128]{};
                            std::size_t nonSupercededCount = 0;
                            if (filterTrainerSupercededLevelMoves(levelMoves,
                                    levelMoveCount, nonSupercededMoves, 128,
                                    nonSupercededCount) != PokemonTrainerMoveFilterResult::Ok) {
                                allSupercedenceResolved = false;
                                continue;
                            }
                            m_context.trainerPartySupersededMoveCounts[i] =
                                static_cast<uint16_t>(nonSupercededCount);
                            PokemonLevelMoveCandidate eligibleMoves[128]{};
                            std::size_t eligibleCount = 0;
                            if (filterTrainerHardForbiddenLevelMoves(nonSupercededMoves,
                                    nonSupercededCount, eligibleMoves, 128, eligibleCount) !=
                                PokemonTrainerMoveFilterResult::Ok) {
                                allHardMoveFiltersResolved = false;
                                continue;
                            }
                            m_context.trainerPartyHardEligibleMoveCounts[i] =
                                static_cast<uint16_t>(eligibleCount);
                            // This fixed Youngster callback only constructs DEFAULT
                            // or FEMALE variants; upstream checkIsDouble treats both
                            // as singles. Other trainer variants need their own path.
                            PokemonLevelMoveCandidate singleMoves[128]{};
                            std::size_t singleCount = 0;
                            if (filterTrainerSingleBattleLevelMoves(eligibleMoves,
                                    eligibleCount, singleMoves, 128, singleCount) !=
                                PokemonTrainerMoveFilterResult::Ok) {
                                allSingleMoveFiltersResolved = false;
                                continue;
                            }
                            m_context.trainerPartySingleEligibleMoveCounts[i] =
                                static_cast<uint16_t>(singleCount);
                            PokemonTrainerBaseWeightedMove adjustedMoves[128]{};
                            std::size_t adjustedCount = 0;
                            if (adjustTrainerLevelMoveBaseWeights(singleMoves,
                                    singleCount, adjustedMoves, 128, adjustedCount) !=
                                PokemonTrainerMoveFilterResult::Ok ||
                                adjustedCount != singleCount) {
                                allBaseWeightsResolved = false;
                                continue;
                            }
                            PokemonBattleInit preMovegenInput{};
                            preMovegenInput.speciesDex = member.species->dex;
                            preMovegenInput.formId = actor.formId;
                            preMovegenInput.level = levels.values[i];
                            PokemonBattleState preMovegenState{};
                            if (initializePokemonBattleStateForActor(preMovegenInput,
                                    actor, preMovegenState) != PokemonBattleInitResult::Ok) {
                                allDamageWeightsResolved = false;
                                continue;
                            }
                            PokemonWeightedMove weightedMoves[128]{};
                            std::size_t weightedCount = 0;
                            if (weightTrainerLevelMoves(adjustedMoves, adjustedCount,
                                    preMovegenState.abilityId, preMovegenState.stats[1],
                                    preMovegenState.stats[3], preMovegenState.stats[2],
                                    weightedMoves, 128, weightedCount) !=
                                PokemonTrainerMovesetResult::Ok) {
                                allDamageWeightsResolved = false;
                                continue;
                            }
                            const auto* selectedForm = actor.formId
                                ? PokerogueContent::findFormById(actor.formId) : nullptr;
                            const char* type1 = selectedForm ? selectedForm->type1 : member.species->type1;
                            const char* type2 = selectedForm ? selectedForm->type2 : member.species->type2;
                            uint16_t moveIds[4]{};
                            uint8_t moveCount = 0;
                            if (selectTrainerMovesetFromWeightedPool(member.species->dex,
                                    false, false, weightedMoves, weightedCount, type1, type2,
                                    memberRng, moveIds, moveCount) !=
                                PokemonTrainerMovesetResult::Ok || !moveCount) {
                                allMovesetsResolved = false;
                                continue;
                            }
                            auto& partyMember = m_context.trainerParty[i];
                            partyMember.moveCount = moveCount;
                            partyMember.movesetResolved = true;
                            for (uint8_t slot = 0; slot < moveCount; ++slot)
                                partyMember.moveIds[slot] = moveIds[slot];
                            m_trainerPostMovesetRngStates[i] = memberRng.state();
                            uint8_t trainerIvs[6]{};
                            if (!generateClassicTrainerIvs(m_run.wave, memberRng,
                                    trainerIvs)) {
                                allIvsResolved = false;
                                continue;
                            }
                            for (uint8_t stat = 0; stat < 6; ++stat)
                                actor.ivs[stat] = trainerIvs[stat];
                            m_trainerPostIvRngStates[i] = memberRng.state();
                            partyMember.actor = actor;
                            PokemonBattleInit trainerBattleInput{};
                            trainerBattleInput.speciesDex = member.species->dex;
                            trainerBattleInput.formId = actor.formId;
                            trainerBattleInput.level = levels.values[i];
                            trainerBattleInput.pokemonId = actor.pokemonId;
                            trainerBattleInput.nature = actor.nature;
                            trainerBattleInput.abilityId = preMovegenState.abilityId;
                            trainerBattleInput.gender = actor.gender;
                            trainerBattleInput.moveCount = moveCount;
                            for (uint8_t stat = 0; stat < 6; ++stat)
                                trainerBattleInput.ivs[stat] = trainerIvs[stat];
                            for (uint8_t slot = 0; slot < moveCount; ++slot)
                                trainerBattleInput.moveIds[slot] = moveIds[slot];
                            PokemonBattleState trainerBattleState{};
                            if (initializePokemonBattleState(trainerBattleInput,
                                    trainerBattleState) != PokemonBattleInitResult::Ok) {
                                allBattleStatesResolved = false;
                                continue;
                            }
                            partyMember.actorIdentityResolved = true;
                            partyMember.battleState = trainerBattleState;
                        }
                        m_context.trainerPartySpeciesResolved = allSpeciesResolved;
                        m_context.trainerPartyConstructorResolved =
                            allSpeciesResolved && allConstructorsResolved;
                        m_context.trainerPartyLevelMovesResolved =
                            m_context.trainerPartyConstructorResolved && allLevelMovesResolved;
                        m_context.trainerPartySupercedenceResolved =
                            m_context.trainerPartyLevelMovesResolved && allSupercedenceResolved;
                        m_context.trainerPartyHardMoveFilterResolved =
                            m_context.trainerPartySupercedenceResolved && allHardMoveFiltersResolved;
                        m_context.trainerPartySingleMoveFilterResolved =
                            m_context.trainerPartyHardMoveFilterResolved && allSingleMoveFiltersResolved;
                        m_context.trainerPartyBaseWeightsResolved =
                            m_context.trainerPartySingleMoveFilterResolved && allBaseWeightsResolved;
                        m_context.trainerPartyDamageWeightsResolved =
                            m_context.trainerPartyBaseWeightsResolved && allDamageWeightsResolved;
                        m_context.trainerPartyMovesetsResolved =
                            m_context.trainerPartyDamageWeightsResolved && allMovesetsResolved;
                        m_context.trainerPartyIvsResolved =
                            m_context.trainerPartyMovesetsResolved && allIvsResolved;
                        m_context.trainerPartyBattleStatesResolved =
                            m_context.trainerPartyIvsResolved && allBattleStatesResolved;
                        if (m_context.trainerPartyBattleStatesResolved && levels.count &&
                            m_battleRng.initialize(m_seedCodeUnits.data(), m_seedLength,
                                m_run.wave) && m_battleRng.beginTurn(1)) {
                            m_context.activeTrainerPartyIndex = 0;
                            m_context.enemy = m_context.trainerParty[0];
                            m_run.encounterDex = m_context.enemy.dex;
                            m_encounterResolved = true;
                            m_checkpointAvailable = true;
                            refreshTrainerBaselineMatchups();
                        }
                    }
                }
                m_battleFeedback = m_encounterResolved
                    ? "Trainer active; AI, switching and rewards pending"
                    : m_context.trainerPartyBattleStatesResolved
                    ? "Trainer battle RNG unsupported"
                    : m_context.trainerPartyIvsResolved
                    ? "Trainer battle state unsupported"
                    : m_context.trainerPartyMovesetsResolved
                    ? "Trainer IV draws unsupported"
                    : m_context.trainerPartyDamageWeightsResolved
                    ? "Trainer move selection unsupported"
                    : m_context.trainerPartyBaseWeightsResolved
                    ? "Trainer damage weights unsupported"
                    : m_context.trainerPartySingleMoveFilterResolved
                    ? "Trainer base move weights unsupported"
                    : m_context.trainerPartyHardMoveFilterResolved
                    ? "Trainer single-battle move filter unsupported"
                    : m_context.trainerPartyLevelMovesResolved
                    ? "Trainer move-filter metadata unsupported"
                    : m_context.trainerPartyConstructorResolved
                    ? "Trainer level-move metadata unsupported"
                    : m_context.trainerPartySpeciesResolved
                    ? "Trainer constructor metadata unsupported"
                    : m_context.trainerPartyCount
                    ? "Canonical trainer species could not resolve"
                    : "Canonical trainer party template could not resolve";
        } else {
            m_battleFeedback = "Fixed trainer selection callback is not ported yet";
        }
        buildScene();
        return;
    } else if (waveKind != ClassicWaveKind::RegularWild &&
               waveKind != ClassicWaveKind::TrainerChanceRequired &&
               waveKind != ClassicWaveKind::MajorBoss &&
               waveKind != ClassicWaveKind::FinalBoss) {
        m_battleFeedback = waveKind == ClassicWaveKind::Invalid
            ? "Invalid Classic wave" : "Classic fixed trainer is not ported yet";
        buildScene();
        return;
    }
    m_doubleBattle = (waveKind == ClassicWaveKind::RegularWild ||
        waveKind == ClassicWaveKind::TrainerChanceRequired) && (waveRng.randSeedInt(8) == 0);
    // newBattle has already reset the global seed to rootSeed + waveIndex;
    // Battle construction then applies waveIndex<<3 to that wave seed. The
    // constructor's seeded 16-character battleSeed consumes the first draws
    // before getLevelForWave() consumes its Gaussian samples from that stream.
    PokerogueRngAdapter levelRng;
    uint16_t waveSeed[PokerogueRngAdapter::kMaxSeedCodeUnits]{};
    if (!PokerogueRngAdapter::shiftCharCodes(m_seedCodeUnits.data(), m_seedLength,
            m_run.wave, waveSeed, PokerogueRngAdapter::kMaxSeedCodeUnits)) return;
    if (!m_battleRng.initialize(m_seedCodeUnits.data(), m_seedLength, m_run.wave) ||
        !m_battleRng.beginTurn(1)) return;
    PokerogueSeedOffsetScope levelScope(levelRng, waveSeed, m_seedLength,
                                         static_cast<uint32_t>(m_run.wave) << 3);
    if (!levelScope.valid()) return;
    for (uint8_t i = 0; i < 16; ++i) (void)levelRng.randSeedInt(62);
    const uint16_t level = (waveKind == ClassicWaveKind::FinalBoss) ? 200 :
        (waveKind == ClassicWaveKind::MajorBoss) ? PokerogueEncounterResolver::bossLevelForWave(m_run.wave, levelRng) :
        PokerogueEncounterResolver::nonBossLevelForWave(m_run.wave, levelRng);

    const char* resolvedSpeciesId = nullptr;
    if (waveKind == ClassicWaveKind::FinalBoss) {
        m_run.biomeId = "end";
        resolvedSpeciesId = "eternatus";
    } else if (waveKind == ClassicWaveKind::MajorBoss) {
        const auto pool = PokerogueEncounterResolver::resolveBoss(
            m_run.biomeId, time, m_run.wave, waveRng);
        if (!pool.valid || !pool.speciesId) return;
        resolvedSpeciesId = PokerogueEncounterResolver::resolveWildSpeciesForLevel(
            pool.speciesId, level, true, waveRng);
    } else {
        const auto pool = PokerogueEncounterResolver::resolveNonBoss(
            m_run.biomeId, time, m_run.wave, waveRng);
        if (!pool.valid || !pool.speciesId) return;
        resolvedSpeciesId = PokerogueEncounterResolver::resolveWildSpeciesForLevel(
            pool.speciesId, level, true, waveRng);
    }
    if (!resolvedSpeciesId) return;

    const auto findSpeciesIndex = [](const char* speciesId) {
        for (std::size_t i = 0; i < PokerogueContent::kSpeciesCount; ++i) {
            if (std::string(PokerogueContent::kSpecies[i].id) == speciesId) return i;
        }
        return PokerogueContent::kSpeciesCount;
    };
    const std::size_t enemyIndex = findSpeciesIndex(resolvedSpeciesId);
    if (enemyIndex == PokerogueContent::kSpeciesCount) return;
    const auto& enemy = PokerogueContent::kSpecies[enemyIndex];

    const auto resolveEnemyActor = [&](const PokerogueContent::Species& species,
                                       ResolvedPokemon& destination) -> bool {
        PokemonFormSelectionContext formContext{};
        formContext.biomeId = m_run.biomeId;
        formContext.timeOfDay = time == PokerogueTimeOfDay::Day ? "DAY"
            : time == PokerogueTimeOfDay::Dusk ? "DUSK"
            : time == PokerogueTimeOfDay::Night ? "NIGHT" : "DAWN";
        formContext.waveIndex = m_run.wave;
        PokemonActorIdentity actor{};
        if (generatePokemonActorForWildEncounter(species.dex, 256, formContext, waveRng, actor) !=
            PokemonActorIdentityResult::Ok) return false;
        PokemonBattleInit battleInput{};
        battleInput.speciesDex = species.dex;
        battleInput.formId = actor.formId;
        battleInput.level = destination.level;
        PokemonBattleState battleState{};
        if (initializePokemonBattleStateForActor(battleInput, actor, battleState) != PokemonBattleInitResult::Ok) return false;

        PokemonLevelMoveCandidate candidates[128]{};
        std::size_t candidateCount = 0;
        if (buildPokemonLevelMovePool(species.dex, actor.formId, destination.level,
                candidates, 128, candidateCount) != PokemonLevelMovePoolResult::Ok) return false;
        PokemonWildMoveRuntimeMetadata metadata[128]{};
        for (std::size_t i = 0; i < candidateCount; ++i) {
            if (!buildPokemonWildMoveRuntimeMetadata(candidates[i].moveId, battleState.abilityId, metadata[i])) return false;
        }
        const auto* form = actor.formId ? PokerogueContent::findFormById(actor.formId) : nullptr;
        const char* type1 = form ? form->type1 : species.type1;
        const char* type2 = form ? form->type2 : species.type2;
        uint16_t moveIds[4]{};
        uint8_t moveCount = 0;
        if (generateWildMovesetFromLearnset(species.dex, actor.formId, destination.level,
                battleState.stats[1], battleState.stats[3], battleState.stats[2], type1, type2,
                metadata, candidateCount, waveRng, moveIds, moveCount) != PokemonWildMovesetResult::Ok) return false;
        battleInput.moveCount = moveCount;
        for (uint8_t i = 0; i < moveCount; ++i) battleInput.moveIds[i] = moveIds[i];
        PokemonBattleState battleReadyState{};
        if (initializePokemonBattleStateForActor(battleInput, actor, battleReadyState) != PokemonBattleInitResult::Ok) return false;
        destination.actor = actor;
        destination.actorIdentityResolved = true;
        destination.formId = actor.formId;
        destination.battleState = battleReadyState;
        destination.moveCount = moveCount;
        destination.movesetResolved = true;
        for (uint8_t i = 0; i < moveCount; ++i) destination.moveIds[i] = moveIds[i];
        return true;
    };

    m_run.encounterDex = enemy.dex;
    const std::string enemyLocaleId = std::string("pokemon:") + enemy.id;
    m_context.enemy = {enemy.dex, level, enemy.id, locale(enemyLocaleId.c_str(), enemy.name), enemy.firstFormId, enemy.assetSourcePath};
    if (!resolveEnemyActor(enemy, m_context.enemy)) return;
    const bool finalBoss = waveKind == ClassicWaveKind::FinalBoss;
    if (!initializeClassicPokemonBossState(enemy.dex, level, m_run.wave, finalBoss,
            finalBoss, m_context.enemy.bossState)) return;
    if (m_doubleBattle) {
        // EncounterPhase resolves actors sequentially. The first actor and
        // its regular-wild moveset must consume their draws before slot two
        // resolves from the shared encounter stream.
        const uint16_t secondLevel = PokerogueEncounterResolver::nonBossLevelForWave(
            m_run.wave, levelRng);
        const auto secondPool = PokerogueEncounterResolver::resolveNonBoss(
            m_run.biomeId, time, m_run.wave, waveRng);
        if (!secondPool.valid || !secondPool.speciesId) return;
        const char* secondSpeciesId = PokerogueEncounterResolver::resolveWildSpeciesForLevel(
            secondPool.speciesId, secondLevel, true, waveRng);
        if (!secondSpeciesId) return;
        const std::size_t secondIndex = findSpeciesIndex(secondSpeciesId);
        if (secondIndex == PokerogueContent::kSpeciesCount) return;
        const auto& secondSpecies = PokerogueContent::kSpecies[secondIndex];
        const std::string secondLocaleId = std::string("pokemon:") + secondSpecies.id;
        m_context.secondEnemy = {secondSpecies.dex, secondLevel, secondSpecies.id,
            locale(secondLocaleId.c_str(), secondSpecies.name), secondSpecies.firstFormId,
            secondSpecies.assetSourcePath};
        if (!resolveEnemyActor(secondSpecies, m_context.secondEnemy)) return;
        if (!initializeClassicPokemonBossState(secondSpecies.dex, secondLevel, m_run.wave, false,
                false, m_context.secondEnemy.bossState)) return;
        if (m_context.enemy.bossState.segmentCount && m_context.secondEnemy.bossState.segmentCount) {
            const auto* firstForm = PokerogueContent::findFormById(m_context.enemy.formId);
            const auto* secondForm = PokerogueContent::findFormById(m_context.secondEnemy.formId);
            if (!firstForm || !secondForm) return;
            const uint16_t firstTotal = firstForm->hp + firstForm->atk + firstForm->def +
                firstForm->spatk + firstForm->spdef + firstForm->speed;
            const uint16_t secondTotal = secondForm->hp + secondForm->atk + secondForm->def +
                secondForm->spatk + secondForm->spdef + secondForm->speed;
            if (!distributePokemonDoubleBossSegments(m_context.enemy.bossState, firstTotal,
                    m_context.secondEnemy.bossState, secondTotal)) return;
        }
        m_secondEncounterResolved = true;
    }
    if (m_run.wave == 1 || (m_run.wave > 1 && (m_run.wave - 1) % 10 == 0)) {
        PokemonEffectiveWeather initialWeather{};
        if (!selectPokemonBiomeWeather(m_run.biomeId,
                time == PokerogueTimeOfDay::Dusk || time == PokerogueTimeOfDay::Night,
                waveRng, initialWeather)) return;
        if (initialWeather != PokemonEffectiveWeather::None) {
            const bool weatherSupported = static_cast<uint8_t>(initialWeather) <= static_cast<uint8_t>(PokemonEffectiveWeather::Snow) &&
                !m_doubleBattle && m_context.player.actorIdentityResolved &&
                m_context.enemy.actorIdentityResolved &&
                pokemonWeatherLifecycleSupported(m_context.player.battleState.abilityId) &&
                pokemonWeatherLifecycleSupported(m_context.enemy.battleState.abilityId);
            if (!weatherSupported) return;
            m_arenaWeather.type = initialWeather;
            m_arenaWeather.turnsLeft = 5;
            m_arenaWeather.maxDuration = 5;
        } else {
            m_arenaWeather = {};
        }
    }
    m_encounterResolved = true;
    m_checkpointAvailable = true;
}

void FirstRunRuntime::buildScene() {
    char line[96];
    std::snprintf(line, sizeof(line), "POKEROGUE 3DS - CLASSIC %u",
                  static_cast<unsigned>(m_run.wave));
    m_text[0] = line;
    m_text[1] = std::string("Mode: ") + m_context.modeName;
    m_text[2] = std::string("Biome: ") + m_context.biomeName;
    m_text[3] = std::string("Starter: ") + starterName();
    if (m_context.trainerName) {
        if (m_encounterResolved)
            std::snprintf(line, sizeof(line), "Trainer: %s | %s Lv.%u",
                m_context.trainerName, m_context.enemy.localizedName,
                static_cast<unsigned>(m_context.enemy.level));
        else
            std::snprintf(line, sizeof(line), "Trainer class: %s", m_context.trainerName);
    } else if (m_doubleBattle && m_encounterResolved && m_secondEncounterResolved) {
        std::snprintf(line, sizeof(line), "%s%s Lv.%u  %s%s Lv.%u",
            m_selectedTarget == 0 ? ">" : " ",
            m_context.enemy.localizedName, static_cast<unsigned>(m_context.enemy.level),
            m_selectedTarget == 1 ? ">" : " ",
            m_context.secondEnemy.localizedName, static_cast<unsigned>(m_context.secondEnemy.level));
    } else if (m_encounterResolved) {
        std::snprintf(line, sizeof(line), "Encounter: %s Lv. %u", m_context.enemy.localizedName,
                      static_cast<unsigned>(m_context.enemy.level));
    } else {
        std::snprintf(line, sizeof(line), "Wave %u encounter: UNSUPPORTED",
                      static_cast<unsigned>(m_run.wave));
    }
    m_text[4] = line;
    m_text[5] = m_victoryPlan.contains(ClassicVictoryStep::GameClear)
        ? "Game Clear! X: save  Y: export"
        : m_rewardsPending
        ? "UP/DOWN: select reward  A: claim  B: skip"
        : (m_trainerBattle && !trainerBattleSupported())
        ? "Trainer battle pending"
        : m_battleFinished
        ? (m_playerWon && !m_experienceGranted ? "A: collect experience" :
            m_playerWon ? (m_trainerBattle && !enemyPartyDefeated() ? "A: next opponent" : "A: select reward  B: skip") : "X: save  Y: export")
        : (!m_encounterResolved && m_runStarted ? "Encounter integration pending" :
            m_doubleBattle ? "A: fight  UP/DOWN: move  L/R: target" :
            m_runStarted ? "A: fight  UP/DOWN: move" :
            "A: fight  UP/DOWN: move  LEFT/RIGHT: starter");
    if (m_context.trainerPartySpeciesResolved) {
        if (m_context.trainerPartyCount > 1) {
            std::snprintf(line, sizeof(line), "Team: %s Lv.%u / %s Lv.%u",
                m_context.trainerParty[0].localizedName,
                static_cast<unsigned>(m_context.trainerParty[0].level),
                m_context.trainerParty[1].localizedName,
                static_cast<unsigned>(m_context.trainerParty[1].level));
        } else {
            std::snprintf(line, sizeof(line), "Team: %s Lv.%u",
                m_context.trainerParty[0].localizedName,
                static_cast<unsigned>(m_context.trainerParty[0].level));
        }
        m_text[6] = line;
    } else if (m_context.trainerPartyCount) {
        std::snprintf(line, sizeof(line), "Party: %s | %u members, first Lv. %u",
            m_context.trainerPartyTemplateKey,
            static_cast<unsigned>(m_context.trainerPartyCount),
            static_cast<unsigned>(m_context.trainerPartyLevels[0]));
        m_text[6] = line;
    } else if (m_doubleBattle && m_encounterResolved && m_secondEncounterResolved &&
               m_context.player.actorIdentityResolved &&
               m_context.enemy.actorIdentityResolved &&
               m_context.secondEnemy.actorIdentityResolved) {
        std::snprintf(line, sizeof(line), "HP: %u/%u vs %u/%u, %u/%u",
            static_cast<unsigned>(m_context.player.battleState.hp),
            static_cast<unsigned>(m_context.player.battleState.maxHp),
            static_cast<unsigned>(m_context.enemy.battleState.hp),
            static_cast<unsigned>(m_context.enemy.battleState.maxHp),
            static_cast<unsigned>(m_context.secondEnemy.battleState.hp),
            static_cast<unsigned>(m_context.secondEnemy.battleState.maxHp));
        m_text[6] = line;
        if (m_battleFinished) m_text[6] = m_playerWon
            ? (m_victoryPlan.contains(ClassicVictoryStep::GameClear)
                ? "Game Clear! Classic run completed"
                : (m_experienceGranted ? (m_rewardsPending ? "Select reward" : "A: select reward  B: skip") : "A: grant EXP"))
            : "Starter fainted - run end pending";
    } else if (m_encounterResolved && m_context.player.actorIdentityResolved &&
               m_context.enemy.actorIdentityResolved) {
        std::snprintf(line, sizeof(line), "HP: %u/%u vs %u/%u",
            static_cast<unsigned>(m_context.player.battleState.hp),
            static_cast<unsigned>(m_context.player.battleState.maxHp),
            static_cast<unsigned>(m_context.enemy.battleState.hp),
            static_cast<unsigned>(m_context.enemy.battleState.maxHp));
        m_text[6] = line;
        if (m_battleFinished) m_text[6] = m_playerWon
            ? (m_victoryPlan.contains(ClassicVictoryStep::GameClear)
                ? "Game Clear! Classic run completed"
                : (m_trainerBattle && !enemyPartyDefeated()
                    ? (m_experienceGranted ? "A: next opponent" : "A: grant EXP")
                    : (m_experienceGranted ? (m_rewardsPending ? "Select reward" : "A: select reward  B: skip") : "A: grant EXP")))
            : "Starter fainted - run end pending";
    } else if (m_encounterResolved) {
        m_text[6] = "Wild evolution rules applied";
    } else {
        m_text[6] = "Encounter inputs unsupported - no fallback used";
    }
    for (std::size_t i = 8; i < 12; ++i) m_text[i].clear();
    if (m_rewardsPending) {
        for (uint8_t i = 0; i < m_rewardChoiceCount && i < 3; ++i) {
            const auto& roll = m_rewardChoices[i];
            const char* itemId = roll.poolEntry && roll.poolEntry->itemId ? roll.poolEntry->itemId : "ITEM";
            const char* tier = roll.poolEntry && roll.poolEntry->tier ? roll.poolEntry->tier : "COMMON";
            const auto* item = findItemById(itemId);
            const std::string itemLocaleId = std::string("modifier-type:") + itemId;
            const char* localizedItem = locale(itemLocaleId.c_str(), item ? item->name : itemId);
            m_text[8 + i] = std::string(i == m_selectedRewardChoice ? "> " : "  ") +
                localizedItem + " [" + tier + "]";
        }
        m_text[11] = "  B: Skip reward";
    } else if (m_context.player.movesetResolved) {
        for (uint8_t i = 0; i < m_context.player.moveCount && i < 4; ++i) {
            const auto* move = PokerogueContent::findMoveById(m_context.player.moveIds[i]);
            if (!move) {
                m_text[8] = "Starter moves unavailable: invalid canonical reference";
                break;
            }
            const std::string localeId = std::string("move:") + move->key;
            m_text[8 + i] = std::string(i == m_selectedBattleMove ? "> Move " : "  Move ") +
                static_cast<char>('1' + i) + ": " +
                locale(localeId.c_str(), move->name);
        }
    } else {
        m_text[8] = "Starter moves unavailable: canonical data error";
    }
    if (!m_battleFeedback.empty()) m_text[7] = m_battleFeedback;
    m_text[12] = std::string("Pinned data: ") + PokerogueContent::kPokerogueRevision;

    static const char* ids[] = {"run-title", "mode", "biome", "starter", "encounter", "controls", "status", "storage-feedback", "starter-move-1", "starter-move-2", "starter-move-3", "starter-move-4", "source"};
    static const float ys[] = {22.0f, 65.0f, 89.0f, 122.0f, 156.0f, 196.0f, 216.0f, 216.0f, 104.0f, 126.0f, 148.0f, 170.0f, 2.0f};
    for (std::size_t i = 0; i < m_nodes.size(); ++i) {
        m_nodes[i] = textNode(ids[i], i >= 7 ? Citro2D::ScreenTarget::Bottom : Citro2D::ScreenTarget::Top,
                              12.0f, ys[i], static_cast<int16_t>(i));
        m_nodes[i].text = m_text[i].c_str();
    }
    m_scene = {"pokerogue-first-run", 1, 60, 0xFF18202Cu, 0xFF10151Eu,
               static_cast<uint16_t>(m_nodes.size()), m_nodes.data(),
               0, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, nullptr};
}

} // namespace Pokerogue3DS
