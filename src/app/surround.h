// X44 + X45: the surround. When the window is wider or taller than the picture, the area around the picture can show a background layer instead of plain black. This step builds the layout
// system (which rectangles are "around the picture") and the setting (Surround: Black / Cabinet); the cabinet is an original look drawn in code (X45: dark body with side shading and T-moulding, a marquee with our own wording, a speaker grille, a bezel band),
// or the player's own picture surround.png next to the program. The layer is drawn before the picture and never changes where or how large the picture is (display.cpp's layout is used as it is).
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "../third_party/miniz/miniz.h" // X45: inflate, for the own-picture PNG loader

namespace surround {

enum Mode { kBlack = 0, kCabinet = 1, kAmbient = 2 }; // X46: Ambient = a blurred, darkened copy of the picture fills the whole window behind the picture
inline const char *name(int m) { return m == kCabinet ? "cabinet" : m == kAmbient ? "ambient" : "black"; }
inline int from_name(const char *s) { return !s ? kBlack : s[0] == 'c' ? kCabinet : s[0] == 'a' ? kAmbient : kBlack; }
inline int next_mode(int m) { return (m + 1) % 3; }

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool empty() const { return w <= 0 || h <= 0; }
    long long area() const { return empty() ? 0 : (long long)w * h; }
};

