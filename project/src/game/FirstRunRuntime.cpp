#include "game/FirstRunRuntime.hpp"
#include "game/PokerogueEncounterResolver.hpp"
#include "game/PokerogueClassicWaveSchedule.hpp"
#include "game/PokerogueTurnOrder.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include "game/PokemonLevelMovePool.hpp"
#include "game/PokemonFreshProfile.hpp"
#include "game/PokemonExperience.hpp"
#include "game/PokemonStarterMoveset.hpp"
#include "game/PokemonWildMovesetGenerator.hpp"
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
            ? (m_playerWon ? NativeSaveStage::BattleWon : NativeSaveStage::BattleLost)
            : NativeSaveStage::BattleActive;
        value.encounterDex = m_context.enemy.dex;
        value.playerHp = m_context.player.battleState.hp;
        value.enemyHp = m_context.enemy.battleState.hp;
        value.battleTurn = m_turn;
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
    }
    output = value;
}

bool FirstRunRuntime::restoreNativeRunSave(const NativeRunSave& save) {
    if (validateNativeRunSave(save, PokerogueContent::kContentHash) != NativeSaveResult::Ok ||
        save.stage < NativeSaveStage::RunSetup ||
        save.stage > NativeSaveStage::BattleLost) return false;
    if (!restoreSetup(save.seed, save.starterDex)) return false;
    if (save.wave != m_run.wave || save.playerLevel != m_context.player.level
        || save.playerExperience != m_playerExperience) return false;
    if (save.stage == NativeSaveStage::RunSetup) return true;
    if (!m_encounterResolved || m_doubleBattle || save.encounterDex != m_run.encounterDex ||
        !save.battleTurn || !save.playerMoveCount || save.playerMoveCount > 4 ||
        !save.enemyMoveCount || save.enemyMoveCount > 4 ||
        save.playerMoveCount != m_context.player.battleState.moveCount ||
        save.enemyMoveCount != m_context.enemy.battleState.moveCount ||
        save.playerHp > m_context.player.battleState.maxHp ||
        save.enemyHp > m_context.enemy.battleState.maxHp ||
        (save.stage == NativeSaveStage::BattleActive && (!save.playerHp || !save.enemyHp)) ||
        (save.stage == NativeSaveStage::BattleWon && (save.enemyHp || !save.playerHp)) ||
        (save.stage == NativeSaveStage::BattleLost && (save.playerHp || !save.enemyHp))) return false;
    for (uint8_t i = 0; i < save.playerMoveCount; ++i) {
        const auto& move = m_context.player.battleState.moves[i];
        if (save.playerMoveIds[i] != move.moveId || save.playerPp[i] > move.maxPp) return false;
    }
    for (uint8_t i = 0; i < save.enemyMoveCount; ++i) {
        const auto& move = m_context.enemy.battleState.moves[i];
        if (save.enemyMoveIds[i] != move.moveId || save.enemyPp[i] > move.maxPp) return false;
    }
    // Run commands are accepted only between turns. The pinned battle stream
    // is re-seeded from battleSeed + turn index, so no mid-turn Alea state is
    // needed in a portable checkpoint.
    if (!m_battleRng.beginTurn(save.battleTurn)) return false;
    m_context.player.battleState.hp = save.playerHp;
    m_context.enemy.battleState.hp = save.enemyHp;
    for (uint8_t i = 0; i < save.playerMoveCount; ++i)
        m_context.player.battleState.moves[i].pp = save.playerPp[i];
    for (uint8_t i = 0; i < save.enemyMoveCount; ++i)
        m_context.enemy.battleState.moves[i].pp = save.enemyPp[i];
    m_turn = save.battleTurn;
    m_runStarted = true;
    m_checkpointAvailable = true;
    m_battleFinished = save.stage == NativeSaveStage::BattleWon || save.stage == NativeSaveStage::BattleLost;
    m_playerWon = save.stage == NativeSaveStage::BattleWon;
    m_victoryPlan = {};
    if (m_playerWon && !planClassicVictory(m_run.wave, m_victoryPlan)) return false;
    m_battleFeedback = m_battleFinished ? (m_playerWon ? "Restored: wild battle won" : "Restored: Pokemon fainted")
                                       : "Battle progress restored";
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
bool supportsBaselineBattleMove(uint16_t moveId) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    // This first resolver only executes plain, single-target damaging moves.
    // Any declared attribute or upstream behavior flag needs its own port.
    return move && move->category != PokerogueContent::MoveStatus && move->power > 0 &&
        move->type && move->target && std::strcmp(move->target, "NEAR_OTHER") == 0 &&
        move->upstreamFlags == 0 && move->attributeCount == 0 &&
        std::strcmp(move->type, "Fire") != 0;
}

bool sameText(const char* left, const char* right) {
    return left && right && std::strcmp(left, right) == 0;
}

