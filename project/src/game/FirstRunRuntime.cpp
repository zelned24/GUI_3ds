#include "game/FirstRunRuntime.hpp"
#include "storage/NativeProgressStore.hpp"
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
#include <memory>
#include <new>

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
        if (starterUnlocked(PokerogueContent::kSpecies[index].dex)) {
            std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
            if (!candidateStorage) {
                m_battleFeedback = "Runtime transaction allocation failed";
                return false;
            }
            auto& candidate = *candidateStorage;
            candidate.m_starterIndex = index;
            candidate.resolve();
            if (!candidate.m_encounterResolved || !candidate.m_context.player.actorIdentityResolved ||
                !candidate.m_context.player.movesetResolved) {
                m_battleFeedback = "Starter setup could not resolve";
                buildScene();
                return false;
            }
            *this = candidate;
            buildScene();
            return true;
        }
    }
    return false;
}

bool FirstRunRuntime::browseSetupStarter(int direction) {
    if (m_runStarted || (direction != -1 && direction != 1)) return false;
    size_t index = 0;
    while (index < PokerogueContent::kSpeciesCount && PokerogueContent::kSpecies[index].dex != selectedSetupStarterDex()) ++index;
    if (index == PokerogueContent::kSpeciesCount) return false;
    for (size_t scanned = 0; scanned < PokerogueContent::kSpeciesCount; ++scanned) {
        index = direction > 0 ? (index + 1) % PokerogueContent::kSpeciesCount :
            (index + PokerogueContent::kSpeciesCount - 1) % PokerogueContent::kSpeciesCount;
        if (starterUnlocked(PokerogueContent::kSpecies[index].dex)) {
            m_setupCursorDex = PokerogueContent::kSpecies[index].dex;
            return true;
        }
    }
    return false;
}

bool FirstRunRuntime::toggleSetupStarter() {
    if (m_runStarted || !m_context.playerPartyCount || m_context.playerPartyCount > 6) return false;
    const uint16_t selected = selectedSetupStarterDex();
    uint16_t dexes[6]{};
    size_t count = 0;
    bool removing = false;
    for (uint8_t i = 0; i < m_context.playerPartyCount; ++i) {
        if (m_context.playerParty[i].dex == selected) removing = true;
        else dexes[count++] = m_context.playerParty[i].dex;
    }
    if (!removing) {
        if (count == 6) return false;
        dexes[count++] = selected;
    }
    if (!count || !restoreStarterTeamSetup(m_run.seed, dexes, count)) return false;
    m_setupCursorDex = selected;
    return true;
}

bool FirstRunRuntime::starterUnlocked(uint16_t dex) const {
    const auto* species = PokerogueContent::findSpeciesByDex(dex);
    return species && species->starterEligible &&
        (species->freshProfileStarter || (m_starterProfileReady && hasCaughtSpecies(dex)));
}

uint8_t FirstRunRuntime::starterCostReduction(uint16_t dex) const {
    if (!m_starterProfileReady) return 0;
    for (size_t i = 0; i < m_starterProfileCount; ++i)
        if (m_starterProfileRecords[i].speciesDex == dex) return m_starterProfileRecords[i].costReduction;
    return 0;
}

bool FirstRunRuntime::starterSelectionAllowed(const uint16_t* dexes, size_t count) const {
    if (!dexes || !count || count > 6) return false;
    uint16_t totalQuarterUnits = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!starterUnlocked(dexes[i])) return false;
        for (size_t prior = 0; prior < i; ++prior) if (dexes[prior] == dexes[i]) return false;
        uint16_t cost = 0;
        if (!pokemonStarterCostQuarterUnits(dexes[i], starterCostReduction(dexes[i]), cost)) return false;
        totalQuarterUnits += cost;
        if (totalQuarterUnits > kClassicStarterValueLimit * 4) return false;
    }
    return true;
}

uint16_t FirstRunRuntime::setupStarterFormIndex(uint16_t dex) const {
    if (m_starterProfileReady) for (size_t i = 0; i < m_starterProfileCount; ++i) {
        const auto& record = m_starterProfileRecords[i];
        if (record.speciesDex == dex && record.preferredFormIndex != 65535) return record.preferredFormIndex;
    }
    return 0;
}

NativeSaveResult FirstRunRuntime::cycleSetupStarterForm(int direction, NativeProgressStore& store) {
    if (m_runStarted) return NativeSaveResult::UnsupportedStage;
    if (direction != -1 && direction != 1) return NativeSaveResult::InvalidRecord;
    const uint16_t dex = selectedSetupStarterDex();
    const auto* species = PokerogueContent::findSpeciesByDex(dex);
    if (!species || !m_starterProfileReady) return NativeSaveResult::InvalidRecord;
    const NativeStarterCandyRecord* record = nullptr;
    for (size_t i = 0; i < m_starterProfileCount; ++i)
        if (m_starterProfileRecords[i].speciesDex == dex) { record = &m_starterProfileRecords[i]; break; }
    if (!record) return NativeSaveResult::InvalidRecord;
    uint16_t count = 0;
    for (const auto& form : PokerogueContent::kForms)
        if (pokemonFormTextEquals(form.speciesId, species->id)) ++count;
    if (count < 2) return NativeSaveResult::InvalidRecord;
    const uint16_t current = setupStarterFormIndex(dex);
    if (current >= count) return NativeSaveResult::InvalidRecord;
    uint16_t next = current;
    for (uint16_t scanned = 1; scanned < count; ++scanned) {
        next = direction > 0 ? (next + 1) % count : (next + count - 1) % count;
        if (pokemonValidateStarterForm(dex, next, record->unlockedFormAttr) != PokemonStarterFormResult::Ok) continue;
        return selectSetupStarterForm(dex, next, store);
    }
    return NativeSaveResult::InvalidRecord;
}

NativeSaveResult FirstRunRuntime::selectSetupStarterForm(uint16_t dex, uint16_t formIndex,
    NativeProgressStore& store) {
    if (m_runStarted) return NativeSaveResult::UnsupportedStage;
    if (!m_starterProfileReady || !starterUnlocked(dex) || !m_context.playerPartyCount ||
        m_context.playerPartyCount > 6) return NativeSaveResult::InvalidRecord;
    std::unique_ptr<FirstRunRuntime> prepared(new (std::nothrow) FirstRunRuntime(*this));
    if (!prepared) return NativeSaveResult::MemoryUnavailable;
    bool found = false;
    for (size_t i = 0; i < prepared->m_starterProfileCount; ++i) {
        auto& record = prepared->m_starterProfileRecords[i];
        if (record.speciesDex != dex) continue;
        if (formIndex != 65535 && pokemonValidateStarterForm(dex, formIndex, record.unlockedFormAttr) !=
                PokemonStarterFormResult::Ok) return NativeSaveResult::InvalidRecord;
        record.preferredFormIndex = formIndex;
        found = true;
        break;
    }
    if (!found) return NativeSaveResult::InvalidRecord;
    PokerogueRngAdapter previewRng;
    previewRng.sow(m_seedCodeUnits.data(), m_seedLength);
    ResolvedPokemon selectedPreview{};
    if (!prepared->resolveStarterFromDex(dex, previewRng, selectedPreview)) return NativeSaveResult::InvalidRecord;
    uint16_t dexes[6]{};
    const uint8_t count = m_context.playerPartyCount;
    for (uint8_t i = 0; i < count; ++i) dexes[i] = m_context.playerParty[i].dex;
    if (!prepared->restoreStarterTeamSetup(m_run.seed, dexes, count)) return NativeSaveResult::InvalidRecord;
    prepared->m_setupCursorDex = m_setupCursorDex;
    const auto result = prepared->saveNativeProgress(store);
    if (result != NativeSaveResult::Ok) return result;
    *this = *prepared;
    buildScene();
    return NativeSaveResult::Ok;
}

NativeSaveResult FirstRunRuntime::purchaseStarterCostReduction(uint16_t dex, NativeProgressStore& store,
    StarterCostPurchaseResult* purchaseResult) {
    if (purchaseResult) *purchaseResult = StarterCostPurchaseResult::InvalidRecord;
    if (m_runStarted) return NativeSaveResult::UnsupportedStage;
    if (!m_starterProfileReady || !starterUnlocked(dex)) return NativeSaveResult::InvalidRecord;
    std::unique_ptr<FirstRunRuntime> prepared(new (std::nothrow) FirstRunRuntime(*this));
    if (!prepared) return NativeSaveResult::MemoryUnavailable;
    bool found = false;
    for (size_t i = 0; i < prepared->m_starterProfileCount; ++i) {
        if (prepared->m_starterProfileRecords[i].speciesDex != dex) continue;
        const auto applied = applyNativeStarterCostReduction(prepared->m_starterProfileRecords[i]);
        if (purchaseResult) *purchaseResult = applied;
        if (applied != StarterCostPurchaseResult::Applied) return NativeSaveResult::InvalidRecord;
        found = true;
        break;
    }
    if (!found) return NativeSaveResult::InvalidRecord;
    const auto committed = prepared->saveNativeProgress(store);
    if (committed != NativeSaveResult::Ok) return committed;
    *this = *prepared;
    buildScene(); // Rebind nodes and text after publishing the heap candidate.
    return NativeSaveResult::Ok;
}

bool FirstRunRuntime::restoreStarterTeamSetup(uint32_t seed, const uint16_t* dexes, size_t count) {
    if (!seed || !starterSelectionAllowed(dexes, count)) return false;
    std::unique_ptr<FirstRunRuntime> prepared(new (std::nothrow) FirstRunRuntime(*this));
    if (!prepared || !prepared->restoreSetupInPlace(seed, dexes[0])) return false;
    PokerogueRngAdapter starterRng;
    starterRng.sow(prepared->m_seedCodeUnits.data(), prepared->m_seedLength);
    for (size_t i = 0; i < count; ++i) {
        if (!prepared->resolveStarterFromDex(dexes[i], starterRng, prepared->m_context.playerParty[i])) return false;
        for (size_t prior = 0; prior < i; ++prior)
            if (prepared->m_context.playerParty[prior].battleState.pokemonId ==
                prepared->m_context.playerParty[i].battleState.pokemonId) return false;
    }
    prepared->m_context.playerPartyCount = static_cast<uint8_t>(count);
    prepared->m_context.activePlayerPartyIndex = 0;
    prepared->m_context.player = prepared->m_context.playerParty[0];
    prepared->m_playerHistoryRequiresSnapshot = count > 1;
    prepared->buildScene();
    *this = *prepared;
    buildScene(); // Rebind nodes and text after publishing the heap candidate.
    return true;
}

bool FirstRunRuntime::restoreSetup(uint32_t seed, uint16_t starterDex) {
    if (!starterSelectionAllowed(&starterDex, 1)) return false;
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime transaction allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
    if (!candidate.restoreSetupInPlace(seed, starterDex)) {
        m_battleFeedback = "New run setup could not resolve";
        buildScene();
        return false;
    }
    *this = candidate;
    buildScene(); // Rebind nodes and text after publishing the heap candidate.
    return true;
}

bool FirstRunRuntime::restoreSetupInPlace(uint32_t seed, uint16_t starterDex) {
    if (!seed) return false;
    std::size_t index = 0;
    for (; index < PokerogueContent::kSpeciesCount; ++index) {
        const auto& species = PokerogueContent::kSpecies[index];
        if (species.dex == starterDex && species.starterEligible) break;
    }
    if (index == PokerogueContent::kSpeciesCount) return false;
    m_run.seed = seed;
    m_setupCursorDex = 0;
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
    return m_encounterResolved && m_context.player.actorIdentityResolved && m_context.player.movesetResolved;
}

NativeSaveResult FirstRunRuntime::captureNativeRunSave(NativeRunSave& output) const {
    bool pending = m_context.player.battleState.pendingStatus != PokemonStatusEffect::None ||
        m_context.enemy.battleState.pendingStatus != PokemonStatusEffect::None ||
        m_context.secondEnemy.battleState.pendingStatus != PokemonStatusEffect::None;
    for (const auto& member : m_context.playerParty) pending |= member.battleState.pendingStatus != PokemonStatusEffect::None;
    for (const auto& member : m_context.trainerParty) pending |= member.battleState.pendingStatus != PokemonStatusEffect::None;
    if (pending) { output = {}; return NativeSaveResult::UnsupportedStage; }
    if (!m_runStarted) {
        for (const auto& member : m_context.playerParty)
            if (member.battleState.status.present || member.battleState.confusion.present || member.battleState.confusion.turns) { output = {}; return NativeSaveResult::UnsupportedStage; }
        for (const auto& member : m_context.trainerParty)
            if (member.battleState.status.present || member.battleState.confusion.present || member.battleState.confusion.turns) { output = {}; return NativeSaveResult::UnsupportedStage; }
    }
    // Setup has no actor snapshot; doubles still require a separate save schema.
    if ((!m_runStarted && (m_context.player.battleState.status.present ||
            m_context.enemy.battleState.status.present || m_context.player.battleState.confusion.present ||
            m_context.enemy.battleState.confusion.present)) || m_context.secondEnemy.battleState.status.present ||
            m_context.secondEnemy.battleState.confusion.present) {
        output = {}; return NativeSaveResult::UnsupportedStage;
    }

    std::unique_ptr<NativeRunSave> valueStorage(new (std::nothrow) NativeRunSave{});
    if (!valueStorage) { output = {}; return NativeSaveResult::MemoryUnavailable; }
    auto& value = *valueStorage;
    // v9 preserves ball inventory; doubles, captured party and later trainer history remain unsupported.
    // Never report a setup checkpoint as a successful save of an active double battle.
    if (m_capturePartyChoicePending || m_context.enemy.bossState.segmentCount || m_doubleBattle || m_pokeballs[5] ||
        m_run.wave > 9 || (m_trainerBattle && m_run.wave != 5)) {
        output = {};
        return NativeSaveResult::UnsupportedStage;
    }
    if (moveLearningPending() || evolutionPending() || (m_runStarted && !m_checkpointAvailable)) {
        output = {};
        return NativeSaveResult::UnsupportedStage;
    }
    const auto setupStatus = makeNativeRunSetupSave(m_run.seed, m_run.starterDex, value);
    if (setupStatus != NativeSaveResult::Ok) {
        output = {};
        return setupStatus;
    }
    for (uint8_t ball = 0; ball < 5; ++ball) value.pokeballCounts[ball] = m_pokeballs[ball];
    value.starterProfileGeneration = m_starterProfileGeneration;
    if (!m_runStarted && m_context.playerPartyCount > 1) {
        if (m_context.playerPartyCount > 6) { output = {}; return NativeSaveResult::InvalidRecord; }
        value.setupStarterCount = m_context.playerPartyCount;
        for (uint8_t i = 0; i < value.setupStarterCount; ++i)
            value.setupStarterDexes[i] = m_context.playerParty[i].dex;
    }
    value.participantHistoryResolved = m_participantHistoryResolved;
    value.participantCount = m_participantCount;
    for (uint8_t i = 0; i < m_participantCount; ++i) value.participantIds[i] = m_participantIds[i];
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
        value.playerStatus = m_context.player.battleState.status;
        value.enemyStatus = m_context.enemy.battleState.status;
        value.playerConfusion = m_context.player.battleState.confusion;
        value.enemyConfusion = m_context.enemy.battleState.confusion;
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
                output = {}; return NativeSaveResult::InvalidRecord;
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
                saved.status = actor.battleState.status;
                saved.confusion = actor.battleState.confusion;
                saved.moveCount = actor.battleState.moveCount;
                for (uint8_t slot = 0; slot < saved.moveCount && slot < 4; ++slot) {
                    saved.moveIds[slot] = actor.battleState.moves[slot].moveId;
                    saved.pp[slot] = actor.battleState.moves[slot].pp;
                }
            }
        }
    }
    bool hasChangedFriendship = false;
    for (const auto& profile : PokerogueContent::kSpeciesFriendshipProfiles)
        if (profile.speciesDex == m_context.player.battleState.speciesDex) {
            hasChangedFriendship = m_context.player.battleState.friendship != profile.baseFriendship;
            break;
        }
    bool hasModifiedMaxPp = false;
    for (uint8_t slot = 0; slot < m_context.player.battleState.moveCount; ++slot) {
        const auto& move = m_context.player.battleState.moves[slot];
        const auto* definition = PokerogueContent::findMoveById(move.moveId);
        if (definition && move.maxPp != definition->pp) hasModifiedMaxPp = true;
    }
    bool hasSummonTags = m_context.player.battleState.heldItemLostTags.unburden;
    for (uint8_t member = 0; member < m_context.playerPartyCount; ++member)
        hasSummonTags |= m_context.playerParty[member].battleState.heldItemLostTags.unburden;
    if (value.stage != NativeSaveStage::RunSetup &&
        (m_context.playerPartyCount > 1 || m_playerHistoryRequiresSnapshot || m_heldModifierCount || value.playerStatus.present || value.playerConfusion.present || hasSummonTags || hasChangedFriendship || hasModifiedMaxPp)) {
        if (m_context.playerPartyCount > 6 ||
            m_context.activePlayerPartyIndex >= m_context.playerPartyCount) { output = {}; return NativeSaveResult::InvalidRecord; }
        value.playerPartyCount = m_context.playerPartyCount;
        value.activePlayerMember = m_context.activePlayerPartyIndex;
        for (uint8_t member = 0; member < value.playerPartyCount; ++member) {
            const auto& actor = member == value.activePlayerMember
                ? m_context.player : m_context.playerParty[member];
            if (!actor.actorIdentityResolved ||
                !captureNativePokemonActorSave(actor.battleState, actor.actor, actor.totalExperience,
                    value.playerParty[member])) { output = {}; return NativeSaveResult::InvalidRecord; }
        }
    }
    if (m_heldModifierCount && value.stage == NativeSaveStage::RunSetup) { output = {}; return NativeSaveResult::InvalidRecord; }
    value.heldModifierCount = static_cast<uint8_t>(m_heldModifierCount);
    for (size_t i = 0; i < m_heldModifierCount; ++i) value.heldModifiers[i] = m_heldModifiers[i];
    output = value;
    return NativeSaveResult::Ok;
}

