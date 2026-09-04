// Conversion from the original HoMM2 save to the fheroes2 world model.
// ID mapping: fheroes2 enums reserve 0 for UNKNOWN/NONE, so most original
// ids are shifted by +1; races and colors become bitmasks.

#pragma once

#include "fheroes2_save.h"
#include "homm2_save.h"

namespace h2 {

// Converts a parsed original save into the fheroes2 world and the save
// options (format version, campaign fields). Returns false if the input
// cannot be converted (e.g. an unsupported variant).
bool convert( const Save & src, fh2::WorldData & world, fh2::ConvertOptions & options );

// Utility: original creature id -> fheroes2 monster id.
int32_t mapCreature( int8_t id );

// Utility: original faction (0..5) -> fheroes2 race bitmask.
int32_t mapRace( uint8_t faction );

// Utility: original color index (0..5) -> fheroes2 color bitmask.
uint8_t mapColor( uint8_t color );

// Utility: original building bitmask -> fheroes2 building bitmask.
uint32_t mapBuildings( uint32_t homm2Flags, uint8_t mageGuildLevel );

} // namespace h2
