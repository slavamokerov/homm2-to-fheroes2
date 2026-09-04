# Conversion logic: HoMM2 → fheroes2

This document describes how the converter builds an fheroes2 save file from
an original Heroes of Might and Magic II save. The source format is
documented in [`HOMM2_SAVE_FORMAT.md`](HOMM2_SAVE_FORMAT.md); the target
format is the fheroes2 save format versions **10032–10034** (big-endian,
zlib compressed, see the `FH2_SAVE_FORMAT.md` document in the
fheroes2-save-editor project). The default is **10033** (read by fheroes2
1.1.15 and later); 10032 targets 1.1.11+, 10034 targets 1.1.80+.

## 1. What the converter does

Input: `.GM1` (standard game), `.GMC` (Succession Wars campaign),
`.GXC` (Price of Loyalty campaign). Output: `.sav` (standard) or
`.savc` (campaign).

The converter reads the whole source save, translates every supported
structure into the fheroes2 representation and writes a new fheroes2 save.
The result is meant to be loaded by fheroes2 and played as a single-player
game.

## 2. ID mapping

fheroes2 reserves `0` (`UNKNOWN`/`NONE`) at the start of every enum, so
most identifiers are shifted by +1:

| Kind | HoMM2 id | fheroes2 id |
|---|---|---|
| Creatures (army, garrisons, dwellings) | 0–65 | +1 (0=Peasant → 1=PEASANT … 37=BlackDragon → 38=BLACK_DRAGON; elementals 62–65 → 63–66) |
| Heroes (heroID, portrait) | 0–53 | +1 (0=Lord Kilburn → 1=LORDKILBURN …) |
| Secondary skills | 0–13 | +1 (0=Pathfinding → 1=PATHFINDING …) |
| Spells | 0–64 | +1 (0=Fireball → 1=FIREBALL … 64=SetWaterGuardian → 65=SETWGUARDIAN) |
| Artifacts | 0–102 | +1 (0=UltimateBook → 1=ULTIMATE_BOOK; 81=MagicBook → 82=MAGIC_BOOK; 86=SpellScroll → 87=SPELL_SCROLL …) |
| Race/faction | 0–5 | bitmask `1 << faction` (0=Knight → KNGT=1, 1=Barbarian → BARB=2, 2=Sorceress → SORC=4, 3=Warlock → WRLK=8, 4=Wizard → WZRD=16, 5=Necromancer → NECR=32) |
| Player color | 0–5 | bitmask `1 << color` (0=blue → BLUE=1 … 5=purple → PURPLE=32) |
| Castle buildings | bitmask | bit-to-bit mapping by building type (see [§5.3](#53-castles)) |

## 3. What is converted

### 3.1 File header

- Map size, map name, description, difficulty, number of players, victory
  and loss conditions, and the player race table come from `SMapHeader`.
- `worldDay` / `worldWeek` / `worldMonth` come from the source `day` /
  `week` / `month`.
- `gameType`: standard saves become `TYPE_STANDARD`; campaign saves become
  `TYPE_CAMPAIGN`.
- `version` (GameVersion): Succession Wars or Price of Loyalty according to
  the input file layout.

### 3.2 Heroes

All 54 source hero records are mapped into the 73-entry `AllHeroes` list
(the remaining entries are empty):

| fheroes2 field | Source |
|---|---|
| attack / defense / knowledge / power | `primarySkills[4]` (note: fheroes2 writes knowledge before power) |
| center | `x` / `y` (as two i16) |
| spell points | `spellpoints` |
| move points | `remainingMobility` |
| spell book | `spellsLearned[65]` → spell id list |
| artifact bag | `artifacts[14]` (0xFF → empty) |
| name | `name[13]` (converted to the same encoding fheroes2 uses, CP1251 for Russian) |
| color | player color bitmask (heroes not hired keep `NONE`) |
| experience | `experience` |
| secondary skills | `skillIndex`/`secondarySkillLevel` pairs |
| army | `creatureTypes[5]` + `quantities[5]` (empty slot → id 0, count 0) |
| hero id / portrait | `heroID` +1; portrait kept if the hero was customized |
| race | faction bitmask |
| object under hero | `occupiedObjType` |
| path / visited objects / patrol | empty (a hero standing still; fheroes2 recalculates movement) |

### 3.3 Kingdoms

One kingdom per alive source player: color, the 7 resources (wood …
gold), castle list, hero list (hired heroes), and the puzzle map. Neutral
(not hired) heroes go into the neutral kingdom with color `NONE`, the same
way fheroes2 stores recruits.

### 3.4 World

- Map tiles: every source `mapCell` becomes an fheroes2 `Tile`
  ([§5.1](#51-map-tiles)).
- Fog of war: the source `mapRevealed` bytes become per-tile fog
  (`_fogColors`).
- Castles, mines (captured objects), boats, obelisks, the ultimate artifact
  location, rumors and map events are translated where an fheroes2
  counterpart exists.

### 3.5 Campaigns

`.GMC` / `.GXC` → `.savc`:

- The 327-byte campaign block (Succession Wars) and the 79-byte
  `xCampaign` block (Price of Loyalty) provide the current campaign, the
  current scenario and the progress data.
- These are mapped onto fheroes2's `CampaignSaveData`: `campaignId`,
  `scenarioId`, `finishedMaps`, `bonusesForFinishedMaps`, `daysPassed`,
  `obtainedCampaignAwards`, `carryOverTroops`, `difficulty` and
  `minDifficulty`.
- Campaign scenario IDs are translated per table between the original
  campaigns (Roland / Archibald / PoL campaigns) and fheroes2's
  `Campaign::ScenarioId` enumeration.

## 4. What may differ or be lost

The two games are different implementations; the conversion is lossy in
places. The web UI shows this list before converting.

**Not carried over:**

- **Cheat flag** — the original game marks the save as cheated
  (`playerData+56`); fheroes2 has no such flag, it is dropped.
- **High scores** — the fheroes2 score table starts empty.
- **Multiplayer / hot-seat saves** — not supported; only single-player
  saves convert.
- **AI internals** — player personalities and AI-only fields are set to
  defaults.
- **Rumors, adventure map events, sign and sphinx texts** — fheroes2 keeps
  them in its map objects section, which the converter does not populate;
  they are dropped.
- **Barrier passwords** — fheroes2 generates its own; only the barrier
  *color* is kept.
- **The save file name** — the converted file gets the name you download
  it as.

**Simplified or approximated:**

- **Movement paths and visited objects** of heroes are cleared; the game
  recalculates movement points on the next day.
- **Some map objects and overlays** without an fheroes2 counterpart become
  their closest equivalents or plain tiles.
- **PoL spell scrolls** are translated where fheroes2 supports the
  corresponding scroll artifact.
- **Castle building flags** are mapped bit-by-bit; buildings unknown to
  fheroes2 are omitted.

**May look or behave differently:**

- The save is written for a fheroes2 format (10032–10034, 10033 by
  default) — it can no longer be loaded by the original game.
- Balance, AI behavior and visuals come from fheroes2, so gameplay details
  (combat odds, AI decisions, some object behaviors) may differ from the
  original.
- Campaign bonuses granted by the original game between scenarios are
  converted where possible; bonuses specific to the original campaigns may
  not appear.

## 5. Detailed notes

### 5.1 Map tiles

A source `mapCell` (12 bytes) carries the ground type, road flag, an
object index, an overlay index and an extra index into `cellExtras`. An
fheroes2 `Tile` carries the ground index, terrain flags, passability,
the main object, fog, metadata, the occupant hero, the road flag and the
ground/top object parts. The mapping:

- `groundIndex` → `_index`/`_terrainImageIndex` and
  `_tilePassabilityDirections` by the terrain table.
- road bit → `_isTileMarkedAsRoad` (fheroes2 derives it from road sprites,
  see `Maps::Tile::isSpriteRoad`).
- `objectIndex`/`objType` + the linked `cellExtra` → `_mainObjectPart`,
  `_groundObjectPart`/`_topObjectPart` and `_mainObjectType`. The original
  stores one bottom part and one top part per cell, plus a chain of
  `mapCellExtra` add-ons (7 bytes each, the MP2 add-on layout without
  UIDs): ground parts use the halved ICN id (`b1 >> 1`), top parts use
  `b6 >> 2`; tileset 63 marks "no top part" in the base game only. Object
  part layers come from the low 2 bits of the cell's/extra's flag byte.
  Since the save has no UIDs, they are reconstructed: parts of one object
  use consecutive frames of one ICN (mountains additionally merge by frame
  continuity, castles by geometry), and FLAG32 parts take the UID of the
  object on their tile.
- `mapRevealed` byte → `_fogColors`. Visibility is stored by PLAYER INDEX
  (bit i = player i sees the tile), so the bits are remapped to fheroes2
  PlayerColor values (bit of player i → color of player i).
- Object type corrections: homm2 type 170 is a castle (fheroes2 163);
  monster placeholder frames 66–70 become the RANDOM_MONSTER types;
  decorative tiles without a type get one from their sprite (the
  `typeByIcn` table generated from fheroes2's object database).
- Tile metadata: mines/sawmills/alchemist labs get their resource type and
  daily income (mines from the EXTRAOVR frame, mirroring
  `updateObjectInfoTile`); resource piles get the resource type from the
  OBJNRSRC frame and the amount from the cell's extraInfo field; artifacts
  on the map get their id from the OBJNARTI frame; treasure chests get
  their gold from the cell's extraInfo field (`extraInfo * 250`).

### 5.2 Mines and boats

`mines[144]` become entries of the fheroes2 `map_captureobj` map (owner,
resource type). `boats[48]` become boat owners on the corresponding tiles
(`_boatOwnerColor`). Captured objects are emitted for every capturable tile
(castles, mines, sawmills, alchemist labs, lighthouses, dragon cities,
abandoned mines); the owner and guardians come from the mines table, which
contains only captured mines.

### 5.3 Castles

- Owner, position, name, garrison army, Mage Guild level and guild spells
  are translated directly.
- `buildingsBuiltFlags` is a bitmask in both formats; bits are mapped by
  building type (Mage Guild levels, Thieves' Guild, Tavern, Shipyard, Well,
  Statue, turrets, Marketplace, Moat, Castle, Captain's Quarters, dwellings
  and their upgrades).
- `numCreaturesInDwelling[12]` maps to the fheroes2 dwelling counts (6 base
  + 6 upgraded are merged).

### 5.4 Players and kingdoms

- A player's `friendsColors` is set to its own color; with an empty mask
  fheroes2 treats every tile as fully fogged and does not render terrain.
- A player's focus points to its active hero (FOCUS_HEROES + tile index);
  the camera is restored from it when loading a save.
- `Kingdom.lostTownDays` is initialized to 8 (`GetLostTownDays() + 1`);
  zero would make the kingdom lose on the first new day.
- Hero artifact bags are always serialized with all 14 slots (empty =
  Artifact::UNKNOWN); otherwise the bag counts as full.
