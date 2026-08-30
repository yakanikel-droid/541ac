# mod-auto-learn-spells

AzerothCore 3.3.5a module that learns only the explicitly configured ordinary class spells.

## Behavior

- Uses a verified `class -> level -> spell ID` table.
- Checks spells on login.
- Checks spells after level increases, including multi-level boosts.
- Removes configured spells above the current level after a level reduction.
- Restores the highest configured rank allowed at the reduced level.
- Reactivates inactive lower ranks immediately; relogging is not required.
- Can repair an offline/manual level reduction during login synchronization.
- Does not give shaman totem items.
- Does not scan every entry in `Spell.dbc`.
- Rejects missing, invalid, disabled, or improperly ranked spells.
- Contains no SQL updates.

Only spell IDs present in this module's class table are removed. Talent,
racial, profession, quest and unrelated custom spells are not touched.

## Custom Death Knight progression

- Death Knights learn a playable base kit from level 1 for the Hyjal campaign.
- Former Acherus quest abilities are learned automatically by level.
- Runeforging is learned at level 10.
- Death Gate is learned at level 20.
- Acherus quests are not completed or rewarded by this module.

## Installation

1. Copy this directory to `azerothcore-wotlk/modules/mod-auto-learn-spells`.
2. Delete or reconfigure the build directory.
3. Run CMake and rebuild the core.
4. Copy `mod_auto_learn_spells.conf.dist` through the normal AzerothCore configuration install step.
5. Disable the old Lua version to avoid duplicate handlers.

## Important

The included table was generated from `auto_learn_spell_fixed.lua`, which was checked against the supplied `Spell.dbc`, `SkillLineAbility.dbc`, and `spell_ranks.sql`.
