# HoMM2 save file format (`.GM1` / `.GMC` / `.GXC`)

The save format of the original Heroes of Might and Magic II (The Succession
Wars and The Price of Loyalty), recovered from the project-ironfist
decompilation (`game::SaveGame`, `playerData::Write`, `hero::Write`,
`fullMap::Write` in HEROES2W.c / HEROES2W.h) and verified byte-for-byte
against real saves (`tests/fixtures/*`, diff pairs with a single in-game
change). `tools/analyze.py` is a working reference parser.

All multi-byte values are little-endian. Structures are packed (alignment 1).

## 1. File layout (game::SaveGame)

The file is a sequential dump of sections. Offsets below are for the
**base game** (The Succession Wars maps, `baseGame = 1`). The Price of
Loyalty expansion adds bytes near the start — see [§1.1](#11-expansion-and-campaign-layouts).

| Offset | Size | Section |
|---|---|---|
| 0 | 4 | `map.width` (i32) |
| 4 | 4 | `map.height` (i32) |
| 8 | 420 | `SMapHeader` ([§2](#2-smapheader-420-bytes)) |
| 428 | 24 | `relatedToPlayerPosAndColor`, `playerHandicap`, `newGameSelectedFaction`, `somePlayerCodeOr10IfMayBeHuman` (4 × 6 bytes) |
| 452 | 1 | `difficulty` (0..4: Easy, Normal, Hard, Expert, Impossible) |
| 453 | 14 | `mapFilename` (8.3, e.g. `SLUGFEST.MP2`) |
| 467 | 6 | `somePlayerNumData[6]` |
| 473 | 1 | `relatedToNewGameSelection` |
| 474 | 1 | `relatedToNewGameInit` |
| 475 | 1 | `numHumanPlayers` |
| 476 | 17 | `field_47C` (17 bytes) |
| 493 | 1 | `gbIAmGreatest` |
| 494 | 2 | `gameDifficulty` (i16; 100 = Normal, a percent rating) |
| 496 | 4 | `giMonthType`, `giMonthTypeExtra`, `giWeekType`, `giWeekTypeExtra` |
| 500 | 126 | player names `cPlayerNames` (6 × 21 bytes, e.g. "Red player") |
| 626 | 36 | zeroes |
| 662 | 4 | `gbInCampaign` (0 = standard, 1 = SW campaign, 2 = PoL campaign) |
| 666 | 4 | `giMapChangeCtr` |
| 670 | 14 | save file name (8.3), `GenerateStandardFileName(lastSaveFile)` |
| 684 | 1 | `numPlayers` |
| 685 | 1 | `giCurPlayer` |
| 686 | 1 | `couldBeNumDefeatedPlayers` |
| 687 | 6 | `playerDead[6]` |
| 693 | 6 | `playerAlive[6]` (alive flags) |
| 699 | 2 | `day` (i16) |
| 701 | 2 | `week` (i16) |
| 703 | 2 | `month` (i16) |
| 705 | 6×207 | `players[6]` — `playerData::Write` ([§3](#3-playerdata-207-bytes)) |
| 1947 | 1 | `numObelisks` |
| 1948 | 54×236 | `heroes[54]` — `hero::Write` ([§4](#4-hero-236-bytes)) |
| 14692 | 54 | `relatedToHeroForHireStatus` |
| 14746 | 72×100 | `castles[72]` ([§5](#5-towncastle-100-bytes)) |
| 21946 | 72 | `field_2773` |
| 22018 | 9 | `field_27BB` |
| 22027 | 144×7 | `mines[144]` ([§6](#6-mine-7-bytes-and-boat-8-bytes)) |
| 23035 | 144 | `field_60A6` |
| 23179 | 82 | `artifactGeneratedRandomly` (103 in PoL) |
| 23261 | 48×8 | `boats[48]` |
| 23645 | 48 | `boatBuilt` |
| 23693 | 48 | `obeliskVisitedMasks` |
| 23741 | 3 | `ultimateArtifactLocX`, `ultimateArtifactLocY`, `ultimateArtifactIdx` |
| 23744 | 301 | `currentRumor` |
| 24045 | 24 | `field_637D` |
| 24069 | 4 | `numRumors` (i32) |
| 24073 | 4 | `numEvents` |
| 24077 | 4 | `numMapEvents` |
| 24081 | 4 | marker `1234` |
| 24085 | 4 | `iMaxMapExtra` |
| 24089 | 4 | marker `1234` |
| 24093 | … | map extras: for i = 1 … iMaxMapExtra−1: marker `1234` (4) + `size` (u16) + data |
| … | 4 | marker `1234` |
| … | w×h | `mapRevealed` (one byte per map tile) |
| … | 4 | marker `1234` |
| … | 8+… | `fullMap::Write`: `width` (4), `height` (4), tiles `mapCell`×w×h (12 bytes, [§7](#7-map-cells-mapcell-and-mapcellextra)), `numCellExtras` (4), `cellExtras`×n (7 bytes) |
| … | 4 | marker `1234` — end of file |

`numRumors` / `numEvents` / `numMapEvents` are followed by index arrays of
`2 × num` bytes each; in all collected saves these counters are 0.

### 1.1 Expansion and campaign layouts

Three layout variants exist, selected by `gbInCampaign` and the 4-byte
expansion marker at the very start of the file:

| Variant | Extra bytes | Where |
|---|---|---|
| Standard (`.GM1`) | — | — |
| PoL map (`.GX1`…) | `FF FF FF FF` at offset 0, `xIsExpansionMap` (1 byte) after the campaign block | all offsets shift by +4 |
| SW campaign (`.GMC`) | campaign block of 327 bytes after `gbInCampaign` | `gbInCampaign = 1`; everything after offset 662 shifts by +327 |
| PoL campaign (`.GXC`) | expansion marker + `xCampaign` (79 bytes) + `xIsExpansionMap` (1 byte) | `gbInCampaign = 2`; everything shifts by +4 +79 +1 = +84 |

Verified on real files: `NEWGAME1.GMC` (72×72, `day@1026 = 699+327`),
`NEWGAME2.GXC` (36×36, `day@783 = 699+84`).

PoL also changes record sizes in the tail: `artifactGeneratedRandomly` is
103 bytes instead of 82, and `hero` records are 250 bytes (236 + the 14-byte
`scrollSpell` array).

## 2. SMapHeader (420 bytes)

| Off | Size | Field |
|---|---|---|
| 0 | 4 | field_0 (usually 92) |
| 4 | 2 | field_4 |
| 6 | 1 | width |
| 7 | 1 | height |
| 8 | 6 | `hasPlayer[6]` |
| 14 | 6 | `playerMayBeHuman[6]` |
| 20 | 6 | `playerMayBeComp[6]` |
| 26 | 1 | `numPlayers` |
| 27 | 1 | `minHumans` |
| 28 | 1 | `maxHumans` |
| 29 | 1 | `winConditionType` |
| 30 | 1 | related to win condition type |
| 31 | 1 | `allowDefeatAllVictory` |
| 32 | 2 | win condition argument or location X |
| 34 | 1 | `lossConditionType` |
| 35 | 1 | loss condition argument or location X |
| 36 | 1 | field_24 |
| 37 | 1 | `noStartingHeroInCastle` |
| 38 | 6 | `playerFactions[6]` |
| 44 | 2 | win condition argument or location Y |
| 46 | 2 | loss condition argument or location Y |
| 48 | 2 | related to player color/side |
| 50 | 4 | field_32 |
| 54 | 4 | field_36, field_37, nextTownName, field_39 |
| 58 | 60 | `name[60]` — map name |
| 118 | 298 | `description[298]` — scenario description |
| 416 | 4 | field_1A0, field_1A1, numRumors, numEvents |

## 3. playerData (207 bytes, playerData::Write)

| Off | Size | Field |
|---|---|---|
| 0 | 1 | `color` (0 = blue, 1 = green, 2 = red, 3 = yellow, 4 = orange, 5 = purple) |
| 1 | 1 | `numHeroes` |
| 2 | 1 | `curHeroIdx` (index into heroes[54], 0xFF = none) |
| 3 | 1 | related to hero count/index |
| 4 | 8 | `heroesOwned[8]` — hero indices (0xFF = empty) |
| 12 | 2 | `heroesForPurchase[2]` |
| 14 | 42 | zeroes |
| 56 | 1 | `gpGame->_B[1]` — global flag; becomes 1 after the first cheat code |
| 57 | 1 | `_3[0]` |
| 58 | 4 | `personality` |
| 62 | 1 | related to max number of heroes |
| 63 | 1 | `hasEvilFaction` |
| 64 | 3 | field_40 … field_42 |
| 67 | 1 | `daysLeftWithoutCastle` |
| 68 | 1 | `numCastles` |
| 69 | 1 | might be current castle index |
| 70 | 1 | related to unknown |
| 71 | 72 | `castlesOwned[72]` — castle indices (0xFF = empty) |
| 143 | 28 | **`resources[7]`** — u32 each: wood, mercury, ore, sulfur, crystal, gems, gold |
| 171 | 28 | field_E7[7] (u32 each) |
| 199 | 2 | barrierTentsVisited (written twice) |
| 201 | 6 | `_4_2` |

## 4. hero (236 bytes base / 250 PoL, hero::Write)

| Off | Size | Field |
|---|---|---|
| 0 | 2 | `spellpoints` (i16) |
| 2 | 1 | `idx` |
| 3 | 1 | `ownerIdx` (0xFF = not hired) |
| 4 | 6 | field_4 … field_9 |
| 10 | 13 | `name[13]` |
| 23 | 1 | `factionID` (0 = Knight … 5 = Necromancer) |
| 24 | 1 | `heroID` |
| 25 | 4 | `x` (i32) |
| 29 | 4 | `y` (i32) |
| 33 | 8 | field_21, field_23, field_25, field_27 (i16 each) |
| 41 | 4 | relatedToX, relatedToY, relatedToFactionID, directionFacing |
| 45 | 2 | `occupiedObjType` (i16) |
| 47 | 2 | `occupiedObjVal` (i16) |
| 49 | 4 | `mobility` (i32) |
| 53 | 4 | `remainingMobility` (i32) |
| 57 | 4 | `experience` (i32) |
| 61 | 2 | `oldLevel` (i16) |
| 63 | 4 | `primarySkills[4]`: attack, defense, spellpower, knowledge |
| 67 | 4 | field_43, tempMoraleBonuses, tempLuckBonuses, field_46 |
| 71 | 28 | visitation counters (gazeboes, forts, witchDoctorHuts, mercenaryCamps, standingStones, treesOfKnowledge, xanadus) — i32 × 7 |
| 99 | 1 | `randomSeed` |
| 100 | 1 | `wisdomLastOffered` |
| 101 | 5 | **army: `creatureTypes[5]`** (0xFF = empty slot) |
| 106 | 10 | **army: `quantities[5]`** (u16) |
| 116 | 14 | `secondarySkillLevel[14]` (0 = none, 1–3 = basic/advanced/expert) |
| 130 | 14 | `skillIndex[14]` (skill id, [§8.4](#84-secondary-skills)) |
| 144 | 4 | `numSecSkillsKnown` |
| 148 | 65 | **`spellsLearned[65]`** — byte flags, index = spell id ([§8.2](#82-spells)) |
| 213 | 14 | **`artifacts[14]`** (0xFF = empty, 81 = Magic Book) |
| 227 | 4 | `flags` |
| 231 | 1 | `isCaptain` |
| 232 | 4 | field_E8 |
| 236 | 14 | `scrollSpell[14]` — **PoL only** |

## 5. town / castle (100 bytes)

| Off | Size | Field |
|---|---|---|
| 0 | 1 | `idx` |
| 1 | 1 | `ownerIdx` |
| 2 | 1 | alignment |
| 3 | 1 | `factionID` |
| 4 | 1 | `x` |
| 5 | 1 | `y` |
| 6 | 1 | buildDockRelated |
| 7 | 1 | field_7 |
| 8 | 5 | garrison: `garrison.creatureTypes[5]` |
| 13 | 10 | garrison: `garrison.quantities[5]` (u16) |
| 23 | 1 | `visitingHeroIdx` (0xFF = nobody) |
| 24 | 4 | `buildingsBuiltFlags` (bitmask) |
| 28 | 1 | `mageGuildLevel` |
| 29 | 1 | field_1D |
| 30 | 24 | `numCreaturesInDwelling[12]` (u16 each) |
| 54 | 1 | `exists` |
| 55 | 1 | `mayNotBeUpgradedToCastle` |
| 56 | 1 | field_38 |
| 57 | 1 | `playerPos` |
| 58 | 2 | `extraIdx` |
| 60 | 20 | `mageGuildSpells[5][4]` — guild spells per level |
| 80 | 5 | `numSpellsOfLevel[5]` |
| 85 | 2 | field_55 |
| 87 | 12 | `name[12]` |
| 99 | 1 | field_63 |

## 6. mine (7 bytes) and boat (8 bytes)

mine: field_0, `owner`, `type`, `guardianType`, `guardianQty`, `x`, `y`.
boat: `idx`, `x`, `y`, field_3, underlyingObjType, underlyingObjExtra, field_6, `owner`.

## 7. Map cells: mapCell (12 bytes) and mapCellExtra (7 bytes)

mapCell:

| Off | Size | Field |
|---|---|---|
| 0 | 2 | `groundIndex` |
| 2 | 1 | bitfield: hasObject (1) / isRoad (1) / objTileset (6) |
| 3 | 1 | `objectIndex` |
| 4 | 2 | bitfield: field_4_1 (1) / isShadow (1) / field_4_3 (1) / **extraInfo (13)** |
| 6 | 1 | bitfield: hasOverlay (1) / hasLateOverlay (1) / overlayTileset (6) |
| 7 | 1 | `overlayIndex` |
| 8 | 1 | flags: flip sprite vertically (0x01) / horizontally (0x02), hasActiveHero (0x40) |
| 9 | 1 | `objType`; TILE_HAS_EVENT (0x80) marks the interactive tile of an object |
| 10 | 2 | `extraIdx` — chain of mapCellExtra records on the same tile (0 = none) |

`extraInfo`, the 13-bit field at bits 3..15 of the offset-4 word
(`word >> 3`), carries the per-object data:

| objType (with TILE_HAS_EVENT) | extraInfo meaning |
|---|---|
| treasure chest (134) | gold = extraInfo × 500 (2/3/4 → 1000/1500/2000; sprite is 2 tiles: frame 18 on the left cell, 19 = OBJNRSRC) |
| resource pile (155) | amount; gold is counted in hundreds (extraInfo × 100) |
| monster (152) | stack size; the engine reads it as an unsigned char (`extraInfo & 0xFF`) |
| shrine (159/202/203) / pyramid (204) | spell id + 1 |
| campfire (136) | (count << 4) \| resourceIdx, count 4..6, resourceIdx: 0 wood, 1 mercury, 2 ore, 3 sulfur, 4 crystal, 5 gems |
| barrier (247) / traveller tent (248) | barrier color = extraInfo & 7, password index = extraInfo >> 3 |
| witch's hut (213) | secondary skill id (0-based, optional) |
| random ultimate artifact (172) | search radius; the artifact is chosen by the game |

mapCellExtra (7 bytes): `nextIdx` (2), animatedObject (1), objTileset (7),
`objectIndex` (1), field_4_1/field_4_2/field_4_3/field_4_4 (1/1/1/5),
animatedLateOverlay (1), hasLateOverlay (1), tileset (6), `overlayIndex` (1).

Tiles are stored row by row (`y × width + x`).

## 8. Reference tables

### 8.1 Creatures (id = byte in creatureTypes)

0 Peasant … 10 Crusader, 11 Goblin, 12 Orc, 13 OrcChief, 14 Wolf,
15 Ogre, 16 OgreLord, 17 Troll, 18 WarTroll, 19 Cyclops,
20 Sprite, 21 Dwarf, 22 BattleDwarf, **23 Elf**, 24 GrandElf,
25 Druid, 26 GreaterDruid, 27 Unicorn, 28 Phoenix,
29 Centaur, 30 Gargoyle, 31 Griffin, 32 Minotaur, 33 MinotaurKing,
34 Hydra, 35 GreenDragon, 36 RedDragon, **37 BlackDragon**,
38 Halfling, 39 Boar, 40 IronGolem, 41 SteelGolem, 42 Roc,
43 Mage, 44 ArchMage, 45 Giant, 46 Titan,
47 Skeleton, 48 Zombie, 49 MutantZombie, 50 Mummy, 51 RoyalMummy,
52 Vampire, 53 VampireLord, 54 Lich, 55 PowerLich, 56 BoneDragon,
57 Rogue, 58 Nomad, 59 Ghost, 60 Genie, 61 Medusa,
62–65 elementals (Earth/Air/Fire/Water).

### 8.2 Spells (0–64; byte flag in spellsLearned)

0 Fireball … 25 MagicArrow, 26 Berzerker, 27 Armageddon, 28 ElementalStorm,
29 MeteorShower, 30 Paralyze, 31 Hypnotize, 32 ColdRay, 33 ColdRing,
34 DisruptingRay, 35 DeathRipple, 36 DeathWave, 37 DragonSlayer,
38 BloodLust, 39 AnimateDead, 40 MirrorImage, 41 Shield, 42 MassShield,
43–46 summon elementals, 47 Earthquake,
48 ViewMines … 53 ViewAll, 54 Identify, 55 SummonBoat,
56 DimensionDoor, 57 TownGate, 58 TownPortal, 59 Visions, 60 Haunt,
61–64 Set*Guardian.

### 8.3 Artifacts (id in artifacts[14])

81 MagicBook, 86 SpellScroll, 87 ArmOfTheMartyr, 88 BreastplateOfAnduran,
89 BroachOfShielding, 90 BattleGarbOfAnduran, 91 CrystalBall, 92 HeartOfFire,
93 HeartOfIce, 94 HelmetOfAnduran, 95 HolyHammer, 96 LegendaryScepter,
97 Masthead, 98 SphereOfNegation, 99 StaffOfWizardry, 100 SwordBreaker,
101 SwordOfAnduran, 102 SpadeOfNecromancy. The full 0–80 range — see the
`ARTIFACT` enum in HEROES2W.h (project-ironfist).

### 8.4 Secondary skills (id in skillIndex)

0 Pathfinding, 1 Archery, 2 Logistics, 3 Scouting, 4 Diplomacy,
5 Navigation, 6 Leadership, 7 Wisdom, 8 Mysticism, 9 Luck,
10 Ballistics, 11 EagleEye, 12 Necromancy, 13 Estates.
Level in secondarySkillLevel: 1/2/3.

## 9. Verified facts (diff pairs in tests/fixtures)

- Gold `@705+143+24` of player 0: 7500 → 6000 (building), → 4750 (hiring
  Luna for 2500), → 3500 (Mage Guild for 1000) — u32 LE.
- Resources: building Mage Guild level 2 (5 wood, 4 mercury, 5 ore, 4 sulfur,
  4 crystal, 4 gems) changed exactly the six u32 values at `+143`.
- Army: cheat `32167` set `creatureTypes[0] = 37` (BlackDragon) and
  `quantities[0] = 5`; a second use set `quantities[0] = 10` (u16).
- Date: End Turn changed `day@699` from 1 to 2.
- Mage Guild: `mageGuildLevel` at castle+28 changed 1 → 2; three new spell
  flags appeared in the hero's `spellsLearned` (7 total for Luna).
- `_B[1]` at playerData+56: 0 in BASE, 1 after the first cheat — the cheat
  marker.
- Tail: with empty rumors/events the layout resolves to the byte
  (`end offset == file size` on all 15 fixtures).
- `NEWGAME1.GMC`: `gbInCampaign = 1`, 327-byte campaign block, day at +327.
  `NEWGAME2.GXC`: expansion marker, `gbInCampaign = 2`, 79-byte `xCampaign`
  block, `xIsExpansionMap = 1`, day at +84.
