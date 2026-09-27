#!/usr/bin/env python3
"""Generate the custom character set for battleships.c.

Writes src/c/charset.h with a full 256-character set (2 KB). Glyphs keep the
standard C64 screen codes the game already uses (letters 1-26, digits, the
graphics codes below), so only the look changes.

Usage: gen_charset.py [output.h] [--preview sheet.png]
"""
import struct
import sys
import zlib

# Letters and digits: 7x7 in the top-left of the 8x8 cell, 2-pixel strokes.
FONT = {
    "A": [".#####.", "##...##", "##...##", "#######", "##...##", "##...##", "##...##"],
    "B": ["######.", "##...##", "##...##", "######.", "##...##", "##...##", "######."],
    "C": [".#####.", "##...##", "##.....", "##.....", "##.....", "##...##", ".#####."],
    "D": ["#####..", "##..##.", "##...##", "##...##", "##...##", "##..##.", "#####.."],
    "E": ["#######", "##.....", "##.....", "#####..", "##.....", "##.....", "#######"],
    "F": ["#######", "##.....", "##.....", "#####..", "##.....", "##.....", "##....."],
    "G": [".#####.", "##...##", "##.....", "##.####", "##...##", "##...##", ".#####."],
    "H": ["##...##", "##...##", "##...##", "#######", "##...##", "##...##", "##...##"],
    "I": ["######.", "..##...", "..##...", "..##...", "..##...", "..##...", "######."],
    "J": [".....##", ".....##", ".....##", ".....##", "##...##", "##...##", ".#####."],
    "K": ["##...##", "##..##.", "##.##..", "####...", "##.##..", "##..##.", "##...##"],
    "L": ["##.....", "##.....", "##.....", "##.....", "##.....", "##.....", "#######"],
    "M": ["##...##", "###.###", "#######", "##.#.##", "##...##", "##...##", "##...##"],
    "N": ["##...##", "###..##", "####.##", "##.####", "##..###", "##...##", "##...##"],
    "O": [".#####.", "##...##", "##...##", "##...##", "##...##", "##...##", ".#####."],
    "P": ["######.", "##...##", "##...##", "######.", "##.....", "##.....", "##....."],
    "Q": [".#####.", "##...##", "##...##", "##...##", "##.#.##", "##..##.", ".###.##"],
    "R": ["######.", "##...##", "##...##", "######.", "##.##..", "##..##.", "##...##"],
    "S": [".#####.", "##...##", "##.....", ".#####.", ".....##", "##...##", ".#####."],
    "T": ["######.", "..##...", "..##...", "..##...", "..##...", "..##...", "..##..."],
    "U": ["##...##", "##...##", "##...##", "##...##", "##...##", "##...##", ".#####."],
    "V": ["##...##", "##...##", "##...##", "##...##", ".##.##.", "..###..", "...#..."],
    "W": ["##...##", "##...##", "##...##", "##.#.##", "#######", "###.###", "##...##"],
    "X": ["##...##", "##...##", ".##.##.", "..###..", ".##.##.", "##...##", "##...##"],
    "Y": ["##..##.", "##..##.", ".####..", "..##...", "..##...", "..##...", "..##..."],
    "Z": ["#######", "....##.", "...##..", "..##...", ".##....", "##.....", "#######"],
    "0": [".#####.", "##...##", "##..###", "##.#.##", "###..##", "##...##", ".#####."],
    "1": ["..##...", ".###...", "..##...", "..##...", "..##...", "..##...", ".####.."],
    "2": [".#####.", "##...##", ".....##", "..####.", ".##....", "##.....", "#######"],
    "3": [".#####.", "##...##", ".....##", "..####.", ".....##", "##...##", ".#####."],
    "4": ["...###.", "..####.", ".##.##.", "##..##.", "#######", "....##.", "....##."],
    "5": ["#######", "##.....", "######.", ".....##", ".....##", "##...##", ".#####."],
    "6": [".#####.", "##.....", "##.....", "######.", "##...##", "##...##", ".#####."],
    "7": ["#######", ".....##", "....##.", "...##..", "..##...", "..##...", "..##..."],
    "8": [".#####.", "##...##", "##...##", ".#####.", "##...##", "##...##", ".#####."],
    "9": [".#####.", "##...##", "##...##", ".######", ".....##", ".....##", ".#####."],
    "!": ["..##...", "..##...", "..##...", "..##...", ".......", "..##...", "..##..."],
    "+": [".......", "..##...", "..##...", "######.", "..##...", "..##...", "......."],
    ",": [".......", ".......", ".......", ".......", "..##...", "..##...", ".##...."],
    "-": [".......", ".......", ".......", "######.", ".......", ".......", "......."],
    ".": [".......", ".......", ".......", ".......", ".......", "..##...", "..##..."],
    "/": [".....##", "....##.", "...##..", "..##...", ".##....", "##.....", "......."],
    "<": ["....##.", "...##..", "..##...", ".##....", "..##...", "...##..", "....##."],
    ">": [".##....", "..##...", "...##..", "....##.", "...##..", "..##...", ".##...."],
    ":": [".......", "..##...", "..##...", ".......", "..##...", "..##...", "......."],
}

