// Writer for the fheroes2 save format (version 10034).
//
// Serialization rules (big-endian, no padding) are documented in the
// FH2_SAVE_FORMAT.md document of the fheroes2-save-editor project and were
// verified against the fheroes2 sources (engine/serialize.h, heroes.cpp,
// castle.cpp, kingdom.cpp, players.cpp, settings.cpp, world.cpp).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "homm2_save.h"

namespace h2conv {

struct ConvertOptions {
    // Output format version (string/number). Supported: 10032 (fheroes2
    // 1.1.11+), 10033 (1.1.15+, default — widest compatibility), 10034
    // (1.1.80+).
    uint16_t formatVersion = 10033;
    // Set REQUIRE_POL resources flag in the header (for PoL saves).
    bool polResources = false;
    // Write a campaign save (.savc): CampaignSaveData after GameOver::Result.
    bool campaign = false;
    // Campaign progress fields (used only when campaign == true).
    int32_t campaignId = 0;
    int32_t scenarioId = 0;
    int32_t campaignBonusId = -1;
    std::vector<std::pair<int32_t, int32_t>> finishedMaps; // (campaignId, scenarioId)
    std::vector<uint32_t> daysPassed;
    std::vector<int32_t> bonusesForFinishedMaps;
    std::vector<int32_t> campaignAwards;
    std::vector<std::pair<int32_t, uint32_t>> carryOverTroops; // (monsterId, count)
    int32_t difficulty = 0;
    int32_t minDifficulty = 0;
};

// A fully prepared fheroes2 world for writing (already translated).
struct WorldData {
    int32_t width = 0;
    int32_t height = 0;

    // One object part of a tile (ObjectLayerType, uid, ObjectIcnType,
    // icnIndex) as serialized by fheroes2.
    struct ObjectPart {
        uint8_t layer = 0;   // OBJECT_LAYER = 0, BACKGROUND_LAYER = 1, SHADOW_LAYER = 2, TERRAIN_LAYER = 3
        uint32_t uid = 0;
        uint8_t icnType = 0; // MP2::ObjectIcnType: UNKNOWN = 0
        uint8_t icnIndex = 0;
    };

    // Tile payload (serialized per tile below).
    struct TileOut {
        int32_t index = 0;
        uint16_t terrainImageIndex = 0;
        uint8_t terrainFlags = 0;
        uint16_t passability = 0xFFFF;
        // mainObjectPart
        uint8_t mainLayerType = 0;      // ObjectLayerType: OBJECT_LAYER = 0
        uint32_t mainUid = 0;
        uint8_t mainIcnType = 0;        // ObjectIcnType: UNKNOWN = 0
        uint8_t mainIcnIndex = 255;
        uint16_t mainObjectType = 0;    // MP2::MapObjectType: OBJ_NONE = 0
        uint8_t fogColors = 0;
        uint32_t metadata[3] = { 0, 0, 0 };
        uint8_t occupantHeroId = 0;     // Heroes::UNKNOWN = 0
        bool isRoad = false;
        // groundObjectPart / topObjectPart
        std::vector<ObjectPart> groundParts;
        std::vector<ObjectPart> topParts;
        uint8_t boatOwnerColor = 0;     // PlayerColor::NONE = 0
    };
    std::vector<TileOut> tiles;

    struct HeroOut {
        int32_t attack = 0;
        int32_t defense = 0;
        int32_t knowledge = 0;
        int32_t power = 0;
        int16_t centerX = 0;
        int16_t centerY = 0;
        uint32_t modes = 0;
        uint32_t spellPoints = 0;
        uint32_t movePoints = 0;
        std::vector<int32_t> spells;
        std::vector<std::pair<int32_t, int32_t>> artifacts; // (id, ext)
        std::string name;
        uint8_t color = 0;
        uint32_t experience = 0;
        std::vector<std::pair<int32_t, int32_t>> secSkills; // (id, level)
        // Army
        int32_t monsterIds[5] = { 0, 0, 0, 0, 0 };
        uint32_t monsterCounts[5] = { 0, 0, 0, 0, 0 };
        bool spread = false;
        uint8_t armyColor = 0;
        int32_t id = 0;
        int32_t portrait = 0;
        int32_t race = 0;
        uint16_t objectTypeUnderHero = 0;
        bool pathHide = false;
        std::vector<std::tuple<int32_t, int32_t, uint32_t>> path; // (from, direction, penalty)
        int32_t direction = 2;
        int32_t spriteIndex = 0;
        int32_t patrolX = 0;
        int32_t patrolY = 0;
        uint32_t patrolDistance = 0;
        std::vector<std::pair<int32_t, uint16_t>> visitedObjects;
        uint32_t lastGroundRegion = 0;
    };
    std::vector<HeroOut> heroes;

