// overlay.cpp: the debug overlay declared in overlay.h; it only reads the machine and draws on a copy of the picture.
#include "overlay.h"
#include <cstdio>
#include <cstring>

namespace overlay {
static int ram(const Bus &b, int a) { return (a >= 0x4C00 && a < 0x4FF0) ? b.workram[a - 0x4C00] : 0; }
static int ram16(const Bus &b, int a) { return ram(b, a) | ram(b, a + 1) << 8; }
static int col_of(int d) { return 59 - d; }
static int row_of(int e) { return e - 32; }
static int sx(int v) { return v < 0x80 ? v : v - 256; } // signed byte

Info read_info(const Bus &b) {
    Info i{};
    i.level = ram(b, 0x4E13) + 1;
    i.phase = ram(b, 0x4DC1);
    i.counter = ram16(b, 0x4DC2);
    i.limit = i.phase < 7 ? ram16(b, 0x4D86 + 2 * i.phase) : 0;
    i.chase_phase = (i.phase & 1) != 0;
    i.pill = ram(b, 0x4DA6) != 0;
    i.elroy1 = ram(b, 0x4DB6) != 0;
    i.elroy2 = ram(b, 0x4DB7) != 0;
    i.dots = ram(b, 0x4E0E);
    i.state = ram(b, 0x4E00);
    i.sub = ram(b, 0x4E04);
    int pe = ram(b, 0x4D39), pd = ram(b, 0x4D3A), vec = ram16(b, 0x4D1C);
    i.pac_col = col_of(pd);
    i.pac_row = row_of(pe);
    i.pac_dir = vec == 0xFF00 ? 0 : vec == 0x0001 ? 1 : vec == 0x0100 ? 2 : vec == 0x00FF ? 3 : -1;
    const int corner[4] = {0x221D, 0x391D, 0x2040, 0x3B40};
    int be = ram(b, 0x4D0A), bd = ram(b, 0x4D0B);
    for (int g = 0; g < 4; ++g) {
        int ge = ram(b, 0x4D0A + 2 * g), gd = ram(b, 0x4D0B + 2 * g);
        GhostView &v = i.g[g];
        v.col = col_of(gd);
        v.row = row_of(ge);
        v.edible = ram(b, 0x4DA7 + g) != 0;
        bool scatter = (i.phase & 1) == 0 && i.sub == 3 && (g != 0 || !i.elroy1);
        int te, td;
        v.target_kind = 1;
        if (v.edible) {
            v.target_kind = 2;
            te = ge;
            td = gd;
        } else if (scatter) {
            v.target_kind = 0;
            te = corner[g] & 255;
            td = corner[g] >> 8;
        } else if (g == 0) {
            te = pe;
            td = pd;
        } else if (g == 1 || g == 2) {
            int ahead = g == 1 ? 4 : 2;
            int E = pe, D = pd;
            if (i.pac_dir == 0) D -= ahead;
            else if (i.pac_dir == 2) D += ahead;
            else if (i.pac_dir == 1) E += ahead;
            else if (i.pac_dir == 3) {
                E -= ahead;
                D += ahead;
            } // up: also left (the ROM's overflow bug)
            if (g == 2) {
                E = E + (E - be);
                D = D + (D - bd);
            }
            te = E & 255;
            td = D & 255;
        } else {
            int dc = col_of(gd) - col_of(pd), dr = row_of(ge) - row_of(pe);
            if (dc * dc + dr * dr < 64) {
                v.target_kind = 0;
                te = corner[3] & 255;
                td = corner[3] >> 8;
            } else {
                te = pe;
                td = pd;
            }
        }
        v.tcol = col_of(td);
        v.trow = row_of(te);
    }
    (void)sx;
    return i;
}

// --- drawing helpers on the output buffer
static void put(uint32_t *o, int x, int y, uint32_t c) {
    if (x >= 0 && y >= 0 && x < kOutW && y < kOutH) o[y * kOutW + x] = c;
}
static void rect(uint32_t *o, int x, int y, int w, int h, uint32_t c) {
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) put(o, x + i, y + j, c);
}
static void frame_box(uint32_t *o, int x, int y, int w, int h, uint32_t c) {
    for (int i = 0; i < w; ++i) {
        put(o, x + i, y, c);
        put(o, x + i, y + h - 1, c);
    }
    for (int j = 0; j < h; ++j) {
        put(o, x, y + j, c);
        put(o, x + w - 1, y + j, c);
    }
}
// text from the game's own tile ROM: digits 0x30-0x39, letters 0x41-0x5A, space; '-' drawn as a bar
static void text(const Gfx &g, uint32_t *o, int x, int y, const char *s, uint32_t c) {
    for (; *s; ++s, x += 8) {
        char ch = *s;
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        if (ch == '-') {
            rect(o, x + 1, y + 3, 5, 2, c);
            continue;
        }
        int code = (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ? ch : -1;
        if (code < 0) continue;
        for (int j = 0; j < 8; ++j)
            for (int i = 0; i < 8; ++i)
                if (g.tiles[code][j][i]) put(o, x + (7 - j), y + i, c); // tiles are stored turned: native (x, y) lands at (7 - y, x)
    }
}

void compose(const Gfx &gfx, const Bus &bus, const uint32_t *picture, uint32_t *out) {
    for (int y = 0; y < kOutH; ++y) {
        for (int x = 0; x < kPictureW; ++x) out[y * kOutW + x] = picture[y * kPictureW + x];
        for (int x = kPictureW; x < kOutW; ++x) out[y * kOutW + x] = 0x101018;
    }
    Info in = read_info(bus);
    const uint32_t gc[4] = {0xFF0000, 0xFFB8FF, 0x00FFFF, 0xFFB851};
    const char *gn[4] = {"BLINKY", "PINKY", "INKY", "CLYDE"};
    // markers on the copy: ghost tile (small outline) and its target tile (outline, clamped into the picture), in the ghost's colour
    for (int g = 0; g < 4; ++g) {
        const GhostView &v = in.g[g];
        if (v.target_kind == 2) continue;
        int tx = v.tcol * 8, ty = v.trow * 8;
        tx = tx < 0 ? 0 : tx > kPictureW - 8 ? kPictureW - 8 : tx;
        ty = ty < 0 ? 0 : ty > kPictureH - 8 ? kPictureH - 8 : ty;
        frame_box(out, tx, ty, 8, 8, gc[g]);
        frame_box(out, tx + 2, ty + 2, 4, 4, gc[g]);
    }
    int px = kPictureW + 6, py = 6;
    char b[64];
    text(gfx, out, px, py, "DEBUG", 0xFFFFFF);
    py += 12;
    std::snprintf(b, sizeof b, "LEVEL %d", in.level);
    text(gfx, out, px, py, b, 0xDDDDDD);
    py += 10;
    std::snprintf(b, sizeof b, "MODE %s", in.pill ? "FRIGHT" : in.chase_phase ? "CHASE" : "SCATTER");
    text(gfx, out, px, py, b, in.pill ? 0x4488FF : in.chase_phase ? 0xFF6060 : 0x60FF60);
    py += 10;
    std::snprintf(b, sizeof b, "PHASE %d", in.phase);
    text(gfx, out, px, py, b, 0xDDDDDD);
    py += 10;
    std::snprintf(b, sizeof b, "T %d", in.counter);
    text(gfx, out, px, py, b, 0xDDDDDD);
    py += 10;
    if (in.phase < 7) std::snprintf(b, sizeof b, "NEXT %d", in.limit);
    else std::snprintf(b, sizeof b, "NEXT NONE");
    text(gfx, out, px, py, b, 0xDDDDDD);
    py += 10;
    std::snprintf(b, sizeof b, "DOTS %d", in.dots);
    text(gfx, out, px, py, b, 0xDDDDDD);
    py += 10;
    std::snprintf(b, sizeof b, "ELROY %d", in.elroy2 ? 2 : in.elroy1 ? 1 : 0);
    text(gfx, out, px, py, b, 0xDDDDDD);
    py += 14;
    std::snprintf(b, sizeof b, "PAC %d %d", in.pac_col, in.pac_row);
    text(gfx, out, px, py, b, 0xFFFF00);
    py += 14;
    for (int g = 0; g < 4; ++g) {
        const GhostView &v = in.g[g];
        text(gfx, out, px, py, gn[g], gc[g]);
        py += 9;
        std::snprintf(b, sizeof b, "AT %d %d", v.col, v.row);
        text(gfx, out, px, py, b, 0xBBBBBB);
        py += 9;
        if (v.target_kind == 2) std::snprintf(b, sizeof b, "RANDOM");
        else std::snprintf(b, sizeof b, "%s %d %d", v.target_kind == 0 ? "CORNER" : "GOAL", v.tcol, v.trow);
        text(gfx, out, px, py, b, 0xBBBBBB);
        py += 13;
    }
}
} // namespace overlay