RING = ["..####..", ".#....#.", "#......#", "#......#", "#......#", "#......#", ".#....#.", "..####.."]

# Full 8x8 graphics, by screen code (names match battleships.c).
GRAPHICS = {
    100: ("WAVE_CHAR", ["........", "........", "........", "........", "........",
                        ".##..##.", "#..##..#", "........"]),
    87: ("MARK_CHAR", RING),
    215: ("MARK_LAND", ["".join("." if c == "#" else "#" for c in row) for row in RING]),
    83: ("heart", [".##.##..", "#######.", "#######.", "#######.", ".#####..",
                   "..###...", "...#....", "........"]),
    81: ("ammo", ["...##...", "..####..", "..####..", "..####..", "..####..",
                 "..####..", ".######.", "........"]),
    98: ("reload bar", ["........", "........", "#######.", "#######.", "#######.",
                        "#######.", "........", "........"]),
    64: ("SL_TRACK", ["........", "........", "........", "########", "########",
                      "........", "........", "........"]),
    91: ("SL_ZERO", ["...##...", "...##...", "...##...", "########", "########",
                     "...##...", "...##...", "...##..."]),
    112: ("ICE_CHAR", ["...#....", "..###...", ".#####..", ".####.#.", "#######.",
                        "##.####.", ".######.", "........"]),
    113: ("CRATE_FAST", ["########", "#......#", "#.#.#..#", "#..#.#.#", "#.#.#..#",
                          "#......#", "########", "........"]),
    114: ("CRATE_REPAIR", ["########", "#..##..#", "#.####.#", "#.####.#", "#..##..#",
                            "#......#", "########", "........"]),
    115: ("CRATE_SPOT", ["########", "#..##..#", "#.#..#.#", "#.#..#.#", "#..##..#",
                          "#......#", "########", "........"]),
    90: ("SL_KNOB", ["...##...", "..####..", ".######.", "########", "########",
                     ".######.", "..####..", "...##..."]),
}


# Island coast tiles: ISLAND_BASE + mask, where mask bits say which
# neighbours are land (N=1, E=2, S=4, W=8). Sides facing water get an
# irregular shoreline; corners between two water sides are rounded.
ISLAND_BASE = 0xE0
N, E, S, W = 1, 2, 4, 8
LAND_TEXTURE = ["########", "##.#####", "########", "#####.##",
                "########", ".#######", "########", "####.###"]
COAST = [1, 1, 2, 2, 1, 1, 1, 2]        # shoreline inset along a water side
CORNER_R = 7.0                          # rounding radius of outer corners


