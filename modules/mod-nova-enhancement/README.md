# Nova Character Enhancements

AzerothCore 3.3.5a module for the Nova character enhancement UI.

## Rules

- Every 4 achievement points grant 1 enhancement point.
- One allocated point grants +1 Strength, Agility, Stamina, Intellect, or Spirit.
- Attack Power grants +2 per allocated point.
- Spell Power grants `floor(allocated points × 1.3)`.
- Allocations are stored per character in `nova_character_enhancements`.
- The server validates every allocation; the Lua UI only displays and sends requests.
- Resetting allocations costs 300 gold by default and is configurable with
  `NovaEnhancement.ResetCostGold`.

Supported keys: `strength`, `agility`, `stamina`, `intellect`, `spirit`,
`spell_power`, `attack_power`.

## Installation

1. Copy this directory to `azerothcore-wotlk/modules/mod-nova-enhancement`.
2. Re-run CMake and rebuild the core.
3. Start `worldserver`. AzerothCore's module SQL updater installs the character
   table automatically.

## Client/server protocol

The existing client provider sends hidden `.nova data`, `.nova add <key>`, and `.nova reset`
commands. Server responses begin with `NOVA_DATA|`; the provider filters those
messages from the chat window and calls `NovaEnhancement_SetData`.