bool FirstRunRuntime::restoreStarterCandyProfile(const NativeStarterCandyRecord* records, size_t count,
    uint32_t generation, const PokemonFriendshipPolicy& policy) {
    if (!policy.resolved || generation != m_starterProfileGeneration ||
        count > m_starterProfileRecords.size() || (count && !records)) return false;
    uint16_t previous = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!StarterCandyProfileCodec::valid(records[i], previous, PokerogueContent::kMaxStarterCandyCount))
            return false;
        previous = records[i].speciesDex;
    }
    for (size_t i = 0; i < count; ++i) m_starterProfileRecords[i] = records[i];
    for (size_t i = count; i < m_starterProfileRecords.size(); ++i) m_starterProfileRecords[i] = {};
    m_starterProfileCount = count;
    m_starterFriendshipPolicy = policy;
    m_starterProfileReady = true;
    return true;
}

bool FirstRunRuntime::initializeFreshStarterProfile(const PokemonFriendshipPolicy& policy) {
    if (m_starterProfileReady || m_starterProfileGeneration || m_runStarted) return false;
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime transaction allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
    if (!candidate.restoreStarterCandyProfile(nullptr, 0, 0, policy)) return false;
    // GameData.initDexData marks defaultStarterSpecies caught. Canonical
    // freshProfileStarter comes from that pinned source list, not local IDs.
    for (const auto& species : PokerogueContent::kSpecies)
        if (species.freshProfileStarter && !candidate.recordCaughtSpecies(species.dex)) return false;
    for (size_t i = 0; i < candidate.m_starterProfileCount; ++i) {
        const auto* species = PokerogueContent::findSpeciesByDex(candidate.m_starterProfileRecords[i].speciesDex);
        if (species && species->freshProfileStarter &&
            !seedNativeFreshStarterDexMetadata(candidate.m_starterProfileRecords[i])) return false;
    }
    *this = candidate;
    buildScene(); // Rebind nodes and text after publishing the heap candidate.
    return true;
}

uint32_t FirstRunRuntime::caughtSpeciesCount() const {
    uint32_t count = 0;
    for (size_t i = 0; i < m_starterProfileCount; ++i) count += m_starterProfileRecords[i].caught ? 1 : 0;
    return count;
}

bool FirstRunRuntime::hasCaughtSpecies(uint16_t dex) const {
    for (size_t i = 0; i < m_starterProfileCount; ++i)
        if (m_starterProfileRecords[i].speciesDex == dex) return m_starterProfileRecords[i].caught;
    return false;
}

bool FirstRunRuntime::recordCaughtSpecies(uint16_t dex, const ResolvedPokemon* captured) {
    uint64_t observedForm = 0;
    if (captured && pokemonObservedDexFormAttr(captured->dex, captured->actor, observedForm) !=
            PokemonObservedFormResult::Ok) return false;
    if (captured) {
        if (!captured->actorIdentityResolved || captured->actor.abilityIndex > 2 ||
            captured->actor.gender == PokemonGender::Unspecified ||
            static_cast<uint8_t>(captured->actor.gender) > static_cast<uint8_t>(PokemonGender::Female)) return false;
        if (static_cast<uint8_t>(captured->battleState.nature) >= 25) return false;
        for (uint8_t iv : captured->battleState.ivs) if (iv > 31) return false;
    }
    // Prepare every recipient's unlock metadata before modifying the ledger.
    if (captured) {
        uint16_t ancestor = dex;
        size_t visited = 0;
        while (ancestor) {
            if (++visited > PokerogueContent::kSpeciesCount) return false;
            const auto* definition = PokerogueContent::findSpeciesByDex(ancestor);
            uint64_t unlocked = 0;
            if (!definition || pokemonCaptureFormUnlocks(captured->dex, captured->actor, ancestor, unlocked) !=
                    PokemonCaptureFormUnlockResult::Ok) return false;
            ancestor = definition->prevolutionDex;
        }
    }
    // Legacy diagnostics without an attached profile do not fabricate durable data.
    if (!m_starterProfileReady) return true;
    size_t depth = 0;
    while (dex) {
        if (++depth > PokerogueContent::kSpeciesCount) return false;
        const auto* species = PokerogueContent::findSpeciesByDex(dex);
        if (!species) return false;
        size_t index = 0;
        while (index < m_starterProfileCount && m_starterProfileRecords[index].speciesDex < dex) ++index;
        if (index == m_starterProfileCount || m_starterProfileRecords[index].speciesDex != dex) {
            if (m_starterProfileCount == m_starterProfileRecords.size()) return false;
            for (size_t i = m_starterProfileCount; i > index; --i)
                m_starterProfileRecords[i] = m_starterProfileRecords[i - 1];
            m_starterProfileRecords[index] = {dex, 0, 0, false};
            ++m_starterProfileCount;
        }
        m_starterProfileRecords[index].caught = true;
        if (captured) {
            auto& entry = m_starterProfileRecords[index];
            if (dex == captured->dex) entry.observedFormAttr |= observedForm;
            uint64_t unlocked = 0;
            if (pokemonCaptureFormUnlocks(captured->dex, captured->actor, dex, unlocked) !=
                    PokemonCaptureFormUnlockResult::Ok) return false;
            entry.unlockedFormAttr |= unlocked;

            if (species->freshProfileStarter && !seedNativeFreshStarterDexMetadata(entry)) return false;
            entry.natureAttr |= 1u << (static_cast<uint8_t>(captured->battleState.nature) + 1);
            const auto* originalSpecies = PokerogueContent::findSpeciesByDex(captured->dex);
            if (!originalSpecies) return false;
            if (species->starterEligible) {
                const uint8_t index = captured->actor.abilityIndex;
                entry.abilityAttr |= index == 1 && !originalSpecies->ability2 ? 4u : static_cast<uint8_t>(1u << index);
            }
            if (captured->actor.gender == PokemonGender::Male) entry.genderAttr |= 4u;
            else if (captured->actor.gender == PokemonGender::Female) entry.genderAttr |= 8u;

            for (uint8_t i = 0; i < 6; ++i)
                entry.dexIvs[i] = std::max(entry.dexIvs[i], captured->battleState.ivs[i]);
        }
        dex = species->prevolutionDex;
    }
    return true;
}

NativeSaveResult FirstRunRuntime::saveNativeProgress(NativeProgressStore& store) {
    if (!m_starterProfileReady) return NativeSaveResult::InvalidRecord;
    std::unique_ptr<NativeRunSave> snapshotStorage(new (std::nothrow) NativeRunSave{});
    if (!snapshotStorage) return NativeSaveResult::MemoryUnavailable;
    auto& snapshot = *snapshotStorage;
    const auto captured = captureNativeRunSave(snapshot);
    if (captured != NativeSaveResult::Ok) return captured;
    const auto status = store.commit(snapshot, m_starterProfileRecords.data(), m_starterProfileCount);
    if (status != NativeSaveResult::Ok) return status;
    m_starterProfileGeneration = snapshot.starterProfileGeneration;
    return NativeSaveResult::Ok;
}

NativeSaveResult FirstRunRuntime::loadNativeProgress(NativeRunSaveStore& runs,
    NativeStarterCandyStore& profiles, NativeStarterCandyRecord* staging, size_t capacity,
    const PokemonFriendshipPolicy& policy, NativeRunSave* loadedRun) {
    std::unique_ptr<NativeRunSave> savedStorage(new (std::nothrow) NativeRunSave{});
    if (!savedStorage) return NativeSaveResult::MemoryUnavailable;
    auto& saved = *savedStorage;
    auto status = runs.load(PokerogueContent::kContentHash, saved);
    if (status != NativeSaveResult::Ok) return status;
    size_t count = 0;
    uint32_t generation = 0;
    status = saved.starterProfileGeneration
        ? profiles.loadGeneration(PokerogueContent::kContentHash, saved.starterProfileGeneration,
            staging, capacity, count, generation)
        : profiles.load(PokerogueContent::kContentHash, staging, capacity, count, generation);
    if (status == NativeSaveResult::NotFound && !saved.starterProfileGeneration) {
        count = 0; // Explicit fresh/legacy bootstrap, never fabricated awards.
        status = NativeSaveResult::Ok;
    }
    if (status != NativeSaveResult::Ok) return status;
    if (!restoreNativeRunSave(saved, staging, count, &policy)) return NativeSaveResult::InvalidRecord;
    if (loadedRun) *loadedRun = saved;
    return NativeSaveResult::Ok;
}

bool FirstRunRuntime::restoreNativeRunSave(const NativeRunSave& save,
    const NativeStarterCandyRecord* records, size_t count, const PokemonFriendshipPolicy* policy) {
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) != NativeSaveResult::Ok)
        return false;
    const auto* starter = PokerogueContent::findSpeciesByDex(save.starterDex);
    if (!starter || !starter->starterEligible || count > PokerogueContent::kSpeciesCount || (count && !records)) return false;
    bool unlocked = starter->freshProfileStarter;
    if (policy && policy->resolved)
        for (size_t i = 0; i < count; ++i)
            unlocked |= records[i].speciesDex == save.starterDex && records[i].caught;
    if (!unlocked) return false;
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(save.seed));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime restore allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
    candidate.m_starterProfileGeneration = save.starterProfileGeneration;
    if (policy && !candidate.restoreStarterCandyProfile(records, count, save.starterProfileGeneration, *policy)) return false;
    if (!policy && (records || count)) return false;
    if (!candidate.restoreNativeRunSaveInPlace(save)) return false;
    candidate.m_starterProfileGeneration = save.starterProfileGeneration;
    // Reload the referenced durable profile explicitly; live uncommitted gains
    // must never survive rollback merely because the reference is unchanged.
    candidate.m_participantHistoryResolved = save.participantHistoryResolved;
    candidate.m_participantCount = save.participantCount;
    candidate.m_participantIds = {};
    for (uint8_t i = 0; i < save.participantCount; ++i) {
        bool found = false;
        for (uint8_t member = 0; member < candidate.m_context.playerPartyCount; ++member)
            found |= candidate.m_context.playerParty[member].battleState.pokemonId == save.participantIds[i];
        if (!found) return false;
        candidate.m_participantIds[i] = save.participantIds[i];
    }
    if (policy && !candidate.restoreStarterCandyProfile(records, count, save.starterProfileGeneration, *policy))
        return false;
    if (!policy && (records || count)) return false;
    if (save.stage == NativeSaveStage::RunSetup) {
        const uint16_t* dexes = save.setupStarterCount ? save.setupStarterDexes : &save.starterDex;
        const size_t starterCount = save.setupStarterCount ? save.setupStarterCount : 1;
        if (!candidate.restoreStarterTeamSetup(save.seed, dexes, starterCount)) return false;
    }
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
    if (!restoreSetupInPlace(save.seed, save.starterDex)) return false;
    m_participantHistoryResolved = false; // Seed replay is the legacy single-starter path.
    // A skipped reward adds no modifier or party member. Replay each earlier
    // supported wild/fixed trainer victory to reconstruct level/EXP;
    // saved HP and PP are overlaid only after the target encounter is rebuilt.
    if (save.playerPartyCount) {
        m_playerHistoryRequiresSnapshot = true;
        // Explicit party state replaces reward/capture replay. The currently
        // supported frontier has no biome transition or persisted modifiers.
        if (save.wave > 9 || std::strcmp(save.biomeId, PokerogueContent::kStartingBiomeId)) return false;
        m_context.playerPartyCount = save.playerPartyCount;
        m_context.activePlayerPartyIndex = save.activePlayerMember;
        for (uint8_t member = 0; member < save.playerPartyCount; ++member) {
            const auto& saved = save.playerParty[member];
            auto& actor = m_context.playerParty[member];
            actor = {};
            if (!restoreNativePokemonActorSave(saved, actor.battleState, actor.actor)) return false;
            const auto* species = PokerogueContent::findSpeciesByDex(saved.speciesDex);
            if (!species) return false;
            actor.dex = saved.speciesDex;
            actor.level = saved.level;
            actor.speciesId = species->id;
            const std::string localeId = std::string("pokemon:") + species->id;
            actor.localizedName = locale(localeId.c_str(), species->name);
            actor.formId = actor.battleState.formId;
            actor.assetSourcePath = species->assetSourcePath;
            actor.actorIdentityResolved = actor.movesetResolved = true;
            actor.totalExperience = saved.experience;
            actor.moveCount = saved.moveCount;
            for (uint8_t slot = 0; slot < saved.moveCount; ++slot) actor.moveIds[slot] = saved.moveIds[slot];
        }
        m_context.player = m_context.playerParty[save.activePlayerMember];
        m_run.wave = save.wave;
        resolve(true);
        m_participantHistoryResolved = false;
        // Encounter cleanup can reset stages; overlay the explicit checkpoint.
        for (uint8_t member = 0; member < save.playerPartyCount; ++member)
            for (uint8_t stat = 0; stat < 7; ++stat)
                m_context.playerParty[member].battleState.statStages[stat] = save.playerParty[member].statStages[stat];
        for (uint8_t member = 0; member < save.playerPartyCount; ++member)
            m_context.playerParty[member].battleState.heldItemLostTags.unburden = save.playerParty[member].unburdenTag;
        for (uint8_t member = 0; member < save.playerPartyCount; ++member)
            m_context.playerParty[member].battleState.confusion = save.playerParty[member].confusion;
        m_context.player = m_context.playerParty[save.activePlayerMember];
    }
    for (uint16_t wave = 1; !save.playerPartyCount && wave < save.wave; ++wave) {
        if (!m_encounterResolved) return false;
        if (m_trainerBattle) {
            if (wave != 5 || !m_context.trainerPartyBattleStatesResolved ||
                !m_context.trainerPartyCount) return false;
            for (uint8_t member = 0; member < m_context.trainerPartyCount; ++member) {
                m_context.enemy = m_context.trainerParty[member];
                m_experienceGranted = false;
                if (!grantVictoryExperience() || moveLearningPending() || evolutionPending()) return false;
            }
        } else if (!grantVictoryExperience() || moveLearningPending() || evolutionPending()) return false;
        m_run.wave = static_cast<uint16_t>(wave + 1);
        resolve(true);
        m_participantHistoryResolved = false;
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
    if (save.trainerPartyCount && !save.playerPartyCount) {
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
            if (!grantVictoryExperience() || moveLearningPending() || evolutionPending()) return false;
        }
        m_context.enemy = initialEnemy;
        m_experienceGranted = false;
    }
    const auto& reconstructedEnemy = save.trainerPartyCount
        ? m_context.trainerParty[save.activeTrainerMember] : m_context.enemy;

    if (!save.playerPartyCount && !save.trainerPartyCount && save.stage == NativeSaveStage::ExperienceGranted &&
        (!grantVictoryExperience() || moveLearningPending() || evolutionPending())) return false;
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
    m_context.player.battleState.status = save.playerStatus;
    m_context.enemy.battleState.status = save.enemyStatus;
    m_context.player.battleState.confusion = save.playerConfusion;
    m_context.enemy.battleState.confusion = save.enemyConfusion;
    for (uint8_t i = 0; i < save.playerMoveCount; ++i)
        m_context.player.battleState.moves[i].pp = save.playerPp[i];
    for (uint8_t i = 0; i < save.enemyMoveCount; ++i)
        m_context.enemy.battleState.moves[i].pp = save.enemyPp[i];
    if (save.trainerPartyCount) {
        for (uint8_t member = 0; member < save.trainerPartyCount; ++member) {
            auto& actor = m_context.trainerParty[member];
            const auto& saved = save.trainerParty[member];
            actor.battleState.hp = saved.hp;
            actor.battleState.status = saved.status;
            actor.battleState.confusion = saved.confusion;
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
    for (uint8_t ball = 0; ball < 5; ++ball) m_pokeballs[ball] = save.pokeballCounts[ball];
    m_pokeballs[5] = 0;
    m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;
    if (!restoreHeldModifierInventory(save.heldModifiers, save.heldModifierCount)) return false;
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
    if (!move || !pokemonStatStageMoveBuildersResolved(moveId) ||
        move->category != PokerogueContent::MoveStatus || !move->target ||
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
bool statusActionAbilitySupported(uint16_t abilityId) {
    for (const auto& profile : PokerogueContent::kStatusActionAbilityProfiles)
        if (profile.abilityId == abilityId) return profile.resolved;
    return false;
}
const PokerogueContent::MoveStatusEffect* singleOpponentStatusEffect(uint16_t moveId) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || move->attributeCount != 1 ||
        !PokerogueContent::moveHasAttribute(*move, "StatusEffectAttr") || !move->target ||
        (std::strcmp(move->target, "NEAR_OTHER") && std::strcmp(move->target, "NEAR_ENEMY"))) return nullptr;
    bool flagsKnown = false;
    for (const auto& flags : PokerogueContent::kStatusMoveFlagProfiles)
        if (flags.moveId == moveId) flagsKnown = flags.resolved;
    if (!flagsKnown) return nullptr;
    const PokerogueContent::MoveStatusEffect* result = nullptr;
    for (const auto& row : PokerogueContent::kMoveStatusEffects) {
        if (row.moveId != moveId) continue;
        if (result || !row.parametersResolved || row.selfTarget || !row.effectId || row.effectId >= 7) return nullptr;
        result = &row;
    }
    return result;
}
const PokerogueContent::MoveStatusEffect* singleDamageStatusEffect(uint16_t moveId) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category == PokerogueContent::MoveStatus || move->power <= 0 ||
        move->upstreamFlags || !pokemonDamageSecondaryAttributesResolved(*move, "StatusEffectAttr") || !move->target ||
        std::strcmp(move->target, "NEAR_OTHER")) return nullptr;
    bool buildersResolved = false;
    for (const auto& profile : PokerogueContent::kStatusMoveFlagProfiles)
        if (profile.moveId == moveId) buildersResolved = profile.resolved;
    if (!buildersResolved) return nullptr;
    const PokerogueContent::MoveStatusEffect* result = nullptr;
    for (const auto& row : PokerogueContent::kMoveStatusEffects) {
        if (row.moveId != moveId) continue;
        if (result || !row.parametersResolved || row.selfTarget || !row.effectId || row.effectId >= 7) return nullptr;
        result = &row;
    }
    return result;
}
bool singleDamageConfusionEffect(uint16_t moveId) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category == PokerogueContent::MoveStatus || move->power <= 0 || move->upstreamFlags ||
        !pokemonDamageSecondaryAttributesResolved(*move, "ConfuseAttr") || !move->target ||
        std::strcmp(move->target, "NEAR_OTHER")) return false;
    bool known = false;
    for (const auto& flags : PokerogueContent::kStatusMoveFlagProfiles)
        if (flags.moveId == moveId) known = flags.resolved;
    if (!known) return false;
    for (const auto& profile : PokerogueContent::kMoveConfusionEffects)
        if (profile.moveId == moveId) return profile.resolved && !profile.selfTarget;
    return false;
}
bool singleStatusConfusionEffect(uint16_t moveId) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || move->attributeCount != 1 ||
        !move->target || std::strcmp(move->target, "NEAR_OTHER") ||
        !PokerogueContent::moveHasAttribute(*move, "ConfuseAttr")) return false;
    bool known = false;
    for (const auto& flags : PokerogueContent::kStatusMoveFlagProfiles)
        if (flags.moveId == moveId) known = flags.resolved;
    if (!known) return false;
    for (const auto& profile : PokerogueContent::kMoveConfusionEffects)
        if (profile.moveId == moveId) return profile.resolved && !profile.selfTarget;
    return false;
}
const PokerogueContent::MoveStatStageEffect* singleDamageStatStageEffect(uint16_t moveId) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->power <= 0 || move->upstreamFlags || !move->target ||
        std::strcmp(move->target, "NEAR_OTHER") ||
        !pokemonDamageSecondaryAttributesResolved(*move, "StatStageChangeAttr")) return nullptr;
    bool buildersResolved = false;
    for (const auto& flags : PokerogueContent::kStatusMoveFlagProfiles)
        if (flags.moveId == moveId) buildersResolved = flags.resolved;
    if (!buildersResolved) return nullptr;
    const PokerogueContent::MoveStatStageEffect* effect = nullptr;
    for (const auto& row : PokerogueContent::kMoveStatStageEffects)
        if (row.moveId == moveId) { if (effect) return nullptr; effect = &row; }
    return effect;
}
bool supportsBaselineBattleMove(uint16_t moveId) {
    if (singleDamageStatStageEffect(moveId) || singleStatusConfusionEffect(moveId) || singleOpponentStatusEffect(moveId) || singleDamageStatusEffect(moveId) || singleDamageConfusionEffect(moveId) || pokemonWeatherChangeProfile(moveId) || supportsPokemonTrickRoomMove(moveId) || supportsPokemonStatStageMove(moveId) || selfHealingProfile(moveId) || damageDrainProfile(moveId) || damageRecoilProfile(moveId)) return true;
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
                              const PokerogueContent::Move& move, double secondaryBenefit = 0.0) {
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
    double statBenefit = 0.0;
    if (singleDamageStatStageEffect(move.id) &&
        !calculateCanonicalDamageStatStageAiBenefit(user, target, move.id, statBenefit)) return -20.0;
    const double userBenefit = critBenefit + statBenefit + secondaryBenefit +
        canonicalDamageDrainAiBenefit(user, move) + canonicalRecoilAiBenefit(move) +
        pokemonCanonicalThawAiBenefit(user, move.id);
    if (!calculatePlainAttackAiScore(effectiveness, selectedStat, otherStat,
            move.power, move.accuracy, stab, score, userBenefit)) return -20.0;
    return score;
}

}

