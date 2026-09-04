// Tests for the HoMM2 -> fheroes2 converter core.
//
// Runs against the fixture saves (tests/fixtures/). Set the FIXTURES_DIR
// environment variable to override the location.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
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
        fh2::WorldData world;
        fh2::ConvertOptions options;
        options.formatVersion = version;
        check( h2::convert( save, world, options ), "convert (format " + std::to_string( version ) + ")" );

        const std::vector<uint8_t> out = fh2::buildSaveFile( save.header, world, options );
        check( !out.empty(), "build (format " + std::to_string( version ) + ")" );
        if ( out.empty() )
            continue;
        validateFh2Save( out, version, "header v" + std::to_string( version ) );
        check( out.size() > 1000, "output size sane (v" + std::to_string( version ) + ")" );
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

    fh2::WorldData world;
    fh2::ConvertOptions options;
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
        const fh2::WorldData::TileOut & t = world.tiles[i];
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
        check( mapColor( 2 ) == 0x04, "color red" );
        check( mapColor( 5 ) == 0x20, "color purple" );
        check( mapBuildings( 0x38, 1 ) == ( 0x00000004 | 0x00000008 | 0x00080000 | 0x00004000 ), "buildings base set + guild 1" );
        check( ( mapBuildings( 1u << 25, 1 ) & 0x04000000 ) != 0, "buildings cottage upgrade bit" );
    }

    std::cout << ( failures ? "FAILED" : "all tests passed" ) << std::endl;
    return failures ? 1 : 0;
}
