#!/usr/bin/env python3
"""HoMM2 .GM1 save file analyzer.

Format: Heroes of Might and Magic II (Succession Wars / Price of Loyalty),
per the layout recovered from game::SaveGame / playerData::Write /
hero::Write / fullMap::Write (project-ironfist decompilation) and
verified against real saves (saves/*.GM1, diff pairs).

Usage: python3 scripts/analyze.py <file.GM1> [--hexdump-section NAME]
"""
import struct
import sys

# --- offsets (base game, i.e. non-expansion save; PoL expansion offsets
# differ by a 4-byte magic at the very start) ---
OFF_WIDTH = 0
OFF_HEIGHT = 4
OFF_MAP_HEADER = 8
SZ_MAP_HEADER = 420
OFF_PLAYER_POS_COLOR = 428          # 65 bytes
OFF_GREATEST = 493
OFF_DIFFICULTY = 494               # i16
OFF_MONTH_TYPE = 496               # 4 bytes: monthType, monthTypeExtra, weekType, weekTypeExtra
OFF_PLAYER_NAMES = 500             # 6 x 21 bytes
OFF_ZEROS = 626                    # 36 bytes
OFF_IN_CAMPAIGN = 662              # 4 bytes
OFF_MAP_CHANGE_CTR = 666           # 4 bytes
OFF_FILENAME = 670                 # 14 bytes (8.3)
OFF_NUM_PLAYERS = 684
OFF_CUR_PLAYER = 685
OFF_NUM_DEFEATED = 686
OFF_PLAYER_DEAD = 687              # 6
OFF_PLAYER_ALIVE = 693             # 6
OFF_DAY = 699                      # i16
OFF_WEEK = 701                     # i16
OFF_MONTH = 703                    # i16
OFF_PLAYERS = 705                  # 6 x 207 (playerData::Write)
SZ_PLAYER = 207
OFF_NUM_OBELISKS = 1947
OFF_HEROES = 1948                  # 54 x 236 (hero::Write, base game)
SZ_HERO = 236
OFF_HERO_HIRE_STATUS = 14692       # 54
OFF_CASTLES = 14746                # 72 x 100
SZ_CASTLE = 100
OFF_FIELD_2773 = 21946             # 72
OFF_FIELD_27BB = 22018             # 9
OFF_MINES = 22027                  # 144 x 7
SZ_MINE = 7
OFF_FIELD_60A6 = 23035             # 144
OFF_ARTIFACT_RANDOM = 23179        # 82 (base game; 103 in expansion)
OFF_BOATS = 23261                  # 48 x 8
OFF_BOAT_BUILT = 23645             # 48
OFF_OBELISK_MASKS = 23693          # 48
OFF_UA_X = 23741
OFF_UA_Y = 23742
OFF_UA_IDX = 23743
OFF_RUMOR = 23744                  # 301
OFF_FIELD_637D = 24045             # 24
OFF_NUM_RUMORS = 24069             # 4

# --- playerData (file layout, 207 bytes; playerData::Write) ---
PLAYER_FIELDS = {
    'color': (0, 1),
    'numHeroes': (1, 1),
    'curHeroIdx': (2, 1),
    'heroesOwned': (4, 8),
    'heroesForPurchase': (12, 2),
    'personality': (58, 4),
    'hasEvilFaction': (63, 1),
    'daysLeftWithoutCastle': (67, 1),
    'numCastles': (68, 1),
    'curCastleIdx': (69, 1),
    'castlesOwned': (71, 72),
    'resources': (143, 28),         # 7 x u32: wood, mercury, ore, sulfur, crystal, gems, gold
    'field_E7': (171, 28),
    'barrierTentsVisited': (199, 1),
    'field_4_2': (201, 6),
}
RESOURCE_NAMES = ['wood', 'mercury', 'ore', 'sulfur', 'crystal', 'gems', 'gold']

