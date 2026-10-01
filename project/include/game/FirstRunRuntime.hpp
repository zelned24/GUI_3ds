#pragma once

#include "screens/SceneData.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokemonCapturePhase.hpp"
#include "game/PokemonEvolutionPhase.hpp"
#include "game/PokerogueBattleRng.hpp"
#include "game/PokerogueTurnOrder.hpp"
#include "game/PokerogueClassicVictoryPlan.hpp"
#include "game/PokerogueModifierReward.hpp"
#include "storage/NativeRunSave.hpp"
#include <cstdint>
#include <array>
#include <string>

namespace Pokerogue3DS {

struct RunState {
    uint32_t seed;
    uint16_t wave;
    const char* modeId;
    const char* biomeId;
    uint16_t starterDex;
    uint16_t encounterDex;
};

struct ResolvedPokemon {
    uint16_t dex;
    uint16_t level;
    const char* speciesId;
    const char* localizedName;
    const char* formId;
    const char* assetSourcePath;
    PokemonActorIdentity actor{};
    bool actorIdentityResolved = false;
    uint16_t moveIds[4]{};
    uint8_t moveCount = 0;
    bool movesetResolved = false;
    PokemonBattleState battleState{};
    PokemonBossState bossState{};
    uint32_t totalExperience = 0; // Pokemon.exp belongs to this actor, not to the run.
};

struct PresentationContext {
    const char* modeName;
    const char* biomeName;
    uint16_t trainerTypeId = 0;
    const char* trainerName = nullptr;
    const char* trainerPartyTemplateKey = nullptr;
    uint16_t trainerPartyLevels[6]{};
    uint8_t trainerPartyCount = 0;
    uint8_t activeTrainerPartyIndex = 0xFF;
    bool trainerFemaleVariant = false;
    ResolvedPokemon trainerParty[6]{};
    bool trainerPartySpeciesResolved = false;
    bool trainerPartyConstructorResolved = false;
    uint16_t trainerPartyLevelMoveCounts[6]{};
    bool trainerPartyLevelMovesResolved = false;
    uint16_t trainerPartySupersededMoveCounts[6]{};
    bool trainerPartySupercedenceResolved = false;
    uint16_t trainerPartyHardEligibleMoveCounts[6]{};
    bool trainerPartyHardMoveFilterResolved = false;
    uint16_t trainerPartySingleEligibleMoveCounts[6]{};
    bool trainerPartySingleMoveFilterResolved = false;
    bool trainerPartyBaseWeightsResolved = false;
    bool trainerPartyDamageWeightsResolved = false;
    bool trainerPartyMovesetsResolved = false;
    bool trainerPartyIvsResolved = false;
    bool trainerPartyBattleStatesResolved = false;
    uint8_t nextTrainerPartyIndex = 0xFF;
    bool trainerPartyBaselineMatchupResolved = false;
    double trainerPartyBaselineMatchupScores[6]{};
    ResolvedPokemon player;
    ResolvedPokemon enemy;
    ResolvedPokemon secondEnemy;
    ResolvedPokemon playerParty[6]{};
    uint8_t playerPartyCount = 1;
    uint8_t activePlayerPartyIndex = 0;
};

// Native content-driven first-run adapter. It owns the text storage referenced by
// SceneDefinition and keeps Scene Composer's ScenePlayer as the presentation layer.
class FirstRunRuntime {
public:
    explicit FirstRunRuntime(uint32_t seed);
    bool cycleStarter(int direction);
    bool restoreSetup(uint32_t seed, uint16_t starterDex);
    void captureNativeRunSave(NativeRunSave& output) const;
    bool restoreNativeRunSave(const NativeRunSave& save);
    bool battleInputSupported() const;
    bool trainerBattleSupported() const;
    bool doubleBattleSupported() const;
    bool rewardsPending() const { return m_rewardsPending; }
    uint8_t rewardChoiceCount() const { return m_rewardChoiceCount; }
    uint8_t selectedRewardChoice() const { return m_selectedRewardChoice; }
    const ModifierRewardRoll* rewardChoice(uint8_t index) const {
        return index < m_rewardChoiceCount ? &m_rewardChoices[index] : nullptr;
    }
    bool selectRewardChoice(int direction);
    bool claimRewardChoice();
    bool selectBattleMove(int direction);
    bool evolutionPending() const { return m_pendingEvolutionSpeciesId != nullptr || m_evolutionPauseConfirmation; }
    bool moveLearningPending() const { return m_pendingLevelMoves.count != 0; }
    uint16_t pendingLearnMoveId() const { return m_pendingLevelMoves.count ? m_pendingLevelMoves.moveIds[0] : 0; }
    bool resolvePendingLearnMove(int selectedSlot); // -1 rejects; 0..3 replaces.
    bool advanceBattleTurn();
    bool skipVictoryReward();
    uint8_t selectedBattleMove() const { return m_selectedBattleMove; }
    uint8_t selectedTarget() const { return m_selectedTarget; }
    bool cycleTarget(int direction);
    const std::string& battleFeedback() const { return m_battleFeedback; }
    const PokemonArenaWeatherState& arenaWeather() const { return m_arenaWeather; }
    bool doubleBattle() const { return m_doubleBattle; }
    bool playerWon() const { return m_playerWon; }
    bool battleFinished() const { return m_battleFinished; }
    bool experienceGranted() const { return m_experienceGranted; }
    const ClassicVictoryPlan& victoryPlan() const { return m_victoryPlan; }
    bool runStarted() const { return m_runStarted; }
    void setStorageFeedback(const char* message);
    const Citro2D::SceneDefinition& scene() const { return m_scene; }
    const RunState& run() const { return m_run; }
    const PresentationContext& presentation() const { return m_context; }
    PokerogueBattleRng& battleRng() { return m_battleRng; }
    const char* starterName() const;