bool FirstRunRuntime::resolveActiveStatusRecipientPolicies(const PokemonBattleState& recipient,
    const PokemonBattleState& source, PokemonStatusEffect effect, PokemonStatusRecipientPolicies& output) const {
    // Current arena has weather only. Terrain/Safeguard, passive abilities,
    // type overrides and grounding modifiers need explicit runtime state before
    // this provider can admit actors carrying those capabilities.
    if (m_doubleBattle || m_heldModifierCount || !recipient.statsAreBaseFormulaOnly ||
        !source.statsAreBaseFormulaOnly) return false;
    if (!statusActionAbilitySupported(recipient.abilityId) ||
        !statusActionAbilitySupported(source.abilityId)) return false;
    const auto* form = recipient.formId ? PokerogueContent::findFormById(recipient.formId) : nullptr;
    const auto* species = PokerogueContent::findSpeciesByDex(recipient.speciesDex);
    if (!form || !species || std::strcmp(form->speciesId, species->id) != 0) return false;
    const char* types[] = {form->type1, form->type2};
    if (!types[0] || !resolvePokemonTypeSymbol(types[0])) return false;
    const size_t count = types[1] && types[1][0] ? 2 : 1;
    if (count == 2 && !resolvePokemonTypeSymbol(types[1])) return false;
    PokemonStatusFieldContext field{};
    field.resolved = true;
    field.effectiveTypes = field.originalIfStellarTypes = types;
    field.effectiveTypeCount = field.originalIfStellarTypeCount = count;
    field.grounded = std::strcmp(types[0], "FLYING") != 0 &&
        (count == 1 || std::strcmp(types[1], "FLYING") != 0);
    field.sunnyOrHarshSun = m_arenaWeather.type == PokemonEffectiveWeather::Sunny ||
        m_arenaWeather.type == PokemonEffectiveWeather::HarshSun;
    // Admitted status-action abilities have no bypassFaint builder; HP controls
    // activity after recoil while provenance/capability resolution stays explicit.
    const PokemonStatusAbilityComponent own[] = {{recipient.abilityId, recipient.hp != 0, true}};
    const PokemonStatusAbilityComponent sourceAbilities[] = {{source.abilityId, source.hp != 0, true}};
    return resolvePokemonStatusRecipientPolicies(recipient, &source, effect, field,
        own, 1, nullptr, 0, sourceAbilities, 1, output);
}

bool FirstRunRuntime::resolveActiveStatusCommandPolicies(const PokemonBattleState& user,
    const PokemonBattleState& opponent, uint16_t moveId, uint8_t ppCost,
    PokemonStatusEffectCommandPolicy& commandOutput, PokemonPostSetStatusPolicy& reactionsOutput) const {
    const auto* move = PokerogueContent::findMoveById(moveId);
    const auto* effect = singleOpponentStatusEffect(moveId);
    if (!move || (!effect && !singleStatusConfusionEffect(moveId))) return false;
    PokemonStatusRecipientPolicies recipientPolicies{}, sourcePolicies{};
    const auto status = effect ? static_cast<PokemonStatusEffect>(effect->effectId) : PokemonStatusEffect::None;
    if (!resolveActiveStatusRecipientPolicies(opponent, user, status, recipientPolicies) ||
        !resolveActiveStatusRecipientPolicies(user, opponent, status, sourcePolicies)) return false;
    PokemonPostSetStatusPolicy reactions{};
    if (effect && !resolvePokemonPostSetStatusPolicy(opponent, user, status, recipientPolicies,
            sourcePolicies, true, true, true, reactions)) return false;
    PokemonStatusEffectCommandPolicy command{};
    command.reactionsResolved = true;
    command.move.application = recipientPolicies.status;
    command.move.chanceCallbacksResolved = true;
    if (!resolvePokemonMoveEffectChance(moveId, user.abilityId, opponent.abilityId, false,
            command.move.effectiveChance)) return false;
    command.move.ppCost = ppCost;
    PokemonMoveWeatherContext weather{};
    PokemonHitPolicy hit{};
    const PokemonWeatherAbilityComponent abilities[] = {
        {user.abilityId, true, true}, {opponent.abilityId, true, false}
    };
    if (!resolveActiveMoveWeather(user, opponent, weather) ||
        !composePokemonAlwaysHitPolicy(abilities, 2, hit, move->id, &weather)) return false;
    command.move.hit.resolved = true;
    command.move.hit.blockedBeforeAccuracy = hit.blockedByAbility;
    command.move.hit.bypassAccuracy = hit.bypassAccuracy || move->accuracy < 0;
    command.move.hit.accuracyMultiplier = hit.accuracyMultiplier;
    bool ignoreUserAccuracy = false, ignoreTargetEvasion = false;
    for (const auto& profile : PokerogueContent::kStatusActionAbilityProfiles) {
        if (profile.abilityId == opponent.abilityId) ignoreUserAccuracy = profile.ignoresOpponentAccuracy;
        if (profile.abilityId == user.abilityId) ignoreTargetEvasion = profile.ignoresOpponentEvasion;
    }
    if (!composePokemonStatusAccuracyStagePolicy(user, opponent, command.move.hit, command.move.hit,
            ignoreUserAccuracy, ignoreTargetEvasion)) return false;
    const auto* form = PokerogueContent::findFormById(opponent.formId);
    if (!form) return false;
    const char* types[] = {form->type1, form->type2};
    PokemonStatusMoveTypeImmunityPolicy typePolicy{};
    typePolicy.resolved = typePolicy.opponents = true;
    typePolicy.originalIfStellarTypes = types;
    typePolicy.typeCount = types[1] && types[1][0] ? 2 : 1;
    const PokemonStatusAbilityComponent defenders[] = {{opponent.abilityId, true, true}};
    if (!composePokemonStatusMoveTypeHitPolicy(move->id, command.move.hit, typePolicy, command.move.hit) ||
        !composePokemonStatusFlagAbilityHitPolicy(move->id, command.move.hit, false,
            defenders, 1, command.move.hit)) return false;
    commandOutput = command;
    reactionsOutput = reactions;
    return true;
}

bool FirstRunRuntime::resolveActiveStatStageCommandPolicy(const PokemonBattleState& user,
    const PokemonBattleState& opponent, uint16_t moveId, uint8_t ppCost,
    PokemonStatStageCommandPolicy& output) const {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || (!supportsPokemonStatStageMove(moveId) && !singleDamageStatStageEffect(moveId)) ||
        m_doubleBattle || m_heldModifierCount ||
        !user.statsAreBaseFormulaOnly || !opponent.statsAreBaseFormulaOnly) return false;
    const bool damaging = move->category != PokerogueContent::MoveStatus;
    const PokerogueContent::MoveStatStageEffect* effect = nullptr;
    for (const auto& row : PokerogueContent::kMoveStatStageEffects)
        if (row.moveId == moveId) { if (effect) return false; effect = &row; }
    if (!effect) return false;
    const bool self = effect->selfTarget;
    // A missing stage profile is neutral only after the complete action
    // capability was resolved. Unknown callbacks must not disappear for USER.
    if (!statusActionAbilitySupported(user.abilityId) ||
        !statusActionAbilitySupported(opponent.abilityId)) return false;
    PokemonStatStageCommandPolicy policy{};
    // Negative chance is the pinned guaranteed-effect sentinel; multiplier
    // and IgnoreMoveEffects callbacks leave it unchanged. Nonnegative chances
    // always need the resolved callback path, including status-category moves.
    if (move->upstreamChance < -1 || move->upstreamChance > 100) return false;
    policy.move.stagePolicy.chance = move->upstreamChance;
    if (damaging || move->upstreamChance >= 0) {
        if (!statusActionAbilitySupported(user.abilityId) || !statusActionAbilitySupported(opponent.abilityId) ||
            !resolvePokemonMoveEffectChance(moveId, user.abilityId, opponent.abilityId, self,
                policy.move.stagePolicy.chance)) return false;
    }
    policy.move.hitPolicyResolved = true;
    policy.move.ppCost = self ? 1 : ppCost;
    policy.move.stagePolicy.resolved = true;
    policy.postChangePoliciesResolved = true;
    if (!self && !damaging) {
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
        PokemonStatusMoveHitPolicy baseAccuracy{}, stagedAccuracy{};
        baseAccuracy.resolved = true;
        baseAccuracy.accuracyMultiplier = hit.accuracyMultiplier;
        bool ignoreUserAccuracy = false, ignoreTargetEvasion = false;
        bool userAccuracyResolved = false, targetAccuracyResolved = false;
        for (const auto& profile : PokerogueContent::kStatusActionAbilityProfiles) {
            if (profile.abilityId == user.abilityId) {
                userAccuracyResolved = profile.resolved;
                ignoreTargetEvasion = profile.ignoresOpponentEvasion;
            }
            if (profile.abilityId == opponent.abilityId) {
                targetAccuracyResolved = profile.resolved;
                ignoreUserAccuracy = profile.ignoresOpponentAccuracy;
            }
        }
        if (!userAccuracyResolved || !targetAccuracyResolved ||
            !composePokemonStatusAccuracyStagePolicy(user, opponent, baseAccuracy, stagedAccuracy,
                ignoreUserAccuracy, ignoreTargetEvasion)) return false;
        const auto* form = PokerogueContent::findFormById(opponent.formId);
        if (!form) return false;
        const char* types[] = {form->type1, form->type2};
        PokemonStatusMoveTypeImmunityPolicy typePolicy{};
        typePolicy.resolved = typePolicy.opponents = true;
        typePolicy.originalIfStellarTypes = types;
        typePolicy.typeCount = types[1] && types[1][0] ? 2 : 1;
        const PokemonStatusAbilityComponent defenders[] = {{opponent.abilityId, true, true}};
        stagedAccuracy.blockedBeforeAccuracy = hit.blockedByAbility;
        stagedAccuracy.bypassAccuracy = hit.bypassAccuracy || move->accuracy < 0;
        if (!composePokemonStatusMoveTypeHitPolicy(moveId, stagedAccuracy, typePolicy, stagedAccuracy) ||
            !composePokemonStatusFlagAbilityHitPolicy(moveId, stagedAccuracy, false,
                defenders, 1, stagedAccuracy)) return false;
        policy.move.blockedBeforeAccuracy = stagedAccuracy.blockedBeforeAccuracy;
        policy.move.bypassAccuracy = stagedAccuracy.bypassAccuracy;
        policy.move.typeImmune = stagedAccuracy.typeImmune;
        policy.move.accuracyMultiplier = stagedAccuracy.accuracyMultiplier;
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
        if (row.abilityId == recipient.abilityId) {
            if (recipientCount >= 2) return false;
            policy.recipientReactions[recipientCount++] = &row;
        }

    uint8_t sourceCount = 0;
    for (const auto& row : PokerogueContent::kAbilityStatStageReactions)
        if (row.abilityId == source.abilityId) {
            if (sourceCount >= 2) return false;
            policy.sourceReactions[sourceCount++] = &row;
        }

    output = policy;
    return true;
}