def island_tile(mask):
    rows = []
    for y in range(8):
        row = ""
        for x in range(8):
            land = LAND_TEXTURE[y][x] == "#"
            if not mask & N and y < COAST[x]:
                land = False
            if not mask & S and 7 - y < COAST[7 - x]:
                land = False
            if not mask & W and x < COAST[7 - y]:
                land = False
            if not mask & E and 7 - x < COAST[y]:
                land = False
            # rounded outer corners (pixel centres at +0.5)
            for (bits, cx, cy) in ((N | W, 0, 0), (N | E, 8, 0), (S | W, 0, 8), (S | E, 8, 8)):
                if mask & bits:
                    continue
                ox = CORNER_R if cx == 0 else 8 - CORNER_R
                oy = CORNER_R if cy == 0 else 8 - CORNER_R
                px, py = x + 0.5, y + 0.5
                inside_x = px < ox if cx == 0 else px > ox
                inside_y = py < oy if cy == 0 else py > oy
                if inside_x and inside_y and (px - ox) ** 2 + (py - oy) ** 2 > CORNER_R ** 2:
                    land = False
            row += "#" if land else "."
        rows.append(row)
    return rows


# Title logo: big letters designed on an 11x11 grid, scaled 2x into 3x3
# character cells (24x24 pixels), with every 4th pixel row left out for a
# striped look. Tiles are de-duplicated into unused screen codes.
BIG = {
    "A": ["...#####...", "..#######..", ".###...###.", "###.....###", "###.....###", "###########",
          "###########", "###.....###", "###.....###", "###.....###", "###.....###"],
    "B": ["##########.", "###########", "###.....###", "###.....###", "##########.", "##########.",
          "###.....###", "###.....###", "###.....###", "###########", "##########."],
    "E": ["###########", "###########", "###........", "###........", "#########..", "#########..",
          "###........", "###........", "###........", "###########", "###########"],
    "H": ["###.....###", "###.....###", "###.....###", "###.....###", "###.....###", "###########",
          "###########", "###.....###", "###.....###", "###.....###", "###.....###"],
    "I": [".#########.", ".#########.", "....###....", "....###....", "....###....", "....###....",
          "....###....", "....###....", "....###....", ".#########.", ".#########."],
    "L": ["###........", "###........", "###........", "###........", "###........", "###........",
          "###........", "###........", "###........", "###########", "###########"],
    "P": ["##########.", "###########", "###.....###", "###.....###", "###.....###", "###########",
          "##########.", "###........", "###........", "###........", "###........"],
    "S": [".#########.", "###########", "###........", "###........", "##########.", ".##########",
          "........###", "........###", "........###", "###########", ".#########."],
    "T": ["###########", "###########", "....###....", "....###....", "....###....", "....###....",
          "....###....", "....###....", "....###....", "....###....", "....###...."],
}
TITLE = "BATTLESHIPS"
TITLE_ROWS = 3                          # character rows per letter
TITLE_COLS = 3                          # character columns per letter


def big_letter_pixels(ch):
    """24x24 pixel rows for a logo letter."""
    rows = []
    for y in range(24):
        gy = y // 2
        line = []
        for x in range(24):
            gx = x // 2
            on = gy < 11 and gx < 11 and BIG[ch][gy][gx] == "#"
            if y % 4 == 3:
                on = False              # stripes
            line.append(1 if on else 0)
        rows.append(line)
    return rows


def build_title(data, used):
    """Adds the logo tiles to data; returns rows of screen codes."""
    free = [c for c in list(range(128, 215)) + list(range(116, 128)) + list(range(65, 81))
            if c not in used]
    tiles = {}
    layout = [[] for _ in range(TITLE_ROWS)]
    for ch in TITLE:
        px = big_letter_pixels(ch)
        for ty in range(TITLE_ROWS):
            for tx in range(TITLE_COLS):
                glyph = tuple(
                    int("".join(str(b) for b in px[ty * 8 + r][tx * 8:tx * 8 + 8]), 2) for r in range(8))
                if not any(glyph):
                    code = 32                   # blank: space
                elif glyph in tiles:
                    code = tiles[glyph]
                else:
                    code = free.pop(0)
                    tiles[glyph] = code
                    data[code * 8:code * 8 + 8] = list(glyph)
                layout[ty].append(code)
    return layout


def screen_code(ch):
    if "A" <= ch <= "Z":
        return ord(ch) - 64
    return ord(ch)                      # digits and punctuation: same as ASCII


def glyph_bytes(rows):
    rows = [r.ljust(8, ".") for r in rows] + ["........"] * (8 - len(rows))
    return [int(r.replace("#", "1").replace(".", "0"), 2) for r in rows]


