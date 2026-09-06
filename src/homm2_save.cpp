// Parser for the original Heroes of Might and Magic II save files.
// See docs/HOMM2_SAVE_FORMAT.md for the layout reference.

#include "homm2_save.h"

#include <cstring>

namespace h2 {

namespace {

struct Reader {
    const uint8_t * d = nullptr;
    size_t size = 0;
    size_t p = 0;
    bool ok = true;

    explicit Reader( const std::vector<uint8_t> & data )
        : d( data.data() )
        , size( data.size() )
    {}

    bool need( size_t n )
    {
        if ( p + n > size ) {
            ok = false;
            return false;
        }
        return true;
    }

    uint8_t u8()
    {
        if ( !need( 1 ) ) return 0;
        return d[p++];
    }

    int8_t i8()
    {
        return static_cast<int8_t>( u8() );
    }

    uint16_t u16()
    {
        if ( !need( 2 ) ) return 0;
        uint16_t v = static_cast<uint16_t>( d[p] | ( d[p + 1] << 8 ) );
        p += 2;
        return v;
    }

    int16_t i16()
    {
        return static_cast<int16_t>( u16() );
    }

    uint32_t u32()
    {
        if ( !need( 4 ) ) return 0;
        uint32_t v = static_cast<uint32_t>( d[p] ) | ( static_cast<uint32_t>( d[p + 1] ) << 8 )
                     | ( static_cast<uint32_t>( d[p + 2] ) << 16 ) | ( static_cast<uint32_t>( d[p + 3] ) << 24 );
        p += 4;
        return v;
    }

    int32_t i32()
    {
        return static_cast<int32_t>( u32() );
    }

    void bytes( std::vector<uint8_t> & out, size_t n )
    {
        out.clear();
        if ( !need( n ) ) return;
        out.assign( d + p, d + p + n );
        p += n;
    }

    void skip( size_t n )
    {
        if ( !need( n ) ) return;
        p += n;
    }