# --- hero (file layout, 236 bytes; hero::Write, offsetof(hero, scrollSpell)) ---
HERO_FIELDS = {
    'spellpoints': (0, 2, 'u16'),
    'idx': (2, 1),
    'ownerIdx': (3, 1),
    'name': (10, 13),
    'factionID': (23, 1),
    'heroID': (24, 1),
    'x': (25, 4, 'i32'),
    'y': (29, 4, 'i32'),
    'occupiedObjType': (45, 2, 'u16'),
    'occupiedObjVal': (47, 2, 'u16'),
    'mobility': (49, 4, 'i32'),
    'remainingMobility': (53, 4, 'i32'),
    'experience': (57, 4, 'i32'),
    'oldLevel': (61, 2, 'u16'),
    'primarySkills': (63, 4),
    'tempMorale': (68, 1),
    'tempLuck': (69, 1),
    'randomSeed': (99, 1),
    'wisdomLastOffered': (100, 1),
    'armyTypes': (101, 5),
    'armyQty': (106, 10),           # 5 x u16
    'secSkillLevel': (116, 14),
    'secSkillIdx': (130, 14),
    'numSecSkills': (144, 4),
    'spells': (148, 65),
    'artifacts': (213, 14),
    'flags': (227, 4),
    'isCaptain': (231, 1),
}

# --- town (file layout, 100 bytes) ---
TOWN_FIELDS = {
    'idx': (0, 1),
    'ownerIdx': (1, 1),
    'alignment': (2, 1),
    'factionID': (3, 1),
    'x': (4, 1),
    'y': (5, 1),
    'buildDockRelated': (6, 1),
    'garrisonTypes': (8, 5),
    'garrisonQty': (13, 10),
    'visitingHeroIdx': (23, 1),
    'buildingsBuilt': (24, 4),
    'mageGuildLevel': (28, 1),
    'numCreaturesInDwelling': (30, 24),
    'exists': (54, 1),
    'mayNotBeUpgradedToCastle': (55, 1),
    'playerPos': (57, 1),
    'extraIdx': (58, 2),
    'mageGuildSpells': (60, 20),
    'numSpellsOfLevel': (80, 5),
    'name': (87, 12),
}

# --- mapCell (12 bytes) / mapCellExtra (7 bytes) ---
SZ_CELL = 12
SZ_CELL_EXTRA = 7


def cstr(b):
    return b.split(b'\x00')[0].decode('cp1252', 'replace')


def read_save(path):
    with open(path, 'rb') as f:
        return f.read()


def parse_header(d):
    width, height = struct.unpack_from('<ii', d, OFF_WIDTH)
    map_hdr = d[OFF_MAP_HEADER:OFF_MAP_HEADER + SZ_MAP_HEADER]
    name = cstr(map_hdr[58:118])
    description = cstr(map_hdr[118:416])
    day, week, month = struct.unpack_from('<hhh', d, OFF_DAY)
    return {
        'width': width,
        'height': height,
        'mapName': name,
        'description': description[:120],
        'day': day,
        'week': week,
        'month': month,
        'numPlayers': d[OFF_NUM_PLAYERS],
        'difficulty': struct.unpack_from('<h', d, OFF_DIFFICULTY)[0],
        'filename': cstr(d[OFF_FILENAME:OFF_FILENAME + 14]),
    }


def parse_players(d):
    players = []
    for i in range(6):
        p = d[OFF_PLAYERS + i * SZ_PLAYER: OFF_PLAYERS + (i + 1) * SZ_PLAYER]
        res = {}
        for name, (off, sz) in PLAYER_FIELDS.items():
            res[name] = p[off:off + sz]
        res['resources'] = [struct.unpack_from('<I', res['resources'], k * 4)[0] for k in range(7)]
        players.append(res)
    return players


def parse_heroes(d):
    heroes = []
    for i in range(54):
        h = d[OFF_HEROES + i * SZ_HERO: OFF_HEROES + (i + 1) * SZ_HERO]
        name = cstr(h[10:23])
        qty = struct.unpack_from('<HHHHH', h, 106)
        types = list(h[101:106])
        primary = list(h[63:67])
        spells = [k for k in range(65) if h[148 + k]]
        arts = list(h[213:227])
        x, y = struct.unpack_from('<ii', h, 25)
        exp = struct.unpack_from('<i', h, 57)[0]
        heroes.append({
            'name': name,
            'idx': h[2],
            'owner': h[3],
            'faction': h[23],
            'heroID': h[24],
            'x': x,
            'y': y,
            'exp': exp,
            'level': struct.unpack_from('<H', h, 61)[0],
            'spellpoints': struct.unpack_from('<H', h, 0)[0],
            'primary': primary,
            'army': list(zip(types, qty)),
            'spells': spells,
            'artifacts': arts,
            'numSecSkills': struct.unpack_from('<I', h, 144)[0],
        })
    return heroes


