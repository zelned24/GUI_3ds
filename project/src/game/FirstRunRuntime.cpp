#include "game/FirstRunRuntime.hpp"
#include "game/PokerogueEncounterResolver.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <algorithm>
#include <cstdio>

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
        if (PokerogueContent::kSpecies[i].starterEligible) {
            m_starterIndex = i;
            break;
        }
    }
    resolve();
    buildScene();
}

bool FirstRunRuntime::cycleStarter(int direction) {
    if (!direction || !PokerogueContent::kSpeciesCount) return false;
    std::size_t index = m_starterIndex;
    for (std::size_t scanned = 0; scanned < PokerogueContent::kSpeciesCount; ++scanned) {
        index = direction > 0
            ? (index + 1) % PokerogueContent::kSpeciesCount
            : (index + PokerogueContent::kSpeciesCount - 1) % PokerogueContent::kSpeciesCount;
        if (PokerogueContent::kSpecies[index].starterEligible) {
            m_starterIndex = index;
            resolve();
            buildScene();
            return true;
        }
    }
    return false;
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

void FirstRunRuntime::resolve() {
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

    uint8_t cycleOffset = 0;
    if (!PokerogueWaveClock::deriveCycleOffset(m_seedCodeUnits.data(), m_seedLength, cycleOffset)) return;
    const auto time = PokerogueWaveClock::timeOfDay(m_run.wave, cycleOffset);

    // BattleScene.resetSeed(1) establishes a fresh wave stream. Classic's
    // wave-1 checkIsDouble consumes its first draw before Arena.randomSpecies.
    PokerogueRngAdapter waveRng;
    PokerogueSeedOffsetScope waveScope(waveRng, m_seedCodeUnits.data(), m_seedLength, m_run.wave);
    if (!waveScope.valid()) return;
    m_doubleBattle = waveRng.randSeedInt(8) == 0; // Classic default chance, zero party luck.
    // Battle constructor runs in a separate waveIndex<<3 seed scope. Its
    // battleSeed initializer consumes 16 draws before getLevelForWave().
    PokerogueRngAdapter levelRng;
    PokerogueSeedOffsetScope levelScope(levelRng, m_seedCodeUnits.data(), m_seedLength,
                                         static_cast<uint32_t>(m_run.wave) << 3);
    if (!levelScope.valid()) return;
    for (uint8_t i = 0; i < 16; ++i) (void)levelRng.randSeedInt(62);
    const uint16_t level = PokerogueEncounterResolver::nonBossLevelForWave(m_run.wave, levelRng);
    const uint16_t secondLevel = m_doubleBattle
        ? PokerogueEncounterResolver::nonBossLevelForWave(m_run.wave, levelRng) : 0;

    // Arena.randomSpecies evaluates the level before selecting the pool and
    // uses it for the post-selection LegendLike/BST gate.
    const auto pool = PokerogueEncounterResolver::resolveNonBoss(
        "town", time, m_run.wave, waveRng);
    if (!pool.valid || !pool.speciesId) return;
    const char* resolvedSpeciesId = PokerogueEncounterResolver::resolveWildSpeciesForLevel(
        pool.speciesId, level, true, waveRng);
    if (!resolvedSpeciesId) return;

    std::size_t enemyIndex = PokerogueContent::kSpeciesCount;
    for (std::size_t i = 0; i < PokerogueContent::kSpeciesCount; ++i) {
        if (std::string(PokerogueContent::kSpecies[i].id) == resolvedSpeciesId) {
            enemyIndex = i;
            break;
        }
    }
    if (enemyIndex == PokerogueContent::kSpeciesCount) return;
    const auto& enemy = PokerogueContent::kSpecies[enemyIndex];

    m_run.encounterDex = enemy.dex;
    const std::string enemyLocaleId = std::string("pokemon:") + enemy.id;
    m_context.enemy = {enemy.dex, level, enemy.id, locale(enemyLocaleId.c_str(), enemy.name), enemy.firstFormId, enemy.assetSourcePath};
    if (m_doubleBattle) {
        const auto secondPool = PokerogueEncounterResolver::resolveNonBoss(
            "town", time, m_run.wave, waveRng);
        if (!secondPool.valid || !secondPool.speciesId) return;
        const char* secondSpeciesId = PokerogueEncounterResolver::resolveWildSpeciesForLevel(
            secondPool.speciesId, secondLevel, true, waveRng);
        if (!secondSpeciesId) return;
        std::size_t secondIndex = PokerogueContent::kSpeciesCount;
        for (std::size_t i = 0; i < PokerogueContent::kSpeciesCount; ++i) {
            if (std::string(PokerogueContent::kSpecies[i].id) == secondSpeciesId) {
                secondIndex = i;
                break;
            }
        }
        if (secondIndex == PokerogueContent::kSpeciesCount) return;
        const auto& second = PokerogueContent::kSpecies[secondIndex];
        const std::string secondLocaleId = std::string("pokemon:") + second.id;
        m_context.secondEnemy = {second.dex, secondLevel, second.id,
            locale(secondLocaleId.c_str(), second.name), second.firstFormId, second.assetSourcePath};
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
    m_text[5] = "LEFT/RIGHT: choose upstream starter";
    if (m_doubleBattle && m_secondEncounterResolved) {
        std::snprintf(line, sizeof(line), "2nd: %s Lv. %u", m_context.secondEnemy.localizedName,
                      static_cast<unsigned>(m_context.secondEnemy.level));
        m_text[6] = line;
    } else {
        m_text[6] = m_doubleBattle && m_encounterResolved
            ? "Second slot unsupported - no fallback"
            : m_encounterResolved ? "Wild evolution rules applied"
                                  : "Encounter inputs unsupported - no fallback used";
    }
    m_text[7] = std::string("Pinned data: ") + PokerogueContent::kPokerogueRevision;

    static const char* ids[] = {"run-title", "mode", "biome", "starter", "encounter", "controls", "status", "source"};
    static const float ys[] = {22.0f, 65.0f, 89.0f, 122.0f, 156.0f, 196.0f, 216.0f, 2.0f};
    for (std::size_t i = 0; i < m_nodes.size(); ++i) {
        m_nodes[i] = textNode(ids[i], i == 7 ? Citro2D::ScreenTarget::Bottom : Citro2D::ScreenTarget::Top,
                              12.0f, ys[i], static_cast<int16_t>(i));
        m_nodes[i].text = m_text[i].c_str();
    }
    m_scene = {"pokerogue-first-run", 1, 60, 0xFF18202Cu, 0xFF10151Eu,
               static_cast<uint16_t>(m_nodes.size()), m_nodes.data(),
               0, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, nullptr};
}

} // namespace Pokerogue3DS