bool FirstRunRuntime::supportsActiveBattleMove(const PokemonBattleState& user,
    const PokemonBattleState& opponent, uint16_t moveId) const {
    if (!supportsBaselineBattleMove(moveId)) return false;
    if (supportsPokemonStatStageMove(moveId) || singleDamageStatStageEffect(moveId)) {
        const auto* move = PokerogueContent::findMoveById(moveId);
        uint8_t ppCost = 1;
        PokemonStatStageCommandPolicy policy{};
        return move && (!std::strcmp(move->target, "USER") ||
            pokemonSingleOpponentPpCost(opponent.abilityId, ppCost)) &&
            resolveActiveStatStageCommandPolicy(user, opponent, moveId, ppCost, policy);
    }
    if (const auto* effect = singleDamageStatusEffect(moveId)) {
        PokemonStatusRecipientPolicies recipient{}, source{};
        PokemonPostSetStatusPolicy reactions{};
        const auto status = static_cast<PokemonStatusEffect>(effect->effectId);
        int16_t chance = 0;
        return resolvePokemonMoveEffectChance(moveId, user.abilityId, opponent.abilityId, false, chance) &&
            resolveActiveStatusRecipientPolicies(opponent, user, status, recipient) &&
            resolveActiveStatusRecipientPolicies(user, opponent, status, source) &&
            resolvePokemonPostSetStatusPolicy(opponent, user, status, recipient, source, true, true, true, reactions);
    }
    if (singleDamageConfusionEffect(moveId)) {
        PokemonStatusRecipientPolicies recipient{};
        int16_t chance = 0;
        return resolveActiveStatusRecipientPolicies(opponent, user, PokemonStatusEffect::None, recipient) &&
            resolvePokemonMoveEffectChance(moveId, user.abilityId, opponent.abilityId, false, chance);
    }
    if (!singleOpponentStatusEffect(moveId) && !singleStatusConfusionEffect(moveId)) return true;
    uint8_t ppCost = 1;
    PokemonStatusEffectCommandPolicy command{};
    PokemonPostSetStatusPolicy reactions{};
    return pokemonSingleOpponentPpCost(opponent.abilityId, ppCost) &&
        resolveActiveStatusCommandPolicies(user, opponent, moveId, ppCost, command, reactions);
}

double FirstRunRuntime::scoreActiveEnemyMove(const PokemonBattleState& user,
    const PokemonBattleState& target, const PokerogueContent::Move& move) const {
    int16_t chance = move.upstreamChance;
    if (singleStatusConfusionEffect(move.id)) {
        double benefit = 0;
        return resolvePokemonMoveEffectChance(move.id, user.abilityId, target.abilityId, false, chance) &&
            calculatePokemonConfusionMoveAiBenefit(move.id, chance, benefit) ? -benefit : -20.0;
    }
    const auto* effect = singleOpponentStatusEffect(move.id);
    if (!effect) {
        double secondaryBenefit = 0.0;
        if (const auto* secondary = singleDamageStatusEffect(move.id)) {
            PokemonStatusRecipientPolicies policies{};
            double benefit = 0;
            if (!resolvePokemonMoveEffectChance(move.id, user.abilityId, target.abilityId, false, chance) ||
                !resolveActiveStatusRecipientPolicies(target, user,
                    static_cast<PokemonStatusEffect>(secondary->effectId), policies) ||
                !calculatePokemonStatusEffectAiBenefit(target, move.id, chance,
                    true, policies.status, benefit)) return -20.0;
            secondaryBenefit -= benefit;
        }
        if (singleDamageConfusionEffect(move.id)) {
            double benefit = 0;
            if (!resolvePokemonMoveEffectChance(move.id, user.abilityId, target.abilityId, false, chance) ||
                !calculatePokemonConfusionMoveAiBenefit(move.id, chance, benefit)) return -20.0;
            secondaryBenefit -= benefit;
        }
        return baselineEnemyMoveScore(user, target, move, secondaryBenefit);
    }
    PokemonStatusRecipientPolicies policies{};
    double targetBenefit = 0;
    if (!resolvePokemonMoveEffectChance(move.id, user.abilityId, target.abilityId, false, chance) ||
        !resolveActiveStatusRecipientPolicies(target, user,
            static_cast<PokemonStatusEffect>(effect->effectId), policies) ||
        !calculatePokemonStatusEffectAiBenefit(target, move.id, chance,
            true, policies.status, targetBenefit)) return -20.0;
    // Pokemon.getEnemyMoveScores flips target benefit for opposing Pokemon.
    return -targetBenefit;
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
        if (!supportsActiveBattleMove(m_context.enemy.battleState, m_context.player.battleState, move.moveId) || !weatherMoveAllowed(move.moveId) ||
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
            if (!supportsActiveBattleMove(m_context.enemy.battleState, m_context.player.battleState, move.moveId) || !weatherMoveAllowed(move.moveId) ||
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
            if (!supportsActiveBattleMove(m_context.secondEnemy.battleState, m_context.player.battleState, move.moveId) || !weatherMoveAllowed(move.moveId) ||
                (damageDrainProfile(move.moveId) && hasCanonicalReverseDrain(m_context.player.battleState.abilityId))) return false;
            ++enemy1Usable;
        }
    } else {
        enemy1Usable = 1;
    }

    if (!enemy0Usable || !enemy1Usable) return false;

    if (m_selectedBattleMove >= m_context.player.battleState.moveCount) return false;
    const auto& playerMove = m_context.player.battleState.moves[m_selectedBattleMove];
    if (!playerMove.pp || !supportsActiveBattleMove(m_context.player.battleState, m_context.enemy.battleState, playerMove.moveId) || !weatherMoveAllowed(playerMove.moveId)) return false;
    if (damageDrainProfile(playerMove.moveId) &&
        (hasCanonicalReverseDrain(m_context.enemy.battleState.abilityId) ||
         hasCanonicalReverseDrain(m_context.secondEnemy.battleState.abilityId))) return false;

    return true;
}

bool FirstRunRuntime::battleInputSupported() const {
    if (m_capturePartyChoicePending) return false;
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
        if (!supportsActiveBattleMove(m_context.enemy.battleState, m_context.player.battleState, move.moveId) || !weatherMoveAllowed(move.moveId) ||
            (damageDrainProfile(move.moveId) && hasCanonicalReverseDrain(m_context.player.battleState.abilityId))) return false;
        ++enemyUsable;
    }
    return enemyUsable && m_selectedBattleMove < m_context.player.battleState.moveCount &&
        m_context.player.battleState.moves[m_selectedBattleMove].pp &&
        supportsActiveBattleMove(m_context.player.battleState, m_context.enemy.battleState,
            m_context.player.battleState.moves[m_selectedBattleMove].moveId) &&
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

    const PokemonBattleState* party[6]{};
    if (!m_context.playerPartyCount || m_context.playerPartyCount > 6 ||
        m_context.activePlayerPartyIndex >= m_context.playerPartyCount) return false;
    for (uint8_t member = 0; member < m_context.playerPartyCount; ++member)
        party[member] = member == m_context.activePlayerPartyIndex ? &m_context.player.battleState
            : &m_context.playerParty[member].battleState;
    InitialClassicRewardWeights weights(m_context.player.battleState, true, party, m_context.playerPartyCount,
        m_pokeballs.data(), 5);
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
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime transaction allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
    if (!candidate.claimRewardChoiceInPlace()) {
        m_battleFeedback = candidate.m_battleFeedback;
        buildScene();
        return false;
    }
    *this = candidate;
    buildScene();
    return true;
}

bool FirstRunRuntime::claimHeldRewardChoice(uint8_t partyMember) {
    if (partyMember >= m_context.playerPartyCount) return false;
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime transaction allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
    if (!candidate.claimRewardChoiceInPlace(partyMember)) {
        m_battleFeedback = candidate.m_battleFeedback;
        buildScene();
        return false;
    }
    *this = candidate;
    buildScene();
    return true;
}

bool FirstRunRuntime::claimRecoveryRewardChoice(uint8_t partyMember, uint8_t moveSlot) {
    if (partyMember >= m_context.playerPartyCount) return false;
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime transaction allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
    if (!candidate.claimRewardChoiceInPlace(partyMember, true, moveSlot)) {
        m_battleFeedback = candidate.m_battleFeedback;
        buildScene();
        return false;
    }
    *this = candidate;
    buildScene();
    return true;
}

bool FirstRunRuntime::claimRewardChoiceInPlace(uint8_t heldPartyMember, bool recoveryTarget, uint8_t recoveryMove) {
    if (!heldHealingInventorySupported(m_heldModifiers.data(), m_heldModifierCount)) {
        m_battleFeedback = "Held modifier effects require native dispatch";
        return false;
    }
    if (moveLearningPending() || evolutionPending()) return false;
    if (!m_rewardsPending || m_selectedRewardChoice >= m_rewardChoiceCount) return false;
    const auto& choice = m_rewardChoices[m_selectedRewardChoice];
    if (!choice.poolEntry || !choice.poolEntry->itemId) return false;
    if (choice.poolEntry && choice.poolEntry->itemId) {
        const char* itemId = choice.poolEntry->itemId;
        auto& playerState = m_context.player.battleState;
        const uint8_t targetMember = heldPartyMember == 0xFF ? m_context.activePlayerPartyIndex : heldPartyMember;
        if (targetMember >= m_context.playerPartyCount) return false;
        auto& targetState = targetMember == m_context.activePlayerPartyIndex ? playerState
            : m_context.playerParty[targetMember].battleState;
        NativeHeldModifierInstance heldReward{};
        const bool knownHeldReward = initializeHeldModifierInstance(itemId, targetState.pokemonId,
            1, true, nullptr, heldReward) == HeldModifierStorageResult::Ok &&
            heldHealingInventorySupported(&heldReward, 1);
        if (recoveryTarget && !hpRestoreItemProfile(itemId) && !ppRestoreItemProfile(itemId) && !ppUpItemProfile(itemId) && !reviveItemProfile(itemId)) {
            m_battleFeedback = "Selected reward has no supported recovery recipient policy";
            return false;
        }
        if (heldPartyMember != 0xFF && !recoveryTarget && !knownHeldReward) {
            m_battleFeedback = "Selected reward has no supported held recipient policy";
            return false;
        }
        if (knownHeldReward) {
            const auto added = addKnownHealingHeldReward(m_heldModifiers.data(), m_heldModifiers.size(),
                m_heldModifierCount, heldReward);
            if (added != HeldRewardAddResult::Added && added != HeldRewardAddResult::Merged) {
                m_battleFeedback = "Held reward requires stack replacement or storage policy";
                return false;
            }
        } else if (const auto* restore = hpRestoreItemProfile(itemId)) {
            uint16_t healed = 0;
            // Current baseline actor has no unresolved status or Healing Charm.
            if (!applyPokemonHpRestoreItem(targetState, *restore, 1.0, true, healed)) {
                m_battleFeedback = "HP restore item policy could not resolve";
                return false;
            }
        } else if (const auto* restore = ppRestoreItemProfile(itemId)) {
            if (!applyPokemonPpRestoreItem(targetState, *restore, recoveryTarget ? recoveryMove : m_selectedBattleMove)) {
                m_battleFeedback = "PP restore item selection could not resolve";
                return false;
            }
        } else if (const auto* ppUp = ppUpItemProfile(itemId)) {
            if (!applyPokemonPpUpItem(targetState, *ppUp, recoveryTarget ? recoveryMove : m_selectedBattleMove)) {
                m_battleFeedback = "PP Up requires an eligible move without an override";
                return false;
            }
        } else if (const auto* ballReward = pokeballRewardProfile(itemId)) {
            if (!applyPokeballReward(*ballReward, m_pokeballs.data(), m_pokeballs.size())) {
                m_battleFeedback = "Pokeball reward type could not resolve";
                return false;
            }
        } else if (const auto* revive = reviveItemProfile(itemId)) {
            bool applied = false;
            if (revive->allParty) {
                PokemonBattleState* party[6]{};
                if (m_context.playerPartyCount > 6) return false;
                for (uint8_t member = 0; member < m_context.playerPartyCount; ++member)
                    party[member] = member == m_context.activePlayerPartyIndex ? &playerState
                        : &m_context.playerParty[member].battleState;
                applied = applyPokemonPartyReviveItem(party, m_context.playerPartyCount, *revive,
                    true, false, true);
            } else applied = applyPokemonReviveItem(targetState, *revive, true, false, true);
            if (!applied) {
                m_battleFeedback = "Revive requires eligible fainted party members";
                return false;
            }
        } else {
            m_battleFeedback = "Reward effect requires a canonical native adapter";
            return false;
        }
    }
    m_rewardsPending = false;
    m_run.wave = m_victoryPlan.nextWave;
    resolve(true);
    if (!m_encounterResolved) {
        m_checkpointAvailable = false;
        if (m_battleFeedback.empty()) m_battleFeedback = "Next canonical encounter could not resolve";
        buildScene();
        return false;
    }
    m_battleFeedback = "Reward claimed - next Classic wave";
    buildScene();
    return m_encounterResolved;
}

ResolvedPokemon& FirstRunRuntime::progressionPokemonMutable() {
    return m_progressionPartyIndex < m_context.playerPartyCount &&
        m_progressionPartyIndex != m_context.activePlayerPartyIndex
        ? m_context.playerParty[m_progressionPartyIndex] : m_context.player;
}

void FirstRunRuntime::advanceProgressionQueue() {
    if (moveLearningPending() || m_pendingEvolutionSpeciesId || m_evolutionPauseConfirmation) return;
    m_progressionPartyIndex = 0xFF;
    if (m_progressionQueueCursor >= m_progressionQueueCount) return;
    const uint8_t entry = m_progressionQueueCursor++;
    m_progressionPartyIndex = m_progressionQueueMembers[entry];
    m_pendingLevelMoves = m_progressionQueueMoves[entry];
    m_pendingEvolutionSpeciesId = m_progressionQueueEvolutions[entry];
}

bool FirstRunRuntime::finishPendingEvolution(bool accepted) {
    if (!m_pendingEvolutionSpeciesId) { advanceProgressionQueue(); return true; }
    if (!accepted) {
        m_battleFeedback = "Evolution ready: A continue, B cancel";
        return true;
    }
    auto next = progressionPokemon();
    EvolutionResult event{};
    std::string feedback;
    if (!applySpeciesEvolution(next.dex, m_pendingEvolutionSpeciesId, next.battleState,
            event, &feedback, &next.actor)) return false;
    const auto* species = PokerogueContent::findSpeciesByDex(event.newDex);
    if (!species) return false;
    next.dex = event.newDex;
    next.speciesId = species->id;
    const std::string key = std::string("pokemon:") + species->id;
    next.localizedName = locale(key.c_str(), species->name);
    next.formId = next.battleState.formId;
    next.assetSourcePath = species->assetSourcePath;
    auto pending = m_pendingLevelMoves;
    if (!learnPokemonEvolutionMoves(next.battleState, pending)) return false;
    next.moveCount = next.battleState.moveCount;
    for (uint8_t slot = 0; slot < next.moveCount; ++slot) next.moveIds[slot] = next.battleState.moves[slot].moveId;
    m_pendingLevelMoves = pending;
    const uint8_t member = m_progressionPartyIndex < m_context.playerPartyCount
        ? m_progressionPartyIndex : m_context.activePlayerPartyIndex;
    m_context.playerParty[member] = next;
    if (member == m_context.activePlayerPartyIndex) m_context.player = next;
    m_playerHistoryRequiresSnapshot = true;
    m_pendingEvolutionSpeciesId = nullptr;
    advanceProgressionQueue();
    m_battleFeedback = feedback;
    return true;
}

bool FirstRunRuntime::resolvePendingLearnMove(int selectedSlot) {
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime transaction allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
    if (!candidate.resolvePendingLearnMoveInPlace(selectedSlot)) return false;
    *this = candidate;
    buildScene();
    return true;
}