double baselineEnemyMoveScore(const PokemonBattleState& user,
                              const PokemonBattleState& target,
                              const PokerogueContent::Move& move) {
    double effectiveness = 1.0;
    if (calculatePokemonTypeEffectiveness(move.id, target, effectiveness) !=
        PokemonTypeEffectivenessResult::Ok) return -20.0;
    const bool physical = move.category == PokerogueContent::MovePhysical;
    const uint16_t selectedStat = user.stats[physical ? 1 : 3];
    const uint16_t otherStat = user.stats[physical ? 3 : 1];
    if (!selectedStat) return -20.0;
    double attackScore = (effectiveness - 1.0) * (effectiveness - 1.0) *
        (effectiveness < 1.0 ? -2.0 : 2.0);
    const double statRatio = static_cast<double>(otherStat) / selectedStat;
    if (statRatio <= 0.75) attackScore *= 2.0;
    else if (statRatio <= 0.875) attackScore *= 1.5;
    attackScore += move.power / 5;
    double score = -attackScore;

    const auto* species = PokerogueContent::findSpeciesByDex(user.speciesDex);
    const auto* form = user.formId ? PokerogueContent::findFormById(user.formId) : nullptr;
    const char* type1 = form ? form->type1 : (species ? species->type1 : nullptr);
    const char* type2 = form ? form->type2 : (species ? species->type2 : nullptr);
    if (sameText(type1, move.type) || (type2 && !sameText(type2, "NONE") && sameText(type2, move.type))) score *= 1.5;
    score *= effectiveness;
    if (!score) return -20.0;
    return score;
}

}