    uint8_t playerPartyCount() const { return m_context.playerPartyCount; }
    uint8_t activePlayerPartyIndex() const { return m_context.activePlayerPartyIndex; }
    const ResolvedPokemon* playerPartyMember(uint8_t index) const {
        return index < m_context.playerPartyCount ? &m_context.playerParty[index] : nullptr;
    }
    // Provisional Old 3DS storage budget; exceeding it fails explicitly.
    static constexpr size_t kHeldModifierStorageCapacity = 32;
    size_t heldModifierCount() const { return m_heldModifierCount; }
    const NativeHeldModifierInstance* heldModifier(size_t index) const {
        return index < m_heldModifierCount ? &m_heldModifiers[index] : nullptr;
    }
    bool restoreHeldModifierInventory(const NativeHeldModifierInstance* records, size_t count);
    bool switchPlayerPokemon(uint8_t targetIndex);
    bool togglePlayerEvolutionPause(uint8_t memberIndex);
    bool advancePlayerAfterDefeat();
    bool playerPartyDefeated() const;
    uint16_t pokeballCount(PokeballType type) const {
        const auto idx = static_cast<uint8_t>(type);
        return idx < 6 ? m_pokeballs[idx] : 0;
    }
    bool throwPokeball(PokeballType type = PokeballType::Pokeball);

private:
    bool advanceBattleTurnInPlace();
    bool claimRewardChoiceInPlace();
    bool resolvePendingLearnMoveInPlace(int selectedSlot);
    bool finishPendingEvolution(bool accepted = false);
    bool skipVictoryRewardInPlace();
    bool throwPokeballInPlace(PokeballType ball);
    bool switchPlayerPokemonInPlace(uint8_t targetIndex);
    bool selectEnemyMoveSlot(const PokemonBattleState& enemy, const PokemonBattleState& player,
        PokerogueRngAdapter& rng, uint8_t& slot);
    bool executeEnemyResponse(uint8_t userIndex, PokerogueRngAdapter& rng);
    bool enemyPartyDefeated() const;
    bool weatherBattleSupported() const;
    void resolve(bool carryPlayer = false);
    bool restoreNativeRunSaveInPlace(const NativeRunSave& save);
    bool grantVictoryExperience();
    bool advanceTrainerAfterDefeat();
    bool generateVictoryRewards();
    bool finishBattleTurn();
    bool executeActiveBattleMove(bool enemyActs, uint8_t moveSlot, PokerogueRngAdapter& rng);
    bool executeActiveBattleMove(uint8_t userIndex, uint8_t targetIndex, uint8_t moveSlot, PokerogueRngAdapter& rng, const PokemonPpPolicy* ppOverride = nullptr);
    void refreshTrainerBaselineMatchups();
    bool resolveActiveMoveWeather(bool enemyAttacks, PokemonMoveWeatherContext& output) const;
    bool resolveActiveMoveWeather(const PokemonBattleState& user, const PokemonBattleState& opponent,
                                  PokemonMoveWeatherContext& output) const;
    bool resolveActiveMoveCritical(bool enemyAttacks, PokemonCriticalPolicy& output) const;
    bool resolveActiveMoveCritical(const PokemonBattleState& user, const PokemonBattleState& opponent,
                                   PokemonCriticalPolicy& output) const;
    void buildScene();
    static const char* locale(const char* canonicalId, const char* fallback);

    RunState m_run{};
    PokemonArenaWeatherState m_arenaWeather{};
    PokemonTrickRoomState m_trickRoom{};
    PresentationContext m_context{};
    Citro2D::SceneDefinition m_scene{};
    std::array<Citro2D::SceneNodeData, 13> m_nodes{};
    std::array<std::string, 13> m_text{};
    std::size_t m_starterIndex = 0;
    std::array<uint16_t, 10> m_seedCodeUnits{};
    PokerogueBattleRng m_battleRng{};
    PokerogueRngAdapter m_globalRng{}; // Wave stream for non-Battle.randSeedInt draws.
    PokerogueRngState m_trainerConstructorRngStates[6]{};
    PokerogueRngState m_trainerPostMovesetRngStates[6]{};
    PokerogueRngState m_trainerPostIvRngStates[6]{};
    std::size_t m_seedLength = 0;
    bool m_encounterResolved = false;
    bool m_doubleBattle = false;
    bool m_trainerBattle = false;
    bool m_secondEncounterResolved = false;
    uint8_t m_selectedBattleMove = 0;
    uint8_t m_selectedTarget = 0;
    uint32_t m_turn = 1;
    uint32_t m_enemySwitchCounter = 0;
    bool m_battleFinished = false;
    bool m_playerWon = false;
    bool m_experienceGranted = false;
    bool m_playerHistoryRequiresSnapshot = false; // Player choices cannot be replayed from seed.
    PokemonPendingLevelMoves m_pendingLevelMoves{};
    const char* m_pendingEvolutionSpeciesId = nullptr;
    bool m_evolutionPauseConfirmation = false;
    ClassicVictoryPlan m_victoryPlan{};
    std::array<ModifierRewardRoll, 3> m_rewardChoices{};
    uint8_t m_rewardChoiceCount = 0;
    uint8_t m_selectedRewardChoice = 0;
    bool m_rewardsPending = false;
    bool m_runStarted = false;
    bool m_checkpointAvailable = true;
    std::string m_battleFeedback;
    std::array<uint16_t, 6> m_pokeballs{ 5, 0, 0, 0, 0, 0 };
    std::array<NativeHeldModifierInstance, kHeldModifierStorageCapacity> m_heldModifiers{};
    size_t m_heldModifierCount = 0;
};

} // namespace Pokerogue3DS
