#pragma once

#include "screens/SceneData.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokerogueBattleRng.hpp"
#include "game/PokerogueTurnOrder.hpp"
#include "game/PokerogueClassicVictoryPlan.hpp"
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
    bool selectBattleMove(int direction);
    bool advanceBattleTurn();
    bool skipVictoryReward();
    uint8_t selectedBattleMove() const { return m_selectedBattleMove; }
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

private:
    void resolve(bool carryPlayer = false);
    bool restoreNativeRunSaveInPlace(const NativeRunSave& save);
    bool grantVictoryExperience();
    bool advanceTrainerAfterDefeat();
    bool finishBattleTurn();
    bool executeActiveBattleMove(bool enemyActs, uint8_t moveSlot, PokerogueRngAdapter& rng);
    void refreshTrainerBaselineMatchups();
    bool resolveActiveMoveWeather(bool enemyAttacks, PokemonMoveWeatherContext& output) const;
    bool resolveActiveMoveCritical(bool enemyAttacks, PokemonCriticalPolicy& output) const;
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
    PokerogueRngState m_trainerConstructorRngStates[6]{};
    PokerogueRngState m_trainerPostMovesetRngStates[6]{};
    PokerogueRngState m_trainerPostIvRngStates[6]{};
    std::size_t m_seedLength = 0;
    bool m_encounterResolved = false;
    bool m_doubleBattle = false;
    bool m_trainerBattle = false;
    bool m_secondEncounterResolved = false;
    uint8_t m_selectedBattleMove = 0;
    uint32_t m_turn = 1;
    uint32_t m_enemySwitchCounter = 0;
    uint32_t m_playerExperience = 0;
    bool m_battleFinished = false;
    bool m_playerWon = false;
    bool m_experienceGranted = false;
    ClassicVictoryPlan m_victoryPlan{};
    bool m_runStarted = false;
    bool m_checkpointAvailable = true;
    std::string m_battleFeedback;
};

} // namespace Pokerogue3DS
