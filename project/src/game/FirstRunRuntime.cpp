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
#include "game/PokemonFreshProfile.hpp"
#include "game/PokemonExperience.hpp"
#include "game/PokemonStarterMoveset.hpp"
#include "game/PokemonWildMovesetGenerator.hpp"
#include "game/PokerogueTrainerPartyLevels.hpp"
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
    m_playerExperience = 0;
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
    value.playerExperience = m_playerExperience;
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
    // Arena weather save data is supported by the codec. This baseline cannot
    // yet execute its ability/field effects; never load it as neutral weather.
    if (save.weatherType || save.weatherTurnsLeft || save.weatherMaxDuration) return false;
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
        if (!m_encounterResolved || m_doubleBattle) return false;
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
        save.playerExperience != m_playerExperience) return false;
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
    if (m_playerWon && !planClassicVictory(m_run.wave, m_victoryPlan)) return false;
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
bool supportsSelfStatStageMove(uint16_t moveId) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || !move->target ||
        std::strcmp(move->target, "USER") || move->attributeCount != 1 ||
        !PokerogueContent::moveHasAttribute(*move, "StatStageChangeAttr")) return false;
    unsigned count = 0;
    for (const auto& effect : PokerogueContent::kMoveStatStageEffects)
        if (effect.moveId == moveId && effect.selfTarget) ++count;
    return count == 1;
}
bool supportsBaselineBattleMove(uint16_t moveId) {
    if (supportsPokemonTrickRoomMove(moveId) || supportsSelfStatStageMove(moveId) || selfHealingProfile(moveId) || damageDrainProfile(moveId) || damageRecoilProfile(moveId)) return true;
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

bool FirstRunRuntime::resolveActiveMoveWeather(bool enemyAttacks,
    PokemonMoveWeatherContext& output) const {
    // Fresh-profile regular single encounters have no active passives or
    // ability-changing effects. Those effects must extend field resolution.
    if (m_doubleBattle || !m_context.player.actorIdentityResolved ||
        !m_context.enemy.actorIdentityResolved) return false;
    const PokemonWeatherAbilityComponent components[2] = {
        {m_context.player.battleState.abilityId, true, !enemyAttacks},
        {m_context.enemy.battleState.abilityId, true, enemyAttacks}
    };
    PokemonWeatherResolutionPolicy policy{};
    return composePokemonWeatherResolutionPolicy(components, 2, policy) &&
        resolvePokemonMoveWeatherContext(m_arenaWeather, policy, output);
}

bool FirstRunRuntime::resolveActiveMoveCritical(bool enemyAttacks,
    PokemonCriticalPolicy& output) const {
    // Current fresh-profile singles have no unlocked passive, crit items,
    // crit tags, suppression or ability-bypass effects. Their future presence
    // must extend this eligibility/resolution boundary before executing moves.
    if (m_doubleBattle || !m_context.player.actorIdentityResolved ||
        !m_context.enemy.actorIdentityResolved) return false;
    const PokemonCriticalAbilityComponent components[2] = {
        {m_context.player.battleState.abilityId, true, !enemyAttacks},
        {m_context.enemy.battleState.abilityId, true, enemyAttacks}
    };
    return composePokemonCriticalAbilityPolicy(components, 2, false, output);
}

bool FirstRunRuntime::battleInputSupported() const {
    if (!m_encounterResolved || m_trainerBattle || !m_context.player.actorIdentityResolved ||
        !m_context.enemy.actorIdentityResolved || m_doubleBattle || m_battleFinished ||
        !m_context.enemy.battleState.moveCount || m_context.enemy.battleState.moveCount > 4) return false;
    uint8_t enemyUsable = 0;
    for (uint8_t i = 0; i < m_context.enemy.battleState.moveCount; ++i) {
        const auto& move = m_context.enemy.battleState.moves[i];
        if (!move.pp) continue;
        if (!supportsBaselineBattleMove(move.moveId) ||
            (damageDrainProfile(move.moveId) && hasCanonicalReverseDrain(m_context.player.battleState.abilityId))) return false;
        ++enemyUsable;
    }
    return enemyUsable && m_selectedBattleMove < m_context.player.battleState.moveCount &&
        m_context.player.battleState.moves[m_selectedBattleMove].pp &&
        supportsBaselineBattleMove(m_context.player.battleState.moves[m_selectedBattleMove].moveId) &&
        !(damageDrainProfile(m_context.player.battleState.moves[m_selectedBattleMove].moveId) &&
          hasCanonicalReverseDrain(m_context.enemy.battleState.abilityId));
}

bool FirstRunRuntime::selectBattleMove(int direction) {
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

bool FirstRunRuntime::grantVictoryExperience() {
    if (m_experienceGranted || !m_context.player.actorIdentityResolved ||
        !m_context.enemy.actorIdentityResolved || m_doubleBattle) return false;
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
    PokemonExperienceProgress progress{};
    if (applyPokemonExperience(starter->growthRate, m_context.player.level,
                               m_playerExperience, awardedExperience,
                               classicExperienceLevelCap(m_run.wave), progress)
        != PokemonExperienceResult::Ok || progress.level > 100) return false;

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
    }
    m_context.player.battleState = next;
    m_context.player.level = progress.level;
    m_playerExperience = progress.totalExperience;
    m_experienceGranted = true;
    return true;
}

bool FirstRunRuntime::advanceBattleTurn() {
    if (m_battleFinished && m_playerWon && (!m_experienceGranted || m_trainerBattle)) {
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
    if (m_battleFinished && m_playerWon && m_experienceGranted) {
        m_battleFeedback = "Item reward selection is not ported yet";
        buildScene();
        return false;
    }
    if (!battleInputSupported()) {
        m_battleFeedback = m_trainerBattle ? "Trainer AI and switching unsupported" :
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
        // Restricted to the complete fixed two-member party. The current plain
        // battle gate represents no queue/trap/hazard state; broader effects
        // must resolve those inputs before trainer eligibility is enabled.
        if (m_context.trainerPartyCount != 2 ||
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

    // Pinned EnemyPokemon.SMART_RANDOM: score each usable move in moveset order,
    // then advance through the descending pool while randBattleSeedInt(8) >= 5.
    PokemonMoveWeatherContext simulatedWeather{};
    if (!resolveActiveMoveWeather(true, simulatedWeather)) return false;
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
        (void)rng->randSeedInt(1);
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
    uint8_t enemyMoveSlot = 0;
    if (m_trainerBattle) {
        // EnemyPokemon constructor selects SMART whenever hasTrainer() is true.
        if (!selectSmartTrainerMoveSlot(scores, candidates, candidateCount, *rng,
                enemyMoveSlot)) {
            m_battleFeedback = "Trainer SMART move selection failed";
            buildScene();
            return false;
        }
    } else {
        uint8_t chosenIndex = 0;
        while (chosenIndex + 1 < candidateCount && rng->randSeedInt(8) >= 5) ++chosenIndex;
        enemyMoveSlot = candidates[chosenIndex];
    }
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

bool FirstRunRuntime::executeActiveBattleMove(bool enemyActs, uint8_t moveSlot,
    PokerogueRngAdapter& rng) {
    auto& user = enemyActs ? m_context.enemy.battleState : m_context.player.battleState;
    auto& opponent = enemyActs ? m_context.player.battleState : m_context.enemy.battleState;
    if (!user.hp || !opponent.hp) return true;
    if (moveSlot >= user.moveCount || moveSlot >= 4) return false;
    const auto* move = PokerogueContent::findMoveById(user.moves[moveSlot].moveId);
    if (!move || !supportsBaselineBattleMove(move->id)) return false;
    if (selfHealingProfile(move->id)) {
        PokemonHealingPolicy policy{};
        // Current fresh single actors have no held items, passives or Heal Block.
        // Constant USER moves here have no PULSE flag, so Mega Launcher cannot boost them.
        policy.resolved = true;
        PokemonHealingEvent event{};
        if (usePokemonSelfHealingCommand(user, moveSlot, policy, event) != PokemonHealingResult::Ok)
            return false;
        m_battleFeedback = event.healed ? "HP restored" : "HP unchanged";
        return true;
    }
    PokemonPpPolicy pp{};
    pp.resolved = true;
    if (!pokemonSingleOpponentPpCost(opponent.abilityId, pp.cost)) return false;
    if (supportsSelfStatStageMove(move->id)) {
        PokemonStatStageCommandPolicy policy{};
        policy.move.hitPolicyResolved = true; // USER bypasses hit checks upstream.
        policy.move.ppCost = 1; // USER targets no opponent: Pressure cannot apply.
        policy.move.stagePolicy.resolved = true;
        policy.postChangePoliciesResolved = true;
        const auto* ownProfile = PokerogueContent::findAbilityStatStageProfile(user.abilityId);
        const auto* observerProfile = PokerogueContent::findAbilityStatStageProfile(opponent.abilityId);
        const ResolvedStatStageAbilityComponent own[] = {{ownProfile, ownProfile != nullptr}};
        const ResolvedStatStageAbilityComponent observer[] = {{observerProfile, observerProfile != nullptr}};
        const PokerogueContent::MoveStatStageEffect* effect = nullptr;
        for (const auto& row : PokerogueContent::kMoveStatStageEffects)
            if (row.moveId == move->id) effect = &row;
        if (!effect || !composePokemonStatStageAbilityPolicy(*effect, own, 1, false,
                policy.move.stagePolicy)) return false;
        // Reaction phases apply their own stat multiplier; protection does not
        // block self-originated changes. No items/passives/tags in fresh actors.
        const PokerogueContent::MoveStatStageEffect reaction{move->id, 127, 1, true};
        policy.recipientReaction.resolved = policy.sourceReaction.resolved =
            policy.reflection.resolved = policy.opponentCopy.resolved = true;
        if (!composePokemonStatStageAbilityPolicy(reaction, own, 1, false, policy.recipientReaction) ||
            !composePokemonStatStageAbilityPolicy(reaction, observer, 1, false, policy.opponentCopy)) return false;
        policy.opponentCopyProfile = observerProfile;
        uint8_t count = 0;
        for (const auto& row : PokerogueContent::kAbilityStatStageReactions)
            if (row.abilityId == user.abilityId && count < 2) policy.recipientReactions[count++] = &row;
        PokemonStatStageCommandEvent event{};
        if (usePokemonStatStageStatusCommand(user, opponent, moveSlot, policy, rng, event) !=
                PokemonStatStageEffectResult::Ok) return false;
        m_battleFeedback = event.move.stages.changedStatMask ? "Stats changed" : "Stats unchanged";
        return true;
    }
    if (supportsPokemonTrickRoomMove(move->id)) {
        PokemonTrickRoomCommandPolicy policy{};
        // Fresh actors have no status, held items, passives or move-blocking tags.
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
        {m_context.player.battleState.abilityId, true, !enemyActs},
        {m_context.enemy.battleState.abilityId, true, enemyActs}
    };
    if (!resolveActiveMoveWeather(enemyActs, weather) ||
        !composePokemonAlwaysHitPolicy(activeAbilities, 2, hit, move->id, &weather) ||
        !resolveActiveMoveCritical(enemyActs, critical)) return false;
    if (damageRecoilProfile(move->id)) {
        auto nextUser = user;
        auto nextOpponent = opponent;
        auto nextRng = rng;
        PokemonMoveActionResult attack{};
        if (useStandardPokemonMove(nextUser, nextOpponent, moveSlot, false, nextRng,
            attack, &weather, &critical, &hit, &pp) != PokemonMoveActionStatus::Ok) return false;
        const auto policy = canonicalFreshActorRecoilPolicy(user.abilityId);
        PokemonRecoilEvent recoil{};
        if (applyPokemonRecoil(nextUser, move->id, attack.damageApplied,
            attack.damageRoll.hit && !attack.weatherCancelled, policy, recoil) != PokemonRecoilResult::Ok)
            return false;
        user = nextUser;
        opponent = nextOpponent;
        rng = nextRng;
        m_battleFeedback = recoil.damage ? "Attack caused recoil" :
            attack.weatherCancelled ? "Move blocked by weather" :
            attack.damageRoll.hit ? "Attack hit" : "Attack missed";
        return true;
    }
    if (damageDrainProfile(move->id)) {
        // Liquid Ooze requires post-defend indirect-damage policies not yet in the run.
        if (hasCanonicalReverseDrain(opponent.abilityId)) return false;
        auto nextUser = user;
        auto nextOpponent = opponent;
        auto nextRng = rng;
        PokemonMoveActionResult attack{};
        if (useStandardPokemonMove(nextUser, nextOpponent, moveSlot, false, nextRng,
            attack, &weather, &critical, &hit, &pp) != PokemonMoveActionStatus::Ok) return false;
        PokemonDrainPolicy policy{};
        policy.resolved = true; // Current run has no Healing Charm, passives or Heal Block.
        PokemonDrainEvent event{};
        if (attack.damageRoll.hit && !attack.weatherCancelled && attack.damageApplied &&
            applyPokemonDamageDrain(nextUser, move->id, attack.damageApplied, policy, event) !=
                PokemonHealingResult::Ok) return false;
        user = nextUser;
        opponent = nextOpponent;
        rng = nextRng;
        m_battleFeedback = event.healed ? "Attack drained HP" :
            attack.weatherCancelled ? "Move blocked by weather" :
            attack.damageRoll.hit ? "Attack hit" : "Attack missed";
        return true;
    }
    PokemonMoveActionResult result{};
    if (useStandardPokemonMove(user, opponent, moveSlot, false, rng, result,
            &weather, &critical, &hit, &pp) != PokemonMoveActionStatus::Ok) return false;
    m_battleFeedback = result.weatherCancelled
        ? (enemyActs ? "Enemy move blocked by weather" : "Your move blocked by weather")
        : result.damageRoll.hit ? (enemyActs ? "Enemy move hit" : "Your move hit")
                               : (enemyActs ? "Enemy move missed" : "Your move missed");
    return true;
}

bool FirstRunRuntime::finishBattleTurn() {
    // TurnEndPhase lapses arena tags except during a biome interlude.
    // Current checkpoint progression ends before the first X0 transition.
    PokemonTrickRoomEvent roomEvent{};
    if (!advancePokemonTrickRoomTurnEnd(m_trickRoom, roomEvent)) {
        m_battleFeedback = "Invalid Trick Room field state";
        buildScene();
        return false;
    }
    if (!m_context.enemy.battleState.hp || !m_context.player.battleState.hp) {
        m_battleFinished = true;
        m_playerWon = m_context.enemy.battleState.hp == 0 && m_context.player.battleState.hp != 0;
        m_victoryPlan = {};
        if (m_playerWon && !planClassicVictory(m_run.wave, m_victoryPlan)) {
            m_battleFeedback = "Classic victory plan unavailable";
            buildScene();
            return false;
        }
        m_battleFeedback = m_playerWon ? "Enemy defeated - experience pending" : "Pokemon fainted - run end pending";
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

bool FirstRunRuntime::skipVictoryReward() {
    if (!m_battleFinished || !m_playerWon || !m_experienceGranted ||
        !m_victoryPlan.contains(ClassicVictoryStep::SelectModifier) ||
        !m_victoryPlan.nextWave || m_victoryPlan.nextWave > PokerogueContent::kClassicFinalWave)
        return false;
    // SelectModifierPhase permits skipping its choice. The one-starter run
    // has no reward modifier to carry into the next wave in this branch.
    m_run.wave = m_victoryPlan.nextWave;
    resolve(true);
    if (!m_encounterResolved || m_doubleBattle) m_checkpointAvailable = false;
    if (m_encounterResolved && !m_doubleBattle && !m_trainerBattle)
        m_battleFeedback = "Reward skipped - next Classic wave";
    buildScene();
    return m_encounterResolved && !m_doubleBattle;
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

void FirstRunRuntime::resolve(bool carryPlayer) {
    m_selectedBattleMove = 0;
    m_turn = 1;
    m_enemySwitchCounter = 0;
    m_battleFinished = false;
    m_playerWon = false;
    m_experienceGranted = false;
    m_victoryPlan = {};
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
    const std::string biomeLocaleId = std::string("biomes:") + PokerogueContent::kStartingBiomeId;
    m_context.biomeName = locale(biomeLocaleId.c_str(), "Town");
    const std::string starterLocaleId = std::string("pokemon:") + starter.id;
    if (!carryPlayer) {
        m_context.player = {starter.dex, 5, starter.id, locale(starterLocaleId.c_str(), starter.name), starter.firstFormId, starter.assetSourcePath};
        if (pokemonTotalExperienceForLevel(starter.growthRate, 5, m_playerExperience)
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
    if (waveKind == ClassicWaveKind::TrainerChanceRequired) {
        bool offsetGym = false;
        bool isTrainer = false;
        if (!PokerogueWaveClock::deriveOffsetGym(m_seedCodeUnits.data(), m_seedLength, offsetGym) ||
            resolveClassicTrainerChance(m_run.wave, m_run.biomeId, offsetGym,
                m_seedCodeUnits.data(), m_seedLength, waveRng, isTrainer) ==
                ClassicTrainerDecision::Invalid) return;
        if (isTrainer) {
            m_trainerBattle = true;
            m_trickRoom = {}; // BattleScene.doPostBattleCleanup resets effects for trainers.
            const auto selectedTrainer = PokerogueEncounterResolver::resolveTrainerType(
                m_run.biomeId, false, false, waveRng);
            if (!selectedTrainer.valid || !selectedTrainer.trainerType) {
                m_battleFeedback = "Canonical trainer pool could not resolve";
                buildScene();
                return;
            }
            m_context.trainerTypeId = selectedTrainer.trainerType->id;
            m_context.trainerName = selectedTrainer.trainerType->name;
            m_battleFeedback = "Trainer party construction pending";
            buildScene();
            return;
        }
    } else if (waveKind == ClassicWaveKind::FixedTrainerBattle) {
        m_trainerBattle = true;
        m_trickRoom = {}; // BattleScene.doPostBattleCleanup resets effects for trainers.
        const auto* fixedBattle = PokerogueContent::findClassicFixedBattleWave(m_run.wave);
        const auto* trainer = fixedBattle && fixedBattle->hasStaticTrainerType
            ? PokerogueContent::findTrainerType(fixedBattle->trainerTypeId) : nullptr;
        if (trainer) {
            m_context.trainerTypeId = trainer->id;
            m_context.trainerName = trainer->name;
            if (fixedBattle->seededBinaryGenderVariant) {
                // handleFixedBattle invokes getTrainer under rootSeed + (wave << 8).
                // TOWN_YOUNGSTER draws its gender before Trainer.constructor
                // chooses a party template. The callback is source-normalized.
                PokerogueRngAdapter trainerRng;
                PokerogueSeedOffsetScope trainerScope(trainerRng,
                    m_seedCodeUnits.data(), m_seedLength,
                    static_cast<uint32_t>(m_run.wave) << 8);
                if (!trainerScope.valid()) return;
                m_context.trainerFemaleVariant = trainerRng.randSeedInt(2) != 0;
                const auto chosen = selectTrainerPartyTemplate(*trainer, m_run.wave, trainerRng);
                if (chosen.supported && chosen.value) {
                    const auto levels = resolveClassicTrainerPartyLevels(*chosen.value, m_run.wave, false);
                    if (levels.supported) {
                        m_context.trainerPartyTemplateKey = chosen.value->key;
                        m_context.trainerPartyCount = levels.count;
                        for (uint8_t i = 0; i < levels.count; ++i)
                            m_context.trainerPartyLevels[i] = levels.values[i];
                        const PokerogueContent::Species* selectedSpecies[6]{};
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
                            const auto member = resolveSimpleTrainerPoolMember(*trainer,
                                *chosen.value, i, levels.values[i], m_run.wave,
                                selectedSpecies, i, memberRng);
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
                m_battleFeedback = "Fixed trainer variant callback is not ported yet";
            }
        } else {
            m_battleFeedback = "Fixed trainer selection callback is not ported yet";
        }
        buildScene();
        return;
    } else if (waveKind != ClassicWaveKind::RegularWild) {
        m_battleFeedback = waveKind == ClassicWaveKind::Invalid
            ? "Invalid Classic wave" : "Classic fixed trainer or boss encounter is not ported yet";
        buildScene();
        return;
    }
    m_doubleBattle = waveRng.randSeedInt(8) == 0; // Classic default chance, zero party luck.
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
    const uint16_t level = PokerogueEncounterResolver::nonBossLevelForWave(m_run.wave, levelRng);

    // Arena.randomSpecies evaluates the level before selecting the pool and
    // uses it for the post-selection LegendLike/BST gate.
    const auto pool = PokerogueEncounterResolver::resolveNonBoss(
        m_run.biomeId, time, m_run.wave, waveRng);
    if (!pool.valid || !pool.speciesId) return;
    const char* resolvedSpeciesId = PokerogueEncounterResolver::resolveWildSpeciesForLevel(
        pool.speciesId, level, true, waveRng);
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
        m_secondEncounterResolved = true;
    }
    if (m_run.wave == 1) {
        PokemonEffectiveWeather initialWeather{};
        if (!selectPokemonBiomeWeather(m_run.biomeId,
                time == PokerogueTimeOfDay::Dusk || time == PokerogueTimeOfDay::Night,
                waveRng, initialWeather)) return;
        // Current starting biome has only NONE. Non-neutral fields require
        // residual damage, stat modifiers and summon effects before eligibility.
        if (initialWeather != PokemonEffectiveWeather::None) return;
        m_arenaWeather = {};
    }
    m_encounterResolved = true;
    m_checkpointAvailable = !m_doubleBattle;
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
    } else if (m_encounterResolved) {
        std::snprintf(line, sizeof(line), "Encounter: %s Lv. %u", m_context.enemy.localizedName,
                      static_cast<unsigned>(m_context.enemy.level));
    } else {
        std::snprintf(line, sizeof(line), "Wave %u encounter: UNSUPPORTED",
                      static_cast<unsigned>(m_run.wave));
    }
    m_text[4] = line;
    m_text[5] = m_trainerBattle
        ? "Trainer battle pending"
        : m_battleFinished
        ? (m_playerWon && !m_experienceGranted ? "A: collect experience" :
            m_playerWon ? "B: skip reward  X: save" : "X: save  Y: export")
        : (!m_encounterResolved && m_runStarted ? "Encounter integration pending" :
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
    } else if (m_doubleBattle && m_secondEncounterResolved) {
        std::snprintf(line, sizeof(line), "2nd: %s Lv. %u", m_context.secondEnemy.localizedName,
                      static_cast<unsigned>(m_context.secondEnemy.level));
        m_text[6] = line;
    } else if (m_doubleBattle && m_encounterResolved) {
        m_text[6] = "Second slot unsupported - no fallback";
    } else if (m_encounterResolved && m_context.player.actorIdentityResolved &&
               m_context.enemy.actorIdentityResolved) {
        std::snprintf(line, sizeof(line), "HP: %u/%u vs %u/%u",
            static_cast<unsigned>(m_context.player.battleState.hp),
            static_cast<unsigned>(m_context.player.battleState.maxHp),
            static_cast<unsigned>(m_context.enemy.battleState.hp),
            static_cast<unsigned>(m_context.enemy.battleState.maxHp));
        m_text[6] = line;
        if (m_battleFinished) m_text[6] = m_playerWon
            ? (m_experienceGranted ? "Item reward pending" : "A: grant EXP")
            : "Starter fainted - run end pending";
    } else if (m_encounterResolved) {
        m_text[6] = "Wild evolution rules applied";
    } else {
        m_text[6] = "Encounter inputs unsupported - no fallback used";
    }
    for (std::size_t i = 8; i < 12; ++i) m_text[i].clear();
    if (m_context.player.movesetResolved) {
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