    std::string fixedString( size_t n )
    {
        if ( !need( n ) ) return {};
        const char * start = reinterpret_cast<const char *>( d + p );
        const char * end = static_cast<const char *>( std::memchr( start, 0, n ) );
        size_t len = end ? static_cast<size_t>( end - start ) : n;
        std::string s( start, len );
        p += n;
        return s;
    }
};

} // namespace

bool parseSave( const std::vector<uint8_t> & data, Save & out )
{
    out = Save();
    Reader r( data );

    // Layout detection: the PoL expansion marker is FF FF FF FF at offset 0.
    bool expansion = data.size() > 4 && data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF;
    int wOff = expansion ? 4 : 0;
    if ( data.size() < static_cast<size_t>( wOff ) + 8 ) return false;

    Header & h = out.header;
    h.expansion = expansion;
    h.mapWidth = static_cast<int>( static_cast<int32_t>( data[wOff] | ( data[wOff + 1] << 8 ) | ( data[wOff + 2] << 16 ) | ( data[wOff + 3] << 24 ) ) );
    h.mapHeight = static_cast<int>( static_cast<int32_t>( data[wOff + 4] | ( data[wOff + 5] << 8 ) | ( data[wOff + 6] << 16 ) | ( data[wOff + 7] << 24 ) ) );
    if ( h.mapWidth < 1 || h.mapWidth > 256 || h.mapHeight < 1 || h.mapHeight > 256 ) return false;

    // SMapHeader.
    r.p = wOff + 8 + 58;
    h.mapName = r.fixedString( 60 );
    h.description = r.fixedString( 298 );
    r.p = wOff + 8 + 26;
    h.numPlayers = r.u8();
    r.p = wOff + 8 + 29;
    h.winConditionType = r.u8();
    r.p = wOff + 8 + 31;
    h.allowNormalVictory = r.u8() != 0;
    r.p = wOff + 8 + 32;
    h.winConditionArg[0] = r.i16();
    r.p = wOff + 8 + 34;
    h.lossConditionType = r.u8();
    h.lossConditionArg[0] = r.i8();
    r.p = wOff + 8 + 37;
    h.startWithHeroInFirstCastle = r.u8() == 0;
    h.playerFactions.assign( data.begin() + wOff + 8 + 38, data.begin() + wOff + 8 + 44 );
    r.p = wOff + 8 + 44;
    h.winConditionArg[1] = r.i16();
    h.lossConditionArg[1] = r.i16();

    // Fixed header fields after SMapHeader.
    h.difficulty8 = static_cast<uint8_t>( data[wOff + 452] ); // 0..4: easy..impossible
    // somePlayerCodeOr10IfMayBeHuman[6] (value 10 = player may be human).
    r.p = wOff + 446;
    h.playerMayBeHuman.assign( 6, 0 );
    for ( uint8_t & v : h.playerMayBeHuman )
        v = r.u8();
    h.humanPlayers = data[wOff + 475]; // numHumanPlayers
    r.p = wOff + 428 + 65;
    r.u8(); // gbIAmGreatest
    h.difficulty = r.i16();
    r.p = wOff + 500;
    for ( int i = 0; i < 6; ++i )
        h.playerNames.push_back( r.fixedString( 21 ) );

    // Campaign detection.
    r.p = wOff + 662;
    uint32_t inCampaign = r.u32();
    h.isCampaign = ( inCampaign == 1 || inCampaign == 2 );
    h.isPoLCampaign = ( inCampaign == 2 );
    int shift = 0;
    if ( inCampaign == 1 ) {
        h.variant = Variant::SwCampaign;
        r.bytes( h.campaignBlock, 327 );
        shift = 327;
    }
    else if ( inCampaign == 2 ) {
        h.variant = Variant::PoLCampaign;
        r.bytes( h.campaignBlock, 79 );
        shift = 79;
        // xIsExpansionMap follows; then the file continues with baseGame = 0 layout.
        if ( r.need( 1 ) )
            ++shift;
    }
    else {
        h.variant = Variant::Standard;
    }
    if ( expansion && inCampaign != 2 )
        ++shift; // xIsExpansionMap byte after the campaign block
    h.baseOffset = wOff + shift;
    h.heroRecordSize = ( expansion || inCampaign == 2 ) ? kHeroSizePoL : kHeroSizeBase;

    r.p = h.baseOffset + 666;
    r.u32(); // giMapChangeCtr
    h.slotName = r.fixedString( 14 ); // save name in the game's Save/Load menu
    r.u8();      // numPlayers (stored again, use header value)
    h.curPlayer = r.u8();  // giCurPlayer (human / current player index)
    r.u8();                // couldBeNumDefeatedPlayers
    h.playerDead.assign( 6, 0 );
    for ( uint8_t & v : h.playerDead ) v = r.u8();
    h.playerAlive.assign( 6, 0 );
    for ( uint8_t & v : h.playerAlive ) v = r.u8();
    h.day = r.i16();
    h.week = r.i16();
    h.month = r.i16();

    // Players.
    out.players.resize( kNumPlayers );
    for ( Player & pl : out.players ) {
        pl.color = r.u8();
        pl.numHeroes = r.u8();
        pl.curHeroIdx = r.i8();
        r.u8(); // relatedToSomeSortOfHeroCountOrIdx
        pl.heroesOwned.resize( 8 );
        for ( int8_t & v : pl.heroesOwned ) v = r.i8();
        pl.heroesForPurchase.resize( 2 );
        for ( int8_t & v : pl.heroesForPurchase ) v = r.i8();
        r.skip( 42 );
        r.u8(); // _B[1] (cheat flag)
        r.u8(); // _3[0]
        pl.personality = r.i32();
        r.u8(); // relatedToMaxOrNumHeroes
        pl.hasEvilFaction = r.u8();
        r.skip( 3 ); // field_40..42
        pl.daysLeftWithoutCastle = r.i8();
        pl.numCastles = r.u8();
        pl.curCastleIdx = r.i8();
        r.u8(); // relatedToUnknown
        pl.castlesOwned.resize( 72 );
        for ( int8_t & v : pl.castlesOwned ) v = r.i8();
        pl.resources.resize( 7 );
        for ( uint32_t & v : pl.resources ) v = r.u32();
        r.skip( 28 ); // field_E7
        r.skip( 2 );  // barrierTentsVisited x2
        r.skip( 6 );  // _4_2
    }

    h.numObelisks = r.u8();

    // Heroes.
    out.heroes.resize( kNumHeroes );
    for ( Hero & hero : out.heroes ) {
        hero.spellPoints = r.i16();
        hero.idx = r.u8();
        hero.ownerIdx = r.i8();
        r.skip( 6 ); // field_4..9
        hero.name = r.fixedString( 13 );
        hero.factionID = r.u8();
        hero.heroID = r.u8();
        hero.x = r.i32();
        hero.y = r.i32();
        r.skip( 8 ); // field_21..27
        r.skip( 4 ); // relatedToX/Y/FactionID, directionFacing
        hero.occupiedObjType = r.i16();
        hero.occupiedObjVal = r.i16();
        hero.mobility = r.i32();
        hero.remainingMobility = r.i32();
        hero.experience = r.i32();
        hero.oldLevel = r.i16();
        hero.primarySkills.resize( 4 );
        for ( uint8_t & v : hero.primarySkills ) v = r.u8();
        r.skip( 1 ); // field_43
        hero.tempMorale = r.u8();
        hero.tempLuck = r.u8();
        r.skip( 1 ); // field_46
        r.skip( 28 ); // visitation counters
        hero.randomSeed = r.u8();
        hero.wisdomLastOffered = r.u8();
        hero.army.creatureTypes.resize( 5 );
        for ( int8_t & v : hero.army.creatureTypes ) v = r.i8();
        hero.army.quantities.resize( 5 );
        for ( uint16_t & v : hero.army.quantities ) v = r.u16();
        hero.secondarySkillLevel.resize( 14 );
        for ( uint8_t & v : hero.secondarySkillLevel ) v = r.u8();
        hero.secondarySkillIdx.resize( 14 );
        for ( uint8_t & v : hero.secondarySkillIdx ) v = r.u8();
        hero.numSecSkills = r.u32();
        hero.spellsLearned.resize( 65 );
        for ( uint8_t & v : hero.spellsLearned ) v = r.u8();
        hero.artifacts.resize( 14 );
        for ( int8_t & v : hero.artifacts ) v = r.i8();
        hero.flags = r.u32();
        hero.isCaptain = r.u8() != 0;
        r.skip( 4 ); // field_E8
        if ( h.heroRecordSize == kHeroSizePoL ) {
            hero.scrollSpell.resize( 14 );
            for ( uint8_t & v : hero.scrollSpell ) v = r.u8();
        }
    }

    out.heroForHireStatus.resize( 54 );
    for ( uint8_t & v : out.heroForHireStatus ) v = r.u8();

    // Castles.
    out.castles.resize( kNumCastles );
    for ( Castle & c : out.castles ) {
        c.idx = r.u8();
        c.ownerIdx = r.i8();
        c.alignment = r.u8();
        c.factionID = r.u8();
        c.x = r.u8();
        c.y = r.u8();
        r.skip( 2 ); // buildDockRelated, field_7
        c.garrison.creatureTypes.resize( 5 );
        for ( int8_t & v : c.garrison.creatureTypes ) v = r.i8();
        c.garrison.quantities.resize( 5 );
        for ( uint16_t & v : c.garrison.quantities ) v = r.u16();
        c.visitingHeroIdx = r.i8();
        c.buildingsBuilt = r.u32();
        c.mageGuildLevel = r.u8();
        r.skip( 1 ); // field_1D
        c.dwellingCounts.resize( 12 );
        for ( uint16_t & v : c.dwellingCounts ) v = r.u16();
        c.exists = r.u8() != 0;
        c.mayNotBeUpgradedToCastle = r.u8() != 0;
        r.skip( 1 ); // field_38
        c.playerPos = r.u8();
        r.skip( 2 ); // extraIdx
        c.mageGuildSpells.resize( 5 );
        for ( auto & lvl : c.mageGuildSpells ) {
            lvl.resize( 4 );
            for ( uint8_t & v : lvl ) v = r.u8();
        }
        c.numSpellsOfLevel.resize( 5 );
        for ( uint8_t & v : c.numSpellsOfLevel ) v = r.u8();
        r.skip( 2 ); // field_55
        c.name = r.fixedString( 12 );
        r.skip( 1 ); // field_63
    }

    r.skip( 72 ); // field_2773
    r.skip( 9 );  // field_27BB

    // Mines.
    out.mines.resize( kNumMines );
    for ( Mine & m : out.mines ) {
        r.u8(); // field_0
        m.owner = r.u8();
        m.type = r.u8();
        m.guardianType = r.u8();
        m.guardianQty = r.u8();
        m.x = r.u8();
        m.y = r.u8();
    }
    r.skip( 144 ); // field_60A6
    r.skip( expansion ? 103 : 82 ); // artifactGeneratedRandomly

    // Boats.
    out.boats.resize( kNumBoats );
    for ( Boat & b : out.boats ) {
        b.idx = r.u8();
        b.x = r.u8();
        b.y = r.u8();
        r.skip( 4 ); // field_3, underlyingObjType, underlyingObjExtra, field_6
        b.owner = r.u8();
    }
    out.boatBuilt.resize( 48 );
    for ( uint8_t & v : out.boatBuilt ) v = r.u8();
    out.obeliskVisitedMasks.resize( 48 );
    for ( uint8_t & v : out.obeliskVisitedMasks ) v = r.u8();

    out.ultimateArtifactX = r.i8();
    out.ultimateArtifactY = r.i8();
    out.ultimateArtifactIdx = r.u8();
    r.skip( 301 ); // currentRumor
    r.skip( 24 );  // field_637D

    // Event counters. Due to memory packing each counter is written as 4
    // bytes: [u16 value][first two index bytes], followed by 2*value bytes
    // of the remaining indices.
    uint32_t numRumors = r.u16();
    r.skip( 2 + 2 * numRumors );
    uint32_t numEvents = r.u16();
    r.skip( 2 + 2 * numEvents );
    uint32_t numMapEvents = r.u16();
    r.skip( 2 + 2 * numMapEvents );
    if ( !r.ok ) return false;

    // Map extras: [1234][iMaxMapExtra][1234] then per entry [1234][size u16][data].
    r.u32(); // marker 1234
    uint32_t maxExtra = r.u32();
    r.u32(); // marker 1234
    out.mapExtras.clear();
    out.mapExtras.push_back( {} ); // index 0 unused
    for ( uint32_t i = 1; i < maxExtra; ++i ) {
        r.u32(); // marker 1234
        uint16_t sz = r.u16();
        std::vector<uint8_t> blob;
        r.bytes( blob, sz );
        out.mapExtras.push_back( std::move( blob ) );
    }
    r.u32(); // marker 1234

    // mapRevealed: mapWidth * mapHeight bytes.
    out.mapRevealed.assign( static_cast<size_t>( h.mapWidth ) * h.mapHeight, 0 );
    for ( uint8_t & v : out.mapRevealed ) v = r.u8();
    r.u32(); // marker 1234

    // fullMap: width, height, tiles, numCellExtras, cellExtras.
    int mw = r.i32();
    int mh = r.i32();
    if ( mw != h.mapWidth || mh != h.mapHeight ) return false;
    out.tiles.resize( static_cast<size_t>( mw ) * mh );
    for ( MapCell & c : out.tiles ) {
        c.groundIndex = r.u16();
        c.bitfield1 = r.u8();
        c.objectIndex = r.u8();
        c.bitfield4 = r.u16();
        c.bitfield6 = r.u8();
        c.overlayIndex = r.u8();
        c.flags = r.u8();
        c.objType = r.u8();
        c.extraIdx = r.u16();
    }
    uint32_t numExtras = r.u32();
    out.cellExtras.resize( numExtras );
    for ( MapCellExtra & e : out.cellExtras ) {
        e.nextIdx = r.i16();
        e.b1 = r.u8();
        e.objectIndex = r.u8();
        e.b4 = r.u8();
        e.b6 = r.u8();
        e.field6 = r.u8();
    }
    r.u32(); // final marker 1234

    return r.ok && r.p == r.size;
}

} // namespace h2
