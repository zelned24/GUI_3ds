#pragma once

#include "screens/SceneData.hpp"
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
};

struct PresentationContext {
    const char* modeName;
    const char* biomeName;
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
    const Citro2D::SceneDefinition& scene() const { return m_scene; }
    const RunState& run() const { return m_run; }
    const PresentationContext& presentation() const { return m_context; }
    const char* starterName() const;

private:
    void resolve();
    void buildScene();
    static const char* locale(const char* canonicalId, const char* fallback);

    RunState m_run{};
    PresentationContext m_context{};
    Citro2D::SceneDefinition m_scene{};
    std::array<Citro2D::SceneNodeData, 8> m_nodes{};
    std::array<std::string, 8> m_text{};
    std::size_t m_starterIndex = 0;
    std::array<uint16_t, 10> m_seedCodeUnits{};
    std::size_t m_seedLength = 0;
    bool m_encounterResolved = false;
    bool m_doubleBattle = false;
    bool m_secondEncounterResolved = false;
};

} // namespace Pokerogue3DS
