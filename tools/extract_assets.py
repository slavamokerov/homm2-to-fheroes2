#!/usr/bin/env python3
"""Extract ROUTE.ICN frames, CLOF32.TIL tiles and KB.PAL from HEROES2.AGG.

The AGG/ICN/TIL binary formats mirror the fheroes2 engine readers
(engine/agg_file.cpp, fheroes2/agg/agg_image.cpp). The game palette is
KB.PAL (see src/fheroes2/game/fheroes2.cpp: setGamePalette(KB.PAL)).
Usage: extract_assets.py <path-to-HEROES2.AGG> <out-dir>
"""

import os
import struct
import sys

MAX_NAME_SIZE = 15


def parse_agg( path ):
    with open( path, "rb" ) as f:
        data = f.read()
    count = struct.unpack_from( "<H", data, 0 )[0]
    files = {}
    rec_size = 12
    for i in range( count ):
        h, off, size = struct.unpack_from( "<III", data, 2 + i * rec_size )
        files.setdefault( "_rec", [] ).append( ( h, off, size ) )
    name_start = len( data ) - count * MAX_NAME_SIZE
    for i in range( count ):
        name = data[name_start + i * MAX_NAME_SIZE: name_start + ( i + 1 ) * MAX_NAME_SIZE]
        name = name.split( b"\x00", 1 )[0].decode( "latin1", "replace" )
        h, off, size = files["_rec"][i]
        files[name] = data[off: off + size]
    return files


def decode_icn( body ):
    count = struct.unpack_from( "<H", body, 0 )[0]
    block_size = struct.unpack_from( "<I", body, 2 )[0]
    headers = []
    for i in range( count ):
        x, y, w, h, anim, od = struct.unpack_from( "<hhHBBI", body, 6 + 13 * i )
        headers.append( ( x, y, w, h, od ) )
    frames = []
    for i, ( x, y, w, h, od ) in enumerate( headers ):
        data_end = headers[i + 1][4] if i + 1 < count else ( 6 + 13 * count + block_size )
        raw = body[6 + od: data_end]
        px = [0] * ( w * h )          # palette indices
        tr = [1] * ( w * h )          # 1 = visible, 0 = opaque-default? (mirror of fheroes2: reset() sets transform 1, image 0)
        pos = 0
        row = 0
        p = 0
        while p < len( raw ):
            b = raw[p]
            if b == 0x00:
                row += 1
                pos = 0
                p += 1
            elif b < 0x80:
                n = b
                p += 1
                for k in range( n ):
                    if row < h and pos + k < w:
                        px[row * w + pos + k] = raw[p + k]
                        tr[row * w + pos + k] = 0
                p += n
                pos += n
            elif b == 0x80:
                break
            elif b < 0xC0:
                pos += b - 0x80
                p += 1
            elif b == 0xC0:
                p += 1
                tv = raw[p]
                cnt = tv & 0x03
                if cnt == 0:
                    p += 1
                    cnt = raw[p]
                # transform types 2..15 (shadows etc.)
                ttype = ( ( tv & 0x3C ) >> 2 ) + 2
                if ttype < 16:
                    base = row * w + pos
                    for k in range( cnt ):
                        if row < h and pos + k < w:
                            tr[base + k] = ttype
                pos += cnt
                p += 1
            else:
                # 0xC1: next byte is the count; 0xC2..0xFF: count = b - 0xC0.
                # In both cases the count is followed by a single color byte.
                if b == 0xC1:
                    p += 1
                    cnt = raw[p]
                else:
                    cnt = b - 0xC0
                p += 1
                color = raw[p]
                p += 1
                base = row * w + pos
                for k in range( cnt ):
                    if row < h and pos + k < w:
                        px[base + k] = color
                        tr[base + k] = 0
                pos += cnt
        frames.append( ( w, h, px, tr ) )
    return frames


def decode_til( body ):
    count, w, h = struct.unpack_from( "<HHH", body, 0 )
    tiles = []
    for i in range( count ):
        tiles.append( body[6 + i * w * h: 6 + ( i + 1 ) * w * h] )
    return w, h, tiles


def save_png( path, w, h, px, tr, palette, alpha_threshold=2 ):
    from PIL import Image
    img = Image.new( "RGBA", ( w, h ) )
    pix = img.load()
    for y in range( h ):
        for x in range( w ):
            off = y * w + x
            if tr[off] >= alpha_threshold:
                pix[x, y] = ( 0, 0, 0, 0 )
            else:
                c = px[off]
                r, g, b = palette[c * 3], palette[c * 3 + 1], palette[c * 3 + 2]
                pix[x, y] = ( r, g, b, 255 )
    img.save( path )
    return path


def main():
    agg_path = sys.argv[1]
    out_dir = sys.argv[2]
    os.makedirs( out_dir, exist_ok=True )

    files = parse_agg( agg_path )
    print( "AGG files:", ", ".join( sorted( files.keys() )[:40] ), "..." )

    palette = files["KB.PAL"]
    assert len( palette ) == 768, "KB.PAL size %d" % len( palette )

    frames = decode_icn( files["ROUTE.ICN"] )
    print( "ROUTE.ICN frames:", len( frames ) )
    for i, ( w, h, px, tr ) in enumerate( frames ):
        save_png( os.path.join( out_dir, "route%03d.png" % i ), w, h, px, tr, palette )

    tw, th, tiles = decode_til( files["CLOF32.TIL"] )
    print( "CLOF32.TIL: %d tiles %dx%d" % ( len( tiles ), tw, th ) )
    for i, t in enumerate( tiles ):
        save_png( os.path.join( out_dir, "clof%d.png" % i ), tw, th, t, [0] * len( t ), palette, alpha_threshold=1 )


if __name__ == "__main__":
    main()
