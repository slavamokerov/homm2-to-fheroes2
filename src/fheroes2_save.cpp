// Writer for the fheroes2 save format (version 10034). Big-endian.

#include "fheroes2_save.h"

#include <zlib.h>

#include <cstring>
#include <ctime>

namespace h2conv {

namespace {

class Writer {
public:
    void u8( uint8_t v )
    {
        _data.push_back( v );
    }

    void u16( uint16_t v )
    {
        _data.push_back( static_cast<uint8_t>( v >> 8 ) );
        _data.push_back( static_cast<uint8_t>( v ) );
    }

    void i16( int16_t v )
    {
        u16( static_cast<uint16_t>( v ) );
    }

    void u32( uint32_t v )
    {
        _data.push_back( static_cast<uint8_t>( v >> 24 ) );
        _data.push_back( static_cast<uint8_t>( v >> 16 ) );
        _data.push_back( static_cast<uint8_t>( v >> 8 ) );
        _data.push_back( static_cast<uint8_t>( v ) );
    }

    void i32( int32_t v )
    {
        u32( static_cast<uint32_t>( v ) );
    }

    void str( const std::string & s )
    {
        u32( static_cast<uint32_t>( s.size() ) );
        _data.insert( _data.end(), s.begin(), s.end() );
    }

    void raw( const std::vector<uint8_t> & bytes )
    {
        _data.insert( _data.end(), bytes.begin(), bytes.end() );
    }