bool FirstRunRuntime::resolvePendingLearnMoveInPlace(int selectedSlot) {
    if (!moveLearningPending() || selectedSlot < -1 || selectedSlot > 3) return false;
    if (selectedSlot >= 0) {
        auto next = progressionPokemon().battleState;
        const auto result = learnPokemonMoveAtSlot(next, pendingLearnMoveId(), static_cast<uint8_t>(selectedSlot));
        if (result != PokemonLearnMoveResult::Learned && result != PokemonLearnMoveResult::AlreadyKnown) return false;
        auto& target = progressionPokemonMutable();
        target.battleState = next;
        target.moveCount = next.moveCount;
        for (uint8_t slot = 0; slot < next.moveCount; ++slot) target.moveIds[slot] = next.moves[slot].moveId;
        if (m_progressionPartyIndex == 0xFF || m_progressionPartyIndex == m_context.activePlayerPartyIndex)
            m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;
    }
    m_playerHistoryRequiresSnapshot = true;
    for (uint16_t i = 1; i < m_pendingLevelMoves.count; ++i)
        m_pendingLevelMoves.moveIds[i - 1] = m_pendingLevelMoves.moveIds[i];
    m_pendingLevelMoves.moveIds[--m_pendingLevelMoves.count] = 0;
    if (!moveLearningPending() && !finishPendingEvolution()) return false;
    m_battleFeedback = moveLearningPending() ? "Choose move to replace: UP/DOWN, A learn, B reject"
        : selectedSlot < 0 ? "Move not learned" : "Move learned";
    buildScene();
    return true;
}

