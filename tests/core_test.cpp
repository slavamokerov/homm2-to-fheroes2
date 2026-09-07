// Tests for the HoMM2 -> fheroes2 converter core.
//
// Runs against the fixture saves (tests/fixtures/). Set the FIXTURES_DIR
// environment variable to override the location.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <zlib.h>

#include "convert.h"
#include "fheroes2_save.h"
#include "homm2_save.h"

namespace {

std::string fixturesDir()
{
    const char * env = std::getenv( "FIXTURES_DIR" );
    if ( env && *env )
        return env;
#ifdef FIXTURES_DEFAULT_DIR
    return FIXTURES_DEFAULT_DIR;
#else
    return "tests/fixtures";
#endif
}

std::vector<uint8_t> readFile( const std::string & path )
{
    std::ifstream f( path, std::ios::binary );
    if ( !f )
        return {};
    return std::vector<uint8_t>( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
}

int failures = 0;

void check( bool cond, const std::string & what )
{
    if ( cond ) {
        std::cout << "  ok: " << what << std::endl;
    }
    else {
        std::cout << "  FAIL: " << what << std::endl;
        ++failures;
    }
}

uint16_t be16( const uint8_t * p )
{
    return static_cast<uint16_t>( ( p[0] << 8 ) | p[1] );
}

uint32_t be32( const uint8_t * p )
{
    return ( static_cast<uint32_t>( p[0] ) << 24 ) | ( static_cast<uint32_t>( p[1] ) << 16 )
           | ( static_cast<uint32_t>( p[2] ) << 8 ) | static_cast<uint32_t>( p[3] );
}

// The 13-bit "extraInfo" field of a homm2 map cell (bitfield4 holds 3 flag
// bits in the low positions; extraInfo is the remainder).
uint32_t extraInfo( const h2::MapCell & cell )
{
    return static_cast<uint32_t>( cell.bitfield4 >> 3 );
}

// Checks the structural invariants of the produced fheroes2 world. These are
// the layout traps discovered while fixing save-load failures (see
// SESSION_NOTES.md): Kingdoms must always be 7 records, dwelling counts 6,
// BagArtifacts 14 slots, and the human-flag / auto-color detection.
void verifyWorldStructure( const h2::Save & save, const h2conv::WorldData & world )
{
    // Kingdoms: fheroes2 always serializes std::array<Kingdom, 7>.
    check( world.kingdoms.size() == 7, "kingdoms size == 7" );

    // Every castle reference must point at a tile whose object type is the
    // castle (mapObjectType == 163 = MP2::OBJ_CASTLE). When a hero stands on
    // the castle entrance the tile is OBJ_HERO (183) and the castle type is
    // recovered via hero->getObjectTypeUnderHero(), so both are valid.
    {
        int badCastle = 0;
        int badHeroId = 0;
        int noCastleTile = 0;
        for ( const auto & k : world.kingdoms ) {
            for ( int32_t idx : k.castleIndices ) {
                if ( idx < 0 || idx >= static_cast<int32_t>( world.tiles.size() ) ) {
                    ++noCastleTile;
                    continue;
                }
                const uint16_t ot = world.tiles[static_cast<size_t>( idx )].mainObjectType;
                if ( ot != 163 && ot != 183 )
                    ++badCastle;
            }
            for ( int32_t id : k.heroIndices ) {
                if ( id < 1 || id >= 73 )
                    ++badHeroId;
            }
        }
        check( badCastle == 0, "kingdom castleIndices point to OBJ_CASTLE tiles (" + std::to_string( badCastle ) + " bad)" );
        check( badHeroId == 0, "kingdom heroIndices are valid id 1..72 (" + std::to_string( badHeroId ) + " bad)" );
        check( noCastleTile == 0, "kingdom castleIndices in bounds (" + std::to_string( noCastleTile ) + " out of range)" );
    }

    // Players: ST_INGAME (0x2000) must be set for every player and exactly
    // one player must be CONTROL_HUMAN (the player whose turn it is); the rest
    // are AI. The human's color must be mapColor(curPlayer) — the auto-color /
    // human-player detection fix.
    {
        int noIngame = 0;
        int humans = 0;
        int humanColorOk = true;
        uint8_t humanColor = 0;
        for ( const auto & p : world.players ) {
            if ( ( p.modes & 0x2000 ) == 0 )
                ++noIngame;
            if ( p.control == 1 ) {
                ++humans;
                humanColor = p.color;
            }
        }
        const uint8_t curColor = ( save.header.curPlayer < 6 && save.header.curPlayer < static_cast<int>( save.players.size() ) )
                                     ? h2::mapColor( save.players[static_cast<size_t>( save.header.curPlayer )].color )
                                     : 0;
        if ( humans == 1 ) {
            if ( humanColor != curColor )
                humanColorOk = false;
        }
        else {
            humanColorOk = false;
        }
        check( noIngame == 0, "player modes have ST_INGAME (" + std::to_string( noIngame ) + " missing)" );
        check( humans == 1, "exactly one player is CONTROL_HUMAN (" + std::to_string( humans ) + ")" );
        check( humanColorOk, "human player color matches mapColor(curPlayer)" );
    }

    // Kingdoms: lostTownDays must be non-zero for live kingdoms (0 makes
    // fheroes2 eliminate the kingdom on the first new day). Empty/neutral
    // records (color == 0) are allowed to have 0.
    {
        int zeroLost = 0;
        int liveKingdoms = 0;
        for ( const auto & k : world.kingdoms ) {
            if ( k.color == 0 )
                continue;
            ++liveKingdoms;
            if ( k.lostTownDays == 0 )
                ++zeroLost;
        }
        check( liveKingdoms > 0, "at least one live kingdom" );
        check( zeroLost == 0, "live kingdom lostTownDays non-zero (" + std::to_string( zeroLost ) + " are 0)" );
    }

    // Castles: dwellingCounts must be 6 slots; heroes: BagArtifacts must be 14.
    {
        int badDwelling = 0;
        int badArtifacts = 0;
        for ( const auto & c : world.castles )
            if ( c.dwellingCounts.size() != 6 )
                ++badDwelling;
        for ( const auto & h : world.heroes )
            if ( h.artifacts.size() > 14 )
                ++badArtifacts;
        check( badDwelling == 0, "castle dwellingCounts size == 6 (" + std::to_string( badDwelling ) + " bad)" );
        check( badArtifacts == 0, "hero artifacts <= 14 slots (" + std::to_string( badArtifacts ) + " bad)" );
    }

    // Recruit slots: empty slot is UNKNOWN (id 0), never -1.
    {
        int badRecruit = 0;
        for ( const auto & k : world.kingdoms )
            for ( int r : k.recruitIds )
                if ( r < 0 )
                    ++badRecruit;
        check( badRecruit == 0, "kingdom recruitIds >= 0 (" + std::to_string( badRecruit ) + " are -1)" );
    }
}

// Validates the metadata invariants of object tiles (the extraInfo >> 3 fixes
// of the last session). Object tiles are matched between the parsed original
// save and the converted world by tile index; each rule is only checked when a
// tile of that object type is present in the fixture.
void verifyObjectMetadata( const h2::Save & save, const h2conv::WorldData & world )
{
    const size_t n = std::min( save.tiles.size(), world.tiles.size() );
    // Counts per rule; a rule only "applies" when at least one matching tile
    // exists, so a fixture without a given object does not fail the test.
    std::map<std::string, std::pair<int, int>> stats;

    auto note = [&]( const std::string & rule, bool ok ) {
        auto & s = stats[rule];
        s.first += 1; // applied
        if ( !ok )
            s.second += 1; // failures
    };

    for ( size_t i = 0; i < n; ++i ) {
        const h2::MapCell & cell = save.tiles[i];
        const h2conv::WorldData::TileOut & t = world.tiles[i];
        const uint32_t extra = extraInfo( cell );

        switch ( t.mainObjectType ) {
        case 152: // OBJ_MONSTER: stack size in metadata[0] (low 8 bits of extra);
            // the creature type is derived by fheroes2 from the MONS32 frame
            // (icnIndex + 1), not from metadata.
            if ( t.mainIcnType == 12 ) {
                note( "monster metadata[0] == extra & 0xFF", t.metadata[0] == ( extra & 0xFF ) );
            }
            break;
        case 134: // OBJ_TREASURE_CHEST: gold = extra * 500
            note( "treasure chest gold == extra * 500", t.metadata[1] == extra * 500 );
            break;
        case 155: { // OBJ_RESOURCE: count = extra; gold piles use hundreds
            const uint32_t expected = ( t.metadata[0] == 64 ) ? extra * 100 : extra;
            note( "resource count == extra", t.metadata[1] == expected );
            break;
        }
        case 159: // OBJ_SHRINE_FIRST_CIRCLE
        case 202: // OBJ_SHRINE_SECOND_CIRCLE
        case 203: // OBJ_SHRINE_THIRD_CIRCLE
        case 204: // OBJ_PYRAMID
            note( "shrine/pyramid spell == extra", t.metadata[0] == extra );
            break;
        case 247: // OBJ_BARRIER
        case 248: // OBJ_TRAVELLER_TENT
            note( "barrier/tent color == extra & 7", t.metadata[0] == ( extra & 7 ) );
            break;
        case 213: // OBJ_WITCHS_HUT: skill = extra + 1
            note( "witch's hut skill == extra + 1", t.metadata[0] == extra + 1 );
            break;
        default:
            break;
        }
    }

    for ( const auto & [rule, counts] : stats ) {
        const bool ok = counts.second == 0;
        check( ok, rule + " (applied on " + std::to_string( counts.first ) + " tiles)" );
    }
}

// UID uniqueness: neighboring single-tile objects of the same type (chests,
// resource piles) must not share a UID. The original save does not store UIDs,
// so this verifies that the converter's reconstruction did not glue adjacent
// same-type objects together (the fix for the "removing one chest removes the
// neighbor" AI assertion).
void verifyUidSeparation( const h2conv::WorldData & world )
{
    const int32_t w = world.width;
    const int32_t h = world.height;

    auto tileUid = [&]( const h2conv::WorldData::TileOut & t ) -> uint32_t {
        // The main part's uid, or failed to reconstruct.
        if ( t.mainUid != 0 )
            return t.mainUid;
        return t.mainUid;
    };
    auto isTreasureOrResource = []( uint16_t type ) { return type == 134 || type == 155; };

    // Gather main-UID per tile for treasure chests / resource piles.
    std::map<uint32_t, std::tuple<int32_t, int32_t>> uidPositions; // uid -> (x,y)
    std::map<uint32_t, bool> uidIsChest;                          // uid -> was it a chest
    int conflict = 0;
    int applied = 0;

    for ( int32_t y = 0; y < h; ++y ) {
        for ( int32_t x = 0; x < w; ++x ) {
            const auto & t = world.tiles[static_cast<size_t>( y ) * w + x];
            if ( !isTreasureOrResource( t.mainObjectType ) )
                continue;
            if ( t.mainUid == 0 )
                continue;
            ++applied;
            const uint32_t uid = tileUid( t );
            auto it = uidPositions.find( uid );
            if ( it == uidPositions.end() ) {
                uidPositions[uid] = { x, y };
                uidIsChest[uid] = ( t.mainObjectType == 134 );
            }
            else {
                // Same UID on two tiles: only legal for a multi-tile object
                // (e.g. two cast-of-one treasure chest frames). A chest and a
                // resource sharing a UID is always a bug.
                if ( uidIsChest[uid] != ( t.mainObjectType == 134 ) )
                    ++conflict;
            }
        }
    }

    check( applied > 0, "uid separation applied (at least one chest/resource present)" );
    check( conflict == 0, "no mixed chest/resource UID merges (" + std::to_string( conflict ) + " conflicts)" );

    // Adjacent same-type single-tile objects (horizontal / vertical) must have
    // distinct UIDs.
    int adjacentConflict = 0;
    for ( const auto & [uid, pos] : uidPositions ) {
        const auto [x, y] = pos;
        for ( const auto & d : std::vector<std::pair<int32_t, int32_t>>{ { 1, 0 }, { 0, 1 } } ) {
            const int32_t nx = x + d.first;
            const int32_t ny = y + d.second;
            if ( nx >= w || ny >= h )
                continue;
            const auto & nt = world.tiles[static_cast<size_t>( ny ) * w + nx];
            if ( !isTreasureOrResource( nt.mainObjectType ) || nt.mainUid == 0 )
                continue;
            if ( nt.mainUid == uid )
                ++adjacentConflict;
        }
    }
    check( adjacentConflict == 0, "adjacent chest/resource tiles have distinct UIDs (" + std::to_string( adjacentConflict ) + " conflicts)" );
}

// Reads the filename string (the 2nd string after the version) from a
// produced fheroes2 save. fheroes2 requires a non-empty header filename for
// SaveFile::findPlayers (auto-color / fog detection).
std::string readFh2Filename( const std::vector<uint8_t> & data )
{
    size_t p = 2; // magic
    auto readStr = [&]( std::string & out ) {
        if ( p + 4 > data.size() )
            return;
        const uint32_t len = be32( data.data() + p );
        p += 4;
        if ( p + len > data.size() )
            return;
        out.assign( reinterpret_cast<const char *>( data.data() + p ), len );
        p += len;
    };
    std::string verStr, filename;
    readStr( verStr ); // version string
    p += 2;            // version number (u16)
    p += 2;            // requirements (u16)
    readStr( filename ); // filename
    return filename;
}

// Validates the structure of a produced fheroes2 save: header magic,
// version, the zlib block header, decompression, and the end-of-stream
// marker.
bool validateFh2Save( const std::vector<uint8_t> & data, uint16_t expectVersion, const std::string & what )
{
    if ( data.size() < 16 ) {
        check( false, what + " (too small)" );
        return false;
    }
    check( be16( data.data() ) == 0xFF03, what + " magic" );
    const uint32_t verLen = be32( data.data() + 2 );
    const std::string verStr( reinterpret_cast<const char *>( data.data() + 6 ),
                              reinterpret_cast<const char *>( data.data() + 6 + verLen ) );
    check( verStr == std::to_string( expectVersion ), what + " version string" );
    check( be16( data.data() + 6 + verLen ) == expectVersion, what + " version number" );
    return true;
}

void testFixture( const std::string & name, bool expectCampaign )
{
    std::cout << "fixture " << name << std::endl;
    const std::vector<uint8_t> data = readFile( fixturesDir() + "/" + name );
    check( !data.empty(), "read" );

    h2::Save save;
    check( h2::parseSave( data, save ), "parse" );
    if ( failures )
        return;
    check( save.header.mapWidth > 0 && save.header.mapHeight > 0, "map size" );
    check( save.header.isCampaign == expectCampaign, "campaign detection" );

    for ( uint16_t version : { uint16_t( 10032 ), uint16_t( 10033 ), uint16_t( 10034 ) } ) {
        h2conv::WorldData world;
        h2conv::ConvertOptions options;
        options.formatVersion = version;
        check( h2::convert( save, world, options ), "convert (format " + std::to_string( version ) + ")" );

        verifyWorldStructure( save, world );
        verifyObjectMetadata( save, world );
        verifyUidSeparation( world );

        const std::vector<uint8_t> out = h2conv::buildSaveFile( save.header, world, options );
        check( !out.empty(), "build (format " + std::to_string( version ) + ")" );
        if ( out.empty() )
            continue;
        validateFh2Save( out, version, "header v" + std::to_string( version ) );
        check( out.size() > 1000, "output size sane (v" + std::to_string( version ) + ")" );
        // fheroes2 needs a non-empty header filename (SaveFile::findPlayers).
        const std::string fname = readFh2Filename( out );
        check( !fname.empty(), "header filename non-empty (v" + std::to_string( version ) + ")" );
        check( fname == save.header.mapName, "header filename == mapName (v" + std::to_string( version ) + ")" );
    }
}

// Minimal parser of a fheroes2 save's tile section (format 10033/10034),
// used to compare the converter output against a reference save.
struct ReferenceTile {
    int32_t terrainImageIndex = 0;
    uint16_t passability = 0;
    uint8_t mainLayer = 0;
    uint8_t mainIcn = 0;
    uint8_t mainFrame = 255;
    uint16_t mainObjectType = 0;
    uint8_t occupantHeroId = 0;
    bool isRoad = false;
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> ground; // (layer, icn, frame)
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> top;
};

std::vector<uint8_t> zlibDecompress( const uint8_t * data, size_t size )
{
    uLongf dstLen = static_cast<uLongf>( size ) * 8 + 4096;
    std::vector<uint8_t> out( dstLen );
    if ( uncompress( out.data(), &dstLen, data, static_cast<uLong>( size ) ) != Z_OK ) {
        // Retry with a much larger buffer if the size estimate was too low.
        dstLen = static_cast<uLongf>( size ) * 64;
        out.resize( dstLen );
        if ( uncompress( out.data(), &dstLen, data, static_cast<uLong>( size ) ) != Z_OK )
            return {};
    }
    out.resize( dstLen );
    return out;
}

bool parseReferenceSave( const std::vector<uint8_t> & data, std::vector<ReferenceTile> & tiles )
{
    size_t p = 2; // magic
    auto readStr = [&]( size_t & pos ) {
        const uint32_t len = be32( data.data() + pos );
        pos += 4 + len;
    };
    readStr( p ); // version string
    p += 2;       // version number
    p += 2;       // requirements
    readStr( p ); // filename
    readStr( p ); // map name
    readStr( p ); // description
    p += 4 + 1 + 1 + 6 + 6 + 4 + 3 + 4 + 1 + 4 + 4 + 1 + 4 + 12 + 1; // FileInfo tail
    readStr( p );                                                     // creatorNotes
    p += 4;                                                           // gameType
    const uint32_t rawSize = be32( data.data() + p );
    p += 4;
    const uint32_t zipSize = be32( data.data() + p );
    p += 4 + 2 + 2;
    if ( p + zipSize > data.size() )
        return false;
    const std::vector<uint8_t> raw = zlibDecompress( data.data() + p, zipSize );
    if ( raw.empty() || raw.size() != rawSize )
        return false;

    size_t q = 0;
    const int32_t width = static_cast<int32_t>( be32( raw.data() + q ) );
    q += 4;
    const int32_t height = static_cast<int32_t>( be32( raw.data() + q ) );
    q += 4;
    const int32_t count = static_cast<int32_t>( be32( raw.data() + q ) );
    q += 4;
    if ( width <= 0 || height <= 0 || count != width * height || count < 0 || count > 100000 )
        return false;

    tiles.clear();
    tiles.resize( static_cast<size_t>( count ) );
    for ( ReferenceTile & t : tiles ) {
        if ( q + 46 > raw.size() )
            return false;
        q += 4; // index
        t.terrainImageIndex = static_cast<int32_t>( be16( raw.data() + q ) );
        q += 2;
        q += 1; // terrainFlags
        t.passability = be16( raw.data() + q );
        q += 2;
        t.mainLayer = raw[q++];
        q += 4; // uid
        t.mainIcn = raw[q++];
        t.mainFrame = raw[q++];
        t.mainObjectType = be16( raw.data() + q );
        q += 2;
        q += 1; // fog
        const uint32_t metaCount = be32( raw.data() + q );
        q += 4 + 4 * metaCount;
        t.occupantHeroId = raw[q++];
        t.isRoad = raw[q++] != 0;
        uint32_t groundCount = be32( raw.data() + q );
        q += 4;
        for ( uint32_t i = 0; i < groundCount; ++i ) {
            const uint8_t layer = raw[q];
            const uint8_t icn = raw[q + 5];
            const uint8_t frame = raw[q + 6];
            q += 7;
            t.ground.push_back( { layer, icn, frame } );
        }
        uint32_t topCount = be32( raw.data() + q );
        q += 4;
        for ( uint32_t i = 0; i < topCount; ++i ) {
            const uint8_t layer = raw[q];
            const uint8_t icn = raw[q + 5];
            const uint8_t frame = raw[q + 6];
            q += 7;
            t.top.push_back( { layer, icn, frame } );
        }
        q += 1; // boatOwnerColor
    }
    return true;
}

// Compares the converted Slugfest save against the reference save made by
// fheroes2 itself (SLUGFEST_REF.sav, same map, fresh start). The reference
// uses randomly materialized artifacts and heroes, so small deviations in
// the artifact frames / hero tiles are expected.
void testSlugfestReference()
{
    std::cout << "reference save (Slugfest)" << std::endl;

    const std::vector<uint8_t> saveData = readFile( fixturesDir() + "/BASE.GM1" );
    h2::Save save;
    if ( !h2::parseSave( saveData, save ) ) {
        check( false, "parse BASE.GM1" );
        return;
    }

    h2conv::WorldData world;
    h2conv::ConvertOptions options;
    options.formatVersion = 10033;
    if ( !h2::convert( save, world, options ) ) {
        check( false, "convert BASE.GM1" );
        return;
    }

    const std::vector<uint8_t> ref = readFile( fixturesDir() + "/SLUGFEST_REF.sav" );
    std::vector<ReferenceTile> refTiles;
    check( parseReferenceSave( ref, refTiles ), "parse reference save" );
    if ( refTiles.size() != world.tiles.size() ) {
        check( false, "reference tile count" );
        return;
    }

    int mainDiffs = 0;
    int groundDiffs = 0;
    int topDiffs = 0;
    int objTypeDiffs = 0;
    int roadDiffs = 0;
    int passabilityDiffs = 0;

    for ( size_t i = 0; i < world.tiles.size(); ++i ) {
        const h2conv::WorldData::TileOut & t = world.tiles[i];
        const ReferenceTile & r = refTiles[i];

        if ( t.mainIcnType != r.mainIcn || t.mainIcnIndex != r.mainFrame || t.mainLayerType != r.mainLayer )
            ++mainDiffs;

        std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> ground;
        for ( const auto & part : t.groundParts )
            ground.push_back( { part.layer, part.icnType, part.icnIndex } );
        if ( ground != r.ground )
            ++groundDiffs;

        std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> top;
        for ( const auto & part : t.topParts )
            top.push_back( { part.layer, part.icnType, part.icnIndex } );
        if ( top != r.top )
            ++topDiffs;

        if ( t.mainObjectType != r.mainObjectType )
            ++objTypeDiffs;
        if ( t.isRoad != r.isRoad )
            ++roadDiffs;
        if ( t.passability != r.passability )
            ++passabilityDiffs;
    }

    // Artifact frames differ because the reference materializes random
    // artifacts at map load (12 artifact tiles on Slugfest). Everything
    // else must match exactly or be within a small tolerance.
    check( groundDiffs == 0, "reference ground parts match (" + std::to_string( groundDiffs ) + " diffs)" );
    check( topDiffs == 0, "reference top parts match (" + std::to_string( topDiffs ) + " diffs)" );
    check( roadDiffs == 0, "reference road flags match (" + std::to_string( roadDiffs ) + " diffs)" );
    check( mainDiffs <= 14, "reference main parts match (" + std::to_string( mainDiffs ) + " diffs)" );
    check( objTypeDiffs <= 2, "reference object types match (" + std::to_string( objTypeDiffs ) + " diffs)" );
    check( passabilityDiffs <= 30, "reference passability match (" + std::to_string( passabilityDiffs ) + " diffs)" );
}

} // namespace

int main()
{
    std::cout << "homm2-to-fheroes2 core tests" << std::endl;

    testFixture( "BASE.GM1", false );
    testFixture( "ARMY5.GM1", false );
    testFixture( "ARMY10.GM1", false );
    testFixture( "GOLD.GM1", false );
    testFixture( "BUILD.GM1", false );
    testFixture( "MOVE.GM1", false );
    testFixture( "MOVE2.GM1", false );
    testFixture( "MOVE3.GM1", false );
    testFixture( "DAY2.GM1", false );
    testFixture( "HERO2.GM1", false );
    testFixture( "ARMY2.GM1", false );
    testFixture( "SPELL.GM1", false );
    testFixture( "AUTOSAVE.GM1", false );
    testFixture( "NEWGAME1.GMC", true );
    testFixture( "NEWGAME2.GXC", true );

    testSlugfestReference();

    // ID mapping spot checks.
    {
        using h2::mapBuildings;
        using h2::mapColor;
        using h2::mapCreature;
        using h2::mapRace;
        check( mapCreature( 37 ) == 38, "creature BlackDragon 37 -> 38" );
        check( mapCreature( 23 ) == 24, "creature Elf 23 -> 24" );
        check( mapCreature( -1 ) == 0, "creature empty -> UNKNOWN" );
        check( mapRace( 0 ) == 0x01, "race Knight" );
        check( mapRace( 5 ) == 0x20, "race Necromancer" );
        // Full color table (HoMM2 index -> fheroes2 PlayerColor bit).
        check( mapColor( 0 ) == 0x04, "color blue" );
        check( mapColor( 1 ) == 0x02, "color green" );
        check( mapColor( 2 ) == 0x01, "color red" );
        check( mapColor( 3 ) == 0x08, "color yellow" );
        check( mapColor( 4 ) == 0x10, "color orange" );
        check( mapColor( 5 ) == 0x20, "color purple" );
        check( mapBuildings( 0x38, 1 ) == ( 0x00000004 | 0x00000008 | 0x00080000 | 0x00004000 ), "buildings base set + guild 1" );
        check( ( mapBuildings( 1u << 25, 1 ) & 0x04000000 ) != 0, "buildings cottage upgrade bit" );
    }

    std::cout << ( failures ? "FAILED" : "all tests passed" ) << std::endl;
    return failures ? 1 : 0;
}
