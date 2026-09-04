# Test fixtures

Save files used by the test suite (`tests/core_test.cpp`) and for manual
verification of the converter. All files were created with the original game
(Heroes of Might and Magic II, GOG release, version 1.01/2.1) in DOSBox
Staging and are committed for testing only.

## Standard saves (`.GM1`)

Map: **Slugfest** (36×36, The Succession Wars), red player (Sorceress Troyan
in Woodhaven), Normal difficulty, 6 players. Each file differs from the
previous one by a single in-game action (diff pairs).

| File | In-game action |
|---|---|
| `BASE.GM1` | Turn 1, no actions taken |
| `ARMY5.GM1` | Cheat `32167` — 5 Black Dragons added to the main hero |
| `ARMY10.GM1` | Cheat `32167` again — 10 Black Dragons |
| `GOLD.GM1` | Cheat attempt (no effect — identical to ARMY10) |
| `BUILD.GM1` | Upg. Cottage built (1500 gold, 5 wood) |
| `MOVE.GM1` | One step south |
| `MOVE2.GM1` | Another step south |
| `MOVE3.GM1` | One step east |
| `DAY2.GM1` | End Turn (day 2, AI income/moves) |
| `HERO2.GM1` | Luna the Sorceress hired (2500 gold) |
| `ARMY2.GM1` | 21 Elves recruited into Luna's army |
| `SPELL.GM1` | Mage Guild level 2 built, 3 new spells for Luna |
| `AUTOSAVE.GM1` | Autosave made at End Turn |

## Campaign saves

| File | Campaign |
|---|---|
| `NEWGAME1.GMC` | The Succession Wars (Roland), first scenario, 72×72 map |
| `NEWGAME2.GXC` | The Price of Loyalty (expansion), first scenario, 36×36 map |

The save layout is documented in
[`docs/HOMM2_SAVE_FORMAT.md`](../docs/HOMM2_SAVE_FORMAT.md);
`tools/analyze.py` parses these files and prints a summary.