// The four rectangles around the picture rectangle `pic` inside a window of win_w x win_h (they never overlap each other or the picture, and together with the picture they cover the window):
//   top and bottom: the full window width; left and right: only the height of the picture.
struct Around {
    Rect top, bottom, left, right;
};
inline Around around(int win_w, int win_h, Rect pic) {
    Around a;
    const int px0 = std::max(0, std::min(win_w, pic.x)), py0 = std::max(0, std::min(win_h, pic.y));
    const int px1 = std::max(px0, std::min(win_w, pic.x + pic.w)), py1 = std::max(py0, std::min(win_h, pic.y + pic.h));
    a.top = {0, 0, win_w, py0};
    a.bottom = {0, py1, win_w, win_h - py1};
    a.left = {0, py0, px0, py1 - py0};
    a.right = {px1, py0, win_w - px1, py1 - py0};
    return a;
}
// The bezel: a band of `bezel_width(pic)` pixels around the picture (clipped to the window), drawn as part of the cabinet. Its width follows the picture size (3 % of the picture height, 2 to 24 pixels).
inline int bezel_width(Rect pic) { return std::max(2, std::min(24, pic.h * 3 / 100)); }
constexpr uint32_t kBezelColor = 0xFF5A5A78, kBezelLight = 0xFF9A9AB8, kBezelDark = 0xFF14141E;
inline Rect bezel_outer(int win_w, int win_h, Rect pic) {
    const int b = bezel_width(pic);
    const int x0 = std::max(0, pic.x - b), y0 = std::max(0, pic.y - b), x1 = std::min(win_w, pic.x + pic.w + b), y1 = std::min(win_h, pic.y + pic.h + b);
    return {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// X45: the original cabinet look, drawn in code (no image file is shipped, nothing copied from any real cabinet: our own wording and shapes, no company names or logos).
inline const char *marquee_text() { return "PACARCADECOMP"; }
inline const char *forbidden_words() { return "NAMCO MIDWAY BALLY"; } // never to appear in any text this file draws (checked by the unit test)

// A 5 x 7 letter font (capitals, digits, space, '-' and '.'), one string of 35 characters per glyph (rows from the top, '1' = lit); other characters draw as a space.
inline const char *glyph(char c) {
    switch (c) {
    case 'A': return "01110100011000110001111111000110001";
    case 'B': return "11110100011000111110100011000111110";
    case 'C': return "01110100011000010000100001000101110";
    case 'D': return "11110100011000110001100011000111110";
    case 'E': return "11111100001000011110100001000011111";
    case 'F': return "11111100001000011110100001000010000";
    case 'G': return "01110100011000010111100011000101111";
    case 'H': return "10001100011000111111100011000110001";
    case 'I': return "01110001000010000100001000010001110";
    case 'J': return "00111000100001000010000101001001100";
    case 'K': return "10001100101010011000101001001010001";
    case 'L': return "10000100001000010000100001000011111";
    case 'M': return "10001110111010110101100011000110001";
    case 'N': return "10001110011010110011100011000110001";
    case 'O': return "01110100011000110001100011000101110";
    case 'P': return "11110100011000111110100001000010000";
    case 'Q': return "01110100011000110001101011001001101";
    case 'R': return "11110100011000111110101001001010001";
    case 'S': return "01111100001000001110000010000111110";
    case 'T': return "11111001000010000100001000010000100";
    case 'U': return "10001100011000110001100011000101110";
    case 'V': return "10001100011000110001100010101000100";
    case 'W': return "10001100011000110101101011101110001";
    case 'X': return "10001100010101000100010101000110001";
    case 'Y': return "10001100010101000100001000010000100";
    case 'Z': return "11111000010001000100010001000011111";
    case '0': return "01110100011001110101110011000101110";
    case '1': return "00100011000010000100001000010001110";
    case '2': return "01110100010000100010001000100011111";
    case '3': return "11110000010000101110000010000111110";
    case '4': return "00010001100101010010111110001000010";
    case '5': return "11111100001111000001000011000101110";
    case '6': return "00110010001000011110100011000101110";
    case '7': return "11111000010001000100010000100001000";
    case '8': return "01110100011000101110100011000101110";
    case '9': return "01110100011000101111000010001001100";
    case '-': return "00000000000000011111000000000000000";
    case '.': return "00000000000000000000000000011000110";
    default: return "00000000000000000000000000000000000";
    }
}
inline int text_width(const char *t, int scale) {
    const int n = (int)std::strlen(t);
    return n ? (n * 6 - 1) * scale : 0;
}

struct Canvas { // ARGB pixels, w x h
    uint32_t *p;
    int w, h;
    void put(int x, int y, uint32_t c) {
        if (x >= 0 && y >= 0 && x < w && y < h) p[(size_t)y * w + x] = c;
    }
    void fill(Rect r, uint32_t c) {
        const int x0 = std::max(0, r.x), y0 = std::max(0, r.y), x1 = std::min(w, r.x + r.w), y1 = std::min(h, r.y + r.h);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) p[(size_t)y * w + x] = c;
    }
    void text(const char *t, int x, int y, int scale, uint32_t c) {
        for (const char *q = t; *q; ++q, x += 6 * scale) {
            const char *g = glyph(*q);
            for (int r = 0; r < 7; ++r)
                for (int k = 0; k < 5; ++k)
                    if (g[r * 5 + k] == '1') fill({x + k * scale, y + r * scale, scale, scale}, c);
        }
    }
};
inline uint32_t rgb(int r, int g, int b) {
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// The marquee and speaker grille rectangles for a window and picture rectangle (empty when there is not enough room above or below the picture).
struct Parts {
    Rect body, marquee, grille;
};
inline Parts parts(int win_w, int win_h, Rect pic) {
    Parts q;
    const Rect o = bezel_outer(win_w, win_h, pic);
    const int side = pic.w * 7 / 20; // each side of the cabinet body: 35 % of the picture width
    const int bw = std::min(win_w, pic.w + 2 * side);
    q.body = {(win_w - bw) / 2, 0, bw, win_h};
    const int topRoom = o.y - 6, botRoom = win_h - (o.y + o.h) - 6; // free height above / below the bezel (6 pixels air)
    const int mh = std::min(topRoom * 85 / 100, std::max(0, pic.h * 14 / 100));
    if (mh >= 20) q.marquee = {o.x, o.y - 6 - mh, o.w, mh};
    const int gh = std::min(botRoom * 85 / 100, std::max(0, pic.h * 9 / 100));
    if (gh >= 16) q.grille = {o.x + o.w / 5, o.y + o.h + 6, o.w * 3 / 5, gh};
    return q;
}

// The whole cabinet, one picture of win_w x win_h pixels (alpha 255). The picture rectangle itself stays black (the game is drawn over it).
inline void render_cabinet(uint32_t *out, int win_w, int win_h, Rect pic) {
    Canvas c{out, win_w, win_h};
    c.fill({0, 0, win_w, win_h}, rgb(10, 10, 14));
    const Parts q = parts(win_w, win_h, pic);
    const int u = std::max(1, pic.h / 288); // u: one "unit" of drawing size (the picture's scale)
    // body: dark blue-grey, shaded like a rounded surface (darker towards both edges) with a slow vertical gradient
    for (int x = q.body.x; x < q.body.x + q.body.w; ++x) {
        const double t = q.body.w > 1 ? (double)(x - q.body.x) / (q.body.w - 1) : 0.5;
        const double sh = 0.50 + 0.50 * (1.0 - (2 * t - 1) * (2 * t - 1));
        for (int y = 0; y < win_h; ++y) {
            const double v = 0.88 + 0.12 * y / std::max(1, win_h - 1);
            c.p[(size_t)y * win_w + x] = rgb((int)(40 * sh * v), (int)(44 * sh * v), (int)(66 * sh * v));
        }
    }
    // T-moulding: an orange band with a light line at both outer edges of the body, and two thin side-art lines inside each side panel
    const int tm = std::max(3, 2 * u);
    for (int s = 0; s < 2; ++s) {
        const int x = s == 0 ? q.body.x : q.body.x + q.body.w - tm;
        if (q.body.w > 2 * tm + 8) {
            c.fill({x, 0, tm, win_h}, rgb(200, 80, 36));
            c.fill({s == 0 ? x : x + tm - 1, 0, 1, win_h}, rgb(240, 150, 90));
        }
        const int side = (pic.w * 7 / 20);
        const int lx = s == 0 ? q.body.x + side / 3 : q.body.x + q.body.w - side / 3 - std::max(1, u);
        if (side > 12) {
            c.fill({lx, 0, std::max(1, u), win_h}, rgb(70, 90, 150));
            c.fill({s == 0 ? lx + 3 * u : lx - 3 * u, 0, std::max(1, u), win_h}, rgb(52, 66, 110));
        }
    }
    // marquee: a back-lit box above the picture with our own wording and two lamps
    if (!q.marquee.empty()) {
        const Rect m = q.marquee;
        c.fill(m, rgb(210, 160, 50));
        const int bd = std::max(2, m.h / 14);
        const Rect in{m.x + bd, m.y + bd, m.w - 2 * bd, m.h - 2 * bd};
        for (int y = in.y; y < in.y + in.h; ++y) {
            const double t = (double)(y - in.y) / std::max(1, in.h - 1);
            for (int x = in.x; x < in.x + in.w; ++x) c.p[(size_t)y * win_w + x] = rgb((int)(70 - 40 * t), (int)(14 + 6 * t), (int)(96 - 30 * t));
        }
        int scale = std::max(1, std::min(in.h * 55 / 100 / 7, in.w * 70 / 100 / ((int)std::strlen(marquee_text()) * 6 - 1)));
        const int tw = text_width(marquee_text(), scale), th = 7 * scale;
        const int tx = in.x + (in.w - tw) / 2, ty = in.y + (in.h - th) / 2;
        c.text(marquee_text(), tx + scale, ty + scale, scale, rgb(40, 8, 20));
        c.text(marquee_text(), tx, ty, scale, rgb(255, 205, 60));
        const int lr = std::max(2, in.h / 8);
        for (int s = 0; s < 2; ++s) {
            const int cx = s == 0 ? in.x + in.h / 2 : in.x + in.w - in.h / 2, cy = in.y + in.h / 2;
            for (int y = -lr; y <= lr; ++y)
                for (int x = -lr; x <= lr; ++x)
                    if (x * x + y * y <= lr * lr) c.put(cx + x, cy + y, rgb(255, 235, 150));
        }
    }
    // speaker grille: a dark panel with rows of slots
    if (!q.grille.empty()) {
        const Rect g = q.grille;
        c.fill(g, rgb(60, 62, 84));
        const Rect in{g.x + 2, g.y + 2, g.w - 4, g.h - 4};
        c.fill(in, rgb(12, 12, 18));
        const int rowh = std::max(2, in.h / 8), gap = std::max(2, rowh);
        for (int y = in.y + gap; y + rowh <= in.y + in.h - gap / 2; y += rowh + gap)
            for (int x = in.x + gap; x + 2 * gap <= in.x + in.w - gap; x += 4 * gap) c.fill({x, y, 3 * gap, rowh}, rgb(30, 32, 44));
    }
    // bezel around the picture (as in X44): band, light line outside, dark line against the picture
    const Rect o = bezel_outer(win_w, win_h, pic);
    c.fill(o, kBezelColor);
    c.fill({o.x, o.y, o.w, 1}, kBezelLight);
    c.fill({o.x, o.y, 1, o.h}, kBezelLight);
    c.fill({pic.x - 1, pic.y - 1, pic.w + 2, pic.h + 2}, kBezelDark);
    c.fill(pic, 0xFF000000u);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// X45: the player's own picture. A file called surround.png next to the program replaces the generated cabinet (the picture is scaled to cover the window; nothing else is drawn).
// A small PNG reader (8 bits per channel: grey, grey + alpha, RGB, RGBA, palette; not interlaced) with limits so that a bad file can never cause trouble.
constexpr size_t kMaxFileBytes = 16u * 1024 * 1024; // the file
constexpr int kMinSide = 64, kMaxSide = 8192;       // width and height
constexpr size_t kMaxPixels = 33u * 1000 * 1000;    // width * height
struct Image {
    int w = 0, h = 0;
    std::vector<uint32_t> argb;
};
inline uint32_t be32(const unsigned char *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
// Returns "" when the picture was read, otherwise the reason (for the log). The picture is left empty on failure.
inline std::string decode_png(const unsigned char *d, size_t n, Image *out) {
    out->w = out->h = 0;
    out->argb.clear();
    if (n > kMaxFileBytes) return "the file is bigger than 16 MB";
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (n < 8 + 25 || std::memcmp(d, sig, 8) != 0) return "not a PNG file";
    size_t pos = 8;
    int w = 0, h = 0, depth = 0, ctype = -1, interlace = 0;
    bool have_ihdr = false;
    std::vector<unsigned char> idat, plte, trns;
    while (pos + 12 <= n) {
        const uint32_t len = be32(d + pos);
        const char *type = (const char *)(d + pos + 4);
        if ((size_t)len > n - pos - 12) return "damaged PNG (a block runs past the end of the file)";
        const unsigned char *body = d + pos + 8;
        if (!std::memcmp(type, "IHDR", 4)) {
            if (len != 13) return "damaged PNG header";
            w = (int)be32(body);
            h = (int)be32(body + 4);
            depth = body[8];
            ctype = body[9];
            interlace = body[12];
            have_ihdr = true;
            if (body[10] != 0 || body[11] != 0) return "unsupported PNG compression";
        } else if (!std::memcmp(type, "IDAT", 4)) {
            if (idat.size() + len > kMaxFileBytes) return "the file is bigger than 16 MB";
            idat.insert(idat.end(), body, body + len);
        } else if (!std::memcmp(type, "PLTE", 4)) plte.assign(body, body + len);
        else if (!std::memcmp(type, "tRNS", 4)) trns.assign(body, body + len);
        else if (!std::memcmp(type, "IEND", 4)) break;
        pos += 12 + (size_t)len;
    }
    if (!have_ihdr) return "damaged PNG (no header)";
    if (w < kMinSide || h < kMinSide) return "the picture is too small (at least 64 x 64)";
    if (w > kMaxSide || h > kMaxSide || (size_t)w * (size_t)h > kMaxPixels) return "the picture is too large (at most 8192 x 8192 and 33 million pixels)";
    if (interlace != 0) return "interlaced PNG is not supported (save it without interlacing)";
    if (depth != 8) return "only 8 bits per colour are supported";
    int ch = 0;
    switch (ctype) {
    case 0: ch = 1; break;
    case 2: ch = 3; break;
    case 3: ch = 1; break;
    case 4: ch = 2; break;
    case 6: ch = 4; break;
    default: return "unsupported PNG colour type";
    }
    if (ctype == 3 && plte.size() < 3) return "damaged PNG (no palette)";
    const size_t stride = (size_t)w * ch, raw_size = (stride + 1) * (size_t)h;
    std::vector<unsigned char> raw(raw_size);
    mz_ulong rl = (mz_ulong)raw_size;
    if (idat.empty()) return "damaged PNG (no picture data)";
    if (mz_uncompress(raw.data(), &rl, idat.data(), (mz_ulong)idat.size()) != MZ_OK || rl != raw_size) return "damaged PNG (the picture data cannot be read)";
    std::vector<unsigned char> img((size_t)h * stride);
    for (int y = 0; y < h; ++y) {
        const unsigned char f = raw[(size_t)y * (stride + 1)];
        const unsigned char *src = &raw[(size_t)y * (stride + 1) + 1];
        unsigned char *dst = &img[(size_t)y * stride];
        const unsigned char *up = y ? &img[(size_t)(y - 1) * stride] : nullptr;
        if (f > 4) return "damaged PNG (unknown filter)";
        for (size_t i = 0; i < stride; ++i) {
            const int a = i >= (size_t)ch ? dst[i - ch] : 0, b = up ? up[i] : 0, c = (up && i >= (size_t)ch) ? up[i - ch] : 0;
            int v = src[i];
            switch (f) {
            case 1: v += a; break;
            case 2: v += b; break;
            case 3: v += (a + b) / 2; break;
            case 4: {
                const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
                v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                break;
            }
            default: break;
            }
            dst[i] = (unsigned char)v;
        }
    }
    out->w = w;
    out->h = h;
    out->argb.resize((size_t)w * h);
    for (size_t i = 0; i < (size_t)w * h; ++i) {
        const unsigned char *p = &img[i * ch];
        int r, g, b, a = 255;
        switch (ctype) {
        case 0: r = g = b = p[0]; break;
        case 4:
            r = g = b = p[0];
            a = p[1];
            break;
        case 2:
            r = p[0];
            g = p[1];
            b = p[2];
            break;
        case 6:
            r = p[0];
            g = p[1];
            b = p[2];
            a = p[3];
            break;
        default: {
            const size_t k = p[0];
            if (k * 3 + 2 >= plte.size()) {
                out->w = out->h = 0;
                out->argb.clear();
                return "damaged PNG (palette index out of range)";
            }
            r = plte[k * 3];
            g = plte[k * 3 + 1];
            b = plte[k * 3 + 2];
            a = k < trns.size() ? trns[k] : 255;
        }
        }
        // the picture is a background: transparency is shown over black
        out->argb[i] = 0xFF000000u | ((uint32_t)(r * a / 255) << 16) | ((uint32_t)(g * a / 255) << 8) | (uint32_t)(b * a / 255);
    }
    return "";
}
// The part of a w x h picture that covers a win_w x win_h window (aspect kept, centred, the overflow cropped): the source rectangle for the drawing code.
inline Rect cover_source(int w, int h, int win_w, int win_h) {
    if (w <= 0 || h <= 0 || win_w <= 0 || win_h <= 0) return {0, 0, w, h};
    if ((long long)w * win_h >= (long long)h * win_w) {
        const int sw = (int)((long long)h * win_w / win_h);
        return {(w - sw) / 2, 0, std::max(1, sw), h};
    }
    const int sh = (int)((long long)w * win_h / win_w);
    return {0, (h - sh) / 2, w, std::max(1, sh)};
}

// X46: the ambient glow. The picture (w x h pixels inside an array with `stride` pixels per row) is reduced to gw x gh cells by averaging (gw and gh divide w and h evenly or not: cell edges are rounded);
// the drawing code stretches that small picture over the whole window with a smooth filter, which blurs it, and darkens it.
constexpr int kAmbientW = 28, kAmbientH = 36,
              kAmbientDarkAlpha = 100; // grid size, and how dark: every colour is multiplied by (255 - alpha) / 255 while averaging (no extra drawing pass is needed)
inline void ambient_grid(const uint32_t *src, int stride, int w, int h, int gw, int gh, uint32_t *out) {
    for (int gy = 0; gy < gh; ++gy)
        for (int gx = 0; gx < gw; ++gx) {
            const int x0 = gx * w / gw, x1 = std::max(x0 + 1, (gx + 1) * w / gw), y0 = gy * h / gh, y1 = std::max(y0 + 1, (gy + 1) * h / gh);
            unsigned long long r = 0, g = 0, b = 0, n = 0;
            for (int y = y0; y < y1 && y < h; ++y)
                for (int x = x0; x < x1 && x < w; ++x) {
                    const uint32_t v = src[(size_t)y * stride + x];
                    r += (v >> 16) & 255;
                    g += (v >> 8) & 255;
                    b += v & 255;
                    ++n;
                }
            if (!n) { n = 1; }
            const unsigned long long k = 255 - kAmbientDarkAlpha, d = n * 255;
            out[(size_t)gy * gw + gx] = 0xFF000000u | (uint32_t)(r * k / d) << 16 | (uint32_t)(g * k / d) << 8 | (uint32_t)(b * k / d);
        }
}

} // namespace surround
