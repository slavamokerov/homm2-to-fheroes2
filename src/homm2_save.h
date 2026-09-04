// Parser for the original Heroes of Might and Magic II save files.
//
// Supports all three layout variants: standard (.GM1), The Succession Wars
// campaign (.GMC, a 327-byte campaign block after the header) and The Price
// of Loyalty campaign (.GXC, a 4-byte expansion marker at the start plus a
// 79-byte expansion campaign block). The layout is documented in
// docs/HOMM2_SAVE_FORMAT.md and verified against real saves.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace h2 {

// Fixed sizes of the serialized structures (see the format documentation).
constexpr int kMapHeaderSize = 420;
constexpr int kPlayerSize = 207;
constexpr int kHeroSizeBase = 236;   // The Succession Wars (no scrollSpell)
constexpr int kHeroSizePoL = 250;    // The Price of Loyalty (with scrollSpell)
constexpr int kCastleSize = 100;
constexpr int kMineSize = 7;
constexpr int kBoatSize = 8;
constexpr int kMapCellSize = 12;
constexpr int kMapCellExtraSize = 7;

constexpr int kNumPlayers = 6;
constexpr int kNumHeroes = 54;
constexpr int kNumCastles = 72;
constexpr int kNumMines = 144;
constexpr int kNumBoats = 48;

enum class Variant {
    Standard,  // .GM1
    SwCampaign, // .GMC
    PoLCampaign, // .GXC
};

struct Header {
    Variant variant = Variant::Standard;
    bool expansion = false;          // the FF FF FF FF marker is present
    int baseOffset = 0;              // total offset shift of the data sections

    int mapWidth = 0;
    int mapHeight = 0;
    std::string mapName;
    std::string description;
    std::string slotName;            // the save's name in the game's Save/Load menu
    uint8_t numPlayers = 0;
    uint8_t numObelisks = 0;
    uint8_t difficulty8 = 0;         // 1..5 (game display)
    int16_t difficulty = 0;          // raw value (100 = Normal)
    int16_t day = 0;
    int16_t week = 0;
    int16_t month = 0;

    std::vector<uint8_t> playerAlive;    // 6
    std::vector<uint8_t> playerDead;     // 6
    std::vector<uint8_t> playerFactions; // 6
    std::vector<uint8_t> playerColors;   // 6
    uint8_t winConditionType = 0;
    uint8_t lossConditionType = 0;
    bool allowNormalVictory = true;
    int16_t winConditionArg[2] = { 0, 0 };
    int16_t lossConditionArg[2] = { 0, 0 };
    bool startWithHeroInFirstCastle = true;

    // Campaign fields.
    bool isCampaign = false;
    bool isPoLCampaign = false;
    std::vector<uint8_t> campaignBlock; // 327 or 79 bytes, raw

    // Player names (6 x 21 bytes).
    std::vector<std::string> playerNames;

    int heroRecordSize = kHeroSizeBase; // 236 or 250 (PoL)
};

struct Player {
    uint8_t color = 0;
    uint8_t numHeroes = 0;
    int8_t curHeroIdx = -1;
    std::vector<int8_t> heroesOwned;     // 8
    std::vector<int8_t> heroesForPurchase; // 2
    int32_t personality = 0;
    uint8_t hasEvilFaction = 0;
    int8_t daysLeftWithoutCastle = 0;
    uint8_t numCastles = 0;
    int8_t curCastleIdx = -1;
    std::vector<int8_t> castlesOwned;    // 72
    std::vector<uint32_t> resources;     // 7: wood, mercury, ore, sulfur, crystal, gems, gold
};

struct Army {
    std::vector<int8_t> creatureTypes;   // 5, -1 = empty
    std::vector<uint16_t> quantities;    // 5
};