def parse_towns(d):
    towns = []
    for i in range(72):
        t = d[OFF_CASTLES + i * SZ_CASTLE: OFF_CASTLES + (i + 1) * SZ_CASTLE]
        if not t[54]:
            continue
        towns.append({
            'idx': t[0],
            'owner': t[1],
            'faction': t[3],
            'x': t[4],
            'y': t[5],
            'name': cstr(t[87:99]),
            'exists': t[54],
            'visitingHero': t[23],
            'buildings': struct.unpack_from('<I', t, 24)[0],
            'mageGuildLevel': t[28],
            'garrison': list(zip(list(t[8:13]), struct.unpack_from('<HHHHH', t, 13))),
        })
    return towns


MARKER = b'\xd2\x04\x00\x00'  # 1234


def parse_tail(d):
    # numRumors / numEvents / numMapEvents then the 1234-marked sections
    p = OFF_NUM_RUMORS
    num_rumors = struct.unpack_from('<I', d, p)[0]
    p += 4 + 2 * num_rumors
    num_events = struct.unpack_from('<I', d, p)[0]
    p += 4 + 2 * num_events
    num_map_events = struct.unpack_from('<I', d, p)[0]
    p += 4 + 2 * num_map_events
    marker = struct.unpack_from('<I', d, p)[0]
    p += 4
    max_extra = struct.unpack_from('<I', d, p)[0]
    p += 4
    marker2 = struct.unpack_from('<I', d, p)[0]
    p += 4
    extras = []
    for i in range(1, max_extra):
        m = struct.unpack_from('<I', d, p)[0]
        p += 4
        sz = struct.unpack_from('<H', d, p)[0]
        p += 2
        p += sz
        extras.append(sz)
    marker3 = struct.unpack_from('<I', d, p)[0]
    p += 4
    # mapRevealed: map width * map height bytes (ends at the next 1234 marker)
    next_marker = d.find(MARKER, p)
    revealed_size = next_marker - p
    p = next_marker + 4
    mw, mh = struct.unpack_from('<ii', d, p)
    p += 8
    p += SZ_CELL * mw * mh
    num_extras = struct.unpack_from('<I', d, p)[0]
    p += 4
    p += SZ_CELL_EXTRA * num_extras
    marker5 = struct.unpack_from('<I', d, p)[0]
    return {
        'numRumors': num_rumors,
        'numEvents': num_events,
        'numMapEvents': num_map_events,
        'numMapExtras': max_extra,
        'revealedSize': revealed_size,
        'mapW': mw,
        'mapH': mh,
        'numCellExtras': num_extras,
        'endOffset': p + 4,
        'markers': [marker, marker2, marker3, marker5],
    }


def main():
    path = sys.argv[1]
    d = read_save(path)
    print(f'=== {path} ({len(d)} bytes) ===')
    hdr = parse_header(d)
    for k, v in hdr.items():
        print(f'  {k}: {v}')
    print()
    for i, p in enumerate(parse_players(d)):
        alive = d[OFF_PLAYER_ALIVE + i]
        res = ', '.join(f'{n}={v}' for n, v in zip(RESOURCE_NAMES, p['resources']))
        print(f'  player {i}: color={p["color"][0]} heroes={p["numHeroes"][0]} alive={alive} '
              f'curHero={p["curHeroIdx"][0] if p["curHeroIdx"][0] != 0xFF else -1} | {res}')
    print()
    print('  heroes:')
    for h in parse_heroes(d):
        if h['owner'] not in (0xFF, 255) or h['name'] != '':
            army = ', '.join(f'{t}x{q}' if t != 0xFF else '--' for t, q in h['army'])
            print(f'    {h["name"] or "(unnamed)"} owner={h["owner"]} '
                  f'pos=({h["x"]},{h["y"]}) exp={h["exp"]} lvl={h["level"]} sp={h["spellpoints"]} '
                  f'prim={h["primary"]} army=[{army}] spells={len(h["spells"])} art={h["artifacts"]}')
    print()
    print('  towns:')
    for t in parse_towns(d):
        print(f'    {t["name"] or "(unnamed)"} owner={t["owner"]} pos=({t["x"]},{t["y"]}) '
              f'build=0x{t["buildings"]:08x} mage={t["mageGuildLevel"]} visit={t["visitingHero"]}')
    print()
    tail = parse_tail(d)
    print(f'  tail: rumors={tail["numRumors"]} events={tail["numEvents"]} '
          f'mapEvents={tail["numMapEvents"]} extras={tail["numMapExtras"]} '
          f'map={tail["mapW"]}x{tail["mapH"]} cellExtras={tail["numCellExtras"]} '
          f'end={tail["endOffset"]} markers={tail["markers"]}')


if __name__ == '__main__':
    main()
