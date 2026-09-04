// Conversion from the original HoMM2 save to the fheroes2 world model.

#include "convert.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <set>

namespace h2 {

int32_t mapCreature( int8_t id )
{
    if ( id < 0 )
        return 0; // UNKNOWN (empty slot)
    return static_cast<int32_t>( id ) + 1;
}

int32_t mapRace( uint8_t faction )
{
    if ( faction > 5 )
        return 0;
    return 1 << faction;
}

uint8_t mapColor( uint8_t color )
{
    if ( color > 5 )
        return 0;
    return static_cast<uint8_t>( 1 << color );
}

uint32_t mapBuildings( uint32_t homm2Flags, uint8_t mageGuildLevel )
{
    // homm2 bit -> fheroes2 bit. Verified on saves: bit 25 is the
    // Dwelling-2 upgrade (Cottage), etc.
    static const uint32_t kTable[] = {
        0x00000000, // 0: Mage Guild (levels written separately)
        0x00000001, // 1: Thieves' Guild
        0x00000002, // 2: Tavern
        0x00000004, // 3: Shipyard
        0x00000008, // 4: Well
        0x00080000, // 5: Tent
        0x00000800, // 6: Castle
        0x00000010, // 7: Statue
        0x00000020, // 8: Left Turret
        0x00000040, // 9: Right Turret
        0x00000080, // 10: Marketplace
        0x00000100, // 11: Wel2 (resource silo)
        0x00000200, // 12: Moat
        0x00000400, // 13: Spec (fortifications etc.)
        0x00002000, // 14: Shrine (PoL)
        0x00001000, // 15: Captain's Quarters
        0x00000000, // 16: (unused)
        0x00000000, // 17: (unused)
        0x00000000, // 18: (unused)
        0x00100000, // 19: Dwelling 1
        0x00200000, // 20: Dwelling 2
        0x00400000, // 21: Dwelling 3
        0x00800000, // 22: Dwelling 4
        0x01000000, // 23: Dwelling 5
        0x02000000, // 24: Dwelling 6
        0x04000000, // 25: Dwelling 2 upgrade
        0x08000000, // 26: Dwelling 3 upgrade
        0x10000000, // 27: Dwelling 4 upgrade
        0x20000000, // 28: Dwelling 5 upgrade
        0x40000000, // 29: Dwelling 6 upgrade
        0x80000000, // 30: Dwelling 6 upgrade 2
    };

    uint32_t out = 0;
    for ( int bit = 0; bit < 31; ++bit ) {
        if ( homm2Flags & ( 1u << bit ) )
            out |= kTable[bit];
    }
    for ( int lvl = 1; lvl <= mageGuildLevel && lvl <= 5; ++lvl )
        out |= ( 0x00004000u << ( lvl - 1 ) );

    return out;
}

namespace {

uint32_t mapSecondary( uint8_t idx )
{
    return ( idx <= 13 ) ? static_cast<uint32_t>( idx ) + 1 : 0;
}

// fheroes2 constants used by the tile conversion (values from the engine's
// mp2.h / maps_tiles.h; see docs/CONVERSION.md for the full mapping).
namespace fh {
constexpr uint8_t kLayerObject = 0;
constexpr uint8_t kLayerBackground = 1;
constexpr uint8_t kLayerShadow = 2;
constexpr uint8_t kLayerTerrain = 3;

constexpr uint8_t kIcnUnknown = 0;
constexpr uint8_t kIcnFlag32 = 14;
constexpr uint8_t kIcnRoad = 30;
constexpr uint8_t kIcnStream = 45;

constexpr uint16_t kObjReefs = 98;
constexpr uint16_t kObjCastle = 163;
constexpr uint16_t kObjRandomArtifact = 244;
constexpr uint16_t kObjRandomMonster = 175;
constexpr uint16_t kObjRandomMonsterWeak = 179;
constexpr uint16_t kObjRandomMonsterMedium = 180;
constexpr uint16_t kObjRandomMonsterStrong = 181;
constexpr uint16_t kObjRandomMonsterVeryStrong = 182;

constexpr uint16_t kDirAll = 0x01FF;
constexpr uint16_t kDirCenterBottom = 0x01F8; // DIRECTION_CENTER_ROW | DIRECTION_BOTTOM_ROW
} // namespace fh

// homm2's internal object type is almost identical to MP2::MapObjectType but
// a few values differ (verified on Slugfest): 170 is a castle (fheroes2:
// 163). Note: in the save homm2 stores 169 for a materialized artifact,
// which matches fheroes2's OBJ_ARTIFACT (169).
uint16_t mapObjectType( uint8_t homm2Type )
{
    switch ( homm2Type ) {
    case 170:
        return fh::kObjCastle;
    default:
        return homm2Type;
    }
}

// Mirrors Maps::Tile::isSpriteRoad: a part counts as a road sprite for
// certain frames of the road / town / random town ICNs.
bool isRoadSprite( uint8_t icnType, uint8_t icnIndex )
{
    switch ( icnType ) {
    case fh::kIcnRoad: {
        static const uint8_t frames[] = { 0,  2,  3,  4,  5,  6,  7,  9,  12, 13, 14, 16, 17, 18, 19, 20, 21, 26, 28, 29,
                                          30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48 };
        return std::find( std::begin( frames ), std::end( frames ), icnIndex ) != std::end( frames );
    }
    case 35: { // OBJ_ICN_TYPE_OBJNTOWN
        static const uint8_t frames[] = { 13, 29, 45, 61, 77, 93, 109, 125, 141, 157, 173, 189 };
        return std::find( std::begin( frames ), std::end( frames ), icnIndex ) != std::end( frames );
    }
    case 38: { // OBJ_ICN_TYPE_OBJNTWRD
        static const uint8_t frames[] = { 13, 29 };
        return std::find( std::begin( frames ), std::end( frames ), icnIndex ) != std::end( frames );
    }
    default:
        return false;
    }
}

// Mirrors MP2::getActionObjectDirection (tables generated from the
// fheroes2 sources).
uint16_t actionObjectDirection( uint16_t objectType )
{
    static const uint16_t allPassable[] = { 28,  131, 132, 134, 136, 139, 147, 152, 155, 161, 167, 169, 171, 172, 173, 174, 175,
                                            176, 177, 179, 180, 181, 182, 183, 218, 220, 221, 244, 245, 246, 247, 251, 387, 388 };
    if ( std::find( std::begin( allPassable ), std::end( allPassable ), objectType ) != std::end( allPassable ) )
        return fh::kDirAll;
    return fh::kDirCenterBottom;
}

// Auto-generated from fheroes2 (Maps::isValidShadowSprite).
bool isShadowSprite( uint8_t icnType, uint8_t frame )
{
    switch ( icnType ) {
    case 1: return false; // OBJ_ICN_TYPE_UNUSED_1
    case 2: return false; // OBJ_ICN_TYPE_UNUSED_2
    case 3: return false; // OBJ_ICN_TYPE_UNUSED_3
    case 4: return false; // OBJ_ICN_TYPE_UNUSED_4
    case 5: return false; // OBJ_ICN_TYPE_UNUSED_5
    case 6: return false; // OBJ_ICN_TYPE_BOAT32
    case 7: return false; // OBJ_ICN_TYPE_UNUSED_7
    case 8: return false; // OBJ_ICN_TYPE_UNUSED_8
    case 9: return false; // OBJ_ICN_TYPE_UNUSED_9
    case 10: return false; // OBJ_ICN_TYPE_OBJNHAUN
    case 11: return ( frame % 2 ) == 0; // OBJ_ICN_TYPE_OBJNARTI
    case 12: return false; // OBJ_ICN_TYPE_MONS32
    case 13: return false; // OBJ_ICN_TYPE_UNUSED_13
    case 14: return false; // OBJ_ICN_TYPE_FLAG32
    case 15: return false; // OBJ_ICN_TYPE_UNUSED_15
    case 16: return false; // OBJ_ICN_TYPE_UNUSED_16
    case 17: return false; // OBJ_ICN_TYPE_UNUSED_17
    case 18: return false; // OBJ_ICN_TYPE_UNUSED_18
    case 19: return false; // OBJ_ICN_TYPE_UNUSED_19
    case 20: return false; // OBJ_ICN_TYPE_MINIMON
    case 21: return false; // OBJ_ICN_TYPE_MINIHERO
    case 22: { static const uint8_t f[] = { 0, 5, 11, 17, 21, 26, 32, 38, 42, 45, 49, 52, 55, 59, 62, 65, 68, 71, 74, 75, 79, 80 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_MTNSNOW
    case 23: { static const uint8_t f[] = { 0, 5, 11, 17, 21, 26, 32, 38, 42, 45, 49, 52, 55, 59, 62, 65, 68, 71, 74, 75, 79, 80 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_MTNSWMP
    case 24: { static const uint8_t f[] = { 0, 5, 11, 17, 21, 26, 32, 38, 42, 45, 49, 52, 55, 59, 62, 65, 68, 71, 74, 75, 79, 80 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_MTNLAVA
    case 25: { static const uint8_t f[] = { 0, 5, 11, 17, 21, 26, 32, 38, 42, 45, 49, 52, 55, 59, 62, 65, 68, 71, 74, 75, 79, 80 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_MTNDSRT
    case 26: { static const uint8_t f[] = { 0, 5, 11, 17, 21, 26, 32, 38, 42, 47, 53, 62, 68, 72, 75, 79, 82, 85, 89, 92, 95, 98, 101, 104, 105, 109, 110 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_MTNDIRT
    case 27: { static const uint8_t f[] = { 0, 5, 11, 17, 21, 26, 32, 38, 42, 45, 49, 52, 55, 59, 62, 65, 68, 71, 74, 75, 79, 80 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_MTNMULT
    case 28: return false; // OBJ_ICN_TYPE_UNUSED_28
    case 29: return false; // OBJ_ICN_TYPE_EXTRAOVR
    case 30: return false; // OBJ_ICN_TYPE_ROAD
    case 31: { static const uint8_t f[] = { 0, 5, 11, 17, 21, 26, 32, 38, 42, 47, 53, 62, 68, 72, 75, 79, 82, 85, 89, 92, 95, 98, 101, 104, 105, 109, 110 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_MTNCRCK
    case 32: { static const uint8_t f[] = { 0, 5, 11, 17, 21, 26, 32, 38, 42, 45, 49, 52, 55, 59, 62, 65, 68, 71, 74, 75, 79, 80 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_MTNGRAS
    case 33: { static const uint8_t f[] = { 0, 3, 7, 10, 13, 17, 20, 23, 26, 29, 32, 34 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_TREJNGL
    case 34: { static const uint8_t f[] = { 0, 3, 7, 10, 13, 17, 20, 23, 26, 29, 32, 34 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_TREEVIL
    case 35: return false; // OBJ_ICN_TYPE_OBJNTOWN
    case 36: return false; // OBJ_ICN_TYPE_OBJNTWBA
    case 37: return true; // OBJ_ICN_TYPE_OBJNTWSH
    case 38: return frame > 31; // OBJ_ICN_TYPE_OBJNTWRD
    case 39: return false; // OBJ_ICN_TYPE_OBJNXTRA
    case 40: return frame == 1; // OBJ_ICN_TYPE_OBJNWAT2
    case 41: { static const uint8_t f[] = { 14, 17, 20, 24, 42, 43, 49, 50, 60, 71, 72, 113, 115, 118, 121, 123, 127, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 164, 180, 181, 182, 183, 184, 185, 186, 189, 199, 200, 202, 206 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNMUL2
    case 42: { static const uint8_t f[] = { 0, 3, 7, 10, 13, 17, 20, 23, 26, 29, 32, 34 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_TRESNOW
    case 43: { static const uint8_t f[] = { 0, 3, 7, 10, 13, 17, 20, 23, 26, 29, 32, 34 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_TREFIR
    case 44: { static const uint8_t f[] = { 0, 3, 7, 10, 13, 17, 20, 23, 26, 29, 32, 34 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_TREFALL
    case 45: return false; // OBJ_ICN_TYPE_STREAM
    case 46: return ( frame % 2 ) == 0; // OBJ_ICN_TYPE_OBJNRSRC
    case 47: return false; // OBJ_ICN_TYPE_UNUSED_47
    case 48: { static const uint8_t f[] = { 5, 14, 19, 20, 28, 31, 32, 33, 34, 35, 36, 37, 38, 47, 48, 49, 50, 51, 52, 53, 54, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 91, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 121, 124, 128 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNGRA2
    case 49: { static const uint8_t f[] = { 0, 3, 7, 10, 13, 17, 20, 23, 26, 29, 32, 34 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_TREDECI
    case 50: { static const uint8_t f[] = { 12, 13, 14, 15, 16, 17, 18, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 52, 55, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 184, 188, 189, 190, 191, 192, 193, 194, 240 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNWATR
    case 51: { static const uint8_t f[] = { 0, 4, 29, 32, 36, 39, 42, 44, 46, 48, 76, 82, 88, 92, 94, 98, 102, 105, 108, 111, 113, 120, 124, 128, 134, 138, 141, 143, 145, 147 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNGRAS
    case 52: { static const uint8_t f[] = { 21, 25, 29, 31, 33, 36, 40, 48, 54, 59, 63, 67, 70, 73, 76, 79, 101, 104, 105, 106, 107, 108, 109, 110, 111, 120, 121, 122, 123, 124, 125, 126, 127, 137, 140, 142, 144, 148, 193, 203, 207 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNSNOW
    case 53: { static const uint8_t f[] = { 2, 3, 14, 15, 16, 17, 18, 19, 20, 21, 31, 43, 44, 45, 46, 47, 48, 49, 66, 83, 125, 127, 130, 132, 136, 141, 163, 170, 175, 178, 195, 197, 202, 204, 207, 211, 215 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNSWMP
    case 54: { static const uint8_t f[] = { 45, 49, 79, 80, 81, 82, 109, 113, 116 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNLAVA
    case 55: { static const uint8_t f[] = { 11, 13, 16, 19, 23, 25, 27, 29, 33, 35, 38, 41, 44, 47, 50, 52, 54, 55, 56, 57, 58, 59, 60, 71, 75, 77, 80, 86, 103, 115, 118 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNDSRT
    case 56: { static const uint8_t f[] = { 0, 1, 5, 6, 14, 47, 52, 59, 62, 65, 68, 70, 72, 75, 78, 81, 84, 87, 91, 94, 97, 100, 103, 111, 114, 117, 126, 128, 136, 149, 150, 158, 161, 162, 163, 164, 165, 166, 167, 168, 177, 178, 179, 180, 181, 182, 183, 184, 193, 196, 200 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNDIRT
    case 57: { static const uint8_t f[] = { 2, 9, 13, 15, 20, 23, 28, 33, 36, 39, 45, 48, 51, 54, 56, 73, 75, 79, 200, 201, 207, 237 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNCRCK
    case 58: { static const uint8_t f[] = { 1, 2, 3, 4, 16, 17, 18, 19, 31, 32, 33, 34, 38, 46, 47, 48, 49, 50, 57, 58, 59, 61, 62, 63, 64, 76, 77, 91, 92, 93, 106, 107, 108, 109, 110, 111, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 136, 137, 138, 139, 142, 143, 144, 145, 146, 147, 148, 149, 166, 167, 168, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 243 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNLAV3
    case 59: { static const uint8_t f[] = { 1, 3, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 57, 59, 61, 67, 68, 75, 77, 79, 81, 83, 97, 98, 99, 100, 101, 102, 103, 105, 106, 107, 108, 109, 110, 113, 115, 121, 122, 124, 125, 126, 127, 128, 129, 130 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNMULT
    case 60: { static const uint8_t f[] = { 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 29, 34, 38, 39, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 72, 77, 78 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_OBJNLAV2
    case 61: { static const uint8_t f[] = { 1, 2, 32, 33, 34, 35, 36, 37, 38, 39, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 72, 78, 79, 83, 84, 112, 116, 120, 124, 125, 129, 133 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_X_LOC1
    case 62: { static const uint8_t f[] = { 2, 7, 10, 11, 12, 13, 14, 15, 16, 17, 18, 47, 48, 49, 50, 51, 52, 53, 54, 55, 83, 84, 85, 86, 87, 88, 89, 90, 91 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_X_LOC2
    case 63: { static const uint8_t f[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 41, 42, 43, 44, 45, 46, 47, 48, 49, 59, 65, 71, 77, 83, 89, 95, 101, 108, 109, 112, 113, 116, 117, 120, 121, 124, 125, 128, 129, 132, 133, 136, 137 }; return std::find( std::begin( f ), std::end( f ), frame ) != std::end( f ); } // OBJ_ICN_TYPE_X_LOC3
    default: return false;
    }
}

// Object types that cannot have an action variant (MP2::isObjectCanBeAction).
bool canBeActionObject( uint16_t type )
{
    static const uint16_t cannotBeAction[] = { 0,  28,  56,  57,  81,  98,  99,  100, 101, 102, 103, 104,
                                               105, 106, 107, 108, 109, 110, 111, 112, 113, 257, 258 };
    if ( type & 128 )
        return false;
    return std::find( std::begin( cannotBeAction ), std::end( cannotBeAction ), type ) == std::end( cannotBeAction );
}

uint16_t baseActionObjectType( uint16_t type )
{
    if ( type & 128 )
        return type;
    if ( !canBeActionObject( type ) )
        return type;
    return static_cast<uint16_t>( type | 128 );
}

// MP2::isShortObject equivalent.
bool isShortObject( uint16_t type )
{
    static const uint16_t ids[] = { 56, 61, 72, 73, 130, 135, 142, 159, 164, 165, 189, 194, 195, 200, 201, 202, 203, 208, 216, 222, 223, 247 };
    return std::find( std::begin( ids ), std::end( ids ), type ) != std::end( ids );
}

// Detached object types (MP2::isDetachedObjectType).
bool isDetachedObjectType( uint16_t type )
{
    static const uint16_t ids[] = { 135, 151, 157, 163, 165, 252, 253, 254, 255 };
    return std::find( std::begin( ids ), std::end( ids ), type ) != std::end( ids );
}

// Combined objects: trees and craters (MP2::isCombinedObject).
bool isCombinedObject( uint16_t type )
{
    return type == 99 || type == 108;
}

bool isTransparentLayer( uint8_t layer )
{
    return layer == fh::kLayerShadow || layer == fh::kLayerTerrain;
}

// (icnType, icnIndex) -> MP2::MapObjectType for non-action objects, used to
// fill in the object type of tiles which have no type in the original save
// (mirrors Maps::Tile::updateObjectType / Maps::getObjectTypeByIcn).
#include "type_by_icn.inc"

int32_t mapSpell( uint8_t id )
{
    return ( id <= 64 ) ? static_cast<int32_t>( id ) + 1 : 0;
}

int32_t mapArtifact( int8_t id )
{
    if ( id < 0 )
        return 0; // empty
    return static_cast<int32_t>( id ) + 1;
}

void convertHero( const Hero & src, fh2::WorldData::HeroOut & out, uint8_t colorBit )
{
    out.attack = src.primarySkills[0];
    out.defense = src.primarySkills[1];
    out.knowledge = src.primarySkills[3];
    out.power = src.primarySkills[2];
    out.centerX = static_cast<int16_t>( src.x );
    out.centerY = static_cast<int16_t>( src.y );
    out.modes = 0;
    out.spellPoints = static_cast<uint32_t>( std::max( int16_t( 0 ), src.spellPoints ) );
    out.movePoints = static_cast<uint32_t>( std::max( 0, src.remainingMobility ) );
    for ( int i = 0; i < 65; ++i ) {
        if ( src.spellsLearned[i] )
            out.spells.push_back( mapSpell( static_cast<uint8_t>( i ) ) );
    }
    for ( int8_t a : src.artifacts ) {
        if ( a >= 0 )
            out.artifacts.push_back( { mapArtifact( a ), 0 } );
    }
    out.name = src.name;
    out.color = ( src.ownerIdx >= 0 && src.ownerIdx < 6 ) ? colorBit : 0;
    out.experience = static_cast<uint32_t>( std::max( 0, src.experience ) );
    for ( int i = 0; i < 14; ++i ) {
        if ( src.secondarySkillIdx[i] != 0 && src.secondarySkillLevel[i] != 0 )
            out.secSkills.push_back( { static_cast<int32_t>( mapSecondary( src.secondarySkillIdx[i] ) ),
                                       static_cast<int32_t>( src.secondarySkillLevel[i] ) } );
    }
    for ( int i = 0; i < 5; ++i ) {
        out.monsterIds[i] = mapCreature( src.army.creatureTypes[i] );
        out.monsterCounts[i] = ( src.army.creatureTypes[i] < 0 ) ? 0 : src.army.quantities[i];
    }
    out.spread = true; // distributed combat formation (fheroes2 default)
    out.armyColor = out.color;
    out.id = static_cast<int32_t>( src.heroID ) + 1;
    out.portrait = out.id;
    out.race = mapRace( src.factionID );
    out.objectTypeUnderHero = mapObjectType( static_cast<uint8_t>( src.occupiedObjType & 0xFF ) );
    out.pathHide = false;
    out.direction = 2; // TOP_RIGHT
    out.spriteIndex = 0;
    out.patrolX = src.x;
    out.patrolY = src.y;
    out.patrolDistance = 0;
    out.lastGroundRegion = 0;
}

void convertCastle( const Castle & src, fh2::WorldData::CastleOut & out, uint8_t colorBit )
{
    out.x = static_cast<int16_t>( src.x );
    out.y = static_cast<int16_t>( src.y );
    // Without ALLOW_TO_BUILD_TODAY (bit 3) the castle behaves as if a
    // building was already constructed today: fheroes2 only sets this flag
    // when loading MP2 maps, not when loading saves.
    out.modes = 0x0008; // ALLOW_TO_BUILD_TODAY
    out.race = mapRace( src.factionID );
    out.builtBuildings = mapBuildings( src.buildingsBuilt, src.mageGuildLevel );
    out.disabledBuildings = 0;
    // Captain: an empty HeroBase positioned on the castle tile.
    out.capX = out.x;
    out.capY = out.y;
    out.color = colorBit;
    out.name = src.name;
    for ( const auto & lvl : src.mageGuildSpells ) {
        for ( uint8_t s : lvl ) {
            if ( s != 0xFF && s <= 64 )
                out.mageGuildGeneral.push_back( mapSpell( s ) );
        }
    }
    for ( int i = 0; i < 6; ++i ) {
        // fheroes2 keeps 6 dwelling slots; the original stores the base and
        // upgraded dwellings separately (12 values), so merge them.
        uint32_t count = 0;
        if ( i < static_cast<int>( src.dwellingCounts.size() ) )
            count += src.dwellingCounts[i];
        if ( i + 6 < static_cast<int>( src.dwellingCounts.size() ) )
            count += src.dwellingCounts[i + 6];
        out.dwellingCounts.push_back( count );
    }
    for ( int i = 0; i < 5; ++i ) {
        out.garrisonIds[i] = mapCreature( src.garrison.creatureTypes[i] );
        out.garrisonCounts[i] = ( src.garrison.creatureTypes[i] < 0 ) ? 0 : src.garrison.quantities[i];
    }
    out.garrisonSpread = true; // distributed combat formation (fheroes2 default)
    out.garrisonColor = colorBit;
}

// Ports Maps::Tile::fixMP2MapTileObjectType (the parts that apply to the
// original data layout): object types stored in the original save may need
// the same corrections fheroes2 applies to MP2 maps.
uint16_t fixObjectType( uint16_t objectType, uint8_t mainIcn, uint8_t mainFrame )
{
    if ( objectType == 132 && mainIcn == 55 && mainFrame == 83 ) { // OBJ_SKELETON
        return 4;                                                  // OBJ_NON_ACTION_SKELETON
    }
    if ( objectType == 28 && mainIcn == 12 ) { // OBJ_COAST with a monster placeholder is nonsense, keep
        // handled below for MONS32 placeholders
    }
    // Random monster placeholders: OBJ_ICN_TYPE_MONS32 (12) frames 66..70.
    if ( mainIcn == 12 ) {
        switch ( mainFrame ) {
        case 66:
            return fh::kObjRandomMonster;
        case 67:
            return fh::kObjRandomMonsterWeak;
        case 68:
            return fh::kObjRandomMonsterMedium;
        case 69:
            return fh::kObjRandomMonsterStrong;
        case 70:
            return fh::kObjRandomMonsterVeryStrong;
        default:
            break;
        }
    }
    return objectType;
}

// Mirrors Maps::Tile::getTileIndependentPassability for a tile whose parts
// and main object type (the type "under the hero") are known.
uint16_t tileIndependentPassability( const fh2::WorldData::TileOut & t, uint16_t objectType )
{
    uint16_t passability = fh::kDirAll;
    auto applyPart = [&]( uint8_t layer, uint8_t icn, bool isMain, uint16_t & out ) {
        if ( icn == fh::kIcnRoad || icn == fh::kIcnStream || icn == fh::kIcnFlag32 )
            return true;
        const uint16_t type = isMain ? objectType : 0;
        if ( type & 128 ) {
            out = out & actionObjectDirection( type );
            return false; // action object stops the calculation
        }
        if ( type == fh::kObjReefs ) {
            out = 0;
            return false;
        }
        if ( layer != fh::kLayerShadow && layer != fh::kLayerTerrain ) {
            out = out & fh::kDirCenterBottom;
        }
        return true;
    };

    if ( t.mainIcnType != fh::kIcnUnknown ) {
        if ( !applyPart( t.mainLayerType, t.mainIcnType, true, passability ) )
            return passability;
    }
    for ( auto it = t.groundParts.rbegin(); it != t.groundParts.rend(); ++it ) {
        if ( !applyPart( it->layer, it->icnType, false, passability ) )
            return passability;
    }
    return passability;
}

void convertTiles( const Save & src, fh2::WorldData & world, std::vector<uint16_t> & underHeroTypes )
{
    const int w = src.header.mapWidth;
    const int h = src.header.mapHeight;
    uint32_t nextUid = 1;

    // In the base game tileset 63 marks "no top part"; in the expansion it
    // is the valid X_LOC3 ICN.
    const bool sentinel63 = !src.header.expansion && !src.header.isPoLCampaign;

    for ( int y = 0; y < h; ++y ) {
        for ( int x = 0; x < w; ++x ) {
            const size_t pos = static_cast<size_t>( y ) * w + x;
            const MapCell & cell = src.tiles[pos];
            fh2::WorldData::TileOut & t = world.tiles[pos];

            t.index = static_cast<int32_t>( pos );
            t.terrainImageIndex = cell.groundIndex;
            // The original stores the terrain sprite flip (vertical /
            // horizontal / both) in the cell's flags; fheroes2 keeps it in
            // terrainFlags (bits 0-1).
            t.terrainFlags = static_cast<uint8_t>( cell.flags & 0x03 );
            t.fogColors = 0;

            std::vector<fh2::WorldData::ObjectPart> ground;
            std::vector<fh2::WorldData::ObjectPart> top;

            // Bottom part of the map cell itself (the MP2 tile equivalent).
            const uint8_t mainIcn = cell.bitfield1 >> 2;
            if ( mainIcn != fh::kIcnUnknown && cell.objectIndex != 255 )
                ground.push_back( { static_cast<uint8_t>( cell.bitfield4 & 3 ), nextUid++, mainIcn, cell.objectIndex } );

            // Top part of the map cell.
            const uint8_t topIcn = cell.bitfield6 >> 2;
            if ( topIcn != fh::kIcnUnknown && !( sentinel63 && topIcn == 63 ) && cell.overlayIndex != 255 )
                top.push_back( { fh::kLayerObject, nextUid++, topIcn, cell.overlayIndex } );

            // Add-on chain (mapCellExtra, the MP2 add-on equivalent).
            int32_t extraIdx = cell.extraIdx;
            std::set<int32_t> seen;
            while ( extraIdx > 0 && extraIdx < static_cast<int32_t>( src.cellExtras.size() ) && seen.insert( extraIdx ).second ) {
                const MapCellExtra & e = src.cellExtras[extraIdx];
                // Ground part: the ICN id is stored halved (the MP2 add-on
                // layout stores objectNameN1 * 2).
                const uint8_t gIcn = e.b1 >> 1;
                if ( gIcn != fh::kIcnUnknown && e.objectIndex != 255 )
                    ground.push_back( { static_cast<uint8_t>( e.b4 & 3 ), nextUid++, gIcn, e.objectIndex } );
                const uint8_t tIcn = e.b6 >> 2;
                if ( tIcn != fh::kIcnUnknown && !( sentinel63 && tIcn == 63 ) && e.field6 != 255 )
                    top.push_back( { fh::kLayerObject, nextUid++, tIcn, e.field6 } );
                extraIdx = e.nextIdx;
            }

            // Mirrors Maps::Tile::sortObjectParts: ground parts are stably
            // sorted by descending layer type and the last non-flag part
            // becomes the main object part.
            std::stable_sort( ground.begin(), ground.end(), []( const fh2::WorldData::ObjectPart & a, const fh2::WorldData::ObjectPart & b ) {
                return a.layer > b.layer;
            } );
            int mainPos = -1;
            for ( int i = static_cast<int>( ground.size() ) - 1; i >= 0; --i ) {
                if ( ground[i].icnType != fh::kIcnFlag32 ) {
                    mainPos = i;
                    break;
                }
            }
            if ( mainPos >= 0 ) {
                t.mainLayerType = ground[mainPos].layer;
                t.mainUid = ground[mainPos].uid;
                t.mainIcnType = ground[mainPos].icnType;
                t.mainIcnIndex = ground[mainPos].icnIndex;
                ground.erase( ground.begin() + mainPos );
            }
            else {
                t.mainLayerType = fh::kLayerObject;
                t.mainUid = 0;
                t.mainIcnType = fh::kIcnUnknown;
                t.mainIcnIndex = 255;
            }
            t.groundParts = ground;
            t.topParts = top;

            t.mainObjectType = fixObjectType( mapObjectType( cell.objType ), t.mainIcnType, t.mainIcnIndex );
            if ( t.mainObjectType == 0 && t.mainIcnType != fh::kIcnUnknown ) {
                // The original save may leave the object type unset for
                // decorative objects; recover it from the sprite (this is
                // what fheroes2 does when loading maps).
                const uint16_t byIcn = typeByIcn( t.mainIcnType, t.mainIcnIndex );
                if ( byIcn != 0 )
                    t.mainObjectType = byIcn;
            }

            // Price of Loyalty objects are stored under the generic
            // OBJ_EXPANSION_DWELLING / OBJ_EXPANSION_OBJECT types; the real
            // type is encoded in the X_LOC* sprite. fheroes2 resolves it
            // only when loading maps (updatePriceOfLoyaltyObjectType), not
            // saves, so resolve it here.
            if ( t.mainObjectType == 249 || t.mainObjectType == 250 ) {
                const auto polTypeOfPart = []( uint8_t icn, uint8_t frame ) -> uint16_t {
                    if ( icn != 61 && icn != 62 && icn != 63 ) // X_LOC1/2/3
                        return 0;
                    return typeByIcn( icn, frame );
                };
                uint16_t real = polTypeOfPart( t.mainIcnType, t.mainIcnIndex );
                if ( real == 0 ) {
                    for ( const fh2::WorldData::ObjectPart & p : t.groundParts ) {
                        real = polTypeOfPart( p.icnType, p.icnIndex );
                        if ( real != 0 )
                            break;
                    }
                }
                if ( real == 0 ) {
                    for ( const fh2::WorldData::ObjectPart & p : t.topParts ) {
                        real = polTypeOfPart( p.icnType, p.icnIndex );
                        if ( real != 0 )
                            break;
                    }
                }
                if ( real != 0 )
                    t.mainObjectType = real;
            }

            // Resource piles (OBJ_RESOURCE): the resource type is encoded in
            // the OBJNRSRC frame, the amount in the cell's extraInfo field.
            if ( t.mainObjectType == 155 && t.mainIcnType == 46 ) {
                switch ( t.mainIcnIndex ) {
                case 1:
                    t.metadata[0] = 1; // Resource::WOOD
                    break;
                case 3:
                    t.metadata[0] = 2; // Resource::MERCURY
                    break;
                case 5:
                    t.metadata[0] = 4; // Resource::ORE
                    break;
                case 7:
                    t.metadata[0] = 8; // Resource::SULFUR
                    break;
                case 9:
                    t.metadata[0] = 16; // Resource::CRYSTAL
                    break;
                case 11:
                    t.metadata[0] = 32; // Resource::GEMS
                    break;
                case 13:
                    t.metadata[0] = 64; // Resource::GOLD
                    break;
                default:
                    break;
                }
                // extraInfo (13 bits after 3 flag bits) holds the pile
                // amount; gold is counted in hundreds.
                const uint32_t extra = cell.bitfield4 >> 3;
                t.metadata[1] = ( t.metadata[0] == 64 ) ? extra * 100 : extra;
            }

            // Treasure chests: the amount of gold is 500 * extraInfo
            // (extraInfo 2/3/4 -> 1000/1500/2000 gold, matching the
            // fheroes2 "gold or experience" dialog ranges). extraInfo is
            // the 13-bit field after the 3 low flag bits of bitfield4.
            if ( t.mainObjectType == 134 ) {
                t.metadata[1] = ( cell.bitfield4 >> 3 ) * 500;
            }

            // Map monsters (OBJ_MONSTER): the stack size is stored in the
            // cell's extraInfo field; like the original engine, keep only
            // the low 8 bits. fheroes2 derives the creature type from the
            // MONS32 frame (getMonsterFromTile: icnIndex + 1) and the stack
            // size from tile.metadata()[0] (getMonsterCountFromTile).
            if ( t.mainObjectType == 152 && t.mainIcnType == 12 ) {
                t.metadata[0] = ( cell.bitfield4 >> 3 ) & 0xFF;
            }

            // Shrines and pyramids store the spell id + 1 in the cell's
            // extraInfo field; fheroes2 reads the spell id from
            // tile.metadata()[0] (getSpellFromTile).
            if ( t.mainObjectType == 159 || t.mainObjectType == 202 || t.mainObjectType == 203 || t.mainObjectType == 204 ) {
                t.metadata[0] = cell.bitfield4 >> 3;
            }

            // Barriers and traveller tents: the barrier color is the low
            // 3 bits of the cell's extraInfo field; fheroes2 reads it from
            // tile.metadata()[0] (getBarrierColorFromTile).
            if ( t.mainObjectType == 247 || t.mainObjectType == 248 ) {
                t.metadata[0] = ( cell.bitfield4 >> 3 ) & 7;
            }

            // Witch's huts: the taught secondary skill is stored in the
            // cell's extraInfo field; fheroes2 reads it from
            // tile.metadata()[0] (getSecondarySkillFromWitchsHut). The
            // original skill ids are 0-based, fheroes2's are +1.
            if ( t.mainObjectType == 213 ) {
                t.metadata[0] = ( cell.bitfield4 >> 3 ) + 1;
            }

            // Campfires (and sea barrels): the loot — a non-gold resource
            // and its amount — is packed into the cell's extraInfo field
            // as (count << 4) | resourceIdx, where resourceIdx: 0 wood,
            // 1 mercury, 2 ore, 3 sulfur, 4 crystal, 5 gems (count 4..6).
            // fheroes2 reads the resource from metadata[0] and the count
            // from metadata[1] and adds count * 100 gold (getFundsFromTile).
            if ( t.mainObjectType == 136 ) {
                const uint32_t packed = cell.bitfield4 >> 3;
                if ( ( packed & 0x0F ) <= 5 ) {
                    t.metadata[0] = 1u << ( packed & 0x0F );
                    t.metadata[1] = packed >> 4;
                }
            }

            // Road flag: fheroes2 marks a tile as a road when it contains a
            // road sprite (see Maps::Tile::isSpriteRoad).
            t.isRoad = isRoadSprite( t.mainIcnType, t.mainIcnIndex );
            if ( !t.isRoad ) {
                for ( const fh2::WorldData::ObjectPart & p : t.groundParts ) {
                    if ( isRoadSprite( p.icnType, p.icnIndex ) ) {
                        t.isRoad = true;
                        break;
                    }
                }
            }

            underHeroTypes[pos] = t.mainObjectType;
            t.passability = tileIndependentPassability( t, t.mainObjectType );
        }
    }

    // The original save does not store object UIDs, but fheroes2 relies on
    // shared UIDs for multi-tile objects (mountains, trees, castles).
    // Reconstruct them:
    //  - parts of an object use consecutive frames of the same ICN, so
    //    neighbouring parts with equal or adjacent frames are merged;
    //  - castle parts (ICNs 35..38) always form one object;
    //  - a FLAG32 part belongs to the main part of its tile;
    //  - roads, streams and the mine overlay are per-tile objects.
    std::map<uint32_t, uint32_t> parent;
    const auto findRoot = [&]( uint32_t uid ) {
        uint32_t root = uid;
        while ( parent.count( root ) && parent[root] != root )
            root = parent[root];
        while ( parent.count( uid ) && parent[uid] != uid ) {
            const uint32_t next = parent[uid];
            parent[uid] = root;
            uid = next;
        }
        return root;
    };
    const auto makeRoot = [&]( uint32_t uid ) {
        if ( !parent.count( uid ) )
            parent[uid] = uid;
        return findRoot( uid );
    };
    const auto unionUids = [&]( uint32_t a, uint32_t b ) {
        if ( a == 0 || b == 0 )
            return;
        const uint32_t ra = makeRoot( a );
        const uint32_t rb = makeRoot( b );
        if ( ra != rb )
            parent[std::max( ra, rb )] = std::min( ra, rb );
    };
    const auto isCastleIcn = []( uint8_t icn ) { return icn == 35 || icn == 36 || icn == 37 || icn == 38; };
    const auto isMountainIcn = []( uint8_t icn ) {
        return ( icn >= 22 && icn <= 27 ) || icn == 31 || icn == 32;
    };
    const auto isTreeIcn = []( uint8_t icn ) { return icn == 33 || icn == 34 || icn == 42 || icn == 43 || icn == 44 || icn == 49; };
    const auto isTileIcn = []( uint8_t icn ) {
        return icn != fh::kIcnRoad && icn != fh::kIcnStream && icn != fh::kIcnFlag32 && icn != 29 /* EXTRAOVR */;
    };

    const size_t tileCount = world.tiles.size();
    const auto forEachPart = [&]( const fh2::WorldData::TileOut & t, const std::function<void( uint32_t, uint8_t, uint8_t )> & fn ) {
        if ( t.mainIcnType != fh::kIcnUnknown )
            fn( t.mainUid, t.mainIcnType, t.mainIcnIndex );
        for ( const auto & p : t.groundParts )
            fn( p.uid, p.icnType, p.icnIndex );
        for ( const auto & p : t.topParts )
            fn( p.uid, p.icnType, p.icnIndex );
    };
    struct PartRef {
        uint32_t uid;
        uint8_t icn;
        uint8_t frame;
    };
    std::vector<std::vector<PartRef>> parts( tileCount );
    for ( size_t i = 0; i < tileCount; ++i ) {
        forEachPart( world.tiles[i], [&]( uint32_t uid, uint8_t icn, uint8_t frame ) { parts[i].push_back( { uid, icn, frame } ); } );
    }

    const auto unionTilePair = [&]( const std::vector<PartRef> & a, const std::vector<PartRef> & b, bool vertical ) {
        // Castle parts always form one object (35 = town, 36 = basement,
        // 37 = shadows, 38 = random town).
        std::vector<PartRef> acastle, bcastle;
        for ( const PartRef & p : a )
            if ( isCastleIcn( p.icn ) )
                acastle.push_back( p );
        for ( const PartRef & p : b )
            if ( isCastleIcn( p.icn ) )
                bcastle.push_back( p );
        for ( const PartRef & x : acastle )
            for ( const PartRef & y : bcastle )
                unionUids( x.uid, y.uid );

        for ( const PartRef & x : a ) {
            for ( const PartRef & y : b ) {
                if ( x.icn != y.icn || !isTileIcn( x.icn ) )
                    continue;
                // A tree's crown sits on the tile right above its trunk.
                if ( vertical && isTreeIcn( x.icn ) ) {
                    unionUids( x.uid, y.uid );
                    continue;
                }
                // Halves of a multi-tile object have consecutive frames
                // and lie side by side horizontally (e.g. a treasure chest
                // is frames 18 and 19, a gold pile is 12 and 13; verified
                // against fheroes2's map_object_info.cpp: no object has a
                // vertical pair of consecutive frames). Merging only
                // strictly ascending horizontal pairs keeps adjacent
                // single-tile objects of the same type apart: their frames
                // run N..N+1..N..N+1, so an N+1-frame followed by an
                // N-frame never merges.
                if ( !vertical && x.frame + 1 == y.frame ) {
                    unionUids( x.uid, y.uid );
                }
            }
        }
    };

    for ( int y = 0; y < h; ++y ) {
        for ( int x = 0; x < w; ++x ) {
            const size_t pos = static_cast<size_t>( y ) * w + x;
            if ( y + 1 < h )
                unionTilePair( parts[pos], parts[pos + w], true );
            if ( x + 1 < w )
                unionTilePair( parts[pos], parts[pos + 1], false );
        }
    }

    // Mountains: consecutive frames of a mountain ICN form one object even
    // when the parts are not on adjacent tiles (the MP2 editor places a
    // whole mountain as a single object).
    const uint8_t mountainIcns[] = { 22, 23, 24, 25, 26, 27, 31, 32 };
    for ( uint8_t mtnIcn : mountainIcns ) {
        std::vector<std::pair<uint8_t, uint32_t>> byFrame;
        for ( const auto & tileParts : parts ) {
            for ( const PartRef & p : tileParts ) {
                if ( p.icn == mtnIcn )
                    byFrame.push_back( { p.frame, p.uid } );
            }
        }
        std::sort( byFrame.begin(), byFrame.end() );
        for ( size_t i = 1; i < byFrame.size(); ++i ) {
            if ( byFrame[i].first <= byFrame[i - 1].first + 1 )
                unionUids( byFrame[i - 1].second, byFrame[i].second );
        }
    }

    // A flag shares the UID of the object on the same tile.
    for ( fh2::WorldData::TileOut & t : world.tiles ) {
        uint32_t flagUid = 0;
        uint32_t objectUid = 0;
        forEachPart( t, [&]( uint32_t uid, uint8_t icn, uint8_t ) {
            if ( icn == fh::kIcnFlag32 && flagUid == 0 )
                flagUid = uid;
            else if ( icn != fh::kIcnFlag32 && objectUid == 0 )
                objectUid = uid;
        } );
        if ( flagUid != 0 && objectUid != 0 )
            unionUids( flagUid, objectUid );
    }

    // Re-assign uids to the canonical root values.
    for ( fh2::WorldData::TileOut & t : world.tiles ) {
        if ( t.mainUid != 0 )
            t.mainUid = makeRoot( t.mainUid );
        for ( auto & p : t.groundParts )
            p.uid = makeRoot( p.uid );
        for ( auto & p : t.topParts )
            p.uid = makeRoot( p.uid );
    }
}

// Fills the metadata of resource-producing tiles: mines store the resource
// type in the EXTRAOVR frame (fheroes2 recovers it the same way when
// loading maps). Income per day mirrors ProfitConditions::FromMine.
void fillResourceMetadata( fh2::WorldData & world )
{
    for ( fh2::WorldData::TileOut & t : world.tiles ) {
        switch ( t.mainObjectType ) {
        case 151: { // OBJ_MINE
            uint8_t frame = 255;
            if ( t.mainIcnType == 29 )
                frame = t.mainIcnIndex;
            else {
                for ( const auto & p : t.groundParts ) {
                    if ( p.icnType == 29 ) {
                        frame = p.icnIndex;
                        break;
                    }
                }
            }
            switch ( frame ) {
            case 0:
                t.metadata[0] = 4; // Resource::ORE
                t.metadata[1] = 2;
                break;
            case 1:
                t.metadata[0] = 8; // Resource::SULFUR
                t.metadata[1] = 1;
                break;
            case 2:
                t.metadata[0] = 16; // Resource::CRYSTAL
                t.metadata[1] = 1;
                break;
            case 3:
                t.metadata[0] = 32; // Resource::GEMS
                t.metadata[1] = 1;
                break;
            case 4:
                t.metadata[0] = 64; // Resource::GOLD
                t.metadata[1] = 1000;
                break;
            default:
                break;
            }
            break;
        }
        case 157: // OBJ_SAWMILL
            t.metadata[0] = 1; // Resource::WOOD
            t.metadata[1] = 2;
            break;
        case 129: // OBJ_ALCHEMIST_LAB
            t.metadata[0] = 2; // Resource::MERCURY
            t.metadata[1] = 1;
            break;
        case 169: { // OBJ_ARTIFACT: the artifact id comes from the OBJNARTI frame
            uint8_t frame = 255;
            if ( t.mainIcnType == 11 )
                frame = t.mainIcnIndex;
            else {
                for ( const auto & p : t.groundParts ) {
                    if ( p.icnType == 11 ) {
                        frame = p.icnIndex;
                        break;
                    }
                }
            }
            // Artifact::getArtifactFromMapSpriteIndex: id = (frame - 1) / 2 + 1.
            if ( frame != 255 && frame < 162 )
                t.metadata[0] = ( static_cast<uint32_t>( frame ) - 1 ) / 2 + 1;
            else if ( frame != 255 )
                t.metadata[0] = 1; // random-level frames are not present in saves; fall back
            break;
        }
        default:
            break;
        }
    }
}

// Second pass of Maps::World::updatePassabilities: adjusts the initial
// per-tile passability based on neighbouring tiles.
void finalizePassability( const fh2::WorldData & world, const std::vector<uint16_t> & underHeroTypes, std::vector<uint16_t> & passability )
{
    const int w = world.width;
    const int h = world.height;
    const auto & tiles = world.tiles;

    const auto isWaterTile = []( const fh2::WorldData::TileOut & t ) { return t.terrainImageIndex < 30; };

    const auto doesObjectExist = [&]( const fh2::WorldData::TileOut & t, uint32_t uid ) {
        if ( t.mainUid == uid && !isTransparentLayer( t.mainLayerType ) )
            return true;
        for ( const auto & part : t.groundParts ) {
            if ( part.uid == uid && !isTransparentLayer( part.layer ) )
                return true;
        }
        return false;
    };

    const auto isAnyTallObject = [&]( const fh2::WorldData::TileOut & t, int pos ) {
        if ( pos < w )
            return false; // first row: cannot be tall
        std::vector<uint32_t> uids;
        if ( t.mainIcnType != fh::kIcnUnknown && !isTransparentLayer( t.mainLayerType ) )
            uids.push_back( t.mainUid );
        for ( const auto & part : t.groundParts ) {
            if ( !isTransparentLayer( part.layer ) )
                uids.push_back( part.uid );
        }
        const fh2::WorldData::TileOut & topTile = tiles[pos - w];
        for ( uint32_t uid : uids ) {
            for ( const auto & part : topTile.topParts ) {
                if ( part.uid == uid )
                    return true;
            }
        }
        return false;
    };

    const auto isShadowTile = []( const fh2::WorldData::TileOut & t ) {
        if ( !isShadowSprite( t.mainIcnType, t.mainIcnIndex ) )
            return false;
        for ( const auto & part : t.groundParts ) {
            if ( !isShadowSprite( part.icnType, part.icnIndex ) )
                return false;
        }
        return true;
    };

    const auto getIndexOfMainTile = [&]( int pos, uint16_t objectType, uint16_t correctedType ) {
        if ( correctedType == objectType )
            return pos;
        // uids of the current tile
        std::set<uint32_t> uids;
        if ( tiles[pos].mainUid != 0 )
            uids.insert( tiles[pos].mainUid );
        for ( const auto & part : tiles[pos].groundParts )
            uids.insert( part.uid );
        for ( const auto & part : tiles[pos].topParts )
            uids.insert( part.uid );

        const int tx = pos % w;
        const int ty = pos / w;
        for ( int dy = 3; dy >= -1; --dy ) {
            const int ny = ty + dy;
            if ( ny < 0 || ny >= h )
                continue;
            for ( int dx = -3; dx <= 3; ++dx ) {
                const int nx = tx + dx;
                if ( nx < 0 || nx >= w )
                    continue;
                const int index = ny * w + nx;
                const fh2::WorldData::TileOut & found = tiles[index];
                if ( found.mainObjectType != correctedType )
                    continue;
                if ( found.mainUid != 0 && uids.count( found.mainUid ) > 0 )
                    return index;
            }
        }
        return -1;
    };

    const auto isDetached = [&]( int pos, uint16_t objectType ) {
        if ( isDetachedObjectType( objectType ) )
            return true;
        const uint16_t correctedType = baseActionObjectType( objectType );
        if ( !isDetachedObjectType( correctedType ) )
            return false;
        const int mainIdx = getIndexOfMainTile( pos, objectType, correctedType );
        if ( mainIdx == -1 )
            return false;
        const uint32_t objectUid = tiles[mainIdx].mainUid;
        if ( tiles[pos].mainUid == objectUid )
            return !isTransparentLayer( tiles[pos].mainLayerType );
        for ( const auto & part : tiles[pos].groundParts ) {
            if ( part.uid == objectUid )
                return !isTransparentLayer( part.layer );
        }
        return false;
    };

    for ( int pos = 0; pos < w * h; ++pos ) {
        uint16_t p = passability[pos];
        if ( p == 0 )
            continue;
        const fh2::WorldData::TileOut & t = tiles[pos];
        const int tx = pos % w;
        const int ty = pos / w;

        if ( ( p & 0x0001 ) && tx > 0 ) { // TOP_LEFT
            const fh2::WorldData::TileOut & left = tiles[pos - 1];
            if ( isAnyTallObject( left, pos - 1 ) && ( passability[pos - 1] & 0x0002 ) == 0 )
                p &= ~0x0001;
        }
        if ( ( p & 0x0004 ) && tx < w - 1 ) { // TOP_RIGHT
            const fh2::WorldData::TileOut & right = tiles[pos + 1];
            if ( isAnyTallObject( right, pos + 1 ) && ( passability[pos + 1] & 0x0002 ) == 0 )
                p &= ~0x0004;
        }
        passability[pos] = p;

        const uint16_t objectType = underHeroTypes[pos];
        if ( objectType & 128 )
            continue;
        if ( isShadowTile( t ) )
            continue;
        if ( t.mainIcnType == fh::kIcnUnknown )
            continue;
        if ( isTransparentLayer( t.mainLayerType ) )
            continue;

        if ( ty >= h - 1 ) {
            p = 0;
            passability[pos] = p;
            continue;
        }
        const fh2::WorldData::TileOut & bottom = tiles[pos + w];
        if ( !isWaterTile( t ) && isWaterTile( bottom ) ) {
            p = 0;
            passability[pos] = p;
            continue;
        }

        std::vector<uint32_t> uids;
        uids.push_back( t.mainUid );
        for ( const auto & part : t.groundParts ) {
            if ( !isTransparentLayer( part.layer ) )
                uids.push_back( part.uid );
        }
        bool blocked = false;
        for ( uint32_t uid : uids ) {
            if ( doesObjectExist( bottom, uid ) ) {
                blocked = true;
                break;
            }
        }
        if ( blocked ) {
            p = 0;
            passability[pos] = p;
            continue;
        }

        const uint16_t bottomType = underHeroTypes[pos + w];
        if ( ( bottomType & 128 ) && ( passability[pos + w] & 0x0002 ) != 0 )
            continue;

        int validBottomLayerObjects = 0;
        for ( const auto & part : t.groundParts ) {
            if ( isShadowSprite( part.icnType, part.icnIndex ) )
                continue;
            if ( part.icnType != fh::kIcnRoad && part.icnType != fh::kIcnStream && part.icnType != fh::kIcnFlag32 )
                ++validBottomLayerObjects;
        }
        const bool singleObjectTile
            = ( validBottomLayerObjects == 0 ) && t.topParts.empty() && ( bottom.mainIcnType != t.mainIcnType );

        if ( !singleObjectTile && !isDetached( pos, objectType ) && bottom.mainIcnType != fh::kIcnUnknown
             && !isTransparentLayer( bottom.mainLayerType ) ) {
            const uint16_t correctedType = baseActionObjectType( bottomType );
            if ( ( bottomType & 128 ) || ( correctedType & 128 ) ) {
                if ( !isShortObject( bottomType ) && !isShortObject( correctedType ) )
                    p = 0;
                passability[pos] = p;
                continue;
            }

            // Valid icn types of the current tile.
            std::set<uint8_t> validIcns;
            if ( t.mainIcnType != fh::kIcnUnknown )
                validIcns.insert( t.mainIcnType );
            for ( const auto & part : t.groundParts )
                validIcns.insert( part.icnType );
            for ( const auto & part : t.topParts )
                validIcns.insert( part.icnType );

            bool bottomContainsValidIcn = false;
            if ( validIcns.count( bottom.mainIcnType ) > 0 )
                bottomContainsValidIcn = true;
            if ( !bottomContainsValidIcn ) {
                for ( const auto & part : bottom.groundParts ) {
                    if ( validIcns.count( part.icnType ) > 0 ) {
                        bottomContainsValidIcn = true;
                        break;
                    }
                }
            }
            if ( !bottomContainsValidIcn ) {
                for ( const auto & part : bottom.topParts ) {
                    if ( validIcns.count( part.icnType ) > 0 ) {
                        bottomContainsValidIcn = true;
                        break;
                    }
                }
            }

            if ( !( isShortObject( bottomType )
                    || ( !bottomContainsValidIcn && ( isCombinedObject( objectType ) || isCombinedObject( bottomType ) ) ) ) ) {
                p = 0;
            }
        }

        passability[pos] = p;
    }
}

} // namespace

bool convert( const Save & src, fh2::WorldData & world, fh2::ConvertOptions & options )
{
    const Header & h = src.header;
    world = fh2::WorldData();
    const uint16_t requestedVersion = options.formatVersion;
    options = fh2::ConvertOptions();
    options.formatVersion = requestedVersion;
    options.polResources = h.expansion || h.isPoLCampaign;
    options.campaign = h.isCampaign;

    world.width = h.mapWidth;
    world.height = h.mapHeight;
    world.day = static_cast<uint32_t>( h.day );
    world.week = static_cast<uint32_t>( h.week );
    world.month = static_cast<uint32_t>( h.month );
    world.gameType = options.campaign ? 2 : 1;
    world.seed = 0;

    // Difficulty: original raw value -> fheroes2 enum (0..4).
    // Original: 0 = Easy, 100 = Normal, 200 = Hard, 300 = Expert, 400 = Impossible.
    const int32_t rawDiff = h.difficulty;
    if ( rawDiff <= 0 )
        world.gameDifficulty = 0;
    else if ( rawDiff <= 100 )
        world.gameDifficulty = 1;
    else if ( rawDiff <= 200 )
        world.gameDifficulty = 2;
    else if ( rawDiff <= 300 )
        world.gameDifficulty = 3;
    else
        world.gameDifficulty = 4;

    // Player colors: the source stores an index per player; the kingdom
    // color is the bitmask. "Alive" = not in playerDead; "human" =
    // playerAlive (the original's gbHumanPlayer flag).
    std::vector<uint8_t> playerColorBits( 6, 0 );
    std::set<int32_t> aliveColors;
    for ( int i = 0; i < 6; ++i ) {
        if ( i < static_cast<int>( src.players.size() ) ) {
            playerColorBits[i] = mapColor( src.players[i].color );
            if ( !h.playerDead[i] )
                aliveColors.insert( playerColorBits[i] );
        }
    }

    // Kingdoms: fheroes2 always serializes 7 records (std::array<Kingdom, 7>):
    // slots 0..5 = BLUE..PURPLE (index = the original color index), slot 6 =
    // the neutral kingdom. Only alive players get real data.
    std::map<uint8_t, int> colorToKingdom;
    world.kingdoms.clear();
    world.kingdoms.resize( 7 );
    for ( int i = 0; i < 6; ++i ) {
        const int slot = src.players[i].color; // 0..5, matches Color::GetIndex
        if ( slot < 0 || slot > 5 )
            continue;
        fh2::WorldData::KingdomOut & k = world.kingdoms[slot];
        if ( h.playerDead[i] ) {
            k.color = 0; // empty kingdom record
            continue;
        }
        k.color = playerColorBits[i];
        k.modes = 0;
        // Must be non-zero: on a new day fheroes2 eliminates a kingdom when
        // lost_town_days reaches 0 (Kingdom::ActionNewDay), so 0 would make
        // every player lose on the first day.
        k.lostTownDays = 8;
        k.visitedTentsColors = 0;
        k.topCastleInKingdomView = -1;
        k.topHeroInKingdomView = -1;
        colorToKingdom[playerColorBits[i]] = slot;
    }
    world.kingdoms[6].color = 0; // neutral

    // Resources.
    for ( int i = 0; i < 6; ++i ) {
        const Player & pl = src.players[i];
        auto it = colorToKingdom.find( playerColorBits[i] );
        if ( it == colorToKingdom.end() )
            continue;
        fh2::WorldData::KingdomOut & k = world.kingdoms[it->second];
        for ( int r = 0; r < 7; ++r )
            k.resources[r] = static_cast<int32_t>( pl.resources[r] );
    }

    // Castles.
    world.castles.clear();
    std::vector<int32_t> castleIndexBySrc( src.castles.size(), -1 );
    for ( size_t ci = 0; ci < src.castles.size(); ++ci ) {
        const Castle & c = src.castles[ci];
        if ( !c.exists )
            continue;
        fh2::WorldData::CastleOut out;
        uint8_t cbit = ( c.ownerIdx >= 0 && c.ownerIdx < 6 ) ? playerColorBits[c.ownerIdx] : 0;
        convertCastle( c, out, cbit );
        int32_t newIdx = static_cast<int32_t>( world.castles.size() );
        world.castles.push_back( out );
        castleIndexBySrc[ci] = newIdx;

        auto it = colorToKingdom.find( cbit );
        if ( it != colorToKingdom.end() )
            world.kingdoms[it->second].castleIndices.push_back( static_cast<int32_t>( c.y ) * h.mapWidth + c.x );
    }

    // Heroes: fheroes2 indexes AllHeroes by hero id (HEROES_COUNT = 73,
    // entry 0 is UNKNOWN). Place each converted hero at index = its id.
    world.heroes.clear();
    world.heroes.resize( 73 );
    for ( const Hero & hero : src.heroes ) {
        const int32_t id = static_cast<int32_t>( hero.heroID ) + 1;
        if ( id < 1 || id >= 73 )
            continue;
        uint8_t cbit = ( hero.ownerIdx >= 0 && hero.ownerIdx < 6 ) ? playerColorBits[hero.ownerIdx] : 0;
        convertHero( hero, world.heroes[id], cbit );

        if ( hero.ownerIdx >= 0 && hero.ownerIdx < 6 ) {
            auto it = colorToKingdom.find( cbit );
            if ( it != colorToKingdom.end() )
                world.kingdoms[it->second].heroIndices.push_back( id );
        }
    }
    for ( fh2::WorldData::HeroOut & out : world.heroes ) {
        if ( out.name.empty() && out.id == 0 && out.centerX == 0 && out.centerY == 0 ) {
            out.centerX = -1;
            out.centerY = -1;
        }
    }

    // Heroes owned by players (kingdom hero lists are built from
    // playerData.heroesOwned as the authoritative order).
    for ( int i = 0; i < 6; ++i ) {
        const Player & pl = src.players[i];
        auto it = colorToKingdom.find( playerColorBits[i] );
        if ( it == colorToKingdom.end() )
            continue;
        fh2::WorldData::KingdomOut & k = world.kingdoms[it->second];
        std::vector<int32_t> ordered;
        for ( int8_t hIdx : pl.heroesOwned ) {
            if ( hIdx >= 0 && hIdx < 54 )
                ordered.push_back( static_cast<int32_t>( hIdx ) + 1 );
        }
        if ( !ordered.empty() )
            k.heroIndices = ordered;
    }

    // Tiles: groundIndex == fheroes2 terrainImageIndex (1:1). Object parts
    // and object types are mapped from the mapCell / cellExtras data (see
    // convertTiles above and docs/CONVERSION.md).
    world.tiles.resize( static_cast<size_t>( h.mapWidth ) * h.mapHeight );
    std::vector<uint16_t> underHeroTypes( world.tiles.size(), 0 );
    convertTiles( src, world, underHeroTypes );

    // Mark castle tiles: kingdom castle lists reference the tile index, and
    // fheroes2 validates it by the tile's object type (MP2::OBJ_CASTLE = 163).
    for ( const Castle & c : src.castles ) {
        if ( !c.exists || c.x >= h.mapWidth || c.y >= h.mapHeight )
            continue;
        fh2::WorldData::TileOut & t = world.tiles[static_cast<size_t>( c.y ) * h.mapWidth + c.x];
        t.mainObjectType = 163; // MP2::OBJ_CASTLE
    }

    // Heroes on the map: the tile of a hero is marked as OBJ_HERO (183)
    // with the hero id; the type under the hero is remembered separately
    // (it is also stored in the hero record as objectTypeUnderHero).
    for ( const Hero & hero : src.heroes ) {
        if ( hero.ownerIdx < 0 || hero.x < 0 || hero.x >= h.mapWidth || hero.y < 0 || hero.y >= h.mapHeight )
            continue;
        const int32_t id = static_cast<int32_t>( hero.heroID ) + 1;
        if ( id < 1 || id >= 73 )
            continue;
        fh2::WorldData::TileOut & t = world.tiles[static_cast<size_t>( hero.y ) * h.mapWidth + hero.x];
        if ( t.mainObjectType == 183 )
            continue;
        underHeroTypes[static_cast<size_t>( hero.y ) * h.mapWidth + hero.x] = t.mainObjectType;
        t.mainObjectType = 183; // OBJ_HERO
        t.occupantHeroId = static_cast<uint8_t>( id );
    }

    // Final passability (neighbour-based adjustments), mirrored from
    // Maps::World::updatePassabilities.
    {
        std::vector<uint16_t> passability( world.tiles.size(), 0 );
        for ( size_t i = 0; i < world.tiles.size(); ++i )
            passability[i] = world.tiles[i].passability;
        finalizePassability( world, underHeroTypes, passability );
        for ( size_t i = 0; i < world.tiles.size(); ++i )
            world.tiles[i].passability = passability[i];
    }

    // Resource metadata for mines / sawmills / alchemist labs.
    fillResourceMetadata( world );

    // Fog of war. The original mapRevealed byte marks visibility by PLAYER
    // INDEX (bit i = player i sees the tile), while fheroes2 stores a
    // bitmask of COLORS that do NOT see the tile (PlayerColor values).
    uint8_t aliveMask = 0;
    for ( uint8_t cbit : aliveColors )
        aliveMask |= cbit;
    for ( size_t i = 0; i < world.tiles.size() && i < src.mapRevealed.size(); ++i ) {
        uint8_t visibleColors = 0;
        for ( int p = 0; p < 6; ++p ) {
            if ( src.mapRevealed[i] & ( 1u << p ) )
                visibleColors |= playerColorBits[p];
        }
        world.tiles[i].fogColors = static_cast<uint8_t>( aliveMask & ~visibleColors & 0x3F );
    }

    // Boats.
    for ( const Boat & b : src.boats ) {
        if ( b.owner == 0xFF || b.x >= h.mapWidth || b.y >= h.mapHeight )
            continue;
        fh2::WorldData::TileOut & t = world.tiles[static_cast<size_t>( b.y ) * h.mapWidth + b.x];
        t.boatOwnerColor = ( b.owner < 6 ) ? playerColorBits[b.owner] : 0;
    }

    // Captured objects: fheroes2 stores one entry per capturable tile
    // (castles, mines, sawmills, alchemist labs, lighthouses, dragon
    // cities, abandoned mines). The owner and guardians come from the
    // original mines table (only captured mines have entries there).
    {
        const auto ownerOfMine = [&]( int x, int y ) -> int {
            for ( const Mine & m : src.mines ) {
                if ( m.owner != 0xFF && m.x == x && m.y == y )
                    return m.owner;
            }
            return -1;
        };
        const auto guardiansOfMine = [&]( int x, int y ) {
            std::vector<std::pair<int32_t, uint32_t>> guardians;
            for ( const Mine & m : src.mines ) {
                if ( m.owner != 0xFF && m.x == x && m.y == y && m.guardianType != 0xFF ) {
                    guardians.push_back( { mapCreature( static_cast<int8_t>( m.guardianType ) ), m.guardianQty } );
                }
            }
            return guardians;
        };

        for ( size_t i = 0; i < world.tiles.size(); ++i ) {
            const uint16_t ot = world.tiles[i].mainObjectType;
            const bool capturable = ( ot == 163 || ot == 35 /* castle */ ) || ot == 151 /* mine */ || ot == 157 /* sawmill */
                                    || ot == 129 /* alchemist lab */ || ot == 149 /* lighthouse */ || ot == 148 /* dragon city */
                                    || ot == 192 /* abandoned mine */;
            if ( !capturable )
                continue;

            const int tx = static_cast<int>( i ) % h.mapWidth;
            const int ty = static_cast<int>( i ) / h.mapWidth;
            const int owner = ownerOfMine( tx, ty );
            const uint8_t colorBit = ( owner >= 0 && owner < 6 ) ? playerColorBits[owner] : 0;
            world.capturedObjects.push_back( { static_cast<int32_t>( i ), ot, colorBit, guardiansOfMine( tx, ty ) } );
        }
    }

    // Obelisks: the original stores a visited mask per obelisk (48 bytes,
    // one bit per color). fheroes2 tracks them via Kingdom::visitedObjects.
    {
        std::vector<int32_t> obeliskTiles;
        for ( size_t i = 0; i < world.tiles.size(); ++i ) {
            if ( world.tiles[i].mainObjectType == 153 ) // OBJ_OBELISK
                obeliskTiles.push_back( static_cast<int32_t>( i ) );
        }
        for ( size_t k = 0; k < obeliskTiles.size() && k < src.obeliskVisitedMasks.size(); ++k ) {
            const uint8_t mask = src.obeliskVisitedMasks[k];
            for ( int i = 0; i < 6; ++i ) {
                if ( !( mask & playerColorBits[i] ) )
                    continue;
                auto it = colorToKingdom.find( playerColorBits[i] );
                if ( it != colorToKingdom.end() )
                    world.kingdoms[it->second].visitedObjects.push_back( { obeliskTiles[k], 153 } );
            }
        }
    }

    // Ultimate artifact location. The original stores the artifact id
    // (0 = Ultimate Book of Knowledge, ..., 6 = Ultimate Crown, and in PoL
    // also non-ultimate artifacts such as the Golden Goose); fheroes2
    // artifact ids are the original ones + 1.
    world.ultimateArtifactId = static_cast<int32_t>( src.ultimateArtifactIdx ) + 1;
    world.ultimateArtifactExt = 0;
    world.ultimateArtifactIndex
        = ( src.ultimateArtifactX >= 0 && src.ultimateArtifactY >= 0 )
              ? static_cast<int32_t>( src.ultimateArtifactY ) * h.mapWidth + src.ultimateArtifactX
              : -1;
    world.ultimateArtifactFound = false;
    world.ultimateArtifactOffsetX = 0;
    world.ultimateArtifactOffsetY = 0;

    // Players (Settings section).
    world.playersColors = 0;
    for ( uint8_t cbit : aliveColors )
        world.playersColors |= cbit;
    world.currentPlayerColor = 0;
    for ( int i = 0; i < 6; ++i ) {
        if ( !h.playerDead[i] ) {
            world.currentPlayerColor = playerColorBits[i];
            break;
        }
    }
    world.players.clear();
    for ( uint8_t cbit : aliveColors ) {
        // Find the source player index for this color bit.
        int srcIdx = -1;
        for ( int i = 0; i < 6; ++i ) {
            if ( playerColorBits[i] == cbit ) {
                srcIdx = i;
                break;
            }
        }
        fh2::WorldData::PlayerOut p;
        p.color = cbit;
        p.modes = 0x2000; // ST_INGAME (otherwise the player is considered eliminated)
        p.control = 1; // CONTROL_HUMAN (the human flag is set below)
        p.race = 0;
        p.name = ( srcIdx >= 0 ) ? src.header.playerNames[srcIdx] : "";
        // fheroes2 renders fog by FriendsColors(): with an empty mask every
        // tile is treated as fully covered by fog and the terrain is not
        // drawn at all. Each player sees at least their own color.
        p.friendsColors = cbit;
        p.focusType = 0;
        p.focusIndex = -1;
        p.aiPersonality = 0;
        p.handicapStatus = 0;
        world.players.push_back( p );
    }
    // Control flags: the original marks human players in playerAlive
    // (gbHumanPlayer); the rest are AI. The game area camera is restored
    // from the player's focus (FOCUS_HEROES + the hero's tile index).
    for ( size_t i = 0; i < world.players.size(); ++i ) {
        int srcIdx = -1;
        for ( int j = 0; j < 6; ++j ) {
            if ( playerColorBits[j] == world.players[i].color ) {
                srcIdx = j;
                break;
            }
        }
        world.players[i].control = ( srcIdx >= 0 && h.playerAlive[srcIdx] ) ? 1 : 4;
        if ( srcIdx >= 0 ) {
            world.players[i].race = mapRace( h.playerFactions[srcIdx] );

            int heroIdx = src.players[srcIdx].curHeroIdx;
            if ( heroIdx < 0 || heroIdx >= kNumHeroes ) {
                for ( int8_t hIdx : src.players[srcIdx].heroesOwned ) {
                    if ( hIdx >= 0 && hIdx < kNumHeroes ) {
                        heroIdx = hIdx;
                        break;
                    }
                }
            }
            if ( heroIdx >= 0 && heroIdx < kNumHeroes ) {
                const Hero & hero = src.heroes[heroIdx];
                if ( hero.ownerIdx == srcIdx && hero.x >= 0 && hero.x < h.mapWidth && hero.y >= 0 && hero.y < h.mapHeight ) {
                    world.players[i].focusType = 1; // FOCUS_HEROES
                    world.players[i].focusIndex = hero.y * h.mapWidth + hero.x;
                }
            }
        }
    }

    // GameOver: the color bitmask of active players (empty mask means
    // everyone is eliminated).
    world.gameOverColors = world.playersColors;

    // Campaign options.
    if ( options.campaign ) {
        options.campaignId = h.isPoLCampaign ? 2 : 0;
        options.scenarioId = 0;
        options.difficulty = 0;
        options.minDifficulty = 0;
    }

    return true;
}

} // namespace h2
