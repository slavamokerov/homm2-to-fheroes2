// Command line interface: convert original HoMM2 saves to fheroes2 saves.
//
// Usage: homm2-to-fheroes2 [--format 10032|10033|10034]
//                          <input.GM1|GMC|GXC> [output.sav|savc] [more inputs...]

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "convert.h"
#include "fheroes2_save.h"
#include "homm2_save.h"

namespace {

uint16_t g_formatVersion = 10033;

std::vector<uint8_t> readFile( const std::string & path )
{
    std::ifstream f( path, std::ios::binary );
    if ( !f )
        return {};
    return std::vector<uint8_t>( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
}

bool writeFile( const std::string & path, const std::vector<uint8_t> & data )
{
    std::ofstream f( path, std::ios::binary );
    if ( !f )
        return false;
    f.write( reinterpret_cast<const char *>( data.data() ), static_cast<std::streamsize>( data.size() ) );
    return static_cast<bool>( f );
}

std::string defaultOutputName( const std::string & input, bool campaign )
{
    std::string base = input;
    const auto pos = base.find_last_of( ".GM" );
    if ( pos != std::string::npos ) {
        const std::string ext = base.substr( pos );
        if ( ext == ".GM1" || ext == ".GM2" || ext == ".GMC" || ext == ".GXC" || ext == ".GX1" || ext == ".GX2" )
            base = base.substr( 0, pos );
    }
    return base + ( campaign ? ".savc" : ".sav" );
}

int convertFile( const std::string & input, const std::string & output )
{
    const std::vector<uint8_t> data = readFile( input );
    if ( data.empty() ) {
        std::cerr << "cannot read " << input << std::endl;
        return 1;
    }

    h2::Save save;
    if ( !h2::parseSave( data, save ) ) {
        std::cerr << input << ": not a supported Heroes of Might and Magic II save" << std::endl;
        return 1;
    }

    h2conv::WorldData world;
    h2conv::ConvertOptions options;
    options.formatVersion = g_formatVersion;
    if ( !h2::convert( save, world, options ) ) {
        std::cerr << input << ": conversion failed" << std::endl;
        return 1;
    }

    const std::vector<uint8_t> result = h2conv::buildSaveFile( save.header, world, options );
    if ( result.empty() ) {
        std::cerr << input << ": failed to build the fheroes2 save" << std::endl;
        return 1;
    }

    if ( !writeFile( output, result ) ) {
        std::cerr << "cannot write " << output << std::endl;
        return 1;
    }

    std::cout << input << " -> " << output << " ("
              << save.header.mapWidth << 'x' << save.header.mapHeight << ", day "
              << save.header.day << ", " << static_cast<int>( save.header.numPlayers ) << " players, "
              << ( options.campaign ? "campaign" : "standard" ) << ')' << std::endl;
    return 0;
}

} // namespace

int main( int argc, char ** argv )
{
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    auto isInputName = []( const std::string & s ) {
        if ( s.size() < 4 )
            return false;
        const std::string ext = s.substr( s.size() - 4 );
        return ext == ".GM1" || ext == ".GM2" || ext == ".GMC" || ext == ".GXC" || ext == ".GX1" || ext == ".GX2";
    };
    for ( int i = 1; i < argc; ++i ) {
        const std::string arg = argv[i];
        if ( arg == "-o" && i + 1 < argc ) {
            outputs.push_back( argv[++i] );
        }
        else if ( arg == "--format" && i + 1 < argc ) {
            const int v = std::atoi( argv[++i] );
            if ( v != 10032 && v != 10033 && v != 10034 ) {
                std::cerr << "unsupported format " << argv[i] << " (use 10032, 10033 or 10034)" << std::endl;
                return 1;
            }
            g_formatVersion = static_cast<uint16_t>( v );
        }
        else if ( isInputName( arg ) ) {
            inputs.push_back( arg );
        }
        else {
            outputs.push_back( arg );
        }
    }

    if ( inputs.empty() ) {
        std::cout << "usage: homm2-to-fheroes2 [--format 10032|10033|10034] <input.GM1|GMC|GXC> [output.sav|savc] [more inputs...]" << std::endl;
        return 1;
    }

    int rc = 0;
    for ( size_t i = 0; i < inputs.size(); ++i ) {
        const std::string input = inputs[i];
        // Detect campaign from the extension before parsing, for the output
        // name; the parse will confirm it.
        const bool looksLikeCampaign = input.size() >= 4
                                       && ( input.compare( input.size() - 4, 4, ".GMC" ) == 0 || input.compare( input.size() - 4, 4, ".GXC" ) == 0 );
        std::string output;
        if ( i < outputs.size() )
            output = outputs[i];
        else
            output = defaultOutputName( input, looksLikeCampaign );
        if ( convertFile( input, output ) != 0 )
            rc = 1;
    }
    return rc;
}
