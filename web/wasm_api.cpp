// WebAssembly API for the browser version of homm2-to-fheroes2
// (GitHub Pages). Embind wrapper around the Qt-free converter core.
// A save comes in as bytes (Uint8Array), the converted fheroes2 save goes
// out as bytes. No filesystem, no Qt.
//
// Comments are in English (project convention).

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "convert.h"
#include "fheroes2_save.h"
#include "homm2_save.h"

namespace {

std::string jsonString( const std::string & s )
{
    std::string out = "\"";
    for ( unsigned char c : s ) {
        switch ( c ) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if ( c < 0x20 )
                out += '?';
            else
                out += static_cast<char>( c );
            break;
        }
    }
    out += '"';
    return out;
}

std::string jsonInt( int64_t v )
{
    return std::to_string( v );
}

std::vector<uint8_t> valToBytes( const emscripten::val & v )
{
    const unsigned len = v["byteLength"].as<unsigned>();
    std::vector<uint8_t> out( len );
    emscripten::val view = emscripten::val( emscripten::typed_memory_view( len, out.data() ) );
    view.call<void>( "set", v );
    return out;
}

emscripten::val bytesToVal( const std::vector<uint8_t> & bytes )
{
    emscripten::val arr = emscripten::val::global( "Uint8Array" ).new_( bytes.size() );
    emscripten::val view = emscripten::val( emscripten::typed_memory_view( bytes.size(), bytes.data() ) );
    arr.call<void>( "set", view );
    return arr;
}

std::string lowercase( std::string s )
{
    std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
    return s;
}

std::string outputExtension( const std::string & name )
{
    const std::string n = lowercase( name );
    if ( n.size() >= 4 && ( n.compare( n.size() - 4, 4, ".gmc" ) == 0 || n.compare( n.size() - 4, 4, ".gxc" ) == 0 ) )
        return ".savc";
    return ".sav";
}

void appendUtf8( std::string & out, uint32_t code )
{
    if ( code < 0x80 ) {
        out.push_back( static_cast<char>( code ) );
    }
    else if ( code < 0x800 ) {
        out.push_back( static_cast<char>( 0xC0 | ( code >> 6 ) ) );
        out.push_back( static_cast<char>( 0x80 | ( code & 0x3F ) ) );
    }
    else {
        out.push_back( static_cast<char>( 0xE0 | ( code >> 12 ) ) );
        out.push_back( static_cast<char>( 0x80 | ( ( code >> 6 ) & 0x3F ) ) );
        out.push_back( static_cast<char>( 0x80 | ( code & 0x3F ) ) );
    }
}

// The original game is a DOS program: its strings are in code page 866.
// Convert them to UTF-8 for display; characters without a mapping become '?'.
std::string cp866ToUtf8( const std::string & s )
{
    std::string out;
    for ( unsigned char c : s ) {
        if ( c < 0x80 ) {
            out.push_back( static_cast<char>( c ) );
            continue;
        }
        uint32_t code = 0;
        if ( c >= 0x80 && c <= 0xAF ) {
            code = 0x0410 + ( c - 0x80 );          // А..п
        }
        else if ( c >= 0xE0 && c <= 0xEF ) {
            code = 0x0440 + ( c - 0xE0 );          // р..я
        }
        else {
            switch ( c ) {
            case 0xF0: code = 0x0401; break;       // Ё
            case 0xF1: code = 0x0451; break;       // ё
            case 0xFC: code = 0x2116; break;       // №
            case 0xF8: code = 0x00B0; break;       // °
            case 0xFA: code = 0x00B7; break;       // ·
            case 0xFF: code = 0x00A0; break;       // nbsp
            default:
                break;                             // box drawing etc. -> '?'
            }
        }
        if ( code == 0 )
            out.push_back( '?' );
        else
            appendUtf8( out, code );
    }
    return out;
}

// Converts one original save. Returns a JSON object:
//   { "ok": true,  "name": ..., "mapName": ..., "day": ..., "outExt": ... }
//   { "ok": false, "name": ..., "error": ... }
// The result bytes are returned separately by takeLastResult().
std::vector<uint8_t> g_lastResult;

std::string convertSave( const std::string & name, const emscripten::val & data, int version )
{
    try {
        if ( version != 10032 && version != 10033 && version != 10034 ) {
            version = 10033;
        }

        const std::vector<uint8_t> bytes = valToBytes( data );
        if ( bytes.empty() ) {
            return "{\"ok\":false,\"name\":" + jsonString( name ) + ",\"error\":\"The file is empty.\"}";
        }

        h2::Save save;
        if ( !h2::parseSave( bytes, save ) ) {
            return "{\"ok\":false,\"name\":" + jsonString( name )
                   + ",\"error\":\"Not a supported Heroes of Might and Magic II save (.GM1 / .GMC / .GXC).\"}";
        }

        h2conv::WorldData world;
        h2conv::ConvertOptions options;
        options.formatVersion = static_cast<uint16_t>( version );
        if ( !h2::convert( save, world, options ) ) {
            return "{\"ok\":false,\"name\":" + jsonString( name ) + ",\"error\":\"Conversion failed.\"}";
        }

        g_lastResult = h2conv::buildSaveFile( save.header, world, options );
        if ( g_lastResult.empty() ) {
            return "{\"ok\":false,\"name\":" + jsonString( name ) + ",\"error\":\"Failed to build the fheroes2 save.\"}";
        }

        std::string info = "{\"ok\":true,\"name\":" + jsonString( name );
        info += ",\"slotName\":" + jsonString( cp866ToUtf8( save.header.slotName ) );
        info += ",\"mapName\":" + jsonString( cp866ToUtf8( save.header.mapName ) );
        info += ",\"width\":" + jsonInt( save.header.mapWidth );
        info += ",\"height\":" + jsonInt( save.header.mapHeight );
        info += ",\"day\":" + jsonInt( save.header.day );
        info += ",\"week\":" + jsonInt( save.header.week );
        info += ",\"month\":" + jsonInt( save.header.month );
        info += ",\"players\":" + jsonInt( save.header.numPlayers );
        {
            static const char * const names[] = { "easy", "normal", "hard", "expert", "impossible" };
            info += ",\"difficulty\":";
            info += jsonInt( save.header.difficulty8 );
            info += ",\"difficultyName\":";
            info += jsonString( save.header.difficulty8 <= 4 ? names[save.header.difficulty8] : "?" );
        }
        info += ",\"campaign\":" + std::string( options.campaign ? "true" : "false" );
        info += ",\"outExt\":" + jsonString( outputExtension( name ) );
        info += "}";
        return info;
    }
    catch ( const std::exception & e ) {
        return "{\"ok\":false,\"name\":" + jsonString( name ) + ",\"error\":" + jsonString( e.what() ) + "}";
    }
    catch ( ... ) {
        return "{\"ok\":false,\"name\":" + jsonString( name ) + ",\"error\":\"Unknown error.\"}";
    }
}

emscripten::val takeLastResult()
{
    const std::vector<uint8_t> bytes = std::move( g_lastResult );
    return bytesToVal( bytes );
}

} // namespace

EMSCRIPTEN_BINDINGS( h2conv )
{
    emscripten::function( "convertSave", &convertSave );
    emscripten::function( "takeLastResult", &takeLastResult );
}