    std::vector<uint8_t> take()
    {
        return std::move( _data );
    }

private:
    std::vector<uint8_t> _data;
};

void writeHeroBase( Writer & w, const WorldData::HeroOut & h )
{
    w.i32( h.attack );
    w.i32( h.defense );
    w.i32( h.knowledge );
    w.i32( h.power );
    w.i16( h.centerX );
    w.i16( h.centerY );
    w.u32( h.modes );
    w.u32( h.spellPoints );
    w.u32( h.movePoints );
    w.u32( static_cast<uint32_t>( h.spells.size() ) );
    for ( int32_t s : h.spells )
        w.i32( s );
    // fheroes2's BagArtifacts always serializes maxCapacity (14) entries:
    // empty slots are Artifact::UNKNOWN (0, 0). A shorter list would make
    // BagArtifacts::isFull() return true (no UNKNOWN entries in the bag).
    w.u32( 14 );
    for ( const auto & a : h.artifacts ) {
        w.i32( a.first );
        w.i32( a.second );
    }
    for ( size_t i = h.artifacts.size(); i < 14; ++i ) {
        w.i32( 0 );
        w.i32( 0 );
    }
}

void writeArmy( Writer & w, const int32_t ( &ids )[5], const uint32_t ( &counts )[5], bool spread, uint8_t color )
{
    w.u32( 5 );
    for ( int i = 0; i < 5; ++i ) {
        w.i32( ids[i] );
        w.u32( counts[i] );
    }
    w.u8( spread ? 1 : 0 );
    w.u8( color );
}

void writeHero( Writer & w, const WorldData::HeroOut & h )
{
    writeHeroBase( w, h );
    w.str( h.name );
    w.u8( h.color );
    w.u32( h.experience );
    w.u32( static_cast<uint32_t>( h.secSkills.size() ) );
    for ( const auto & s : h.secSkills ) {
        w.i32( s.first );
        w.i32( s.second );
    }
    writeArmy( w, h.monsterIds, h.monsterCounts, h.spread, h.armyColor );
    w.i32( h.id );
    w.i32( h.portrait );
    w.i32( h.race );
    w.u16( h.objectTypeUnderHero );
    w.u8( h.pathHide ? 1 : 0 );
    w.u32( static_cast<uint32_t>( h.path.size() ) );
    for ( const auto & p : h.path ) {
        w.i32( std::get<0>( p ) );
        w.i32( std::get<1>( p ) );
        w.u32( std::get<2>( p ) );
    }
    w.i32( h.direction );
    w.i32( h.spriteIndex );
    w.i32( h.patrolX );
    w.i32( h.patrolY );
    w.u32( h.patrolDistance );
    w.u32( static_cast<uint32_t>( h.visitedObjects.size() ) );
    for ( const auto & v : h.visitedObjects ) {
        w.i32( v.first );
        w.u16( v.second );
    }
    w.u32( h.lastGroundRegion );
}

void writeTile( Writer & w, const WorldData::TileOut & t )
{
    w.i32( t.index );
    w.u16( t.terrainImageIndex );
    w.u8( t.terrainFlags );
    w.u16( t.passability );
    w.u8( t.mainLayerType );
    w.u32( t.mainUid );
    w.u8( t.mainIcnType );
    w.u8( t.mainIcnIndex );
    w.u16( t.mainObjectType );
    w.u8( t.fogColors );
    w.u32( 3 );
    w.u32( t.metadata[0] );
    w.u32( t.metadata[1] );
    w.u32( t.metadata[2] );
    w.u8( t.occupantHeroId );
    w.u8( t.isRoad ? 1 : 0 );
    w.u32( static_cast<uint32_t>( t.groundParts.size() ) );
    for ( const auto & part : t.groundParts ) {
        w.u8( part.layer );
        w.u32( part.uid );
        w.u8( part.icnType );
        w.u8( part.icnIndex );
    }
    w.u32( static_cast<uint32_t>( t.topParts.size() ) );
    for ( const auto & part : t.topParts ) {
        w.u8( part.layer );
        w.u32( part.uid );
        w.u8( part.icnType );
        w.u8( part.icnIndex );
    }
    w.u8( t.boatOwnerColor );
}

void writeCaptain( Writer & w, const WorldData::CastleOut & c )
{
    w.i32( c.capAttack );
    w.i32( c.capDefense );
    w.i32( c.capKnowledge );
    w.i32( c.capPower );
    w.i16( c.capX );
    w.i16( c.capY );
    w.u32( c.capModes );
    w.u32( c.capSpellPoints );
    w.u32( c.capMovePoints );
    w.u32( static_cast<uint32_t>( c.capSpells.size() ) );
    for ( int32_t s : c.capSpells )
        w.i32( s );
    w.u32( 14 );
    for ( const auto & a : c.capArtifacts ) {
        w.i32( a.first );
        w.i32( a.second );
    }
    for ( size_t i = c.capArtifacts.size(); i < 14; ++i ) {
        w.i32( 0 );
        w.i32( 0 );
    }
}

void writeCastle( Writer & w, const WorldData::CastleOut & c )
{
    w.i16( c.x );
    w.i16( c.y );
    w.u32( c.modes );
    w.i32( c.race );
    w.u32( c.builtBuildings );
    w.u32( c.disabledBuildings );
    writeCaptain( w, c );
    w.u8( c.color );
    w.str( c.name );
    w.u32( static_cast<uint32_t>( c.mageGuildGeneral.size() ) );
    for ( int32_t s : c.mageGuildGeneral )
        w.i32( s );
    w.u32( static_cast<uint32_t>( c.mageGuildLibrary.size() ) );
    for ( int32_t s : c.mageGuildLibrary )
        w.i32( s );
    w.u32( static_cast<uint32_t>( c.dwellingCounts.size() ) );
    for ( uint32_t d : c.dwellingCounts )
        w.u32( d );
    writeArmy( w, c.garrisonIds, c.garrisonCounts, c.garrisonSpread, c.garrisonColor );
}

void writeKingdom( Writer & w, const WorldData::KingdomOut & k, uint16_t formatVersion )
{
    w.u32( k.modes );
    w.u8( k.color );
    for ( int32_t r : k.resources )
        w.i32( r );
    w.i32( k.lostTownDays );
    w.u32( static_cast<uint32_t>( k.castleIndices.size() ) );
    for ( int32_t v : k.castleIndices )
        w.i32( v );
    w.u32( static_cast<uint32_t>( k.heroIndices.size() ) );
    for ( int32_t v : k.heroIndices )
        w.i32( v );
    for ( int i = 0; i < 2; ++i ) {
        w.i32( k.recruitIds[i] );
        w.u32( k.recruitDays[i] );
    }
    w.u32( static_cast<uint32_t>( k.visitedObjects.size() ) );
    for ( const auto & v : k.visitedObjects ) {
        w.i32( v.first );
        w.u16( v.second );
    }
    w.str( k.puzzleString );
    for ( const auto * zone : { &k.puzzleZone1, &k.puzzleZone2, &k.puzzleZone3, &k.puzzleZone4 } ) {
        w.u8( static_cast<uint8_t>( zone->size() ) );
        for ( uint8_t t : *zone )
            w.u8( t );
    }
    w.i32( k.visitedTentsColors );
    w.i32( k.topCastleInKingdomView );
    w.i32( k.topHeroInKingdomView );
    if ( formatVersion >= 10034 ) {
        w.u32( static_cast<uint32_t>( k.monstersUnderVision.size() ) );
        for ( int32_t v : k.monstersUnderVision )
            w.i32( v );
    }
}

void writePlayer( Writer & w, const WorldData::PlayerOut & p )
{
    w.u32( p.modes );
    w.i32( p.control );
    w.u8( p.color );
    w.i32( p.race );
    w.u8( p.friendsColors );
    w.str( p.name );
    w.i32( p.focusType );
    w.i32( p.focusIndex );
    w.i32( p.aiPersonality );
    w.u8( p.handicapStatus );
}

void writeFileInfo( Writer & w, const h2::Header & h, const ConvertOptions & opt, uint32_t timestamp )
{
    // Non-empty filename (fallback: the map name) — required by fh2core's
    // SaveFile::findPlayers, which bails when the header filename is empty and so
    // never fills the player list (humanColors()); that made the poster's "fog
    // auto" POV wrong for HoMM2-converted saves.
    w.str( h.mapName );
    w.str( h.mapName );
    w.str( h.description );
    w.u16( static_cast<uint16_t>( h.mapWidth ) );
    w.u16( static_cast<uint16_t>( h.mapHeight ) );
    w.u8( h.difficulty8 );
    w.u8( 6 ); // kingdommax (always the maximum number of players)
    for ( int i = 0; i < 6; ++i )
        w.u8( ( h.playerFactions[i] <= 5 ) ? static_cast<uint8_t>( 1 << h.playerFactions[i] ) : 0 );
    for ( int i = 0; i < 6; ++i )
        w.u8( 0 ); // unions
    uint8_t kingdomColors = 0;
    for ( int i = 0; i < 6; ++i ) {
        if ( !h.playerDead[i] )
            kingdomColors |= static_cast<uint8_t>( 1 << i );
    }
    w.u8( kingdomColors );
    w.u8( 0 ); // colorsAvailableForHumans
    w.u8( 0 ); // colorsAvailableForComp
    w.u8( 0 ); // colorsOfRandomRaces
    w.u8( h.winConditionType );
    w.u8( 0 ); // compAlsoWins
    w.u8( h.allowNormalVictory ? 1 : 0 );
    w.u16( static_cast<uint16_t>( h.winConditionArg[0] ) );
    w.u16( static_cast<uint16_t>( h.winConditionArg[1] ) );
    w.u8( h.lossConditionType );
    w.u16( static_cast<uint16_t>( h.lossConditionArg[0] ) );
    w.u16( static_cast<uint16_t>( h.lossConditionArg[1] ) );
    w.u32( timestamp );
    w.u8( h.startWithHeroInFirstCastle ? 1 : 0 );
    w.i32( opt.polResources ? 1 : 0 ); // GameVersion: SUCCESSION_WARS=0, PRICE_OF_LOYALTY=1
    w.u32( static_cast<uint32_t>( h.day ) );
    w.u32( static_cast<uint32_t>( h.week ) );
    w.u32( static_cast<uint32_t>( h.month ) );
    w.u8( 0 ); // mainLanguage: English
    if ( opt.formatVersion >= 10033 )
        w.str( "" ); // creatorNotes
}

} // namespace

std::vector<uint8_t> serializeWorld( const WorldData & world, uint16_t formatVersion )
{
    Writer w;

    w.u32( static_cast<uint32_t>( world.width ) );
    w.u32( static_cast<uint32_t>( world.height ) );
    w.u32( static_cast<uint32_t>( world.tiles.size() ) );
    for ( const auto & t : world.tiles )
        writeTile( w, t );

    w.u32( static_cast<uint32_t>( world.heroes.size() ) );
    for ( const auto & h : world.heroes )
        writeHero( w, h );

    w.u32( static_cast<uint32_t>( world.castles.size() ) );
    for ( const auto & c : world.castles )
        writeCastle( w, c );

    w.u32( static_cast<uint32_t>( world.kingdoms.size() ) );
    for ( const auto & k : world.kingdoms )
        writeKingdom( w, k, formatVersion );

    w.u32( static_cast<uint32_t>( world.customRumors.size() ) );
    for ( const auto & s : world.customRumors )
        w.str( s );

    w.u32( 0 ); // vec_eventsday (empty)

    w.u32( static_cast<uint32_t>( world.capturedObjects.size() ) );
    for ( const auto & co : world.capturedObjects ) {
        // CapturedObject: ObjectColor (type u16, color u8) + Troop
        // (monsterId i32, count u32).
        w.i32( std::get<0>( co ) );
        w.u16( std::get<1>( co ) );
        w.u8( std::get<2>( co ) );
        const auto & guardians = std::get<3>( co );
        if ( guardians.empty() ) {
            w.i32( 0 );
            w.u32( 0 );
        }
        else {
            w.i32( guardians[0].first );
            w.u32( guardians[0].second );
        }
    }

    // UltimateArtifact: Artifact (id, ext) + _index + _isFound + _offset.
    w.i32( world.ultimateArtifactId );
    w.i32( world.ultimateArtifactExt );
    w.i32( world.ultimateArtifactIndex );
    w.u8( world.ultimateArtifactFound ? 1 : 0 );
    w.i32( world.ultimateArtifactOffsetX );
    w.i32( world.ultimateArtifactOffsetY );

    w.u32( world.day );
    w.u32( world.week );
    w.u32( world.month );
    w.i32( world.heroIdAsWinCondition );
    w.i32( world.heroIdAsLossCondition );

    w.u32( 0 ); // map_objects (empty)
    w.u32( world.seed );

    return w.take();
}

std::vector<uint8_t> buildSaveFile( const h2::Header & srcHeader, const WorldData & world, const ConvertOptions & options )
{
    const uint32_t timestamp = static_cast<uint32_t>( time( nullptr ) );

    // Header (uncompressed).
    Writer header;
    header.u16( 0xFF03 );
    header.str( std::to_string( options.formatVersion ) );
    header.u16( options.formatVersion );
    header.u16( options.polResources ? 0x4000 : 0 );
    writeFileInfo( header, srcHeader, options, timestamp );
    header.i32( options.campaign ? 2 : 1 ); // gameType

    // Uncompressed stream.
    Writer stream;
    stream.raw( serializeWorld( world, options.formatVersion ) );

    // Settings.
    stream.str( world.gameLanguage );
    writeFileInfo( stream, srcHeader, options, timestamp );
    stream.i32( world.gameDifficulty );
    stream.i32( world.gameType );

    // Players: colors bitmask + current color, then one record per set bit.
    stream.u8( world.playersColors );
    stream.u8( world.currentPlayerColor );
    for ( const auto & p : world.players )
        writePlayer( stream, p );

    // GameOver::Result: colors + empty high score map.
    stream.u8( world.gameOverColors );
    stream.u32( 0 );

    // CampaignSaveData.
    if ( options.campaign ) {
        stream.i32( options.campaignId );
        stream.i32( options.scenarioId );
        stream.i32( options.campaignBonusId );
        stream.u32( static_cast<uint32_t>( options.finishedMaps.size() ) );
        for ( const auto & m : options.finishedMaps ) {
            stream.i32( m.first );
            stream.i32( m.second );
        }
        stream.u32( static_cast<uint32_t>( options.bonusesForFinishedMaps.size() ) );
        for ( int32_t v : options.bonusesForFinishedMaps )
            stream.i32( v );
        stream.u32( static_cast<uint32_t>( options.daysPassed.size() ) );
        for ( uint32_t v : options.daysPassed )
            stream.u32( v );
        stream.u32( static_cast<uint32_t>( options.campaignAwards.size() ) );
        for ( int32_t v : options.campaignAwards )
            stream.i32( v );
        stream.u32( static_cast<uint32_t>( options.carryOverTroops.size() ) );
        for ( const auto & t : options.carryOverTroops ) {
            stream.i32( t.first );
            stream.u32( t.second );
        }
        stream.i32( options.difficulty );
        stream.i32( options.minDifficulty );
    }

    // End marker.
    stream.u16( 0xFF03 );

    std::vector<uint8_t> raw = stream.take();

    // Compress (zlib, RFC 1950).
    uLongf dstLen = compressBound( static_cast<uLong>( raw.size() ) );
    std::vector<uint8_t> z( dstLen );
    if ( compress2( z.data(), &dstLen, raw.data(), static_cast<uLong>( raw.size() ), Z_BEST_COMPRESSION ) != Z_OK )
        return {};
    z.resize( dstLen );

    Writer out;
    out.raw( header.take() );
    out.u32( static_cast<uint32_t>( raw.size() ) );
    out.u32( static_cast<uint32_t>( z.size() ) );
    out.u16( 0 ); // compressionVersion
    out.u16( 0 ); // unused
    out.raw( z );

    return out.take();
}

} // namespace h2conv