bool FirstRunRuntime::battleInputSupported() const {
    if (!m_encounterResolved || !m_context.player.actorIdentityResolved ||
        !m_context.enemy.actorIdentityResolved || m_doubleBattle || m_battleFinished ||
        !m_context.enemy.battleState.moveCount || m_context.enemy.battleState.moveCount > 4) return false;
    uint8_t enemyUsable = 0;
    for (uint8_t i = 0; i < m_context.enemy.battleState.moveCount; ++i) {
        const auto& move = m_context.enemy.battleState.moves[i];
        if (!move.pp) continue;
        if (!supportsBaselineBattleMove(move.moveId)) return false;
        ++enemyUsable;
    }
    return enemyUsable && m_selectedBattleMove < m_context.player.battleState.moveCount &&
        m_context.player.battleState.moves[m_selectedBattleMove].pp &&
        supportsBaselineBattleMove(m_context.player.battleState.moves[m_selectedBattleMove].moveId);
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

bool FirstRunRuntime::advanceBattleTurn() {
    if (!battleInputSupported()) {
        m_battleFeedback = m_doubleBattle ? "Double battle turn order unsupported" :
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

    // Pinned EnemyPokemon.SMART_RANDOM: score each usable move in moveset order,
    // then advance through the descending pool while randBattleSeedInt(8) >= 5.
    uint8_t candidates[4]{};
    double scores[4]{};
    uint8_t candidateCount = 0;
    for (uint8_t i = 0; i < enemyState.moveCount; ++i) {
        if (!enemyState.moves[i].pp) continue;
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
    uint8_t chosenIndex = 0;
    while (chosenIndex + 1 < candidateCount && rng->randSeedInt(8) >= 5) ++chosenIndex;
    const uint8_t enemyMoveSlot = candidates[chosenIndex];
    const auto* enemyMove = PokerogueContent::findMoveById(enemyState.moves[enemyMoveSlot].moveId);
    if (!enemyMove) {
        m_battleFeedback = "Canonical enemy move reference invalid";
        buildScene();
        return false;
    }

    const auto firstMover = resolveBaselineFirstMover(playerState, enemyState,
        selected->id, enemyMove->id, m_seedCodeUnits.data(), m_seedLength,
        m_run.wave, m_turn);
    if (firstMover == BaselineFirstMover::Invalid) {
        m_battleFeedback = "Turn order inputs unsupported";
        buildScene();
        return false;
    }
    m_runStarted = true;
    m_checkpointAvailable = false;

    const auto act = [&](bool enemyActs) -> bool {
        PokemonMoveActionResult result{};
        if (enemyActs) {
            if (!m_context.player.battleState.hp) return true;
            const auto status = useStandardPokemonMove(m_context.enemy.battleState,
                m_context.player.battleState, enemyMoveSlot, false, *rng, result);
            if (status != PokemonMoveActionStatus::Ok) return false;
            m_battleFeedback = result.damageRoll.hit
                ? "Wild move hit" : "Wild move missed";
        } else {
            if (!m_context.enemy.battleState.hp) return true;
            const auto status = useStandardPokemonMove(m_context.player.battleState,
                m_context.enemy.battleState, m_selectedBattleMove, false, *rng, result);
            if (status != PokemonMoveActionStatus::Ok) return false;
            m_battleFeedback = result.damageRoll.hit ? "Your move hit" : "Your move missed";
        }
        return true;
    };
    const bool enemyFirst = firstMover == BaselineFirstMover::Enemy;
    if (!act(enemyFirst)) {
        m_battleFeedback = enemyFirst ? "Wild move resolution failed" : "Player move resolution failed";
        buildScene();
        return false;
    }
    if (m_context.player.battleState.hp && m_context.enemy.battleState.hp && !act(!enemyFirst)) {
        m_battleFeedback = enemyFirst ? "Player move resolution failed" : "Wild move resolution failed";
        buildScene();
        return false;
    }

    if (!m_context.enemy.battleState.hp || !m_context.player.battleState.hp) {
        m_battleFinished = true;
        m_playerWon = m_context.enemy.battleState.hp == 0;
        m_victoryPlan = {};
        if (m_playerWon && !planClassicVictory(m_run.wave, m_victoryPlan)) {
            m_battleFeedback = "Classic victory plan unavailable";
            buildScene();
            return false;
        }
        m_battleFeedback = m_playerWon ? "Wild battle won - rewards pending" : "Pokemon fainted - run end pending";
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

void FirstRunRuntime::resolve() {
    m_selectedBattleMove = 0;
    m_turn = 1;
    m_battleFinished = false;
    m_playerWon = false;
    m_victoryPlan = {};
    m_runStarted = false;
    m_checkpointAvailable = true;
    m_battleFeedback.clear();
    m_encounterResolved = false;
    m_doubleBattle = false;
    m_secondEncounterResolved = false;
    m_context.enemy = {};
    m_context.secondEnemy = {};
    const auto& starter = PokerogueContent::kSpecies[m_starterIndex];
    m_run.starterDex = starter.dex;
    m_context.modeName = locale("gameMode:classic", "Classic");
    const std::string biomeLocaleId = std::string("biomes:") + PokerogueContent::kStartingBiomeId;
    m_context.biomeName = locale(biomeLocaleId.c_str(), "Town");
    const std::string starterLocaleId = std::string("pokemon:") + starter.id;
    m_context.player = {starter.dex, 5, starter.id, locale(starterLocaleId.c_str(), starter.name), starter.firstFormId, starter.assetSourcePath};
    if (pokemonTotalExperienceForLevel(starter.growthRate, 5, m_playerExperience)
        != PokemonExperienceResult::Ok) return;
    const ClassicWaveKind waveKind = classifyClassicWave(m_run.wave);
    if (waveKind != ClassicWaveKind::RegularWild) {
        m_battleFeedback = waveKind == ClassicWaveKind::Invalid
            ? "Invalid Classic wave"
            : "This Classic trainer or boss encounter is not ported yet";
        buildScene();
        return;
    }
    // Fresh-profile save data starts with no unlocked egg moves and no saved
    // move preferences. Resolve its actual level-1-to-5 learnset through the
    // same canonical catalog as the wild actor; do not invent a starter list.
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

    uint8_t cycleOffset = 0;
    if (!PokerogueWaveClock::deriveCycleOffset(m_seedCodeUnits.data(), m_seedLength, cycleOffset)) return;
    const auto time = PokerogueWaveClock::timeOfDay(m_run.wave, cycleOffset);

    // BattleScene.resetSeed(1) establishes a fresh wave stream. Classic's
    // wave-1 checkIsDouble consumes its first draw before Arena.randomSpecies.
    PokerogueRngAdapter waveRng;
    PokerogueSeedOffsetScope waveScope(waveRng, m_seedCodeUnits.data(), m_seedLength, m_run.wave);
    if (!waveScope.valid()) return;
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
    m_encounterResolved = true;
}

void FirstRunRuntime::buildScene() {
    char line[96];
    m_text[0] = "POKEROGUE 3DS - RUN SETUP";
    m_text[1] = std::string("Mode: ") + m_context.modeName;
    m_text[2] = std::string("Biome: ") + m_context.biomeName;
    m_text[3] = std::string("Starter: ") + starterName();
    if (m_encounterResolved) {
        std::snprintf(line, sizeof(line), "Encounter: %s Lv. %u", m_context.enemy.localizedName,
                      static_cast<unsigned>(m_context.enemy.level));
    } else {
        std::snprintf(line, sizeof(line), "Wave 1 encounter: UNSUPPORTED");
    }
    m_text[4] = line;
    m_text[5] = m_runStarted ? "A: fight  UP/DOWN: move" :
        "A: fight  UP/DOWN: move  LEFT/RIGHT: starter";
    if (m_doubleBattle && m_secondEncounterResolved) {
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
            ? (m_victoryPlan.contains(ClassicVictoryStep::SelectModifier)
                ? "EXP + item choice pending"
                : "EXP + fixed reward pending")
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
