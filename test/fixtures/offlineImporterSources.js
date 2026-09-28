/** Offline-only TypeScript snippets used to exercise importer parsing. Never production data. */
export function getOfflineImporterFixtureSources() {
  return {
    species: `// Upstream PokéRogue generation-01.ts
export const generationOneSpeciesData = {
  [SpeciesId.PIKACHU]: {
    speciesId: 25,
    name: 'Pikachu',
    generation: 1,
    type1: Type.ELECTRIC,
    type2: Type.NONE,
    baseStats: [35, 55, 40, 50, 50, 90],
    ability1: AbilityId.STATIC,
    abilityHidden: AbilityId.LIGHTNING_ROD,
    passive: AbilityId.MOTOR_DRIVE,
    height: 0.4,
    weight: 6.0,
    levelMoves: [
      [1, Moves.TACKLE],
      [5, Moves.QUICK_ATTACK],
      [9, Moves.THUNDERBOLT]
    ],
    eggMoves: [Moves.VOLT_TACKLE, Moves.FAKE_OUT, Moves.EXTREME_SPEED, Moves.WISH]
  },
  [SpeciesId.GOLEM]: {
    speciesId: 76,
    name: 'Golem',
    generation: 1,
    type1: Type.ROCK,
    type2: Type.GROUND,
    baseStats: [80, 120, 130, 55, 65, 45],
    ability1: AbilityId.ROCK_HEAD,
    ability2: AbilityId.STURDY,
    abilityHidden: AbilityId.SAND_VEIL,
    passive: AbilityId.SOLID_ROCK,
    height: 1.4,
    weight: 300.0,
    levelMoves: [
      [1, Moves.TACKLE],
      [16, Moves.ROCK_SLIDE],
      [32, Moves.EARTHQUAKE]
    ],
    eggMoves: [Moves.ACCELEROCK, Moves.HEAD_SMASH, Moves.SHORE_UP, Moves.DIAMOND_STORM]
  }
};`,
    moves: `// Upstream PokéRogue move.ts
export const movesData = {
  [Moves.THUNDERBOLT]: {
    id: 85,
    name: 'Thunderbolt',
    type: Type.ELECTRIC,
    category: MoveCategory.SPECIAL,
    power: 90,
    accuracy: 100,
    pp: 15,
    priority: 0,
    flags: { contact: false, protectable: true },
    secondaryEffects: [{ chance: 10, status: 'PARALYSIS' }]
  },
  [Moves.TACKLE]: {
    id: 33,
    name: 'Tackle',
    type: Type.NORMAL,
    category: MoveCategory.PHYSICAL,
    power: 40,
    accuracy: 100,
    pp: 35,
    priority: 0,
    flags: { contact: true, protectable: true }
  },
  [Moves.QUICK_ATTACK]: {
    id: 98,
    name: 'Quick Attack',
    type: Type.NORMAL,
    category: MoveCategory.PHYSICAL,
    power: 40,
    accuracy: 100,
    pp: 30,
    priority: 1,
    flags: { contact: true, protectable: true }
  },
  [Moves.EARTHQUAKE]: {
    id: 89,
    name: 'Earthquake',
    type: Type.GROUND,
    category: MoveCategory.PHYSICAL,
    power: 100,
    accuracy: 100,
    pp: 10,
    priority: 0,
    flags: { contact: false, protectable: true }
  },
  [Moves.ROCK_SLIDE]: {
    id: 157,
    name: 'Rock Slide',
    type: Type.ROCK,
    category: MoveCategory.PHYSICAL,
    power: 75,
    accuracy: 90,
    pp: 10,
    priority: 0,
    flags: { contact: false, protectable: true }
  }
};`,
    abilities: `// Upstream PokéRogue init-abilities.ts
export const abilitiesData = {
  [Abilities.STATIC]: {
    id: 'static',
    name: 'Static',
    description: 'The Pokémon is charged with static electricity, so contact with it may cause paralysis.',
    trigger: 'ON_DAMAGE_RECEIVED',
    conditions: [{ key: 'contact', value: true }],
    effects: [{ action: 'APPLY_STATUS', status: 'PARALYSIS', chance: 30 }]
  },
  [Abilities.STURDY]: {
    id: 'sturdy',
    name: 'Sturdy',
    description: 'It cannot be knocked out with one hit if at full HP.',
    trigger: 'ON_DAMAGE_PREVENTION',
    conditions: [{ key: 'hpRatio', value: 1.0 }],
    effects: [{ action: 'SURVIVE_LETHAL_HIT', minHP: 1 }]
  }
};`,
  };
}