    struct CastleOut {
        int16_t x = 0;
        int16_t y = 0;
        uint32_t modes = 0;
        int32_t race = 0;
        uint32_t builtBuildings = 0;
        uint32_t disabledBuildings = 0;
        // Captain (HeroBase)
        int32_t capAttack = 0;
        int32_t capDefense = 0;
        int32_t capKnowledge = 0;
        int32_t capPower = 0;
        int16_t capX = 0;
        int16_t capY = 0;
        uint32_t capModes = 0;
        uint32_t capSpellPoints = 0;
        uint32_t capMovePoints = 0;
        std::vector<int32_t> capSpells;
        std::vector<std::pair<int32_t, int32_t>> capArtifacts;
        uint8_t color = 0;
        std::string name;
        std::vector<int32_t> mageGuildGeneral;   // spell ids
        std::vector<int32_t> mageGuildLibrary;   // spell ids
        std::vector<uint32_t> dwellingCounts;
        int32_t garrisonIds[5] = { 0, 0, 0, 0, 0 };
        uint32_t garrisonCounts[5] = { 0, 0, 0, 0, 0 };
        bool garrisonSpread = false;
        uint8_t garrisonColor = 0;
    };
    std::vector<CastleOut> castles;

    struct KingdomOut {
        uint32_t modes = 0;
        uint8_t color = 0;
        int32_t resources[7] = { 0, 0, 0, 0, 0, 0, 0 };
        int32_t lostTownDays = 0;
        std::vector<int32_t> castleIndices;
        std::vector<int32_t> heroIndices;
        // Recruits (pair of (id, surrenderDay); UNKNOWN = 0)
        int32_t recruitIds[2] = { 0, 0 };
        uint32_t recruitDays[2] = { 0, 0 };
        std::vector<std::pair<int32_t, uint16_t>> visitedObjects;
        std::string puzzleString;
        std::vector<uint8_t> puzzleZone1;
        std::vector<uint8_t> puzzleZone2;
        std::vector<uint8_t> puzzleZone3;
        std::vector<uint8_t> puzzleZone4;
        int32_t visitedTentsColors = 0;
        int32_t topCastleInKingdomView = -1;
        int32_t topHeroInKingdomView = -1;
        std::vector<int32_t> monstersUnderVision;
    };
    std::vector<KingdomOut> kingdoms;

    std::vector<std::string> customRumors;
    // Events, captured objects, ultimate artifact, map objects:
    // (index, objectType, ownerColor, guardians [(monsterId, count)])
    std::vector<std::tuple<int32_t, uint16_t, uint8_t, std::vector<std::pair<int32_t, uint32_t>>>> capturedObjects;
    int32_t ultimateArtifactId = -1;   // -1 = no ultimate artifact
    int32_t ultimateArtifactExt = 0;
    int32_t ultimateArtifactIndex = -1; // tile index where it is buried
    bool ultimateArtifactFound = false;
    int32_t ultimateArtifactOffsetX = 0;
    int32_t ultimateArtifactOffsetY = 0;
    uint32_t day = 1;
    uint32_t week = 1;
    uint32_t month = 1;
    int32_t heroIdAsWinCondition = -1;
    int32_t heroIdAsLossCondition = -1;
    uint32_t seed = 0;

    // Settings / Players
    std::string gameLanguage = "en"; // Settings: language as a string
    int32_t gameDifficulty = 0;     // 0..4 (Easy..Impossible) or raw?
    int32_t gameType = 1;           // TYPE_STANDARD = 1, TYPE_CAMPAIGN = 2

    struct PlayerOut {
        uint32_t modes = 0;
        int32_t control = 0;        // CONTROL_NONE/HUMAN/AI
        uint8_t color = 0;
        int32_t race = 0;
        uint8_t friendsColors = 0;
        std::string name;
        int32_t focusType = 0;      // FOCUS_HEROES = 1, FOCUS_CASTLE = 2
        int32_t focusIndex = -1;
        int32_t aiPersonality = 0;
        uint8_t handicapStatus = 0;
    };
    uint8_t playersColors = 0;
    uint8_t currentPlayerColor = 0;
    std::vector<PlayerOut> players;

    // GameOver::Result
    uint8_t gameOverColors = 0; // empty high score table
};

// Serializes the world (uncompressed stream, big-endian).
std::vector<uint8_t> serializeWorld( const WorldData & world, uint16_t formatVersion = 10033 );

// Builds a complete .sav file (header + zlib-compressed stream).
std::vector<uint8_t> buildSaveFile( const h2::Header & srcHeader, const WorldData & world, const ConvertOptions & options );

} // namespace h2conv