struct Hero {
    int16_t spellPoints = 0;
    int32_t idx = 0;
    int8_t ownerIdx = -1;                // -1 = not hired
    std::string name;
    uint8_t factionID = 0;
    uint8_t heroID = 0;
    int32_t x = 0;
    int32_t y = 0;
    int32_t mobility = 0;
    int32_t remainingMobility = 0;
    int32_t experience = 0;
    int16_t oldLevel = 1;
    std::vector<uint8_t> primarySkills;  // 4: attack, defense, spellpower, knowledge
    uint8_t tempMorale = 0;
    uint8_t tempLuck = 0;
    Army army;
    std::vector<uint8_t> secondarySkillLevel; // 14
    std::vector<uint8_t> secondarySkillIdx;   // 14
    uint32_t numSecSkills = 0;
    std::vector<uint8_t> spellsLearned;       // 65 (0/1)
    std::vector<int8_t> artifacts;            // 14, -1 = empty
    std::vector<uint8_t> scrollSpell;         // 14 (PoL only)
    int16_t occupiedObjType = 0;
    int16_t occupiedObjVal = 0;
    uint32_t flags = 0;
    bool isCaptain = false;
    uint8_t directionFacing = 2;
    uint8_t randomSeed = 0;
    uint8_t wisdomLastOffered = 0;
};

struct Castle {
    int32_t idx = 0;
    int8_t ownerIdx = -1;
    uint8_t alignment = 0;
    uint8_t factionID = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    bool exists = false;
    std::string name;
    Army garrison;
    int8_t visitingHeroIdx = -1;
    uint32_t buildingsBuilt = 0;
    uint8_t mageGuildLevel = 0;
    std::vector<uint16_t> dwellingCounts;   // 12
    std::vector<std::vector<uint8_t>> mageGuildSpells; // 5 x 4
    std::vector<uint8_t> numSpellsOfLevel;  // 5
    uint8_t playerPos = 0;
    bool mayNotBeUpgradedToCastle = false;
};

struct Mine {
    uint8_t owner = 0xFF;
    uint8_t type = 0;
    uint8_t guardianType = 0;
    uint8_t guardianQty = 0;
    uint8_t x = 0;
    uint8_t y = 0;
};

struct Boat {
    int32_t idx = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint8_t owner = 0;
};

struct MapCell {
    uint16_t groundIndex = 0;
    uint8_t bitfield1 = 0; // hasObject / isRoad / objTileset
    uint8_t objectIndex = 0;
    uint16_t bitfield4 = 0; // preventSpawn / isShadow / extraInfo
    uint8_t bitfield6 = 0; // hasOverlay / hasLateOverlay / overlayTileset
    uint8_t overlayIndex = 0;
    uint8_t flags = 0;
    uint8_t objType = 0;
    uint16_t extraIdx = 0;

    bool hasObject() const { return bitfield1 & 0x01; }
    bool isRoad() const { return bitfield1 & 0x02; }
};

struct MapCellExtra {
    int16_t nextIdx = 0;
    uint8_t b1 = 0;
    uint8_t objectIndex = 0;
    uint8_t b4 = 0;
    uint8_t b6 = 0;
    uint8_t field6 = 0;
};

struct Save {
    Header header;
    std::vector<Player> players;
    std::vector<Hero> heroes;
    std::vector<Castle> castles;
    std::vector<Mine> mines;
    std::vector<Boat> boats;
    std::vector<uint8_t> heroForHireStatus;   // 54
    std::vector<uint8_t> boatBuilt;           // 48
    std::vector<uint8_t> obeliskVisitedMasks; // 48
    int8_t ultimateArtifactX = -1;
    int8_t ultimateArtifactY = -1;
    uint8_t ultimateArtifactIdx = 0;

    std::vector<MapCell> tiles;      // width * height
    std::vector<uint8_t> mapRevealed; // width * height
    std::vector<MapCellExtra> cellExtras;

    std::vector<std::string> rumors;
    std::vector<int16_t> eventIndices;
    std::vector<int16_t> mapEventIndices;

    // Map extras (raw, one blob per index).
    std::vector<std::vector<uint8_t>> mapExtras;
};

// Parses the file. Returns false if the file is not a recognizable save.
bool parseSave( const std::vector<uint8_t> & data, Save & out );

} // namespace h2