def build():
    data = [0] * (256 * 8)
    for ch, rows in FONT.items():
        c = screen_code(ch)
        data[c * 8:c * 8 + 8] = glyph_bytes(rows)
    for c, (_, rows) in GRAPHICS.items():
        data[c * 8:c * 8 + 8] = glyph_bytes(rows)
    for mask in range(16):
        c = ISLAND_BASE + mask
        data[c * 8:c * 8 + 8] = glyph_bytes(island_tile(mask))
    used = {screen_code(ch) for ch in FONT} | set(GRAPHICS) | set(range(ISLAND_BASE, ISLAND_BASE + 16)) | {32}
    layout = build_title(data, used)
    return data, layout


def write_header(path, data, layout):
    with open(path, "w") as f:
        f.write("/* Generated by tools/gen_charset.py - do not edit. */\n\n")
        f.write(f"#define ISLAND_BASE 0x{ISLAND_BASE:02X}  /* + neighbour mask: N=1 E=2 S=4 W=8 */\n\n")
        f.write("/* 256 characters x 8 bytes, indexed by screen code */\n")
        f.write(f"static const unsigned char charset_data[{len(data)}] = {{\n")
        for i in range(0, len(data), 16):
            f.write("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",\n")
        f.write("};\n")
        f.write(f"\n/* Title logo: {TITLE_ROWS} rows x {len(layout[0])} screen codes */\n")
        f.write(f"#define TITLE_ROWS {TITLE_ROWS}\n#define TITLE_COLS {len(layout[0])}\n")
        f.write(f"static const unsigned char title_logo[{TITLE_ROWS}][{len(layout[0])}] = {{\n")
        for row in layout:
            f.write("    { " + ", ".join(f"{c:3d}" for c in row) + " },\n")
        f.write("};\n")


def write_preview(path, data, layout=None):
    """Sample text and graphics at 3x, light on blue like the game."""
    lines = ["BATTLESHIPS 0123456789", "FIRE + LEFT/RIGHT/UP", "3 SHOTS, THEN 2.5 S!"]
    codes = [[screen_code(c) for c in ln] for ln in lines]
    codes.append([c for c in GRAPHICS] + [100, 100])
    if layout:
        codes = [row for row in layout] + codes
    # sample islands: blobs of 1x1, 2x2, 2x3 and a 3x3 with cut corners
    grid = [[0] * 22 for _ in range(5)]
    for (c0, r0, w, h, cut) in ((1, 1, 1, 1, 0), (4, 1, 2, 2, 0), (8, 1, 2, 3, 0), (12, 1, 3, 3, 1), (17, 1, 4, 3, 1)):
        for r in range(r0, r0 + h):
            for c in range(c0, c0 + w):
                if cut and r in (r0, r0 + h - 1) and c in (c0, c0 + w - 1):
                    continue
                grid[r][c] = 1
    for r in range(5):
        row = []
        for c in range(22):
            if grid[r][c]:
                m = (N if r > 0 and grid[r - 1][c] else 0) | (S if r < 4 and grid[r + 1][c] else 0) \
                    | (W if c > 0 and grid[r][c - 1] else 0) | (E if c < 21 and grid[r][c + 1] else 0)
                row.append(ISLAND_BASE + m)
            else:
                row.append(32)
        codes.append(row)
    scale, w = 3, max(len(r) for r in codes)
    img = []
    for row in codes:
        for y in range(8):
            line = []
            for c in row + [32] * (w - len(row)):
                bits = data[c * 8 + y]
                line += [bits >> (7 - x) & 1 for x in range(8)]
            for _ in range(scale):
                img.append([v for v in line for _ in range(scale)])
    raw = b"".join(b"\x00" + bytes(c for v in r for c in ((230, 230, 230) if v else (40, 50, 200))) for r in img)

    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", len(img[0]), len(img), 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


def main():
    args = sys.argv[1:]
    preview = None
    if "--preview" in args:
        i = args.index("--preview")
        preview = args[i + 1]
        del args[i:i + 2]
    data, layout = build()
    write_header(args[0] if args else "src/c/charset.h", data, layout)
    if preview:
        write_preview(preview, data, layout)


if __name__ == "__main__":
    main()
