// overlay.cpp: drawing helpers (pixels, boxes, text in the game's own tile font) used by the menu. (The F3 debug overlay was removed in 2.1.0.)
#include "overlay.h"
#include <cstdio>
#include <cstring>

namespace overlay {
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

} // namespace overlay