bool FirstRunRuntime::selectBattleMove(int direction) {
    if (m_capturePartyChoicePending) {
        if (!direction) return false;
        m_selectedCapturePartyChoice = static_cast<uint8_t>((m_selectedCapturePartyChoice + (direction > 0 ? 1 : 5)) % 6);
        const auto& selected = m_context.playerParty[m_selectedCapturePartyChoice];
        m_battleFeedback = std::string("Replace: ") + (selected.localizedName ? selected.localizedName : "Pokemon") + " A: replace B: decline";
        buildScene();
        return true;
    }
    if (moveLearningPending()) {
        if (!direction) return false;
        m_selectedBattleMove = static_cast<uint8_t>((m_selectedBattleMove + (direction > 0 ? 1 : 3)) % 4);
        m_battleFeedback = "Choose move to replace: UP/DOWN, A learn, B reject";
        buildScene();
        return true;
    }
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
    if (m_capturePartyChoicePending) return false;
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

bool FirstRunRuntime::grantVictoryExperience(bool pokemonDefeated, uint8_t enemyMask) {
    if (!m_context.playerPartyCount || m_context.playerPartyCount > 6 ||
        m_context.activePlayerPartyIndex >= m_context.playerPartyCount ||
        moveLearningPending() || evolutionPending()) return false;
    if (m_experienceGranted || !m_context.player.actorIdentityResolved ||
        !m_context.enemy.actorIdentityResolved) return false;
    if (m_doubleBattle && !m_secondEncounterResolved) return false;
    if (m_doubleBattle) {
        if (!enemyMask) enemyMask = pokemonPendingDoubleExperienceMask(!m_context.enemy.battleState.hp,
            !m_context.secondEnemy.battleState.hp, m_doubleExperienceGrantedMask);
        if (enemyMask > 3 || (enemyMask & m_doubleExperienceGrantedMask)) return false;
        if (!enemyMask) { m_experienceGranted = true; return true; }
    } else enemyMask = 1;
    if (!m_participantHistoryResolved && m_context.playerPartyCount > 1) {
        m_battleFeedback = "Party EXP requires persisted participant history";
        return false;
    }
    const auto* starter = PokerogueContent::findSpeciesByDex(m_context.player.dex);
    const auto& defeatedActor = enemyMask == 2 ? m_context.secondEnemy : m_context.enemy;
    const auto* defeated = PokerogueContent::findSpeciesByDex(defeatedActor.dex);
    const auto* defeatedForm = defeatedActor.formId ? PokerogueContent::findFormById(defeatedActor.formId) : nullptr;
    if (!starter || !defeated || (defeatedActor.formId && !defeatedForm)) return false;
    double rawExperience = 0.0;
    if (pokemonExperienceForDefeat(*defeated, defeatedActor.level, rawExperience,
                                  defeatedForm) != PokemonExperienceResult::Ok ||
        rawExperience < 0.0 || rawExperience > 4294967295.0) return false;
    std::array<ResolvedPokemon, 6> nextParty{};
    for (uint8_t member = 0; member < 6; ++member) nextParty[member] = m_context.playerParty[member];
    // Catalog-sized ledger must not sit beside actor arrays on the ARM11 stack.
    std::unique_ptr<decltype(m_starterProfileRecords)> nextProfileStorage(
        new (std::nothrow) decltype(m_starterProfileRecords)(m_starterProfileRecords));
    if (!nextProfileStorage) {
        m_battleFeedback = "EXP profile allocation failed";
        return false;
    }
    auto& nextProfile = *nextProfileStorage;
    size_t nextProfileCount = m_starterProfileCount;
    nextParty[m_context.activePlayerPartyIndex] = m_context.player;
    std::array<PokemonPendingLevelMoves, 6> pendingMoves{};
    std::array<const char*, 6> pendingEvolutions{};
    std::array<uint8_t, 6> pendingMembers{};
    uint8_t pendingCount = 0;
    // Legacy checkpoints retain the previous single-actor path; new histories
    // resolve identity membership rather than granting all EXP to the active actor.
    const uint8_t participants = m_participantHistoryResolved ? m_participantCount : 1;
    for (uint8_t member = 0; member < m_context.playerPartyCount; ++member) {
        auto& target = nextParty[member];
        bool participated = !m_participantHistoryResolved && member == m_context.activePlayerPartyIndex;
        for (uint8_t i = 0; i < m_participantCount; ++i)
            participated |= m_participantIds[i] == target.battleState.pokemonId;
        PokemonParticipantExperiencePolicy policy{};
        policy.resolved = true; // Current held frontier excludes EXP modifiers/Pokerus.
        policy.participantCount = participants;
        policy.participated = participated;
        policy.eligible = target.battleState.hp && target.level < classicExperienceLevelCap(m_run.wave);
        uint32_t memberAward = 0;
        if (pokemonParticipantExperience(rawExperience, m_trainerBattle, policy, memberAward) !=
                PokemonExperienceResult::Ok) return false;
        if (m_doubleBattle && enemyMask == 3) {
            const auto* species2 = PokerogueContent::findSpeciesByDex(m_context.secondEnemy.dex);
            const auto* form2 = m_context.secondEnemy.formId
                ? PokerogueContent::findFormById(m_context.secondEnemy.formId) : nullptr;
            double raw2 = 0.0;
            uint32_t award2 = 0;
            if (!species2 || pokemonExperienceForDefeat(*species2, m_context.secondEnemy.level, raw2, form2) !=
                    PokemonExperienceResult::Ok || pokemonParticipantExperience(raw2, false, policy, award2) !=
                    PokemonExperienceResult::Ok || award2 > 0xffffffffU - memberAward) return false;
            memberAward += award2;
        }
        const uint8_t friendshipDefeats = pokemonDefeated ? (enemyMask == 3 ? 2 : 1) : 0;
        if (m_starterProfileReady && friendshipDefeats && participated && target.battleState.hp) {
            const auto* root = pokemonRootSpecies(target.dex);
            if (!root) return false;
            size_t record = 0;
            while (record < nextProfileCount && nextProfile[record].speciesDex < root->dex) ++record;
            if (record == nextProfileCount || nextProfile[record].speciesDex != root->dex) {
                if (nextProfileCount == nextProfile.size()) return false;
                for (size_t i = nextProfileCount; i > record; --i) nextProfile[i] = nextProfile[i - 1];
                nextProfile[record] = {root->dex, 0, 0};
                ++nextProfileCount;
            }
            for (uint8_t defeat = 0; defeat < friendshipDefeats; ++defeat) {
                StarterCandyAwardEvent event{};
                if (applyNativePokemonFriendship(target.battleState, nextProfile[record],
                        PokerogueContent::kFriendshipGainFromBattle, m_starterFriendshipPolicy, false, event) !=
                        NativeFriendshipApplyResult::Applied) {
                    m_battleFeedback = "Friendship requires resolved root/profile/maximum callbacks";
                    return false;
                }
            }
        }
        if (!memberAward) continue;
        const auto* species = PokerogueContent::findSpeciesByDex(target.dex);
        if (!species || !target.actorIdentityResolved) return false;
        PokemonExperienceProgress progress{};
        if (applyPokemonExperience(species->growthRate, target.level, target.totalExperience,
                memberAward, classicExperienceLevelCap(m_run.wave), progress) != PokemonExperienceResult::Ok)
            return false;
        const uint16_t oldLevel = target.level;
        if (progress.level != oldLevel) {
            if (!recalculatePokemonBattleLevel(target.battleState, progress.level)) return false;
            PokemonPendingLevelMoves moves{};
            learnNewLevelMoves(target.dex, oldLevel, progress.level, target.battleState,
                target.moveIds, target.moveCount, nullptr, &moves);
            if (moves.overflow) return false;
            const auto* evo = checkSpeciesLevelEvolution(target.speciesId, oldLevel, progress.level);
            const char* evolution = evo && !target.battleState.pauseEvolutions ? evo->targetSpeciesId : nullptr;
            if (moves.count || evolution) {
                pendingMembers[pendingCount] = member;
                pendingMoves[pendingCount] = moves;
                pendingEvolutions[pendingCount++] = evolution;
            }
        }
        target.level = progress.level;
        target.totalExperience = progress.totalExperience;
    }
    m_starterProfileRecords = nextProfile;
    m_starterProfileCount = nextProfileCount;
    for (uint8_t member = 0; member < 6; ++member) m_context.playerParty[member] = nextParty[member];
    m_context.player = nextParty[m_context.activePlayerPartyIndex];
    m_progressionQueueMembers = pendingMembers;
    m_progressionQueueMoves = pendingMoves;
    m_progressionQueueEvolutions = pendingEvolutions;
    m_progressionQueueCount = pendingCount;
    m_progressionQueueCursor = 0;
    m_progressionPartyIndex = 0xFF;
    m_pendingLevelMoves = {};
    m_pendingEvolutionSpeciesId = nullptr;
    advanceProgressionQueue();
    if (m_context.playerPartyCount > 1) m_playerHistoryRequiresSnapshot = true;
    if (m_doubleBattle) m_doubleExperienceGrantedMask |= enemyMask;
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
        scores[candidateCount] = scoreActiveEnemyMove(enemyState, playerState, *move);
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
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime transaction allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
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
    if (m_capturePartyChoicePending) return resolveCapturePartyChoiceInPlace(m_selectedCapturePartyChoice);
    if (!heldHealingInventorySupported(m_heldModifiers.data(), m_heldModifierCount)) {
        m_battleFeedback = "Held modifier effects require native dispatch";
        return false;
    }
    if (moveLearningPending()) return resolvePendingLearnMove(m_selectedBattleMove);
    if (m_evolutionPauseConfirmation) {
        progressionPokemonMutable().battleState.pauseEvolutions = true;
        if (m_progressionPartyIndex == 0xFF || m_progressionPartyIndex == m_context.activePlayerPartyIndex)
            m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;
        m_evolutionPauseConfirmation = false;
        advanceProgressionQueue();
        m_battleFeedback = "Future evolutions paused";
        buildScene();
        return true;
    }
    if (evolutionPending()) {
        if (!finishPendingEvolution(true)) return false;
        buildScene();
        return true;
    }
    if (m_rewardsPending) {
        return claimRewardChoice();
    }
    if (m_battleFinished && m_playerWon && (!m_experienceGranted || (m_trainerBattle && !enemyPartyDefeated()))) {
        if (!m_experienceGranted && !grantVictoryExperience()) {
            m_battleFeedback = "Victory experience could not be resolved";
            buildScene();
            return false;
        }
        if (moveLearningPending() || evolutionPending()) {
            m_battleFeedback = moveLearningPending() ? "Choose move to replace: UP/DOWN, A learn, B reject"
                : "Evolution ready: A continue, B cancel";
            buildScene();
            return true;
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

    if (!recordActiveParticipant()) return false;

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
                if (incoming.battleState.confusion.present) {
                    PokemonConfusionRemovalEvent incomingConfusion{};
                    if (applyPokemonPostSummonConfusionRemoval(incoming.battleState, true, true, incomingConfusion) !=
                            PokemonStatusImmunityResult::Resolved) return false;
                }
                resetPokemonSummonState(incoming.battleState);
                PokemonPostSummonStatusHealingEvent incomingStatus{};
                if (!applyPokemonPostSummonStatusHealing(incoming.battleState, true, true, incomingStatus)) return false;
                PokerogueRngAdapter actionRng = *rng;
                // Resolve abilities against the incoming actor. A rejected command
                // leaves player HP/PP, field state and RNG untouched.
                m_context.enemy = incoming;
                if (!executeActiveBattleMove(false, m_selectedBattleMove, actionRng)) {
                    m_context.enemy = outgoing;
                    return false;
                }
                auto recalled = outgoing;
                resetPokemonSummonState(recalled.battleState);
                m_context.trainerParty[m_context.activeTrainerPartyIndex] = recalled;
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
                scores[candidateCount] = scoreActiveEnemyMove(m_context.enemy.battleState, playerState, *move);
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
                scores[candidateCount] = scoreActiveEnemyMove(m_context.secondEnemy.battleState, playerState, *move);
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

    bool thawAfterFailureChecks = false;
    if (user.status.present && (user.status.effect == PokemonStatusEffect::Sleep ||
            user.status.effect == PokemonStatusEffect::Freeze)) {
        // Area attacks visit each target; status checks belong to one MovePhase,
        // so doubles require the shared action dispatcher before enabling this.
        if (m_doubleBattle || PokerogueContent::moveHasAttribute(*move, "BypassSleepAttr") ||
            (PokerogueContent::moveHasAttribute(*move, "HealStatusEffectAttr") &&
             !pokemonMoveSelfThawResolved(move->id))) {
            m_battleFeedback = "Status move-use conditions require dispatcher";
            return false;
        }
        PokemonStatusMoveCheckPolicy statusPolicy{};
        statusPolicy.resolved = true;
        statusPolicy.deferredFreezeThawMove = pokemonMoveSelfThawResolved(move->id);
        if (user.status.effect == PokemonStatusEffect::Sleep) {
            statusPolicy.resolved = false;
            for (const auto& profile : PokerogueContent::kStatusDurationAbilityProfiles)
                if (profile.abilityId == user.abilityId) {
                    statusPolicy.resolved = profile.resolved;
                    statusPolicy.sleepDurationReduction = profile.sleepReduction;
                    break;
                }
        }
        // Current gated actor path has no Nightmare tags or indirect
        // use modes. Unknown ability conditions fail before publishing the turn.
        PokemonStatusMoveCheckEvent statusEvent{};
        if (checkPokemonStatusBeforeMove(user.status, statusPolicy, rng, statusEvent) !=
                PokemonStatusMoveCheckResult::Ok) {
            m_battleFeedback = "Status callbacks require dispatcher";
            return false;
        }
        thawAfterFailureChecks = statusEvent.thawAfterFailureChecks;
        if (statusEvent.cancelled) {
            m_battleFeedback = "Status prevented the move";
            return true; // First failure check cancels without consuming PP.
        }
    }
    if (user.confusion.present || user.confusion.turns) {
        // Status-action capability admits no ATK/DEF multipliers or indirect
        // damage callbacks. Its EVA-only bypass does not affect self-hit stats.
        if (m_doubleBattle || m_heldModifierCount || m_arenaWeather.type != PokemonEffectiveWeather::None ||
            !statusActionAbilitySupported(user.abilityId) || !statusActionAbilitySupported(opponent.abilityId)) {
            m_battleFeedback = "Confusion stat/damage callbacks require dispatcher";
            return false;
        }
        ResolvedPokemon* resolvedActors[] = {&m_context.player, &m_context.enemy, &m_context.secondEnemy};
        if (resolvedActors[userIndex]->bossState.segmentCount) {
            m_battleFeedback = "Confusion boss damage requires dispatcher";
            return false;
        }
        uint32_t attack = 0, defense = 0;
        if (!pokemonBaselineEffectiveStat(user, 1, false, attack) ||
            !pokemonBaselineEffectiveStat(user, 2, false, defense)) return false;
        PokemonConfusionMovePolicy confusionPolicy{};
        confusionPolicy.resolved = true;
        confusionPolicy.effectiveAttack = attack;
        confusionPolicy.effectiveDefense = defense;
        PokemonConfusionMoveEvent confusionEvent{};
        if (!checkPokemonConfusionBeforeMove(user, user.confusion, confusionPolicy, rng, confusionEvent)) return false;
        if (confusionEvent.moveCancelled) {
            m_battleFeedback = "Confusion prevented the move";
            return true;
        }
    }
    if (user.status.present && user.status.effect == PokemonStatusEffect::Paralysis) {
        if (m_doubleBattle) {
            m_battleFeedback = "Status move-use conditions require dispatcher";
            return false;
        }
        PokemonStatusMoveCheckPolicy paralysisPolicy{};
        paralysisPolicy.resolved = true;
        PokemonStatusMoveCheckEvent paralysisEvent{};
        if (checkPokemonStatusBeforeMove(user.status, paralysisPolicy, rng, paralysisEvent) !=
                PokemonStatusMoveCheckResult::Ok) return false;
        if (paralysisEvent.cancelled) {
            m_battleFeedback = "Status prevented the move";
            return true;
        }
    }
    if (thawAfterFailureChecks) {
        // MovePhase.doThawCheck follows confusion and PP checks, before accuracy.
        if (!user.moves[moveSlot].pp || user.moves[moveSlot].pp > user.moves[moveSlot].maxPp)
            return false;
        user.status = {};
    }
    const auto applyMoveHeldHealing = [this](PokemonBattleState& actor) {
        PokemonHealingPolicy policy{};
        policy.resolved = true; // Current gated frontier has no Heal Block/Healing Charms.
        PokemonHealingEvent event{};
        return applyHeldMoveHealingPhase(m_heldModifiers.data(), m_heldModifierCount,
            actor, actor.hp != 0, policy, event) == HeldHealingResult::Resolved;
    };
    if (selfHealingProfile(move->id)) {
        PokemonHealingPolicy policy{};
        policy.resolved = true;
        PokemonHealingEvent event{};
        if (usePokemonSelfHealingCommand(user, moveSlot, policy, event) != PokemonHealingResult::Ok)
            return false;
        if (!applyMoveHeldHealing(user)) return false;
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
        if (!applyMoveHeldHealing(user)) return false;
        m_battleFeedback = event.changed ? "Weather changed" : "Weather move failed";
        return true;
    }
    if (singleStatusConfusionEffect(move->id)) {
        if (!user.moves[moveSlot].pp || user.moves[moveSlot].pp > user.moves[moveSlot].maxPp) return false;
        PokemonStatusEffectCommandPolicy command{};
        PokemonPostSetStatusPolicy unusedReactions{};
        PokemonStatusRecipientPolicies recipient{};
        if (!resolveActiveStatusCommandPolicies(user, opponent, move->id, pp.cost, command, unusedReactions) ||
            !resolveActiveStatusRecipientPolicies(opponent, user, PokemonStatusEffect::None, recipient)) return false;
        auto nextUser = user, nextOpponent = opponent;
        auto nextRng = rng;
        PokemonStatusMoveHitEvent hitEvent{};
        PokemonMoveConfusionEvent confusionEvent{};
        if (!resolvePokemonStatusMoveHit(*move, false, command.move.hit, nextRng, hitEvent)) return false;
        if (hitEvent.hit && !applyPokemonMoveConfusion(nextOpponent, move->id, command.move.effectiveChance,
                recipient.status.safeguardBlocks, recipient.confusion, nextRng, confusionEvent, nextUser.pokemonId, true)) return false;
        const uint8_t consumed = pp.cost < nextUser.moves[moveSlot].pp ? pp.cost : nextUser.moves[moveSlot].pp;
        nextUser.moves[moveSlot].pp -= consumed;
        if (!applyMoveHeldHealing(nextUser)) return false;
        user = nextUser;
        opponent = nextOpponent;
        rng = nextRng;
        m_battleFeedback = !hitEvent.hit ? "Confusion move missed or blocked" :
            confusionEvent.tagAttempted && confusionEvent.tagResult == PokemonConfusionTagResult::Added
                ? "Target confused" : "Confusion unchanged";
        return true;
    }
    if (singleOpponentStatusEffect(move->id)) {
        PokemonStatusEffectCommandPolicy command{};
        PokemonPostSetStatusPolicy reactions{};
        if (!resolveActiveStatusCommandPolicies(user, opponent, move->id, pp.cost, command, reactions)) return false;
        PokemonStatusActionEvent event{};
        // Pokemon.randBattleSeedInt delegates to currentBattle: duration and
        // post-set reactions consume the same stream as accuracy/chance.
        if (!executePokemonStatusAction(user, opponent, moveSlot, command, reactions, rng, rng, event)) return false;
        if (!applyMoveHeldHealing(user)) return false;
        m_battleFeedback = !event.move.hit.hit ? "Status move missed or blocked" :
            event.reactionsExecuted ? "Status applied" : "Status unchanged";
        return true;
    }
    if (supportsPokemonStatStageMove(move->id)) {
        PokemonStatStageCommandPolicy policy{};
        if (!resolveActiveStatStageCommandPolicy(user, opponent, move->id, pp.cost, policy)) return false;
        PokemonStatStageCommandEvent event{};
        if (usePokemonStatStageStatusCommand(user, opponent, moveSlot, policy, rng, event) !=
                PokemonStatStageEffectResult::Ok) return false;
        if (!applyMoveHeldHealing(user)) return false;
        if (!event.move.hit) {
            m_battleFeedback = event.move.typeImmune ? "Target immune to move" :
                policy.move.blockedBeforeAccuracy ? "Move blocked by ability" : "Move missed";
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
        if (!applyMoveHeldHealing(user)) return false;
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
    for (const auto& profile : PokerogueContent::kStatusActionAbilityProfiles) {
        if (!profile.resolved) continue;
        if (profile.abilityId == user.abilityId) hit.ignoreDefenderEvasionStage = profile.ignoresOpponentEvasion;
        if (profile.abilityId == opponent.abilityId) hit.ignoreAttackerAccuracyStage = profile.ignoresOpponentAccuracy;
    }
    ResolvedPokemon* resolvedActors[] = {&m_context.player, &m_context.enemy, &m_context.secondEnemy};
    auto* targetBossState = &resolvedActors[targetIndex]->bossState;
    auto nextBossState = *targetBossState;
    auto nextGlobalRng = m_globalRng;
    const bool targetIsBoss = nextBossState.segmentCount != 0;
    PokemonBossDamagePolicy bossPolicy{};
    const auto* bossAbility = PokerogueContent::findAbilityMovegenProfile(opponent.abilityId);
    bossPolicy.resolved = bossPolicy.damageCallbacksResolved =
        bossAbility && bossAbility->bossDamageCallbacksResolved;
    if (targetIsBoss && !bossPolicy.resolved) {
        m_battleFeedback = "Boss damage callbacks require dispatcher";
        return false;
    }
    PokemonBurnDamagePolicy burn{};
    if (user.status.present && user.status.effect == PokemonStatusEffect::Burn &&
        move->category == PokerogueContent::MovePhysical) {
        bool callbacksResolved = false;
        for (const auto& profile : PokerogueContent::kBurnAbilityCallbacks)
            if (profile.abilityId == user.abilityId) { callbacksResolved = profile.resolved; break; }
        // Current native actors use unsuppressed primary abilities; passive and
        // ability-ignore move flags remain outside this supported move path.
        if (!resolvePokemonBurnDamagePolicy(user.abilityId, callbacksResolved, true, false, burn)) {
            m_battleFeedback = "Burn ability callbacks require dispatcher";
            return false;
        }
    }
    if (damageRecoilProfile(move->id)) {
        auto nextUser = user;
        auto nextOpponent = opponent;
        auto nextRng = rng;
        PokemonMoveActionResult attack{};
        if (useStandardPokemonMove(nextUser, nextOpponent, moveSlot, false, nextRng,
            attack, &weather, &critical, &hit, &pp, targetIsBoss ? &nextBossState : nullptr,
            targetIsBoss ? &bossPolicy : nullptr,
            targetIsBoss ? &nextGlobalRng : nullptr, &burn) != PokemonMoveActionStatus::Ok) return false;
        const auto policy = canonicalFreshActorRecoilPolicy(user.abilityId);
        PokemonRecoilEvent recoil{};
        if (applyPokemonRecoil(nextUser, move->id, attack.damageApplied,
            attack.damageRoll.hit && !attack.weatherCancelled, policy, recoil) != PokemonRecoilResult::Ok)
            return false;
        if (!attack.weatherCancelled && !applyMoveHeldHealing(nextUser)) return false;
        user = nextUser;
        opponent = nextOpponent;
        rng = nextRng;
        if (targetIsBoss) {
            *targetBossState = nextBossState;
            m_globalRng = nextGlobalRng;
        }
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
            targetIsBoss ? &bossPolicy : nullptr,
            targetIsBoss ? &nextGlobalRng : nullptr, &burn) != PokemonMoveActionStatus::Ok) return false;
        PokemonDrainPolicy policy{};
        policy.resolved = true;
        PokemonDrainEvent event{};
        if (attack.damageRoll.hit && !attack.weatherCancelled && attack.damageApplied &&
            applyPokemonDamageDrain(nextUser, move->id, attack.damageApplied, policy, event) !=
                PokemonHealingResult::Ok) return false;
        if (!attack.weatherCancelled && !applyMoveHeldHealing(nextUser)) return false;
        user = nextUser;
        opponent = nextOpponent;
        rng = nextRng;
        if (targetIsBoss) {
            *targetBossState = nextBossState;
            m_globalRng = nextGlobalRng;
        }
        m_battleFeedback = event.healed ? "Attack drained HP" :
            attack.weatherCancelled ? "Move blocked by weather" :
            attack.damageRoll.hit ? "Attack hit" : "Attack missed";
        return true;
    }
    auto nextUser = user;
    auto nextOpponent = opponent;
    auto nextRng = rng;
    PokemonMoveActionResult result{};
    if (useStandardPokemonMove(nextUser, nextOpponent, moveSlot, false, nextRng, result,
            &weather, &critical, &hit, &pp, targetIsBoss ? &nextBossState : nullptr,
            targetIsBoss ? &bossPolicy : nullptr,
            targetIsBoss ? &nextGlobalRng : nullptr, &burn) != PokemonMoveActionStatus::Ok) return false;
    if (PokerogueContent::moveHasAttribute(*move, "RecoilAttr")) {
        PokemonRecoilEvent recoil{};
        const auto recoilPolicy = canonicalFreshActorRecoilPolicy(nextUser.abilityId);
        if (applyPokemonRecoil(nextUser, move->id, result.damageApplied,
                !result.weatherCancelled && result.damageRoll.hit, recoilPolicy, recoil) !=
                PokemonRecoilResult::Ok) return false;
    }
    bool targetThawed = false;
    if (!applyPokemonMoveTargetThaw(nextOpponent, move->id,
            !result.weatherCancelled && result.damageRoll.hit && result.damageApplied,
            targetThawed)) return false;
    if (const auto* effect = singleDamageStatusEffect(move->id)) {
        PokemonStatusRecipientPolicies recipient{}, source{};
        PokemonPostSetStatusPolicy reactions{};
        const auto status = static_cast<PokemonStatusEffect>(effect->effectId);
        if (!resolveActiveStatusRecipientPolicies(nextOpponent, nextUser, status, recipient) ||
            !resolveActiveStatusRecipientPolicies(nextUser, nextOpponent, status, source) ||
            !resolvePokemonPostSetStatusPolicy(nextOpponent, nextUser, status, recipient, source,
                nextOpponent.hp != 0, nextUser.hp != 0, true, reactions)) return false;
        if (!result.weatherCancelled && result.damageRoll.hit && result.damageApplied) {
            PokemonMoveStatusPhaseEvent statusEvent{};
            int16_t chance = 0;
            if (!resolvePokemonMoveEffectChance(move->id, nextUser.abilityId, nextOpponent.abilityId, false, chance,
                    nextUser.hp != 0, nextOpponent.hp != 0)) return false;
            if (!executePokemonMoveStatusPhase(nextUser, nextOpponent, move->id, chance,
                    recipient.status, reactions, nextRng, statusEvent)) return false;
        }
    }
    if (const auto* effect = singleDamageStatStageEffect(move->id)) {
        PokemonStatStageCommandPolicy policy{};
        if (!resolveActiveStatStageCommandPolicy(nextUser, nextOpponent, move->id, pp.cost, policy)) return false;
        if (!result.weatherCancelled && result.damageRoll.hit && result.damageApplied) {
            PokemonStatStageCommandEvent event{};
            if (executePokemonDamageStatStagePhase(nextUser, nextOpponent, *effect, policy, nextRng, event) !=
                PokemonStatStageEffectResult::Ok) return false;
        }
    }
    if (singleDamageConfusionEffect(move->id) && !result.weatherCancelled &&
            result.damageRoll.hit && result.damageApplied) {
        PokemonStatusRecipientPolicies recipient{};
        int16_t chance = 0;
        PokemonMoveConfusionEvent confusionEvent{};
        if (!resolveActiveStatusRecipientPolicies(nextOpponent, nextUser, PokemonStatusEffect::None, recipient) ||
            !resolvePokemonMoveEffectChance(move->id, nextUser.abilityId, nextOpponent.abilityId, false, chance,
                    nextUser.hp != 0, nextOpponent.hp != 0) ||
            !applyPokemonMoveConfusion(nextOpponent, move->id, chance, recipient.status.safeguardBlocks,
                recipient.confusion, nextRng, confusionEvent, nextUser.pokemonId, true)) return false;
    }
    if (!result.weatherCancelled && !applyMoveHeldHealing(nextUser)) return false;
    user = nextUser;
    opponent = nextOpponent;
    rng = nextRng;
    if (targetIsBoss) {
        *targetBossState = nextBossState;
        m_globalRng = nextGlobalRng;
    }
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

bool FirstRunRuntime::recordActiveParticipant() {
    if (!m_participantHistoryResolved) return true;
    const uint32_t id = m_context.player.battleState.pokemonId;
    uint8_t position = 0;
    while (position < m_participantCount && m_participantIds[position] < id) ++position;
    if (position < m_participantCount && m_participantIds[position] == id) return true;
    if (m_participantCount == m_participantIds.size()) return false;
    for (uint8_t i = m_participantCount; i > position; --i) m_participantIds[i] = m_participantIds[i - 1];
    m_participantIds[position] = id;
    ++m_participantCount;
    return true;
}

void FirstRunRuntime::removeParticipant(uint32_t id) {
    for (uint8_t i = 0; i < m_participantCount; ++i) {
        if (m_participantIds[i] != id) continue;
        for (uint8_t n = i + 1; n < m_participantCount; ++n) m_participantIds[n - 1] = m_participantIds[n];
        m_participantIds[--m_participantCount] = 0;
        return;
    }
}

bool FirstRunRuntime::finishBattleTurn() {
    if (m_context.player.battleState.pendingStatus != PokemonStatusEffect::None ||
        m_context.enemy.battleState.pendingStatus != PokemonStatusEffect::None ||
        (m_doubleBattle && m_context.secondEnemy.battleState.pendingStatus != PokemonStatusEffect::None)) {
        m_battleFeedback = "Pending status phase must finish before turn end";
        return false;
    }
    // TurnEndPhase lapses arena tags except during a biome interlude.
    // Current checkpoint progression ends before the first X0 transition.
    // Resolve both field clocks before committing either. TurnEndPhase lapses
    // weather even during an interlude; arena tags have a separate interlude gate.
    // Actors requiring unported weather callbacks remain gated before a command.
    auto nextPlayer = m_context.player.battleState;
    auto nextEnemy = m_context.enemy.battleState;
    auto nextSecondEnemy = m_context.secondEnemy.battleState;
    auto nextEnemyBoss = m_context.enemy.bossState;
    auto nextSecondEnemyBoss = m_context.secondEnemy.bossState;
    auto nextGlobalRng = m_globalRng;
    PokemonWeatherPhaseEvent residual{};
    const bool allEnemiesDown = m_doubleBattle
        ? (!nextEnemy.hp && !nextSecondEnemy.hp)
        : (!nextEnemy.hp);
    const bool upcomingInterlude = m_run.wave < PokerogueContent::kClassicFinalWave &&
        m_run.wave % 10 == 0 && nextPlayer.hp &&
        allEnemiesDown && enemyPartyDefeated();
    if (!applyPokemonMultiWeatherPhase(nextPlayer, nextEnemy, m_doubleBattle ? &nextSecondEnemy : nullptr,
            m_arenaWeather, upcomingInterlude, residual,
            nextEnemyBoss.segmentCount ? &nextEnemyBoss : nullptr,
            m_doubleBattle && nextSecondEnemyBoss.segmentCount ? &nextSecondEnemyBoss : nullptr,
            &nextGlobalRng)) {
        m_battleFeedback = "Weather effects require ability dispatcher";
        buildScene();
        return false;
    }
    // phase-manager.ts: WeatherEffect -> CheckStatusEffect -> TurnEnd.
    // Positional tags and berry dispatch remain outside the gated frontier.
    if (!upcomingInterlude) {
        PokemonBattleState* statusActors[] = {&nextPlayer, &nextEnemy, &nextSecondEnemy};
        const uint8_t actorCount = m_doubleBattle ? 3 : 2;
        for (uint8_t i = 0; i < actorCount; ++i) {
            auto& actor = *statusActors[i];
            if (!actor.hp || !pokemonStatusIsPostTurn(actor.status)) continue;
            PokemonStatusResidualPolicy policy{};
            // Resolve primary ability attributes from pinned canonical metadata.
            // Passive/suppression dispatch remains outside this gated actor path.
            if (!resolvePokemonStatusResidualPolicy(actor.abilityId, actor.status.effect, true, true, policy)) {
                m_battleFeedback = "Status residual ability policy is unresolved";
                return false;
            }
            // PostDamage can affect other actors; doubles need phase ordering
            // and the shared callback dispatcher before enabling residuals.
            policy.bossDamageNeedsDispatcher = m_doubleBattle ||
                (i == 1 && nextEnemyBoss.segmentCount) ||
                (i == 2 && nextSecondEnemyBoss.segmentCount) ||
                m_run.wave == PokerogueContent::kClassicFinalWave;
            PokemonStatusResidualEvent statusEvent{};
            const auto result = applyPokemonStatusResidual(actor, policy, statusEvent);
            if (result != PokemonStatusResidualResult::Applied && result != PokemonStatusResidualResult::Blocked &&
                result != PokemonStatusResidualResult::NoEffect) {
                m_battleFeedback = "Status residual damage requires ability dispatcher";
                return false;
            }
        }
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
    // Current command frontier excludes unported tags/Healing Charms.
    // Do not infer this policy when that frontier is expanded.
    if (!upcomingInterlude && m_heldModifierCount) {
        PokemonHealingPolicy healing{};
        healing.resolved = true;
        PokemonHealingEvent heldEvent{};
        if (applyHeldTurnHealingPhase(m_heldModifiers.data(), m_heldModifierCount,
                nextPlayer, nextPlayer.hp != 0, healing, heldEvent) != HeldHealingResult::Resolved ||
            applyHeldTurnHealingPhase(m_heldModifiers.data(), m_heldModifierCount,
                nextEnemy, nextEnemy.hp != 0, healing, heldEvent) != HeldHealingResult::Resolved ||
            (m_doubleBattle && applyHeldTurnHealingPhase(m_heldModifiers.data(), m_heldModifierCount,
                nextSecondEnemy, nextSecondEnemy.hp != 0, healing, heldEvent) != HeldHealingResult::Resolved)) {
            m_battleFeedback = "Held turn healing could not resolve";
            return false;
        }
    }
    if (!upcomingInterlude) {
        PokemonBattleState* healActors[] = {&nextPlayer, &nextEnemy, &nextSecondEnemy};
        for (uint8_t i = 0; i < (m_doubleBattle ? 3 : 2); ++i) {
            auto& actor = *healActors[i];
            bool statusHealingDeclared = false;
            for (const auto& profile : PokerogueContent::kStatusResidualAbilityProfiles)
                if (profile.abilityId == actor.abilityId) { statusHealingDeclared = profile.healedStatusMask != 0; break; }
            if (!actor.hp || !statusHealingDeclared) continue;
            PokemonHealingPolicy healing{};
            healing.resolved = true; // Current frontier excludes Heal Block/Healing Charms.
            PokemonHealingEvent event{};
            if (m_doubleBattle || applyPokemonPostTurnStatusHealing(actor, true, healing, event) != PokemonHealingResult::Ok) {
                m_battleFeedback = "Post-turn status healing requires dispatcher";
                return false;
            }
        }
    }
    // Reset PokemonTurnData after all end-of-turn consumers.
    if ((!nextPlayer.hp || !nextEnemy.hp || (m_doubleBattle && !nextSecondEnemy.hp)) &&
        !recordActiveParticipant()) return false;
    if (!nextPlayer.hp) {
        if (!applyPokemonFaintFriendship(nextPlayer)) return false;
        removeParticipant(nextPlayer.pokemonId);
    }
    nextPlayer.turnDamageDealt = nextEnemy.turnDamageDealt = nextSecondEnemy.turnDamageDealt = 0;
    m_context.player.battleState = nextPlayer;
    m_context.enemy.battleState = nextEnemy;
    m_context.enemy.bossState = nextEnemyBoss;
    if (m_doubleBattle) {
        m_context.secondEnemy.battleState = nextSecondEnemy;
        m_context.secondEnemy.bossState = nextSecondEnemyBoss;
    }
    m_globalRng = nextGlobalRng;
    m_trickRoom = nextRoom;
    m_arenaWeather = nextWeather;
    m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;
    const bool playerDown = !m_context.player.battleState.hp;
    const bool enemiesDownNow = m_doubleBattle
        ? (!m_context.enemy.battleState.hp && !m_context.secondEnemy.battleState.hp)
        : (!m_context.enemy.battleState.hp);
    const auto conclusion = pokemonBattleConclusion(playerPartyDefeated(), enemiesDownNow);
    if (m_doubleBattle && !enemiesDownNow && conclusion != PokemonBattleConclusion::PlayerDefeat) {
        const uint8_t pending = pokemonPendingDoubleExperienceMask(!m_context.enemy.battleState.hp,
            !m_context.secondEnemy.battleState.hp, m_doubleExperienceGrantedMask);
        if (pending) {
            if (!grantVictoryExperience(true, pending)) return false;
            // An intermediate enemy defeat does not complete battle rewards.
            // Pending level decisions are processed before the next combat command.
            m_experienceGranted = false;
        }
    }
    if (playerDown && conclusion != PokemonBattleConclusion::PlayerDefeat) {
        // A legal reserve prevents GameOver. If the last enemy also fainted,
        // resolve victory rather than starting another turn against a dead field.
        if (!advancePlayerAfterDefeat()) return false;
    }
    if (conclusion != PokemonBattleConclusion::Continue) {
        m_battleFinished = true;
        m_playerWon = conclusion == PokemonBattleConclusion::PlayerVictory;
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
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime transaction allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
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
    if (m_capturePartyChoicePending) return resolveCapturePartyChoiceInPlace(-1);
    if (!heldHealingInventorySupported(m_heldModifiers.data(), m_heldModifierCount)) {
        m_battleFeedback = "Held modifier effects require native dispatch";
        return false;
    }
    if (moveLearningPending()) return resolvePendingLearnMove(-1);
    if (m_evolutionPauseConfirmation) {
        m_evolutionPauseConfirmation = false;
        advanceProgressionQueue();
        m_battleFeedback = "Future evolutions remain enabled";
        buildScene();
        return true;
    }
    if (evolutionPending()) {
        m_playerHistoryRequiresSnapshot = true;
        m_pendingEvolutionSpeciesId = nullptr;
        m_evolutionPauseConfirmation = true;
        m_battleFeedback = "Evolution cancelled: pause future evolutions?";
        buildScene();
        return true;
    }
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
    if (m_context.enemy.battleState.confusion.present) {
        PokemonConfusionRemovalEvent incomingConfusion{};
        if (applyPokemonPostSummonConfusionRemoval(m_context.enemy.battleState, true, true, incomingConfusion) !=
                PokemonStatusImmunityResult::Resolved) return false;
    }
    resetPokemonSummonState(m_context.enemy.battleState);
    PokemonPostSummonStatusHealingEvent incomingStatus{};
    if (!applyPokemonPostSummonStatusHealing(m_context.enemy.battleState, true, true, incomingStatus)) return false;
    m_run.encounterDex = m_context.enemy.dex;
    m_battleRng = nextTurn;
    ++m_turn;
    m_battleFinished = false;
    m_playerWon = false;
    m_experienceGranted = false;
    m_progressionQueueCount = m_progressionQueueCursor = 0;
    m_progressionPartyIndex = 0xFF;
    m_pendingLevelMoves = {};
    m_pendingEvolutionSpeciesId = nullptr;
    m_evolutionPauseConfirmation = false;
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
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime transaction allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
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
    if (m_capturePartyChoicePending) return false;
    if (!heldHealingInventorySupported(m_heldModifiers.data(), m_heldModifierCount)) {
        m_battleFeedback = "Held modifier effects require native dispatch";
        return false;
    }
    if (moveLearningPending() || evolutionPending()) return false;
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

    if (m_heldModifierCount) {
        for (size_t i = 0; i < m_heldModifierCount; ++i) {
            const uint32_t owner = m_heldModifiers[i].ownerPokemonId;
            bool retained = owner == target->battleState.pokemonId;
            for (uint8_t member = 0; member < m_context.playerPartyCount; ++member)
                retained |= owner == m_context.playerParty[member].battleState.pokemonId;
            if (!retained) {
                m_battleFeedback = "Capture requires other enemy held-item cleanup policy";
                return false;
            }
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
    if (!recordActiveParticipant()) return false;
    --m_pokeballs[ballIdx];

    PokemonCaptureEvent captureEvent{};
    const bool isFinalBoss = (m_run.wave == PokerogueContent::kClassicFinalWave);
    PokemonCaptureCriticalPolicy criticalPolicy{};
    criticalPolicy.resolved = m_starterProfileReady;
    criticalPolicy.caughtSpeciesCount = caughtSpeciesCount();
    // Current playable mode is Classic and no Catching Charm dispatcher is active.
    if (!executeCaptureAttempt(target->battleState, ball, false, false, bossShieldBlocked, isFinalBoss,
            *rng, captureEvent, m_starterProfileReady ? &criticalPolicy : nullptr)) {
        m_battleFeedback = "Capture inputs could not resolve";
        buildScene();
        return false;
    }

    if (captureEvent.caught) {
        // GameData.setPokemonCaught precedes the full-party incorporation choice.
        if (!recordCaughtSpecies(target->dex, target)) {
            m_battleFeedback = "Caught species profile could not resolve";
            return false;
        }
        if (m_starterProfileReady) {
            const auto* root = pokemonRootSpecies(target->dex);
            if (!root) return false;
            size_t index = 0;
            while (index < m_starterProfileCount && m_starterProfileRecords[index].speciesDex != root->dex) ++index;
            if (index == m_starterProfileCount) return false;
            StarterCandyAwardEvent candyEvent{};
            // Current actors have no shiny/variant state. This is the ordinary
            // Classic catch reward; shiny/egg/Daily policies remain unported.
            const uint32_t award = target->bossState.segmentCount ? 2 : 1;
            if (applyNativeStarterCandyAward(m_starterProfileRecords[index], award, candyEvent) !=
                    StarterCandyApplyResult::Applied) return false;
        }
        if (m_context.playerPartyCount > 6) return false;
        {
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
            resetPokemonSummonState(caughtMon.battleState);
            if (m_context.playerPartyCount == 6) {
                m_pendingCapturedPokemon = caughtMon;
                m_capturePartyTarget = target == &m_context.secondEnemy ? 1 : 0;
                m_selectedCapturePartyChoice = 0;
                m_capturePartyChoicePending = true;
                m_checkpointAvailable = false;
                m_runStarted = true;
                m_battleFeedback = "Party full: UP/DOWN recipient, A replace, B decline";
                buildScene();
                return true;
            }
            m_context.playerParty[m_context.playerPartyCount++] = caughtMon;
        }
        return finishSuccessfulCapture(*target);
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

bool FirstRunRuntime::restoreHeldModifierInventory(const NativeHeldModifierInstance* records, size_t count) {
    if (m_capturePartyChoicePending) return false;
    if ((count && !records) || count > m_heldModifiers.size() || moveLearningPending() || evolutionPending())
        return false;
    for (size_t i = 0; i < count; ++i) {
        if (!validateHeldModifierInstance(records[i])) return false;
        const uint32_t owner = records[i].ownerPokemonId;
        bool found = m_context.player.actorIdentityResolved && m_context.player.battleState.pokemonId == owner;
        found |= m_context.enemy.actorIdentityResolved && m_context.enemy.battleState.pokemonId == owner;
        found |= m_secondEncounterResolved && m_context.secondEnemy.actorIdentityResolved &&
            m_context.secondEnemy.battleState.pokemonId == owner;
        for (uint8_t member = 0; member < m_context.playerPartyCount; ++member)
            found |= m_context.playerParty[member].actorIdentityResolved &&
                m_context.playerParty[member].battleState.pokemonId == owner;
        for (uint8_t member = 0; member < m_context.trainerPartyCount; ++member)
            found |= m_context.trainerParty[member].actorIdentityResolved &&
                m_context.trainerParty[member].battleState.pokemonId == owner;
        if (!found) return false;
    }
    // Copy before publication also supports source slices of this inventory.
    std::unique_ptr<decltype(m_heldModifiers)> nextStorage(new (std::nothrow) decltype(m_heldModifiers){});
    if (!nextStorage) {
        m_battleFeedback = "Held inventory allocation failed";
        return false;
    }
    auto& next = *nextStorage;
    for (size_t i = 0; i < count; ++i) next[i] = records[i];
    m_heldModifiers = next;
    m_heldModifierCount = count;
    return true;
}

bool FirstRunRuntime::finishSuccessfulCapture(ResolvedPokemon& target, const uint32_t* releasedParticipant) {
    // Capture retains the source PID. Release discards outgoing held items;
    // incorporation keeps captured items without rewriting their metadata.
    uint32_t retainedOwners[6]{};
    for (uint8_t member = 0; member < m_context.playerPartyCount; ++member)
        retainedOwners[member] = m_context.playerParty[member].battleState.pokemonId;
    if (!retainPartyHeldInventory(m_heldModifiers.data(), m_heldModifiers.size(), m_heldModifierCount,
            retainedOwners, m_context.playerPartyCount)) return false;
    target.battleState.hp = 0;

    m_checkpointAvailable = false;
    m_runStarted = true;

    if (enemyPartyDefeated()) {
        // Pinned AttemptCapturePhase queues VictoryPhase, whose start calls
        // applyPartyExp(expValue, true), including friendship for participants.
        if (!grantVictoryExperience(true) || !planClassicVictory(m_run.wave, m_victoryPlan)) {
            m_battleFeedback = "Capture victory could not resolve";
            return false;
        }
        m_playerWon = true;
        m_battleFinished = true;
        m_battleFeedback = std::string("Gotcha! ") + (target.localizedName ? target.localizedName : "Pokémon") + " was caught!";
    } else {
        m_battleFeedback = std::string("Caught ") + (target.localizedName ? target.localizedName : "Pokémon") + "! Defeat remaining foe.";
    }
    // EXP used the original participation divisor. Remove the released identity
    // before turn-end registers the incoming active actor: six original
    // participants must not become a transient seven-entry history.
    if (releasedParticipant) removeParticipant(*releasedParticipant);
    return finishBattleTurn();
}

bool FirstRunRuntime::resolveCapturePartyChoice(int member) {
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime transaction allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
    if (!candidate.resolveCapturePartyChoiceInPlace(member)) {
        m_battleFeedback = candidate.m_battleFeedback;
        buildScene();
        return false;
    }
    *this = candidate;
    buildScene();
    return true;
}

bool FirstRunRuntime::resolveCapturePartyChoiceInPlace(int member) {
    if (!m_capturePartyChoicePending || m_context.playerPartyCount != 6 || member < -1 || member >= 6 ||
        !heldHealingInventorySupported(m_heldModifiers.data(), m_heldModifierCount)) return false;
    uint32_t releasedId = 0;
    if (member >= 0) {
        releasedId = m_context.playerParty[member].battleState.pokemonId;
        m_context.playerParty[member] = m_pendingCapturedPokemon;
        if (member == m_context.activePlayerPartyIndex) m_context.player = m_pendingCapturedPokemon;
        m_playerHistoryRequiresSnapshot = true;
    }
    m_capturePartyChoicePending = false;
    auto& target = m_capturePartyTarget ? m_context.secondEnemy : m_context.enemy;
    if (!finishSuccessfulCapture(target, member >= 0 ? &releasedId : nullptr)) return false;
    m_pendingCapturedPokemon = {};
    return true;
}

bool FirstRunRuntime::togglePlayerEvolutionPause(uint8_t memberIndex) {
    if (m_capturePartyChoicePending || !m_runStarted || moveLearningPending() || evolutionPending() ||
        memberIndex >= m_context.playerPartyCount) return false;
    auto& member = memberIndex == m_context.activePlayerPartyIndex
        ? m_context.player : m_context.playerParty[memberIndex];
    if (!member.actorIdentityResolved || !member.speciesId) return false;
    bool hasEvolutions = false;
    for (const auto& edge : PokerogueContent::kSpeciesEvolutions)
        if (pokemonEvolutionTextEqual(edge.sourceSpeciesId, member.speciesId)) {
            hasEvolutions = true;
            break;
        }
    if (!hasEvolutions) return false;
    // PartyUiHandler.processUnpauseEvolutionOption changes an actor option;
    // it neither consumes a battle turn nor initiates evolution immediately.
    member.battleState.pauseEvolutions = !member.battleState.pauseEvolutions;
    if (memberIndex == m_context.activePlayerPartyIndex)
        m_context.playerParty[memberIndex] = m_context.player;
    m_playerHistoryRequiresSnapshot = true;
    m_battleFeedback = member.battleState.pauseEvolutions
        ? "Future evolutions paused" : "Future evolutions enabled";
    buildScene();
    return true;
}

bool FirstRunRuntime::switchPlayerPokemon(uint8_t targetIndex) {
    // Commands run before render. Commit the complete action only on success.
    std::unique_ptr<FirstRunRuntime> candidateStorage(new (std::nothrow) FirstRunRuntime(*this));
    if (!candidateStorage) {
        m_battleFeedback = "Runtime transaction allocation failed";
        return false;
    }
    auto& candidate = *candidateStorage;
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
    if (m_capturePartyChoicePending) return false;
    if (!heldHealingInventorySupported(m_heldModifiers.data(), m_heldModifierCount)) {
        m_battleFeedback = "Held modifier effects require native dispatch";
        return false;
    }
    if (moveLearningPending() || evolutionPending()) return false;
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

    if (!recordActiveParticipant()) return false;
    resetPokemonSummonState(m_context.player.battleState);
    m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;
    m_context.activePlayerPartyIndex = targetIndex;
    m_context.player = m_context.playerParty[targetIndex];
    if (m_context.player.battleState.confusion.present) {
        PokemonConfusionRemovalEvent incomingConfusion{};
        if (applyPokemonPostSummonConfusionRemoval(m_context.player.battleState, true, true, incomingConfusion) !=
                PokemonStatusImmunityResult::Resolved) return false;
    }
    resetPokemonSummonState(m_context.player.battleState);
    PokemonPostSummonStatusHealingEvent incomingStatus{};
    if (!applyPokemonPostSummonStatusHealing(m_context.player.battleState, true, true, incomingStatus)) return false;

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
    resetPokemonSummonState(m_context.player.battleState);
    m_context.playerParty[m_context.activePlayerPartyIndex] = m_context.player;

    for (uint8_t i = 0; i < m_context.playerPartyCount; ++i) {
        if (m_context.playerParty[i].battleState.hp > 0) {
            m_context.activePlayerPartyIndex = i;
            m_context.player = m_context.playerParty[i];
            if (m_context.player.battleState.confusion.present) {
                PokemonConfusionRemovalEvent incomingConfusion{};
                if (applyPokemonPostSummonConfusionRemoval(m_context.player.battleState, true, true, incomingConfusion) !=
                        PokemonStatusImmunityResult::Resolved) return false;
            }
            resetPokemonSummonState(m_context.player.battleState);
            PokemonPostSummonStatusHealingEvent incomingStatus{};
            if (!applyPokemonPostSummonStatusHealing(m_context.player.battleState, true, true, incomingStatus)) return false;
            m_battleFeedback = std::string("Fainted! Sent out ") +
                (m_context.player.localizedName ? m_context.player.localizedName : "next Pokémon");
            buildScene();
            return true;
        }
    }
    return false;
}

bool FirstRunRuntime::resolveStarterFromDex(uint16_t dex, PokerogueRngAdapter& rng, ResolvedPokemon& output) {
    const auto* entry = PokerogueContent::findSpeciesByDex(dex);
    if (!entry || !entry->starterEligible) return false;
    const auto& starter = *entry;
    const NativeStarterCandyRecord* dexMetadata = nullptr;
    if (m_starterProfileReady)
        for (size_t i = 0; i < m_starterProfileCount; ++i)
            if (m_starterProfileRecords[i].speciesDex == dex) { dexMetadata = &m_starterProfileRecords[i]; break; }
    if (!starter.freshProfileStarter && (!dexMetadata || !dexMetadata->caught ||
        !dexMetadata->natureAttr || !dexMetadata->abilityAttr)) return false;
    const uint16_t selectedFormIndex = dexMetadata && dexMetadata->preferredFormIndex != 65535
        ? dexMetadata->preferredFormIndex : 0;
    const auto* selectedForm = PokerogueContent::findFormByUpstreamIndex(dex, selectedFormIndex);
    if (dexMetadata && dexMetadata->preferredFormIndex != 65535 &&
        pokemonValidateStarterForm(dex, selectedFormIndex, dexMetadata->unlockedFormAttr) != PokemonStarterFormResult::Ok)
        return false;
    if (!starter.freshProfileStarter && starter.firstFormId && *starter.firstFormId &&
        pokemonValidateStarterForm(dex, selectedFormIndex, dexMetadata->unlockedFormAttr) != PokemonStarterFormResult::Ok)
        return false;
    const char* selectedFormId = selectedForm ? selectedForm->id : starter.firstFormId;
    const std::string starterLocaleId = std::string("pokemon:") + starter.id;
    ResolvedPokemon prepared{starter.dex, 5, starter.id, locale(starterLocaleId.c_str(), starter.name),
        selectedFormId, starter.assetSourcePath};
    if (pokemonTotalExperienceForLevel(starter.growthRate, 5, prepared.totalExperience) !=
            PokemonExperienceResult::Ok) return false;
    auto nextRng = rng;
      prepared.movesetResolved = selectPokemonStarterMoveset(
          starter.dex, selectedFormId, 0, nullptr, 0,
          prepared.moveIds, prepared.moveCount) ==
          PokemonStarterMovesetResult::Ok;
      if (prepared.movesetResolved) {
        PokemonActorIdentity starterActor{};
        PokemonNature starterNature = PokemonNature::Unspecified;
        if (starter.freshProfileStarter) {
            if (pokemonFreshProfileNature(starter.dex, starterNature) != PokemonFreshProfileResult::Ok) return false;
        } else if (!nativeStarterDefaultNature(*dexMetadata, starterNature)) return false;
        if (m_starterProfileReady) {
            for (size_t record = 0; record < m_starterProfileCount; ++record) {
                const auto& dexEntry = m_starterProfileRecords[record];
                if (dexEntry.speciesDex == starter.dex) {
                    if (dexEntry.natureAttr && !nativeStarterDefaultNature(dexEntry, starterNature)) return false;
                    break;
                }
            }
        }
        const char* starterFormId = selectedFormId && *selectedFormId ? selectedFormId : nullptr;

        // The new profile's DexData supplies 15 IVs, the first unlocked
        // ability, default male gender (or genderless), base form, and its
        // source-derived default nature. Pokemon's constructor still consumes
        // its PID and initial Tera pick from the run's root RNG stream.

        starterActor.pokemonId = nextRng.randSeedUint32();
        starterActor.abilityIndex = 0;
        uint16_t starterAbility = starter.ability1;
        if (m_starterProfileReady) {
            for (size_t record = 0; record < m_starterProfileCount; ++record) {
                const auto& entry = m_starterProfileRecords[record];
                if (entry.speciesDex != starter.dex) continue;
                if (entry.abilityAttr && !nativeStarterDefaultAbility(entry, starterActor.abilityIndex, starterAbility)) return false;
                break;
            }
        }

        starterActor.gender = starter.malePercentTenths == 65534 ? PokemonGender::Genderless :
            starter.malePercentTenths == 0 ? PokemonGender::Female : PokemonGender::Male;
        if (m_starterProfileReady) {
            for (size_t record = 0; record < m_starterProfileCount; ++record) {
                const auto& entry = m_starterProfileRecords[record];
                if (entry.speciesDex != starter.dex) continue;
                if (entry.genderAttr && !nativeStarterDefaultGender(entry, starterActor.gender)) return false;
                break;
            }
        }
        if (!starter.freshProfileStarter && !nativeStarterDefaultGender(*dexMetadata, starterActor.gender)) return false;
        if (dexMetadata && dexMetadata->preferredFormIndex != 65535 && selectedForm &&
            pokemonFormTextEquals(selectedForm->formKey, "FEMALE")) {
            if (!(dexMetadata->genderAttr & 8u)) return false;
            starterActor.gender = PokemonGender::Female;
        }
        starterActor.nature = starterNature;
        starterActor.formId = starterFormId;
        for (uint8_t& iv : starterActor.ivs) iv = starter.freshProfileStarter ? 15 : 0;
        // Default starters already own the canonical 15-IV baseline. Preserve
        // improvements accumulated in their durable Pokédex, without RNG draws.
        if (m_starterProfileReady) {
            for (size_t record = 0; record < m_starterProfileCount; ++record) {
                const auto& dexEntry = m_starterProfileRecords[record];
                if (dexEntry.speciesDex != starter.dex) continue;
                for (uint8_t stat = 0; stat < 6; ++stat)
                    starterActor.ivs[stat] = std::max(starterActor.ivs[stat], dexEntry.dexIvs[stat]);
                break;
            }
        }

        const auto* starterForm = PokerogueContent::findFormById(starterFormId);
        const char* starterType1 = starterForm ? starterForm->type1 : starter.type1;
        const char* starterType2 = starterForm ? starterForm->type2 : starter.type2;
        if (!starterType1 || !*starterType1) return false;
        const bool hasSecondaryType = starterType2 && *starterType2 &&
            std::strcmp(starterType2, "NONE") != 0;
        starterActor.initialTeraTypeIndex = static_cast<uint8_t>(
            nextRng.randSeedInt(hasSecondaryType ? 2 : 1));
        starterActor.initialTeraType = resolvePokemonTypeSymbol(
            starterActor.initialTeraTypeIndex ? starterType2 : starterType1);
        if (!starterActor.initialTeraType) return false;
        starterActor.initialTeraTypeResolved = true;

        PokemonBattleInit starterInput{};
        starterInput.speciesDex = starter.dex;
        starterInput.formId = starterActor.formId;
        starterInput.level = prepared.level;
        starterInput.pokemonId = starterActor.pokemonId;
        starterInput.nature = starterActor.nature;
        starterInput.gender = starterActor.gender;
        starterInput.abilityId = starterAbility;
        if (starterForm) {
            const uint16_t formAbility = starterActor.abilityIndex == 0 ? starterForm->ability1 :
                starterActor.abilityIndex == 1 && starter.ability2 ? starterForm->ability2 : starterForm->abilityHidden;
            if (formAbility) starterInput.abilityId = formAbility;
        }
        for (uint8_t i = 0; i < 6; ++i) starterInput.ivs[i] = starterActor.ivs[i];
        starterInput.moveCount = prepared.moveCount;
        for (uint8_t i = 0; i < starterInput.moveCount; ++i)
            starterInput.moveIds[i] = prepared.moveIds[i];
        if (initializePokemonBattleState(starterInput, prepared.battleState) !=
            PokemonBattleInitResult::Ok) return false;
        prepared.actor = starterActor;
        prepared.actorIdentityResolved = true;
        prepared.formId = starterActor.formId;
      } else return false;
    rng = nextRng;
    output = prepared;
    return true;
}

void FirstRunRuntime::resolve(bool carryPlayer) {
    m_participantHistoryResolved = true;
    m_participantCount = 0;
    m_participantIds = {};
    if (carryPlayer) {
        uint32_t partyIds[6]{};
        if (m_context.playerPartyCount > 6) { m_encounterResolved = false; return; }
        for (uint8_t member = 0; member < m_context.playerPartyCount; ++member)
            partyIds[member] = member == m_context.activePlayerPartyIndex
                ? m_context.player.battleState.pokemonId : m_context.playerParty[member].battleState.pokemonId;
        if (!retainPartyHeldInventory(m_heldModifiers.data(), m_heldModifiers.size(), m_heldModifierCount,
                partyIds, m_context.playerPartyCount)) { m_encounterResolved = false; return; }
    }
    m_selectedBattleMove = 0;
    m_turn = 1;
    m_enemySwitchCounter = 0;
    m_battleFinished = false;
    m_playerWon = false;
    m_experienceGranted = false;
    m_progressionQueueCount = m_progressionQueueCursor = 0;
    m_progressionPartyIndex = 0xFF;
    m_pendingLevelMoves = {};
    m_pendingEvolutionSpeciesId = nullptr;
    m_evolutionPauseConfirmation = false;
    m_victoryPlan = {};
    m_rewardChoices = {};
    m_rewardChoiceCount = 0;
    m_selectedRewardChoice = 0;
    m_rewardsPending = false;
    m_capturePartyChoicePending = false;
    m_pendingCapturedPokemon = {};
    m_runStarted = carryPlayer;
    m_checkpointAvailable = !carryPlayer;
    m_battleFeedback.clear();
    m_encounterResolved = false;
    m_doubleBattle = false;
    m_trainerBattle = false;
    m_secondEncounterResolved = false;
    m_doubleExperienceGrantedMask = 0;
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
        const auto transition = resolveClassicNextBiome(m_run.biomeId, m_run.wave, true,
            m_seedCodeUnits.data(), m_seedLength, false, nullptr, nextBiomeId);
        if (transition != ClassicBiomeTransitionResult::Ok || !nextBiomeId) {
            m_battleFeedback = classicBiomeTransitionResultName(transition == ClassicBiomeTransitionResult::Ok
                ? ClassicBiomeTransitionResult::MissingBiome : transition);
            return; // Candidate command rolls back; never reuse the prior biome silently.
        }
        m_run.biomeId = nextBiomeId;
    }
    const auto* biomeEntry = findBiomeById(m_run.biomeId);
    const std::string biomeLocaleId = std::string("biomes:") + m_run.biomeId;
    m_context.biomeName = locale(biomeLocaleId.c_str(), biomeEntry ? biomeEntry->name : m_run.biomeId);
    if (!carryPlayer) {
        m_playerHistoryRequiresSnapshot = false;
        m_heldModifiers = {};
        m_heldModifierCount = 0;
        PokerogueRngAdapter starterRng;
        starterRng.sow(m_seedCodeUnits.data(), m_seedLength);
        if (!resolveStarterFromDex(starter.dex, starterRng, m_context.player)) return;
        m_context.playerParty[0] = m_context.player;
        m_context.playerPartyCount = 1;
        m_context.activePlayerPartyIndex = 0;
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
        resetPokemonSummonState(m_context.player.battleState);
        for (uint8_t i = 0; i < m_context.playerPartyCount; ++i)
            resetPokemonSummonState(m_context.playerParty[i].battleState);
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
                            m_globalRng = waveRng;
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

        const auto* selectedForm = actor.formId ? PokerogueContent::findFormById(actor.formId) : nullptr;
        const auto* fixedMoveset = findPokemonFixedEnemyMoveset(species.dex,
            selectedForm && selectedForm->formKey && !std::strcmp(selectedForm->formKey, "ETERNAMAX") ? 1 : 0);
        uint16_t moveIds[4]{};
        uint8_t moveCount = 0;
        if (fixedMoveset) {
            moveCount = 4;
            for (uint8_t slot = 0; slot < 4; ++slot) moveIds[slot] = fixedMoveset->moveIds[slot];
        } else {
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
            if (generateWildMovesetFromLearnset(species.dex, actor.formId, destination.level,
                    battleState.stats[1], battleState.stats[3], battleState.stats[2], type1, type2,
                    metadata, candidateCount, waveRng, moveIds, moveCount) != PokemonWildMovesetResult::Ok) return false;
        }
        battleInput.moveCount = moveCount;
        for (uint8_t i = 0; i < moveCount; ++i) battleInput.moveIds[i] = moveIds[i];
        PokemonBattleState battleReadyState{};
        if (initializePokemonBattleStateForActor(battleInput, actor, battleReadyState) != PokemonBattleInitResult::Ok) return false;
        if (fixedMoveset && !applyPokemonFixedEnemyMovePp(*fixedMoveset, battleReadyState)) return false;
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
    m_globalRng = waveRng;
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
    } else if (progressionPokemon().movesetResolved) {
        const auto& displayed = progressionPokemon();
        for (uint8_t i = 0; i < displayed.moveCount && i < 4; ++i) {
            const auto* move = PokerogueContent::findMoveById(displayed.moveIds[i]);
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
    if (moveLearningPending()) {
        const auto* proposed = PokerogueContent::findMoveById(pendingLearnMoveId());
        if (proposed) {
            const std::string key = std::string("move:") + proposed->key;
            m_text[6] = std::string("Learn ") + locale(key.c_str(), proposed->name) + "?";
            m_text[5] = "UP/DOWN slot - A replace - B reject";
        }
    }
    if (!moveLearningPending() && evolutionPending()) {
        const auto* target = m_pendingEvolutionSpeciesId ? findSpeciesById(m_pendingEvolutionSpeciesId) : nullptr;
        if (target) {
            const std::string key = std::string("pokemon:") + target->id;
            m_text[6] = std::string("Evolve into ") + locale(key.c_str(), target->name) + "?";
            m_text[5] = "A continue - B cancel evolution";
        }
    }
    if (m_evolutionPauseConfirmation) {
        m_text[6] = "Pause future evolutions?";
        m_text[5] = "A yes - B no";
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
